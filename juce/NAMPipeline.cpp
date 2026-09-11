#include "NAMPipeline.h"
#include <cmath>
#include <algorithm>
#include <cctype>

namespace {
    constexpr float kSmoothEpsilon = 1e-5f;
    inline float db2lin(float dB) { return std::pow(10.0f, dB * 0.05f); }

    // Rete di sicurezza sull'uscita: sotto -1 dBFS non tocca un bit, sopra piega
    // il segnale con un ginocchio tanh che tende asintoticamente a 1.0. Serve a
    // impedire che un modello caldo o un preset spinto mandino l'host in over;
    // non sostituisce una calibrazione corretta, la protegge soltanto.
    constexpr float kClipKnee = 0.891251f;   // -1 dBFS
    inline float safetyClip(float x) {
        const float a = std::fabs(x);
        if (a <= kClipKnee) return x;
        const float head   = 1.0f - kClipKnee;
        const float shaped = kClipKnee + head * std::tanh((a - kClipKnee) / head);
        return x < 0.f ? -shaped : shaped;
    }
}

NAMPipeline::NAMPipeline()  = default;
NAMPipeline::~NAMPipeline() = default;

void NAMPipeline::prepare(double sampleRate, int blockSize)
{
    sampleRate_ = sampleRate;
    blockSize_  = blockSize;
    preparedSampleRate_ = sampleRate;
    preparedBlockSize_  = blockSize;

    eq_.prepare(sampleRate, 1);
    depth_.prepare(sampleRate, 1);
    comp_.prepare(sampleRate);
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
    amp_.prepare(sampleRate);
    marshall_.prepare(sampleRate);
    irHp_.reset();
    irLp_.reset();

    tmp_.assign(static_cast<size_t>(std::max(blockSize, 1)), 0.f);

    // Aggancia i follower di guadagno al bersaglio corrente. Senza questo una
    // pipeline appena costruita parte da 1.0 (0 dB) e ci mette ~10 ms a scendere:
    // con un preset che chiede -18 dB di uscita, ogni caricamento di modello
    // lasciava passare una raffica di quella durata al livello vecchio.
    inputGainLin_  = db2lin(inputGainDB_.load());
    outputGainLin_ = db2lin(outputGainDB_.load());
    irTrimGainSmoothed_ = irTrimGain_;

    // Force a re-push of cached DSP coeffs.
    eqBassCached_ = 999.f;
    eqMidFreqCached_ = -1.f; eqMidQCached_ = -1.f; eqMidDBCached_ = 999.f;
    eqPresCached_ = eqTrebleCached_ = eqAirCached_ = 999.f;
    depthCached_  = 999.f;
    resDBCached_  = 999.f; resFreqCached_ = -1.f;
    modelBassCached_ = modelMidCached_ = modelTrebCached_ = 999.f;
    modelVolLin_ = db2lin(modelVolDB_);

    if (ir_)  ir_ ->prepare(128, 1024);
    if (ir2_) ir2_->prepare(128, 1024);
}

