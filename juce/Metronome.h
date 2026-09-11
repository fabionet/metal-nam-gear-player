// Metronome.h — click a campanaccio, mescolato in fondo alla catena.
//
// Il timbro e' quello del campanaccio da batteria: due parziali inarmoniche
// che decadono insieme, quindi un suono metallico e corto invece del solito
// bip sinusoidale. Il primo movimento della battuta e' piu' acuto e piu'
// lungo ("tin"), gli altri piu' spenti ("tap").
#pragma once

#include <atomic>
#include <cmath>

namespace nam_dsp {

class Metronome
{
public:
    void prepare (double sampleRate)
    {
        sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
        reset();
    }

    void reset()
    {
        phaseSamples_ = 0.0;
        beatIndex_    = 0;
        env_          = 0.0f;
        p1_ = p2_     = 0.0f;
        beatFlag_.store (false, std::memory_order_relaxed);
    }

    // Chiamata a fine catena. `beatsPerBar` a 0 disattiva l'accento.
    void process (float* left, float* right, int numSamples,
                  double bpm, int beatsPerBar, float gainLin)
    {
        if (numSamples <= 0) return;
        const double spb = sr_ * 60.0 / (bpm > 1.0 ? bpm : 120.0);   // campioni per movimento

        for (int i = 0; i < numSamples; ++i)
        {
            if (phaseSamples_ <= 0.0)
            {
                phaseSamples_ += spb;
                const bool accent = (beatsPerBar > 0) && (beatIndex_ % beatsPerBar == 0);
                beatIndex_ = (beatsPerBar > 0) ? (beatIndex_ + 1) % beatsPerBar : beatIndex_ + 1;

                // Campanaccio: rapporto inarmonico fra le due parziali. Sul
                // primo movimento salgono di una quinta scarsa e il decadimento
                // e' piu' lungo, cosi' si distingue a orecchio.
                f1_ = accent ? 835.0f : 587.0f;
                f2_ = accent ? 1235.0f : 845.0f;
                decay_ = std::exp (-1.0f / (float) (sr_ * (accent ? 0.085 : 0.045)));
                env_ = accent ? 1.0f : 0.62f;
                p1_ = p2_ = 0.0f;
                beatFlag_.store (true, std::memory_order_release);
            }
            phaseSamples_ -= 1.0;

            if (env_ > 1.0e-4f)
            {
                const float tp = 6.283185307179586f;
                p1_ += f1_ / (float) sr_; if (p1_ >= 1.f) p1_ -= 1.f;
                p2_ += f2_ / (float) sr_; if (p2_ >= 1.f) p2_ -= 1.f;
                // Le due parziali a pesi diversi, piu' una punta di terza
                // armonica sulla prima per l'attacco metallico.
                float s = 0.60f * std::sin (tp * p1_)
                        + 0.40f * std::sin (tp * p2_)
                        + 0.12f * std::sin (tp * 2.0f * p1_);
                s *= env_ * gainLin;
                env_ *= decay_;
                left[i] += s;
                if (right != nullptr && right != left) right[i] += s;
            }
        }
    }

    // Consumata dall'interfaccia per far lampeggiare il TAP a tempo.
    bool consumeBeatFlag()
    {
        return beatFlag_.exchange (false, std::memory_order_acq_rel);
    }

private:
    double sr_            = 48000.0;
    double phaseSamples_  = 0.0;
    int    beatIndex_     = 0;
    float  env_           = 0.0f;
    float  decay_         = 0.999f;
    float  f1_ = 587.0f, f2_ = 845.0f;
    float  p1_ = 0.0f,   p2_ = 0.0f;
    std::atomic<bool> beatFlag_ { false };
};

} // namespace nam_dsp
