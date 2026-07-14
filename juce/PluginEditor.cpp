#include "PluginEditor.h"
#include "GlobalSettings.h"
#include <array>
#include <BinaryData.h>

namespace {
    juce::Typeface::Ptr pirataTypeface()
    {
        static auto tf = juce::Typeface::createSystemTypefaceFor (
            BinaryData::PirataOneRegular_ttf, BinaryData::PirataOneRegular_ttfSize);
        return tf;
    }
    juce::Typeface::Ptr metalManiaTypeface()
    {
        static auto tf = juce::Typeface::createSystemTypefaceFor (
            BinaryData::MetalManiaRegular_ttf, BinaryData::MetalManiaRegular_ttfSize);
        return tf;
    }
}

namespace {
    // Indices into knobs_ vector — must match the order of addKnob calls below.
    enum K {
        kNgThresh, kNgRelease,
        kGateThresh, kGateRelease,
        kOdDrive, kOdTone, kOdLevel,
        kDistDrive, kDistTone, kDistLevel,
        kInput, kOutput,
        kBass, kMidFreq, kMidQ, kMidGain, kTreble, kPres, kAir,
        kDepth, kRes, kResFreq,
        kIrMix, kQuality,
        kHpFreq, kLnTarget,
        kDelTime, kDelFb, kDelMix,
        kChRate, kChDepth, kChMix,
        kFlRate, kFlDepth, kFlFb, kFlMix,
        kRvRoom, kRvDamp, kRvMix,
        kTrRate, kTrDepth, kTrShape,
        kIrHp, kIrLp, kIrTrim,
        kCount
    };

    struct KnobDef { const char* id; const char* label; };
    constexpr std::array<KnobDef, kCount> kDefs {{
        {"ng_threshold",   "THRESH"}, {"ng_release",  "RELEASE"},
        {"gate_threshold", "THRESH"}, {"gate_release", "RELEASE"},
        {"od_drive",       "DRIVE"},  {"od_tone",     "TONE"},   {"od_level",  "LEVEL"},
        {"dist_drive",     "DRIVE"},  {"dist_tone",   "TONE"},   {"dist_level","LEVEL"},
        {"input_level",    "INPUT"},  {"output_level","OUTPUT"},
        {"eq_bass",        "BASS"},   {"eq_mid_freq", "MID F"},  {"eq_mid_q",  "MID Q"},
        {"eq_mid_gain",    "MID"},    {"eq_treble",   "TREBLE"}, {"eq_presence","PRES"},
        {"eq_air",         "AIR"},
        {"depth",          "DEPTH"},  {"resonance",   "RES"},    {"resonance_freq","RES F"},
        {"ir_mix",         "IR MIX"}, {"quality_scale","QUAL"},
        {"hp_freq",        "HP FRQ"}, {"ln_target_db", "LN dB"},
        {"delay_time_ms",  "TIME"},   {"delay_feedback","FBK"},  {"delay_mix", "MIX"},
        {"chorus_rate_hz", "RATE"},   {"chorus_depth","DEPTH"},  {"chorus_mix","MIX"},
        {"flanger_rate_hz","RATE"},   {"flanger_depth","DEPTH"}, {"flanger_feedback","FBK"}, {"flanger_mix","MIX"},
        {"reverb_room",    "ROOM"},   {"reverb_damping","DAMP"}, {"reverb_mix",       "MIX"},
        {"tremolo_rate_hz","RATE"},   {"tremolo_depth","DEPTH"}, {"tremolo_shape",    "SHAPE"},
        {"ir_hp_freq",     "IR HP"},  {"ir_lp_freq",   "IR LP"}, {"ir_trim_db",       "TRIM"}
    }};
}

NAMAudioProcessorEditor::KnobBox&
NAMAudioProcessorEditor::addKnob (const juce::String& paramId, const juce::String& name)
{
    auto box = std::make_unique<KnobBox>();
    box->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    box->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 14);
    box->slider.setLookAndFeel (&lnf_);
    addAndMakeVisible (box->slider);

    box->label.setText (name, juce::dontSendNotification);
    box->label.setJustificationType (juce::Justification::centredTop);
    box->label.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    box->label.setColour (juce::Label::textColourId, juce::Colour (0xfff0e6c2));
    addAndMakeVisible (box->label);

    box->att = std::make_unique<SAtt> (processorRef.apvts, paramId, box->slider);
    knobs_.push_back (std::move (box));
    return *knobs_.back();
}

