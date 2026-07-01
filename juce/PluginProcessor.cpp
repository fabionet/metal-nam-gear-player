#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetManager.h"

namespace ids {
    constexpr auto inputLevel    = "input_level";
    constexpr auto outputLevel   = "output_level";
    constexpr auto eqBass        = "eq_bass";
    constexpr auto eqMidFreq     = "eq_mid_freq";
    constexpr auto eqMidQ        = "eq_mid_q";
    constexpr auto eqMidGain     = "eq_mid_gain";
    constexpr auto eqPresence    = "eq_presence";
    constexpr auto eqTreble      = "eq_treble";
    constexpr auto eqAir         = "eq_air";
    constexpr auto eqBypass      = "eq_bypass";
    constexpr auto depth         = "depth";
    constexpr auto resonance     = "resonance";
    constexpr auto resonanceFreq = "resonance_freq";
    constexpr auto channelMode   = "channel_mode";
    constexpr auto qualityScale  = "quality_scale";
    constexpr auto irMix         = "ir_mix";
    constexpr auto irBypass      = "ir_bypass";
    constexpr auto modelBypass   = "model_bypass";
    // Pre-FX
    constexpr auto gateThresh    = "gate_threshold";
    constexpr auto gateRelease   = "gate_release";
    constexpr auto gateBypass    = "gate_bypass";
    constexpr auto odDrive       = "od_drive";
    constexpr auto odTone        = "od_tone";
    constexpr auto odLevel       = "od_level";
    constexpr auto odBypass      = "od_bypass";
    constexpr auto distDrive     = "dist_drive";
    constexpr auto distTone      = "dist_tone";
    constexpr auto distLevel     = "dist_level";
    constexpr auto distBypass    = "dist_bypass";
    constexpr auto hpFreq        = "hp_freq";
    constexpr auto hpBypass      = "hp_bypass";
    constexpr auto lnEnabled     = "ln_enabled";
    constexpr auto lnTargetDB    = "ln_target_db";
    // NoiseGate (pre-chain)
    constexpr auto ngThresh      = "ng_threshold";
    constexpr auto ngRelease     = "ng_release";
    constexpr auto ngBypass      = "ng_bypass";
    // Post-cab FX
    constexpr auto delTime       = "delay_time_ms";
    constexpr auto delFb         = "delay_feedback";
    constexpr auto delMix        = "delay_mix";
    constexpr auto delBypass     = "delay_bypass";
    constexpr auto chRate        = "chorus_rate_hz";
    constexpr auto chDepth       = "chorus_depth";
    constexpr auto chMix         = "chorus_mix";
    constexpr auto chBypass      = "chorus_bypass";
    constexpr auto flRate        = "flanger_rate_hz";
    constexpr auto flDepth       = "flanger_depth";
    constexpr auto flFb          = "flanger_feedback";
    constexpr auto flMix         = "flanger_mix";
    constexpr auto flBypass      = "flanger_bypass";
}

