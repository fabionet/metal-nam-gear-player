// EQAnalyserComponent — implementazione.
#include "EQAnalyserComponent.h"
#include "PluginProcessor.h"
#include "JuceFontCompat.h"
#include "EQ.h"
#include "PedalRegistry.h"
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

// Ricostruisce la tabella delle bande secondo il modello scelto nel menu della
// sezione EQ. L'analizzatore resta sempre lo stesso: cambiano solo le maniglie
// e la curva che ci disegna sopra.
void EQAnalyserComponent::rebuildBands()
{
    using EQ = nam_dsp::FiveBandEQ;
    static const juce::Colour kCol[8] = {
        juce::Colour (0xff4fa3ff), juce::Colour (0xff7ddc5a), juce::Colour (0xffffc14f),
        juce::Colour (0xffff7a4f), juce::Colour (0xffd68cff), juce::Colour (0xff5ad8d8),
        juce::Colour (0xffe8e07a), juce::Colour (0xffff8fb0)
    };

    bands_.clear();
    const auto& m = pedal::at (modelIdx_);

    if (m.topo == pedal::Topology::EqNative)
    {
        // Solo MID ha frequenza e Q regolabili; le altre stanno ferme in
        // frequenza e si muovono soltanto in verticale.
        bands_.push_back ({ "eq_bass",     "",            "",         EQ::kBassFreqHz,     0.9, "BASS",   kCol[0] });
        bands_.push_back ({ "eq_mid_gain", "eq_mid_freq", "eq_mid_q", 0.0,                 1.0, "MID",    kCol[1] });
        bands_.push_back ({ "eq_presence", "",            "",         EQ::kPresenceFreqHz, 0.9, "PRES",   kCol[2] });
        bands_.push_back ({ "eq_treble",   "",            "",         EQ::kTrebleFreqHz,   0.9, "TREBLE", kCol[3] });
        bands_.push_back ({ "eq_air",      "",            "",         EQ::kAirFreqHz,      0.9, "AIR",    kCol[4] });
        return;
    }

    if (m.topo == pedal::Topology::EqGraphic7)
    {
        static const double kF[7] = { 100.0, 200.0, 400.0, 800.0, 1600.0, 3200.0, 6400.0 };
        for (int i = 0; i < 7; ++i) {
            const auto& k = m.knobs[i];
            BandInfo b;
            b.gainParam  = "eq_p" + juce::String (i + 1);
            b.fixedFreq  = kF[i];
            b.fixedQ     = 1.4;
            b.label      = k.label;
            b.colour     = kCol[i % 8];
            b.normalised = true;
            bands_.push_back (b);
        }
        return;
    }

    if (m.topo == pedal::Topology::EqParametric)
    {
        // Coppie guadagno/frequenza: p1-p2 per la bassa, p3-p4 per l'alta.
        for (int i = 0; i < 2; ++i) {
            const auto& kf = m.knobs[i * 2 + 1];
            BandInfo b;
            b.gainParam  = "eq_p" + juce::String (i * 2 + 1);
            b.freqParam  = "eq_p" + juce::String (i * 2 + 2);
            b.fixedQ     = 1.0;
            b.label      = (i == 0) ? "LOW" : "HIGH";
            b.colour     = kCol[i == 0 ? 0 : 3];
            b.normalised = true;
            b.fixedFreq  = kf.def;
            bands_.push_back (b);
        }
        // Gli estremi della frequenza servono al disegno: li tiene il modello.
        bands_[0].fixedFreq = m.knobs[1].def;
        bands_[1].fixedFreq = m.knobs[3].def;
        return;
    }
}

// I parametri nativi sono gia' in unita' reali, quelli della riserva sono
// normalizzati 0..1 sull'intervallo del modello.
float EQAnalyserComponent::bandParam (const BandInfo& info, const juce::String& id) const
{
    const float raw = paramValue (id.toRawUTF8());
    if (! info.normalised) return raw;
    // Frequenza e guadagno hanno intervalli diversi: si risale dal pomello.
    const auto& m = pedal::at (modelIdx_);
    const int   n = id.getTrailingIntValue() - 1;      // eq_pN -> N-1
    if (n < 0 || n >= m.numKnobs) return raw;
    const auto& k = m.knobs[n];
    return k.min + raw * (k.max - k.min);
}

