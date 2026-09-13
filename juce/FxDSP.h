// FxDSP.h — algoritmi dei pedali della scheda FX.
//
// Header-only, monocanale, senza dipendenze da JUCE, come PedalDSP.h.
// Un'istanza per canale e per sezione.
//
// Le differenze fra un modello e l'altro non stanno nei nomi dei pomelli ma
// nella catena: un ritardo a secchi filtra e satura dentro l'anello di
// reazione, uno digitale no; un coro a una voce batte, uno a due voci in
// quadratura allarga; un vibrato non ha segnale diretto, e per questo si sente
// l'intonazione muoversi invece del battimento.
#pragma once

#include "FxRegistry.h"
#include "PedalDSP.h"      // OnePole, Peak
#include <vector>
#include <cmath>
#include <algorithm>

namespace fxpedal {

// Linea di ritardo con lettura interpolata, il mattone comune a ritardi,
// cori e flanger.
class Line
{
public:
    void prepare (double sr, float maxMs)
    {
        const int n = std::max (16, (int) std::ceil (sr * (double) maxMs * 0.001));
        buf_.assign ((size_t) n, 0.f);
        w_ = 0;
    }
    void reset() { std::fill (buf_.begin(), buf_.end(), 0.f); w_ = 0; }

    float read (float delaySamples) const
    {
        const int n = (int) buf_.size();
        if (n < 4) return 0.f;
        const float d = std::clamp (delaySamples, 1.f, (float) n - 2.f);
        float p = (float) w_ - d;
        while (p < 0.f) p += (float) n;
        const int i0 = (int) p;
        const int i1 = (i0 + 1) % n;
        const float f = p - (float) i0;
        return buf_[(size_t) i0] * (1.f - f) + buf_[(size_t) i1] * f;
    }
    void write (float x)
    {
        if (buf_.empty()) return;
        buf_[(size_t) w_] = x;
        w_ = (w_ + 1) % (int) buf_.size();
    }
    bool empty() const { return buf_.size() < 4; }

private:
    std::vector<float> buf_;
    int w_ = 0;
};

// Riverbero a pettini e passa-tutto, nella forma classica. I parametri di
// dimensione, smorzamento e densita' li decide il modello.
class Tank
{
public:
    void prepare (double sr)
    {
        const double s = sr / 44100.0;
        static const int comb[kC] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int allp[kA] = { 225, 341, 441, 556 };
        for (int i = 0; i < kC; ++i) {
            c_[i].len = std::max (1, (int) std::lround (comb[i] * s));
            c_[i].buf.assign ((size_t) c_[i].len, 0.f); c_[i].idx = 0; c_[i].lp = 0.f;
        }
        for (int i = 0; i < kA; ++i) {
            a_[i].len = std::max (1, (int) std::lround (allp[i] * s));
            a_[i].buf.assign ((size_t) a_[i].len, 0.f); a_[i].idx = 0;
        }
    }
    void reset()
    {
        for (auto& c : c_) { std::fill (c.buf.begin(), c.buf.end(), 0.f); c.idx = 0; c.lp = 0.f; }
        for (auto& a : a_) { std::fill (a.buf.begin(), a.buf.end(), 0.f); a.idx = 0; }
    }

    // `fb` guadagno dei pettini, `damp` smorzamento nell'anello, `diff`
    // coefficiente dei passa-tutto: piu' alto, piu' densa la coda.
    float process (float x, float fb, float damp, float diff)
    {
        float sum = 0.f;
        const float in = x * 0.015f;
        for (int i = 0; i < kC; ++i) {
            auto& c = c_[i];
            const float y = c.buf[(size_t) c.idx];
            c.lp = y * (1.f - damp) + c.lp * damp;
            c.buf[(size_t) c.idx] = in + c.lp * fb;
            c.idx = (c.idx + 1) % c.len;
            sum += y;
        }
        float y = sum;
        for (int i = 0; i < kA; ++i) {
            auto& a = a_[i];
            const float out = a.buf[(size_t) a.idx];
            a.buf[(size_t) a.idx] = y + out * diff;
            a.idx = (a.idx + 1) % a.len;
            y = out - y;
        }
        return y;
    }

private:
    static constexpr int kC = 8, kA = 4;
    struct C { std::vector<float> buf; int len = 0, idx = 0; float lp = 0.f; };
    struct A { std::vector<float> buf; int len = 0, idx = 0; };
    C c_[kC];
    A a_[kA];
};

// --- il pedale -------------------------------------------------------------

class FxFX
{
public:
    void prepare (double sr)
    {
        sr_ = (sr > 0.0) ? sr : 48000.0;
        // Ogni sezione puo' ospitare qualunque modello dell'elenco, quindi la
        // linea lunga serve a tutte. 1,2 s copre il tempo massimo dichiarato.
        long_.prepare (sr_, 1250.f);
        mod_ .prepare (sr_, 80.f);
        tank_.prepare (sr_);
        reset();
    }

