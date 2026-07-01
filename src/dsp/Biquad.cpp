// Biquad.cpp - RBJ cookbook coefficient calculation.
#include "Biquad.h"

namespace nam_dsp {

static constexpr double kPi = 3.14159265358979323846;

static inline void normalize(double a0, double& b0, double& b1, double& b2, double& a1, double& a2) {
    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
}

void Biquad::setLowShelf(double sr, double f, double q, double gainDB) {
    const double A = std::pow(10.0, gainDB / 40.0);
    const double w0 = 2.0 * kPi * f / sr;
    const double cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / (2.0 * q);
    const double sqrtA2alpha = 2.0 * std::sqrt(A) * alpha;

    double b0 =      A * ((A + 1) - (A - 1) * cw + sqrtA2alpha);
    double b1 =  2 * A * ((A - 1) - (A + 1) * cw);
    double b2 =      A * ((A + 1) - (A - 1) * cw - sqrtA2alpha);
    double a0 =          (A + 1) + (A - 1) * cw + sqrtA2alpha;
    double a1 = -2 *    ((A - 1) + (A + 1) * cw);
    double a2 =          (A + 1) + (A - 1) * cw - sqrtA2alpha;

    normalize(a0, b0, b1, b2, a1, a2);
    b0_ = b0; b1_ = b1; b2_ = b2; a1_ = a1; a2_ = a2;
}

void Biquad::setHighShelf(double sr, double f, double q, double gainDB) {
    const double A = std::pow(10.0, gainDB / 40.0);
    const double w0 = 2.0 * kPi * f / sr;
    const double cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / (2.0 * q);
    const double sqrtA2alpha = 2.0 * std::sqrt(A) * alpha;

    double b0 =       A * ((A + 1) + (A - 1) * cw + sqrtA2alpha);
    double b1 = -2 *  A * ((A - 1) + (A + 1) * cw);
    double b2 =       A * ((A + 1) + (A - 1) * cw - sqrtA2alpha);
    double a0 =           (A + 1) - (A - 1) * cw + sqrtA2alpha;
    double a1 =  2 *     ((A - 1) - (A + 1) * cw);
    double a2 =           (A + 1) - (A - 1) * cw - sqrtA2alpha;

    normalize(a0, b0, b1, b2, a1, a2);
    b0_ = b0; b1_ = b1; b2_ = b2; a1_ = a1; a2_ = a2;
}

void Biquad::setPeak(double sr, double f, double q, double gainDB) {
    const double A = std::pow(10.0, gainDB / 40.0);
    const double w0 = 2.0 * kPi * f / sr;
    const double cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / (2.0 * q);

    double b0 = 1 + alpha * A;
    double b1 = -2 * cw;
    double b2 = 1 - alpha * A;
    double a0 = 1 + alpha / A;
    double a1 = -2 * cw;
    double a2 = 1 - alpha / A;

    normalize(a0, b0, b1, b2, a1, a2);
    b0_ = b0; b1_ = b1; b2_ = b2; a1_ = a1; a2_ = a2;
}

} // namespace nam_dsp
