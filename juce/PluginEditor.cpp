#include "PluginEditor.h"

NAMAudioProcessorEditor::NAMAudioProcessorEditor (NAMAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), generic (p)
{
    addAndMakeVisible (generic);
    addAndMakeVisible (loadModelBtn);
    addAndMakeVisible (loadIRBtn);
    addAndMakeVisible (modelLabel);
    addAndMakeVisible (irLabel);

    modelLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    irLabel   .setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    refreshLabels();

    loadModelBtn.onClick = [this] { browseModel(); };
    loadIRBtn   .onClick = [this] { browseIR(); };

    setSize (640, 520);
}

NAMAudioProcessorEditor::~NAMAudioProcessorEditor() = default;

void NAMAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
}

void NAMAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto top = r.removeFromTop (64);
    auto half = top.removeFromLeft (top.getWidth() / 2);
    loadModelBtn.setBounds (half.removeFromTop (28));
    modelLabel  .setBounds (half);
    loadIRBtn   .setBounds (top.removeFromTop (28));
    irLabel     .setBounds (top);
    generic.setBounds (r);
}

void NAMAudioProcessorEditor::browseModel()
{
    chooser = std::make_unique<juce::FileChooser>(
        "Load NAM model", juce::File(), "*.nam");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f.existsAsFile()) { processorRef.loadModelAsync (f); refreshLabels(); }
        });
}

void NAMAudioProcessorEditor::browseIR()
{
    chooser = std::make_unique<juce::FileChooser>(
        "Load IR (WAV)", juce::File(), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f.existsAsFile()) { processorRef.loadIRAsync (f); refreshLabels(); }
        });
}

void NAMAudioProcessorEditor::refreshLabels()
{
    auto mp = processorRef.getCurrentModelPath();
    auto ip = processorRef.getCurrentIRPath();
    modelLabel.setText (mp.isEmpty() ? "no model" : juce::File (mp).getFileName(), juce::dontSendNotification);
    irLabel   .setText (ip.isEmpty() ? "no IR"    : juce::File (ip).getFileName(), juce::dontSendNotification);
}