NAMAudioProcessorEditor::NAMAudioProcessorEditor (NAMAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), presetPanel (p.getPresetManager())
{
    setLookAndFeel (&lnf_);

    // Preset panel (added first so toggle in header sits above it; visibility via toFront).
    addChildComponent (presetPanel);
    addAndMakeVisible (presetsToggleBtn);
    presetsToggleBtn.onClick = [this] { togglePresetPanel(); };

    addAndMakeVisible (calBtn);
    calBtn.setTooltip ("Calibration / Output Mode");
    calBtn.onClick = [this] { showCalibrationPopup(); };

    // NORMAL toggle — Raw (0) <-> Normalized (1) shortcut. Choice range 0..2,
    // so param value in [0,1] must be mapped as normalised = index/2.
    addAndMakeVisible (normalToggle_);
    normalToggle_.setClickingTogglesState (true);
    normalToggle_.setTooltip ("Toggle Output Mode: Normalized <-> Raw");
    if (auto* pOut = processorRef.apvts.getParameter ("output_mode")) {
        normalAtt_ = std::make_unique<juce::ParameterAttachment> (
            *pOut,
            [this] (float v) {
                // v is the raw (unnormalised) parameter value = choice index (0..2).
                normalToggle_.setToggleState (v >= 0.5f, juce::dontSendNotification);
            });
        normalToggle_.onClick = [this] {
            const bool on = normalToggle_.getToggleState();
            // Choice param: raw value = index. 1 = Normalized, 0 = Raw.
            normalAtt_->setValueAsCompleteGesture (on ? 1.0f : 0.0f);
        };
        normalAtt_->sendInitialUpdate();
    }

    addAndMakeVisible (optionsBtn);
    optionsBtn.onClick = [this] { showOptionsMenu(); };

    addAndMakeVisible (zoomBtn);
    zoomBtn.onClick = [this] { showZoomMenu(); };

    addAndMakeVisible (osBtn);
    osBtn.setClickingTogglesState (true);
    osBtn.setToggleState (false, juce::dontSendNotification);
    osBtn.setTooltip ("Real 2x oversampling (higher CPU, cleaner highs)");
    osBtn.onClick = [this] {
        processorRef.setOversamplingEnabled (osBtn.getToggleState());
    };

    addAndMakeVisible (infoBtn);
    infoBtn.setTooltip ("Info / Credits / Guides / Donate");
    infoBtn.onClick = [this] { showInfoPopup(); };

    addAndMakeVisible (cpuMeter_);

    // Meters — closure reads APVTS choice "channel_mode" (0=Mono → 1 bar, ≥1 → 2 bars).
    auto isStereoFn = [&p = processorRef]() {
        auto* v = p.apvts.getRawParameterValue ("channel_mode");
        return v != nullptr && v->load() > 0.5f;
    };
    inMeter_  = std::make_unique<MeterStripComponent> (
        processorRef.getMeterInL(),  processorRef.getMeterInR(),  isStereoFn);
    outMeter_ = std::make_unique<MeterStripComponent> (
        processorRef.getMeterOutL(), processorRef.getMeterOutR(), isStereoFn);
    addAndMakeVisible (*inMeter_);
    addAndMakeVisible (*outMeter_);

    // Loader strip (below footer).
    for (auto* b : { &modelPrevBtn, &modelNextBtn, &modelBrowseBtn,
                     &irPrevBtn, &irNextBtn, &irBrowseBtn }) {
        addAndMakeVisible (*b);
    }
    for (auto* cb : { &modelCombo, &irCombo }) {
        addAndMakeVisible (*cb);
        cb->setTextWhenNothingSelected ("— none —");
    }
    for (auto* l : { &modelTitleLabel, &irTitleLabel }) {
        addAndMakeVisible (*l);
        l->setJustificationType (juce::Justification::centredRight);
        l->setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
        l->setColour (juce::Label::textColourId, juce::Colour (0xfff0e6c2));
    }
    refreshLabels();

    modelBrowseBtn.onClick = [this] { browseModel(); };
    irBrowseBtn   .onClick = [this] { browseIR(); };
    modelPrevBtn  .onClick = [this] { stepCombo (modelCombo, -1); };
    modelNextBtn  .onClick = [this] { stepCombo (modelCombo, +1); };
    irPrevBtn     .onClick = [this] { stepCombo (irCombo,    -1); };
    irNextBtn     .onClick = [this] { stepCombo (irCombo,    +1); };

    modelCombo.onChange = [this] {
        const int idx = modelCombo.getSelectedItemIndex();
        if (idx >= 0 && idx < modelFiles_.size()) {
            juce::File f = modelDir_.getChildFile (modelFiles_[idx]);
            if (f.existsAsFile()) processorRef.loadModelAsync (f);
        }
    };
    irCombo.onChange = [this] {
        const int idx = irCombo.getSelectedItemIndex();
        if (idx >= 0 && idx < irFiles_.size()) {
            juce::File f = irDir_.getChildFile (irFiles_[idx]);
            if (f.existsAsFile()) processorRef.loadIRAsync (f);
        }
    };

    // Knobs.
    knobs_.reserve (kCount);
    for (auto& d : kDefs)
        addKnob (d.id, d.label);

    // Mode + bypass toggles.
    modeBox.addItem ("Mono", 1);
    modeBox.addItem ("Dual", 2);
    modeBox.addItem ("Stereo", 3);
    addAndMakeVisible (modeBox);
    addAndMakeVisible (modeLabel);
    modeLabel.setJustificationType (juce::Justification::centredRight);
    modeLabel.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    modeLabel.setColour (juce::Label::textColourId, juce::Colour (0xfff0e6c2));
    modeAtt = std::make_unique<CAtt> (processorRef.apvts, "channel_mode", modeBox);

    for (auto* b : { &ampBypass, &irBypass, &ngBypass, &gateBypass, &odBypass, &distBypass,
                     &eqBypass, &hpBypass, &lnEnabled, &delBypass, &chBypass, &flBypass, &rvBypass, &trBypass,
                     &irHpBypass, &irLpBypass, &irPhaseInv }) {
        addAndMakeVisible (*b);
        b->setColour (juce::ToggleButton::textColourId, juce::Colour (0xfff0e6c2));
    }
    ampBypassAtt  = std::make_unique<InvertBypassBinding> (processorRef.apvts, "model_bypass", ampBypass);
    irBypassAtt   = std::make_unique<InvertBypassBinding> (processorRef.apvts, "ir_bypass",    irBypass);
    ngBypassAtt   = std::make_unique<InvertBypassBinding> (processorRef.apvts, "ng_bypass",    ngBypass);
    gateBypassAtt = std::make_unique<InvertBypassBinding> (processorRef.apvts, "gate_bypass",  gateBypass);
    odBypassAtt   = std::make_unique<InvertBypassBinding> (processorRef.apvts, "od_bypass",    odBypass);
    distBypassAtt = std::make_unique<InvertBypassBinding> (processorRef.apvts, "dist_bypass",  distBypass);
    eqBypassAtt   = std::make_unique<InvertBypassBinding> (processorRef.apvts, "eq_bypass",    eqBypass);
    hpBypassAtt   = std::make_unique<InvertBypassBinding> (processorRef.apvts, "hp_bypass",    hpBypass);
    lnEnabledAtt  = std::make_unique<BAtt> (processorRef.apvts, "ln_enabled",   lnEnabled);
    delBypassAtt  = std::make_unique<InvertBypassBinding> (processorRef.apvts, "delay_bypass", delBypass);
    chBypassAtt   = std::make_unique<InvertBypassBinding> (processorRef.apvts, "chorus_bypass",chBypass);
    flBypassAtt   = std::make_unique<InvertBypassBinding> (processorRef.apvts, "flanger_bypass",flBypass);
    rvBypassAtt   = std::make_unique<InvertBypassBinding> (processorRef.apvts, "reverb_bypass", rvBypass);
    trBypassAtt   = std::make_unique<InvertBypassBinding> (processorRef.apvts, "tremolo_bypass",trBypass);
    irHpBypassAtt = std::make_unique<InvertBypassBinding> (processorRef.apvts, "ir_hp_bypass",  irHpBypass);
    irLpBypassAtt = std::make_unique<InvertBypassBinding> (processorRef.apvts, "ir_lp_bypass",  irLpBypass);
    irPhaseInvAtt = std::make_unique<BAtt> (processorRef.apvts, "ir_phase_inv", irPhaseInv);
    // Toggles in our layout = "enabled when off" — invert visually if desired.

    // Tab buttons.
    addAndMakeVisible (mainTabBtn);
    addAndMakeVisible (fxTabBtn);
    mainTabBtn.setClickingTogglesState (true);
    fxTabBtn  .setClickingTogglesState (true);
    mainTabBtn.setRadioGroupId (0xCA11);
    fxTabBtn  .setRadioGroupId (0xCA11);
    mainTabBtn.setToggleState (true, juce::dontSendNotification);
    mainTabBtn.onClick = [this] { setActiveTab (Tab::Main); };
    fxTabBtn  .onClick = [this] { setActiveTab (Tab::Fx);   };

    // Slim slider (below MODEL loader). Mirrors QUAL knob via shared APVTS param.
    addAndMakeVisible (slimLabel_);
    slimLabel_.setJustificationType (juce::Justification::centredRight);
    slimLabel_.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    slimLabel_.setColour (juce::Label::textColourId, juce::Colour (0xfff0e6c2));
    slimSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    slimSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
    slimSlider_.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slimSlider_);
    slimAtt_ = std::make_unique<SAtt> (processorRef.apvts, "quality_scale", slimSlider_);
    updateSlimEnabled();
    startTimerHz (8);

    applyUiScale (GlobalSettings::get().getUiScalePercent());
}

