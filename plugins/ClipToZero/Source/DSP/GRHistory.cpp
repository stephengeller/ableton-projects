#include "GRHistory.h"
#include <cmath>

void GRHistory::prepare(double sampleRate) {
    samplesPerBin = juce::jmax(1, static_cast<int>(sampleRate * 0.001));  // 1 ms
    reset();
}

void GRHistory::reset() noexcept {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writeIndex.store(0);
    sampleAccum = 0;
    binPeakGrDb = 0.0f;
}

void GRHistory::process(const juce::AudioBuffer<float>& pre,
                        const juce::AudioBuffer<float>& post) noexcept {
    const int n     = juce::jmin(pre.getNumSamples(), post.getNumSamples());
    const int numCh = juce::jmin(pre.getNumChannels(), post.getNumChannels(), 2);
    if (n == 0 || numCh == 0) return;

    int idx = writeIndex.load();

    for (int i = 0; i < n; ++i) {
        // Worst-case per-sample GR across channels.
        float maxAbsPre  = 0.0f;
        float maxAbsPost = 0.0f;
        for (int ch = 0; ch < numCh; ++ch) {
            maxAbsPre  = juce::jmax(maxAbsPre,  std::abs(pre .getReadPointer(ch)[i]));
            maxAbsPost = juce::jmax(maxAbsPost, std::abs(post.getReadPointer(ch)[i]));
        }

        // GR (dB) for this sample. Only meaningful when the pre signal is
        // loud enough that the clipper could *plausibly* be active --
        // below ~-40 dBFS we're in noise-floor territory and any
        // 'difference' between pre and post is just filter phase/ringing
        // from the oversampler's downsampling stage (the pre buffer is
        // captured at the native rate before upsampling; post is the
        // downsampled output, so even with the clipper as a no-op the
        // two sample streams are slightly different by construction).
        // Without this gate, quiet bits between transients would report
        // phantom -50/-60 dB GR readings that swamped the visualisation.
        constexpr float audibleThreshold = 0.01f;   // = -40 dBFS
        if (maxAbsPre > audibleThreshold && maxAbsPost < maxAbsPre) {
            const float grDb = 20.0f * std::log10(juce::jmax(0.0001f, maxAbsPost / maxAbsPre));
            if (grDb < binPeakGrDb) binPeakGrDb = grDb;
        }

        if (++sampleAccum >= samplesPerBin) {
            buffer[idx] = binPeakGrDb;
            idx = (idx + 1) % historySize;
            sampleAccum = 0;
            binPeakGrDb = 0.0f;
        }
    }

    writeIndex.store(idx);
}

void GRHistory::readLatest(float* dest, int count) const noexcept {
    if (count <= 0) return;
    count = juce::jmin(count, historySize);
    const int currentWrite = writeIndex.load();
    // The newest entry is at (currentWrite - 1). Walk backwards `count`
    // entries; dest[0] gets the oldest, dest[count-1] gets the newest.
    for (int i = 0; i < count; ++i) {
        const int srcIdx = (currentWrite - count + i + historySize) % historySize;
        dest[i] = buffer[srcIdx];
    }
}

float GRHistory::getRecentPeakGrDb() const noexcept {
    // Scan the last 100 bins (~100 ms) for the most-negative value.
    constexpr int window = 100;
    const int currentWrite = writeIndex.load();
    float peak = 0.0f;
    for (int i = 0; i < window; ++i) {
        const int idx = (currentWrite - 1 - i + historySize) % historySize;
        if (buffer[idx] < peak) peak = buffer[idx];
    }
    return peak;
}
