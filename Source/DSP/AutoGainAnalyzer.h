#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

// Captures the maximum |sample| over a measurement window, and reports the
// gain-in-dB needed to make that peak hit 0 dBFS.
class AutoGainAnalyzer {
public:
    void  prepare(double sampleRate);
    void  process(const juce::AudioBuffer<float>& buffer) noexcept;
    void  reset() noexcept;

    void  startMeasurement(double seconds = 2.0) noexcept;
    bool  isMeasuring() const noexcept       { return measuring.load(); }

    // dB to add to current input to bring the captured peak to 0 dBFS.
    // Returns 0.0 if no signal was measured.
    float getSuggestedGainDb() const noexcept;

private:
    double sr = 44100.0;
    std::atomic<float> peakLinear { 0.0f };
    std::atomic<bool>  measuring  { false };
    std::atomic<int>   samplesRemaining { 0 };
};