juce::AudioProcessorValueTreeState::ParameterLayout NAMAudioProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&](auto&& p) { layout.add (std::move (p)); };

    add (std::make_unique<P>(juce::ParameterID{ids::inputLevel,1},    "Input",    juce::NormalisableRange<float>(-20.f, 20.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::outputLevel,1},   "Output",   juce::NormalisableRange<float>(-20.f, 20.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqBass,1},        "Bass",     juce::NormalisableRange<float>(-15.f, 15.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqMidFreq,1},     "Mid Freq", juce::NormalisableRange<float>(200.f, 2000.f, 1.f, 0.3f), 700.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqMidQ,1},        "Mid Q",    juce::NormalisableRange<float>(0.3f, 3.0f, 0.01f), 0.707f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqMidGain,1},     "Mid",      juce::NormalisableRange<float>(-15.f, 15.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqPresence,1},    "Presence", juce::NormalisableRange<float>(-15.f, 15.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqTreble,1},      "Treble",   juce::NormalisableRange<float>(-15.f, 15.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqAir,1},         "Air",      juce::NormalisableRange<float>(-15.f, 15.f, 0.01f), 0.f));
    add (std::make_unique<B>(juce::ParameterID{ids::eqBypass,1},      "EQ Bypass", false));
    add (std::make_unique<P>(juce::ParameterID{ids::depth,1},         "Depth",    juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::resonance,1},     "Resonance",juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::resonanceFreq,1}, "Res Freq", juce::NormalisableRange<float>(60.f, 250.f, 1.f, 0.5f), 100.f));
    add (std::make_unique<C>(juce::ParameterID{ids::channelMode,1},   "Mode",     juce::StringArray{"Mono","Dual-Mono","Stereo"}, 0));
    add (std::make_unique<P>(juce::ParameterID{ids::qualityScale,1},  "Quality",  juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));
    add (std::make_unique<P>(juce::ParameterID{ids::irMix,1},         "IR Mix",   juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));
    add (std::make_unique<B>(juce::ParameterID{ids::irBypass,1},      "IR Bypass",    false));
    add (std::make_unique<B>(juce::ParameterID{ids::modelBypass,1},   "Amp Bypass",   false));

    // Pre-FX
    add (std::make_unique<P>(juce::ParameterID{ids::gateThresh,1},  "Gate Threshold", juce::NormalisableRange<float>(-80.f, 0.f, 0.1f), -60.f));
    add (std::make_unique<P>(juce::ParameterID{ids::gateRelease,1}, "Gate Release",   juce::NormalisableRange<float>(10.f, 500.f, 1.f), 80.f));
    add (std::make_unique<B>(juce::ParameterID{ids::gateBypass,1},  "Gate Bypass",    false));
    add (std::make_unique<P>(juce::ParameterID{ids::odDrive,1},     "OD Drive",       juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.3f));
    add (std::make_unique<P>(juce::ParameterID{ids::odTone,1},      "OD Tone",        juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::odLevel,1},     "OD Level",       juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<B>(juce::ParameterID{ids::odBypass,1},    "OD Bypass",      true));
    add (std::make_unique<P>(juce::ParameterID{ids::distDrive,1},   "Dist Drive",     juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.4f));
    add (std::make_unique<P>(juce::ParameterID{ids::distTone,1},    "Dist Tone",      juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::distLevel,1},   "Dist Level",     juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<B>(juce::ParameterID{ids::distBypass,1},  "Dist Bypass",    true));

    // Master post-cab
    add (std::make_unique<P>(juce::ParameterID{ids::hpFreq,1},    "HP Freq",   juce::NormalisableRange<float>(10.f, 200.f, 0.5f, 0.5f), 30.f));
    add (std::make_unique<B>(juce::ParameterID{ids::hpBypass,1},  "HP Bypass", false));
    add (std::make_unique<B>(juce::ParameterID{ids::lnEnabled,1}, "LN On",     false));
    add (std::make_unique<P>(juce::ParameterID{ids::lnTargetDB,1},"LN Target", juce::NormalisableRange<float>(-30.f, -6.f, 0.1f), -18.f));

    // NoiseGate (pre-chain)
    add (std::make_unique<P>(juce::ParameterID{ids::ngThresh,1},  "NG Threshold", juce::NormalisableRange<float>(-80.f, 0.f, 0.1f), -55.f));
    add (std::make_unique<P>(juce::ParameterID{ids::ngRelease,1}, "NG Release",   juce::NormalisableRange<float>(5.f, 500.f, 1.f), 80.f));
    add (std::make_unique<B>(juce::ParameterID{ids::ngBypass,1},  "NG Bypass",    false));

    // Delay
    add (std::make_unique<P>(juce::ParameterID{ids::delTime,1},   "Delay Time",     juce::NormalisableRange<float>(10.f, 2000.f, 1.f, 0.5f), 350.f));
    add (std::make_unique<P>(juce::ParameterID{ids::delFb,1},     "Delay Feedback", juce::NormalisableRange<float>(0.f, 0.9f, 0.001f), 0.35f));
    add (std::make_unique<P>(juce::ParameterID{ids::delMix,1},    "Delay Mix",      juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.25f));
    add (std::make_unique<B>(juce::ParameterID{ids::delBypass,1}, "Delay Bypass",   true));

    // Chorus
    add (std::make_unique<P>(juce::ParameterID{ids::chRate,1},   "Chorus Rate",  juce::NormalisableRange<float>(0.05f, 6.f, 0.001f, 0.5f), 0.8f));
    add (std::make_unique<P>(juce::ParameterID{ids::chDepth,1},  "Chorus Depth", juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.4f));
    add (std::make_unique<P>(juce::ParameterID{ids::chMix,1},    "Chorus Mix",   juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.3f));
    add (std::make_unique<B>(juce::ParameterID{ids::chBypass,1}, "Chorus Bypass",true));

    // Flanger
    add (std::make_unique<P>(juce::ParameterID{ids::flRate,1},   "Flanger Rate",     juce::NormalisableRange<float>(0.05f, 6.f, 0.001f, 0.5f), 0.3f));
    add (std::make_unique<P>(juce::ParameterID{ids::flDepth,1},  "Flanger Depth",    juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    add (std::make_unique<P>(juce::ParameterID{ids::flFb,1},     "Flanger Feedback", juce::NormalisableRange<float>(0.f, 0.9f, 0.001f), 0.4f));
    add (std::make_unique<P>(juce::ParameterID{ids::flMix,1},    "Flanger Mix",      juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.25f));
    add (std::make_unique<B>(juce::ParameterID{ids::flBypass,1}, "Flanger Bypass",   true));
    return layout;
}

NAMAudioProcessor::NAMAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pipelineL_ = std::make_unique<NAMPipeline>();
    pipelineR_ = std::make_unique<NAMPipeline>();
    presetManager_ = std::make_unique<PresetManager>(*this, apvts);
}

