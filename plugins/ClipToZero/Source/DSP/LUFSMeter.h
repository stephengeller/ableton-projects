#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>

// ITU-R BS.1770-4 / EBU R128 loudness meter.
//
// Reports three values, all in LUFS (loudness units relative to full scale):
//   * Momentary  — 400 ms K-weighted mean-square window
//   * Short-term — 3000 ms K-weighted mean-square window
//   * Integrated — gated mean over the full measurement (since last reset),
//                  with absolute (-70 LUFS) and relative (-10 LU) gates.
//
// The class is realtime-safe: all storage is preallocated in `prepare()`,
// and the audio thread never allocates, locks, or blocks. Reset is requested
// via an atomic flag and applied at the start of the next process call.
class LUFSMeter {
public:
    static constexpr int maxChannels = 2;

    void  prepare(double sampleRate, int numChannels);
    void  process(const juce::AudioBuffer<float>& buffer) noexcept;

    // Hard reset everything (filter state, history, integrated). Call from
    // prepareToPlay or any moment we know audio isn't running.
    void  resetFullSync() noexcept;

    // Reset just the integrated value — momentary and short-term keep
    // running. Safe to call from the GUI thread.
    void  requestResetIntegrated() noexcept { resetIntegratedFlag.store(true); }

    float getMomentaryLUFS() const noexcept  { return momentary.load(); }
    float getShortTermLUFS() const noexcept  { return shortterm.load(); }
    float getIntegratedLUFS() const noexcept { return integrated.load(); }

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    // K-weighting filter chain (per channel: high-shelf then high-pass).
    std::array<Filter, maxChannels> preFilter;
    std::array<Filter, maxChannels> hpFilter;

    double sr      = 48000.0;
    int    numCh   = 2;

    // 100 ms hop accumulator (75 % overlap on the 400 ms momentary window).
    int    samplesPerHop      = 4800;
    int    samplesAccumulated = 0;
    std::array<double, maxChannels> hopSumSq {};

    // Ring buffer of the last 30 hops (= 3000 ms), per channel. Each entry
    // holds the sum-of-squared-K-weighted-samples for one 100 ms hop.
    static constexpr int ringHops = 30;
    std::array<std::array<double, ringHops>, maxChannels> hopHistory {};
    int hopIndex  = 0;
    int totalHops = 0;

    // Integrated history: one entry per finished 400 ms block (one per hop
    // once the ring is warm). 36000 entries ≈ 1 hour at 10 hops/sec.
    static constexpr int maxIntegrated = 36000;
    std::array<double, maxIntegrated> blockZ {};
    int integratedCount = 0;

    // Outputs visible to the GUI thread.
    std::atomic<float> momentary  { -100.0f };
    std::atomic<float> shortterm  { -100.0f };
    std::atomic<float> integrated { -100.0f };

    std::atomic<bool>  resetIntegratedFlag { false };

    void finishHop() noexcept;
    void recomputeIntegrated() noexcept;
};
