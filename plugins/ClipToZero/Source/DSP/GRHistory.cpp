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
    binMaxPre   = 0.0f;
    binMaxPost  = 0.0f;
}

void GRHistory::process(const juce::AudioBuffer<float>& pre,
                        const juce::AudioBuffer<float>& post) noexcept {
    const int n     = juce::jmin(pre.getNumSamples(), post.getNumSamples());
    const int numCh = juce::jmin(pre.getNumChannels(), post.getNumChannels(), 2);
    if (n == 0 || numCh == 0) return;

    int idx = writeIndex.load();

    // Per-sample loop tracks running peak |pre| and |post| separately.
    // GR for the bin is computed at the bin boundary from those peaks,
    // not from per-sample comparisons -- this makes the calculation
    // robust to the OS downsampler's group delay (peaks shift by less
    // than one bin's worth of samples, so max-pre and max-post still
    // capture the same physical clipping event).
    for (int i = 0; i < n; ++i) {
        float samplePre  = 0.0f;
        float samplePost = 0.0f;
        for (int ch = 0; ch < numCh; ++ch) {
            samplePre  = juce::jmax(samplePre,  std::abs(pre .getReadPointer(ch)[i]));
            samplePost = juce::jmax(samplePost, std::abs(post.getReadPointer(ch)[i]));
        }
        if (samplePre  > binMaxPre)  binMaxPre  = samplePre;
        if (samplePost > binMaxPost) binMaxPost = samplePost;

        if (++sampleAccum >= samplesPerBin) {
            // GR is the ratio of the bin's peak post to peak pre, in dB.
            // Only meaningful when pre crossed the audibility threshold
            // (-40 dBFS): below that we're in noise-floor territory and
            // any peak difference is filter ringing, not clipping.
            constexpr float audibleThreshold = 0.01f;   // = -40 dBFS
            float binGrDb = 0.0f;
            if (binMaxPre > audibleThreshold && binMaxPost < binMaxPre) {
                binGrDb = 20.0f * std::log10(juce::jmax(0.0001f, binMaxPost / binMaxPre));
            }
            buffer[idx] = binGrDb;
            idx = (idx + 1) % historySize;
            sampleAccum = 0;
            binMaxPre   = 0.0f;
            binMaxPost  = 0.0f;
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
