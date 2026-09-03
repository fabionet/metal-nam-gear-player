#include "MeterStripComponent.h"
#include "GlobalSettings.h"

MeterStripComponent::MeterStripComponent (std::atomic<float>& pL,
                                          std::atomic<float>& pR,
                                          std::function<bool()> isStereoFn)
    : peakL_ (pL), peakR_ (pR), isStereoFn_ (std::move (isStereoFn))
{
    setInterceptsMouseClicks (false, false);
    // Il processor accumula il massimo fra un tick e l'altro e non ha modo di
    // azzerarlo da solo: mentre l'editor era chiuso l'atomico ha continuato a
    // salire fino al picco assoluto. Scartiamo quel valore, altrimenti il primo
    // frame dopo la riapertura sbatte la barra al massimo storico.
    peakL_.exchange (0.f, std::memory_order_relaxed);
    peakR_.exchange (0.f, std::memory_order_relaxed);
    startTimerHz (kTimerHz);
}

MeterStripComponent::~MeterStripComponent()
{
    stopTimer();
}

static inline float linToDb (float v)
{
    if (v <= 1.0e-7f) return -150.f;
    return 20.0f * std::log10 (v);
}

// Scala non lineare: meta' barra per i primi 20 dB, dove si lavora davvero.
// Con una mappatura lineare su 90 dB un segnale a -40 dBFS riempie oltre meta'
// del meter e sembra molto piu' caldo di quello che e'.
static float dbToNorm (float db, float floorDb)
{
    if (db >= 0.f)   return 0.f;
    if (db <= floorDb) return 1.f;
    if (db >= -20.f)  return (0.f - db) / 20.f * 0.50f;
    if (db >= -50.f)  return 0.50f + (-20.f - db) / 30.f * 0.30f;
    return 0.80f + (-50.f - db) / (-50.f - floorDb) * 0.20f;
}

void MeterStripComponent::advance (Channel& c, float newDb, bool over)
{
    // Barra: sale subito, scende in modo continuo. Niente tenuta, era la tenuta
    // a far sembrare il meter a scatti — restava congelata 1,5 s e poi crollava.
    c.bar = (newDb >= c.bar) ? newDb
                             : juce::jmax (newDb, c.bar - kBarRelease);
    if (c.bar < kFloorDb) c.bar = kFloorDb;

    // Marcatore di picco: e' lui a tenere e poi scendere, non la barra.
    if (newDb >= c.peak) { c.peak = newDb; c.hold = kHoldFrames; }
    else if (c.hold > 0) { --c.hold; }
    else                 { c.peak = juce::jmax (newDb, c.peak - kPeakRelease); }
    if (c.peak < kFloorDb) c.peak = kFloorDb;

    if (over)          c.clip = kClipFrames;
    else if (c.clip>0) --c.clip;
}

void MeterStripComponent::timerCallback()
{
    const float pL = peakL_.exchange (0.f, std::memory_order_relaxed);
    const float pR = peakR_.exchange (0.f, std::memory_order_relaxed);

    advance (chL_, linToDb (pL), pL >= 0.999f);
    advance (chR_, linToDb (pR), pR >= 0.999f);

    repaint();
}

