#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    constexpr auto depth         = "depth";
    constexpr auto resonance     = "resonance";
    constexpr auto resonanceFreq = "resonance_freq";
    constexpr auto channelMode   = "channel_mode";
    constexpr auto qualityScale  = "quality_scale";
    constexpr auto irMix         = "ir_mix";
    constexpr auto irBypass      = "ir_bypass";
    constexpr auto modelBypass   = "model_bypass";
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
    add (std::make_unique<P>(juce::ParameterID{ids::depth,1},         "Depth",    juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::resonance,1},     "Resonance",juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::resonanceFreq,1}, "Res Freq", juce::NormalisableRange<float>(60.f, 250.f, 1.f, 0.5f), 100.f));
    add (std::make_unique<C>(juce::ParameterID{ids::channelMode,1},   "Mode",     juce::StringArray{"Mono","Dual-Mono","Stereo"}, 0));
    add (std::make_unique<P>(juce::ParameterID{ids::qualityScale,1},  "Quality",  juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));
    add (std::make_unique<P>(juce::ParameterID{ids::irMix,1},         "IR Mix",   juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));
    add (std::make_unique<B>(juce::ParameterID{ids::irBypass,1},      "IR Bypass",    false));
    add (std::make_unique<B>(juce::ParameterID{ids::modelBypass,1},   "Amp Bypass",   false));
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
    const float bass = apvts.getRawParameterValue (ids::eqBass)->load();
    const float midF = apvts.getRawParameterValue (ids::eqMidFreq)->load();
    const float midQ = apvts.getRawParameterValue (ids::eqMidQ)->load();
    const float midG = apvts.getRawParameterValue (ids::eqMidGain)->load();
    const float pres = apvts.getRawParameterValue (ids::eqPresence)->load();
    const float treb = apvts.getRawParameterValue (ids::eqTreble)->load();
    const float air  = apvts.getRawParameterValue (ids::eqAir)->load();
    const float dep  = apvts.getRawParameterValue (ids::depth)->load();
    const float res  = apvts.getRawParameterValue (ids::resonance)->load();
    const float resF = apvts.getRawParameterValue (ids::resonanceFreq)->load();
    const float qual = apvts.getRawParameterValue (ids::qualityScale)->load();
    const float mix  = apvts.getRawParameterValue (ids::irMix)->load();
    const bool  irBp = apvts.getRawParameterValue (ids::irBypass)->load() > 0.5f;
    const bool  mdBp = apvts.getRawParameterValue (ids::modelBypass)->load() > 0.5f;

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
