// NativeAmp.h — original 3-channel tube-amp emulation (framework-independent).
//
// Layout modelled on a classic 3-channel head (Clean / Crunch / Lead) but the
// DSP is entirely original: cascaded asymmetric tanh triode stages + an RBJ
// tone stack. Header-only, mono, RT-safe in process(). No JUCE includes.
//
// Signal per call: input → global Thump (low-shelf tightness) → per-channel
// [gain stages → tone stack] → per-channel Master → global Presence (high-shelf).
//
// Channels:
//   0 Clean  — 1 gentle stage, full Bass/Mid/Treble tone stack, Master1
//   1 Crunch — 2 stages, fixed internal voicing (mid scoop), Gain2/Master2 only
//   2 Lead   — 3 hot stages, full Bass/Mid/Treble tone stack, Master3
#pragma once

#include <atomic>
#include <cmath>

#include "Biquad.h"   // nam_dsp::Biquad (src/dsp/Biquad.h — on the include path)

namespace preamp_fx {

class NativeAmp {
public:
    NativeAmp() = default;

    void prepare (double sr)
    {
        sr_ = (sr > 0.0 ? sr : 48000.0);

        // Fixed Crunch voicing: gentle mid scoop for a modern rhythm tone.
        ch2Voice_.setPeak (sr_, 520.0, 0.8, -3.5);

        // Invalidate caches so the next apply* recomputes every biquad, then
        // recompute now from the currently-stored targets.
        thumpCached_ = presCached_ = 1e9f;
        bass1C_ = mid1C_ = treb1C_ = 1e9f;
        bass3C_ = mid3C_ = treb3C_ = 1e9f;
        recomputeAll();
        reset();
    }

    void reset()
    {
        thump_.reset();   presence_.reset();
        bass1_.reset();   mid1_.reset();   treble1_.reset();
        bass3_.reset();   mid3_.reset();   treble3_.reset();
        ch2Voice_.reset();
    }

    // --- Parameter setters (called each block from the processor) ------------
    void setEnabled (bool on)  { enabled_.store (on); }
    void setChannel (int ch)   { channel_.store (ch < 0 ? 0 : (ch > 2 ? 2 : ch)); }

    void setGlobal (float thumpDB, float presenceDB)
    {
        if (thumpDB != thumpCached_) { thump_.setLowShelf  (sr_, 120.0, 0.7, thumpDB);    thumpCached_ = thumpDB; }
        if (presenceDB != presCached_) { presence_.setHighShelf (sr_, 3000.0, 0.7, presenceDB); presCached_ = presenceDB; }
    }

    void setChannel1 (float bassDB, float midDB, float trebleDB, float gain01, float masterDB)
    {
        gain1_ = clamp01 (gain01);
        master1Lin_ = db2lin (masterDB);
        if (bassDB   != bass1C_) { bass1_.setLowShelf  (sr_, 120.0, 0.7, bassDB);   bass1C_ = bassDB; }
        if (midDB    != mid1C_)  { mid1_.setPeak       (sr_, 650.0, 0.7, midDB);    mid1C_  = midDB; }
        if (trebleDB != treb1C_) { treble1_.setHighShelf (sr_, 3000.0, 0.7, trebleDB); treb1C_ = trebleDB; }
    }

    void setChannel2 (float gain01, float masterDB)
    {
        gain2_ = clamp01 (gain01);
        master2Lin_ = db2lin (masterDB);
    }

    void setChannel3 (float bassDB, float midDB, float trebleDB, float gain01, float masterDB)
    {
        gain3_ = clamp01 (gain01);
        master3Lin_ = db2lin (masterDB);
        if (bassDB   != bass3C_) { bass3_.setLowShelf  (sr_, 110.0, 0.7, bassDB);   bass3C_ = bassDB; }
        if (midDB    != mid3C_)  { mid3_.setPeak       (sr_, 700.0, 0.7, midDB);    mid3C_  = midDB; }
        if (trebleDB != treb3C_) { treble3_.setHighShelf (sr_, 3200.0, 0.7, trebleDB); treb3C_ = trebleDB; }
    }

