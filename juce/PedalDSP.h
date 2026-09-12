// PedalDSP.h — algoritmi dei pedali emulabili.
//
// Header-only, monocanale, senza dipendenze da JUCE, come il resto di
// PreampFX.h. Un'istanza per canale.
//
// Le topologie seguono gli schemi pubblici dei circuiti classici:
//
// - clipping morbido NELL'ANELLO di reazione di uno stadio non invertente
//   (famiglia overdrive): il guadagno e' (Rf/Rg)+1, circa x7.8 a x107.8, e i
//   diodi in parallelo a Rf comprimono progressivamente. Asimmetrico quando i
//   diodi sono due da un lato e uno dall'altro, il che lascia in evidenza la
//   seconda armonica.
// - un condensatore in serie a Rg limita il guadagno sulle basse: da qui il
//   passa-alto interno all'anello intorno ai 720 Hz, che evita l'impasto.
// - clipping duro VERSO MASSA dopo lo stadio di guadagno (famiglia distorsore):
//   la forma d'onda viene tosata a soglia fissa, molto piu' squadrata.
// - rete di tono a medi scavati per i distorsori classici, bilanciamento
//   semplice per gli overdrive, tre bande piu' medio parametrico per il
//   modello a doppio stadio.
//
// Riferimenti consultati: electricdruid.net "Designing a classic overdrive"
// per la famiglia OD-1/TS-808/SD-1; electrosmash per il DS-1.
#pragma once

#include "PedalRegistry.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace pedal {

// --- mattoni ---------------------------------------------------------------

class OnePole
{
public:
    void setCutoff (float fc, double sr, bool highpass)
    {
        a_  = std::exp (-2.0f * 3.14159265f * std::max (1.f, fc) / (float) sr);
        hp_ = highpass;
    }
    void  reset() { z_ = 0.f; }
    float process (float in)
    {
        z_ = in * (1.f - a_) + z_ * a_;
        return hp_ ? in - z_ : z_;
    }
private:
    float a_ = 0.f, z_ = 0.f;
    bool  hp_ = false;
};

// Campana, per l'equalizzatore a tre bande del modello a doppio stadio.
class Peak
{
public:
    void set (float fc, float q, float gainDB, double sr)
    {
        const double A  = std::pow (10.0, (double) gainDB / 40.0);
        const double w0 = 2.0 * 3.14159265358979 * (double) fc / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double al = sw / (2.0 * (double) q);
        const double nb0 = 1.0 + al * A, nb1 = -2.0 * cw, nb2 = 1.0 - al * A;
        const double na0 = 1.0 + al / A, na1 = -2.0 * cw, na2 = 1.0 - al / A;
        b0_ = (float) (nb0 / na0); b1_ = (float) (nb1 / na0); b2_ = (float) (nb2 / na0);
        a1_ = (float) (na1 / na0); a2_ = (float) (na2 / na0);
    }
    void  reset() { z1_ = z2_ = 0.f; }
    float process (float x)
    {
        const float y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }
private:
    float b0_ = 1.f, b1_ = 0.f, b2_ = 0.f, a1_ = 0.f, a2_ = 0.f;
    float z1_ = 0.f, z2_ = 0.f;
};

// Clipping morbido: con sym = 0 la soglia negativa e' piu' bassa, perche' da
// quel lato conduce un diodo solo.
inline float softClip (float x, float sym)
{
    const float k = (x >= 0.f) ? 1.0f : (0.55f + 0.45f * sym);
    return std::tanh (x / k) * k;
}

// Clipping duro verso massa, con il ginocchio appena arrotondato per non
// generare alias a valanga.
inline float hardClip (float x, float th)
{
    if (x >  th) return  th + std::tanh ((x - th) * 3.f) * 0.06f;
    if (x < -th) return -th + std::tanh ((x + th) * 3.f) * 0.06f;
    return x;
}

// --- il pedale -------------------------------------------------------------

