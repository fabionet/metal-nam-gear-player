// Stage 5 — pre-amp FX: smart noise gate, overdrive, distortion.
// Header-only, single-channel, RT-safe.
#pragma once

#include <atomic>
#include <cmath>
#include <algorithm>

namespace preamp_fx {

inline float db2lin (float dB) { return std::pow (10.0f, dB * 0.05f); }

// --- Smart Noise Gate ---------------------------------------------------------
// Envelope follower (peak) with hysteresis open/close + smoothed gain ramp.
class SmartGate {
public:
    void prepare (double sampleRate)
    {
        sr_ = sampleRate;
        // Attack ~1 ms, release coefficient computed from releaseMs_.
        attCoef_ = std::exp (-1.0f / (0.001f * (float) sr_));
        updateRelease();
        env_ = 0.f; gain_ = 1.f;
    }
    void reset() { env_ = 0.f; gain_ = 1.f; }

    void setThresholdDB (float dB)  { thresholdDB_ = dB; }
    void setReleaseMs   (float ms)  { releaseMs_ = std::max (5.f, ms); updateRelease(); }
    void setBypass      (bool b)    { bypass_ = b; }

    float process (float x)
    {
        if (bypass_) return x;
        const float a = std::fabs (x);
        // Peak envelope follower (faster attack, slower release).
        const float coef = (a > env_) ? attCoef_ : relCoef_;
        env_ = coef * env_ + (1.f - coef) * a;

        const float openLin  = db2lin (thresholdDB_ + 3.f);
        const float closeLin = db2lin (thresholdDB_ - 3.f);
        float target = gain_;
        if (env_ > openLin)  target = 1.f;
        else if (env_ < closeLin) target = 0.f;
        // Smooth gate gain.
        gain_ = 0.995f * gain_ + 0.005f * target;
        return x * gain_;
    }

private:
    void updateRelease()
    {
        relCoef_ = std::exp (-1.0f / (0.001f * releaseMs_ * (float) sr_));
    }
    double sr_ = 48000.0;
    float  thresholdDB_ = -60.f;
    float  releaseMs_   = 80.f;
    bool   bypass_      = false;
    float  attCoef_ = 0.f, relCoef_ = 0.f;
    float  env_ = 0.f, gain_ = 1.f;
};

// --- One-pole tilt (boost highs / cut highs) ---------------------------------
class TiltOnePole {
public:
    void prepare (double sampleRate) { sr_ = sampleRate; z_ = 0.f; updateCoef(); }
    void reset() { z_ = 0.f; }
    void setToneDB (float dB) { toneDB_ = dB; updateCoef(); }

    float process (float x)
    {
        // Simple shelving via one-pole low-pass blend.
        const float lp = a_ * x + (1.f - a_) * z_;
        z_ = lp;
        const float hp = x - lp;
        return lp * lowGain_ + hp * highGain_;
    }
private:
    void updateCoef()
    {
        const float fc = 800.f;
        a_ = 1.f - std::exp (-2.0f * (float) M_PI * fc / (float) sr_);
        const float lin = db2lin (toneDB_);
        // Boost highs (positive tone) reduces lows, vice versa.
        highGain_ = lin;
        lowGain_  = 1.f / std::max (lin, 1e-3f);
    }
    double sr_ = 48000.0;
    float  toneDB_ = 0.f;
    float  a_ = 0.f, z_ = 0.f;
    float  lowGain_ = 1.f, highGain_ = 1.f;
};

// --- Overdrive: tanh soft clip + tone tilt -----------------------------------
class Overdrive {
public:
    void prepare (double sr) { tilt_.prepare (sr); }
    void reset() { tilt_.reset(); }

    void setDrive  (float d)   { drive_ = std::clamp (d, 0.f, 1.f); }
    void setToneDB (float dB)  { tilt_.setToneDB (dB); }
    void setLevelDB(float dB)  { levelLin_ = db2lin (dB); }
    void setBypass (bool b)    { bypass_ = b; }