    // RT-safe. Returns x unchanged when disabled.
    float process (float x)
    {
        if (! enabled_.load()) return x;

        float s = thump_.process (x);
        const int ch = channel_.load();

        if (ch <= 0) {
            // Clean: single gentle triode stage.
            s = tubeStage (s, 1.f + gain1_ * 7.f, 0.f);
            s = bass1_.process (s);
            s = mid1_.process (s);
            s = treble1_.process (s);
            s *= master1Lin_ * kMakeupClean;
        }
        else if (ch == 1) {
            // Crunch: two medium stages + fixed voicing.
            s = tubeStage (s, 2.f + gain2_ * 23.f, 0.02f);
            s = tubeStage (s, 1.6f, 0.02f);
            s = ch2Voice_.process (s);
            s *= master2Lin_ * kMakeupCrunch;
        }
        else {
            // Lead: three hot cascaded stages.
            s = tubeStage (s, 4.f + gain3_ * 56.f, 0.05f);
            s = tubeStage (s, 2.2f, 0.04f);
            s = tubeStage (s, 1.5f, 0.03f);
            s = bass3_.process (s);
            s = mid3_.process (s);
            s = treble3_.process (s);
            s *= master3Lin_ * kMakeupLead;
        }

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
        thump_.setLowShelf   (sr_, 120.0, 0.7, thumpCached_ >= 1e8f ? 0.f : thumpCached_);
        presence_.setHighShelf (sr_, 3000.0, 0.7, presCached_ >= 1e8f ? 0.f : presCached_);
        bass1_.setLowShelf   (sr_, 120.0, 0.7, bass1C_ >= 1e8f ? 0.f : bass1C_);
        mid1_.setPeak        (sr_, 650.0, 0.7, mid1C_  >= 1e8f ? 0.f : mid1C_);
        treble1_.setHighShelf (sr_, 3000.0, 0.7, treb1C_ >= 1e8f ? 0.f : treb1C_);
        bass3_.setLowShelf   (sr_, 110.0, 0.7, bass3C_ >= 1e8f ? 0.f : bass3C_);
        mid3_.setPeak        (sr_, 700.0, 0.7, mid3C_  >= 1e8f ? 0.f : mid3C_);
        treble3_.setHighShelf (sr_, 3200.0, 0.7, treb3C_ >= 1e8f ? 0.f : treb3C_);
        // Reset caches to real (zeroed) values so setters detect future changes.
        if (thumpCached_ >= 1e8f) thumpCached_ = 0.f;
        if (presCached_  >= 1e8f) presCached_  = 0.f;
        if (bass1C_ >= 1e8f) bass1C_ = 0.f;
        if (mid1C_  >= 1e8f) mid1C_  = 0.f;
        if (treb1C_ >= 1e8f) treb1C_ = 0.f;
        if (bass3C_ >= 1e8f) bass3C_ = 0.f;
        if (mid3C_  >= 1e8f) mid3C_  = 0.f;
        if (treb3C_ >= 1e8f) treb3C_ = 0.f;
    }

    // Level makeup so nominal masters land near unity (tanh cascades lose level).
    static constexpr float kMakeupClean  = 2.0f;
    static constexpr float kMakeupCrunch = 1.4f;
    static constexpr float kMakeupLead   = 1.2f;

    double sr_ = 48000.0;

    std::atomic<bool> enabled_ { false };
    std::atomic<int>  channel_ { 0 };

    nam_dsp::Biquad thump_, presence_;
    nam_dsp::Biquad bass1_, mid1_, treble1_;
    nam_dsp::Biquad bass3_, mid3_, treble3_;
    nam_dsp::Biquad ch2Voice_;

    float gain1_ = 0.5f, gain2_ = 0.5f, gain3_ = 0.6f;
    float master1Lin_ = db2lin (-12.f);
    float master2Lin_ = db2lin (-12.f);
    float master3Lin_ = db2lin (-12.f);

    // Change-detection caches for biquad coefficient recompute.
    float thumpCached_ = 0.f, presCached_ = 0.f;
    float bass1C_ = 0.f, mid1C_ = 0.f, treb1C_ = 0.f;
    float bass3C_ = 0.f, mid3C_ = 0.f, treb3C_ = 0.f;
};

} // namespace preamp_fx
