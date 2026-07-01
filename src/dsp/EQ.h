// EQ.h - 5-band tonestack: Bass (low-shelf 100Hz) + Mid (peak, var freq/Q) +
// Presence (peak 3kHz, Q=0.8) + Treble (high-shelf 5kHz) + Air (high-shelf 10kHz).
#pragma once

#include "Biquad.h"
#include <array>

namespace nam_dsp {

class FiveBandEQ {
public:
    static constexpr int kNumBands = 5;
    static constexpr int kMaxChannels = 2;

    void prepare(double sampleRate, int numChannels);

    // dB; 0 = flat.
    void setBass(float dB);
    void setMid(float freqHz, float q, float dB);
    void setPresence(float dB);
    void setTreble(float dB);
    void setAir(float dB);

    // Per-channel, sample-by-sample in series.
    inline float processSample(int ch, float x) {
        for (int b = 0; b < kNumBands; ++b)
            x = bands_[b][ch].process(x);
        return x;
    }

    void reset();

private:
    double sampleRate_ = 48000.0;
    int numChannels_ = 1;

    // bands_[band][channel]
    std::array<std::array<Biquad, kMaxChannels>, kNumBands> bands_;

    // Cached state
    float bassDB_ = 0.f;
    float midFreq_ = 700.f, midQ_ = 0.707f, midDB_ = 0.f;
    float presDB_ = 0.f;
    float treDB_ = 0.f;
    float airDB_ = 0.f;

    void updateBand(int band);
};

} // namespace nam_dsp
