// Stage 5 — pre-amp FX: smart noise gate, overdrive, distortion.
// Header-only, single-channel, RT-safe.
#pragma once

#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

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

// --- Simple NoiseGate (pre-chain) --------------------------------------------
// Hard threshold downward gate with smoothed gain (release ms controls close).
class NoiseGate {
public:
    void prepare (double sampleRate)
    {
        sr_ = sampleRate;
        attCoef_ = std::exp (-1.0f / (0.001f * (float) sr_)); // ~1 ms attack
        updateRelease();
        env_ = 0.f; gain_ = 1.f;
    }
    void reset() { env_ = 0.f; gain_ = 1.f; }

    void setThresholdDB (float dB) { thresholdDB_ = dB; }
    void setReleaseMs   (float ms) { releaseMs_ = std::max (5.f, ms); updateRelease(); }
    void setBypass      (bool b)   { bypass_ = b; }

    float process (float x)
    {
        if (bypass_) return x;
        const float a = std::fabs (x);
        const float coef = (a > env_) ? attCoef_ : envRelCoef_;
        env_ = coef * env_ + (1.f - coef) * a;

        const float thrLin = db2lin (thresholdDB_);
        const float target = (env_ > thrLin) ? 1.f : 0.f;
        gain_ = gainCoef_ * gain_ + (1.f - gainCoef_) * target;
        return x * gain_;
    }
private:
    void updateRelease()
    {
        envRelCoef_ = std::exp (-1.0f / (0.001f * releaseMs_ * (float) sr_));
        // Gain smoothing ~ releaseMs/2.
        gainCoef_ = std::exp (-1.0f / (0.001f * std::max (5.f, releaseMs_ * 0.5f) * (float) sr_));
    }
    double sr_ = 48000.0;
    float thresholdDB_ = -55.f;
    float releaseMs_ = 80.f;
    bool  bypass_ = false;
    float attCoef_ = 0.f, envRelCoef_ = 0.f, gainCoef_ = 0.f;
    float env_ = 0.f, gain_ = 1.f;
};

// --- Delay (mono, simple ring buffer with feedback) --------------------------
class DelayFX {
public:
    void prepare (double sampleRate)
    {
        sr_ = sampleRate;
        const int maxSamples = (int) std::ceil (sr_ * 2.5); // 2.5 s headroom
        buf_.assign ((size_t) maxSamples, 0.f);
        writeIdx_ = 0;
    }
    void reset() { std::fill (buf_.begin(), buf_.end(), 0.f); writeIdx_ = 0; }

    void setTimeMs    (float ms) { timeSamples_ = std::clamp (ms, 1.f, 2000.f) * 0.001f * (float) sr_; }
    void setFeedback  (float f)  { feedback_ = std::clamp (f, 0.f, 0.9f); }
    void setMix       (float m)  { mix_ = std::clamp (m, 0.f, 1.f); }
    void setBypass    (bool b)   { bypass_ = b; }

    float process (float x)
    {
        if (bypass_ || buf_.empty()) return x;
        const int N = (int) buf_.size();
        const float ts = std::clamp (timeSamples_, 1.f, (float) N - 2.f);
        // Linear interp read.
        float readPos = (float) writeIdx_ - ts;
        while (readPos < 0.f) readPos += (float) N;
        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % N;
        const float frac = readPos - (float) i0;
        const float y = buf_[(size_t) i0] * (1.f - frac) + buf_[(size_t) i1] * frac;
        // Write input + feedback.
        buf_[(size_t) writeIdx_] = x + y * feedback_;
        writeIdx_ = (writeIdx_ + 1) % N;
        return x * (1.f - mix_) + y * mix_;
    }
private:
    double sr_ = 48000.0;
    std::vector<float> buf_;
    int   writeIdx_ = 0;
    float timeSamples_ = 0.f;
    float feedback_ = 0.35f;
    float mix_ = 0.25f;
    bool  bypass_ = true;
};

