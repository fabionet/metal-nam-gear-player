// Stage 4 — amp-style rotary knob LookAndFeel.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class NAMLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NAMLookAndFeel();

    // Il numero sotto al pomello lo disegna una Label creata dal look and feel:
    // qui le si da' un corpo leggibile, perche' quello predefinito e' tarato su
    // interfacce chiare e su questo fondo scuro si perde.
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawRotarySlider (juce::Graphics&,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider&) override;
};
