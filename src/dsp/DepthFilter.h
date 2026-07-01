// DepthFilter.h - Mesa-style Depth (low-shelf 80Hz) + Resonance (peak ~100Hz, var freq).
#pragma once

#include "Biquad.h"
#include <array>

namespace nam_dsp {

class DepthFilter {
public:
    static constexpr int kMaxChannels = 2;

    void prepare(double sampleRate, int numChannels);
    void reset();

    void setDepth(float dB);                       // low-shelf 80 Hz gain
    void setResonance(float dB, float freqHz);     // peak Q=1.0

    inline float processSample(int ch, float x) {
        x = depth_[ch].process(x);
        x = res_[ch].process(x);
        return x;
    }

private:
    double sampleRate_ = 48000.0;
    int numChannels_ = 1;

    std::array<Biquad, kMaxChannels> depth_;
    std::array<Biquad, kMaxChannels> res_;

    float depthDB_ = 0.f;
    float resDB_   = 0.f;
    float resFreq_ = 100.f;

    void updateDepth();
    void updateRes();
};

} // namespace nam_dsp
