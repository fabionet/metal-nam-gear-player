// RectifierAmp.h — emulazione del Mesa/Boogie Dual Rectifier a due canali.
//
// Indipendente dal framework, monocanale, sicura in tempo reale nel process().
// Stessa impostazione di MarshallAmp.h (MARCHELLOW) e NativeAmp.h (GEAR SX):
// stadi a triodo con tanh asimmetrica, torre di tono a biquad RBJ, nessun
// include di JUCE.
//
// Il Dual Rectifier a due canali ha, per ciascun canale, i suoi Gain, Treble,
// Mid, Bass, Presence e Master indipendenti, piu' il modo del canale. Il
// canale uno va da pulito a saturazione variabile, il due e' l'alto guadagno
// moderno, e l'interruttore di clonazione permette a ciascuno di prendere la
// voce dell'altro. Da qui i tre modi: Clean, Vintage, Modern.
//
// Due cose distinguono questo ampli dagli altri della catena e sono modellate
// perche' si sentono:
//
// - **Dove sta la presenza.** Sul canale rosso lavora nel preamplificatore,
//   sull'arancio dentro l'anello di controreazione. Nel primo caso apre il
//   fronte della nota prima che venga distorta, nel secondo agisce dopo, sulla
//   coda: e' la stessa manopola ma non fa la stessa cosa.
// - **Il raddrizzatore.** A valvole la tensione di alimentazione cede sotto la
//   pennata e torna su dopo: e' il cedimento che rende elastico l'attacco. Coi
//   diodi al silicio non cede, e l'attacco resta duro. L'interruttore
//   Bold/Spongy abbassa la tensione di partenza, e il cedimento diventa piu'
//   marcato e piu' lento.
//
// La catena: ingresso -> preamplificatore del canale (due o tre stadi secondo
// il modo) -> torre di tono -> voce dello stadio finale col cedimento ->
// Master -> presenza (dove la vuole il canale).
#pragma once

#include <atomic>
#include <cmath>

#include "Biquad.h"   // nam_dsp::Biquad (src/dsp/Biquad.h — sul percorso di include)

namespace preamp_fx {

class RectifierAmp {
public:
    RectifierAmp() = default;

    void prepare (double sr)
    {
        sr_ = (sr > 0.0 ? sr : 48000.0);
        for (auto& c : ch_) c.invalidate();
        recomputeAll();
        // Costanti del cedimento: la tensione scende in qualche millisecondo e
        // risale in qualche decimo di secondo, come un condensatore che si
        // svuota e si ricarica.
        sagDown_ = std::exp (-1.0f / (float) (sr_ * 0.004));
        sagUp_     = std::exp (-1.0f / (float) (sr_ * 0.220));
        sagUpSlow_ = std::exp (-1.0f / (float) (sr_ * 0.480));
        reset();
    }

    void reset()
    {
        for (auto& c : ch_) { c.bass.reset(); c.mid.reset(); c.treble.reset();
                              c.voice.reset(); c.presence.reset(); }
        sag_ = 1.f;
    }

    // --- comandi, chiamati a ogni blocco dal processore ---------------------
    void setEnabled (bool on) { enabled_.store (on); }

    // 0 = canale uno (arancio), 1 = canale due (rosso).
    void setChannel (int c) { channel_.store (c <= 0 ? 0 : 1); }

    // 0 = Clean, 1 = Vintage, 2 = Modern. E' il selettore di modo del canale,
    // quello che sull'ampli fa anche da interruttore di clonazione.
    void setMode (int ch, int mode)
    { mode_[ch <= 0 ? 0 : 1].store (mode < 0 ? 0 : (mode > 2 ? 2 : mode)); }

    // 0 = valvole (cede), 1 = diodi al silicio (non cede).
    void setRectifier (int r) { rect_.store (r <= 0 ? 0 : 1); }

    // 0 = Bold (tensione piena), 1 = Spongy (ridotta: cede prima e di piu').
    void setPower (int p) { power_.store (p <= 0 ? 0 : 1); }

    void setChannelControls (int chIdx, float gain01, float masterDB,
                             float bassDB, float midDB, float trebleDB, float presenceDB)
    {
        auto& c = ch_[chIdx <= 0 ? 0 : 1];
        c.gain      = clamp01 (gain01);
        c.masterLin = db2lin (masterDB);
        // La torre di tono del Rectifier e' passiva e sta dopo il guadagno: i
        // punti sono piu' bassi di quelli britannici, ed e' per questo che con
        // i medi a zero il suono si svuota invece di assottigliarsi.
        if (bassDB   != c.bassC) { c.bass.setLowShelf   (sr_,  80.0,  0.7, bassDB);   c.bassC = bassDB; }
        if (midDB    != c.midC)  { c.mid.setPeak        (sr_, 500.0,  0.8, midDB);    c.midC  = midDB;  }
        if (trebleDB != c.trebC) { c.treble.setHighShelf (sr_, 2200.0, 0.7, trebleDB); c.trebC = trebleDB; }
        if (presenceDB != c.presC) {
            // Sul canale due la presenza sta nel preamplificatore e lavora piu'
            // in alto e piu' stretta; sul canale uno sta nell'anello di
            // controreazione, largo e piu' basso.
            if (chIdx >= 1) c.presence.setHighShelf (sr_, 4200.0, 0.9, presenceDB);
            else            c.presence.setHighShelf (sr_, 2800.0, 0.6, presenceDB);
            c.presC = presenceDB;
        }
    }