void EQAnalyserComponent::setBandParam (const BandInfo& info, const juce::String& id, float real)
{
    if (! info.normalised) { setParamValue (id.toRawUTF8(), real); return; }
    const auto& m = pedal::at (modelIdx_);
    const int   n = id.getTrailingIntValue() - 1;
    if (n < 0 || n >= m.numKnobs) return;
    const auto& k = m.knobs[n];
    const float norm = (k.max > k.min) ? (real - k.min) / (k.max - k.min) : 0.f;
    setParamValue (id.toRawUTF8(), juce::jlimit (0.f, 1.f, norm));
}

float EQAnalyserComponent::bandGainDB (int b) const
{
    const auto& info = bands_[(size_t) b];
    return bandParam (info, info.gainParam);
}

void EQAnalyserComponent::setBandGainDB (int b, float dB)
{
    const auto& info = bands_[(size_t) b];
    setBandParam (info, info.gainParam, dB);
}

EQAnalyserComponent::EQAnalyserComponent (NAMAudioProcessor& proc,
                                          juce::AudioProcessorValueTreeState& state)
    : proc_ (proc), apvts_ (state)
{
    scratch_.assign (2 * kFftSize, 0.f);
    binDB_.fill ((float) kMinDb * 4.f);
    modelIdx_ = juce::jlimit (0, pedal::count() - 1,
                              (int) paramValue ("eq_model"));
    rebuildBands();
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
    const auto& info = bands_[(size_t) b];
    return info.freqParam.isNotEmpty() ? (double) bandParam (info, info.freqParam)
                                       : info.fixedFreq;
}

double EQAnalyserComponent::bandQ (int b) const
{
    const auto& info = bands_[(size_t) b];
    return info.qParam.isNotEmpty() ? (double) bandParam (info, info.qParam) : info.fixedQ;
}

