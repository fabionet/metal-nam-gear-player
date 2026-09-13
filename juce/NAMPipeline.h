// NAMPipeline — single-channel NAM processing chain (framework-independent).
// Pipeline: input gain → NAM model → depth+resonance → 5-band EQ → IR → output gain.
// RT-safe in process(); loadModel/loadIR are sync and must run off the audio thread.
#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <string>

#include <NeuralAudio/NeuralModel.h>
#include "EQ.h"
#include "DepthFilter.h"
#include "IRConvolver.h"
#include "PreampFX.h"
#include "PedalDSP.h"
#include "FxDSP.h"
#include "NativeAmp.h"
#include "MarshallAmp.h"
#include "Biquad.h"

class NAMPipeline {
public:
    enum class OutputMode { Raw = 0, Normalized = 1, Calibrated = 2 };

    NAMPipeline();
    ~NAMPipeline();

    void prepare(double sampleRate, int blockSize);
    void reset();

    // RT-safe. Processes n samples in-place-friendly (in/out may overlap).
    void process(const float* in, float* out, int n);

    // --- Parameter setters (RT-safe, just store) ---
    void setInputGainDB(float dB)       { inputGainDB_   = dB; }
    void setOutputGainDB(float dB)      { outputGainDB_  = dB; }
    void setEqBass(float dB)            { eqBassDB_      = dB; }
    void setEqMid(float freq, float q, float dB) { eqMidFreq_ = freq; eqMidQ_ = q; eqMidDB_ = dB; }
    void setEqPresence(float dB)        { eqPresDB_      = dB; }
    void setEqTreble(float dB)          { eqTrebleDB_    = dB; }
    void setEqAir(float dB)             { eqAirDB_       = dB; }
    void setDepth(float dB)             { depthDB_       = dB; }
    void setResonance(float dB, float freq) { resDB_ = dB; resFreq_ = freq; }
    void setIrMix(float mix)            { irMix_         = mix; }
    void setIrBypass(bool b)            { irBypass_      = b; }
    // Secondo IR: abilitazione e bilanciamento fra i due (0 = solo IR1,
    // 1 = solo IR2, 0.5 = miscela paritaria). Il bilanciamento agisce sulla
    // parte wet, prima del dry/wet di irMix.
    void setIr2Enable(bool b)           { ir2Enable_     = b; }
    void setIrBalance(float b)          { irBalance_     = b; }
    // Volume indipendente per ciascun IR, in dB, applicato alla rispettiva
    // uscita wet prima dell'incrocio: permette di pareggiare due cabinet di
    // livello diverso senza toccare il bilanciamento.
    void setIr1VolumeDB(float db)       { ir1VolDB_      = db; }
    void setIr2VolumeDB(float db)       { ir2VolDB_      = db; }
    void setModelBypass(bool b)         { modelBypass_   = b; }
    void setQualityScale(float s)       { qualityScale_  = s; }

    // Steve-style gain-staging: Output Mode + Calibrate Input.
    void setOutputMode(OutputMode m)            { outputMode_.store(m); }
    void setCalibrateInput(bool on)             { calibrateInput_.store(on); }
    void setInputCalibrationLevelDBu(float dBu) { inputCalDBu_.store(dBu); }

    // UI queries about currently loaded model metadata (populated at loadModel()).
    bool  modelHasLoudness()    const noexcept { return hasLoudnessCached_.load(); }
    bool  modelHasInputLevel()  const noexcept { return hasInputLevelCached_.load(); }
    bool  modelHasOutputLevel() const noexcept { return hasOutputLevelCached_.load(); }
    float modelLoudnessDB()     const noexcept { return modelLoudnessCached_.load(); }
    float modelInputLevelDBu()  const noexcept { return modelInputLevelCached_.load(); }
    float modelOutputLevelDBu() const noexcept { return modelOutputLevelCached_.load(); }

    // Runtime slim/quality push: stores value and, if a slimmable model is loaded,
    // applies it live via SetQualityScaleFactor (RT-safe per NeuralAudio API).
    void setQualityScaleRuntime(float s);

    // True iff the currently loaded model exposes A2 quality scaling
    // (HasQualityScaling() returned true after the last successful load).
    bool isSlimmable() const noexcept   { return isSlimmable_.load(); }

