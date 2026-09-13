// AmpDSP.h — algoritmi degli amplificatori a riserva condivisa.
//
// Header-only, monocanale, senza dipendenze da JUCE, come PedalDSP.h e FxDSP.h.
// Un'istanza per canale audio.
//
// Lo schema e' sempre lo stesso e viene dai circuiti veri: una cascata di stadi
// a triodo, una torre di tono passiva fra il preamplificatore e il finale, poi
// il finale con la sua voce, il suo cedimento e l'anello di controreazione da
// cui esce la presenza. Quello che distingue una testa dall'altra e' dove si
// mettono le cose e quanto ce n'e':
//
//  - quanti stadi prima della torre di tono, e con che polarizzazione;
//  - se la torre e' una sola per tutti i canali o una per canale;
//  - dove si taglia il fondo prima del guadagno, che e' la cosa che decide se
//    una testa a guadagno alto resta leggibile o impasta;
//  - quanto e' stretto l'anello di controreazione, cioe' quanto il finale
//    "tiene" invece di allentarsi.
#pragma once

#include "AmpRegistry.h"
#include "PedalDSP.h"     // pedal::OnePole, pedal::Peak
#include <cmath>
#include <algorithm>

namespace ampmodel {

class AmpFX
{
public:
    void prepare (double sr)
    {
        sr_ = (sr > 0.0) ? sr : 48000.0;
        preHP_ .setCutoff (90.f,   sr_, true);
        tightHP_.setCutoff (180.f, sr_, true);
        shLo_  .setCutoff (250.f,  sr_, true);
        shHi_  .setCutoff (2200.f, sr_, true);
        presHP_.setCutoff (2800.f, sr_, true);
        deepLP_.setCutoff (120.f,  sr_, false);
        brightHP_.setCutoff (1200.f, sr_, true);
        sagDown_ = std::exp (-1.0f / (float) (sr_ * 0.005));
        sagUp_   = std::exp (-1.0f / (float) (sr_ * 0.250));
        reset();
    }

    void reset()
    {
        preHP_.reset(); tightHP_.reset(); shLo_.reset(); shHi_.reset();
        presHP_.reset(); deepLP_.reset(); brightHP_.reset();
        midBell_.reset();
        for (auto& b : graphic_) b.reset();
        sag_ = 1.f;
    }

    void setEnabled (bool on)        { enabled_ = on; }
    void setModel   (int i)          { model_ = std::clamp (i, 0, count() - 1); }
    void setKnob    (int i, float v) { if (i >= 0 && i < kMaxKnobs)  k_[i]  = v; }
    void setSwitch  (int i, int v)   { if (i >= 0 && i < kMaxSwitch) s_[i]  = v; }

    void commit()
    {
        const Model& m = at (model_);
        ch_ = (m.channelSwitch >= 0)
            ? std::clamp (s_[m.channelSwitch], 0, m.numChannels - 1) : 0;

        // I comandi del canale attivo si raccolgono qui una volta per blocco,
        // cosi' il ciclo dei campioni non deve cercarli.
        g_ = b_ = mi_ = t_ = 0.f; volDB_ = -12.f; focus_ = 0.f;
        presDB_ = deepDB_ = 0.f; masterDB_ = 0.f;
        for (int i = 0; i < m.numKnobs; ++i) {
            const auto& kn = m.knobs[i];
            const float v = k_[i];
            const bool mine = (kn.group == ch_);
            const juce_like label = kn.label;
            if (kn.group == -1) {
                if (same (label, "PRESENCE")) presDB_ = v;
                else if (same (label, "DEPTH")) deepDB_ = v;
                else if (same (label, "MASTER")) masterDB_ = v;
                else if (same (label, "BASS"))   b_  = v;
                else if (same (label, "MIDDLE")) mi_ = v;
                else if (same (label, "TREBLE")) t_  = v;
                else if (same (label, "FEEDBACK")) fbAmt_ = v;
            } else if (mine) {
                if (same (label, "GAIN"))        g_ = v;
                else if (same (label, "BASS"))   b_  = v;
                else if (same (label, "MIDDLE")) mi_ = v;
                else if (same (label, "TREBLE")) t_  = v;
                else if (same (label, "VOLUME")) volDB_ = v;
                else if (same (label, "FOCUS"))  focus_ = v;
                else if (same (label, "PRESENCE")) presDB_ = v;
            }
        }

        midBell_.set (650.f, 0.9f, mi_, sr_);
        if (m.topo == Topology::Mark5) {
            static const float kF[5] = { 80.f, 240.f, 750.f, 2200.f, 6600.f };
            const int base = m.numKnobs - 5;
            for (int i = 0; i < 5; ++i)
                graphic_[i].set (kF[i], 1.3f, (s_[2] == 1) ? k_[base + i] : 0.f, sr_);
        }
        // La messa a fuoco stringe il fondo prima del guadagno.
        tightHP_.setCutoff (90.f + focus_ * 260.f, sr_, true);
    }

