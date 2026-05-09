#include "LevelMeter.h"
#include <cmath>

void LevelMeter::prepare(double sampleRate, int /*numChannels*/) {
    sr = sampleRate;

    // -20 dB/sec → linear factor per sample = 10^(-20 / (20 * sr))
    peakDecayPerSample = std::pow(10.0f, -20.0f / (20.0f * static_cast<float>(sr)));

    // 1-pole RMS smoothing: time constant 300ms ≈ AES standard short-term RMS.
    rmsAlpha = 1.0f - std::exp(-1.0f / (static_cast<float>(sr) * 0.300f));

    peakHoldSamples = juce::jmax(1, static_cast<int>(sr * 1.5));

    reset();
}

void LevelMeter::reset() noexcept {
    for (int ch = 0; ch < maxChannels; ++ch) {
        peakLinear[ch]     = 0.0f;
        peakHoldLinear[ch] = 0.0f;
        peakHoldFrames[ch] = 0;
        rmsSquared[ch]     = 0.0f;
        peakDbAtomic[ch].store(-100.0f);
        rmsDbAtomic[ch].store(-100.0f);
        peakHoldDbAtomic[ch].store(-100.0f);
    }
}

void LevelMeter::process(const juce::AudioBuffer<float>& buffer) noexcept {
    const int numCh = juce::jmin(buffer.getNumChannels(), maxChannels);
    const int n    = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch) {
        const float* x = buffer.getReadPointer(ch);

        float peak     = peakLinear[ch];
        float peakHold = peakHoldLinear[ch];
        int   holdN    = peakHoldFrames[ch];
        float rmsSq    = rmsSquared[ch];

        for (int i = 0; i < n; ++i) {
            const float a = std::abs(x[i]);

            // Instantaneous peak with constant-dB decay.
            peak *= peakDecayPerSample;
            if (a > peak) peak = a;

            // Peak hold: refresh on any new high, then count down before decaying.
            if (a > peakHold) {
                peakHold = a;
                holdN    = peakHoldSamples;
            } else if (holdN > 0) {
                --holdN;
            } else {
                peakHold *= peakDecayPerSample;
            }

            // RMS via 1-pole on squared samples.
            rmsSq += rmsAlpha * (x[i] * x[i] - rmsSq);
        }

        peakLinear[ch]     = peak;
        peakHoldLinear[ch] = peakHold;
        peakHoldFrames[ch] = holdN;
        rmsSquared[ch]     = rmsSq;

        peakDbAtomic[ch].store(juce::Decibels::gainToDecibels(peak, -100.0f));
        peakHoldDbAtomic[ch].store(juce::Decibels::gainToDecibels(peakHold, -100.0f));
        rmsDbAtomic[ch].store(juce::Decibels::gainToDecibels(std::sqrt(juce::jmax(0.0f, rmsSq)), -100.0f));
    }
}

float LevelMeter::getPeakDb(int ch)     const noexcept { return peakDbAtomic[juce::jlimit(0, maxChannels - 1, ch)].load(); }
float LevelMeter::getRmsDb(int ch)      const noexcept { return rmsDbAtomic[juce::jlimit(0, maxChannels - 1, ch)].load(); }
float LevelMeter::getPeakHoldDb(int ch) const noexcept { return peakHoldDbAtomic[juce::jlimit(0, maxChannels - 1, ch)].load(); }
