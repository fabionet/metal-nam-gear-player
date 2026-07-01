// Stage 3 — NAMAudioProcessor: APVTS + dual NAMPipeline + async model/IR loader.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>

#include "NAMPipeline.h"

class PresetManager;

class NAMAudioProcessor : public juce::AudioProcessor
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
    void clearModel();
    void clearIR();

    juce::String getCurrentModelPath() const { return currentModelPath_; }
    juce::String getCurrentIRPath()    const { return currentIRPath_;    }

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    PresetManager& getPresetManager() { return *presetManager_; }

    // Stage 8 — meter taps (Editor reads & resets atomically at ~30 Hz).
    std::atomic<float>& getMeterInL()  noexcept { return meterInL_;  }
    std::atomic<float>& getMeterInR()  noexcept { return meterInR_;  }
    std::atomic<float>& getMeterOutL() noexcept { return meterOutL_; }
    std::atomic<float>& getMeterOutR() noexcept { return meterOutR_; }

private:
    std::atomic<float> meterInL_  { 0.f };
    std::atomic<float> meterInR_  { 0.f };
    std::atomic<float> meterOutL_ { 0.f };
    std::atomic<float> meterOutR_ { 0.f };

    // Two pipelines for 3 channel modes (mono mirror / dual-mono / stereo split).
    std::unique_ptr<NAMPipeline> pipelineL_;
    std::unique_ptr<NAMPipeline> pipelineR_;

    // Pending swap slots: worker fills, audio thread consumes.
    std::atomic<NAMPipeline*> pendingL_ { nullptr };
    std::atomic<NAMPipeline*> pendingR_ { nullptr };

    juce::ThreadPool loaderPool_ { 1 };

    juce::String currentModelPath_;
    juce::String currentIRPath_;

    double sampleRate_  = 48000.0;
    int    blockSize_   = 512;

    void pushParametersToPipelines();
    void consumePendingSwaps();

    std::unique_ptr<PresetManager> presetManager_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMAudioProcessor)
};
