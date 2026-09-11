// LCDDisplayComponent — implementazione.
#include "LCDDisplayComponent.h"
#include "PluginProcessor.h"
#include "PresetManager.h"

namespace {
    const juce::Colour kLcdBack   { 0xff0a1410 };   // vetro spento
    const juce::Colour kLcdInk    { 0xff7fe0a0 };   // fosforo verde
    const juce::Colour kLcdDim    { 0xff2d5c44 };   // segmenti spenti
    const juce::Colour kBtnOff    { 0xff23352c };
    const juce::Colour kBtnOn     { 0xff2fa35f };
}

LCDDisplayComponent::LCDDisplayComponent (NAMAudioProcessor& proc,
                                          juce::AudioProcessorValueTreeState& state,
                                          PresetManager& mgr)
    : proc_ (proc), apvts_ (state), mgr_ (mgr)
{
    auto styleBtn = [this] (juce::TextButton& b, bool toggles)
    {
        addAndMakeVisible (b);
        b.setClickingTogglesState (toggles);
        b.setColour (juce::TextButton::buttonColourId,   kBtnOff);
        b.setColour (juce::TextButton::buttonOnColourId, kBtnOn);
        b.setColour (juce::TextButton::textColourOffId,  kLcdInk);
        b.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    };

    for (int i = 0; i < 4; ++i)
    {
        styleBtn (bankBtn_[i], true);
        bankBtn_[i].setRadioGroupId (0);   // gestiti a mano: e' un parametro a scelta
        bankBtn_[i].onClick = [this, i] { setParamValue ("preset_bank", (float) i); repaint(); };
    }

    styleBtn (saveBtn_, false);
    saveBtn_.setTooltip ("Salva il preset corrente nel banco selezionato; se esiste, lo sovrascrive");
    saveBtn_.onClick = [this] { doSave(); };

    styleBtn (tapBtn_, false);
    tapBtn_.setTooltip ("Batti il tempo: tre tocchi bastano");
    tapBtn_.onClick = [this] { tapPressed(); };

    styleBtn (metroBtn_, true);
    metroAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts_, "metro_enable", metroBtn_);

    addAndMakeVisible (sigBox_);
    sigBox_.setColour (juce::ComboBox::backgroundColourId, kBtnOff);
    sigBox_.setColour (juce::ComboBox::textColourId, kLcdInk);
    sigBox_.addItemList ({ "4/4", "3/4", "2/4", "6/8", "5/4", "7/8" }, 1);
    sigAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts_, "metro_sig", sigBox_);

    addAndMakeVisible (tempoSlider_);
    tempoSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider_.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    tempoSlider_.setColour (juce::Slider::thumbColourId, kLcdInk);
    tempoSlider_.setColour (juce::Slider::trackColourId, kLcdDim);
    tempoAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts_, "tempo_bpm", tempoSlider_);

    addAndMakeVisible (metroVolSlider_);
    metroVolSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    metroVolSlider_.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    metroVolSlider_.setColour (juce::Slider::thumbColourId, kLcdInk);
    metroVolSlider_.setColour (juce::Slider::trackColourId, kLcdDim);
    metroVolSlider_.setTooltip ("Volume del metronomo, indipendente dall'uscita");
    metroVolAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts_, "metro_volume", metroVolSlider_);

    startTimerHz (30);
}

LCDDisplayComponent::~LCDDisplayComponent() { stopTimer(); }

float LCDDisplayComponent::paramValue (const char* id) const
{
    if (auto* p = apvts_.getRawParameterValue (id)) return p->load();
    return 0.f;
}

void LCDDisplayComponent::setParamValue (const char* id, float v)
{
    if (auto* p = apvts_.getParameter (id))
    {
        const auto norm = p->getNormalisableRange().convertTo0to1 (v);
        p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, norm));
    }
}

// Media degli intervalli recenti. I tocchi piu' vecchi di due secondi non
// contano: ripartire dopo una pausa non deve trascinarsi dietro il tempo
// precedente.
void LCDDisplayComponent::tapPressed()
{
    const auto now = juce::Time::currentTimeMillis();
    if (! tapTimes_.empty() && now - tapTimes_.back() > 2000) tapTimes_.clear();
    tapTimes_.push_back (now);
    while (tapTimes_.size() > 5) tapTimes_.pop_front();

    if (tapTimes_.size() >= 2)
    {
        double sum = 0.0;
        for (size_t i = 1; i < tapTimes_.size(); ++i)
            sum += (double) (tapTimes_[i] - tapTimes_[i - 1]);
        const double meanMs = sum / (double) (tapTimes_.size() - 1);
        if (meanMs > 1.0)
            setParamValue ("tempo_bpm", (float) juce::jlimit (40.0, 300.0, 60000.0 / meanMs));
    }
    blinkOn_ = true; blinkFrames_ = 4;
    repaint();
}

