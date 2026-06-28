#include "NAMPipeline.h"
#include <cmath>
#include <algorithm>

namespace {
    constexpr float kSmoothEpsilon = 1e-5f;
    inline float db2lin(float dB) { return std::pow(10.0f, dB * 0.05f); }
}

NAMPipeline::NAMPipeline()  = default;
NAMPipeline::~NAMPipeline() = default;

void NAMPipeline::prepare(double sampleRate, int blockSize)
{
    sampleRate_ = sampleRate;
    blockSize_  = blockSize;

    eq_.prepare(sampleRate, 1);
    depth_.prepare(sampleRate, 1);
    gate_.prepare(sampleRate);
    od_.prepare(sampleRate);
    dist_.prepare(sampleRate);
    hp_.prepare(sampleRate);
    loud_.prepare(sampleRate);

    tmp_.assign(static_cast<size_t>(std::max(blockSize, 1)), 0.f);

    // Force a re-push of cached DSP coeffs.
    eqBassCached_ = 999.f;
    eqMidFreqCached_ = -1.f; eqMidQCached_ = -1.f; eqMidDBCached_ = 999.f;
    eqPresCached_ = eqTrebleCached_ = eqAirCached_ = 999.f;
    depthCached_  = 999.f;
    resDBCached_  = 999.f; resFreqCached_ = -1.f;

    if (ir_) ir_->prepare(128, 1024);
}

void NAMPipeline::reset()
{
    eq_.reset();
    depth_.reset();
    gate_.reset();
    od_.reset();
    dist_.reset();
    hp_.reset();
    loud_.reset();
    if (ir_) ir_->reset();
    inputGainLin_  = db2lin(inputGainDB_.load());
    outputGainLin_ = db2lin(outputGainDB_.load());
}

void NAMPipeline::updateCachedDsp()
{
    if (eqBassDB_   != eqBassCached_)   { eq_.setBass(eqBassDB_);       eqBassCached_   = eqBassDB_; }
    if (eqPresDB_   != eqPresCached_)   { eq_.setPresence(eqPresDB_);   eqPresCached_   = eqPresDB_; }
    if (eqTrebleDB_ != eqTrebleCached_) { eq_.setTreble(eqTrebleDB_);   eqTrebleCached_ = eqTrebleDB_; }
    if (eqAirDB_    != eqAirCached_)    { eq_.setAir(eqAirDB_);         eqAirCached_    = eqAirDB_; }
    if (eqMidFreq_ != eqMidFreqCached_ || eqMidQ_ != eqMidQCached_ || eqMidDB_ != eqMidDBCached_) {
        eq_.setMid(eqMidFreq_, eqMidQ_, eqMidDB_);
        eqMidFreqCached_ = eqMidFreq_;
        eqMidQCached_    = eqMidQ_;
        eqMidDBCached_   = eqMidDB_;
    }
    if (depthDB_ != depthCached_) { depth_.setDepth(depthDB_); depthCached_ = depthDB_; }
    if (resDB_ != resDBCached_ || resFreq_ != resFreqCached_) {
        depth_.setResonance(resDB_, resFreq_);
        resDBCached_   = resDB_;
        resFreqCached_ = resFreq_;
    }
}

void NAMPipeline::process(const float* in, float* out, int n)
{
    if (n <= 0) return;
    updateCachedDsp();

    const float modelInDB  = (model_ && !modelBypass_.load()) ? model_->GetRecommendedInputDBAdjustment()  : 0.f;
    const float modelOutDB = (model_ && !modelBypass_.load()) ? model_->GetRecommendedOutputDBAdjustment() : 0.f;

    // --- Input gain (smoothed) ---
    const float desiredIn = db2lin(inputGainDB_.load() + modelInDB);
    if (std::fabs(desiredIn - inputGainLin_) > kSmoothEpsilon) {
        float g = inputGainLin_;
        for (int i = 0; i < n; ++i) {
            g = 0.99f * g + 0.01f * desiredIn;
            out[i] = in[i] * g;
        }
        inputGainLin_ = g;
    } else {
        inputGainLin_ = desiredIn;
        for (int i = 0; i < n; ++i) out[i] = in[i] * desiredIn;
    }

    // --- Pre-FX: Gate → Overdrive → Distortion ---
    for (int i = 0; i < n; ++i) {
        float s = out[i];
        s = gate_.process (s);
        s = od_.process   (s);
        s = dist_.process (s);
        out[i] = s;
    }

    // --- NAM model ---
    if (model_ && !modelBypass_.load()) {
        model_->Process(out, out, n);
    }

    // --- Depth + Resonance ---
    for (int i = 0; i < n; ++i) out[i] = depth_.processSample(0, out[i]);

    // --- 5-band EQ ---
    for (int i = 0; i < n; ++i) out[i] = eq_.processSample(0, out[i]);

    // --- IR convolver (dry/wet) ---
    if (ir_ && ir_->isReady() && !irBypass_.load()) {
        const float mix = std::clamp(irMix_.load(), 0.f, 1.f);
        if (mix >= 0.9999f) {
            ir_->process(out, out, static_cast<size_t>(n));
        } else if (mix > 0.f) {
            if ((int)tmp_.size() < n) tmp_.assign(n, 0.f);
            ir_->process(out, tmp_.data(), static_cast<size_t>(n));
            const float dry = 1.f - mix;
            for (int i = 0; i < n; ++i) out[i] = out[i] * dry + tmp_[i] * mix;
        }
    }

    // --- Post-cab: High-pass → Loudness Normalization ---
    for (int i = 0; i < n; ++i) {
        float s = hp_.process (out[i]);
        s = loud_.process (s);
        out[i] = s;
    }

    // --- Output gain (smoothed) ---
    const float desiredOut = db2lin(outputGainDB_.load() + modelOutDB);
    if (std::fabs(desiredOut - outputGainLin_) > kSmoothEpsilon) {
        float g = outputGainLin_;
        for (int i = 0; i < n; ++i) {
            g = 0.99f * g + 0.01f * desiredOut;
            out[i] *= g;
        }
        outputGainLin_ = g;
    } else {
        outputGainLin_ = desiredOut;
        for (int i = 0; i < n; ++i) out[i] *= desiredOut;
    }
}

bool NAMPipeline::loadModel(const std::string& path)
{
    if (path.empty()) { clearModel(); return false; }
    loader_.SetExternalSampleRate(static_cast<int>(sampleRate_));
    loader_.SetDefaultMaxAudioBufferSize(blockSize_);
    loader_.SetDefaultQualityScaleFactor(qualityScale_.load());
    NeuralAudio::NeuralModel* m = loader_.CreateFromFile(path);
    if (!m) return false;
    model_.reset(m);
    return true;
}

bool NAMPipeline::loadIR(const std::string& path)
{
    if (path.empty()) { clearIR(); return false; }
    auto next = std::make_unique<nam_dsp::IRConvolver>();
    if (!next->loadFromFile(path, sampleRate_)) return false;
    next->prepare(128, 1024);
    ir_ = std::move(next);
    return true;
}

void NAMPipeline::clearModel() { model_.reset(); }
void NAMPipeline::clearIR()    { ir_.reset(); }

float NAMPipeline::modelInputDBAdjustment()  const { return model_ ? model_->GetRecommendedInputDBAdjustment()  : 0.f; }
float NAMPipeline::modelOutputDBAdjustment() const { return model_ ? model_->GetRecommendedOutputDBAdjustment() : 0.f; }