// Modulo della campana RBJ, in dB: serve per disegnare la curva dei modelli a
// bande, che nella catena sono proprio biquad di questa forma.
namespace {
    double peakingDB (double sr, double hz, double fc, double q, double gainDB)
    {
        if (std::abs (gainDB) < 0.001) return 0.0;
        const double A  = std::pow (10.0, gainDB / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * fc / sr;
        const double al = std::sin (w0) / (2.0 * q), cw = std::cos (w0);
        const double b0 = 1.0 + al * A, b1 = -2.0 * cw, b2 = 1.0 - al * A;
        const double a0 = 1.0 + al / A, a1 = -2.0 * cw, a2 = 1.0 - al / A;
        const double w  = 2.0 * juce::MathConstants<double>::pi * hz / sr;
        const double cr = std::cos (w), ci = std::sin (w);
        const double c2r = std::cos (2.0 * w), c2i = std::sin (2.0 * w);
        const double nr = b0 + b1 * cr + b2 * c2r, ni = -(b1 * ci + b2 * c2i);
        const double dr = a0 + a1 * cr + a2 * c2r, di = -(a1 * ci + a2 * c2i);
        const double num = std::sqrt (nr * nr + ni * ni);
        const double den = std::sqrt (dr * dr + di * di);
        return 20.0 * std::log10 (std::max (num / std::max (den, 1e-12), 1e-6));
    }
}

double EQAnalyserComponent::curveDB (double hz, double sr) const
{
    const auto& m = pedal::at (modelIdx_);
    if (m.topo == pedal::Topology::EqNative)
        return nam_dsp::FiveBandEQ::responseDB (sr, hz,
                    paramValue ("eq_bass"), paramValue ("eq_mid_freq"),
                    paramValue ("eq_mid_q"), paramValue ("eq_mid_gain"),
                    paramValue ("eq_presence"), paramValue ("eq_treble"),
                    paramValue ("eq_air"));

    double dB = 0.0;
    for (int b = 0; b < numBands(); ++b)
        dB += peakingDB (sr, hz, bandFreq (b), bandQ (b), (double) bandGainDB (b));

    // Il livello di uscita trasla tutta la curva.
    const int lvl = (m.topo == pedal::Topology::EqGraphic7) ? 8 : 5;
    if (m.numKnobs >= lvl) {
        const auto& k = m.knobs[lvl - 1];
        dB += k.min + paramValue ((juce::String ("eq_p") + juce::String (lvl)).toRawUTF8())
                      * (k.max - k.min);
    }
    return dB;
}

// --- analisi ---------------------------------------------------------------

void EQAnalyserComponent::timerCallback()
{
    const bool nowActive = paramValue ("eq_bypass") < 0.5f;
    if (nowActive != active_) { active_ = nowActive; }

    // Cambiando equalizzatore nel menu della sezione cambiano le maniglie, ma
    // l'analizzatore resta: e' sempre lo stesso componente.
    const int mi = juce::jlimit (0, pedal::count() - 1, (int) paramValue ("eq_model"));
    if (mi != modelIdx_) { modelIdx_ = mi; rebuildBands(); repaint(); }

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

        curvePath_.clear();
        const int steps = juce::jmax (2, getWidth());
        for (int i = 0; i < steps; ++i)
        {
            const double hz = xToFreq ((double) i);
            const double dB = curveDB (hz, sr);
            const float x = (float) i, y = (float) dbToY (dB);
            if (i == 0) curvePath_.startNewSubPath (x, y);
            else        curvePath_.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xfff2c438).withAlpha (active_ ? 0.95f : 0.3f));
        g.strokePath (curvePath_, juce::PathStrokeType (1.8f));
    }

    // Maniglie delle bande.
    for (int b = 0; b < numBands(); ++b)
    {
        const auto& info = bands_[(size_t) b];
        const float x = (float) freqToX (bandFreq (b));
        const float y = (float) dbToY (bandGainDB (b));
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
            t << "  " << juce::String (bandGainDB (b), 1) << " dB";
            if (info.freqParam.isNotEmpty())
                t << "  " << juce::String ((int) bandFreq (b)) << " Hz";
            if (info.qParam.isNotEmpty())
                t << "  Q " << juce::String (bandQ (b), 2);
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
    for (int b = 0; b < numBands(); ++b)
    {
        const juce::Point<float> c ((float) freqToX (bandFreq (b)),
                                    (float) dbToY (bandGainDB (b)));
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
    const auto& info = bands_[(size_t) dragBand_];
    if (auto* p = apvts_.getParameter (info.gainParam)) p->beginChangeGesture();
    if (info.freqParam.isNotEmpty())
        if (auto* p = apvts_.getParameter (info.freqParam)) p->beginChangeGesture();
    repaint();
}

void EQAnalyserComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand_ < 0) return;
    const auto& info = bands_[(size_t) dragBand_];
    setBandGainDB (dragBand_, (float) yToDb (e.position.y));
    // Le bande a frequenza fissa si muovono solo in verticale.
    if (info.freqParam.isNotEmpty())
        setBandParam (info, info.freqParam, (float) xToFreq (e.position.x));
    repaint();
}

void EQAnalyserComponent::mouseUp (const juce::MouseEvent&)
{
    if (dragBand_ < 0) return;
    const auto& info = bands_[(size_t) dragBand_];
    if (auto* p = apvts_.getParameter (info.gainParam)) p->endChangeGesture();
    if (info.freqParam.isNotEmpty())
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
    const auto& info = bands_[(size_t) b];
    if (info.qParam.isEmpty()) return;
    const float q = (float) bandQ (b) * (1.0f + w.deltaY * 0.6f);
    setBandParam (info, info.qParam, q);
    repaint();
}
