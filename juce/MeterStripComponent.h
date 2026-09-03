// Stage 8 — Vertical input/output level meter.
// Fast bar with continuous release + separate peak-hold marker + clip latch.
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

    // Orientamento: verticale (default) o orizzontale, con la barra che
    // cresce da sinistra a destra. La balistica resta la stessa.
    void setHorizontal (bool h) { horizontal_ = h; repaint(); }

private:
    struct Channel
    {
        float bar   = -150.f;   // barra veloce: attacco istantaneo, rilascio continuo
        float peak  = -150.f;   // marcatore di picco: tiene, poi scende
        int   hold  = 0;        // frame residui di tenuta del marcatore
        int   clip  = 0;        // frame residui dell'indicatore di clip
    };

    void timerCallback() override;
    void advance (Channel&, float newDb, bool over);
    void drawBar (juce::Graphics&, juce::Rectangle<int> bar, const Channel&, int depthDb);

    std::atomic<float>& peakL_;
    std::atomic<float>& peakR_;
    std::function<bool()> isStereoFn_;

    Channel chL_, chR_;
    bool    horizontal_ = false;

    static constexpr int   kTimerHz     = 60;
    static constexpr int   kHoldFrames  = kTimerHz * 3 / 2;   // 1.5 s di tenuta del marcatore
    static constexpr int   kClipFrames  = kTimerHz * 5 / 2;   // 2.5 s di indicatore clip
    static constexpr float kBarRelease  = 26.f / kTimerHz;    // 26 dB/s: scende ma resta leggibile
    static constexpr float kPeakRelease = 12.f / kTimerHz;    // 12 dB/s dopo la tenuta
    static constexpr float kFloorDb     = -150.f;

    // Soglie dei colori: il rosso e' gli ultimi 6 dB prima del fondo scala.
    static constexpr float kYellowDb = -18.f;
    static constexpr float kRedDb    =  -6.f;
};