NAMAudioProcessor::~NAMAudioProcessor()
{
    loaderPool_.removeAllJobs (true, 2000);
    if (auto* p = pendingL_.exchange (nullptr)) delete p;
    if (auto* p = pendingR_.exchange (nullptr)) delete p;
}

void NAMAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate_ = sr;
    blockSize_  = samplesPerBlock;
    pipelineL_->prepare (sr, samplesPerBlock);
    pipelineR_->prepare (sr, samplesPerBlock);
    pipelineL_->reset();
    pipelineR_->reset();
}

bool NAMAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void NAMAudioProcessor::pushParametersToPipelines()
{
    const float in_ = apvts.getRawParameterValue (ids::inputLevel)->load();
    const float out_ = apvts.getRawParameterValue (ids::outputLevel)->load();
    float bass = apvts.getRawParameterValue (ids::eqBass)->load();
    const float midF = apvts.getRawParameterValue (ids::eqMidFreq)->load();
    const float midQ = apvts.getRawParameterValue (ids::eqMidQ)->load();
    float midG = apvts.getRawParameterValue (ids::eqMidGain)->load();
    float pres = apvts.getRawParameterValue (ids::eqPresence)->load();
    float treb = apvts.getRawParameterValue (ids::eqTreble)->load();
    float air  = apvts.getRawParameterValue (ids::eqAir)->load();
    const bool  eqBp = apvts.getRawParameterValue (ids::eqBypass)->load() > 0.5f;
    if (eqBp) { bass = midG = pres = treb = air = 0.f; }
    const float dep  = apvts.getRawParameterValue (ids::depth)->load();
    const float res  = apvts.getRawParameterValue (ids::resonance)->load();
    const float resF = apvts.getRawParameterValue (ids::resonanceFreq)->load();
    const float qual = apvts.getRawParameterValue (ids::qualityScale)->load();
    const float mix  = apvts.getRawParameterValue (ids::irMix)->load();
    const bool  irBp = apvts.getRawParameterValue (ids::irBypass)->load() > 0.5f;
    const bool  mdBp = apvts.getRawParameterValue (ids::modelBypass)->load() > 0.5f;

    const float gT  = apvts.getRawParameterValue (ids::gateThresh)->load();
    const float gR  = apvts.getRawParameterValue (ids::gateRelease)->load();
    const bool  gBp = apvts.getRawParameterValue (ids::gateBypass)->load() > 0.5f;
    const float odD = apvts.getRawParameterValue (ids::odDrive)->load();
    const float odT = apvts.getRawParameterValue (ids::odTone)->load();
    const float odL = apvts.getRawParameterValue (ids::odLevel)->load();
    const bool  odBp= apvts.getRawParameterValue (ids::odBypass)->load() > 0.5f;
    const float dsD = apvts.getRawParameterValue (ids::distDrive)->load();
    const float dsT = apvts.getRawParameterValue (ids::distTone)->load();
    const float dsL = apvts.getRawParameterValue (ids::distLevel)->load();
    const bool  dsBp= apvts.getRawParameterValue (ids::distBypass)->load() > 0.5f;
    const float hpF = apvts.getRawParameterValue (ids::hpFreq)->load();
    const bool  hpBp= apvts.getRawParameterValue (ids::hpBypass)->load() > 0.5f;
    const bool  lnOn= apvts.getRawParameterValue (ids::lnEnabled)->load() > 0.5f;
    const float lnT = apvts.getRawParameterValue (ids::lnTargetDB)->load();

    const float ngT  = apvts.getRawParameterValue (ids::ngThresh)->load();
    const float ngR  = apvts.getRawParameterValue (ids::ngRelease)->load();
    const bool  ngBp = apvts.getRawParameterValue (ids::ngBypass)->load() > 0.5f;
    const float dT  = apvts.getRawParameterValue (ids::delTime)->load();
    const float dFb = apvts.getRawParameterValue (ids::delFb)->load();
    const float dMx = apvts.getRawParameterValue (ids::delMix)->load();
    const bool  dBp = apvts.getRawParameterValue (ids::delBypass)->load() > 0.5f;
    const float cR  = apvts.getRawParameterValue (ids::chRate)->load();
    const float cD  = apvts.getRawParameterValue (ids::chDepth)->load();
    const float cMx = apvts.getRawParameterValue (ids::chMix)->load();
    const bool  cBp = apvts.getRawParameterValue (ids::chBypass)->load() > 0.5f;
    const float fR  = apvts.getRawParameterValue (ids::flRate)->load();
    const float fD  = apvts.getRawParameterValue (ids::flDepth)->load();
    const float fFb = apvts.getRawParameterValue (ids::flFb)->load();
    const float fMx = apvts.getRawParameterValue (ids::flMix)->load();
    const bool  fBp = apvts.getRawParameterValue (ids::flBypass)->load() > 0.5f;

    auto apply = [&](NAMPipeline& p) {
        p.setInputGainDB  (in_);
        p.setOutputGainDB (out_);
        p.setEqBass (bass);
        p.setEqMid  (midF, midQ, midG);
        p.setEqPresence (pres);
        p.setEqTreble (treb);
        p.setEqAir (air);
        p.setDepth (dep);
        p.setResonance (res, resF);
        p.setQualityScale (qual);
        p.setIrMix (mix);
        p.setIrBypass (irBp);
        p.setModelBypass (mdBp);
        p.setGate (gT, gR, gBp);
        p.setOverdrive (odD, odT, odL, odBp);
        p.setDistortion (dsD, dsT, dsL, dsBp);
        p.setHighPass (hpF, hpBp);
        p.setLoudnessNorm (lnOn, lnT);
        p.setNoiseGate (ngT, ngR, ngBp);
        p.setDelay     (dT, dFb, dMx, dBp);
        p.setChorus    (cR, cD, cMx, cBp);
        p.setFlanger   (fR, fD, fFb, fMx, fBp);
    };
    apply (*pipelineL_);
    apply (*pipelineR_);
}