    void reset()
    {
        long_.reset(); mod_.reset(); tank_.reset();
        lfo_ = lfo2_ = 0.f;
        fbLp_.reset(); fbHp_.reset(); toneLp_.reset(); wetHp_.reset(); wetLp_.reset();
        spring_.reset();
        riseEnv_ = 0.f;
        step_ = 0; stepPhase_ = 0.f; gateSm_ = 1.f;
        fbState_ = 0.f;
    }

    void setBypass (bool b)              { bypass_ = b; }
    void setModel  (int i)               { model_ = std::clamp (i, 0, count() - 1); }
    void setKnob   (int i, float v)      { if (i >= 0 && i < kMaxKnobs)  k_[i]  = v; }
    void setSwitch (int i, int v)        { if (i >= 0 && i < kMaxSwitch) sw_[i] = v; }

    void commit()
    {
        const Model& m = at (model_);
        switch (m.topo) {
            case Topology::ChorusCe5:
                wetLp_.setCutoff (std::clamp (k_[3], 1000.f, 12000.f), sr_, false);
                wetHp_.setCutoff (std::clamp (k_[4],   20.f,   800.f), sr_, true);
                break;
            case Topology::DelayAnalog:
                // Larghezza di banda del circuito a secchi: la reazione perde
                // acuti e bassi a ogni giro, ed e' questo che scurisce i secchi.
                fbLp_.setCutoff (2600.f, sr_, false);
                fbHp_.setCutoff (180.f,  sr_, true);
                break;
            case Topology::DelayMod:
                fbLp_.setCutoff (5000.f, sr_, false);
                break;
            case Topology::ReverbSix:
            case Topology::ReverbMod: {
                const float tone = (m.topo == Topology::ReverbSix) ? k_[1] : k_[1];
                // Il tono chiude o apre la coda: da 1,2 kHz a 16 kHz.
                const float fc = 1200.f * std::pow (2.f, (tone + 12.f) / 6.f);
                toneLp_.setCutoff (std::clamp (fc, 800.f, 16000.f), sr_, false);
                spring_.set (2100.f, 1.6f, springOn() ? 7.f : 0.f, sr_);
                break;
            }
            default: break;
        }
    }

    float process (float x)
    {
        if (bypass_) return x;
        const Model& m = at (model_);
        switch (m.topo) {
            case Topology::DelayStd:     return delayStd (x);
            case Topology::DelayAnalog:  return delayAnalog (x);
            case Topology::DelayDigital: return delayDigital (x);
            case Topology::DelayMod:     return delayMod (x);
            case Topology::ChorusStd:    return chorusStd (x);
            case Topology::ChorusCe2:    return chorusCe2 (x);
            case Topology::ChorusCe5:    return chorusCe5 (x);
            case Topology::Vibrato:      return vibrato (x);
            case Topology::FlangerStd:   return flangerStd (x);
            case Topology::FlangerBf2:   return flangerBf (x, false);
            case Topology::FlangerBf3:   return flangerBf (x, sw_[0] == 1);
            case Topology::ReverbStd:    return reverbStd (x);
            case Topology::ReverbSix:    return reverbSix (x, false);
            case Topology::ReverbMod:    return reverbSix (x, true);
            case Topology::TremStd:      return tremStd (x);
            case Topology::TremTr2:      return tremTr2 (x);
            case Topology::TremSlicer:   return tremSlicer (x);
        }
        return x;
    }

private:
    bool springOn() const
    { return at (model_).topo == Topology::ReverbSix && sw_[0] == 3; }

    float lfoStep (float rateHz, float& ph) const
    {
        ph += 6.2831853f * std::max (0.01f, rateHz) / (float) sr_;
        if (ph > 6.2831853f) ph -= 6.2831853f;
        return std::sin (ph);
    }

    // --- ritardi ----------------------------------------------------------
    float delayStd (float x)
    {
        const float ts = k_[0] * 0.001f * (float) sr_;
        const float y  = long_.read (ts);
        long_.write (x + y * k_[1]);
        return x * (1.f - k_[2]) + y * k_[2];
    }