    // Pre-FX
    void setGate(float threshDB, float releaseMs, bool byp)
    { gate_.setThresholdDB(threshDB); gate_.setReleaseMs(releaseMs); gate_.setBypass(byp); }
    // I due stadi sono ora pedali selezionabili dal registro condiviso. I valori
    // arrivano gia' convertiti nell'intervallo reale del modello scelto.
    // Slot: 0 OVERDRIVE, 1 DISTORTION, 2 NGATE, 3 GATE, 4 COMP, 5 EQ.
    void setPedal (int slot, int model, const float* knobs, const int* switches, bool byp)
    {
        pedal::PedalFX& p = pedals_[(std::size_t) (slot < 0 ? 0 : (slot > 5 ? 5 : slot))];
        p.setModel (model);
        for (int i = 0; i < pedal::kMaxKnobs;  ++i) p.setKnob   (i, knobs[i]);
        for (int i = 0; i < pedal::kMaxSwitch; ++i) p.setSwitch (i, switches[i]);
        p.setBypass (byp);
        p.commit();
    }
    // Pedali della scheda FX. Slot: 0 DELAY, 1 CHORUS, 2 FLANGER, 3 REVERB,
    // 4 TREMOLO, nell'ordine in cui stanno nella coda della catena.
    void setFxPedal (int slot, int model, const float* knobs, const int* switches, bool byp)
    {
        fxpedal::FxFX& p = fxPedals_[(std::size_t) (slot < 0 ? 0 : (slot > 4 ? 4 : slot))];
        p.setModel (model);
        for (int i = 0; i < fxpedal::kMaxKnobs;  ++i) p.setKnob   (i, knobs[i]);
        for (int i = 0; i < fxpedal::kMaxSwitch; ++i) p.setSwitch (i, switches[i]);
        p.setBypass (byp);
        p.commit();
    }

    void setHighPass(float freqHz, bool byp)
    { hp_.setFreqHz(freqHz); hp_.setBypass(byp); }
    void setLoudnessNorm(bool enabled, float targetDB)
    { loud_.setEnabled(enabled); loud_.setTargetDB(targetDB); }

    void setNoiseGate(float threshDB, float releaseMs, bool byp)
    { ng_.setThresholdDB(threshDB); ng_.setReleaseMs(releaseMs); ng_.setBypass(byp); }
    void setDepthBypass(bool byp) { depthBypass_.store(byp); }
    // Compressor routing position: 0=Front (pre-gate), 1=Post-Gate, 2=Post-IR.
    void setCompPos(int p) { compPos_.store(p); }
    // Riduzione di guadagno di un singolo slot: il compressore puo' stare in
    // una sezione qualunque, e il misuratore va mostrato dove sta lui.
    float pedalGainReductionDB (int slot) const noexcept
    { return pedals_[(std::size_t) (slot < 0 ? 0 : (slot > 5 ? 5 : slot))].gainReductionDB(); }

    // Dove prelevare il segnale per l'analizzatore: subito dopo lo slot che
    // ospita l'equalizzatore, cosi' nello spettro si vede l'effetto della sua
    // curva. 5 e' lo slot EQ, cioe' il comportamento di sempre.
    void setScopeSlot (int slot) { scopeSlot_.store (slot); }
    void setIRTools(float hpFreqHz, bool hpBypass,
                    float lpFreqHz, bool lpBypass,
                    float trimDb,   bool phaseInv);

    // Native tube amp (non-NAM). When enabled, the NAM model acts as a drive
    // pedal in front of this amp (routing: pre-FX → NAM(pedal) → native amp → …).
    void setNativeAmp(bool en, int ch, float thumpDB, float presenceDB,
                      float b1, float m1, float t1, float g1, float ma1,
                      float g2, float ma2,
                      float b3, float m3, float t3, float g3, float ma3)
    {
        ampEnabled_.store(en);
        amp_.setEnabled(en);
        amp_.setChannel(ch);
        amp_.setGlobal(thumpDB, presenceDB);
        amp_.setChannel1(b1, m1, t1, g1, ma1);
        amp_.setChannel2(g2, ma2);
        amp_.setChannel3(b3, m3, t3, g3, ma3);
    }