// --- Chorus (one voice, modulated short delay) -------------------------------
class ChorusFX {
public:
    void prepare (double sampleRate)
    {
        sr_ = sampleRate;
        const int N = (int) std::ceil (sr_ * 0.05); // 50 ms
        buf_.assign ((size_t) N, 0.f);
        writeIdx_ = 0;
        phase_ = 0.f;
    }
    void reset() { std::fill (buf_.begin(), buf_.end(), 0.f); writeIdx_ = 0; phase_ = 0.f; }

    void setRateHz (float r) { rateHz_ = std::clamp (r, 0.01f, 10.f); }
    void setDepth  (float d) { depth_ = std::clamp (d, 0.f, 1.f); }
    void setMix    (float m) { mix_ = std::clamp (m, 0.f, 1.f); }
    void setBypass (bool b)  { bypass_ = b; }

    float process (float x)
    {
        if (bypass_ || buf_.empty()) return x;
        const int N = (int) buf_.size();
        phase_ += 2.f * (float) M_PI * rateHz_ / (float) sr_;
        if (phase_ > 2.f * (float) M_PI) phase_ -= 2.f * (float) M_PI;
        // Base ~15ms + ±10ms * depth.
        const float baseMs = 15.f;
        const float modMs  = 10.f * depth_ * std::sin (phase_);
        const float delaySamples = std::clamp ((baseMs + modMs) * 0.001f * (float) sr_, 1.f, (float) N - 2.f);
        float readPos = (float) writeIdx_ - delaySamples;
        while (readPos < 0.f) readPos += (float) N;
        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % N;
        const float frac = readPos - (float) i0;
        const float y = buf_[(size_t) i0] * (1.f - frac) + buf_[(size_t) i1] * frac;
        buf_[(size_t) writeIdx_] = x;
        writeIdx_ = (writeIdx_ + 1) % N;
        return x * (1.f - mix_) + y * mix_;
    }
private:
    double sr_ = 48000.0;
    std::vector<float> buf_;
    int   writeIdx_ = 0;
    float rateHz_ = 0.8f;
    float depth_ = 0.4f;
    float mix_ = 0.3f;
    float phase_ = 0.f;
    bool  bypass_ = true;
};

// --- Flanger (very short delay + feedback + LFO) -----------------------------
class FlangerFX {
public:
    void prepare (double sampleRate)
    {
        sr_ = sampleRate;
        const int N = (int) std::ceil (sr_ * 0.025); // 25 ms
        buf_.assign ((size_t) N, 0.f);
        writeIdx_ = 0;
        phase_ = 0.f;
    }
    void reset() { std::fill (buf_.begin(), buf_.end(), 0.f); writeIdx_ = 0; phase_ = 0.f; }

    void setRateHz   (float r) { rateHz_ = std::clamp (r, 0.01f, 10.f); }
    void setDepth    (float d) { depth_ = std::clamp (d, 0.f, 1.f); }
    void setFeedback (float f) { feedback_ = std::clamp (f, 0.f, 0.9f); }
    void setMix      (float m) { mix_ = std::clamp (m, 0.f, 1.f); }
    void setBypass   (bool b)  { bypass_ = b; }

    float process (float x)
    {
        if (bypass_ || buf_.empty()) return x;
        const int N = (int) buf_.size();
        phase_ += 2.f * (float) M_PI * rateHz_ / (float) sr_;
        if (phase_ > 2.f * (float) M_PI) phase_ -= 2.f * (float) M_PI;
        // Base ~3ms + ±2.5ms * depth (very short).
        const float baseMs = 3.f;
        const float modMs  = 2.5f * depth_ * std::sin (phase_);
        const float delaySamples = std::clamp ((baseMs + modMs) * 0.001f * (float) sr_, 1.f, (float) N - 2.f);
        float readPos = (float) writeIdx_ - delaySamples;
        while (readPos < 0.f) readPos += (float) N;
        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % N;
        const float frac = readPos - (float) i0;
        const float y = buf_[(size_t) i0] * (1.f - frac) + buf_[(size_t) i1] * frac;
        buf_[(size_t) writeIdx_] = x + y * feedback_;
        writeIdx_ = (writeIdx_ + 1) % N;
        return x * (1.f - mix_) + y * mix_;
    }
private:
    double sr_ = 48000.0;
    std::vector<float> buf_;
    int   writeIdx_ = 0;
    float rateHz_ = 0.3f;
    float depth_ = 0.5f;
    float feedback_ = 0.4f;
    float mix_ = 0.25f;
    float phase_ = 0.f;
    bool  bypass_ = true;
};

