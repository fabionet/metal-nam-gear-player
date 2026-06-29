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

private:
    GlobalSettings();
    juce::ApplicationProperties props_;
};
