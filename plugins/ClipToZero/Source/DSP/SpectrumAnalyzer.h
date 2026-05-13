#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>

// FFT-based spectrum analyser feeding the scope's translucent spectrum
// overlay. Lock-free single-writer/single-reader: audio thread pushes mono
// mix-down samples into a ring buffer; GUI thread pulls fftSize samples,
// windows + FFTs them, and updates the smoothed magnitude bins for
// rendering.
//
// Design choices:
//   * fftSize = 2048 (order 11) -> 1024 magnitude bins, 23 Hz/bin at 48 kHz.
//     Good balance of frequency resolution and update latency (~43 ms of
//     audio per FFT).
//   * Hann window -> ~32 dB peak side-lobes, smooth main lobe. Standard
//     choice for music visualisation; no need for fancier windows here.
//   * Per-bin peak + linear-dB decay smoothing on the GUI side: instant
//     attack on rising peaks, slow visible release (default 30 dB/s).
class SpectrumAnalyzer {
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;   // 2048
    static constexpr int numBins  = fftSize / 2;     // 1024 (0..Nyquist)

    void prepare(double sampleRate);
    void reset() noexcept;

    // Audio thread: push a stereo (or mono) buffer's worth of post-clip
    // samples. Mixed down to mono internally before being written to the
    // ring buffer.
    void pushSamples(const juce::AudioBuffer<float>& buffer) noexcept;

    // GUI thread: if at least fftSize new samples have accumulated since
    // the last FFT, copy out, window, transform, and update the bin
    // magnitudes. Returns true if bins were refreshed.
    bool computeIfReady();

    // GUI thread (read-only): one smoothed dB magnitude per bin, range
    // typically [-100, 0]. Index 0 = DC, index numBins-1 = Nyquist.
    const float* getMagnitudesDb() const noexcept { return magnitudesDb.data(); }
    double getSampleRate() const noexcept { return sampleRate; }

private:
    double sampleRate = 48000.0;
    juce::dsp::FFT                       fft    { fftOrder };
    juce::dsp::WindowingFunction<float>  window { fftSize, juce::dsp::WindowingFunction<float>::hann };

    // Ring buffer for incoming samples. Sized to hold several FFTs of
    // backlog so a paused GUI thread doesn't lose audio.
    static constexpr int ringSize = fftSize * 4;
    std::array<float, ringSize>   ringBuffer {};
    std::atomic<int>              writeIndex { 0 };
    int                           lastFftReadIndex = 0;

    // FFT scratch — JUCE's real-only FFT writes interleaved into a 2x
    // buffer. Kept as a member so we never allocate.
    std::array<float, fftSize * 2> fftScratch {};

    // Smoothed magnitudes (GUI side state only).
    std::array<float, numBins>     magnitudesDb {};
    static constexpr float         decayDbPerCall = 1.5f;   // ~45 dB/s at 30 Hz timer
    static constexpr float         floorDb        = -100.0f;
};