// --- Reverb (Freeverb-style mono, 8 comb + 4 allpass) ------------------------
class ReverbFX {
public:
    void prepare (double sampleRate)
    {
        sr_ = sampleRate;
        const double scale = sr_ / 44100.0;
        static const int combLenRef[kNComb] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int allpLenRef[kNAllp] = { 225, 341, 441, 556 };
        for (int i = 0; i < kNComb; ++i) {
            comb_[i].len = std::max (1, (int) std::round (combLenRef[i] * scale));
            comb_[i].buf.assign ((size_t) comb_[i].len, 0.f);
            comb_[i].idx = 0;
            comb_[i].lpz = 0.f;
        }
        for (int i = 0; i < kNAllp; ++i) {
            allp_[i].len = std::max (1, (int) std::round (allpLenRef[i] * scale));
            allp_[i].buf.assign ((size_t) allp_[i].len, 0.f);
            allp_[i].idx = 0;
        }
    }
    void reset()
    {
        for (auto& c : comb_) { std::fill (c.buf.begin(), c.buf.end(), 0.f); c.idx = 0; c.lpz = 0.f; }
        for (auto& a : allp_) { std::fill (a.buf.begin(), a.buf.end(), 0.f); a.idx = 0; }
    }

    void setRoomSize (float r) { roomSize_ = std::clamp (r, 0.f, 1.f); }
    void setDamping  (float d) { damping_  = std::clamp (d, 0.f, 1.f); }
    void setMix      (float m) { mix_      = std::clamp (m, 0.f, 1.f); }
    void setBypass   (bool  b) { bypass_ = b; }

    float process (float x)
    {
        if (bypass_) return x;
        const float fb   = 0.7f + roomSize_ * 0.28f; // 0.70 .. 0.98
        const float damp = damping_ * 0.4f;          // 0 .. 0.4
        const float input = x * 0.015f;              // Freeverb fixed input gain
        float sum = 0.f;
        for (int i = 0; i < kNComb; ++i) {
            auto& c = comb_[i];
            const float y = c.buf[(size_t) c.idx];
            c.lpz = y * (1.f - damp) + c.lpz * damp;
            c.buf[(size_t) c.idx] = input + c.lpz * fb;
            c.idx = (c.idx + 1) % c.len;
            sum += y;
        }
        float y = sum;
        for (int i = 0; i < kNAllp; ++i) {
            auto& a = allp_[i];
            const float bufout = a.buf[(size_t) a.idx];
            const float in = y;
            a.buf[(size_t) a.idx] = in + bufout * 0.5f;
            a.idx = (a.idx + 1) % a.len;
            y = bufout - in;
        }
        return x * (1.f - mix_) + y * mix_;
    }
private:
    static constexpr int kNComb = 8;
    static constexpr int kNAllp = 4;
    struct Comb { std::vector<float> buf; int len = 0, idx = 0; float lpz = 0.f; };
    struct Allp { std::vector<float> buf; int len = 0, idx = 0; };
    Comb comb_[kNComb];
    Allp allp_[kNAllp];
    double sr_ = 48000.0;
    float roomSize_ = 0.5f;
    float damping_  = 0.5f;
    float mix_      = 0.25f;
    bool  bypass_   = true;
};

