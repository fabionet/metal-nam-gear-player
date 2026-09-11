// EQAnalyserComponent — analizzatore di spettro con sopra la curva
// dell'equalizzatore e le maniglie delle bande, trascinabili.
//
// Il segnale viene prelevato dalla pipeline subito dopo lo stadio EQ, cosi'
// nello spettro si vede l'effetto della curva invece del solo ingresso.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

class NAMAudioProcessor;

class EQAnalyserComponent : public juce::Component,
                            private juce::Timer
{
public:
    EQAnalyserComponent (NAMAudioProcessor& proc, juce::AudioProcessorValueTreeState& state);
    ~EQAnalyserComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    // Le cinque bande nell'ordine della catena. Solo MID ha frequenza e Q
    // regolabili; le altre stanno a frequenza fissa e si muovono in verticale.
    enum Band { BandBass, BandMid, BandPres, BandTreble, BandAir, kNumBands };

    struct BandInfo
    {
        const char*  gainParam;
        const char*  freqParam;   // nullptr = frequenza fissa
        const char*  qParam;      // nullptr = Q fisso
        double       fixedFreq;
        const char*  label;
        juce::Colour colour;
    };

    static const std::array<BandInfo, kNumBands>& bands();

    void timerCallback() override;

    float paramValue (const char* id) const;
    void  setParamValue (const char* id, float v);

    double freqToX (double hz)  const;
    double xToFreq (double x)   const;
    double dbToY   (double dB)  const;
    double yToDb   (double y)   const;

    double bandFreq (int b) const;
    int    hitTestHandle (juce::Point<float> p) const;

    NAMAudioProcessor&                   proc_;
    juce::AudioProcessorValueTreeState&  apvts_;

    // Analisi
    static constexpr int kFftOrder = 11;              // 2048 punti
    static constexpr int kFftSize  = 1 << kFftOrder;
    static constexpr int kNumBins  = kFftSize / 2;

    juce::dsp::FFT                        fft_ { kFftOrder };
    juce::dsp::WindowingFunction<float>   window_ { kFftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float>                    scratch_;   // 2 * kFftSize per la FFT reale
    std::array<float, kNumBins>           binDB_ {};  // spettro smorzato, in dB

    // Curva EQ precalcolata a ogni ridisegno
    juce::Path curvePath_;

    int  dragBand_  = -1;
    int  hoverBand_ = -1;
    bool active_    = true;     // segue eq_bypass: da spento si disegna smorto

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQAnalyserComponent)
};