class PedalFX
{
public:
    void prepare (double sr)
    {
        sr_ = (sr > 0.0) ? sr : 48000.0;
        inHP_   .setCutoff (12.f,   sr_, true);
        loopHP_ .setCutoff (720.f,  sr_, true);   // taglio bassi dentro l'anello
        postLP_ .setCutoff (7000.f, sr_, false);
        bodyLP_ .setCutoff (120.f,  sr_, false);
        toneHP_ .setCutoff (700.f,  sr_, true);
        colorLo_.setCutoff (250.f,  sr_, false);
        colorHi_.setCutoff (2200.f, sr_, true);
        boostHP_.setCutoff (80.f,   sr_, true);
        updateEq();
        reset();
    }

    void reset()
    {
        inHP_.reset(); loopHP_.reset(); postLP_.reset(); bodyLP_.reset();
        toneHP_.reset(); colorLo_.reset(); colorHi_.reset(); boostHP_.reset();
        low_.reset(); mid_.reset(); high_.reset();
        for (auto& b : band_) b.reset();
        dc_ = 0.f; env_ = 0.f; gain_ = 1.f;
    }

    void setBypass (bool b) { bypass_ = b; }

    void setModel (int idx)
    {
        if (idx != model_) { model_ = idx; reset(); }
    }
    // I valori arrivano gia' nell'intervallo reale dichiarato dal modello.
    void setKnob   (int i, float v) { if (i >= 0 && i < kMaxKnobs)  k_[(std::size_t) i] = v; }
    void setSwitch (int i, int   v) { if (i >= 0 && i < kMaxSwitch) s_[(std::size_t) i] = v; }

    // Da chiamare dopo aver aggiornato i controlli: ricalcola i coefficienti che
    // non si possono ricavare campione per campione.
    void commit()
    {
        updateEq();
        if (at (model_).topo == Topology::Boost)
            boostHP_.setCutoff (k_[1], sr_, true);
    }

    float process (float x)
    {
        if (bypass_) return x;
        const Model& m = at (model_);

        switch (m.topo)
        {
            case Topology::Clean:
                return x * k_[0];

            case Topology::Boost:
                return boostHP_.process (x) * std::pow (10.f, k_[0] / 20.f);

            case Topology::OdSoftSym:
            case Topology::OdSoftAsym:
            case Topology::OdJfet:
                return overdrive (inHP_.process (x), m);

            case Topology::DistHard:
            case Topology::DistTurbo:
            case Topology::DistBody:
                return distortion (inHP_.process (x), m);

            case Topology::HgColor:
            case Topology::HgZone:
                return highGain (inHP_.process (x), m);

            case Topology::FuzzGate:
                return fuzz (inHP_.process (x), m);

            case Topology::GateSuppress:
            case Topology::GateHard:
                return gate (x, m);

            case Topology::EqGraphic7:
            case Topology::EqParametric:
                return equaliser (x, m);
        }
        return x;
    }

private:
    // --- famiglia overdrive: clipping dentro l'anello ---------------------
    float overdrive (float x, const Model& m)
    {
        const bool  custom = (m.numSwitches > 0 && s_[0] == 1);
        const float drive = k_[0], tone = k_[1], lvl = k_[2];

        // Guadagno dello stadio non invertente, x7.8 .. x107.8.
        float gain = 7.8f + drive * 100.f;
        if (custom) gain *= 1.6f;

        // Il condensatore in serie a Rg amplifica meno le basse: e' questo che
        // tiene pulito il fondo. In Custom il taglio si apre e passa piu' corpo.
        const float bassGain = custom ? 0.70f : 0.35f;
        const float hi = loopHP_.process (x);
        const float lo = x - hi;
        float v = (hi + lo * bassGain) * gain;

        v = softClip (v, m.topo == Topology::OdSoftAsym ? 0.f : 1.f);

        // Il verso piu' aperto dei circuiti a JFET: meno compressione statica.
        if (m.topo == Topology::OdJfet)
            v = v * 0.7f + std::tanh (v * 0.8f) * 0.3f;

        v = tilt (v, tone);
        v = postLP_.process (v);
        return v * (0.25f + lvl * 1.5f);
    }