// -------- Butterworth 2nd-order biquad (TDF-II) --------------------------
class Biquad2 {
public:
    void setBypass (bool b) { bypass_ = b; }
    void reset ()           { z1_ = z2_ = 0.f; }
    float process (float x)
    {
        if (bypass_) return x;
        const float y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }
protected:
    float b0_ = 1.f, b1_ = 0.f, b2_ = 0.f, a1_ = 0.f, a2_ = 0.f;
    float z1_ = 0.f, z2_ = 0.f;
    bool  bypass_ = true;

    void setLowPass (float fcHz, double sr)
    {
        const double w0 = 2.0 * M_PI * (double) fcHz / sr;
        const double cosw = std::cos (w0);
        const double sinw = std::sin (w0);
        const double alpha = sinw / (2.0 * 0.70710678); // Q = 1/sqrt(2)
        const double b0 = (1.0 - cosw) * 0.5;
        const double b1 =  1.0 - cosw;
        const double b2 = (1.0 - cosw) * 0.5;
        const double a0 =  1.0 + alpha;
        const double a1 = -2.0 * cosw;
        const double a2 =  1.0 - alpha;
        b0_ = (float) (b0 / a0); b1_ = (float) (b1 / a0); b2_ = (float) (b2 / a0);
        a1_ = (float) (a1 / a0); a2_ = (float) (a2 / a0);
    }
    void setHighPass (float fcHz, double sr)
    {
        const double w0 = 2.0 * M_PI * (double) fcHz / sr;
        const double cosw = std::cos (w0);
        const double sinw = std::sin (w0);
        const double alpha = sinw / (2.0 * 0.70710678);
        const double b0 =  (1.0 + cosw) * 0.5;
        const double b1 = -(1.0 + cosw);
        const double b2 =  (1.0 + cosw) * 0.5;
        const double a0 =  1.0 + alpha;
        const double a1 = -2.0 * cosw;
        const double a2 =  1.0 - alpha;
        b0_ = (float) (b0 / a0); b1_ = (float) (b1 / a0); b2_ = (float) (b2 / a0);
        a1_ = (float) (a1 / a0); a2_ = (float) (a2 / a0);
    }
};

class BiquadHPF : public Biquad2 {
public:
    void setCutoff (float fcHz, double sr) { setHighPass (fcHz, sr); }
};

class BiquadLPF : public Biquad2 {
public:
    void setCutoff (float fcHz, double sr) { setLowPass (fcHz, sr); }
};

// --- Tremolo (LFO amplitude modulation) --------------------------------------
class TremoloFX {
public:
    void setBypass (bool b)    { bypass_ = b; }
    void setRateHz (float r)   { rateHz_  = std::clamp (r, 0.05f, 20.f); updatePhaseInc(); }
    void setDepth  (float d)   { depth_   = std::clamp (d, 0.f, 1.f); }
    void setShape  (float s)   { shape_   = std::clamp (s, 0.f, 1.f); } // 0=sine, 1=square-ish
    void prepare (double sr)   { sr_ = sr; phase_ = 0.f; updatePhaseInc(); }
    void reset ()              { phase_ = 0.f; }
    float process (float x)
    {
        if (bypass_) return x;
        // LFO: sine → square blend via tanh compression on sine.
        const float s = std::sin (phase_);
        const float sq = std::tanh (s * 6.f);
        const float lfo = s * (1.f - shape_) + sq * shape_;
        // Map LFO from [-1..+1] to [1-depth .. 1] (unipolar downward modulation).
        const float gain = 1.f - depth_ * 0.5f * (1.f - lfo);
        phase_ += phaseInc_;
        if (phase_ >= 6.2831853f) phase_ -= 6.2831853f;
        return x * gain;
    }
private:
    void updatePhaseInc()
    {
        phaseInc_ = (float) (2.0 * 3.141592653589793 * (double) rateHz_ / std::max (1.0, sr_));
    }
    bool  bypass_ = true;
    float rateHz_ = 4.f;
    float depth_  = 0.5f;
    float shape_  = 0.f;
    double sr_ = 48000.0;
    float phase_ = 0.f;
    float phaseInc_ = 0.f;
};

} // namespace preamp_fx
