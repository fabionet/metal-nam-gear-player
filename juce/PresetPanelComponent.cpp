// Stage 7 — PresetPanelComponent implementation.
#include "PresetPanelComponent.h"
#include "JuceFontCompat.h"

PresetPanelComponent::PresetPanelComponent (PresetManager& mgr)
    : mgr_ (mgr)
{
    title_.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
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

    for (auto* b : { &saveBtn_, &saveAsBtn_, &deleteBtn_, &prevBtn_, &nextBtn_, &getMoreBtn_,
                     &exportBtn_, &backupBtn_ })
        addAndMakeVisible (b);

    categoryFilter_.addItem ("All",           1);
    categoryFilter_.addItem ("Clean",         2);
    categoryFilter_.addItem ("Rock",          3);
    categoryFilter_.addItem ("Metal",         4);
    categoryFilter_.addItem ("Extreme Metal", 5);
    categoryFilter_.addItem ("Bass",          6);
    categoryFilter_.setSelectedId (1, juce::dontSendNotification);
    categoryFilter_.onChange = [this] { refreshFromManager(); };
    addAndMakeVisible (categoryFilter_);

    saveBtn_  .onClick = [this] { mgr_.save();   };
    saveAsBtn_.onClick = [this] { doSaveAs();    };
    deleteBtn_.onClick = [this] { mgr_.deleteCurrent(); };
    prevBtn_  .onClick = [this] { mgr_.prev();   };
    nextBtn_  .onClick = [this] { mgr_.next();   };
    // Prima apriva soltanto un sito nel browser, cosa che in molti ambienti non
    // produce alcun effetto visibile. Ora importa davvero: .prs per un preset
    // singolo, .prstl per un elenco che ne contiene piu' di uno.
    getMoreBtn_.onClick = [this] {
        chooser_ = std::make_unique<juce::FileChooser> (
            "Importa preset (.prs, .prstl)",
            PresetManager::userPresetDir(),
            "*.prs;*.prstl;*.nampreset");
        chooser_->launchAsync (juce::FileBrowserComponent::openMode
                                 | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::canSelectMultipleItems,
            [this] (const juce::FileChooser& fc) {
                int total = 0;
                for (auto& f : fc.getResults()) total += mgr_.importFrom (f);
                refreshFromManager();
                juce::NativeMessageBox::showMessageBoxAsync (
                    juce::MessageBoxIconType::InfoIcon,
                    "Importazione preset",
                    total > 0 ? (juce::String (total) + (total == 1 ? " preset importato."
                                                                   : " preset importati."))
                              : "Nessun preset importato: il file non contiene preset leggibili.");
            });
    };

    // Esportazione del preset corrente come .prs: un solo <NAMPreset>, senza
    // asset, pensato per passare una singola regolazione a qualcun altro.
    exportBtn_.onClick = [this] {
        const auto name = mgr_.getCurrentName().isNotEmpty() ? mgr_.getCurrentName()
                                                             : juce::String ("preset");
        chooser_ = std::make_unique<juce::FileChooser> (
            "Esporta il preset corrente",
            PresetManager::userPresetDir().getChildFile (name + ".prs"), "*.prs");
        chooser_->launchAsync (juce::FileBrowserComponent::saveMode
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                if (! f.hasFileExtension ("prs")) f = f.withFileExtension ("prs");
                const bool ok = mgr_.exportCurrent (f);
                juce::NativeMessageBox::showMessageBoxAsync (
                    ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                    "Esporta preset",
                    ok ? ("Preset esportato in " + f.getFullPathName())
                       : juce::String ("Esportazione non riuscita: nessun preset selezionato."));
            });
    };

    // Backup completo come .prstl: tutti i preset piu' i file .nam e .wav che
    // vi compaiono, incorporati in base64. Un solo file da conservare.
    backupBtn_.onClick = [this] {
        chooser_ = std::make_unique<juce::FileChooser> (
            "Backup della lista preset",
            PresetManager::userPresetDir().getChildFile ("backup-preset.prstl"), "*.prstl");
        chooser_->launchAsync (juce::FileBrowserComponent::saveMode
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                if (! f.hasFileExtension ("prstl")) f = f.withFileExtension ("prstl");
                const int n = mgr_.exportAll (f);
                juce::NativeMessageBox::showMessageBoxAsync (
                    n > 0 ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                    "Backup lista preset",
                    n > 0 ? (juce::String (n) + " preset salvati in " + f.getFileName()
                             + ", con i file NAM e IR inclusi.")
                          : juce::String ("Backup non riuscito."));
            });
    };

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

    categoryFilter_.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);

    auto navRow = r.removeFromTop (28);
    prevBtn_.setBounds (navRow.removeFromLeft (40));
    navRow.removeFromLeft (4);
    nextBtn_.setBounds (navRow.removeFromLeft (40));
    r.removeFromTop (6);

    getMoreBtn_.setBounds (r.removeFromBottom (26));
    r.removeFromBottom (4);
    backupBtn_ .setBounds (r.removeFromBottom (26));
    r.removeFromBottom (4);
    exportBtn_ .setBounds (r.removeFromBottom (26));
    r.removeFromBottom (6);

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
    return (int) visibleIndices_.size();
}

int PresetPanelComponent::presetIndexForRow (int row) const
{
    if (row < 0 || row >= (int) visibleIndices_.size()) return -1;
    return visibleIndices_[(size_t) row];
}

void PresetPanelComponent::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    const int idx = presetIndexForRow (row);
    if (idx < 0 || idx >= (int) mgr_.presets().size()) return;
    const auto& ref = mgr_.presets()[(size_t) idx];

    if (selected)
        g.fillAll (juce::Colour (0xff8a5a00));
    else if (idx == mgr_.getCurrentIndex())
        g.fillAll (juce::Colour (0xff3a2a00));

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));

    juce::String prefix = ref.isFactory ? "[F] " : "    ";
    g.drawText (prefix + ref.name, 6, 0, w - 8, h, juce::Justification::centredLeft, true);
}

// Il clic singolo seleziona soltanto: serve a scegliere il bersaglio di
// esporta, elimina e sovrascrivi senza cambiare il suono sotto le dita.
// Il richiamo vero e' sul doppio clic.
void PresetPanelComponent::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    selectedRow_ = row;
    list_.selectRow (row, true, true);
    repaint();
}

void PresetPanelComponent::rebuildVisibleIndices()
{
    visibleIndices_.clear();
    const auto& list = mgr_.presets();
    const juce::String sel = categoryFilter_.getText();
    const bool showAll = sel.isEmpty() || sel == "All";
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (showAll || list[i].category.equalsIgnoreCase (sel))
            visibleIndices_.push_back ((int) i);
    }
}

void PresetPanelComponent::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    selectedRow_ = row;
    const int idx = presetIndexForRow (row);
    if (idx >= 0) mgr_.load (idx);
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
    rebuildVisibleIndices();
    updateEnableState();
    list_.updateContent();
    const int cur = mgr_.getCurrentIndex();
    if (cur >= 0)
    {
        for (size_t r = 0; r < visibleIndices_.size(); ++r)
            if (visibleIndices_[r] == cur) { list_.selectRow ((int) r, false, true); break; }
    }
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
