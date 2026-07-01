// Stage 7 — PresetPanelComponent implementation.
#include "PresetPanelComponent.h"

PresetPanelComponent::PresetPanelComponent (PresetManager& mgr)
    : mgr_ (mgr)
{
    title_.setFont (juce::Font (18.0f, juce::Font::bold));
    title_.setColour (juce::Label::textColourId, juce::Colours::white);
    title_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (title_);

    currentLbl_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    currentLbl_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (currentLbl_);

    lockBox_.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    lockBox_.setToggleState (mgr_.getLockModel(), juce::dontSendNotification);
    lockBox_.onClick = [this] { mgr_.setLockModel (lockBox_.getToggleState()); };
    addAndMakeVisible (lockBox_);

    list_.setModel (this);
    list_.setRowHeight (22);
    list_.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff141414));
    list_.setColour (juce::ListBox::outlineColourId,    juce::Colour (0xff3a3a3a));
    list_.setOutlineThickness (1);
    addAndMakeVisible (list_);

    for (auto* b : { &saveBtn_, &saveAsBtn_, &deleteBtn_, &prevBtn_, &nextBtn_ })
        addAndMakeVisible (b);

    saveBtn_  .onClick = [this] { mgr_.save();   };
    saveAsBtn_.onClick = [this] { doSaveAs();    };
    deleteBtn_.onClick = [this] { mgr_.deleteCurrent(); };
    prevBtn_  .onClick = [this] { mgr_.prev();   };
    nextBtn_  .onClick = [this] { mgr_.next();   };

    mgr_.onChanged = [this] { refreshFromManager(); };
    refreshFromManager();
}

PresetPanelComponent::~PresetPanelComponent()
{
    mgr_.onChanged = nullptr;
}

void PresetPanelComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient cg (juce::Colour (0xff2b2b2b), r.getTopLeft(),
                             juce::Colour (0xff141414), r.getBottomLeft(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

void PresetPanelComponent::resized()
{
    auto r = getLocalBounds().reduced (10);
    title_.setBounds (r.removeFromTop (28));
    r.removeFromTop (4);
    currentLbl_.setBounds (r.removeFromTop (20));
    r.removeFromTop (4);
    lockBox_.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);

    auto navRow = r.removeFromTop (28);
    prevBtn_.setBounds (navRow.removeFromLeft (40));
    navRow.removeFromLeft (4);
    nextBtn_.setBounds (navRow.removeFromLeft (40));
    r.removeFromTop (6);

    auto btnRow = r.removeFromBottom (30);
    auto third = btnRow.getWidth() / 3;
    saveBtn_  .setBounds (btnRow.removeFromLeft (third).reduced (2, 0));
    saveAsBtn_.setBounds (btnRow.removeFromLeft (third).reduced (2, 0));
    deleteBtn_.setBounds (btnRow.reduced (2, 0));
    r.removeFromBottom (6);

    list_.setBounds (r);
}

int PresetPanelComponent::getNumRows()
{
    return (int) mgr_.presets().size();
}

void PresetPanelComponent::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) mgr_.presets().size()) return;
    const auto& ref = mgr_.presets()[(size_t) row];

    if (selected)
        g.fillAll (juce::Colour (0xff8a5a00));
    else if (row == mgr_.getCurrentIndex())
        g.fillAll (juce::Colour (0xff3a2a00));

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (13.0f));

    juce::String prefix = ref.isFactory ? "[F] " : "    ";
    g.drawText (prefix + ref.name, 6, 0, w - 8, h, juce::Justification::centredLeft, true);
}

void PresetPanelComponent::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) mgr_.presets().size())
        mgr_.load (row);
}

void PresetPanelComponent::listBoxItemDoubleClicked (int row, const juce::MouseEvent& e)
{
    listBoxItemClicked (row, e);
}

void PresetPanelComponent::doSaveAs()
{
    nameDialog_ = std::make_unique<juce::AlertWindow> ("Save Preset As",
                                                       "Preset name:",
                                                       juce::AlertWindow::NoIcon);
    nameDialog_->addTextEditor ("name", mgr_.getCurrentName(), {});
    nameDialog_->addButton ("OK",     1, juce::KeyPress (juce::KeyPress::returnKey));
    nameDialog_->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    nameDialog_->enterModalState (true,
        juce::ModalCallbackFunction::create ([this] (int result)
        {
            if (result == 1 && nameDialog_ != nullptr)
            {
                auto name = nameDialog_->getTextEditorContents ("name");
                mgr_.saveAs (name);
            }
            nameDialog_.reset();
        }), false);
}

void PresetPanelComponent::refreshFromManager()
{
    auto name = mgr_.getCurrentName();
    if (name.isEmpty()) name = "(unsaved)";
    currentLbl_.setText (juce::String ("Current: ") + name + (mgr_.isDirty() ? " *" : ""),
                         juce::dontSendNotification);
    lockBox_.setToggleState (mgr_.getLockModel(), juce::dontSendNotification);
    updateEnableState();
    list_.updateContent();
    if (mgr_.getCurrentIndex() >= 0)
        list_.selectRow (mgr_.getCurrentIndex(), false, true);
    repaint();
}

void PresetPanelComponent::updateEnableState()
{
    const int idx = mgr_.getCurrentIndex();
    const bool hasCurrent = idx >= 0;
    const bool isFactory  = hasCurrent && mgr_.presets()[(size_t) idx].isFactory;
    saveBtn_  .setEnabled (hasCurrent && ! isFactory);
    deleteBtn_.setEnabled (hasCurrent && ! isFactory);
}