    // --- famiglia distorsore: clipping duro verso massa -------------------
    float distortion (float x, const Model& m)
    {
        const bool  custom = (m.numSwitches > 0 && s_[0] == 1);
        const float drive = k_[0], tone = k_[1], lvl = k_[2];

        float gain = 20.f + drive * 180.f;
        if (custom && m.topo == Topology::DistHard) gain *= 0.8f;   // comprime meno

        const float th = (custom && m.topo == Topology::DistHard) ? 0.42f : 0.32f;
        float v = hardClip (x * gain, th);

        // Modo II: secondo stadio in cascata, attacco piu' spesso.
        if (m.topo == Topology::DistTurbo && s_[0] == 1)
            v = hardClip (v * 3.2f, th * 0.85f);

        // Corpo dedicato: rientro controllato delle basse.
        if (m.topo == Topology::DistBody && m.numKnobs > 3)
            v += bodyLP_.process (v) * (k_[3] * 1.2f);

        v = scoop (v, tone);
        v = postLP_.process (v);
        return v * (0.2f + lvl * 1.2f);
    }

    // --- alto guadagno ----------------------------------------------------
    float highGain (float x, const Model& m)
    {
        const bool  custom = (m.numSwitches > 0 && s_[0] == 1);
        const float drive = k_[0], lvl = k_[1];

        // Due stadi in cascata: e' cio' che rende il timbro cosi' saturo.
        // In Custom il secondo stadio tosa piu' basso e spinge di piu': su un
        // clipping duro e' la SOGLIA a decidere il timbro, non il guadagno che
        // la precede, perche' oltre il ginocchio la forma d'onda e' gia' piatta.
        float v = softClip (x * (25.f + drive * 200.f), 1.f);
        v = hardClip (v * (custom ? 5.0f : 3.0f), custom ? 0.24f : 0.35f);

        if (m.topo == Topology::HgColor) {
            // Due filtri di colore indipendenti, sommati al segnale.
            const float lo = colorLo_.process (v) * (k_[2] * 2.0f);
            const float hi = colorHi_.process (v) * (k_[3] * 2.0f);
            v = v * 0.35f + lo + hi;
        } else {
            v = low_ .process (v);
            v = mid_ .process (v);
            v = high_.process (v);
        }
        v = postLP_.process (v);
        return v * (0.15f + lvl * 1.1f);
    }

    // --- fuzz -------------------------------------------------------------
    float fuzz (float x, const Model& m)
    {
        const bool  custom = (m.numSwitches > 0 && s_[0] == 1);
        const float amt = k_[0], tone = k_[1], lvl = k_[2];

        // Standard assottiglia l'ingresso prima di saturare, ed e' da li' che
        // viene il carattere nasale; Custom lascia passare il corpo.
        const float hi = loopHP_.process (x);
        const float body = custom ? 0.85f : 0.25f;
        float v = (hi + (x - hi) * body) * (30.f + amt * 300.f);

        // Componente continua che si accumula: il carattere "strozzato".
        dc_ = dc_ * 0.999f + v * 0.001f;
        v -= dc_ * (custom ? 0.3f : 0.7f);
        v = std::tanh (hardClip (v, custom ? 0.34f : 0.24f) * 1.8f);
        v = tilt (v, tone);
        return v * (0.2f + lvl * 1.1f);
    }

