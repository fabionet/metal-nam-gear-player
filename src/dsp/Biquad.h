// Biquad.h - RBJ Audio EQ Cookbook biquad filter (Direct Form I)
// MIT-style, internal use.
#pragma once

#include <cmath>

namespace nam_dsp {

class Biquad {
public:
    enum class Type { LowShelf, HighShelf, Peak };

    // Azzera anche la storia di uscita: lasciare y1_/y2_ sporchi faceva
    // ripartire il filtro con una coda del segnale precedente.
    void reset() { z1_ = z2_ = y1_ = y2_ = 0.0; }

    void setLowShelf(double sampleRate, double freqHz, double q, double gainDB);
    void setHighShelf(double sampleRate, double freqHz, double q, double gainDB);
    void setPeak(double sampleRate, double freqHz, double q, double gainDB);

    // Risposta in ampiezza a una frequenza, per disegnare la curva
    // nell'analizzatore senza duplicare il calcolo dei coefficienti.
    double magnitudeAt(double freqHz, double sampleRate) const {
        const double w = 2.0 * 3.14159265358979323846 * freqHz / sampleRate;
        const double c1 = std::cos(-w),      s1 = std::sin(-w);
        const double c2 = std::cos(-2.0*w),  s2 = std::sin(-2.0*w);
        const double nr = b0_ + b1_*c1 + b2_*c2, ni = b1_*s1 + b2_*s2;
        const double dr = 1.0 + a1_*c1 + a2_*c2, di = a1_*s1 + a2_*s2;
        const double den = std::sqrt(dr*dr + di*di);
        return den > 1e-18 ? std::sqrt(nr*nr + ni*ni) / den : 1.0;
    }

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
