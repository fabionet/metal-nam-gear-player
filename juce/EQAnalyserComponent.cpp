// EQAnalyserComponent — implementazione.
#include "EQAnalyserComponent.h"
#include "JuceFontCompat.h"
#include "PluginProcessor.h"
#include "EQ.h"
#include <cmath>

namespace {
    constexpr double kMinHz =   20.0;
    constexpr double kMaxHz = 20000.0;
    constexpr double kMinDb =  -18.0;
    constexpr double kMaxDb =   18.0;
    constexpr float  kHandleR = 5.0f;

    // Smorzamento del picco per bin: salita immediata, discesa lenta, cosi'
    // lo spettro resta leggibile invece di sfarfallare.
    constexpr float kFallDbPerFrame = 1.6f;
}

const std::array<EQAnalyserComponent::BandInfo, EQAnalyserComponent::kNumBands>&
EQAnalyserComponent::bands()
{
    using EQ = nam_dsp::FiveBandEQ;
    static const std::array<BandInfo, kNumBands> b {{
        { "eq_bass",     nullptr,         nullptr,    EQ::kBassFreqHz,     "BASS",   juce::Colour (0xff4fa3ff) },
        { "eq_mid_gain", "eq_mid_freq",   "eq_mid_q", 0.0,                 "MID",    juce::Colour (0xff7ddc5a) },
        { "eq_presence", nullptr,         nullptr,    EQ::kPresenceFreqHz, "PRES",   juce::Colour (0xffffc14f) },
        { "eq_treble",   nullptr,         nullptr,    EQ::kTrebleFreqHz,   "TREBLE", juce::Colour (0xffff7a4f) },
        { "eq_air",      nullptr,         nullptr,    EQ::kAirFreqHz,      "AIR",    juce::Colour (0xffd68cff) },
    }};
    return b;
}

EQAnalyserComponent::EQAnalyserComponent (NAMAudioProcessor& proc,
                                          juce::AudioProcessorValueTreeState& state)
    : proc_ (proc), apvts_ (state)
{
    scratch_.assign (2 * kFftSize, 0.f);
    binDB_.fill ((float) kMinDb * 4.f);
    setOpaque (false);
    startTimerHz (24);
}

EQAnalyserComponent::~EQAnalyserComponent() { stopTimer(); }

float EQAnalyserComponent::paramValue (const char* id) const
{
    if (auto* p = apvts_.getRawParameterValue (id)) return p->load();
    return 0.f;
}

void EQAnalyserComponent::setParamValue (const char* id, float v)
{
    if (auto* p = apvts_.getParameter (id))
    {
        const auto norm = p->getNormalisableRange().convertTo0to1 (v);
        p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, norm));
    }
}

// --- mappature -------------------------------------------------------------

double EQAnalyserComponent::freqToX (double hz) const
{
    const double t = std::log (juce::jlimit (kMinHz, kMaxHz, hz) / kMinHz)
                   / std::log (kMaxHz / kMinHz);
    return t * getWidth();
}

double EQAnalyserComponent::xToFreq (double x) const
{
    const double t = juce::jlimit (0.0, 1.0, x / juce::jmax (1, getWidth()));
    return kMinHz * std::pow (kMaxHz / kMinHz, t);
}

double EQAnalyserComponent::dbToY (double dB) const
{
    const double t = (juce::jlimit (kMinDb, kMaxDb, dB) - kMaxDb) / (kMinDb - kMaxDb);
    return t * getHeight();
}

double EQAnalyserComponent::yToDb (double y) const
{
    const double t = juce::jlimit (0.0, 1.0, y / juce::jmax (1, getHeight()));
    return kMaxDb + t * (kMinDb - kMaxDb);
}

double EQAnalyserComponent::bandFreq (int b) const
{
    const auto& info = bands()[(size_t) b];
    return info.freqParam != nullptr ? (double) paramValue (info.freqParam) : info.fixedFreq;
}

// --- analisi ---------------------------------------------------------------

