// Stage 8 — Global, per-user preferences persisted outside APVTS/preset state.
#pragma once

#include <juce_data_structures/juce_data_structures.h>

class GlobalSettings
{
public:
    static GlobalSettings& get();

    // Meter depth in dB: one of -60, -90 (default), -120.
    int  getMeterDepthDb() const;
    void setMeterDepthDb (int v);

    // UI scale in percent: one of 25, 50, 75, 100 (default), 150, 200.
    int  getUiScalePercent() const;
    void setUiScalePercent (int v);

    // Last directory used in the model / IR file chooser (Stage 12).
    juce::String getLastModelDir() const;
    void         setLastModelDir (const juce::String& path);
    juce::String getLastIRDir() const;
    void         setLastIRDir (const juce::String& path);

private:
    GlobalSettings();
    juce::ApplicationProperties props_;
};