    float process (float x)
    {
        if (bypass_) return x;
        // Map drive 0..1 → preGain 1..30 (log).
        const float pre = 1.f + drive_.load() * 29.f;
        float y = std::tanh (x * pre) * (1.f / std::tanh (pre)); // normalized
        y = tilt_.process (y);
        return y * levelLin_;
    }
private:
    std::atomic<float> drive_ { 0.3f };
    float levelLin_ = 1.f;
    bool  bypass_   = false;
    TiltOnePole tilt_;
};

// --- Distortion: asymmetric hard clip with 2nd-harmonic offset ---------------
class Distortion {
public:
    void prepare (double sr) { tilt_.prepare (sr); }
    void reset() { tilt_.reset(); }

    void setDrive  (float d)   { drive_ = std::clamp (d, 0.f, 1.f); }
    void setToneDB (float dB)  { tilt_.setToneDB (dB); }
    void setLevelDB(float dB)  { levelLin_ = db2lin (dB); }
    void setBypass (bool b)    { bypass_ = b; }

    float process (float x)
    {
        if (bypass_) return x;
        // Map drive 0..1 → preGain 1..60 (more aggressive than OD).
        const float pre = 1.f + drive_.load() * 59.f;
        float y = x * pre + 0.07f * (drive_.load()); // small DC bias → 2nd harmonic
        // Asymmetric clipper.
        const float clipPos = 0.85f;
        const float clipNeg = -0.95f;
        if (y >  clipPos) y = clipPos + (y - clipPos) * 0.05f;
        if (y <  clipNeg) y = clipNeg + (y - clipNeg) * 0.05f;
        y = std::tanh (y);
        y = tilt_.process (y);
        return y * levelLin_;
    }
private:
    std::atomic<float> drive_ { 0.4f };
    float levelLin_ = 1.f;
    bool  bypass_   = false;
    TiltOnePole tilt_;
};

// --- 1-pole High-Pass (post-IR DC/sub blocker) -------------------------------
class HighPass {
public:
    void prepare (double sr) { sr_ = sr; updateCoef(); reset(); }
    void reset() { z1_ = 0.f; }
    void setFreqHz (float f) { freq_ = std::clamp (f, 5.f, 800.f); updateCoef(); }
    void setBypass (bool b)  { bypass_ = b; }

    float process (float x)
    {
        if (bypass_) return x;
        // y[n] = a * (y[n-1] + x[n] - x[n-1])  with a = exp(-2*pi*fc/sr)
        const float y = a_ * (z1_ + x - prevIn_);
        prevIn_ = x;
        z1_     = y;
        return y;
    }
private:
    void updateCoef()
    {
        a_ = std::exp (-2.0f * (float) M_PI * freq_ / (float) sr_);
    }
    double sr_ = 48000.0;
    float  freq_ = 30.f;
    bool   bypass_ = false;
    float  a_ = 0.f, z1_ = 0.f, prevIn_ = 0.f;
};

// --- Loudness Normalization (Steve-style output norm) ------------------------
// Slow RMS detector → auto-gain toward target dBFS. Smoothed to avoid pumping.
class LoudnessNorm {
public:
    void prepare (double sr)
    {
        sr_ = sr;
        // RMS window ~400 ms (LUFS short-term-ish).
        rmsCoef_ = std::exp (-1.0f / (0.4f * (float) sr_));
        // Gain smoothing ~200 ms.
        gainCoef_ = std::exp (-1.0f / (0.2f * (float) sr_));
        reset();
    }
    void reset() { rms_ = 1e-6f; gain_ = 1.f; }

    void setEnabled  (bool b)   { enabled_ = b; }
    void setTargetDB (float dB) { targetDB_ = dB; }

    float process (float x)
    {
        // Always track envelope (so toggling on is smooth).
        const float sq = x * x;
        rms_ = rmsCoef_ * rms_ + (1.f - rmsCoef_) * sq;
        if (! enabled_) return x;

        const float rmsDB = 10.f * std::log10 (std::max (rms_, 1e-10f));
        const float diffDB = targetDB_ - rmsDB;
        // Clamp correction to avoid runaway when input is silent.
        const float clampedDB = std::clamp (diffDB, -12.f, 12.f);
        const float targetGain = db2lin (clampedDB);
        gain_ = gainCoef_ * gain_ + (1.f - gainCoef_) * targetGain;
        return x * gain_;
    }
private:
    double sr_ = 48000.0;
    bool   enabled_  = false;
    float  targetDB_ = -18.f;
    float  rmsCoef_  = 0.f, gainCoef_ = 0.f;
    float  rms_ = 1e-6f, gain_ = 1.f;
};

} // namespace preamp_fx
