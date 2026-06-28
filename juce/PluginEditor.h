// Stage 5 — Marshall-style metal-plate UI with pre-FX (gate/OD/dist).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "NAMLookAndFeel.h"

class NAMAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NAMAudioProcessorEditor (NAMAudioProcessor&);
    ~NAMAudioProcessorEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct KnobBox {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SAtt> att;
    };

    NAMAudioProcessor& processorRef;
    NAMLookAndFeel     lnf_;

    // Header file loaders.
    juce::TextButton loadModelBtn { "LOAD .NAM" };
    juce::TextButton loadIRBtn    { "LOAD IR" };
    juce::Label      modelLabel, irLabel;

    // Knobs (indexed by param id).
    std::vector<std::unique_ptr<KnobBox>> knobs_;

    // Bypass toggles.
    juce::ToggleButton ampBypass  { "AMP" };
    juce::ToggleButton irBypass   { "CAB" };
    juce::ToggleButton gateBypass { "GATE" };
    juce::ToggleButton odBypass   { "OD" };
    juce::ToggleButton distBypass { "DIST" };
    juce::ToggleButton hpBypass   { "HP" };
    juce::ToggleButton lnEnabled  { "LN" };
    std::unique_ptr<BAtt> ampBypassAtt, irBypassAtt, gateBypassAtt, odBypassAtt, distBypassAtt;
    std::unique_ptr<BAtt> hpBypassAtt, lnEnabledAtt;

    // Mode.
    juce::ComboBox modeBox;
    juce::Label    modeLabel { {}, "MODE" };
    std::unique_ptr<CAtt> modeAtt;

    std::unique_ptr<juce::FileChooser> chooser;

    KnobBox& addKnob (const juce::String& paramId, const juce::String& name);
    void browseModel();
    void browseIR();
    void refreshLabels();

    // Painting helpers.
    void paintMetalBackground (juce::Graphics&);
    void paintTitle           (juce::Graphics&, juce::Rectangle<int>);
    void paintScrew           (juce::Graphics&, juce::Point<float> centre, float radius);
    void paintGroupPanel      (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    // Layout state for paint() to read.
    juce::Rectangle<int> titleArea_, headerArea_, panelArea_, footerArea_;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> groupPanels_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMAudioProcessorEditor)
};
