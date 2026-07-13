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

class NAMAudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit NAMAudioProcessorEditor (NAMAudioProcessor&);
    ~NAMAudioProcessorEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

    // Polls processorRef.isCurrentModelSlimmable() at ~8 Hz and enables/disables
    // the Slim slider + the QUAL knob accordingly. Cheap and race-free.
    void timerCallback() override;

    // Small readout that shows the DSP CPU load (%). Polled at 10 Hz.
    class CpuMeterComponent : public juce::Component, private juce::Timer
    {
    public:
        explicit CpuMeterComponent (std::function<float()> getCpu)
            : getCpu_ (std::move (getCpu))
        {
            startTimerHz (10);
            setInterceptsMouseClicks (false, false);
        }
        ~CpuMeterComponent() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            const float pct = juce::jlimit (0.f, 200.f, getCpu_ ? getCpu_() : 0.f);
            juce::Colour col = juce::Colours::lime;
            if (pct > 40.f) col = juce::Colours::yellow;
            if (pct > 75.f) col = juce::Colours::orangered;
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.f);
            g.setColour (col);
            g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 12.f, juce::Font::bold));
            g.drawText ("CPU " + juce::String ((int) std::round (pct)) + "%",
                        getLocalBounds(), juce::Justification::centred);
        }
    private:
        void timerCallback() override { repaint(); }
        std::function<float()> getCpu_;
    };

private:
    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // Adapter: makes a ToggleButton represent the INVERSE of a bool
    // `xxx_bypass` parameter — checked = active, unchecked = bypass.
    struct InvertBypassBinding {
        juce::ToggleButton& btn;
        juce::ParameterAttachment att;
        InvertBypassBinding (juce::AudioProcessorValueTreeState& apvts,
                             const juce::String& paramId,
                             juce::ToggleButton& b)
          : btn (b),
            att (*apvts.getParameter (paramId),
                 [this] (float v) {
                     btn.setToggleState (v < 0.5f, juce::dontSendNotification);
                 })
        {
            btn.onClick = [this] {
                att.setValueAsCompleteGesture (btn.getToggleState() ? 0.0f : 1.0f);
            };
            att.sendInitialUpdate();
        }
    };
    using IBypass = std::unique_ptr<InvertBypassBinding>;

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

    // Slim slider under the MODEL loader row. Bound to APVTS `quality_scale`
    // (same param as the QUAL knob → the two stay in sync automatically).
    // Enabled only when the active pipeline holds an A2 slimmable model.
    juce::Slider slimSlider_;
    juce::Label  slimLabel_ { {}, "SLIM" };
    std::unique_ptr<SAtt> slimAtt_;
    // Init to `true` so the first updateSlimEnabled() call always applies the
    // real state (disabled when no slimmable model is loaded). Otherwise the
    // early-return `slim == lastSlimmable_` skips the first update and the
    // JUCE Slider stays enabled by default → visually looks active on V1.
    bool lastSlimmable_ = true;
    void updateSlimEnabled();

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
    juce::TextButton    calBtn { "CAL" };
    PresetPanelComponent presetPanel;
    bool                 panelOpen_ = false;
    void togglePresetPanel();
    void showCalibrationPopup();

    // NORMAL toggle: shortcut for Output Mode Raw <-> Normalized.
    juce::TextButton normalToggle_ { "NORMAL" };
    std::unique_ptr<juce::ParameterAttachment> normalAtt_;

    // Options button + meters (Stage 8).
    juce::TextButton optionsBtn { "Meeter OPT" };
    std::unique_ptr<MeterStripComponent> inMeter_, outMeter_;
    void showOptionsMenu();

    // Zoom selector (Stage 11) — header button, popup menu with 25/50/75/100/150/200%.
    juce::TextButton zoomBtn { "Zoom" };
    juce::TextButton osBtn { "OS 2x" };
    juce::TextButton infoBtn { juce::CharPointer_UTF8 ("i") };
    void showZoomMenu();
    void applyUiScale (int percent);
    void showInfoPopup();

    // CPU load readout, rightmost widget of the header bar.
    CpuMeterComponent cpuMeter_ { [this] { return processorRef.getCpuLoadPct(); } };

    // Knobs (indexed by param id).
    std::vector<std::unique_ptr<KnobBox>> knobs_;

    // Bypass toggles.
    juce::ToggleButton ampBypass  { "AMP" };
    juce::ToggleButton irBypass   { "CAB" };
    juce::ToggleButton ngBypass   { "NG" };
    juce::ToggleButton gateBypass { "GATE" };
    juce::ToggleButton odBypass   { "OD" };
    juce::ToggleButton distBypass { "DIST" };
    juce::ToggleButton eqBypass   { "EQ" };
    juce::ToggleButton hpBypass   { "HP" };
    juce::ToggleButton lnEnabled  { "LN" };
    juce::ToggleButton delBypass  { "DELAY" };
    juce::ToggleButton chBypass   { "CHOR" };
    juce::ToggleButton flBypass   { "FLAN" };
    juce::ToggleButton rvBypass   { "REV" };
    juce::ToggleButton trBypass   { "TREM" };
    juce::ToggleButton irHpBypass { "iHP" };
    juce::ToggleButton irLpBypass { "iLP" };
    juce::ToggleButton irPhaseInv { juce::CharPointer_UTF8 ("\xcf\x86") };
    IBypass ampBypassAtt, irBypassAtt, ngBypassAtt, gateBypassAtt, odBypassAtt, distBypassAtt;
    IBypass hpBypassAtt, delBypassAtt, chBypassAtt, flBypassAtt, eqBypassAtt, rvBypassAtt, trBypassAtt;
    IBypass irHpBypassAtt, irLpBypassAtt;
    std::unique_ptr<BAtt> lnEnabledAtt;
    std::unique_ptr<BAtt> irPhaseInvAtt; // non-inverted: bool param is truthy=active // LN uses `ln_enabled` (already active-semantics), keep direct.

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
