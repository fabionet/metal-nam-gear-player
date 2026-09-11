// Stage 3 — NAMAudioProcessor: APVTS + dual NAMPipeline + async model/IR loader.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>

#include "NAMPipeline.h"

class PresetManager;

class NAMAudioProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    NAMAudioProcessor();
    ~NAMAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NAM Custom"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- File loading (called from UI thread, runs on ThreadPool) ---
    void loadModelAsync (const juce::File& f);
    void loadIRAsync    (const juce::File& f);
    void loadIR2Async   (const juce::File& f);
    void clearModel();
    void clearIR();
    void clearIR2();

    juce::String getCurrentModelPath() const { return currentModelPath_; }
    juce::String getCurrentIRPath()    const { return currentIRPath_;    }
    juce::String getCurrentIR2Path()   const { return currentIR2Path_;   }

    // True iff the currently active pipeline holds a NAM model that exposes
    // A2 quality scaling (aka "slimmable"). Editor polls this to enable/disable
    // the Quality knob and the new Slim slider under the loader.
    bool isCurrentModelSlimmable() const noexcept
    {
        return pipelineL_ && pipelineL_->isSlimmable();
    }

    // Read-only accessor for the L pipeline (used by the Calibration popup
    // to show cached model metadata). L is always populated when a model
    // is loaded.
    const NAMPipeline& pipelineL() const noexcept { return *pipelineL_; }

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    PresetManager& getPresetManager() { return *presetManager_; }

    // Stage 8 — meter taps (Editor reads & resets atomically at ~30 Hz).
    std::atomic<float>& getMeterInL()  noexcept { return meterInL_;  }
    std::atomic<float>& getMeterInR()  noexcept { return meterInR_;  }
    std::atomic<float>& getMeterOutL() noexcept { return meterOutL_; }
    std::atomic<float>& getMeterOutR() noexcept { return meterOutR_; }
    std::atomic<float>& getMeterModelL() noexcept { return meterModelL_; }
    std::atomic<float>& getMeterModelR() noexcept { return meterModelR_; }
    std::atomic<float>& getMeterIr1L() noexcept { return meterIr1L_; }
    std::atomic<float>& getMeterIr1R() noexcept { return meterIr1R_; }
    std::atomic<float>& getMeterIr2L() noexcept { return meterIr2L_; }
    std::atomic<float>& getMeterIr2R() noexcept { return meterIr2R_; }

    // Esito dell'ultimo caricamento asincrono. Serve all'editor per dirlo
    // all'utente: prima un file mancante veniva scartato in silenzio e la UI
    // restava identica a "nessun modello", senza modo di distinguere i casi.
    enum class LoadStatus { None, Ok, FileMissing, LoadFailed };
    LoadStatus modelLoadStatus() const noexcept { return modelStatus_.load(); }
    LoadStatus irLoadStatus()    const noexcept { return irStatus_.load(); }
    LoadStatus ir2LoadStatus()   const noexcept { return ir2Status_.load(); }
    juce::String lastModelName() const { return lastModelName_; }
    juce::String lastIRName()    const { return lastIRName_; }
    juce::String lastIR2Name()   const { return lastIR2Name_; }

    // CPU load %, updated at every processBlock (EMA).
    float getCpuLoadPct() const noexcept { return cpuLoad_.load (std::memory_order_relaxed); }

    // Compressor gain-reduction (dB, <= 0), updated at every processBlock.
    float getCompGrDb() const noexcept { return compGr_.load (std::memory_order_relaxed); }

    // Oversampling (session-local; not APVTS).
    void setOversamplingEnabled (bool on);
    bool isOversamplingEnabled() const noexcept { return oversamplingOn_.load(); }

private:
    std::atomic<float> meterInL_  { 0.f };
    std::atomic<float> meterInR_  { 0.f };
    std::atomic<float> meterOutL_ { 0.f };
    std::atomic<float> meterOutR_ { 0.f };
    std::atomic<float> meterModelL_ { 0.f };
    std::atomic<float> meterModelR_ { 0.f };
    std::atomic<float> meterIr1L_ { 0.f }, meterIr1R_ { 0.f };
    std::atomic<float> meterIr2L_ { 0.f }, meterIr2R_ { 0.f };
    std::atomic<LoadStatus> modelStatus_ { LoadStatus::None };
    std::atomic<LoadStatus> irStatus_    { LoadStatus::None };
    std::atomic<LoadStatus> ir2Status_   { LoadStatus::None };
    juce::String lastModelName_, lastIRName_, lastIR2Name_;

    // Accende automaticamente il secondo IR passando a Dual-Mono o Stereo.
    // Tornando in Mono non lo spegne: li' resta una scelta manuale.
    void parameterChanged (const juce::String& id, float value) override;
    std::atomic<float> cpuLoad_   { 0.f };
    std::atomic<float> compGr_    { 0.f };

    // Two pipelines for 3 channel modes (mono mirror / dual-mono / stereo split).
    std::unique_ptr<NAMPipeline> pipelineL_;
    std::unique_ptr<NAMPipeline> pipelineR_;

    // Pending swap slots: worker fills, audio thread consumes.
    std::atomic<NAMPipeline*> pendingL_ { nullptr };
    std::atomic<NAMPipeline*> pendingR_ { nullptr };

    // Shutdown latch: loader jobs check this before touching pendingL_/R_
    // so a job that outlives ~NAMAudioProcessor cannot write into a
    // destroyed atomic. Set to true at the top of the destructor before
    // draining the pool. See PluginProcessor.cpp ~ctor / loader lambdas.
    std::atomic<bool> shuttingDown_ { false };

    juce::ThreadPool loaderPool_ { 1 };

    juce::String currentModelPath_;
    juce::String currentIRPath_;
    juce::String currentIR2Path_;

    double sampleRate_  = 48000.0;
    int    blockSize_   = 512;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler_;
    std::atomic<bool> oversamplingOn_ { false };
    std::atomic<bool> oversamplingRequested_ { false };
    double baseSampleRate_ = 48000.0;
    int    baseBlockSize_  = 512;
    bool   oversamplingPrepared_ = false;

    void pushParametersToPipelines();
    void consumePendingSwaps();

    std::unique_ptr<PresetManager> presetManager_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMAudioProcessor)
};
