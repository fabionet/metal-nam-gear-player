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
    // `prefix` e' quello dello slot ("od", "dist", "ng", "gate", "comp", "eq").
    // `ownsNativeStack` vale solo per lo slot EQ, l'unico in cui la torre di
    // tono nativa dell'amplificatore sta davvero nella catena.
    EQAnalyserComponent (NAMAudioProcessor& proc, juce::AudioProcessorValueTreeState& state,
                         juce::String prefix, bool ownsNativeStack);
    ~EQAnalyserComponent() override;

    void paint (juce::Graphics&) override;
    void visibilityChanged() override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    // Le bande non sono piu' fisse: dipendono dall'equalizzatore scelto nel
    // menu della sezione. La torre di tono nativa ne espone cinque, con la sola
    // MID spazzolabile; il grafico ne espone sette a frequenza fissa; il
    // parametrico due, entrambe spazzolabili.
    struct BandInfo
    {
        juce::String gainParam;
        juce::String freqParam;   // vuoto = frequenza fissa
        juce::String qParam;      // vuoto = Q fisso
        double       fixedFreq = 1000.0;
        double       fixedQ    = 1.4;
        juce::String label;
        juce::Colour colour;
        // I pomelli della riserva sono normalizzati 0..1 sull'intervallo reale
        // dichiarato dal modello: la maniglia deve leggere e scrivere in dB.
        bool         normalised = false;
    };

    void rebuildBands();
    int  numBands() const { return (int) bands_.size(); }

    // Guadagno della banda in dB, qualunque sia la forma del parametro.
    float bandGainDB (int b) const;
    void  setBandGainDB (int b, float dB);
    float bandParam (const BandInfo&, const juce::String& id) const;
    void  setBandParam (const BandInfo&, const juce::String& id, float real);

    // Vero quando il modello scelto in questo slot e' un equalizzatore da
    // disegnare: la torre di tono nativa conta solo nello slot EQ, altrove e'
    // un passante e non avrebbe una curva da mostrare.
public:
    bool showsCurve() const;
private:

    // Risposta in dB della curva scelta, a una data frequenza.
    double curveDB (double hz, double sr) const;

    void timerCallback() override;

    juce::String slotParam (const char* suffix) const;   // "<prefisso>_<suffisso>"
    float paramValue (const char* id) const;
    void  setParamValue (const char* id, float v);

    double freqToX (double hz)  const;
    double xToFreq (double x)   const;
    double dbToY   (double dB)  const;
    double yToDb   (double y)   const;

    double bandFreq (int b) const;
    double bandQ    (int b) const;
    int    hitTestHandle (juce::Point<float> p) const;

    NAMAudioProcessor&                   proc_;
    juce::AudioProcessorValueTreeState&  apvts_;
    const juce::String                   prefix_;          // slot a cui e' legato
    const bool                           ownsNativeStack_; // solo lo slot EQ

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

    std::vector<BandInfo> bands_;
    int   modelIdx_ = -1;        // modello attualmente rispecchiato in bands_

    int  dragBand_  = -1;
    int  hoverBand_ = -1;
    bool active_    = true;     // segue eq_bypass: da spento si disegna smorto

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQAnalyserComponent)
};
