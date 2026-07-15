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
    ng_.prepare(sampleRate);
    delay_.prepare(sampleRate);
    chorus_.prepare(sampleRate);
    flanger_.prepare(sampleRate);
    reverb_.prepare(sampleRate);
    tremolo_.prepare(sampleRate);
    irHp_.reset();
    irLp_.reset();

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
    ng_.reset();
    delay_.reset();
    chorus_.reset();
    flanger_.reset();
    reverb_.reset();
    tremolo_.reset();
    irHp_.reset();
    irLp_.reset();
    if (ir_) ir_->reset();
    inputGainLin_  = db2lin(inputGainDB_.load());
    outputGainLin_ = db2lin(outputGainDB_.load());
    irTrimGainSmoothed_ = irTrimGain_;  // snap follower to current target
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

    // Steve-style gain-staging (NeuralAmpModeler.cpp:690-730 reference):
    // Input side  = Calibrate Input ? (inputCalDBu - modelInputLevelDBu) : 0
    // Output side = Raw        -> 0
    //               Normalized -> -18 - modelLoudnessDB           (if known)
    //               Calibrated -> modelOutputLevelDBu - inputCalDBu (if known)
    // If the metadata flag for the term is not known, the term is a no-op — this
    // matches Steve's behavior for V1-legacy models with no dbu/loudness info,
    // and avoids fabricated compensations that caused Bug B (output saturation
    // with A2 slim + IR).
    const bool  modelActive = (model_ && !modelBypass_.load());
    float modelInDB  = 0.f;
    float modelOutDB = 0.f;
    if (modelActive) {
        // Explicit Steve-style input calibration when user opts in AND model exposes it.
        if (calibrateInput_.load() && hasInputLevelCached_.load()) {
            modelInDB = inputCalDBu_.load() - modelInputLevelCached_.load();
        }

        switch (outputMode_.load()) {
            case OutputMode::Normalized:
                if (hasLoudnessCached_.load()) {
                    const float totalCorr = -18.f - modelLoudnessCached_.load();
                    // Half-metadata A2 fallback: when loudness is known but the model
                    // exposes no input_level_dbu, splitting the Normalized correction
                    // 50/50 across input and output keeps hot models (e.g. Orange Dual
                    // Terror slim, loudness ~-12) out of the saturated / high-self-noise
                    // region. When input_level_dbu is known Steve's rule wins and the
                    // whole correction lands on the output side.
                    if (!hasInputLevelCached_.load() && modelInDB == 0.f) {
                        modelInDB  = 0.5f * totalCorr;
                        modelOutDB = 0.5f * totalCorr;
                    } else {
                        modelOutDB = totalCorr;
                    }
                }
                break;
            case OutputMode::Calibrated:
                if (hasOutputLevelCached_.load())
                    modelOutDB = modelOutputLevelCached_.load() - inputCalDBu_.load();
                break;
            case OutputMode::Raw:
            default:
                break;
        }
    }

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

    // --- Pre-FX: NoiseGate → Gate → Overdrive → Distortion ---
    for (int i = 0; i < n; ++i) {
        float s = out[i];
        s = ng_.process   (s);
        s = gate_.process (s);
        s = od_.process   (s);
        s = dist_.process (s);
        out[i] = s;
    }

    // --- NAM model ---
    if (model_ && !modelBypass_.load()) {
        model_->Process(out, out, n);
    }

    // --- Model output gain-stage (Steve-style): apply BEFORE IR/post chain ---
    // Placing modelOutDB here (not at final output) matches NeuralAmpModelerPlugin:
    // Normalized/Calibrated correction lands on the raw model output so the IR
    // convolver and post-cab FX see a level-normalized signal. Applying it at
    // the end (after IR) would let hot A2 models saturate the convolution stage
    // before the attenuation ever reaches them.
    if (std::fabs(modelOutDB) > 1e-6f) {
        const float modelOutLin = db2lin(modelOutDB);
        for (int i = 0; i < n; ++i) out[i] *= modelOutLin;
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

    // --- Post-cab: IR tools (phase/HP/LP/trim) → High-pass → Loudness Normalization → Delay → Chorus → Flanger → Reverb ---
    // Smooth irTrimGain_ with a one-pole follower to avoid audible clicks when
    // the user drags the "IR Trim dB" knob; matches the input/output smoothing
    // pattern above (~10 ms tau at 44.1/48 kHz, coefficient 0.99/0.01).
    const float irTrimTarget = irTrimGain_;
    for (int i = 0; i < n; ++i) {
        irTrimGainSmoothed_ = 0.99f * irTrimGainSmoothed_ + 0.01f * irTrimTarget;
        float s = out[i];
        if (irPhaseInv_) s = -s;
        s = irHp_.process (s);
        s = irLp_.process (s);
        s *= irTrimGainSmoothed_;
        s = hp_.process   (s);
        s = loud_.process (s);
        s = delay_.process   (s);
        s = chorus_.process  (s);
        s = flanger_.process (s);
        s = reverb_.process  (s);
        s = tremolo_.process (s);
        out[i] = s;
    }

    // --- Output gain (smoothed) --- (modelOutDB already applied post-model)
    const float desiredOut = db2lin(outputGainDB_.load());
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
    // NeuralAudio parses untrusted JSON with nlohmann (throws on malformed
    // input / missing keys). A corrupt .nam must not take down the host.
    NeuralAudio::NeuralModel* m = nullptr;
    try {
        m = loader_.CreateFromFile(path);
    } catch (...) {
        m = nullptr;
    }
    if (!m) {
        isSlimmable_.store(false);
        hasLoudnessCached_.store(false);
        hasInputLevelCached_.store(false);
        hasOutputLevelCached_.store(false);
        return false;
    }
    model_.reset(m);
    isSlimmable_.store(model_->HasQualityScaling());
    // Snapshot metadata for the audio thread (avoid virtual calls in process()).
    hasLoudnessCached_.store(model_->HasLoudness());
    hasInputLevelCached_.store(model_->HasInputLevel());
    hasOutputLevelCached_.store(model_->HasOutputLevel());
    modelLoudnessCached_.store(model_->GetLoudnessDB());
    modelInputLevelCached_.store(model_->GetInputLevelDBu());
    modelOutputLevelCached_.store(model_->GetOutputLevelDBu());
    return true;
}