void NAMPipeline::reset()
{
    eq_.reset();
    depth_.reset();
    comp_.reset();
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
    amp_.reset();
    marshall_.reset();
    irHp_.reset();
    irLp_.reset();
    if (ir_)  ir_ ->reset();
    if (ir2_) ir2_->reset();
    lastIr1Peak_ = lastIr2Peak_ = 0.f;
    inputGainLin_  = db2lin(inputGainDB_.load());
    outputGainLin_ = db2lin(outputGainDB_.load());
    irTrimGainSmoothed_ = irTrimGain_;  // snap follower to current target
    modelBass_.reset(); modelMid_.reset(); modelTreble_.reset();
    modelVolLin_   = db2lin(modelVolDB_);
    lastModelPeak_ = 0.f;
    lastIr1Peak_   = 0.f;
    lastIr2Peak_   = 0.f;
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
    if (modelBassDB_ != modelBassCached_) { modelBass_.setLowShelf   (sampleRate_, 120.0,  0.7, modelBassDB_); modelBassCached_ = modelBassDB_; }
    if (modelMidDB_  != modelMidCached_)  { modelMid_.setPeak        (sampleRate_, 750.0,  0.8, modelMidDB_);  modelMidCached_  = modelMidDB_;  }
    if (modelTrebDB_ != modelTrebCached_) { modelTreble_.setHighShelf(sampleRate_, 3000.0, 0.7, modelTrebDB_); modelTrebCached_ = modelTrebDB_; }
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

    // --- Pre-FX: (Compressor@Front) → NoiseGate → Gate → (Compressor@Post-Gate) → Overdrive → Distortion ---
    // compPos_: 0=Front (pre-gate), 1=Post-Gate, 2=Post-IR. Only one position is
    // active per block; the GR meter reads comp_ regardless of where it sits.
    const int cpos = compPos_.load();
    for (int i = 0; i < n; ++i) {
        float s = out[i];
        if (cpos == 0) s = comp_.process (s);
        s = ng_.process   (s);
        s = gate_.process (s);
        if (cpos == 1) s = comp_.process (s);
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

    // --- Stadio del lettore NAM: tonestack 3 bande + volume + tap del meter ---
    // Sta dopo il gain-stage del modello e prima dell'ampli nativo: il volume
    // regola quanto il NAM spinge nell'ampli, e il meter mostra esattamente
    // il livello che esce dal lettore.
    {
        const float desiredMv = db2lin(modelVolDB_);
        float g  = modelVolLin_;
        float pk = 0.f;
        for (int i = 0; i < n; ++i) {
            g = 0.99f * g + 0.01f * desiredMv;
            float s = modelBass_.process(out[i]);
            s = modelMid_.process(s);
            s = modelTreble_.process(s);
            s *= g;
            out[i] = s;
            const float a = std::fabs(s);
            if (a > pk) pk = a;
        }
        modelVolLin_   = g;
        lastModelPeak_ = pk;
    }

    // --- Native tube amp (optional; NAM acts as a drive pedal upstream) ---
    // Placed after the model gain-stage and before Depth so the routing is
    // pre-FX → NAM(pedal) → native amp → depth/EQ → IR cab → post-FX.
    // Each amp carries its own power state: ampEnabled_ for GEAR SX, marshallEnabled_
    // for MARCHELLOW. Gating on ampEnabled_ alone made the MARCHELLOW branch
    // unreachable (ampEnabled_ is only ever true while ampModel_ == 0).
    if (ampModel_.load() == 0) {
        if (ampEnabled_.load())
            for (int i = 0; i < n; ++i) out[i] = amp_.process(out[i]);
    } else {
        if (marshallEnabled_.load())
            for (int i = 0; i < n; ++i) out[i] = marshall_.process(out[i]);
    }

    // --- Depth + Resonance (POWER section, bypassable) ---
    if (!depthBypass_.load())
        for (int i = 0; i < n; ++i) out[i] = depth_.processSample(0, out[i]);

    // --- 5-band EQ ---
    for (int i = 0; i < n; ++i) out[i] = eq_.processSample(0, out[i]);

    // Presa per l'analizzatore: il segnale viene campionato qui, subito dopo
    // l'equalizzatore, cosi' nello spettro si vede l'effetto della curva.
    {
        int w = scopeWrite_.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) {
            scope_[(size_t) w] = out[i];
            w = (w + 1) & (kScopeSize - 1);
        }
        scopeWrite_.store(w, std::memory_order_release);
    }

    // --- IR convolver (due IR incrociati, poi dry/wet) ---
    // Entrambi i convolutori ricevono lo stesso ingresso, quindi `out` non va
    // sovrascritto finche' non hanno elaborato: si usano due buffer separati.
    // Quando il secondo e' abilitato viene elaborato anche a peso zero, cosi'
    // la sua coda resta viva e ruotare il bilanciamento non produce scalini.
    lastIr1Peak_ = lastIr2Peak_ = 0.f;   // in bypass i meter devono scendere
    if (!irBypass_.load()) {
        const bool have1 = ir_  != nullptr && ir_ ->isReady();
        const bool have2 = ir2Enable_.load() && ir2_ != nullptr && ir2_->isReady();
        const float mix = std::clamp(irMix_.load(), 0.f, 1.f);
        if ((have1 || have2) && mix > 0.f) {
            if ((int)tmp_.size()  < n) tmp_ .assign(n, 0.f);
            if ((int)tmp2_.size() < n) tmp2_.assign(n, 0.f);
            if (have1) ir_ ->process(out, tmp_ .data(), static_cast<size_t>(n));
            if (have2) ir2_->process(out, tmp2_.data(), static_cast<size_t>(n));
            // Con un solo IR presente il bilanciamento non deve attenuarlo.
            const float bal = (have1 && have2) ? std::clamp(irBalance_.load(), 0.f, 1.f)
                                               : (have2 ? 1.f : 0.f);
            // Il volume di ciascun IR entra come fattore lineare nel peso.
            const float w1 = (1.f - bal) * db2lin(ir1VolDB_.load());
            const float w2 =         bal  * db2lin(ir2VolDB_.load());
            const float dry = 1.f - mix;
            float pk1 = 0.f, pk2 = 0.f;
            for (int i = 0; i < n; ++i) {
                const float a = have1 ? tmp_ [i] * w1 : 0.f;
                const float b = have2 ? tmp2_[i] * w2 : 0.f;
                const float fa = std::fabs(a); if (fa > pk1) pk1 = fa;
                const float fb = std::fabs(b); if (fb > pk2) pk2 = fb;
                out[i] = out[i] * dry + (a + b) * mix;
            }
            lastIr1Peak_ = pk1 * mix;
            lastIr2Peak_ = pk2 * mix;
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
        if (cpos == 2) s = comp_.process (s);
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

    for (int i = 0; i < n; ++i) out[i] = safetyClip(out[i]);
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
    // gear_type arriva da GetMetadata come valore JSON grezzo, quindi fra
    // virgolette. I valori osservati sul campo sono amp, pedal, amp_cab e
    // full-rig; si cercano le radici "cab" e "rig" per coprire anche le
    // varianti di scrittura (full_rig, amp+cab).
    {
        std::string gt = model_->GetMetadata("gear_type");
        for (auto& ch : gt) ch = (char) std::tolower((unsigned char) ch);
        const bool hasCab = gt.find("cab") != std::string::npos
                         || gt.find("rig") != std::string::npos;
        modelHasCab_.store(hasCab);
    }
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
    // Called once per processBlock from the audio thread: bail out unless the
    // value actually moved. SetQualityScaleFactor() on an on-demand composite
    // model can trigger Prewarm(), which is not realtime-safe.
    if (s == qualityScale_.load()) return;
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
    modelHasCab_.store(false);
    isSlimmable_.store(false);
    hasLoudnessCached_.store(false);
    hasInputLevelCached_.store(false);
    hasOutputLevelCached_.store(false);
}
// Copia gli ultimi n campioni in ordine cronologico.
void NAMPipeline::readScope (float* dst, int n) const
{
    if (n > kScopeSize) n = kScopeSize;
    const int w = scopeWrite_.load(std::memory_order_acquire);
    int r = (w - n) & (kScopeSize - 1);
    for (int i = 0; i < n; ++i) {
        dst[i] = scope_[(size_t) r];
        r = (r + 1) & (kScopeSize - 1);
    }
}

void NAMPipeline::clearIR()    { ir_.reset(); }
void NAMPipeline::clearIR2()   { ir2_.reset(); }

bool NAMPipeline::loadIR2(const std::string& path)
{
    if (path.empty()) { clearIR2(); return false; }
    auto next = std::make_unique<nam_dsp::IRConvolver>();
    try {
        if (!next->loadFromFile(path, sampleRate_)) return false;
    } catch (...) {
        return false;
    }
    next->prepare(128, 1024);
    ir2_ = std::move(next);
    return true;
}

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
