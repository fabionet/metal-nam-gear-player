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
        shA_    .setCutoff (250.f,  sr_, true);   // spartiacque bassi/resto
        shB_    .setCutoff (2200.f, sr_, true);   // spartiacque resto/acuti
        shC_    .setCutoff (4000.f, sr_, true);   // spinta sugli acuti
        tiltA_  .setCutoff (900.f,  sr_, true);   // perno della bilancia
        ratPre_ .setCutoff (1000.f, sr_, true);   // enfasi prima della tosatura
        ratPost_.setCutoff (2000.f, sr_, false);  // filtro, al contrario
        muffA_  .setCutoff (100.f,  sr_, true);   // fra i due stadi del muff
        // I due rami del tono devono essere distanti, non incrociarsi: e' lo
        // spazio fra loro a fare la conca sui medi. Incrociandoli a 1,8 kHz la
        // somma tornava piatta e del muff non restava niente.
        muffB_  .setCutoff (700.f,  sr_, false);  // ramo scuro
        muffC_  .setCutoff (2600.f, sr_, true);   // ramo chiaro
        smoothLP_.setCutoff (6500.f, sr_, false);
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
        shA_.reset(); shB_.reset(); shC_.reset(); tiltA_.reset();
        ratPre_.reset(); ratPost_.reset();
        muffA_.reset(); muffB_.reset(); muffC_.reset(); smoothLP_.reset();
        bandA_.reset(); bandB_.reset(); bandC_.reset();
        optFast_ = optSlow_ = 1.f;
        low_.reset(); mid_.reset(); high_.reset();
        for (auto& b : band_) b.reset();
        dc_ = 0.f; env_ = 0.f; gain_ = 1.f; cEnv_ = 0.f; cGain_ = 1.f; grDb_ = 0.f;
        slew_ = 0.f; sagEnv_ = 1.f;
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
        updateComp();
    }

    // Riduzione di guadagno in corso, in dB negativi. Zero quando il pedale
    // scelto non e' un compressore o e' in bypass.
    float gainReductionDB() const { return grDb_; }

    float process (float x)
    {
        if (bypass_) { grDb_ = 0.f; return x; }
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

            case Topology::EqNative:
                return x;       // ci pensa la torre di tono nativa

            case Topology::EqGraphic7:
            case Topology::EqParametric:
                return equaliser (x, m);

            case Topology::CompSustain:
            case Topology::CompSimple:
            case Topology::CompLimiter:
                return compressor (x, m);

            // --- seconda serie ---------------------------------------------
            case Topology::OdKlon:          return odKlon    (inHP_.process (x));
            case Topology::OdTwoBand:       return odTwoBand (inHP_.process (x));
            case Topology::OdSmooth:        return odSmooth  (inHP_.process (x));
            case Topology::DistRat:         return rat (inHP_.process (x), 0.45f, 2.5f);
            case Topology::DistRatLed:      return rat (inHP_.process (x), 1.05f, 3.5f);
            case Topology::DistThick:       return distThick (inHP_.process (x));
            case Topology::DistFixedHi:     return distFixedHi (inHP_.process (x));
            case Topology::HgMuffMetal:     return hgMuffMetal (inHP_.process (x));
            case Topology::HgFiveBand:      return hgFiveBand (inHP_.process (x));
            case Topology::FuzzMuff:        return fuzzMuff (inHP_.process (x));
            case Topology::FuzzControlled:  return fuzzControlled (inHP_.process (x));
            case Topology::BoostTransistor: return boostTransistor (x);
            case Topology::BoostClean:      return x * std::pow (10.f, k_[0] / 20.f);
            case Topology::GateReduction:   return gateReduction (x);
            case Topology::GateExpander:    return gateExpander (x);
            case Topology::EqGraphic6:      return eqGraphic6 (x);
            case Topology::EqKnockout:      return eqKnockout (x);
            case Topology::CompOpto:        return compOpto (x);

            // --- terza serie ------------------------------------------------
            case Topology::DistRatVintage:  return rat (inHP_.process (x), 0.45f, 2.5f, true);
            case Topology::OdFetBox:        return odFetBox (inHP_.process (x));
            case Topology::OdTubeLike:      return odTubeLike (inHP_.process (x));
            case Topology::DistTwoChan:     return distTwoChan (inHP_.process (x));
            case Topology::DistBritish:     return distBritish (inHP_.process (x));
            case Topology::BoostFetDiode:   return boostFetDiode (inHP_.process (x));
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

    // Gli esponenziali stanno qui e non nel ciclo dei campioni.
    void updateComp()
    {
        const Model& m = at (model_);
        if (m.cat != Category::Compressor) { grDb_ = 0.f; return; }
        const bool limiter = (m.topo == Topology::CompLimiter);
        const float atkMs  = limiter ? 5.f  : std::max (0.1f, k_[1]);
        const float relMs  = limiter ? std::max (1.f, k_[2]) : 200.f;
        cAtk_  = std::exp (-1.0f / (float) (sr_ * atkMs * 0.001));
        cRel_  = std::exp (-1.0f / (float) (sr_ * relMs * 0.001));
        cDetA_ = std::exp (-1.0f / (float) (sr_ * 0.001));      // 1 ms
        cDetR_ = std::exp (-1.0f / (float) (sr_ * 0.120));      // 120 ms
    }

    // ======================= seconda serie ==============================
    // Una bilancia attorno a un perno: quanto alza da un lato tanto toglie
    // dall'altro, cosi' il livello percepito non cambia.
    float tilt (OnePole& hp, float v, float dB)
    {
        const float hi = hp.process (v);
        const float lo = v - hi;
        const float g  = std::pow (10.f, dB / 40.f);
        return lo / g + hi * g;
    }
    float shelfLo (OnePole& hp, float v, float dB)
    {
        const float hi = hp.process (v);
        return (v - hi) * std::pow (10.f, dB / 20.f) + hi;
    }
    float shelfHi (OnePole& hp, float v, float dB)
    {
        const float hi = hp.process (v);
        return (v - hi) + hi * std::pow (10.f, dB / 20.f);
    }

    // Un anello pulito in parallelo a quello che distorce: il diretto non viene
    // mai perso, e per questo il pedale "non si sente" finche' non lo si spinge.
    float odKlon (float x)
    {
        const float d = k_[0];
        const float g = 1.f + d * 45.f;
        const float wet = softClip (x * g, 1.f) / (1.f + d * 2.f);
        float v = x * (1.f - d * 0.45f) + wet * (0.35f + d * 1.1f);
        v = tilt (tiltA_, v, k_[1]);
        return v * k_[2];
    }

    // Clipping morbido con due bande separate invece del solo tono.
    float odTwoBand (float x)
    {
        const float g = 6.f + k_[3] * 90.f;
        float v = softClip (loopHP_.process (x) * g, 1.f) / (1.f + k_[3] * 3.f);
        v = shelfLo (shA_, v, k_[1]);
        v = shelfHi (shB_, v, k_[2]);
        return v * k_[0];
    }

    // Molto dolce e molto compresso: la tosatura arriva presto ma con un
    // ginocchio lunghissimo, e il passa-basso in coda toglie il sibilo.
    float odSmooth (float x)
    {
        const float g = 4.f + k_[2] * 55.f;
        float v = std::tanh (x * g * 0.6f) / (1.f + k_[2] * 1.8f);
        v = smoothLP_.process (v);
        v = tilt (tiltA_, v, (k_[1] - 0.5f) * 14.f);
        return v * k_[0];
    }

    // Op-amp a guadagno altissimo, enfasi sugli acuti prima della tosatura e
    // due diodi verso massa.
    //
    // Fra la versione al silicio e quella coi LED non basta cambiare la soglia:
    // col guadagno a fondo scala l'onda e' gia' piatta in entrambe e non si
    // sentirebbe nulla. Quello che cambia davvero e' il rapporto fra guadagno e
    // soglia, cioe' quanto del segnale resta sotto il ginocchio: i LED
    // conducono a quasi un volt e mezzo, quindi lo stesso pedale tosa molto
    // piu' tardi, resta piu' dinamico e va alzato di piu'. Il ginocchio dei LED
    // e' anche piu' netto, perche' conducono piu' bruscamente.
    float rat (float x, float th, float knee, bool slowOpAmp = false)
    {
        // Il guadagno parte da uno: a comando chiuso il pedale sporca appena,
        // com'e' giusto. Partendo da otto, come faceva la prima stesura, l'onda
        // era gia' piatta a fondo corsa chiusa e le due versioni suonavano
        // identiche.
        const float g = 1.f + k_[0] * 150.f;
        float v = x * g;
        v += ratPre_.process (v) * 0.8f;          // la gobba prima dei diodi

        // Il vecchio op-amp ha una velocita' di salita bassa: alla massima
        // escursione non tiene il passo sopra i 2,6 kHz circa, e il fronte
        // dell'onda gli esce arrotondato. E' un limite di pendenza, non un
        // filtro: agisce solo quando il segnale si muove in fretta, quindi
        // lascia stare le note tenute e smussa gli attacchi e la tosatura.
        if (slowOpAmp) {
            const float maxStep = 6.2831853f * 2600.f * th / (float) sr_;
            const float d = v - slew_;
            slew_ += std::max (-maxStep, std::min (maxStep, d));
            v = slew_;
        }
        // Non si normalizza per la soglia: la versione coi LED deve restare
        // piu' forte, perche' lo e'.
        v = std::tanh (v * knee / th) * th;
        // Il filtro lavora al contrario: piu' si apre in senso orario, piu'
        // chiude gli acuti. Zero lo lascia aperto.
        const float lp = ratPost_.process (v);
        v = v * (1.f - k_[1]) + lp * k_[1];
        return v * k_[2] * 1.6f;
    }

    // Clipping duro ma con i medi spinti invece che scavati: il corpo resta.
    float distThick (float x)
    {
        const float g = 25.f + k_[2] * 160.f;
        float v = hardClip (x * g, 0.5f) / 0.5f;
        v = bandA_.process (v);                   // gobba sui medi
        v = tilt (tiltA_, v, (k_[1] - 0.5f) * 16.f);
        return v * k_[0] * 0.5f;
    }

    // Guadagno inchiodato: si regola solo l'equalizzazione, che e' il punto.
    float distFixedHi (float x)
    {
        float v = hardClip (x * 260.f, 0.35f) / 0.35f;
        v = shelfLo (shA_, v, k_[1]);
        v = bandB_.process (v);
        v = shelfHi (shB_, v, k_[3]);
        return v * k_[0] * 0.4f;
    }

    // Quattro stadi in cascata, con un passa-alto fra l'uno e l'altro perche'
    // le basse non si impastino, e la spinta sugli acuti commutabile in coda.
    float hgMuffMetal (float x)
    {
        const float g = 4.f + k_[3] * 26.f;
        float v = x * g;
        for (int i = 0; i < 4; ++i) v = softClip (v * 1.9f, 0.8f);
        v = muffA_.process (v);
        v = shelfLo (shA_, v, k_[1]);
        v = shelfHi (shB_, v, k_[2]);
        if (s_[0] == 1) v = shelfHi (shC_, v, 8.f);
        return v * k_[0] * 0.45f;
    }

    float hgFiveBand (float x)
    {
        const float g = 20.f + k_[4] * 200.f;
        float v = hardClip (x * g, 0.3f) / 0.3f;
        v = bandA_.process (v);
        v = bandB_.process (v);
        v = bandC_.process (v);
        return v * k_[0] * 0.4f;
    }

    // Due stadi di clipping in cascata e la rete di tono a conca: il comando
    // mescola un ramo scuro e uno chiaro, e a meta' corsa la conca e' massima.
    float fuzzMuff (float x)
    {
        const float g = 8.f + k_[2] * 90.f;
        float v = softClip (x * g, 0.85f);
        v = muffA_.process (v);
        v = softClip (v * 6.f, 0.85f);
        const float dark   = muffB_.process (v);
        const float bright = muffC_.process (v);
        v = dark * (1.f - k_[1]) + bright * k_[1];
        return v * k_[0] * 0.6f;
    }

    // Fuzz tenuto a bada: ingresso filtrato, asimmetria contenuta e uscita
    // limitata, cosi' le note restano leggibili anche in accordo.
    float fuzzControlled (float x)
    {
        const float g = 10.f + k_[2] * 70.f;
        float v = loopHP_.process (x) * g;
        v = softClip (v, 0.55f);
        v = hardClip (v, 0.8f);
        v = tilt (tiltA_, v, (k_[1] - 0.5f) * 12.f);
        return v * k_[0] * 0.5f;
    }

    // Uno stadio a transistor: fino a una certa spinta e' pulito, oltre
    // comincia a schiacciare da solo, e non ha un comando per impedirlo.
    float boostTransistor (float x)
    {
        const float g = std::pow (10.f, k_[0] / 20.f);
        return std::tanh (boostHP_.process (x) * g * 0.8f) * 1.2f;
    }

    // --- gate della seconda serie ----------------------------------------
    float gateReduction (float x)
    {
        const float a  = std::exp (-1.0f / (float) (sr_ * std::max (1.f, k_[1]) * 0.001));
        const float ga = std::exp (-1.0f / (float) (sr_ * 0.005));
        const float lvl = std::fabs (x);
        env_ = (lvl > env_) ? lvl : (env_ * a + lvl * (1.f - a));

        const float over = 20.f * std::log10 (std::max (env_, 1.0e-7f)) - k_[0];
        const float floorG = std::pow (10.f, -k_[2] / 20.f);
        float target;
        if (over >= 0.f)      target = 1.f;
        else if (over > -6.f) target = floorG + (1.f - floorG) * (1.f + over / 6.f);
        else                  target = floorG;

        gain_ = gain_ * ga + target * (1.f - ga);
        return x * gain_;
    }

    // Sotto la soglia abbassa in proporzione invece di chiudere: la coda della
    // nota scende insieme al fondo invece di venire tagliata.
    float gateExpander (float x)
    {
        const float a  = std::exp (-1.0f / (float) (sr_ * std::max (1.f, k_[2]) * 0.001));
        const float ga = std::exp (-1.0f / (float) (sr_ * 0.005));
        const float lvl = std::fabs (x);
        env_ = (lvl > env_) ? lvl : (env_ * a + lvl * (1.f - a));

        const float envDB = 20.f * std::log10 (std::max (env_, 1.0e-7f));
        float gdB = 0.f;
        if (envDB < k_[0]) gdB = (envDB - k_[0]) * (std::max (1.01f, k_[1]) - 1.f);
        const float target = std::pow (10.f, std::max (-60.f, gdB) / 20.f);

        gain_ = gain_ * ga + target * (1.f - ga);
        return x * gain_;
    }

    float eqGraphic6 (float x)
    {
        float v = x;
        for (int i = 0; i < 6; ++i) v = band_[i].process (v);
        return v * std::pow (10.f, k_[6] / 20.f);
    }

    float eqKnockout (float x)
    {
        const float v = tilt (tiltA_, x, k_[0]);
        return v * std::pow (10.f, k_[1] / 20.f);
    }

    // Cella ottica: il rilascio ha due tempi, uno svelto per i transitori e uno
    // lungo per il livello medio. E' quello che la fa "respirare".
    float compOpto (float x)
    {
        const float sustain = std::min (std::max (k_[0], 0.f), 1.f);
        const float thDB  = -8.f - sustain * 28.f;
        const float ratio = 2.f + sustain * 6.f;

        const float lvl = std::fabs (x);
        cEnv_ = (lvl > cEnv_) ? (cEnv_ * cDetA_ + lvl * (1.f - cDetA_))
                              : (cEnv_ * cDetR_ + lvl * (1.f - cDetR_));
        const float envDB = 20.f * std::log10 (std::max (cEnv_, 1.0e-6f));
        float gdB = 0.f;
        if (envDB > thDB) gdB = (thDB - envDB) * (1.f - 1.f / ratio);
        const float target = std::pow (10.f, gdB / 20.f);

        // Due costanti in cascata: la prima insegue, la seconda trattiene.
        static const float kAtk[3] = { 0.00035f, 0.0012f, 0.0055f };   // lento, medio, svelto
        const float atk = kAtk[std::min (std::max (s_[0], 0), 2)];
        optFast_ += (target   - optFast_) * (target < optFast_ ? atk : 0.0016f);
        optSlow_ += (optFast_ - optSlow_) * 0.00012f;
        const float g = std::min (optFast_, optSlow_);
        grDb_ = 20.f * std::log10 (std::max (g, 1.0e-6f));
        return x * g * std::pow (10.f, k_[1] / 20.f);
    }

    // Uno stadio a FET tarato per rifare il canale di un ampli, non un pedale:
    // la compressione arriva prima del clipping e resta anche a guadagno basso,
    // ed e' quello che lo fa rispondere alla mano.
    float odFetBox (float x)
    {
        const float g = 5.f + k_[0] * 70.f;
        float v = loopHP_.process (x) * g;
        v = std::tanh (v * 0.8f);                 // compressione dello stadio
        v = softClip (v * 2.2f, 0.9f);
        v = bandA_.process (v);                   // medi avanti
        v = tilt (tiltA_, v, (k_[1] - 0.5f) * 12.f);
        return v * k_[2] * 0.55f;
    }

    // Clipping morbido col cedimento di uno stadio finale: l'alimentazione
    // scende sotto la pennata forte e risale piano, e l'attacco si schiaccia.
    float odTubeLike (float x)
    {
        const float sag = k_[3];
        const float lvl = std::fabs (x);
        const float target = 1.f - std::min (0.5f, lvl * sag * 2.2f);
        const float a = (target < sagEnv_) ? 0.9992f : 0.99992f;
        sagEnv_ = sagEnv_ * a + target * (1.f - a);

        const float g = 4.f + k_[0] * 60.f;
        float v = std::tanh (loopHP_.process (x) * g * sagEnv_ * 0.7f);
        v = tilt (tiltA_, v, (k_[1] - 0.5f) * 12.f);
        return v * k_[2] * 0.7f;
    }

    // Due voci in un pedale: crunch resta aperto, lead chiude il fondo prima
    // del guadagno e aggiunge due stadi, che e' il modo di stringere senza
    // impastare.
    float distTwoChan (float x)
    {
        const bool lead = (s_[0] == 1);
        float v = lead ? muffA_.process (x) : x;
        // Il crunch deve restare aperto: col guadagno alto come il lead l'onda
        // sarebbe gia' piatta in entrambi e le due voci suonerebbero uguali.
        const float g = (lead ? 25.f : 1.2f) + k_[0] * (lead ? 170.f : 14.f);
        v = softClip (v * g, 0.85f);
        if (lead) { v = softClip (v * 2.6f, 0.8f); v = softClip (v * 1.8f, 0.8f); }
        v = shelfLo (shA_, v, k_[1]);
        v = shelfHi (shB_, v, k_[2]);
        return v * k_[3] * 0.5f;
    }

    // Cascata alla britannica: il condensatore di brillantezza porta gli acuti
    // attorno al primo stadio, quindi a guadagno basso il pedale e' aperto e
    // mordente, e chiudendo il volume della chitarra si pulisce da solo.
    float distBritish (float x)
    {
        const float bright = boostHP_.process (x) * k_[3] * 1.4f;
        const float g = 6.f + k_[0] * 90.f;
        float v = softClip ((x + bright) * g, 0.9f);
        v = softClip (v * 2.2f, 0.85f);
        v = tilt (tiltA_, v, (k_[1] - 0.5f) * 14.f);
        return v * k_[2] * 0.5f;
    }

    // Ibrido FET piu' diodi, in classe A. Il FET spinge sempre e colora con la
    // seconda armonica, perche' la polarizzazione e' asimmetrica; i diodi hanno
    // una soglia e restano fuori finche' il livello non ci arriva. E' per
    // questo che a comando basso spinge pulito e alzandolo diventa crunch,
    // invece di distorcere in modo uniforme come farebbe un solo stadio.
    float boostFetDiode (float x)
    {
        const float g = 1.5f + k_[0] * 22.f;
        float v = x * g;
        // L'asimmetria della classe A non viene da una componente continua
        // aggiunta: quella sparisce appena il segnale cresce, e infatti con la
        // prima stesura la seconda armonica si perdeva alzando il comando.
        // Viene dalla curva stessa, che tosa prima da un lato: cosi' resta
        // asimmetrica a ogni livello.
        v = softClip (v * 0.7f, 0.f);                            // FET in classe A
        const float th = 0.62f;                                  // soglia dei diodi
        if (std::fabs (v) > th)
            v = (v > 0.f ? th : -th) + std::tanh ((v - (v > 0.f ? th : -th)) * 2.2f) * 0.22f;
        v = tilt (tiltA_, v, (k_[1] - 0.5f) * 12.f);
        return v * std::pow (10.f, k_[2] / 20.f) * 0.5f;
    }

    // --- compressori ------------------------------------------------------
    // Inseguitore di picco con attacco regolabile e rilascio fisso per i due
    // sustainer, esplicito per il limitatore. Il rapporto dei sustainer cresce
    // col sustain: e' quello che allunga la coda invece di limitare soltanto.
    // Rilevatore di picco veloce (attacco 1 ms) separato dal livellatore del
    // guadagno, che invece segue l'attacco impostato: e' quello che lascia
    // passare il transiente della pennata prima di stringere. Ginocchio morbido
    // di 6 dB, come nei sustainer a OTA.
    float compressor (float x, const Model& m)
    {
        const bool limiter = (m.topo == Topology::CompLimiter);

        float thDB, ratio, outDB;
        if (limiter) {
            thDB = k_[0]; ratio = std::max (1.01f, k_[1]); outDB = k_[3];
        } else {
            const float sustain = std::min (std::max (k_[0], 0.f), 1.f);
            thDB  = -6.f - sustain * 30.f;          // piu' sustain, soglia piu' bassa
            ratio =  2.f + sustain * 6.f;
            outDB = (m.topo == Topology::CompSustain) ? k_[3] : k_[2];
        }

        const float a = std::fabs (x);
        const float d = (a > cEnv_) ? cDetA_ : cDetR_;
        cEnv_ = d * cEnv_ + (1.f - d) * a;

        const float envDB = 20.f * std::log10 (std::max (cEnv_, 1.0e-6f));

        const float knee = 6.f;
        const float over = envDB - thDB;
        float targetGrDB = 0.f;
        if (over >= knee * 0.5f) {
            targetGrDB = (thDB + over / ratio) - envDB;
        } else if (over > -knee * 0.5f) {
            const float t = (over + knee * 0.5f) / knee;   // 0..1 dentro il ginocchio
            targetGrDB = ((thDB + over / ratio) - envDB) * t * t;
        }

        const float targetGain = std::pow (10.f, targetGrDB / 20.f);
        const float c = (targetGain < cGain_) ? cAtk_ : cRel_;
        cGain_ = c * cGain_ + (1.f - c) * targetGain;
        grDb_  = 20.f * std::log10 (std::max (cGain_, 1.0e-6f));   // <= 0

        float v = x * cGain_;

        // Il tono del sustainer e' una bilancia: quanto alza gli acuti tanto
        // toglie ai bassi, cosi' il livello percepito non cambia.
        if (m.topo == Topology::CompSustain) {
            const float hi = toneHP_.process (v);
            const float lo = v - hi;
            const float g  = std::pow (10.f, k_[2] / 40.f);
            v = lo / g + hi * g;
        }
        return v * std::pow (10.f, outDB / 20.f);
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
        if (m.topo == Topology::EqGraphic6) {
            static const float kF[6] = { 63.f, 125.f, 250.f, 500.f, 1000.f, 2000.f };
            for (int i = 0; i < 6; ++i) band_[i].set (kF[i], 1.4f, k_[i], sr_);
            return;
        }
        if (m.topo == Topology::DistThick) {
            bandA_.set (800.f, 0.9f, 5.f, sr_);          // la gobba sui medi, fissa
            return;
        }
        if (m.topo == Topology::OdFetBox) {
            bandA_.set (900.f, 0.8f, 4.f, sr_);
            return;
        }
        if (m.topo == Topology::DistFixedHi) {
            bandB_.set (700.f, 1.0f, k_[2], sr_);
            return;
        }
        if (m.topo == Topology::HgFiveBand) {
            bandA_.set (100.f,  0.8f, k_[1], sr_);
            bandB_.set (700.f,  1.0f, k_[2], sr_);
            bandC_.set (3200.f, 0.8f, k_[3], sr_);
            return;
        }
        if (m.topo == Topology::EqKnockout) {
            // Bright sposta il perno della bilancia piu' in alto.
            tiltA_.setCutoff (s_[0] == 1 ? 2200.f : 900.f, sr_, true);
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
    // Seconda serie: ogni banco di filtri vuole la sua istanza, perche' un
    // OnePole porta dentro lo stato del campione precedente.
    OnePole shA_, shB_, shC_, tiltA_, ratPre_, ratPost_;
    OnePole muffA_, muffB_, muffC_, smoothLP_;
    Peak    low_, mid_, high_;
    Peak    band_[7];               // bande dell'equalizzatore grafico
    Peak    bandA_, bandB_, bandC_; // tre bande dei modelli a equalizzazione piena
    float   optFast_ = 1.f, optSlow_ = 1.f;   // le due costanti della cella ottica
    float   dc_   = 0.f;
    float   env_  = 0.f;            // inviluppo del gate
    float   cEnv_ = 0.f;            // inviluppo del compressore
    float   cGain_ = 1.f;           // guadagno del compressore, livellato
    float   grDb_  = 0.f;           // riduzione in corso, per il misuratore
    float   cAtk_ = 0.f, cRel_ = 0.f, cDetA_ = 0.f, cDetR_ = 0.f;
    float   slew_ = 0.f;            // uscita dell'op-amp lento
    float   sagEnv_ = 1.f;          // alimentazione che cede
    float   gain_ = 1.f;            // guadagno del gate, con rampa
};

} // namespace pedal
