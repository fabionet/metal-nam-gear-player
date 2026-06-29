// Stage 8 — Vertical input/output level meter with peak-hold ballistics.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <functional>

class MeterStripComponent : public juce::Component,
                            private juce::Timer
{
public:
    MeterStripComponent (std::atomic<float>& peakL,
                         std::atomic<float>& peakR,
                         std::function<bool()> isStereoFn);
    ~MeterStripComponent() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void drawBar (juce::Graphics&, juce::Rectangle<int> bar, float dbHeld, int depthDb);

    std::atomic<float>& peakL_;
    std::atomic<float>& peakR_;
    std::function<bool()> isStereoFn_;

    // Peak-hold + decay state (dB), updated at 30 Hz.
    float dbHeldL_   = -120.f;
    float dbHeldR_   = -120.f;
    int   holdL_     = 0;
    int   holdR_     = 0;

    static constexpr int   kTimerHz       = 30;
    static constexpr int   kHoldFrames    = 45;            // ~1.5 s
    static constexpr float kDecayPerFrame = 20.f / kTimerHz; // 20 dB/s (positive value subtracted)
};