void NAMAudioProcessor::consumePendingSwaps()
{
    if (auto* p = pendingL_.exchange (nullptr)) {
        p->prepare (sampleRate_, blockSize_);
        pipelineL_.reset (p);
    }
    if (auto* p = pendingR_.exchange (nullptr)) {
        p->prepare (sampleRate_, blockSize_);
        pipelineR_.reset (p);
    }
}

void NAMAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    consumePendingSwaps();
    pushParametersToPipelines();

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    const int mode = (int) apvts.getRawParameterValue (ids::channelMode)->load();

    float* L = buffer.getWritePointer (0);
    float* R = (numCh > 1) ? buffer.getWritePointer (1) : nullptr;

    auto sampleAbsPeak = [] (std::atomic<float>& dst, const float* p, int N) {
        auto r = juce::FloatVectorOperations::findMinAndMax (p, N);
        const float peak = juce::jmax (std::abs (r.getStart()), std::abs (r.getEnd()));
        float cur = dst.load (std::memory_order_relaxed);
        while (peak > cur && ! dst.compare_exchange_weak (cur, peak, std::memory_order_relaxed)) {}
    };

    // Input meter taps (pre-DSP).
    sampleAbsPeak (meterInL_, L, n);
    if (R) sampleAbsPeak (meterInR_, R, n);

    if (numCh == 1 || mode == 0) {
        // Mono: process L, mirror to R.
        pipelineL_->process (L, L, n);
        if (R) std::memcpy (R, L, sizeof(float) * n);
    } else if (mode == 1) {
        // Dual-Mono: duplicate L into both pipelines.
        if (R) std::memcpy (R, L, sizeof(float) * n);
        pipelineL_->process (L, L, n);
        pipelineR_->process (R, R, n);
    } else {
        // Stereo split.
        pipelineL_->process (L, L, n);
        if (R) pipelineR_->process (R, R, n);
    }

    // Output meter taps (post-DSP).
    sampleAbsPeak (meterOutL_, L, n);
    if (R) sampleAbsPeak (meterOutR_, R, n);
}

