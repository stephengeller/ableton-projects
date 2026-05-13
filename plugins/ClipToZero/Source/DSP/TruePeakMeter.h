#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <memory>

// ITU-R BS.1770-4 true-peak analyser. Internally upsamples the input by 4x
// using a max-quality linear-phase FIR (the same machinery as the audio-
// path Oversampling), then takes the per-channel absolute peak of the
// upsampled stream. Output is exposed in dBTP with the same peak-hold /
// linear-decay pattern as LevelMeter, so the GUI can read a stable number.
//
// Runs strictly as an analysis tap — we never call processSamplesDown, so
// this adds zero latency to the audio path and is safe to insert as a
// side-chain anywhere in the chain. Standard placement is post-output-trim,
// because TP is meant to predict what the host's DAC will overshoot when
// converting back to analogue.
class TruePeakMeter {
public:
    static constexpr int maxChannels      = 2;
    static constexpr int oversampleFactor = 4;
    static constexpr int oversampleStages = 2;  // 2 doubling stages → 4x

    void prepare(double sampleRate, int numChannels, int maxBlockSize);
    void process(const juce::AudioBuffer<float>& buffer) noexcept;
    void reset() noexcept;

    float getTruePeakDb(int channel)     const noexcept;
    float getTruePeakHoldDb(int channel) const noexcept;

private:
    double sr = 44100.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // State persisted across blocks (audio thread only). All measured at the
    // OS rate, so the decay coefficients are scaled by oversampleFactor.
    std::array<float, maxChannels> peakLinear      { 0.0f, 0.0f };
    std::array<float, maxChannels> peakHoldLinear  { 0.0f, 0.0f };
    std::array<int,   maxChannels> peakHoldFrames  { 0, 0 };

    // Cached dB values for the GUI thread to read.
    std::array<std::atomic<float>, maxChannels> tpDbAtomic;
    std::array<std::atomic<float>, maxChannels> tpHoldDbAtomic;

    float peakDecayPerSample = 1.0f;  // -20 dB/sec wall-clock
    int   peakHoldSamples    = 1;     // 1.5s hold, in OS-rate samples
};