// Il salvataggio dal display usa il nome corrente piu' la lettera del banco.
// Se quel nome esiste gia' viene sovrascritto: e' il comportamento chiesto,
// il banco serve proprio a tenere varianti dello stesso preset.
void LCDDisplayComponent::doSave()
{
    const int bank = juce::jlimit (0, 3, (int) paramValue ("preset_bank"));
    const char bankLetter = (char) ('A' + bank);
    auto base = mgr_.getCurrentName();
    if (base.isEmpty() || PresetManager::isReservedName (base))
    {
        // Su "Default" o senza preset corrente serve un nome nuovo: lo chiede
        // l'editor, che ha la finestrella di testo.
        if (onSaveRequested) onSaveRequested();
        return;
    }
    // Toglie un eventuale suffisso di banco gia' presente, per non accumularli.
    if (base.length() > 4 && base.getLastCharacter() == ']'
        && base.substring (base.length() - 4, base.length() - 1).startsWith ("["))
        base = base.dropLastCharacters (4).trim();

    mgr_.saveAs (base + " [" + juce::String::charToString (bankLetter) + "]");
    repaint();
}

void LCDDisplayComponent::timerCallback()
{
    // Lampeggio del TAP a tempo: la bandiera la alza il metronomo nel thread
    // audio a ogni movimento, anche quando il suono e' spento.
    if (proc_.consumeMetroBeat()) { blinkOn_ = true; blinkFrames_ = 4; }
    else if (blinkFrames_ > 0 && --blinkFrames_ == 0) blinkOn_ = false;

    // Con il metronomo spento il TAP pulsa comunque al tempo impostato, cosi'
    // si vede a colpo d'occhio a che velocita' si sta lavorando.
    if (paramValue ("metro_enable") < 0.5f)
    {
        const double bpm = juce::jmax (40.0f, paramValue ("tempo_bpm"));
        const auto   ms  = juce::Time::currentTimeMillis();
        const double beatMs = 60000.0 / bpm;
        const bool   on = std::fmod ((double) ms, beatMs) < 90.0;
        if (on != blinkOn_) { blinkOn_ = on; }
    }

    tapBtn_.setColour (juce::TextButton::buttonColourId, blinkOn_ ? kBtnOn : kBtnOff);

    // I quattro banchi riflettono il parametro a scelta: acceso solo quello
    // attivo, senza gruppo radio di JUCE che duplicherebbe lo stato.
    const int bank = juce::jlimit (0, 3, (int) paramValue ("preset_bank"));
    for (int i = 0; i < 4; ++i)
        if (bankBtn_[i].getToggleState() != (i == bank))
            bankBtn_[i].setToggleState (i == bank, juce::dontSendNotification);

    repaint();
}

void LCDDisplayComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Vetro dell'LCD con una leggera vignettatura.
    g.setGradientFill (juce::ColourGradient (kLcdBack.brighter (0.10f), r.getTopLeft(),
                                             kLcdBack,                  r.getBottomLeft(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

    // Nome del preset e banco.
    const int bank = juce::jlimit (0, 3, (int) paramValue ("preset_bank"));
    juce::String name = mgr_.getCurrentName();
    if (name.isEmpty()) name = "- - -";
    if (mgr_.isDirty()) name << " *";

    g.setColour (kLcdInk);
    g.setFont (juce::Font (juce::FontOptions (15.0f).withStyle ("Bold")));
    g.drawText (name, nameArea_, juce::Justification::centredLeft, true);

    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.setColour (kLcdDim);
    g.drawText ("BANK " + juce::String::charToString ((juce::juce_wchar) ('A' + bank)),
                bankLabelArea_, juce::Justification::centredLeft);

    // Lettura del tempo.
    g.setColour (kLcdInk);
    g.setFont (juce::Font (juce::FontOptions (15.0f).withStyle ("Bold")));
    g.drawText (juce::String (paramValue ("tempo_bpm"), 1) + " BPM",
                bpmArea_, juce::Justification::centred);

    g.setColour (kLcdDim);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("VOL", metroVolLabelArea_, juce::Justification::centredRight);
}

void LCDDisplayComponent::resized()
{
    auto r = getLocalBounds().reduced (6, 4);

    // Destra: metronomo, volume, metrica.
    metroVolSlider_.setBounds (r.removeFromRight (70).reduced (2, 4));
    metroVolLabelArea_ = r.removeFromRight (26);
    sigBox_  .setBounds (r.removeFromRight (74).reduced (2, 3));   // 56 troncava "4/4" in "..."
    metroBtn_.setBounds (r.removeFromRight (60).reduced (2, 3));
    r.removeFromRight (8);

    // Centro: tempo e TAP.
    tempoSlider_.setBounds (r.removeFromRight (110).reduced (2, 4));
    bpmArea_    = r.removeFromRight (82);
    tapBtn_     .setBounds (r.removeFromRight (52).reduced (2, 3));
    r.removeFromRight (10);

    // Sinistra: nome del preset, banchi, salvataggio.
    saveBtn_.setBounds (r.removeFromRight (54).reduced (2, 3));
    r.removeFromRight (6);
    for (int i = 3; i >= 0; --i) {
        bankBtn_[i].setBounds (r.removeFromRight (26).reduced (1, 4));
    }
    r.removeFromRight (8);
    bankLabelArea_ = r.removeFromBottom (12);
    nameArea_      = r;
}
