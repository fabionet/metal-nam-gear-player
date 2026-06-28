// NAMPipeline — single-channel NAM processing chain (framework-independent).
// Pipeline: input gain → NAM model → depth+resonance → 5-band EQ → IR → output gain.
// RT-safe in process(); loadModel/loadIR are sync and must run off the audio thread.
#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <NeuralAudio/NeuralModel.h>
#include "EQ.h"
#include "DepthFilter.h"
#include "IRConvolver.h"
#include "PreampFX.h"

class NAMPipeline {
public:
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
    void setModelBypass(bool b)         { modelBypass_   = b; }
    void setQualityScale(float s)       { qualityScale_  = s; }

    // Pre-FX
    void setGate(float threshDB, float releaseMs, bool byp)
    { gate_.setThresholdDB(threshDB); gate_.setReleaseMs(releaseMs); gate_.setBypass(byp); }
    void setOverdrive(float drive, float toneDB, float levelDB, bool byp)
    { od_.setDrive(drive); od_.setToneDB(toneDB); od_.setLevelDB(levelDB); od_.setBypass(byp); }
    void setDistortion(float drive, float toneDB, float levelDB, bool byp)
    { dist_.setDrive(drive); dist_.setToneDB(toneDB); dist_.setLevelDB(levelDB); dist_.setBypass(byp); }
    void setHighPass(float freqHz, bool byp)
    { hp_.setFreqHz(freqHz); hp_.setBypass(byp); }
    void setLoudnessNorm(bool enabled, float targetDB)
    { loud_.setEnabled(enabled); loud_.setTargetDB(targetDB); }

    // --- Model / IR loading (call from non-audio thread) ---
    // Returns true on success. Old model/IR is destroyed.
    bool loadModel(const std::string& path);
    bool loadIR(const std::string& path);
    void clearModel();
    void clearIR();

    bool hasModel() const { return model_ != nullptr; }
    bool hasIR()    const { return ir_ != nullptr && ir_->isReady(); }

    float modelInputDBAdjustment()  const;
    float modelOutputDBAdjustment() const;

private:
    double sampleRate_ = 48000.0;
    int    blockSize_  = 512;

    NeuralAudio::NeuralModelLoader loader_;
    std::unique_ptr<NeuralAudio::NeuralModel> model_;
    std::unique_ptr<nam_dsp::IRConvolver>     ir_;

    nam_dsp::FiveBandEQ   eq_;
    nam_dsp::DepthFilter  depth_;

    preamp_fx::SmartGate     gate_;
    preamp_fx::Overdrive     od_;
    preamp_fx::Distortion    dist_;
    preamp_fx::HighPass      hp_;
    preamp_fx::LoudnessNorm  loud_;

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
    std::atomic<bool>  modelBypass_   { false };

    // Smoothed gain state.
    float inputGainLin_  = 1.f;
    float outputGainLin_ = 1.f;

    // Temp buffer for IR wet/dry mixing.
    std::vector<float> tmp_;

    void updateCachedDsp();
};