    // --- gate -------------------------------------------------------------
    // Inseguitore di inviluppo con attacco immediato e rilascio governato dal
    // decadimento. Reduction attenua il fondo lasciando passare la coda, Mute
    // chiude del tutto: e' la differenza che si sente fra i due modi.
    float gate (float x, const Model& m)
    {
        const float thDB   = k_[0];
        const float decayMs = std::max (1.f, k_[1]);
        const bool  mute   = (m.topo == Topology::GateHard) || (m.numSwitches > 0 && s_[0] == 1);

        const float a = std::exp (-1.0f / (float) (sr_ * decayMs * 0.001));
        const float lvl = std::fabs (x);
        env_ = (lvl > env_) ? lvl : (env_ * a + lvl * (1.f - a));

        const float envDB = 20.f * std::log10 (std::max (env_, 1.0e-7f));
        // Ginocchio di 6 dB: sotto soglia la chiusura e' progressiva, non a
        // gradino, altrimenti il gate "respira" udibilmente sulle code.
        const float over = envDB - thDB;
        float target;
        if (over >= 0.f)        target = 1.f;
        else if (over > -6.f)   target = 1.f + over / 6.f;
        else                    target = mute ? 0.f : 0.12f;   // Reduction lascia un filo

        // Rampa del guadagno, per non produrre scalini.
        const float ga = std::exp (-1.0f / (float) (sr_ * 0.005));
        gain_ = gain_ * ga + target * (1.f - ga);
        return x * gain_;
    }

    // --- equalizzatori ----------------------------------------------------
    float equaliser (float x, const Model& m)
    {
        float v = x;
        if (m.topo == Topology::EqGraphic7) {
            for (int i = 0; i < 7; ++i) v = band_[i].process (v);
            return v * std::pow (10.f, k_[7] / 20.f);
        }
        v = band_[0].process (v);
        v = band_[1].process (v);
        return v * std::pow (10.f, k_[4] / 20.f);
    }

    // --- reti di tono -----------------------------------------------------
    // Bilanciamento fra gravi e acuti attorno a un perno.
    float tilt (float v, float t)
    {
        const float hi = toneHP_.process (v);
        const float lo = v - hi;
        return lo * (1.4f - t) + hi * (0.2f + t * 1.6f);
    }

    // Rete a medi scavati dei distorsori classici.
    float scoop (float v, float t)
    {
        const float hi = toneHP_.process (v);
        const float lo = v - hi;
        return lo * (1.2f - t) * 0.55f + hi * (0.3f + t * 1.5f) + v * 0.15f;
    }

    void updateEq()
    {
        const Model& m = at (model_);

        if (m.topo == Topology::EqGraphic7) {
            // Le sette frequenze fisse del grafico classico.
            static const float kF[7] = { 100.f, 200.f, 400.f, 800.f, 1600.f, 3200.f, 6400.f };
            for (int i = 0; i < 7; ++i) band_[i].set (kF[i], 1.4f, k_[i], sr_);
            return;
        }
        if (m.topo == Topology::EqParametric) {
            band_[0].set (std::clamp (k_[1],  40.f, 1000.f), 1.0f, k_[0], sr_);
            band_[1].set (std::clamp (k_[3], 500.f, 8000.f), 1.0f, k_[2], sr_);
            return;
        }
        if (m.topo != Topology::HgZone) return;
        low_ .set (100.f,  0.8f, k_[2], sr_);
        high_.set (3200.f, 0.8f, k_[3], sr_);
        mid_ .set (std::clamp (k_[5], 200.f, 5000.f), 1.1f, k_[4], sr_);
    }

    double sr_     = 48000.0;
    int    model_  = 0;
    bool   bypass_ = false;
    std::array<float, kMaxKnobs>  k_ { { 0.f } };
    std::array<int,   kMaxSwitch> s_ { { 0 } };

    OnePole inHP_, loopHP_, postLP_, bodyLP_, toneHP_, colorLo_, colorHi_, boostHP_;
    Peak    low_, mid_, high_;
    Peak    band_[7];               // bande dell'equalizzatore grafico
    float   dc_   = 0.f;
    float   env_  = 0.f;            // inviluppo del gate
    float   gain_ = 1.f;            // guadagno del gate, con rampa
};

} // namespace pedal