    // A secchi: la reazione passa per un filtro di banda e per una
    // compressione dolce, quindi ogni ripetizione torna piu' scura e piu'
    // tonda, e spinta si impasta invece di sgranare.
    float delayAnalog (float x)
    {
        const float ts = k_[0] * 0.001f * (float) sr_;
        const float y  = long_.read (ts);
        float fb = fbHp_.process (fbLp_.process (y));
        fb = std::tanh (fb * 1.4f) * 0.72f;
        long_.write (x + fb * k_[1]);
        return x + y * k_[2];          // il pedale somma, non incrocia
    }

    float delayDigital (float x)
    {
        static const float kMaxMs[3] = { 50.f, 200.f, 800.f };
        const float ms = 5.f + k_[0] * (kMaxMs[std::clamp (sw_[0], 0, 2)] - 5.f);
        const float y  = long_.read (ms * 0.001f * (float) sr_);
        long_.write (x + y * k_[1]);
        return x + y * k_[2];
    }

    float delayMod (float x)
    {
        const float mod = lfoStep (0.35f, lfo_) * k_[3] * 0.004f * (float) sr_;
        const float ts  = k_[0] * 0.001f * (float) sr_ + mod;
        const float y   = long_.read (ts);
        long_.write (x + fbLp_.process (y) * k_[1]);
        return x + y * k_[2];
    }

    // --- cori e vibrato ---------------------------------------------------
    float chorusStd (float x)
    {
        const float d = (15.f + 10.f * k_[1] * lfoStep (k_[0], lfo_)) * 0.001f * (float) sr_;
        const float y = mod_.read (d);
        mod_.write (x);
        return x * (1.f - k_[2]) + y * k_[2];
    }

    // Una voce sola, dosaggio fisso a meta' come sull'originale: il battimento
    // fra diretto e modulato e' tutto l'effetto.
    float chorusCe2 (float x)
    {
        const float d = (20.f + 7.f * k_[1] * lfoStep (k_[0], lfo_)) * 0.001f * (float) sr_;
        const float y = wetLpFixed (mod_.read (d));
        mod_.write (x);
        return x * 0.7f + y * 0.5f;
    }

    // Due voci sfasate di un quarto di giro: invece di battere, allargano.
    float chorusCe5 (float x)
    {
        const float s1 = lfoStep (k_[1], lfo_);
        lfo2_ = lfo_ + 1.5707963f;
        const float s2 = std::sin (lfo2_);
        const float d1 = (18.f + 8.f * k_[2] * s1) * 0.001f * (float) sr_;
        const float d2 = (24.f + 8.f * k_[2] * s2) * 0.001f * (float) sr_;
        float wet = 0.5f * (mod_.read (d1) + mod_.read (d2));
        mod_.write (x);
        wet = wetHp_.process (wetLp_.process (wet));
        return x + wet * k_[0];
    }

    // Nessun segnale diretto: resta solo quello modulato, ed e' per questo che
    // si sente l'intonazione ondeggiare e non il battimento.
    float vibrato (float x)
    {
        const float riseMs = std::max (1.f, k_[2]);
        const float a = std::exp (-1.0f / (float) (sr_ * riseMs * 0.001));
        riseEnv_ = riseEnv_ * a + (1.f - a);
        const float d = (12.f + 6.f * k_[1] * riseEnv_ * lfoStep (k_[0], lfo_))
                        * 0.001f * (float) sr_;
        const float y = mod_.read (d);
        mod_.write (x);
        return y;
    }

    // --- flanger ----------------------------------------------------------
    float flangerStd (float x)
    {
        const float d = (3.f + 2.5f * k_[1] * lfoStep (k_[0], lfo_)) * 0.001f * (float) sr_;
        const float y = mod_.read (d);
        mod_.write (x + y * k_[2]);
        return x * (1.f - k_[3]) + y * k_[3];
    }

    // Il manuale sposta il centro della spazzolata, la risonanza la reazione.
    // In Ultra la spazzolata copre tutta la corsa e la reazione arriva quasi
    // all'innesco: e' quello che fa il fischio.
    float flangerBf (float x, bool ultra)
    {
        const float man   = k_[0];
        const float depth = k_[1] * (ultra ? 1.6f : 1.0f);
        const float res   = std::min (0.95f, k_[3] * (ultra ? 1.15f : 1.0f));
        const float swing = std::min (man - 0.2f, man * 0.9f) * depth;
        const float ms    = man + swing * lfoStep (k_[2], lfo_);
        const float y     = mod_.read (std::max (0.2f, ms) * 0.001f * (float) sr_);
        mod_.write (x + y * res);
        return x + y * 0.7f;
    }

    // --- riverberi --------------------------------------------------------
    float reverbStd (float x)
    {
        const float y = tank_.process (x, 0.7f + k_[0] * 0.28f, k_[1] * 0.4f, 0.5f);
        return x * (1.f - k_[2]) + y * k_[2];
    }

