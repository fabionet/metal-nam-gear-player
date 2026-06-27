#include "PluginEditor.h"

NAMAudioProcessorEditor::NAMAudioProcessorEditor (NAMAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (600, 300);
}

void NAMAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff202020));
    g.setColour (juce::Colours::white);
    g.setFont (20.0f);
    g.drawFittedText ("NAM Custom — Stage 1 skeleton",
                      getLocalBounds(), juce::Justification::centred, 1);
}
