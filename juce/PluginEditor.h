// Stage 3 — generic editor wrapping APVTS sliders + file pickers.
// Stage 4 will replace this with a custom amp/pedal layout.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class NAMAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NAMAudioProcessorEditor (NAMAudioProcessor&);
    ~NAMAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    NAMAudioProcessor& processorRef;

    juce::GenericAudioProcessorEditor generic;
    juce::TextButton loadModelBtn { "Load .nam..." };
    juce::TextButton loadIRBtn    { "Load IR (.wav)..." };
    juce::Label      modelLabel, irLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    void browseModel();
    void browseIR();
    void refreshLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMAudioProcessorEditor)
};
