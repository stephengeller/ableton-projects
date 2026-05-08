#include "LUFSMeter.h"
#include <cmath>

namespace {
    // ITU-R BS.1770-4 K-weighting: a high-shelf +4 dB around 1.68 kHz, then
    // a high-pass at ~38 Hz. JUCE's biquad designers reproduce these to
    // within rounding at any sample rate.
    constexpr float kPreShelfHz   = 1681.974f;
    constexpr float kHighPassHz   = 38.135f;
    constexpr float kPreShelfQ    = 0.7071068f;     // 1 / sqrt(2)
    constexpr float kHighPassQ    = 0.5f;
    constexpr float kPreShelfDb   = 4.0f;

    // L = -0.691 + 10·log10(z). Linear-domain absolute gate at -70 LUFS:
    // z = 10^((-70 + 0.691)/10) ≈ 1.172e-7
    constexpr double kAbsoluteGateZ = 1.1724653045822357e-07;

    inline float zToLUFS(double z) noexcept {
        if (z <= 0.0) return -100.0f;
        return -0.691f + 10.0f * static_cast<float>(std::log10(z));
    }
    inline double LUFSToZ(double lufs) noexcept {
        return std::pow(10.0, (lufs + 0.691) / 10.0);
    }
}

void LUFSMeter::prepare(double sampleRate, int numChannels) {
    sr      = sampleRate;
    numCh   = juce::jlimit(1, maxChannels, numChannels);

    samplesPerHop = juce::jmax(1, static_cast<int>(std::round(sampleRate * 0.1)));

    auto preCoeffs = Coeffs::makeHighShelf(sampleRate,
                                           kPreShelfHz,
                                           kPreShelfQ,
                                           juce::Decibels::decibelsToGain(kPreShelfDb));
    auto hpCoeffs  = Coeffs::makeHighPass (sampleRate,
                                           kHighPassHz,
                                           kHighPassQ);

    for (int ch = 0; ch < maxChannels; ++ch) {
        preFilter[ch].coefficients = preCoeffs;
        hpFilter [ch].coefficients = hpCoeffs;
    }

    resetFullSync();
}

void LUFSMeter::resetFullSync() noexcept {
    for (int ch = 0; ch < maxChannels; ++ch) {
        preFilter[ch].reset();
        hpFilter [ch].reset();
        hopSumSq [ch] = 0.0;
        for (int i = 0; i < ringHops; ++i) hopHistory[ch][i] = 0.0;
    }
    samplesAccumulated = 0;
    hopIndex           = 0;
    totalHops          = 0;
    integratedCount    = 0;
    momentary.store(-100.0f);
    shortterm.store(-100.0f);
    integrated.store(-100.0f);
    resetIntegratedFlag.store(false);
}

void LUFSMeter::process(const juce::AudioBuffer<float>& buffer) noexcept {
    if (resetIntegratedFlag.exchange(false)) {
        integratedCount = 0;
        integrated.store(-100.0f);
    }

    const int n         = buffer.getNumSamples();
    const int chsToUse  = juce::jmin(buffer.getNumChannels(), numCh);

    int processed = 0;
    while (processed < n) {
        const int remainingInHop = samplesPerHop - samplesAccumulated;
        const int toProcess      = juce::jmin(remainingInHop, n - processed);

        for (int ch = 0; ch < chsToUse; ++ch) {
            const float* x = buffer.getReadPointer(ch) + processed;
            auto& pre = preFilter[ch];
            auto& hp  = hpFilter [ch];
            double  acc = hopSumSq[ch];
            for (int i = 0; i < toProcess; ++i) {
                float y = pre.processSample(x[i]);
                y       = hp.processSample(y);
                acc += static_cast<double>(y) * static_cast<double>(y);
            }
            hopSumSq[ch] = acc;
        }

        samplesAccumulated += toProcess;
        processed          += toProcess;

        if (samplesAccumulated >= samplesPerHop) {
            finishHop();
            samplesAccumulated = 0;
        }
    }
}

void LUFSMeter::finishHop() noexcept {
    // Push this hop's per-channel sum-of-squares into the ring.
    for (int ch = 0; ch < numCh; ++ch) {
        hopHistory[ch][hopIndex] = hopSumSq[ch];
        hopSumSq[ch] = 0.0;
    }
    hopIndex = (hopIndex + 1) % ringHops;
    if (totalHops < ringHops) ++totalHops;

    // Momentary loudness: mean square across the last 4 hops (= 400 ms).
    auto blockZForLastNHops = [&](int nHops) -> double {
        double zSum = 0.0;
        for (int ch = 0; ch < numCh; ++ch) {
            double chSum = 0.0;
            for (int i = 0; i < nHops; ++i) {
                const int idx = (hopIndex - 1 - i + ringHops) % ringHops;
                chSum += hopHistory[ch][idx];
            }
            // Per-channel mean square × channel weight (G_ch = 1.0 for L/R).
            zSum += chSum / static_cast<double>(samplesPerHop * nHops);
        }
        return zSum;
    };

    if (totalHops >= 4) {
        const double zMomentary = blockZForLastNHops(4);
        momentary.store(zToLUFS(zMomentary));

        // Each completed 400 ms block contributes one entry to integrated
        // history. We use the new-block-every-hop convention (the 75 %
        // overlap from the spec).
        if (integratedCount < maxIntegrated) {
            blockZ[integratedCount++] = zMomentary;
        }
        recomputeIntegrated();
    }

    if (totalHops >= ringHops) {
        const double zShort = blockZForLastNHops(ringHops);
        shortterm.store(zToLUFS(zShort));
    }
}

void LUFSMeter::recomputeIntegrated() noexcept {
    if (integratedCount == 0) {
        integrated.store(-100.0f);
        return;
    }

    // Pass 1: absolute gate at -70 LUFS, take the mean of remaining z values.
    double sum = 0.0;
    int    n   = 0;
    for (int i = 0; i < integratedCount; ++i) {
        if (blockZ[i] >= kAbsoluteGateZ) {
            sum += blockZ[i];
            ++n;
        }
    }
    if (n == 0) {
        integrated.store(-100.0f);
        return;
    }
    const double meanZ      = sum / n;
    const double meanLufs   = -0.691 + 10.0 * std::log10(meanZ);
    const double relGateZ   = LUFSToZ(meanLufs - 10.0);

    // Pass 2: relative gate at meanLufs - 10 LU, mean of remaining is the
    // final integrated loudness.
    sum = 0.0; n = 0;
    for (int i = 0; i < integratedCount; ++i) {
        if (blockZ[i] >= relGateZ) {
            sum += blockZ[i];
            ++n;
        }
    }
    if (n == 0) {
        integrated.store(-100.0f);
        return;
    }
    integrated.store(zToLUFS(sum / n));
}
