#include "AutoGainAnalyzer.h"
#include <cmath>

void AutoGainAnalyzer::prepare(double sampleRate) {
    sr = sampleRate;
    reset();
}

void AutoGainAnalyzer::reset() noexcept {
    peakLinear.store(0.0f);
    measuring.store(false);
    samplesRemaining.store(0);
}

void AutoGainAnalyzer::startMeasurement(double seconds) noexcept {
    peakLinear.store(0.0f);
    samplesRemaining.store(static_cast<int>(sr * seconds));
    measuring.store(true);
}

void AutoGainAnalyzer::process(const juce::AudioBuffer<float>& buffer) noexcept {
    if (!measuring.load()) return;

    const int numCh = buffer.getNumChannels();
    const int n    = buffer.getNumSamples();

    float currentMax = peakLinear.load();
    for (int ch = 0; ch < numCh; ++ch) {
        const float* x = buffer.getReadPointer(ch);
        for (int i = 0; i < n; ++i) {
            const float a = std::abs(x[i]);
            if (a > currentMax) currentMax = a;
        }
    }
    peakLinear.store(currentMax);

    int remaining = samplesRemaining.fetch_sub(n) - n;
    if (remaining <= 0) {
        measuring.store(false);
    }
}

float AutoGainAnalyzer::getSuggestedGainDb() const noexcept {
    const float p = peakLinear.load();
    if (p < 1.0e-6f) return 0.0f;
    // We want gain such that p * 10^(gain/20) = 1.
    // gain_dB = -20 * log10(p) = -peakDb.
    return -juce::Decibels::gainToDecibels(p, -100.0f);
}
