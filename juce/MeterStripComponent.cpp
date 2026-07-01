#include "MeterStripComponent.h"
#include "GlobalSettings.h"

MeterStripComponent::MeterStripComponent (std::atomic<float>& pL,
                                          std::atomic<float>& pR,
                                          std::function<bool()> isStereoFn)
    : peakL_ (pL), peakR_ (pR), isStereoFn_ (std::move (isStereoFn))
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (kTimerHz);
}

MeterStripComponent::~MeterStripComponent()
{
    stopTimer();
}

static inline float linToDb (float v)
{
    if (v <= 1.0e-7f) return -120.f;
    return 20.0f * std::log10 (v);
}

void MeterStripComponent::timerCallback()
{
    // Read & reset atomics (Processor accumulates max between ticks).
    const float pL = peakL_.exchange (0.f, std::memory_order_relaxed);
    const float pR = peakR_.exchange (0.f, std::memory_order_relaxed);

    const float dbL = linToDb (pL);
    const float dbR = linToDb (pR);

    auto update = [] (float newDb, float& held, int& hold) {
        if (newDb >= held) { held = newDb; hold = kHoldFrames; }
        else if (hold > 0) { --hold; }
        else               { held -= kDecayPerFrame; if (held < -150.f) held = -150.f; }
    };
    update (dbL, dbHeldL_, holdL_);
    update (dbR, dbHeldR_, holdR_);

    repaint();
}

void MeterStripComponent::drawBar (juce::Graphics& g,
                                   juce::Rectangle<int> bar,
                                   float dbHeld,
                                   int depthDb)
{
    // Bezel.
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRect (bar);
    auto inner = bar.reduced (2);
    g.setColour (juce::Colour (0xff141414));
    g.fillRect (inner);

    if (inner.getHeight() <= 4) return;

    const float topDb    = 0.f;
    const float bottomDb = (float) depthDb;          // e.g. -90
    const float range    = topDb - bottomDb;         // 90
    auto dbToY = [&] (float db) {
        const float clamped = juce::jlimit (bottomDb, topDb, db);
        const float t = (topDb - clamped) / range;   // 0 at top, 1 at bottom
        return inner.getY() + (int) std::round (t * inner.getHeight());
    };

    const int yHeld   = dbToY (dbHeld);
    const int yMinus20 = dbToY (-20.f);
    const int yMinus12 = dbToY (-12.f);
    const int yTop    = inner.getY();
    const int yBot    = inner.getBottom();

    // Fill from yHeld down to yBot, segmenting by color zone.
    if (yHeld < yBot)
    {
        // Green segment: from max(yHeld, yMinus20) down to yBot.
        const int gStart = juce::jmax (yHeld, yMinus20);
        if (gStart < yBot) {
            g.setColour (juce::Colour (0xff00cc00));
            g.fillRect (inner.getX(), gStart, inner.getWidth(), yBot - gStart);
        }
        // Yellow segment: from max(yHeld, yMinus12) down to yMinus20.
        if (yHeld < yMinus20) {
            const int yStart = juce::jmax (yHeld, yMinus12);
            const int yEnd   = juce::jmin (yMinus20, yBot);
            if (yStart < yEnd) {
                g.setColour (juce::Colour (0xffcccc00));
                g.fillRect (inner.getX(), yStart, inner.getWidth(), yEnd - yStart);
            }
        }
        // Red segment: from yHeld (clipped to top) down to yMinus12.
        if (yHeld < yMinus12) {
            const int rStart = juce::jmax (yHeld, yTop);
            const int rEnd   = juce::jmin (yMinus12, yBot);
            if (rStart < rEnd) {
                g.setColour (juce::Colour (0xffcc0000));
                g.fillRect (inner.getX(), rStart, inner.getWidth(), rEnd - rStart);
            }
        }
    }

    // Tick marks every 6 dB on right edge.
    g.setColour (juce::Colour (0xff444444));
    for (int db = 0; db >= depthDb; db -= 6) {
        const int y = dbToY ((float) db);
        g.drawHorizontalLine (y, (float) inner.getRight() - 3, (float) inner.getRight());
    }
}

void MeterStripComponent::paint (juce::Graphics& g)
{
    const int depth = GlobalSettings::get().getMeterDepthDb();
    auto r = getLocalBounds();

    const bool stereo = isStereoFn_ && isStereoFn_();
    if (! stereo)
    {
        // Single bar — average L/R held values so a mono signal mirrored to R doesn't double-show.
        const float mono = juce::jmax (dbHeldL_, dbHeldR_);
        drawBar (g, r, mono, depth);
    }
    else
    {
        auto left  = r.removeFromLeft (r.getWidth() / 2);
        r.removeFromLeft (1);
        drawBar (g, left, dbHeldL_, depth);
        drawBar (g, r,    dbHeldR_, depth);
    }
}