// Sub-FullHD guard: on displays narrower than 1400 px (typical non-FullHD
// laptop panels and some 4K in fractional scaling) the 100/150/200% window
// sizes overflow the screen. Force the fit-safe 75% and lock the higher
// entries out of the menu.
static constexpr int kSubFullHDThreshold = 1400;

static int primaryDisplayWidthPx()
{
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        return d->userArea.getWidth();
    return kSubFullHDThreshold; // permissive fallback: assume FullHD
}

void NAMAudioProcessorEditor::applyUiScale (int percent)
{
    if (percent != 75 && percent != 100 && percent != 150 && percent != 200)
        percent = 100;
    if (primaryDisplayWidthPx() < kSubFullHDThreshold && percent > 75)
        percent = 75;
    const int w = juce::roundToInt (1620.0 * percent / 100.0);
    const int h = juce::roundToInt ( 890.0 * percent / 100.0);
    setSize (w, h);
}

void NAMAudioProcessorEditor::showZoomMenu()
{
    const int cur = GlobalSettings::get().getUiScalePercent();
    const bool subFullHD = primaryDisplayWidthPx() < kSubFullHDThreshold;
    juce::PopupMenu m;
    m.addSectionHeader (subFullHD ? "Window scale (sub-FullHD: only 75%)"
                                  : "Window scale");
    for (int s : { 75, 100, 150, 200 }) {
        const bool enabled = (s == 75) || ! subFullHD;
        m.addItem (juce::String (s) + " %", enabled, s == cur,
                   [this, s] {
                       GlobalSettings::get().setUiScalePercent (s);
                       applyUiScale (s);
                   });
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&zoomBtn));
}

void NAMAudioProcessorEditor::setActiveTab (Tab t)
{
    activeTab_ = t;
    resized();
    repaint();
}

void NAMAudioProcessorEditor::showOptionsMenu()
{
    const int cur = GlobalSettings::get().getMeterDepthDb();
    juce::PopupMenu m;
    m.addSectionHeader ("Meter depth");
    for (int d : { -60, -90, -120 })
        m.addItem (juce::String (d) + " dB", true, d == cur,
                   [d] { GlobalSettings::get().setMeterDepthDb (d); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&optionsBtn));
}

NAMAudioProcessorEditor::~NAMAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
    for (auto& k : knobs_) k->slider.setLookAndFeel (nullptr);
}

void NAMAudioProcessorEditor::updateSlimEnabled()
{
    const bool slim = processorRef.isCurrentModelSlimmable();
    if (slim == lastSlimmable_) return;
    lastSlimmable_ = slim;
    slimSlider_.setEnabled (slim);
    slimLabel_ .setAlpha   (slim ? 1.0f : 0.4f);
    if (kQuality < (int) knobs_.size() && knobs_[kQuality]) {
        knobs_[kQuality]->slider.setEnabled (slim);
        knobs_[kQuality]->label .setAlpha  (slim ? 1.0f : 0.4f);
    }
}

void NAMAudioProcessorEditor::timerCallback()
{
    updateSlimEnabled();
}

// ---------------- Painting --------------------------------------------------

