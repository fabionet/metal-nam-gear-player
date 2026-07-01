// EQ.cpp - 5-band tonestack.
#include "EQ.h"

namespace nam_dsp {

// Fixed frequencies for the non-parametric bands.
static constexpr double kBassFreq     = 100.0;   // low-shelf
static constexpr double kPresenceFreq = 3000.0;  // peak
static constexpr double kPresenceQ    = 0.8;
static constexpr double kTrebleFreq   = 5000.0;  // high-shelf
static constexpr double kAirFreq      = 10000.0; // high-shelf
static constexpr double kShelfQ       = 0.707;

void FiveBandEQ::prepare(double sampleRate, int numChannels) {
    sampleRate_ = sampleRate;
    numChannels_ = numChannels > kMaxChannels ? kMaxChannels : numChannels;
    reset();
    // Re-apply all band coefficients at the new sample rate.
    for (int b = 0; b < kNumBands; ++b)
        updateBand(b);
}

void FiveBandEQ::reset() {
    for (auto& band : bands_)
        for (auto& bq : band)
            bq.reset();
}

void FiveBandEQ::setBass(float dB)      { bassDB_ = dB; updateBand(0); }
void FiveBandEQ::setMid(float f, float q, float dB) {
    midFreq_ = f; midQ_ = q; midDB_ = dB; updateBand(1);
}
void FiveBandEQ::setPresence(float dB)  { presDB_ = dB; updateBand(2); }
void FiveBandEQ::setTreble(float dB)    { treDB_  = dB; updateBand(3); }
void FiveBandEQ::setAir(float dB)       { airDB_  = dB; updateBand(4); }

void FiveBandEQ::updateBand(int band) {
    for (int ch = 0; ch < kMaxChannels; ++ch) {
        Biquad& bq = bands_[band][ch];
        switch (band) {
        case 0: bq.setLowShelf (sampleRate_, kBassFreq,     kShelfQ,     bassDB_); break;
        case 1: bq.setPeak     (sampleRate_, midFreq_,      midQ_,       midDB_ ); break;
        case 2: bq.setPeak     (sampleRate_, kPresenceFreq, kPresenceQ,  presDB_); break;
        case 3: bq.setHighShelf(sampleRate_, kTrebleFreq,   kShelfQ,     treDB_ ); break;
        case 4: bq.setHighShelf(sampleRate_, kAirFreq,      kShelfQ,     airDB_ ); break;
        }
    }
}

} // namespace nam_dsp
