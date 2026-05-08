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

    // Raw measured peak in dBFS (the loudest |sample| seen during the window).
    // Returns -100 dB if no signal was captured.
    float getMeasuredPeakDb() const noexcept;

    // dB to add to the input so the captured peak hits `targetDb` dBFS.
    // Returns 0.0 if no signal was measured.
    float getSuggestedGainDb(float targetDb = 0.0f) const noexcept;

private:
    double sr = 44100.0;
    std::atomic<float> peakLinear { 0.0f };
    std::atomic<bool>  measuring  { false };
    std::atomic<int>   samplesRemaining { 0 };
};