void NAMAudioProcessorEditor::paintMetalBackground (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Base brushed-metal gradient (anthracite).
    juce::ColourGradient base (juce::Colour (0xff2e2e2e), 0, 0,
                               juce::Colour (0xff1a1a1a), 0, r.getHeight(),
                               false);
    g.setGradientFill (base);
    g.fillAll();

    // Vertical brushed streaks.
    juce::Random rng (12345);
    for (int x = 0; x < (int) r.getWidth(); x += 2)
    {
        const float v = rng.nextFloat();
        const auto col = juce::Colour::fromFloatRGBA (0.18f + v * 0.06f,
                                                     0.18f + v * 0.06f,
                                                     0.18f + v * 0.06f, 0.35f);
        g.setColour (col);
        g.drawVerticalLine (x, 0.0f, r.getHeight());
    }

    // Top highlight band + bottom shadow band.
    g.setColour (juce::Colour::fromFloatRGBA (1.f, 1.f, 1.f, 0.07f));
    g.fillRect (juce::Rectangle<float> (0, 0, r.getWidth(), 26.f));
    g.setColour (juce::Colour::fromFloatRGBA (0, 0, 0, 0.35f));
    g.fillRect (juce::Rectangle<float> (0, r.getHeight() - 18.f, r.getWidth(), 18.f));

    // Outer bezel.
    g.setColour (juce::Colour (0xff0a0a0a));
    g.drawRect (getLocalBounds(), 2);
}

void NAMAudioProcessorEditor::paintScrew (juce::Graphics& g, juce::Point<float> c, float radius)
{
    juce::ColourGradient gr (juce::Colour (0xffd0d0d0), c.x - radius, c.y - radius,
                             juce::Colour (0xff555555), c.x + radius, c.y + radius,
                             true);
    g.setGradientFill (gr);
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2.f, radius * 2.f);
    g.setColour (juce::Colour (0xff222222));
    g.drawEllipse (c.x - radius, c.y - radius, radius * 2.f, radius * 2.f, 1.0f);
    // Slot (rotated random).
    g.setColour (juce::Colour (0xff111111));
    juce::AffineTransform tr = juce::AffineTransform::rotation (0.4f, c.x, c.y);
    g.fillRect (juce::Rectangle<float> (c.x - radius * 0.7f, c.y - 1.2f, radius * 1.4f, 2.4f).transformedBy (tr));
}

void NAMAudioProcessorEditor::paintTitle (juce::Graphics& g, juce::Rectangle<int> area)
{
    // Title plate (chrome).
    auto a = area.toFloat().reduced (12.f, 6.f);
    juce::ColourGradient plate (juce::Colour (0xff6b6b6b), a.getX(), a.getY(),
                                juce::Colour (0xff1f1f1f), a.getX(), a.getBottom(),
                                false);
    g.setGradientFill (plate);
    g.fillRoundedRectangle (a, 6.f);
    g.setColour (juce::Colour (0xff0a0a0a));
    g.drawRoundedRectangle (a, 6.f, 1.5f);

    // Embossed text.
    juce::Font titleFont (juce::FontOptions().withTypeface (pirataTypeface()).withHeight (62.0f));
    g.setFont (titleFont);
    auto txtBounds = a.toNearestInt();
    const juce::String txt ("..::METAL NAM GEAR PLAYER::..");
    // Shadow.
    g.setColour (juce::Colour (0xff000000).withAlpha (0.8f));
    g.drawText (txt, txtBounds.translated (1, 2), juce::Justification::centred);
    // Highlight.
    g.setColour (juce::Colour (0xffffe48a));
    g.drawText (txt, txtBounds, juce::Justification::centred);
}