void NAMPipeline::setQualityScaleRuntime(float s)
{
    qualityScale_.store(s);
    if (model_ && isSlimmable_.load()) {
        model_->SetQualityScaleFactor(s);
        // Re-snapshot loudness/level metadata: on slimmable A2 containers the
        // active sub-model changes with quality scale, and its LUFS / input /
        // output levels can differ. Without this refresh the Normalized/
        // Calibrated gain-staging uses stale loudness (cached at load time)
        // and can push the model into saturation → IR overvolume.
        hasLoudnessCached_.store(model_->HasLoudness());
        hasInputLevelCached_.store(model_->HasInputLevel());
        hasOutputLevelCached_.store(model_->HasOutputLevel());
        modelLoudnessCached_.store(model_->GetLoudnessDB());
        modelInputLevelCached_.store(model_->GetInputLevelDBu());
        modelOutputLevelCached_.store(model_->GetOutputLevelDBu());
    }
}

bool NAMPipeline::loadIR(const std::string& path)
{
    if (path.empty()) { clearIR(); return false; }
    auto next = std::make_unique<nam_dsp::IRConvolver>();
    try {
        if (!next->loadFromFile(path, sampleRate_)) return false;
    } catch (...) {
        return false; // corrupt/oversized WAV must not crash the host
    }
    next->prepare(128, 1024);
    ir_ = std::move(next);
    return true;
}

void NAMPipeline::clearModel() {
    model_.reset();
    isSlimmable_.store(false);
    hasLoudnessCached_.store(false);
    hasInputLevelCached_.store(false);
    hasOutputLevelCached_.store(false);
}
void NAMPipeline::clearIR()    { ir_.reset(); }

float NAMPipeline::modelInputDBAdjustment()  const { return model_ ? model_->GetRecommendedInputDBAdjustment()  : 0.f; }
float NAMPipeline::modelOutputDBAdjustment() const { return model_ ? model_->GetRecommendedOutputDBAdjustment() : 0.f; }

void NAMPipeline::setIRTools(float hpFreqHz, bool hpBypass,
                             float lpFreqHz, bool lpBypass,
                             float trimDb,   bool phaseInv)
{
    irHp_.setCutoff (hpFreqHz, sampleRate_);
    irHp_.setBypass (hpBypass);
    irLp_.setCutoff (lpFreqHz, sampleRate_);
    irLp_.setBypass (lpBypass);
    irTrimGain_ = std::pow (10.0f, trimDb * 0.05f);
    irPhaseInv_ = phaseInv;
}
