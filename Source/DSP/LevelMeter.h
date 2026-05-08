#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>

class LevelMeter {
public:
    static constexpr int maxChannels = 2;

    void prepare(double sampleRate, int numChannels);
    void process(const juce::AudioBuffer<float>& buffer) noexcept;
    void reset() noexcept;

    float getPeakDb(int channel) const noexcept;
    float getRmsDb(int channel) const noexcept;
    float getPeakHoldDb(int channel) const noexcept;

private:
    double sr = 44100.0;

    // State persisted across blocks (audio thread only)
    std::array<float, maxChannels> peakLinear      { 0.0f, 0.0f };
    std::array<float, maxChannels> peakHoldLinear  { 0.0f, 0.0f };
    std::array<int,   maxChannels> peakHoldFrames  { 0, 0 };
    std::array<float, maxChannels> rmsSquared      { 0.0f, 0.0f };

    // Cached dB values for the GUI thread to read.
    std::array<std::atomic<float>, maxChannels> peakDbAtomic;
    std::array<std::atomic<float>, maxChannels> rmsDbAtomic;
    std::array<std::atomic<float>, maxChannels> peakHoldDbAtomic;

    float peakDecayPerSample = 1.0f;     // -20 dB/sec when not being driven
    float rmsAlpha            = 0.001f;  // 1-pole lowpass coefficient (~300ms)
    int   peakHoldSamples     = 1;       // ~1.5s hold
};
