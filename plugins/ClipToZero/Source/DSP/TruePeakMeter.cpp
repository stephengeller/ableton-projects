#include "TruePeakMeter.h"
#include <cmath>

void TruePeakMeter::prepare(double sampleRate, int /*numChannels*/, int maxBlockSize) {
    sr = sampleRate;

    using OS = juce::dsp::Oversampling<float>;
    oversampler = std::make_unique<OS>(maxChannels, oversampleStages,
                                        OS::filterHalfBandFIREquiripple,
                                        /*isMaxQuality=*/true,
                                        /*useIntegerLatency=*/true);
    oversampler->initProcessing(static_cast<size_t>(maxBlockSize));
    oversampler->reset();

    // Decay loop runs at the OS rate (4x native), so the per-sample decay
    // coefficient is the 4x-th root of the native-rate one. Result: identical
    // -20 dB/sec wall-clock decay regardless of OS factor.
    const float effectiveRate = static_cast<float>(sr) * oversampleFactor;
    peakDecayPerSample = std::pow(10.0f, -20.0f / (20.0f * effectiveRate));
    peakHoldSamples    = juce::jmax(1, static_cast<int>(effectiveRate * 1.5));

    reset();
}

void TruePeakMeter::reset() noexcept {
    for (int ch = 0; ch < maxChannels; ++ch) {
        peakLinear[ch]     = 0.0f;
        peakHoldLinear[ch] = 0.0f;
        peakHoldFrames[ch] = 0;
        tpDbAtomic[ch].store(-100.0f);
        tpHoldDbAtomic[ch].store(-100.0f);
    }
    if (oversampler) oversampler->reset();
}

void TruePeakMeter::process(const juce::AudioBuffer<float>& buffer) noexcept {
    if (!oversampler) return;

    const int numCh = juce::jmin(buffer.getNumChannels(), maxChannels);
    if (numCh == 0) return;

    // The oversampler reads but never writes to its input — const_cast is
    // safe here. We need a non-const AudioBlock<float> because that's what
    // converts to the AudioBlock<const float> the API expects.
    auto& mutableBuffer = const_cast<juce::AudioBuffer<float>&>(buffer);
    juce::dsp::AudioBlock<float> inBlock(mutableBuffer);
    auto upBlock = oversampler->processSamplesUp(inBlock);

    const int upN   = static_cast<int>(upBlock.getNumSamples());
    const int upCh  = juce::jmin(static_cast<int>(upBlock.getNumChannels()), maxChannels);

    for (int ch = 0; ch < upCh; ++ch) {
        const float* x = upBlock.getChannelPointer(ch);

        float peak     = peakLinear[ch];
        float peakHold = peakHoldLinear[ch];
        int   holdN    = peakHoldFrames[ch];

        for (int i = 0; i < upN; ++i) {
            const float a = std::abs(x[i]);

            // Instantaneous peak with constant-dB decay.
            peak *= peakDecayPerSample;
            if (a > peak) peak = a;

            // Peak hold: refresh on new high, count down, then decay.
            if (a > peakHold) {
                peakHold = a;
                holdN    = peakHoldSamples;
            } else if (holdN > 0) {
                --holdN;
            } else {
                peakHold *= peakDecayPerSample;
            }
        }

        peakLinear[ch]     = peak;
        peakHoldLinear[ch] = peakHold;
        peakHoldFrames[ch] = holdN;

        tpDbAtomic[ch].store(juce::Decibels::gainToDecibels(peak, -100.0f));
        tpHoldDbAtomic[ch].store(juce::Decibels::gainToDecibels(peakHold, -100.0f));
    }
}

float TruePeakMeter::getTruePeakDb(int ch) const noexcept {
    return tpDbAtomic[juce::jlimit(0, maxChannels - 1, ch)].load();
}

float TruePeakMeter::getTruePeakHoldDb(int ch) const noexcept {
    return tpHoldDbAtomic[juce::jlimit(0, maxChannels - 1, ch)].load();
}