    float process (float x)
    {
        if (! enabled_) return x;
        const Model& m = at (model_);

        // --- cedimento del finale -----------------------------------------
        // Al calare della potenza il trasformatore cede prima: la testa a
        // potenza ridotta si schiaccia molto piu' presto, ed e' il motivo per
        // cui si suona a dieci watt e non a novanta in sala prove.
        float sagAmt = 0.9f;
        if (m.topo == Topology::Mark5) {
            static const float kS[3] = { 2.4f, 1.4f, 0.8f };   // 10, 45, 90 W
            sagAmt = kS[std::clamp (s_[3], 0, 2)];
        }
        {
            // Non basta far cedere piu' in fretta: col tetto uguale per tutte
            // le potenze si arriva comunque allo stesso fondo e le tre
            // posizioni suonano identiche. A potenza ridotta deve cedere di
            // piu' e risalire piu' piano, che e' quello che si sente.
            const float lvl = std::fabs (x);
            const float maxSag = std::min (0.55f, 0.12f + sagAmt * 0.16f);
            const float target = 1.f - std::min (maxSag, lvl * sagAmt);
            const float up = sagUp_ + (1.f - sagUp_) * (0.35f / std::max (0.5f, sagAmt));
            sag_ = (target < sag_) ? (sag_ * sagDown_ + target * (1.f - sagDown_))
                                   : (sag_ * up      + target * (1.f - up));
        }

        float v = preHP_.process (x);
        v = tightHP_.process (v);

        switch (m.topo) {
            case Topology::Slo:      v = preSlo (v);     break;
            case Topology::Ecstasy:  v = preEcstasy (v); break;
            case Topology::Vh4:      v = preVh4 (v);     break;
            case Topology::Trinity:  v = preTrinity (v); break;
            case Topology::Revo:     v = preRevo (v);    break;
            case Topology::Mark5:    v = preMark5 (v);   break;
        }

        // --- torre di tono -------------------------------------------------
        v = shelfLo (v, b_);
        v = midBell_.process (v);
        v = shelfHi (v, t_);

        if (m.topo == Topology::Mark5 && s_[2] == 1)
            for (auto& band : graphic_) v = band.process (v);

        // Perdita d'inserzione della torre di tono. Una torre passiva butta via
        // dai dieci ai venti decibel, e senza questa perdita il finale arriva
        // gia' saturo: la torre e l'equalizzatore grafico non si sentirebbero
        // quasi, perche' ogni taglio verrebbe subito ricompresso, e il
        // cedimento del finale non si vedrebbe affatto.
        v *= 0.16f;

        // --- finale ---------------------------------------------------------
        // La controreazione decide quanto il finale "tiene": stretta, resta
        // fermo e duro; allentata, si muove e diventa morbido.
        const float fb = (m.topo == Topology::Trinity) ? (0.4f + fbAmt_ * 0.6f) : 1.f;
        v = tube (v * sag_ * (2.6f / std::max (0.4f, fb)), 1.3f, 0.015f) * 4.2f;

        v *= db2lin (volDB_ + masterDB_);

        // Presenza e profondita' stanno nell'anello, quindi dopo il finale.
        v += presHP_.process (v) * (std::pow (10.f, presDB_ / 20.f) - 1.f);
        v += deepLP_.process (v) * (std::pow (10.f, deepDB_ / 20.f) - 1.f);
        return v;
    }

private:
    using juce_like = const char*;
    static bool same (const char* a, const char* b) { return std::strcmp (a, b) == 0; }
    static float db2lin (float dB) { return std::pow (10.f, dB * 0.05f); }

    static float tube (float x, float pre, float bias)
    { return std::tanh (x * pre + bias) - std::tanh (bias); }

    float shelfLo (float v, float dB)
    { const float hi = shLo_.process (v); return (v - hi) * db2lin (dB) + hi; }
    float shelfHi (float v, float dB)
    { const float hi = shHi_.process (v); return (v - hi) + hi * db2lin (dB); }