    // Stadio del lettore NAM: tonestack a 3 bande + volume, applicati
    // subito dopo il modello e prima dell'ampli nativo.
    void setModelStage(float volumeDB, float bassDB, float midDB, float trebleDB)
    {
        modelVolDB_  = volumeDB; modelBassDB_ = bassDB;
        modelMidDB_  = midDB;    modelTrebDB_ = trebleDB;
    }
    // Picco del blocco appena elaborato all'uscita dello stadio NAM.
    // Scritto e letto dal solo thread audio, subito dopo process().
    float lastModelStagePeak() const noexcept { return lastModelPeak_; }
    // Picco della parte wet di ciascun convolutore IR, prelevato prima del
    // dry/wet: alimenta i due meter sotto ai rispettivi caricatori.
    // Coda circolare per l'analizzatore di spettro, riempita dopo lo stadio
    // EQ. La lettura dal thread grafico non si sincronizza: al limite si legge
    // un blocco a cavallo di una scrittura, che su un analizzatore non si vede.
    static constexpr int kScopeSize = 4096;
    void readScope (float* dst, int n) const;

    float lastIR1Peak() const noexcept { return lastIr1Peak_; }
    float lastIR2Peak() const noexcept { return lastIr2Peak_; }

    // Amp-model selector: 0 = GEAR SX (NativeAmp), 1 = MARCHELLOW (MarshallAmp).
    void setAmpModel(int m) { ampModel_.store(m <= 0 ? 0 : 1); }

    // MARCHELLOW (Marshall JCM800 2203). Enable is gated by the caller so the
    // non-selected amp receives en=false and stays idle.
    void setMarshall(bool en, int valves, int sens, float preamp01, float masterDB,
                     float bassDB, float midDB, float trebleDB, float presenceDB)
    {
        marshallEnabled_.store(en);
        marshall_.setEnabled(en);
        marshall_.setValves(valves);
        marshall_.setSens(sens);
        marshall_.setControls(preamp01, masterDB, bassDB, midDB, trebleDB, presenceDB);
    }

    // --- Model / IR loading (call from non-audio thread) ---
    // Returns true on success. Old model/IR is destroyed.
    bool loadModel(const std::string& path);
    bool loadIR(const std::string& path);
    bool loadIR2(const std::string& path);
    void clearModel();
    void clearIR();
    void clearIR2();

    bool hasModel() const { return model_ != nullptr; }
    bool hasIR()    const { return ir_  != nullptr && ir_ ->isReady(); }
    bool hasIR2()   const { return ir2_ != nullptr && ir2_->isReady(); }
    // Vero quando il modello contiene gia' la cassa (gear_type amp_cab o
    // full-rig): in quel caso convolvere un IR sopra raddoppierebbe il cabinet.
    bool modelHasCab() const noexcept { return modelHasCab_.load(); }

    float modelInputDBAdjustment()  const;
    float modelOutputDBAdjustment() const;

private:
    double sampleRate_ = 48000.0;
    int    blockSize_  = 512;

    NeuralAudio::NeuralModelLoader loader_;
    std::unique_ptr<NeuralAudio::NeuralModel> model_;
    std::unique_ptr<nam_dsp::IRConvolver>     ir_;
    std::unique_ptr<nam_dsp::IRConvolver>     ir2_;

    nam_dsp::FiveBandEQ   eq_;
    nam_dsp::DepthFilter  depth_;

    std::atomic<int>         compPos_ { 0 };
    std::atomic<int>         scopeSlot_ { 5 };
    preamp_fx::SmartGate     gate_;
    pedal::PedalFX  pedals_[6];     // od, dist, ngate, gate, comp, eq
    fxpedal::FxFX   fxPedals_[5];   // delay, chorus, flanger, reverb, tremolo
    preamp_fx::HighPass      hp_;
    preamp_fx::LoudnessNorm  loud_;
    preamp_fx::NoiseGate     ng_;

    // IR post-processing tools (Fase 2a).
    preamp_fx::NativeAmp     amp_;
    preamp_fx::MarshallAmp   marshall_;
    std::atomic<int>         ampModel_ { 0 };

    nam_dsp::Biquad modelBass_, modelMid_, modelTreble_;
    float modelVolDB_  = 0.f, modelBassDB_ = 0.f, modelMidDB_ = 0.f, modelTrebDB_ = 0.f;
    float modelBassCached_ = 999.f, modelMidCached_ = 999.f, modelTrebCached_ = 999.f;
    float modelVolLin_ = 1.f;
    float lastModelPeak_ = 0.f;
    std::array<float, kScopeSize> scope_ {};
    std::vector<float>            scopeTap_;   // presa del blocco, dimensionata in prepare()