void MeterStripComponent::drawBar (juce::Graphics& g,
                                   juce::Rectangle<int> bar,
                                   const Channel& c,
                                   int depthDb)
{
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRect (bar);
    auto inner = bar.reduced (2);
    g.setColour (juce::Colour (0xff141414));
    g.fillRect (inner);

    if (inner.getHeight() <= 4) return;

    const float floorDb = (float) depthDb;
    // In verticale 0 dB sta in alto e la norma cresce verso il basso; in
    // orizzontale la stessa norma va letta da destra verso sinistra.
    const int span = horizontal_ ? inner.getWidth() : inner.getHeight();
    const int base = horizontal_ ? inner.getX()     : inner.getY();
    auto dbToY = [&] (float db) {
        const float nrm = dbToNorm (db, floorDb);
        return horizontal_ ? base + (int) std::round ((1.f - nrm) * span)
                           : base + (int) std::round (nrm * span);
    };

    const int yBar    = dbToY (c.bar);
    const int yYellow = dbToY (kYellowDb);
    const int yRed    = dbToY (kRedDb);
    // "hi" = estremo del fondo scala, "lo" = estremo del silenzio.
    const int hi = horizontal_ ? inner.getRight() : inner.getY();
    const int lo = horizontal_ ? inner.getX()     : inner.getBottom();

    auto fillSeg = [&] (int from, int to, juce::Colour col) {
        if (horizontal_) {
            const int a = juce::jmin (from, hi), b = juce::jmax (to, lo);
            if (b < a) { g.setColour (col); g.fillRect (b, inner.getY(), a - b, inner.getHeight()); }
        } else {
            const int a = juce::jmax (from, hi), b = juce::jmin (to, lo);
            if (a < b) { g.setColour (col); g.fillRect (inner.getX(), a, inner.getWidth(), b - a); }
        }
    };

    if (horizontal_ ? (yBar > lo) : (yBar < lo))
    {
        fillSeg (horizontal_ ? juce::jmin (yBar, yYellow) : juce::jmax (yBar, yYellow), lo,
                 juce::Colour (0xff00cc00));
        if (c.bar > kYellowDb)
            fillSeg (horizontal_ ? juce::jmin (yBar, yRed) : juce::jmax (yBar, yRed), yYellow,
                     juce::Colour (0xffcccc00));
        if (c.bar > kRedDb)
            fillSeg (yBar, yRed, juce::Colour (0xffcc0000));
    }

    // Marcatore di picco: linea sottile, e' questa a "tenere".
    if (c.peak > floorDb)
    {
        g.setColour (c.peak > kRedDb ? juce::Colour (0xffff5555)
                                     : juce::Colour (0xffdddddd));
        if (horizontal_) {
            const int x = juce::jlimit (inner.getX(), inner.getRight() - 2, dbToY (c.peak));
            g.fillRect (x, inner.getY(), 2, inner.getHeight());
        } else {
            const int y = juce::jlimit (inner.getY(), inner.getBottom() - 2, dbToY (c.peak));
            g.fillRect (inner.getX(), y, inner.getWidth(), 2);
        }
    }

    // Indicatore di clip: blocco pieno in cima, si spegne da solo.
    if (c.clip > 0)
    {
        g.setColour (juce::Colour (0xffff2020));
        if (horizontal_) g.fillRect (inner.getRight() - 3, inner.getY(), 3, inner.getHeight());
        else             g.fillRect (inner.getX(), inner.getY(), inner.getWidth(), 3);
    }

    // Tacche ogni 6 dB (non uniformi: la scala non e' lineare).
    g.setColour (juce::Colour (0xff444444));
    for (int db = 0; db >= depthDb; db -= 6) {
        const int p = dbToY ((float) db);
        if (horizontal_) g.drawVerticalLine   (p, (float) inner.getBottom() - 3, (float) inner.getBottom());
        else             g.drawHorizontalLine (p, (float) inner.getRight()  - 3, (float) inner.getRight());
    }
}

void MeterStripComponent::paint (juce::Graphics& g)
{
    const int depth = GlobalSettings::get().getMeterDepthDb();
    auto r = getLocalBounds();

    const bool stereo = isStereoFn_ && isStereoFn_();
    if (! stereo)
    {
        // Barra singola: il piu' alto fra i due, cosi' un segnale mono
        // rispecchiato su R non viene mostrato due volte.
        Channel mono;
        mono.bar  = juce::jmax (chL_.bar,  chR_.bar);
        mono.peak = juce::jmax (chL_.peak, chR_.peak);
        mono.clip = juce::jmax (chL_.clip, chR_.clip);
        drawBar (g, r, mono, depth);
    }
    else
    {
        auto left = r.removeFromLeft (r.getWidth() / 2);
        r.removeFromLeft (1);
        drawBar (g, left, chL_, depth);
        drawBar (g, r,    chR_, depth);
    }
}
