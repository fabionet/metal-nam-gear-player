// LCDDisplayComponent — pannello a cristalli liquidi nell'intestazione.
//
// Mostra il preset selezionato con i quattro banchi A B C D, un tasto di
// salvataggio che puo' sovrascrivere, il TAP lampeggiante a tempo, la lettura
// del tempo e il metronomo con volume proprio.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <deque>
#include "DotMatrixFont.h"

class NAMAudioProcessor;
class PresetManager;

class LCDDisplayComponent : public juce::Component,
                            private juce::Timer
{
public:
    LCDDisplayComponent (NAMAudioProcessor& proc,
                         juce::AudioProcessorValueTreeState& state,
                         PresetManager& mgr);
    ~LCDDisplayComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Richiamata dall'editor quando serve un salvataggio con richiesta di nome.
    std::function<void()> onSaveRequested;

private:
    void timerCallback() override;
    void tapPressed();
    void doSave();

    float paramValue (const char* id) const;
    void  setParamValue (const char* id, float v);

    NAMAudioProcessor&                  proc_;
    juce::AudioProcessorValueTreeState& apvts_;
    PresetManager&                      mgr_;

    // Il costruttore di TextButton e' explicit: le graffe annidate non
    // bastano, servono le chiamate esplicite.
    juce::TextButton bankBtn_[4] { juce::TextButton ("A"), juce::TextButton ("B"),
                                   juce::TextButton ("C"), juce::TextButton ("D") };
    juce::TextButton saveBtn_    { "SAVE" };
    juce::TextButton tapBtn_     { "TAP" };
    juce::TextButton metroBtn_   { "METRO" };
    juce::ComboBox   sigBox_;
    juce::Slider     tempoSlider_, metroVolSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAtt_, metroVolAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> metroAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sigAtt_;

    // Riquadri calcolati in resized() e usati da paint().
    juce::Rectangle<int> nameArea_, bankLabelArea_, bpmArea_, metroVolLabelArea_;

    // Matrice di punti: testo mostrato, scorrimento e disegno.
    juce::String matrixText() const;
    void drawDotMatrix (juce::Graphics&, juce::Rectangle<int> area, const juce::String&);
    double scrollDots_ = 0.0;   // sfasamento in colonne di punti
    int    lastTextW_  = 0;     // larghezza dell'ultimo testo, in colonne

    std::deque<juce::int64> tapTimes_;   // millisecondi degli ultimi tocchi
    bool  blinkOn_   = false;
    int   blinkFrames_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LCDDisplayComponent)
};
