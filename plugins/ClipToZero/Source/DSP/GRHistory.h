#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>

// Gain-reduction history buffer for the time-series GR meter under the scope.
//
// Design notes:
//   * 1 ms bins. At 48 kHz this is 48 samples per bin; we accumulate the
//     most-negative (peak) gain-reduction-in-dB seen across the bin so a
//     brief spike from a transient doesn't get averaged away.
//   * 16 384 entries covers ~16 s of history at 1 ms resolution — comfortably
//     more than the scope's 10 s max window.
//   * Single-writer (audio thread) / single-reader (GUI thread) lock-free
//     access via a simple atomic write index. Each entry is a float — a
//     32-bit aligned write is atomic on x86 and arm64, so reads from the
//     GUI thread won't see partial writes.
//   * GR is stored in dB and is <= 0. Zero means "no clipping in this bin",
//     -3 means "3 dB of gain reduction" (the louder bound, since we take
//     the worst case across the bin).
class GRHistory {
public:
    static constexpr int historySize = 16384;

    void prepare(double sampleRate);
    void reset() noexcept;

    // Pre = signal entering the clipper. Post = signal leaving it. Both
    // buffers must have the same channel count and length. The first two
    // channels are used (mono / stereo).
    //
    // clipperWasActive: must be TRUE for this block to actually generate
    // GR readings. When FALSE (e.g. Hard clip with no samples shaved
    // this block), bins that complete in this block record 0 dB GR
    // regardless of binMaxPre/binMaxPost. This is the semantic gate
    // that prevents phantom GR readings caused by OS-chain FIR
    // artifacts or any other source of post != pre that isn't actual
    // clipper work. For Soft / Poly / Tube the caller should pass TRUE
    // unconditionally because those curves compress every non-zero
    // sample by some amount and the user expects to see that.
    void process(const juce::AudioBuffer<float>& pre,
                 const juce::AudioBuffer<float>& post,
                 bool clipperWasActive) noexcept;

    // Copy the most-recent `count` bins into `dest`. dest[0] is the oldest,
    // dest[count-1] is the newest. `count` is clamped to `historySize`.
    void readLatest(float* dest, int count) const noexcept;

    // Most-negative GR (dB) seen in the last ~100 ms (~100 bins). Used for
    // the corner readout. Returns 0 when there's no recent clipping.
    float getRecentPeakGrDb() const noexcept;

private:
    std::array<float, historySize> buffer {};
    std::atomic<int>               writeIndex { 0 };
    int    samplesPerBin = 48;
    int    sampleAccum   = 0;
    // Bin-level peak comparison instead of per-sample -- bin-peak is
    // robust to the sub-sample fractional group delay of linear-phase
    // FIRs (the integer-sample portion is compensated upstream by
    // PluginProcessor's preClipDelay line). See GRHistory.cpp for the
    // longer explanation.
    float  binMaxPre   = 0.0f;
    float  binMaxPost  = 0.0f;

    // Tracks whether ANY sample in the currently-accumulating bin came
    // through the clipper while it was actively shaving (or while a
    // non-Hard curve was active). Bins span multiple process() calls
    // when block size doesn't divide evenly by samplesPerBin (almost
    // always the case), so the flag must accumulate across calls rather
    // than reset per call. Reset to false only when a bin completes.
    bool   binHadClipping = false;
};