void NAMAudioProcessorEditor::paintGroupPanel (juce::Graphics& g,
                                               juce::Rectangle<int> r,
                                               const juce::String& title)
{
    auto a = r.toFloat().reduced (3.f);
    juce::ColourGradient gr (juce::Colour (0xff262626), a.getX(), a.getY(),
                             juce::Colour (0xff141414), a.getX(), a.getBottom(),
                             false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (a, 4.f);
    g.setColour (juce::Colour (0xff0a0a0a));
    g.drawRoundedRectangle (a, 4.f, 1.0f);
    // Inner highlight line.
    g.setColour (juce::Colour::fromFloatRGBA (1.f, 1.f, 1.f, 0.05f));
    g.drawHorizontalLine ((int) a.getY() + 1, a.getX() + 2, a.getRight() - 2);

    // Title strip at top.
    auto titleStrip = a.removeFromTop (30.f);
    juce::Font groupFont (juce::FontOptions().withTypeface (metalManiaTypeface()).withHeight (26.0f));
    g.setFont (groupFont);
    // Shadow.
    g.setColour (juce::Colour (0xff000000).withAlpha (0.7f));
    g.drawText (title, titleStrip.toNearestInt().translated (1, 1), juce::Justification::centred);
    g.setColour (juce::Colour (0xffd9a200));
    g.drawText (title, titleStrip.toNearestInt(), juce::Justification::centred);
}

void NAMAudioProcessorEditor::paint (juce::Graphics& g)
{
    paintMetalBackground (g);
    paintTitle (g, titleArea_);

    // Screws at 4 corners.
    const float sr = 6.f;
    paintScrew (g, { 14.f, 14.f }, sr);
    paintScrew (g, { (float) getWidth() - 14.f, 14.f }, sr);
    paintScrew (g, { 14.f, (float) getHeight() - 14.f }, sr);
    paintScrew (g, { (float) getWidth() - 14.f, (float) getHeight() - 14.f }, sr);

    for (auto& gp : groupPanels_)
        paintGroupPanel (g, gp.first, gp.second);
}

// ---------------- Layout ----------------------------------------------------

void NAMAudioProcessorEditor::resized()
{
    groupPanels_.clear();

    // Reserve 28-px vertical strips at far left/right for meters (Stage 8).
    auto full = getLocalBounds();
    inMeterArea_  = full.removeFromLeft  (28);
    outMeterArea_ = full.removeFromRight (28);
    if (inMeter_)  inMeter_ ->setBounds (inMeterArea_ .reduced (2, 24));
    if (outMeter_) outMeter_->setBounds (outMeterArea_.reduced (2, 24));

    auto r = full.reduced (24, 24);

    // Title strip.
    titleArea_ = r.removeFromTop (96);
    r.removeFromTop (4);

    // Header (tabs left + presets toggle on far right). Load NAM / IR moved to loader strip.
    headerArea_ = r.removeFromTop (38);
    {
        auto h = headerArea_.reduced (2);
        auto zoomCell    = h.removeFromRight (70).reduced (4, 4);
        zoomBtn.setBounds (zoomCell);
        auto cpuCell     = h.removeFromRight (110).reduced (4, 4);
        cpuMeter_.setBounds (cpuCell);
        auto osCell      = h.removeFromRight (60).reduced (4, 4);
        osBtn.setBounds (osCell);
        auto infoCell    = h.removeFromRight (40).reduced (4, 4);
        infoBtn.setBounds (infoCell);
        auto presetCell  = h.removeFromRight (90).reduced (4, 2);
        presetsToggleBtn.setBounds (presetCell);
        auto calCell     = h.removeFromRight (56).reduced (4, 2);
        calBtn.setBounds (calCell);

        auto tabs = h.removeFromLeft (130).reduced (2, 4);
        mainTabBtn.setBounds (tabs.removeFromLeft (60));
        tabs.removeFromLeft (4);
        fxTabBtn  .setBounds (tabs.removeFromLeft (60));
    }

    // Preset panel overlay (positioned offscreen-right when closed, slides in).
    {
        const int pw = 320;
        const int ph = getHeight();
        const int px = panelOpen_ ? (getWidth() - pw) : getWidth();
        presetPanel.setBounds (px, 0, pw, ph);
        presetPanel.toFront (false);
        presetsToggleBtn.toFront (false);
    }

    r.removeFromTop (6);

    // Loader strip (very bottom): MODEL  ◀ [combo ▾] ▶ [Browse]   |   IR  ◀ [combo ▾] ▶ [Browse]
    loaderArea_ = r.removeFromBottom (92);
    {
        auto strip = loaderArea_.reduced (4, 8);
        auto half = strip.getWidth() / 2;
        auto modelBlock = strip.removeFromLeft (half).reduced (4, 0);
        strip.removeFromLeft (8);
        auto irStrip    = strip.reduced (4, 0);

        // Split model side vertically: top = existing loader row, bottom = SLIM slider.
        auto modelStrip = modelBlock.removeFromTop (28);
        modelBlock.removeFromTop (4);
        auto slimRow = modelBlock.removeFromTop (24);

        // IR side keeps a single row centered vertically for symmetry.
        auto irRow = irStrip.removeFromTop (28);

        auto layoutOne = [] (juce::Rectangle<int> area,
                             juce::Label& title,
                             juce::TextButton& prev, juce::ComboBox& combo,
                             juce::TextButton& next, juce::TextButton& browse) {
            title.setBounds  (area.removeFromLeft (60));
            area.removeFromLeft (4);
            prev .setBounds  (area.removeFromLeft (28));
            area.removeFromLeft (2);
            browse.setBounds (area.removeFromRight (80));
            area.removeFromRight (2);
            next .setBounds  (area.removeFromRight (28));
            area.removeFromRight (2);
            combo.setBounds  (area);
        };
        layoutOne (modelStrip, modelTitleLabel, modelPrevBtn, modelCombo, modelNextBtn, modelBrowseBtn);
        layoutOne (irRow,      irTitleLabel,    irPrevBtn,    irCombo,    irNextBtn,    irBrowseBtn);

        // Slim label + slider aligned with combo column (skip the MODEL title width).
        slimLabel_ .setBounds (slimRow.removeFromLeft (60));
        slimRow.removeFromLeft (4);
        slimSlider_.setBounds (slimRow);
    }
    r.removeFromBottom (4);

    // Footer. Bypass toggles depend on active tab.
    footerArea_ = r.removeFromBottom (54);
    {
        auto f = footerArea_.reduced (4, 6);
        const int cellW = f.getWidth() / 12;
        auto modeCell  = f.removeFromLeft (cellW * 2);
        modeLabel.setBounds (modeCell.removeFromLeft (50));
        modeBox  .setBounds (modeCell.reduced (4, 10));
        optionsBtn.setBounds (f.removeFromLeft (cellW).reduced (6, 10));

        auto hideBtn = [] (juce::ToggleButton& b) { b.setVisible (false); };
        auto placeBtn = [&] (juce::ToggleButton& b) {
            b.setVisible (true);
            b.setBounds (f.removeFromLeft (cellW).reduced (6, 10));
        };

        if (activeTab_ == Tab::Main) {
            placeBtn (ngBypass);
            placeBtn (gateBypass);
            placeBtn (odBypass);
            placeBtn (distBypass);
            placeBtn (ampBypass);
            placeBtn (eqBypass);
            placeBtn (irBypass);
            placeBtn (hpBypass);
            placeBtn (lnEnabled);
            placeBtn (irHpBypass);
            placeBtn (irLpBypass);
            placeBtn (irPhaseInv);
            hideBtn (delBypass); hideBtn (chBypass); hideBtn (flBypass); hideBtn (rvBypass); hideBtn (trBypass);
        } else {
            placeBtn (delBypass);
            placeBtn (chBypass);
            placeBtn (flBypass);
            placeBtn (rvBypass);
            placeBtn (trBypass);
            hideBtn (ngBypass); hideBtn (gateBypass); hideBtn (odBypass);
            hideBtn (distBypass); hideBtn (ampBypass); hideBtn (eqBypass); hideBtn (irBypass);
            hideBtn (hpBypass); hideBtn (lnEnabled);
            hideBtn (irHpBypass); hideBtn (irLpBypass); hideBtn (irPhaseInv);
        }
    }

    r.removeFromBottom (6);

    // Main knob panel.
    panelArea_ = r;

    // Define groups (knob indices + title) per active tab.
    struct Group { juce::String name; std::vector<int> ids; };
    std::vector<Group> groups;
    if (activeTab_ == Tab::Main) {
        groups = {
            { "NGATE",      { kNgThresh, kNgRelease } },
            { "GATE",       { kGateThresh, kGateRelease } },
            { "OVERDRIVE",  { kOdDrive, kOdTone, kOdLevel } },
            { "DISTORTION", { kDistDrive, kDistTone, kDistLevel } },
            { "AMP",        { kInput, kOutput } },
            { "EQ",         { kBass, kMidGain, kTreble, kPres, kAir, kMidFreq, kMidQ } },
            { "POWER",      { kDepth, kRes, kResFreq } },
            { "CAB",        { kIrMix, kQuality } },
            { "IR TOOLS",   { kIrHp, kIrLp, kIrTrim } },
            { "MASTER",     { kHpFreq, kLnTarget } },
        };
    } else {
        groups = {
            { "DELAY",   { kDelTime, kDelFb, kDelMix } },
            { "CHORUS",  { kChRate, kChDepth, kChMix } },
            { "FLANGER", { kFlRate, kFlDepth, kFlFb, kFlMix } },
            { "REVERB",  { kRvRoom, kRvDamp, kRvMix } },
            { "TREMOLO", { kTrRate, kTrDepth, kTrShape } },
        };
    }

    // Hide all knobs first; visible-tab ones will be re-placed below.
    for (auto& kb : knobs_) {
        kb->slider.setVisible (false);
        kb->label .setVisible (false);
    }

    // Total knobs across all groups: 2+3+3+2+7+3+2 = 22 (ok, kCount)
    int totalKnobs = 0; for (auto& g : groups) totalKnobs += (int) g.ids.size();

    // Allocate widths proportionally.
    const int gap = 6;
    int availW = panelArea_.getWidth() - gap * ((int) groups.size() - 1);
    auto cursorRect = panelArea_;

    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        auto& gr = groups[gi];
        const int w = (int) std::round ((double) availW * (double) gr.ids.size() / (double) totalKnobs);
        auto panel = cursorRect.removeFromLeft (w);
        groupPanels_.push_back ({ panel, gr.name });

        auto inside = panel.reduced (6, 4);
        inside.removeFromTop (30); // title strip

        // Show knobs in this group.
        for (int idx : gr.ids) {
            knobs_[idx]->slider.setVisible (true);
            knobs_[idx]->label .setVisible (true);
        }

        // For groups with many knobs (EQ), wrap into 2 rows.
        if ((int) gr.ids.size() > 4) {
            const int rowH = inside.getHeight() / 2;
            auto row1 = inside.removeFromTop (rowH);
            auto row2 = inside;
            const int half = ((int) gr.ids.size() + 1) / 2;
            const int cellW1 = row1.getWidth() / half;
            const int cellW2 = row2.getWidth() / ((int) gr.ids.size() - half);
            for (int i = 0; i < (int) gr.ids.size(); ++i) {
                auto cell = (i < half) ? row1.removeFromLeft (cellW1)
                                       : row2.removeFromLeft (cellW2);
                cell = cell.reduced (3);
                auto lab = cell.removeFromTop (12);
                knobs_[gr.ids[i]]->label.setBounds (lab);
                knobs_[gr.ids[i]]->slider.setBounds (cell);
            }
        } else {
            // Vertical stack — one knob per row, full panel width.
            const int cellH = inside.getHeight() / (int) gr.ids.size();
            for (int idx : gr.ids) {
                auto cell = inside.removeFromTop (cellH).reduced (4, 3);
                auto lab = cell.removeFromTop (12);
                knobs_[idx]->label.setBounds (lab);
                knobs_[idx]->slider.setBounds (cell);
            }
        }

        if (gi + 1 < groups.size())
            cursorRect.removeFromLeft (gap);
    }

    // NORMAL toggle: place a small toggle strip on top of the OUTPUT knob cell,
    // stealing ~14 px from the bottom of the knob slider. Only visible in Main tab.
    if (activeTab_ == Tab::Main
        && kOutput < (int) knobs_.size()
        && knobs_[kOutput]
        && knobs_[kOutput]->slider.isVisible()) {
        auto sb = knobs_[kOutput]->slider.getBounds();
        const int th = 16;
        if (sb.getHeight() > th + 8) {
            auto togRect = sb.removeFromBottom (th);
            knobs_[kOutput]->slider.setBounds (sb);
            normalToggle_.setVisible (true);
            normalToggle_.setBounds (togRect.reduced (2, 1));
        } else {
            normalToggle_.setVisible (false);
        }
    } else {
        normalToggle_.setVisible (false);
    }
}

