// MarshallAmp.h — Marshall JCM800 2203 emulation (framework-independent).
//
// Faithful voicing of the single-channel JCM800 Model 2203 master-volume head:
// a cascaded ECC83 preamp (V1) feeding an FMV (Fender-derived) tone stack, then
// EL34/6550 power-tube voicing and a presence high-shelf emulating the negative
// feedback loop. The DSP is entirely original: cascaded asymmetric tanh triode
// stages + RBJ biquad tone shaping. Header-only, mono, RT-safe in process().
// No JUCE includes. Same design idiom as NativeAmp.h (GEAR SX).
//
// Signal per call: input → preamp V1 (1 or 2 stages by sensitivity) → FMV tone
// stack (Bass/Middle/Treble) → power-tube voicing + saturation (EU EL34 vs US
// 6550) → Master → Presence (NFB high-shelf).
//
// The 2203 used a solid-state rectifier (no sag), so no rectifier modelling is
// present by design. The FX loop (Send/Return) is intentionally NOT modelled.
#pragma once

#include <atomic>
#include <cmath>

#include "Biquad.h"   // nam_dsp::Biquad (src/dsp/Biquad.h — on the include path)

namespace preamp_fx {

class MarshallAmp {
public:
    MarshallAmp() = default;

    void prepare (double sr)
    {
        sr_ = (sr > 0.0 ? sr : 48000.0);

        // Invalidate coefficient caches so the next set* recomputes every biquad,
        // then recompute now from the currently-stored targets.
        bassCached_ = midCached_ = trebCached_ = 1e9f;
        presCached_ = 1e9f;
        valvesCached_ = -1;
        recomputeAll();
        reset();
    }

    void reset()
    {
        bass_.reset();  mid_.reset();  treble_.reset();
        voice_.reset(); presence_.reset();
    }

    // --- Parameter setters (called each block from the processor) ------------
    void setEnabled (bool on)  { enabled_.store (on); }

    // 0 = EU (4x EL34, mid-forward British crunch)
    // 1 = US (4x 6550, more clean headroom, firmer/tighter bass)
    void setValves (int v)
    {
        const int vv = (v <= 0 ? 0 : 1);
        valves_.store (vv);
        if (vv != valvesCached_) {
            if (vv == 0) voice_.setPeak (sr_, 600.0, 0.9, +2.0);  // EL34 mid push
            else         voice_.setPeak (sr_, 250.0, 0.9, -2.0);  // 6550 tight lows
            valvesCached_ = vv;
        }
    }

    // 0 = Low sensitivity (single preamp triode, ~-6 dB, more headroom)
    // 1 = High sensitivity (both V1 triodes cascaded, hard-rock/metal saturation)
    void setSens (int s) { sens_.store (s <= 0 ? 0 : 1); }

    void setControls (float preamp01, float masterDB,
                      float bassDB, float midDB, float trebleDB, float presenceDB)
    {
        preamp_    = clamp01 (preamp01);
        masterLin_ = db2lin (masterDB);
        if (bassDB     != bassCached_) { bass_.setLowShelf   (sr_, 100.0,  0.7, bassDB);     bassCached_ = bassDB; }
        if (midDB      != midCached_)  { mid_.setPeak        (sr_, 650.0,  0.9, midDB);      midCached_  = midDB;  }
        if (trebleDB   != trebCached_) { treble_.setHighShelf (sr_, 2600.0, 0.7, trebleDB);  trebCached_ = trebleDB; }
        if (presenceDB != presCached_) { presence_.setHighShelf (sr_, 3500.0, 0.7, presenceDB); presCached_ = presenceDB; }
    }

    // RT-safe. Returns x unchanged when disabled.
    float process (float x)
    {
        if (! enabled_.load()) return x;

        // 1) Preamp V1 cascade. First triode always; second only in High sens.
        float s = tubeStage (x, 3.f + preamp_ * 57.f, 0.03f);
        if (sens_.load() == 1)
            s = tubeStage (s, 2.0f, 0.03f);

        // 2) FMV tone stack (Bass / Middle / Treble).
        s = bass_.process (s);
        s = mid_.process (s);
        s = treble_.process (s);

        // 3) Power-tube voicing + saturation (depends on selected valves).
        s = voice_.process (s);
        if (valves_.load() == 0) {
            // EL34: earlier breakup, softer top.
            s = tubeStage (s, 1.4f, 0.02f);
            s *= kMakeupEL34;
        } else {
            // 6550: more headroom, cleaner/symmetric.
            s = tubeStage (s, 1.1f, 0.0f);
            s *= kMakeup6550;
        }

        // 4) Master.
        s *= masterLin_;

        // 5) Presence (NFB high-shelf).
        s = presence_.process (s);
        return s;
    }

private:
    static inline float clamp01 (float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
    static inline float db2lin  (float dB) { return std::pow (10.f, dB * 0.05f); }

    // Asymmetric triode-style soft clip. The DC bias adds even harmonics; it is
    // subtracted back out so the stage stays DC-free.
    static inline float tubeStage (float x, float pre, float bias)
    {
        return std::tanh (x * pre + bias) - std::tanh (bias);
    }

    void recomputeAll()
    {
        bass_.setLowShelf    (sr_, 100.0,  0.7, bassCached_ >= 1e8f ? 0.f : bassCached_);
        mid_.setPeak         (sr_, 650.0,  0.9, midCached_  >= 1e8f ? 0.f : midCached_);
        treble_.setHighShelf (sr_, 2600.0, 0.7, trebCached_ >= 1e8f ? 0.f : trebCached_);
        presence_.setHighShelf (sr_, 3500.0, 0.7, presCached_ >= 1e8f ? 0.f : presCached_);
        // EL34 voicing as the neutral default; setValves recomputes when it differs.
        voice_.setPeak (sr_, 600.0, 0.9, +2.0);
        // Reset caches to real (zeroed) values so setters detect future changes.
        if (bassCached_ >= 1e8f) bassCached_ = 0.f;
        if (midCached_  >= 1e8f) midCached_  = 0.f;
        if (trebCached_ >= 1e8f) trebCached_ = 0.f;
        if (presCached_ >= 1e8f) presCached_ = 0.f;
        valvesCached_ = 0;
    }

    // Level makeup so nominal masters land near unity (tanh cascades lose level).
    static constexpr float kMakeupEL34 = 1.3f;
    static constexpr float kMakeup6550 = 1.15f;

    double sr_ = 48000.0;

    std::atomic<bool> enabled_ { false };
    std::atomic<int>  valves_  { 0 };   // 0 = EU/EL34, 1 = US/6550
    std::atomic<int>  sens_    { 1 };   // 0 = Low, 1 = High

    nam_dsp::Biquad bass_, mid_, treble_;
    nam_dsp::Biquad voice_, presence_;

    float preamp_    = 0.6f;
    float masterLin_ = db2lin (-12.f);

    // Change-detection caches for biquad coefficient recompute.
    float bassCached_ = 0.f, midCached_ = 0.f, trebCached_ = 0.f;
    float presCached_ = 0.f;
    int   valvesCached_ = 0;
};

} // namespace preamp_fx
