// Stage 5 — Marshall-style metal-plate UI with pre-FX (gate/OD/dist).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "NAMLookAndFeel.h"
#include "PresetPanelComponent.h"
#include "MeterStripComponent.h"

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

    // File loader strip (below footer): ◀ [ComboBox ▾] ▶ [Browse]  for NAM and IR.
    juce::TextButton modelPrevBtn   { "<" };
    juce::TextButton modelNextBtn   { ">" };
    juce::TextButton modelBrowseBtn { "Browse" };
    juce::ComboBox   modelCombo;
    juce::Label      modelTitleLabel { {}, "MODEL" };

    juce::TextButton irPrevBtn      { "<" };
    juce::TextButton irNextBtn      { ">" };
    juce::TextButton irBrowseBtn    { "Browse" };
    juce::ComboBox   irCombo;
    juce::Label      irTitleLabel   { {}, "IR" };

    juce::File       modelDir_;
    juce::File       irDir_;
    juce::StringArray modelFiles_;
    juce::StringArray irFiles_;

    // Preset panel + toggle.
    juce::TextButton    presetsToggleBtn { "PRESETS" };
    PresetPanelComponent presetPanel;
    bool                 panelOpen_ = false;
    void togglePresetPanel();

    // Options button + meters (Stage 8).
    juce::TextButton optionsBtn { "Meeter OPT" };
    std::unique_ptr<MeterStripComponent> inMeter_, outMeter_;
    void showOptionsMenu();

    // Zoom selector (Stage 11) — header button, popup menu with 25/50/75/100/150/200%.
    juce::TextButton zoomBtn { "Zoom" };
    void showZoomMenu();
    void applyUiScale (int percent);

    // Knobs (indexed by param id).
    std::vector<std::unique_ptr<KnobBox>> knobs_;

    // Bypass toggles.
    juce::ToggleButton ampBypass  { "AMP" };
    juce::ToggleButton irBypass   { "CAB" };
    juce::ToggleButton ngBypass   { "NG" };
    juce::ToggleButton gateBypass { "GATE" };
    juce::ToggleButton odBypass   { "OD" };
    juce::ToggleButton distBypass { "DIST" };
    juce::ToggleButton hpBypass   { "HP" };
    juce::ToggleButton lnEnabled  { "LN" };
    juce::ToggleButton delBypass  { "DELAY" };
    juce::ToggleButton chBypass   { "CHOR" };
    juce::ToggleButton flBypass   { "FLAN" };
    std::unique_ptr<BAtt> ampBypassAtt, irBypassAtt, ngBypassAtt, gateBypassAtt, odBypassAtt, distBypassAtt;
    std::unique_ptr<BAtt> hpBypassAtt, lnEnabledAtt, delBypassAtt, chBypassAtt, flBypassAtt;

    // Tab switcher (MAIN / FX).
    juce::TextButton mainTabBtn { "MAIN" };
    juce::TextButton fxTabBtn   { "FX" };
    enum class Tab { Main, Fx };
    Tab activeTab_ = Tab::Main;
    void setActiveTab (Tab t);

    // Mode.
    juce::ComboBox modeBox;
    juce::Label    modeLabel { {}, "MODE" };
    std::unique_ptr<CAtt> modeAtt;

    std::unique_ptr<juce::FileChooser> chooser;

    KnobBox& addKnob (const juce::String& paramId, const juce::String& name);
    void browseModel();
    void browseIR();
    void refreshLabels();
    void rescanModelDir (const juce::File& sel);
    void rescanIRDir    (const juce::File& sel);
    void stepCombo (juce::ComboBox& cb, int delta);

    // Layout area for the loader strip (below footer).
    juce::Rectangle<int> loaderArea_;

    // Painting helpers.
    void paintMetalBackground (juce::Graphics&);
    void paintTitle           (juce::Graphics&, juce::Rectangle<int>);
    void paintScrew           (juce::Graphics&, juce::Point<float> centre, float radius);
    void paintGroupPanel      (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    // Layout state for paint() to read.
    juce::Rectangle<int> titleArea_, headerArea_, panelArea_, footerArea_;
    juce::Rectangle<int> inMeterArea_, outMeterArea_;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> groupPanels_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMAudioProcessorEditor)
};
