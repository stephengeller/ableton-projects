#include "SpectrumAnalyzer.h"
#include <cmath>

void SpectrumAnalyzer::prepare(double sr) {
    sampleRate = sr;
    reset();
}

void SpectrumAnalyzer::reset() noexcept {
    std::fill(ringBuffer.begin(),   ringBuffer.end(),   0.0f);
    std::fill(fftScratch.begin(),   fftScratch.end(),   0.0f);
    std::fill(magnitudesDb.begin(), magnitudesDb.end(), floorDb);
    writeIndex.store(0);
    lastFftReadIndex = 0;
}

void SpectrumAnalyzer::pushSamples(const juce::AudioBuffer<float>& buffer) noexcept {
    const int n     = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);
    if (n == 0 || numCh == 0) return;

    int idx = writeIndex.load();
    for (int i = 0; i < n; ++i) {
        // Mono mix-down — spectrum analyser doesn't care about stereo.
        float s = 0.0f;
        for (int ch = 0; ch < numCh; ++ch) s += buffer.getReadPointer(ch)[i];
        ringBuffer[idx] = (numCh > 0) ? s / static_cast<float>(numCh) : 0.0f;
        idx = (idx + 1) % ringSize;
    }
    writeIndex.store(idx);
}

bool SpectrumAnalyzer::computeIfReady() {
    const int currentWrite = writeIndex.load();
    // Available = samples written since last FFT.
    int available = currentWrite - lastFftReadIndex;
    if (available < 0) available += ringSize;
    if (available < fftSize) return false;

    // Copy the latest fftSize samples into the scratch buffer.
    // We read from (currentWrite - fftSize), wrapping if necessary.
    const int readStart = (currentWrite - fftSize + ringSize) % ringSize;
    if (readStart + fftSize <= ringSize) {
        std::copy(ringBuffer.begin() + readStart,
                  ringBuffer.begin() + readStart + fftSize,
                  fftScratch.begin());
    } else {
        const int part1 = ringSize - readStart;
        std::copy(ringBuffer.begin() + readStart, ringBuffer.end(),
                  fftScratch.begin());
        std::copy(ringBuffer.begin(), ringBuffer.begin() + (fftSize - part1),
                  fftScratch.begin() + part1);
    }

    // Window + transform. JUCE's frequency-only FFT writes magnitudes into
    // the first half of the buffer in-place.
    window.multiplyWithWindowingTable(fftScratch.data(), fftSize);
    // Zero the imaginary half so the in-place transform produces clean magnitudes.
    std::fill(fftScratch.begin() + fftSize, fftScratch.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform(fftScratch.data());

    // Per-bin magnitude in dB. Smooth: snap up on peak, decay linearly in dB.
    constexpr float gainCorrection = 2.0f / static_cast<float>(fftSize);
    for (int b = 0; b < numBins; ++b) {
        const float mag    = fftScratch[b] * gainCorrection;
        const float magDb  = (mag > 0.0f)
                                ? juce::Decibels::gainToDecibels(mag, floorDb)
                                : floorDb;

        if (magDb > magnitudesDb[b]) {
            magnitudesDb[b] = magDb;                     // attack: snap up
        } else {
            magnitudesDb[b] = juce::jmax(floorDb,
                                         magnitudesDb[b] - decayDbPerCall);  // linear-dB decay
        }
    }

    lastFftReadIndex = currentWrite;
    return true;
}