// ---------------- File pickers ----------------------------------------------

void NAMAudioProcessorEditor::browseModel()
{
    juce::File start = modelDir_.isDirectory() ? modelDir_ : juce::File();
    if (start == juce::File()) {
        auto persisted = GlobalSettings::get().getLastModelDir();
        if (persisted.isNotEmpty() && juce::File (persisted).isDirectory())
            start = juce::File (persisted);
    }
    chooser = std::make_unique<juce::FileChooser> (
        "Load NAM model", start, "*.nam");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f.existsAsFile()) {
                processorRef.loadModelAsync (f);
                rescanModelDir (f);
            }
        });
}

void NAMAudioProcessorEditor::browseIR()
{
    juce::File start = irDir_.isDirectory() ? irDir_ : juce::File();
    if (start == juce::File()) {
        auto persisted = GlobalSettings::get().getLastIRDir();
        if (persisted.isNotEmpty() && juce::File (persisted).isDirectory())
            start = juce::File (persisted);
    }
    chooser = std::make_unique<juce::FileChooser> (
        "Load IR (WAV)", start, "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f.existsAsFile()) {
                processorRef.loadIRAsync (f);
                rescanIRDir (f);
            }
        });
}

void NAMAudioProcessorEditor::rescanModelDir (const juce::File& sel)
{
    modelDir_ = sel.getParentDirectory();
    if (modelDir_.isDirectory())
        GlobalSettings::get().setLastModelDir (modelDir_.getFullPathName());
    modelFiles_.clear();
    if (modelDir_.isDirectory()) {
        juce::Array<juce::File> files;
        modelDir_.findChildFiles (files, juce::File::findFiles, false, "*.nam");
        for (auto& f : files) modelFiles_.add (f.getFileName());
        modelFiles_.sortNatural();
    }
    modelCombo.clear (juce::dontSendNotification);
    for (int i = 0; i < modelFiles_.size(); ++i)
        modelCombo.addItem (modelFiles_[i], i + 1);
    const int sIdx = modelFiles_.indexOf (sel.getFileName());
    if (sIdx >= 0) modelCombo.setSelectedItemIndex (sIdx, juce::dontSendNotification);
}

