// IRConvolver.cpp
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "IRConvolver.h"

#include <algorithm>
#include <cmath>

namespace nam_dsp {

IRConvolver::IRConvolver() = default;
IRConvolver::~IRConvolver() = default;

static void linearResample(const std::vector<float>& src, double srcSR,
                           std::vector<float>& dst, double dstSR) {
    if (src.empty()) { dst.clear(); return; }
    if (std::abs(srcSR - dstSR) < 1e-3) { dst = src; return; }

    const double ratio = dstSR / srcSR;
    const size_t outLen = static_cast<size_t>(src.size() * ratio + 0.5);
    dst.resize(outLen);
    for (size_t i = 0; i < outLen; ++i) {
        const double srcIdx = i / ratio;
        const size_t i0 = static_cast<size_t>(srcIdx);
        const double frac = srcIdx - i0;
        const float a = src[std::min(i0, src.size() - 1)];
        const float b = src[std::min(i0 + 1, src.size() - 1)];
        dst[i] = static_cast<float>(a + (b - a) * frac);
    }
}

bool IRConvolver::loadFromFile(const std::string& path, double targetSR) {
    ready_ = false;
    targetSR_ = targetSR;

    // Stream-read a bounded number of frames instead of slurping the whole
    // file: the final IR is capped to ~1.5 s anyway, and an untrusted WAV
    // header can otherwise request a multi-GB allocation (OOM/DoS).
    drwav wav;
    if (!drwav_init_file(&wav, path.c_str(), nullptr)) return false;

    const unsigned int channels = wav.channels;
    const unsigned int srcRate  = wav.sampleRate;
    // Bound srcRate too: a forged header (e.g. sampleRate=0xFFFFFFFF) would
    // otherwise inflate the 4 s frame cap into a multi-GB allocation.
    if (channels == 0 || channels > 64 || srcRate == 0 || srcRate > 768000) {
        drwav_uninit(&wav);
        return false;
    }

    // 4 s of source material is plenty of headroom before the resample cap.
    const drwav_uint64 maxFrames = static_cast<drwav_uint64>(srcRate) * 4;
    const drwav_uint64 want =
        std::min<drwav_uint64>(wav.totalPCMFrameCount, maxFrames);

    // Keep drwav_uninit reachable on every path: the allocation below can
    // throw bad_alloc, which would otherwise leak the underlying FILE handle.
    std::vector<float> interleaved;
    drwav_uint64 got = 0;
    try {
        interleaved.resize(static_cast<size_t>(want) * channels);
        got = drwav_read_pcm_frames_f32(&wav, want, interleaved.data());
    } catch (...) {
        drwav_uninit(&wav);
        return false;
    }
    drwav_uninit(&wav);
    if (got == 0) return false;

    // Down-mix to mono (first channel).
    std::vector<float> mono(static_cast<size_t>(got));
    for (drwav_uint64 i = 0; i < got; ++i)
        mono[i] = interleaved[i * channels];

    linearResample(mono, static_cast<double>(srcRate), ir_, targetSR_);

    // Hard cap IR length to keep CPU bounded (~1 s @ 48 kHz).
    const size_t maxLen = static_cast<size_t>(targetSR_ * 1.5);
    if (ir_.size() > maxLen) ir_.resize(maxLen);

    return !ir_.empty();
}

void IRConvolver::prepare(size_t headBlockSize, size_t tailBlockSize) {
    if (ir_.empty()) { ready_ = false; return; }
    conv_ = std::make_unique<fftconvolver::TwoStageFFTConvolver>();
    ready_ = conv_->init(headBlockSize, tailBlockSize, ir_.data(), ir_.size());
}

void IRConvolver::process(const float* in, float* out, size_t n) {
    if (!ready_ || !conv_) {
        // Pass-through if no IR loaded.
        if (in != out) std::copy(in, in + n, out);
        return;
    }
    conv_->process(in, out, n);
}

void IRConvolver::reset() {
    if (conv_) conv_->reset();
}

} // namespace nam_dsp
