// Stage 4 — amp-style rotary knob LookAndFeel.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class NAMLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NAMLookAndFeel();

    void drawRotarySlider (juce::Graphics&,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider&) override;
};