    // Quattro ambienti: cambiano reazione, smorzamento, densita' e preritardo,
    // che sono le cose che davvero distinguono una stanza da una sala.
    float reverbSix (float x, bool modulated)
    {
        const int mode = modulated ? 1 : std::clamp (sw_[0], 0, 3);
        const float t  = modulated ? k_[2] : k_[2];

        float fb, damp, diff, preMs;
        switch (mode) {
            case 1:  fb = 0.84f + t * 0.14f; damp = 0.10f; diff = 0.62f; preMs = 25.f; break;  // sala
            case 2:  fb = 0.80f + t * 0.15f; damp = 0.04f; diff = 0.72f; preMs =  0.f; break;  // piastra
            case 3:  fb = 0.78f + t * 0.14f; damp = 0.22f; diff = 0.45f; preMs =  8.f; break;  // molla
            default: fb = 0.70f + t * 0.16f; damp = 0.28f; diff = 0.50f; preMs =  6.f; break;  // stanza
        }

        float in = x;
        if (preMs > 0.f) { in = long_.read (preMs * 0.001f * (float) sr_); long_.write (x); }
        float y = tank_.process (in, fb, damp, diff);

        if (mode == 3) y = spring_.process (y);        // il tonfo della molla
        y = toneLp_.process (y);

        if (modulated) {
            const float d = (14.f + 6.f * k_[3] * lfoStep (0.28f, lfo_)) * 0.001f * (float) sr_;
            const float w = mod_.read (d);
            mod_.write (y);
            y = y * 0.4f + w * 0.6f;
        }
        return x + y * k_[0];
    }

    // --- tremoli ----------------------------------------------------------
    float tremStd (float x)
    {
        const float s  = lfoStep (k_[0], lfo_);
        const float sq = std::tanh (s * 6.f);
        const float l  = s * (1.f - k_[2]) + sq * k_[2];
        return x * (1.f - k_[1] * 0.5f * (1.f - l));
    }

    // L'onda passa dal triangolo all'onda quadra senza scatti: il triangolo e'
    // la sinusoide raddrizzata in modo lineare, la quadra la stessa compressa.
    float tremTr2 (float x)
    {
        const float s  = lfoStep (k_[0], lfo_);
        const float tri = std::asin (std::clamp (s, -1.f, 1.f)) * 0.6366198f;   // 2/pi
        const float sq  = std::tanh (s * 12.f);
        const float l   = tri * (1.f - k_[2]) + sq * k_[2];
        return x * (1.f - k_[1] * 0.5f * (1.f - l));
    }

    // Un motivo a passi invece di un'onda: otto sedicesimi per giro, con lo
    // schema che decide quali suonano. SMOOTH arrotonda i fronti, altrimenti
    // ogni taglio farebbe un click.
    float tremSlicer (float x)
    {
        static const unsigned char kPat[4][8] = {
            { 1,0,1,0,1,0,1,0 },   // otto
            { 1,0,0,1,0,0,1,0 },   // terzine
            { 1,1,0,1,0,1,1,0 },   // salto
            { 0,1,1,0,1,1,0,1 },   // sincope
        };
        stepPhase_ += std::max (0.05f, k_[0]) * 8.f / (float) sr_;
        while (stepPhase_ >= 1.f) { stepPhase_ -= 1.f; step_ = (step_ + 1) & 7; }
        const auto& pat = kPat[std::clamp (sw_[0], 0, 3)];
        const float target = pat[step_] ? 1.f : (1.f - k_[1]);
        const float smoothMs = 0.5f + k_[2] * 40.f;
        const float a = std::exp (-1.0f / (float) (sr_ * smoothMs * 0.001));
        gateSm_ = gateSm_ * a + target * (1.f - a);
        return x * gateSm_;
    }

    float wetLpFixed (float v)
    {
        // Il passa-basso dei secchi, fisso: e' quello che rende dolce il coro.
        fbState_ = fbState_ * 0.72f + v * 0.28f;
        return fbState_;
    }

    double sr_ = 48000.0;
    int    model_ = 0;
    bool   bypass_ = true;
    float  k_[kMaxKnobs] {};
    int    sw_[kMaxSwitch] {};

    Line   long_, mod_;
    Tank   tank_;
    pedal::OnePole fbLp_, fbHp_, toneLp_, wetHp_, wetLp_;
    pedal::Peak    spring_;
    float  lfo_ = 0.f, lfo2_ = 0.f;
    float  riseEnv_ = 0.f, fbState_ = 0.f, gateSm_ = 1.f, stepPhase_ = 0.f;
    int    step_ = 0;
};

} // namespace fxpedal
