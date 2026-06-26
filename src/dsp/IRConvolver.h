// IRConvolver.h - wrapper over FFTConvolver::TwoStageFFTConvolver.
// Loads a WAV IR via dr_wav, resamples linearly to target SR, and convolves.
#pragma once

#include "TwoStageFFTConvolver.h"
#include <string>
#include <vector>
#include <memory>

namespace nam_dsp {

class IRConvolver {
public:
    IRConvolver();
    ~IRConvolver();

    // Loads a mono IR (first channel of WAV). Returns true on success.
    // Must be called BEFORE prepare(), or call prepare() again after loading.
    bool loadFromFile(const std::string& path, double targetSampleRate);

    // Initialize FFT convolver with the loaded IR. Safe to call from RT thread
    // only if no allocation occurs; typically called from the worker thread.
    void prepare(size_t headBlockSize, size_t tailBlockSize);

    // RT-safe processing. n samples in -> n samples out.
    void process(const float* in, float* out, size_t n);

    void reset();

    bool isReady() const { return ready_; }
    size_t irLength() const { return ir_.size(); }

private:
    std::unique_ptr<fftconvolver::TwoStageFFTConvolver> conv_;
    std::vector<float> ir_;          // resampled IR samples
    double targetSR_ = 48000.0;
    bool ready_ = false;
};

} // namespace nam_dsp
