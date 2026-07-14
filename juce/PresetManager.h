// Stage 7 — PresetManager: factory (embedded BinaryData) + user (disk) presets.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <functional>
#include <vector>

class NAMAudioProcessor;

class PresetManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    struct PresetRef
    {
        juce::String name;
        juce::String category { "Uncategorized" };
        bool         isFactory = false;
        juce::File   userFile;          // valid only when !isFactory
        int          factoryIndex = -1; // index into BinaryData when isFactory
    };

    PresetManager (NAMAudioProcessor& proc, juce::AudioProcessorValueTreeState& state);
    ~PresetManager() override;

    // Library
    void               refresh();
    const std::vector<PresetRef>& presets() const { return presets_; }

    // Current
    juce::String getCurrentName() const { return currentName_; }
    bool         isDirty()        const { return dirty_; }
    int          getCurrentIndex() const { return currentIndex_; }
    bool         getLockModel()   const { return lockModel_; }
    void         setLockModel (bool b)  { lockModel_ = b; }

    // Operations
    bool save();                                   // save over current user preset
    bool saveAs (const juce::String& name);        // create new user preset
    bool deleteCurrent();                          // delete current user preset
    bool load   (int index);                       // load by index in presets_
    void next();
    void prev();

    // Where user presets live.
    static juce::File userPresetDir();

    // Listener
    std::function<void()> onChanged;

private:
    void parameterChanged (const juce::String&, float) override;
    void markDirty();
    void notify();

    juce::String serialize (const juce::String& name) const;
    bool         applyXml  (const juce::XmlElement& root);

    NAMAudioProcessor& processor_;
    juce::AudioProcessorValueTreeState& apvts_;

    std::vector<PresetRef> presets_;
    juce::String currentName_;
    juce::String currentCategory_ { "Uncategorized" };
    int   currentIndex_ = -1;
    bool  dirty_        = false;
    bool  lockModel_    = false;
    bool  loading_      = false; // suppress dirty during programmatic load

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