    // --- preamplificatori --------------------------------------------------
    // Due stadi sul pulito, quattro sul canale spinto: e' la cascata lunga a
    // dare il sustain senza bisogno di alzare il volume.
    float preSlo (float v)
    {
        const bool od = (ch_ == 1);
        const int stages = od ? 4 : (s_[1] == 1 ? 3 : 2);
        float pre = 2.f + g_ * (od ? 55.f : 18.f);
        for (int i = 0; i < stages; ++i) {
            v = tube (v, i == 0 ? pre : 2.0f, 0.03f);
            if (i == 0 && od) v = tightHP_.process (v);
        }
        return v;
    }

    // La struttura alta aggiunge uno stadio e alza la polarizzazione; la
    // pre-equalizzazione decide quanto fondo entra nel guadagno, ed e' quella a
    // cambiare il carattere piu' del guadagno stesso.
    float preEcstasy (float v)
    {
        const int strutt = std::clamp (s_[1], 0, 1);
        const int preEq  = std::clamp (s_[2], 0, 2);
        if (preEq >= 1) v -= brightHP_.process (v) * (preEq == 1 ? 0.25f : 0.5f) * -1.f;
        const int stages = (ch_ == 0 ? 2 : (ch_ == 1 ? 3 : 4)) + strutt;
        const float pre = 2.f + g_ * (ch_ == 0 ? 16.f : (ch_ == 1 ? 34.f : 58.f));
        for (int i = 0; i < stages; ++i)
            v = tube (v, i == 0 ? pre : 1.9f, strutt ? 0.01f : 0.04f);
        if (s_[3] == 1) v = tube (v, 1.1f, 0.05f);     // classe A: piu' seconda armonica
        return v;
    }

    // Quattro canali separati: cambia il numero di stadi e quanto si taglia il
    // fondo prima del guadagno.
    float preVh4 (float v)
    {
        static const int kSt[4]  = { 2, 3, 4, 4 };
        static const float kG[4] = { 14.f, 30.f, 55.f, 70.f };
        if (ch_ >= 2) v = tightHP_.process (v);
        const float pre = 2.f + g_ * kG[ch_];
        for (int i = 0; i < kSt[ch_]; ++i)
            v = tube (v, i == 0 ? pre : 2.1f, ch_ >= 2 ? 0.012f : 0.04f);
        return v;
    }

    float preTrinity (float v)
    {
        static const int kSt[3]  = { 2, 3, 4 };
        static const float kG[3] = { 15.f, 32.f, 60.f };
        const float pre = 2.f + g_ * kG[ch_];
        for (int i = 0; i < kSt[ch_]; ++i)
            v = tube (v, i == 0 ? pre : 2.0f, 0.03f);
        return v;
    }

    float preRevo (float v)
    {
        static const int kSt[3]  = { 2, 3, 4 };
        static const float kG[3] = { 16.f, 36.f, 64.f };
        const float pre = 2.f + g_ * kG[ch_];
        for (int i = 0; i < kSt[ch_]; ++i)
            v = tube (v, i == 0 ? pre : 2.0f, 0.025f);
        return v;
    }

    // Il modo del canale cambia quanti stadi e quanto stretto: e' il selettore
    // che sposta questa famiglia da una voce all'altra piu' del guadagno.
    float preMark5 (float v)
    {
        const int mode = std::clamp (s_[1], 0, 2);
        static const int kSt[3][3] = { { 2, 2, 3 }, { 3, 3, 4 }, { 4, 4, 5 } };
        static const float kG[3]   = { 16.f, 38.f, 62.f };
        const int stages = kSt[std::clamp (ch_, 0, 2)][mode];
        const float pre = 2.f + g_ * kG[std::clamp (ch_, 0, 2)];
        for (int i = 0; i < stages; ++i) {
            v = tube (v, i == 0 ? pre : 2.1f, mode == 2 ? 0.01f : 0.035f);
            if (i == 0 && ch_ == 2) v = tightHP_.process (v);
        }
        return v;
    }

    double sr_ = 48000.0;
    int    model_ = 0, ch_ = 0;
    bool   enabled_ = false;
    float  k_[kMaxKnobs] {};
    int    s_[kMaxSwitch] {};

    pedal::OnePole preHP_, tightHP_, shLo_, shHi_, presHP_, deepLP_, brightHP_;
    pedal::Peak    midBell_;
    pedal::Peak    graphic_[5];

    float g_ = 0.5f, b_ = 0.f, mi_ = 0.f, t_ = 0.f;
    float volDB_ = -12.f, masterDB_ = 0.f, presDB_ = 0.f, deepDB_ = 0.f;
    float focus_ = 0.f, fbAmt_ = 0.6f;
    float sag_ = 1.f, sagDown_ = 0.f, sagUp_ = 0.f;
};

} // namespace ampmodel