    // Riempie la coda circolare letta dall'analizzatore.
    void writeScope (const float* src, int n) noexcept
    {
        int w = scopeWrite_.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) {
            scope_[(std::size_t) w] = src[i];
            w = (w + 1) & (kScopeSize - 1);
        }
        scopeWrite_.store (w, std::memory_order_release);
    }
    std::atomic<int> scopeWrite_ { 0 };
    float lastIr1Peak_   = 0.f;
    float lastIr2Peak_   = 0.f;

    preamp_fx::BiquadHPF     irHp_;
    preamp_fx::BiquadLPF     irLp_;
    float irTrimGain_ = 1.f;          // target gain (linear), set by setIRTools()
    float irTrimGainSmoothed_ = 1.f;  // one-pole follower, avoids clicks on knob moves
    bool  irPhaseInv_ = false;

    // Cached EQ/depth values to avoid recomputing biquad coeffs every block.
    float eqBassDB_   = 0.f,  eqBassCached_   = 999.f;
    float eqMidFreq_  = 700.f, eqMidQ_  = 0.707f, eqMidDB_ = 0.f;
    float eqMidFreqCached_ = -1.f, eqMidQCached_ = -1.f, eqMidDBCached_ = 999.f;
    float eqPresDB_   = 0.f,  eqPresCached_   = 999.f;
    float eqTrebleDB_ = 0.f,  eqTrebleCached_ = 999.f;
    float eqAirDB_    = 0.f,  eqAirCached_    = 999.f;
    float depthDB_    = 0.f,  depthCached_    = 999.f;
    float resDB_      = 0.f,  resFreq_        = 100.f;
    float resDBCached_   = 999.f, resFreqCached_ = -1.f;

    // Params (atomic since UI thread writes them).
    std::atomic<float> inputGainDB_   { 0.f };
    std::atomic<float> outputGainDB_  { 0.f };
    std::atomic<float> irMix_         { 1.f };
    std::atomic<float> qualityScale_  { 1.f };
    std::atomic<bool>  irBypass_      { false };
    std::atomic<bool>  modelHasCab_   { false };
    std::atomic<bool>  ir2Enable_     { false };
    std::atomic<float> irBalance_     { 0.5f };
    std::atomic<float> ir1VolDB_      { 0.f };
    std::atomic<float> ir2VolDB_      { 0.f };
    std::atomic<bool>  modelBypass_   { false };
    std::atomic<bool>  isSlimmable_   { false };
    std::atomic<bool>  ampEnabled_    { false };
    std::atomic<bool>  marshallEnabled_ { false };

public:
    // True when prepare() has already run for exactly this sr/blocksize. Lets the
    // audio thread skip a redundant (allocating) re-prepare on pipeline swap.
    bool isPreparedFor(double sr, int bs) const noexcept
    { return preparedSampleRate_ == sr && preparedBlockSize_ == bs; }
private:
    double preparedSampleRate_ = 0.0;
    int    preparedBlockSize_  = 0;
    std::atomic<bool>  depthBypass_   { false };

    // Steve-style calibration state (UI-thread writes; audio-thread reads).
    std::atomic<OutputMode> outputMode_      { OutputMode::Normalized };
    std::atomic<bool>       calibrateInput_  { false };
    std::atomic<float>      inputCalDBu_     { 12.f };

    // Cached model metadata (populated once per loadModel(), read from audio thread).
    std::atomic<bool>  hasLoudnessCached_    { false };
    std::atomic<bool>  hasInputLevelCached_  { false };
    std::atomic<bool>  hasOutputLevelCached_ { false };
    std::atomic<float> modelLoudnessCached_    { -18.f };
    std::atomic<float> modelInputLevelCached_  { 12.f };
    std::atomic<float> modelOutputLevelCached_ { 12.f };

    // Smoothed gain state.
    float inputGainLin_  = 1.f;
    float outputGainLin_ = 1.f;

    // Temp buffer for IR wet/dry mixing.
    std::vector<float> tmp_;
    std::vector<float> tmp2_;      // uscita del secondo convolutore IR

    void updateCachedDsp();
};