juce::AudioProcessorEditor* NAMAudioProcessor::createEditor()
{
    return new NAMAudioProcessorEditor (*this);
}

// --- File loading -----------------------------------------------------------

void NAMAudioProcessor::loadModelAsync (const juce::File& f)
{
    if (! f.existsAsFile()) { clearModel(); return; }
    currentModelPath_ = f.getFullPathName();
    const auto path = currentModelPath_.toStdString();
    const double sr = sampleRate_;
    const int    bs = blockSize_;
    const std::string irPath = currentIRPath_.toStdString();

    loaderPool_.addJob ([this, path, sr, bs, irPath]() {
        auto buildOne = [&]() -> NAMPipeline* {
            auto pl = std::make_unique<NAMPipeline>();
            pl->prepare (sr, bs);
            if (! pl->loadModel (path)) return nullptr;
            if (! irPath.empty()) pl->loadIR (irPath);
            return pl.release();
        };
        if (auto* a = buildOne()) if (auto* old = pendingL_.exchange (a)) delete old;
        if (auto* b = buildOne()) if (auto* old = pendingR_.exchange (b)) delete old;
    });
}

void NAMAudioProcessor::loadIRAsync (const juce::File& f)
{
    if (! f.existsAsFile()) { clearIR(); return; }
    currentIRPath_ = f.getFullPathName();
    const auto path = currentIRPath_.toStdString();
    const double sr = sampleRate_;
    const int    bs = blockSize_;
    const std::string modelPath = currentModelPath_.toStdString();

    loaderPool_.addJob ([this, path, sr, bs, modelPath]() {
        auto buildOne = [&]() -> NAMPipeline* {
            auto pl = std::make_unique<NAMPipeline>();
            pl->prepare (sr, bs);
            if (! modelPath.empty()) pl->loadModel (modelPath);
            if (! pl->loadIR (path)) return nullptr;
            return pl.release();
        };
        if (auto* a = buildOne()) if (auto* old = pendingL_.exchange (a)) delete old;
        if (auto* b = buildOne()) if (auto* old = pendingR_.exchange (b)) delete old;
    });
}

void NAMAudioProcessor::clearModel()
{
    currentModelPath_ = {};
    pipelineL_->clearModel();
    pipelineR_->clearModel();
}

void NAMAudioProcessor::clearIR()
{
    currentIRPath_ = {};
    pipelineL_->clearIR();
    pipelineR_->clearIR();
}

// --- State ------------------------------------------------------------------

void NAMAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("modelPath", currentModelPath_, nullptr);
    state.setProperty ("irPath",    currentIRPath_,    nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NAMAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    apvts.replaceState (state);
    const auto mp = state.getProperty ("modelPath").toString();
    const auto ip = state.getProperty ("irPath").toString();
    if (mp.isNotEmpty()) loadModelAsync (juce::File (mp));
    if (ip.isNotEmpty()) loadIRAsync    (juce::File (ip));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NAMAudioProcessor();
}
