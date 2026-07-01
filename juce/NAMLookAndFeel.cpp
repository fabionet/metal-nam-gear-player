#include "NAMLookAndFeel.h"

NAMLookAndFeel::NAMLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,   juce::Colours::lightgrey);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,           juce::Colours::lightgrey);
    setColour (juce::ComboBox::backgroundColourId,  juce::Colour (0xff222222));
    setColour (juce::ComboBox::textColourId,        juce::Colours::lightgrey);
    setColour (juce::ComboBox::outlineColourId,     juce::Colour (0xff555555));
    setColour (juce::ToggleButton::textColourId,    juce::Colours::lightgrey);
    setColour (juce::ToggleButton::tickColourId,    juce::Colour (0xffd9a200));
    setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff555555));
    setColour (juce::TextButton::buttonColourId,    juce::Colour (0xff2a2a2a));
    setColour (juce::TextButton::textColourOffId,   juce::Colours::lightgrey);
}

void NAMLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                       int x, int y, int w, int h,
                                       float pos,
                                       float startAngle, float endAngle,
                                       juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float arcRadius = radius - 4.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Background arc.
    {
        juce::Path p;
        p.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                         0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff333333));
        g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // Value arc.
    {
        juce::Path p;
        p.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                         0.0f, startAngle, angle, true);
        g.setColour (juce::Colour (0xffd9a200));
        g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // Knob body.
    const float bodyR = arcRadius - 7.0f;
    g.setColour (juce::Colour (0xff222222));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colour (0xff555555));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // Indicator.
    juce::Path ind;
    const float tipR = bodyR - 2.0f;
    const float innerR = bodyR * 0.35f;
    ind.startNewSubPath (centre.x + innerR * std::sin (angle),
                         centre.y - innerR * std::cos (angle));
    ind.lineTo          (centre.x + tipR   * std::sin (angle),
                         centre.y - tipR   * std::cos (angle));
    g.setColour (juce::Colours::white);
    g.strokePath (ind, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}
