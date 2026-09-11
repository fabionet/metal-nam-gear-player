// Stage 7 — Side panel browser for PresetManager.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetManager.h"
#include <vector>

class PresetPanelComponent : public juce::Component,
                             private juce::ListBoxModel
{
public:
    explicit PresetPanelComponent (PresetManager& mgr);
    ~PresetPanelComponent() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

    void refreshFromManager();

private:
    // ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    int  selectedRow_ = -1;   // riga scelta col clic singolo

    void doSaveAs();
    void updateEnableState();
    void rebuildVisibleIndices();
    int  presetIndexForRow (int row) const;

    PresetManager& mgr_;

    juce::Label        title_      { {}, "PRESETS" };
    juce::Label        currentLbl_;
    juce::ToggleButton lockBox_    { "Lock model on load" };
    juce::ComboBox     categoryFilter_;
    juce::ListBox      list_;
    juce::TextButton   saveBtn_    { "SAVE" };
    juce::TextButton   saveAsBtn_  { "SAVE AS..." };
    juce::TextButton   deleteBtn_  { "DELETE" };
    juce::TextButton   prevBtn_    { "<" };
    juce::TextButton   nextBtn_    { ">" };
    juce::TextButton   getMoreBtn_  { "Importa preset..." };
    juce::TextButton   exportBtn_   { "Esporta .prs" };
    juce::TextButton   backupBtn_   { "Backup lista .prstl" };
    std::unique_ptr<juce::FileChooser> chooser_;

    std::vector<int>   visibleIndices_;

    std::unique_ptr<juce::AlertWindow> nameDialog_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetPanelComponent)
};