void NAMAudioProcessorEditor::rescanIRDir (const juce::File& sel)
{
    irDir_ = sel.getParentDirectory();
    if (irDir_.isDirectory())
        GlobalSettings::get().setLastIRDir (irDir_.getFullPathName());
    irFiles_.clear();
    if (irDir_.isDirectory()) {
        juce::Array<juce::File> files;
        irDir_.findChildFiles (files, juce::File::findFiles, false, "*.wav");
        for (auto& f : files) irFiles_.add (f.getFileName());
        irFiles_.sortNatural();
    }
    irCombo.clear (juce::dontSendNotification);
    for (int i = 0; i < irFiles_.size(); ++i)
        irCombo.addItem (irFiles_[i], i + 1);
    const int sIdx = irFiles_.indexOf (sel.getFileName());
    if (sIdx >= 0) irCombo.setSelectedItemIndex (sIdx, juce::dontSendNotification);
}

void NAMAudioProcessorEditor::stepCombo (juce::ComboBox& cb, int delta)
{
    const int n = cb.getNumItems();
    if (n <= 0) return;
    int idx = cb.getSelectedItemIndex();
    if (idx < 0) idx = 0;
    idx = ((idx + delta) % n + n) % n;
    cb.setSelectedItemIndex (idx, juce::sendNotificationSync);
}

void NAMAudioProcessorEditor::refreshLabels()
{
    auto mp = processorRef.getCurrentModelPath();
    auto ip = processorRef.getCurrentIRPath();
    if (mp.isNotEmpty()) {
        juce::File f (mp);
        if (f.existsAsFile()) rescanModelDir (f);
    }
    if (ip.isNotEmpty()) {
        juce::File f (ip);
        if (f.existsAsFile()) rescanIRDir (f);
    }
}

void NAMAudioProcessorEditor::togglePresetPanel()
{
    panelOpen_ = ! panelOpen_;
    const int pw = presetPanel.getWidth() > 0 ? presetPanel.getWidth() : 320;
    const int targetX = panelOpen_ ? (getWidth() - pw) : getWidth();
    presetPanel.setVisible (true); // keep visible during slide
    presetPanel.toFront (false);
    presetsToggleBtn.toFront (false);
    juce::Desktop::getInstance().getAnimator().animateComponent (
        &presetPanel,
        juce::Rectangle<int> (targetX, 0, pw, getHeight()),
        1.0f, 200, false, 1.0, 1.0);
}

void NAMAudioProcessorEditor::showCalibrationPopup()
{
    struct CalContent : public juce::Component {
        juce::Label title;
        juce::Label modeLbl, calInLbl, calLevelLbl, infoLbl;
        juce::ComboBox modeBox;
        juce::ToggleButton calInBtn { "Calibrate Input" };
        juce::Slider calLevel;
        std::unique_ptr<CAtt> modeAtt;
        std::unique_ptr<BAtt> calInAtt;
        std::unique_ptr<SAtt> calLevelAtt;

        CalContent (NAMAudioProcessor& proc) {
            title.setText ("Calibration / Output Mode", juce::dontSendNotification);
            title.setJustificationType (juce::Justification::centred);
            title.setFont (juce::Font (juce::FontOptions().withHeight (18.f).withStyle ("Bold")));
            addAndMakeVisible (title);

            modeLbl.setText ("Output Mode", juce::dontSendNotification);
            addAndMakeVisible (modeLbl);
            modeBox.addItem ("Raw",        1);
            modeBox.addItem ("Normalized", 2);
            modeBox.addItem ("Calibrated", 3);
            addAndMakeVisible (modeBox);
            modeAtt = std::make_unique<CAtt> (proc.apvts, "output_mode", modeBox);

            calInLbl.setText ("Calibrate Input", juce::dontSendNotification);
            addAndMakeVisible (calInLbl);
            addAndMakeVisible (calInBtn);
            calInAtt = std::make_unique<BAtt> (proc.apvts, "calibrate_input", calInBtn);

            calLevelLbl.setText ("Input Cal Level (dBu)", juce::dontSendNotification);
            addAndMakeVisible (calLevelLbl);
            calLevel.setSliderStyle (juce::Slider::LinearHorizontal);
            calLevel.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 18);
            calLevel.setRange (-60.0, 60.0, 0.1);
            addAndMakeVisible (calLevel);
            calLevelAtt = std::make_unique<SAtt> (proc.apvts, "input_cal_level", calLevel);

            // Model info: read cached metadata from pipeline L.
            const auto& pl = proc.pipelineL();
            juce::String info;
            info << "Model loudness: "
                 << (pl.modelHasLoudness()
                     ? juce::String (pl.modelLoudnessDB(), 2) + " dB (known)"
                     : juce::String ("unknown"))
                 << "\nModel input level: "
                 << (pl.modelHasInputLevel()
                     ? juce::String (pl.modelInputLevelDBu(), 2) + " dBu (known)"
                     : juce::String ("unknown"))
                 << "\nModel output level: "
                 << (pl.modelHasOutputLevel()
                     ? juce::String (pl.modelOutputLevelDBu(), 2) + " dBu (known)"
                     : juce::String ("unknown"));
            infoLbl.setText (info, juce::dontSendNotification);
            infoLbl.setJustificationType (juce::Justification::topLeft);
            infoLbl.setFont (juce::Font (juce::FontOptions (11.f)));
            addAndMakeVisible (infoLbl);

            // Grey-out disabled semantics (like Steve): if a mode's needed
            // metadata is absent, the choice still switches but is a no-op.
            // We reflect this by disabling controls whose metadata is missing.
            if (! pl.modelHasInputLevel()) {
                calInBtn.setEnabled (false);
                calLevel.setEnabled (false);
                calInLbl.setAlpha (0.5f);
                calLevelLbl.setAlpha (0.5f);
            }

            setSize (420, 320);
        }
        void resized() override {
            auto r = getLocalBounds().reduced (12);
            title.setBounds (r.removeFromTop (28));
            r.removeFromTop (8);
            auto row = r.removeFromTop (28);
            modeLbl.setBounds (row.removeFromLeft (160));
            modeBox.setBounds (row);
            r.removeFromTop (8);
            row = r.removeFromTop (28);
            calInLbl.setBounds (row.removeFromLeft (160));
            calInBtn.setBounds (row);
            r.removeFromTop (8);
            row = r.removeFromTop (28);
            calLevelLbl.setBounds (row.removeFromLeft (160));
            calLevel.setBounds (row);
            r.removeFromTop (12);
            infoLbl.setBounds (r);
        }
    };
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Calibration";
    opts.dialogBackgroundColour = juce::Colour (0xff1a1a1a);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.content.setOwned (new CalContent (processorRef));
    opts.launchAsync();
}