void EQAnalyserComponent::timerCallback()
{
    const bool nowActive = paramValue ("eq_bypass") < 0.5f;
    if (nowActive != active_) { active_ = nowActive; }

    proc_.readScope (scratch_.data(), kFftSize);
    std::fill (scratch_.begin() + kFftSize, scratch_.end(), 0.f);
    window_.multiplyWithWindowingTable (scratch_.data(), kFftSize);
    fft_.performFrequencyOnlyForwardTransform (scratch_.data());

    // La finestra di Hann dimezza l'ampiezza media: il fattore 2/N riporta i
    // bin a un'ampiezza confrontabile con il segnale d'ingresso.
    const float norm = 2.0f / (float) kFftSize;
    for (int i = 0; i < kNumBins; ++i)
    {
        const float mag = scratch_[(size_t) i] * norm;
        const float dB  = 20.0f * std::log10 (juce::jmax (mag, 1.0e-7f));
        binDB_[(size_t) i] = dB > binDB_[(size_t) i] ? dB
                                                     : binDB_[(size_t) i] - kFallDbPerFrame;
    }
    repaint();
}

// --- disegno ---------------------------------------------------------------

void EQAnalyserComponent::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff0d0d0d));
    g.fillRoundedRectangle (r, 3.0f);

    // Griglia: decadi e alcune frequenze intermedie, piu' le linee a +/-6 e 12 dB.
    g.setColour (juce::Colour (0xff262626));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = (float) freqToX (f);
        g.drawVerticalLine ((int) x, 0.f, r.getHeight());
    }
    for (double d : { -12.0, -6.0, 0.0, 6.0, 12.0 })
    {
        const float y = (float) dbToY (d);
        g.setColour (d == 0.0 ? juce::Colour (0xff3a3a3a) : juce::Colour (0xff1f1f1f));
        g.drawHorizontalLine ((int) y, 0.f, r.getWidth());
    }

    // Etichette di frequenza, discrete.
    g.setColour (juce::Colour (0xff5a5a5a));
    g.setFont (juce::Font (juce::FontOptions (8.5f)));
    for (auto [f, txt] : { std::pair<double, const char*> { 100.0, "100" },
                           { 1000.0, "1k" }, { 10000.0, "10k" } })
        g.drawText (txt, (int) freqToX (f) + 2, (int) r.getHeight() - 11, 26, 10,
                    juce::Justification::left);

    // Spettro: area piena sotto la linea dei bin.
    {
        const double sr = juce::jmax (8000.0, proc_.getSampleRate());
        juce::Path spec;
        bool started = false;
        for (int i = 1; i < kNumBins; ++i)
        {
            const double hz = (double) i * sr / (double) kFftSize;
            if (hz < kMinHz) continue;
            if (hz > kMaxHz) break;
            // I bin arrivano in dBFS. Il livello del singolo bin e' molto piu'
            // basso di quello complessivo del segnale, quindi la finestra utile
            // sta fra -100 e -20 dBFS: con 0 in cima lo spettro restava
            // schiacciato sul fondo e non si leggeva nulla.
            const double v = juce::jmap ((double) binDB_[(size_t) i], -100.0, -20.0, kMinDb, kMaxDb);
            const float x = (float) freqToX (hz);
            const float y = (float) dbToY (v);
            if (! started) { spec.startNewSubPath (x, y); started = true; }
            else            spec.lineTo (x, y);
        }
        if (started)
        {
            juce::Path filled (spec);
            filled.lineTo (r.getRight(), r.getBottom());
            filled.lineTo ((float) freqToX (kMinHz), r.getBottom());
            filled.closeSubPath();
            g.setColour (juce::Colour (0xff2f6f8f).withAlpha (0.35f));
            g.fillPath (filled);
            g.setColour (juce::Colour (0xff64b6d8).withAlpha (0.75f));
            g.strokePath (spec, juce::PathStrokeType (1.0f));
        }
    }

    // Curva dell'equalizzatore.
    {
        const double sr = juce::jmax (8000.0, proc_.getSampleRate());
        const float bass = paramValue ("eq_bass"),   midF = paramValue ("eq_mid_freq");
        const float midQ = paramValue ("eq_mid_q"),  midG = paramValue ("eq_mid_gain");
        const float pres = paramValue ("eq_presence"), treb = paramValue ("eq_treble");
        const float air  = paramValue ("eq_air");

        curvePath_.clear();
        const int steps = juce::jmax (2, getWidth());
        for (int i = 0; i < steps; ++i)
        {
            const double hz = xToFreq ((double) i);
            const double dB = nam_dsp::FiveBandEQ::responseDB (sr, hz, bass, midF, midQ,
                                                               midG, pres, treb, air);
            const float x = (float) i, y = (float) dbToY (dB);
            if (i == 0) curvePath_.startNewSubPath (x, y);
            else        curvePath_.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xfff2c438).withAlpha (active_ ? 0.95f : 0.3f));
        g.strokePath (curvePath_, juce::PathStrokeType (1.8f));
    }

    // Maniglie delle bande.
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& info = bands()[(size_t) b];
        const float x = (float) freqToX (bandFreq (b));
        const float y = (float) dbToY (paramValue (info.gainParam));
        const bool  hot = (b == dragBand_ || b == hoverBand_);
        const float rad = hot ? kHandleR + 1.5f : kHandleR;

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (x - rad - 1.f, y - rad - 1.f, (rad + 1.f) * 2.f, (rad + 1.f) * 2.f);
        g.setColour (info.colour.withAlpha (active_ ? 1.0f : 0.35f));
        g.fillEllipse (x - rad, y - rad, rad * 2.f, rad * 2.f);

        if (hot)
        {
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
            juce::String t (info.label);
            t << "  " << juce::String (paramValue (info.gainParam), 1) << " dB";
            if (info.freqParam != nullptr)
                t << "  " << juce::String ((int) bandFreq (b)) << " Hz  Q "
                  << juce::String (paramValue (info.qParam), 2);
            const int tw = 150;
            int tx = (int) x + 8;
            if (tx + tw > getWidth()) tx = (int) x - 8 - tw;
            g.drawText (t, tx, (int) y - 14, tw, 12, juce::Justification::left);
        }
    }

    g.setColour (juce::Colour (0xff000000).withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

void EQAnalyserComponent::resized() {}

// --- interazione -----------------------------------------------------------

int EQAnalyserComponent::hitTestHandle (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 12.0f;   // raggio di presa in pixel
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& info = bands()[(size_t) b];
        const juce::Point<float> c ((float) freqToX (bandFreq (b)),
                                    (float) dbToY (paramValue (info.gainParam)));
        const float d = c.getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void EQAnalyserComponent::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitTestHandle (e.position);
    if (h != hoverBand_) { hoverBand_ = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor
                           : juce::MouseCursor::NormalCursor);
}

void EQAnalyserComponent::mouseExit (const juce::MouseEvent&)
{
    if (hoverBand_ != -1) { hoverBand_ = -1; repaint(); }
}

void EQAnalyserComponent::mouseDown (const juce::MouseEvent& e)
{
    dragBand_ = hitTestHandle (e.position);
    if (dragBand_ < 0) return;
    const auto& info = bands()[(size_t) dragBand_];
    if (auto* p = apvts_.getParameter (info.gainParam)) p->beginChangeGesture();
    if (info.freqParam != nullptr)
        if (auto* p = apvts_.getParameter (info.freqParam)) p->beginChangeGesture();
    repaint();
}

void EQAnalyserComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand_ < 0) return;
    const auto& info = bands()[(size_t) dragBand_];
    setParamValue (info.gainParam, (float) yToDb (e.position.y));
    // Solo la banda MID ha la frequenza regolabile: le altre restano dove sono.
    if (info.freqParam != nullptr)
        setParamValue (info.freqParam, (float) xToFreq (e.position.x));
    repaint();
}

void EQAnalyserComponent::mouseUp (const juce::MouseEvent&)
{
    if (dragBand_ < 0) return;
    const auto& info = bands()[(size_t) dragBand_];
    if (auto* p = apvts_.getParameter (info.gainParam)) p->endChangeGesture();
    if (info.freqParam != nullptr)
        if (auto* p = apvts_.getParameter (info.freqParam)) p->endChangeGesture();
    dragBand_ = -1;
    repaint();
}

// Rotellina sulla maniglia del MID: regola il Q, che non ha un asse sul piano.
void EQAnalyserComponent::mouseWheelMove (const juce::MouseEvent& e,
                                          const juce::MouseWheelDetails& w)
{
    const int b = dragBand_ >= 0 ? dragBand_ : hitTestHandle (e.position);
    if (b < 0) return;
    const auto& info = bands()[(size_t) b];
    if (info.qParam == nullptr) return;
    const float q = paramValue (info.qParam) * (1.0f + w.deltaY * 0.6f);
    setParamValue (info.qParam, q);
    repaint();
}
