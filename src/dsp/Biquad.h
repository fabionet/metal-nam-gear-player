// Biquad.h - RBJ Audio EQ Cookbook biquad filter (Direct Form I)
// MIT-style, internal use.
#pragma once

#include <cmath>

namespace nam_dsp {

class Biquad {
public:
    enum class Type { LowShelf, HighShelf, Peak };

    void reset() { z1_ = z2_ = 0.0; }

    void setLowShelf(double sampleRate, double freqHz, double q, double gainDB);
    void setHighShelf(double sampleRate, double freqHz, double q, double gainDB);
    void setPeak(double sampleRate, double freqHz, double q, double gainDB);

    inline float process(float x) {
        double y = b0_ * x + b1_ * z1_ + b2_ * z2_ - a1_ * y1_ - a2_ * y2_;
        z2_ = z1_; z1_ = x;
        y2_ = y1_; y1_ = y;
        return static_cast<float>(y);
    }

private:
    double b0_=1.0, b1_=0.0, b2_=0.0;
    double a1_=0.0, a2_=0.0;
    // Direct form I state
    double z1_=0.0, z2_=0.0;   // input history
    double y1_=0.0, y2_=0.0;   // output history
};

} // namespace nam_dsp