    float process (float x)
    {
        if (! enabled_.load()) return x;

        const int  ci   = channel_.load();
        auto&      c    = ch_[ci];
        const int  mode = mode_[ci].load();
        const bool spongy = (power_.load() == 1);

        // --- cedimento dell'alimentazione ---------------------------------
        // A valvole la tensione scende col segnale forte e risale piano. Coi
        // diodi resta ferma. Spongy parte gia' piu' bassa e cede di piu'.
        float supply = 1.f;
        if (rect_.load() == 0) {
            const float lvl = std::fabs (x);
            // Spongy parte da una tensione piu' bassa, cede fino a quasi meta' e
            // ci mette il doppio a risalire: e' quella lentezza a dare la
            // sensazione elastica sotto le dita.
            const float maxSag = spongy ? 0.55f : 0.28f;
            const float base   = spongy ? 0.90f : 1.00f;
            const float target = base - std::min (maxSag, lvl * (spongy ? 2.2f : 0.9f));
            const float up = spongy ? sagUpSlow_ : sagUp_;
            sag_ = (target < sag_) ? (sag_ * sagDown_ + target * (1.f - sagDown_))
                                   : (sag_ * up      + target * (1.f - up));
            supply = sag_;
        } else if (spongy) {
            supply = 0.88f;                 // tensione ridotta, ma senza cedere
        }

        // --- preamplificatore ---------------------------------------------
        // Clean: due stadi appena spinti. Vintage: tre stadi con la seconda
        // armonica in evidenza. Modern: tre stadi piu' duri e un passa-alto
        // fra il secondo e il terzo, che e' quello che tiene pulite le basse
        // sotto guadagno alto e fa il suono "a motosega".
        float s = x;
        if (mode == 0) {
            s = tubeStage (s, 2.0f + c.gain * 14.f, 0.02f);
            s = tubeStage (s, 1.3f, 0.01f);
        } else {
            const float drive = 3.f + c.gain * (mode == 2 ? 62.f : 44.f);
            s = tubeStage (s, drive, mode == 2 ? 0.015f : 0.05f);
            if (mode == 2) { hpz_ = hpz_ * 0.86f + s * 0.14f; s -= hpz_; }
            s = tubeStage (s, mode == 2 ? 2.4f : 1.8f, mode == 2 ? 0.01f : 0.04f);
            s = tubeStage (s, 1.5f, 0.02f);
        }

        // --- torre di tono --------------------------------------------------
        s = c.bass.process (s);
        s = c.mid.process (s);
        s = c.treble.process (s);

        // --- canale due: la presenza sta qui, prima dello stadio finale -----
        if (ci == 1) s = c.presence.process (s);

        // --- stadio finale ---------------------------------------------------
        s = c.voice.process (s);
        s = tubeStage (s * supply, 1.25f, 0.015f) * kMakeup;

        s *= c.masterLin;

        // --- canale uno: la presenza sta nell'anello, quindi dopo ------------
        if (ci == 0) s = c.presence.process (s);
        return s;
    }

private:
    static inline float clamp01 (float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
    static inline float db2lin  (float dB) { return std::pow (10.f, dB * 0.05f); }

    // Stadio a triodo: tanh asimmetrica, con la componente continua della
    // polarizzazione sottratta perche' lo stadio non introduca offset.
    static inline float tubeStage (float x, float pre, float bias)
    {
        return std::tanh (x * pre + bias) - std::tanh (bias);
    }

    struct Chan
    {
        nam_dsp::Biquad bass, mid, treble, voice, presence;
        float gain = 0.5f;
        float masterLin = db2lin (-12.f);
        float bassC = 0.f, midC = 0.f, trebC = 0.f, presC = 0.f;
        void invalidate() { bassC = midC = trebC = presC = 1e9f; }
    };

    void recomputeAll()
    {
        for (int i = 0; i < 2; ++i) {
            auto& c = ch_[i];
            c.bass.setLowShelf    (sr_,  80.0,  0.7, c.bassC >= 1e8f ? 0.f : c.bassC);
            c.mid.setPeak         (sr_, 500.0,  0.8, c.midC  >= 1e8f ? 0.f : c.midC);
            c.treble.setHighShelf (sr_, 2200.0, 0.7, c.trebC >= 1e8f ? 0.f : c.trebC);
            c.presence.setHighShelf (sr_, i >= 1 ? 4200.0 : 2800.0, i >= 1 ? 0.9 : 0.6,
                                     c.presC >= 1e8f ? 0.f : c.presC);
            // La voce dello stadio finale: la conca sui medi alti che e' la
            // firma di questo ampli, e che nessuna manopola toglie del tutto.
            c.voice.setPeak (sr_, 1100.0, 1.1, -3.5);
            if (c.bassC >= 1e8f) c.bassC = 0.f;
            if (c.midC  >= 1e8f) c.midC  = 0.f;
            if (c.trebC >= 1e8f) c.trebC = 0.f;
            if (c.presC >= 1e8f) c.presC = 0.f;
        }
    }

    static constexpr float kMakeup = 1.25f;

    double sr_ = 48000.0;

    std::atomic<bool> enabled_ { false };
    std::atomic<int>  channel_ { 0 };
    std::atomic<int>  mode_[2] { {1}, {2} };   // uno parte Vintage, due Modern
    std::atomic<int>  rect_    { 0 };          // 0 valvole, 1 diodi
    std::atomic<int>  power_   { 0 };          // 0 Bold, 1 Spongy

    Chan  ch_[2];
    float sag_ = 1.f, sagDown_ = 0.f, sagUp_ = 0.f, sagUpSlow_ = 0.f;
    float hpz_ = 0.f;
};

} // namespace preamp_fx