void NAMAudioProcessorEditor::showInfoPopup()
{
    struct InfoContent : public juce::Component {
        juce::Label title, version;
        juce::TextEditor credits;
        juce::TextButton quickBtn, techBtn, donateBtn;
        InfoContent() {
            title.setText ("..::METAL NAM GEAR PLAYER::..", juce::dontSendNotification);
            title.setJustificationType (juce::Justification::centred);
            title.setFont (juce::Font (juce::FontOptions().withHeight (22.f).withStyle ("Bold")));
            version.setText ("Version 1.0.0-dev", juce::dontSendNotification);
            version.setJustificationType (juce::Justification::centred);
            credits.setMultiLine (true);
            credits.setReadOnly (true);
            credits.setScrollbarsShown (true);
            credits.setText (
                "This work: AGPL-3.0-or-later (JUCE 8 combination requirement).\n\n"
                "Third-party components:\n"
                "- Neural Amp Modeler (Steven Atkinson) - MIT\n"
                "- neural-amp-modeler-lv2 (Mike Oliphant) - GPL-3.0\n"
                "- NeuralAudio (Mike Oliphant) - MIT\n"
                "- JUCE (Raw Material Software) - AGPL-3.0\n"
                "- VST3 SDK (Steinberg) - GPL-3.0\n"
                "- dr_wav (David Reid) - MIT / public domain\n"
                "- FFTConvolver (HiFi-LoFi) - MIT\n"
                "- LV2 (LV2 authors) - ISC\n\n"
                "UI fonts (embedded, SIL Open Font License 1.1):\n"
                "- Metal Mania - Copyright (c) 2012 Open Window\n"
                "- Nosifer     - Copyright (c) 2011 Typomondo\n"
                "- Pirata One  - Copyright (c) 2012 Rodrigo Fuenzalida, Nicolas Massi\n\n"
                "Bundled fixture model (demo_wavenet_a1.nam, MIT):\n"
                "- Steven Atkinson - from NeuralAmpModelerCore/example_models\n\n"
                "Trademarks:\n"
                "\"Neural Amp Modeler\" is a trademark of Steven Atkinson.\n"
                "\"VST\" is a trademark of Steinberg Media Technologies GmbH.\n"
                "\"JUCE\" is a trademark of Raw Material Software Limited.\n"
                "This project is independent and not affiliated with any of them.\n");
            quickBtn .setButtonText ("Guida Rapida (PDF)");
            techBtn  .setButtonText ("Guida Tecnica (PDF)");
            donateBtn.setButtonText ("Donation");
            quickBtn.onClick = [] {
                juce::URL ("https://github.com/fabionet/metal-nam-gear-player/blob/juce-rewrite/docs/guida-rapida.pdf").launchInDefaultBrowser();
            };
            techBtn.onClick = [] {
                juce::URL ("https://github.com/fabionet/metal-nam-gear-player/blob/juce-rewrite/docs/guida-tecnica.pdf").launchInDefaultBrowser();
            };
            donateBtn.onClick = [] {
                juce::URL ("https://github.com/fabionet/metal-nam-gear-player").launchInDefaultBrowser();
            };
            addAndMakeVisible (title);
            addAndMakeVisible (version);
            addAndMakeVisible (credits);
            addAndMakeVisible (quickBtn);
            addAndMakeVisible (techBtn);
            addAndMakeVisible (donateBtn);
            setSize (480, 360);
        }
        void resized() override {
            auto r = getLocalBounds().reduced (12);
            title.setBounds (r.removeFromTop (28));
            version.setBounds (r.removeFromTop (20));
            r.removeFromTop (8);
            auto btnRow = r.removeFromBottom (30);
            const int bw = btnRow.getWidth() / 3 - 4;
            quickBtn .setBounds (btnRow.removeFromLeft (bw));
            btnRow.removeFromLeft (6);
            techBtn  .setBounds (btnRow.removeFromLeft (bw));
            btnRow.removeFromLeft (6);
            donateBtn.setBounds (btnRow);
            r.removeFromBottom (8);
            credits.setBounds (r);
        }
    };
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Info";
    opts.dialogBackgroundColour = juce::Colour (0xff1a1a1a);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.content.setOwned (new InfoContent());
    opts.launchAsync();
}
