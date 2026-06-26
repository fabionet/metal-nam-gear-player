// DepthFilter.cpp
#include "DepthFilter.h"

namespace nam_dsp {

static constexpr double kDepthFreq = 80.0;
static constexpr double kDepthQ    = 0.707;
static constexpr double kResQ      = 1.0;

void DepthFilter::prepare(double sr, int nCh) {
    sampleRate_ = sr;
    numChannels_ = nCh > kMaxChannels ? kMaxChannels : nCh;
    reset();
    updateDepth();
    updateRes();
}

void DepthFilter::reset() {
    for (auto& f : depth_) f.reset();
    for (auto& f : res_)   f.reset();
}

void DepthFilter::setDepth(float dB) { depthDB_ = dB; updateDepth(); }
void DepthFilter::setResonance(float dB, float f) {
    resDB_ = dB; resFreq_ = f; updateRes();
}

void DepthFilter::updateDepth() {
    for (auto& f : depth_)
        f.setLowShelf(sampleRate_, kDepthFreq, kDepthQ, depthDB_);
}

void DepthFilter::updateRes() {
    for (auto& f : res_)
        f.setPeak(sampleRate_, resFreq_, kResQ, resDB_);
}

} // namespace nam_dsp
