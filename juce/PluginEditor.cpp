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
        kGateThresh, kGateRelease,
        kOdDrive, kOdTone, kOdLevel,
        kDistDrive, kDistTone, kDistLevel,
        kInput, kOutput,
        kBass, kMidFreq, kMidQ, kMidGain, kTreble, kPres, kAir,
        kDepth, kRes, kResFreq,
        kIrMix, kQuality,
        kHpFreq, kLnTarget,
        kCount
    };

    struct KnobDef { const char* id; const char* label; };
    constexpr std::array<KnobDef, kCount> kDefs {{
        {"gate_threshold", "THRESH"}, {"gate_release", "RELEASE"},
        {"od_drive",       "DRIVE"},  {"od_tone",     "TONE"},   {"od_level",  "LEVEL"},
        {"dist_drive",     "DRIVE"},  {"dist_tone",   "TONE"},   {"dist_level","LEVEL"},
        {"input_level",    "INPUT"},  {"output_level","OUTPUT"},
        {"eq_bass",        "BASS"},   {"eq_mid_freq", "MID F"},  {"eq_mid_q",  "MID Q"},
        {"eq_mid_gain",    "MID"},    {"eq_treble",   "TREBLE"}, {"eq_presence","PRES"},
        {"eq_air",         "AIR"},
        {"depth",          "DEPTH"},  {"resonance",   "RES"},    {"resonance_freq","RES F"},
        {"ir_mix",         "IR MIX"}, {"quality_scale","QUAL"},
        {"hp_freq",        "HP FRQ"}, {"ln_target_db", "LN dB"}
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

    addAndMakeVisible (optionsBtn);
    optionsBtn.onClick = [this] { showOptionsMenu(); };

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

    // Header.
    addAndMakeVisible (loadModelBtn);
    addAndMakeVisible (loadIRBtn);
    addAndMakeVisible (modelLabel);
    addAndMakeVisible (irLabel);
    for (auto* l : { &modelLabel, &irLabel }) {
        l->setJustificationType (juce::Justification::centredLeft);
        l->setFont (juce::Font (juce::FontOptions (11.0f)));
        l->setColour (juce::Label::textColourId, juce::Colour (0xfff0e6c2));
    }
    refreshLabels();
    loadModelBtn.onClick = [this] { browseModel(); };
    loadIRBtn   .onClick = [this] { browseIR(); };

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

    for (auto* b : { &ampBypass, &irBypass, &gateBypass, &odBypass, &distBypass, &hpBypass, &lnEnabled }) {
        addAndMakeVisible (*b);
        b->setColour (juce::ToggleButton::textColourId, juce::Colour (0xfff0e6c2));
    }
    ampBypassAtt  = std::make_unique<BAtt> (processorRef.apvts, "model_bypass", ampBypass);
    irBypassAtt   = std::make_unique<BAtt> (processorRef.apvts, "ir_bypass",    irBypass);
    gateBypassAtt = std::make_unique<BAtt> (processorRef.apvts, "gate_bypass",  gateBypass);
    odBypassAtt   = std::make_unique<BAtt> (processorRef.apvts, "od_bypass",    odBypass);
    distBypassAtt = std::make_unique<BAtt> (processorRef.apvts, "dist_bypass",  distBypass);
    hpBypassAtt   = std::make_unique<BAtt> (processorRef.apvts, "hp_bypass",    hpBypass);
    lnEnabledAtt  = std::make_unique<BAtt> (processorRef.apvts, "ln_enabled",   lnEnabled);
    // Toggles in our layout = "enabled when off" — invert visually if desired.

    setSize (1620, 820);
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
    setLookAndFeel (nullptr);
    for (auto& k : knobs_) k->slider.setLookAndFeel (nullptr);
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
    // Shadow.
    g.setColour (juce::Colour (0xff000000).withAlpha (0.8f));
    g.drawText ("METAL PLUG IN PLAYS", txtBounds.translated (1, 2), juce::Justification::centred);
    // Highlight.
    g.setColour (juce::Colour (0xffffe48a));
    g.drawText ("METAL PLUG IN PLAYS", txtBounds, juce::Justification::centred);
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

    // Header (load model / IR + presets toggle on far right).
    headerArea_ = r.removeFromTop (38);
    {
        auto h = headerArea_.reduced (2);
        auto presetCell  = h.removeFromRight (90).reduced (4, 2);
        presetsToggleBtn.setBounds (presetCell);
        auto optionsCell = h.removeFromRight (66).reduced (4, 2);
        optionsBtn.setBounds (optionsCell);

        auto left  = h.removeFromLeft (h.getWidth() / 2).reduced (4, 0);
        auto right = h.reduced (4, 0);
        loadModelBtn.setBounds (left.removeFromLeft (110));
        left.removeFromLeft (8);
        modelLabel.setBounds (left);
        loadIRBtn.setBounds (right.removeFromLeft (110));
        right.removeFromLeft (8);
        irLabel.setBounds (right);
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

    // Footer.
    footerArea_ = r.removeFromBottom (54);
    {
        auto f = footerArea_.reduced (4, 6);
        const int cellW = f.getWidth() / 9;
        auto modeCell  = f.removeFromLeft (cellW * 2);
        modeLabel.setBounds (modeCell.removeFromLeft (50));
        modeBox  .setBounds (modeCell.reduced (4, 10));
        gateBypass.setBounds (f.removeFromLeft (cellW).reduced (6, 10));
        odBypass  .setBounds (f.removeFromLeft (cellW).reduced (6, 10));
        distBypass.setBounds (f.removeFromLeft (cellW).reduced (6, 10));
        ampBypass .setBounds (f.removeFromLeft (cellW).reduced (6, 10));
        irBypass  .setBounds (f.removeFromLeft (cellW).reduced (6, 10));
        hpBypass  .setBounds (f.removeFromLeft (cellW).reduced (6, 10));
        lnEnabled .setBounds (f.reduced (6, 10));
    }

    r.removeFromBottom (6);

    // Main knob panel.
    panelArea_ = r;

    // Define groups (knob indices + title).
    struct Group { juce::String name; std::vector<int> ids; };
    std::vector<Group> groups = {
        { "GATE",       { kGateThresh, kGateRelease } },
        { "OVERDRIVE",  { kOdDrive, kOdTone, kOdLevel } },
        { "DISTORTION", { kDistDrive, kDistTone, kDistLevel } },
        { "AMP",        { kInput, kOutput } },
        { "EQ",         { kBass, kMidGain, kTreble, kPres, kAir, kMidFreq, kMidQ } },
        { "POWER",      { kDepth, kRes, kResFreq } },
        { "CAB",        { kIrMix, kQuality } },
        { "MASTER",     { kHpFreq, kLnTarget } },
    };

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
}

// ---------------- File pickers ----------------------------------------------

void NAMAudioProcessorEditor::browseModel()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load NAM model", juce::File(), "*.nam");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f.existsAsFile()) { processorRef.loadModelAsync (f); refreshLabels(); }
        });
}

void NAMAudioProcessorEditor::browseIR()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load IR (WAV)", juce::File(), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f.existsAsFile()) { processorRef.loadIRAsync (f); refreshLabels(); }
        });
}

void NAMAudioProcessorEditor::refreshLabels()
{
    auto mp = processorRef.getCurrentModelPath();
    auto ip = processorRef.getCurrentIRPath();
    modelLabel.setText (mp.isEmpty() ? "no model" : juce::File (mp).getFileName(),
                        juce::dontSendNotification);
    irLabel   .setText (ip.isEmpty() ? "no IR"    : juce::File (ip).getFileName(),
                        juce::dontSendNotification);
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
