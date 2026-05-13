#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "DSP/LevelMeter.h"
#include "DSP/Clipper.h"
#include "DSP/AutoGainAnalyzer.h"
#include "DSP/LUFSMeter.h"
#include "DSP/GRHistory.h"
#include "DSP/SpectrumAnalyzer.h"
#include "DSP/TruePeakMeter.h"

class ClipToZeroProcessor : public juce::AudioProcessor {
public:
    ClipToZeroProcessor();
    ~ClipToZeroProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ClipToZero"; }
    bool acceptsMidi() const override   { return false; }
    bool producesMidi() const override  { return false; }
    bool isMidiEffect() const override  { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- public for the editor / UI components to read ----
    juce::AudioProcessorValueTreeState apvts;
    LevelMeter        inputMeter, outputMeter;
    Clipper           clipper;
    AutoGainAnalyzer  autoGain;
    LUFSMeter         lufs;
    GRHistory         grHistory;
    SpectrumAnalyzer  spectrum;
    // ITU-R BS.1770-4 true-peak analyser on the post-output-trim signal —
    // tells the user how much their clipped output will overshoot 0 dBFS at
    // the DAC. In a clipper context this is always positive (clipping by
    // definition creates inter-sample peaks); ≥ +1 dBTP is the "expect
    // audible distortion at the converter" line for most streaming codecs.
    TruePeakMeter     truePeakOut;

#if CTZ_PAID_BUILD
    // Read-only view of the demo flag for the editor (so it can draw the
    // DEMO badge in the brand bar). Always-false in free builds, where
    // this method (and the badge) don't exist.
    bool isInDemoMode() const noexcept { return isDemo; }
#endif

    // Read-only accessor for the registry / editor: is this instance
    // participating in cross-instance bypass linking? AudioParameterBool::get()
    // is thread-safe (atomic load), so this is callable from any thread.
    bool isLinkBypassEnabled() const noexcept {
        return linkBypassParam != nullptr && linkBypassParam->get();
    }

    // SPSC ring buffer feeding the oscilloscope. Sized to comfortably hold
    // the longest scope window (5 s) at the highest sample rate auval / hosts
    // ever throw at us (192 kHz -> 960 000 samples). Memory cost is two
    // float arrays at ~4 MB each = 8 MB total on the heap-allocated
    // processor — fine for a modern plugin.
    static constexpr int scopeSize = 1048576; // 2^20, ~5.5 s @ 192k, ~22 s @ 48k
    juce::AbstractFifo               scopeFifo { scopeSize };
    std::array<float, scopeSize>     scopePre  {};
    std::array<float, scopeSize>     scopePost {};

private:
    juce::AudioParameterFloat*  targetPeakParam = nullptr;
    juce::AudioParameterFloat*  inputGainParam  = nullptr;
    juce::AudioParameterFloat*  driveParam      = nullptr;
    juce::AudioParameterChoice* clipTypeParam   = nullptr;
    juce::AudioParameterChoice* osFactorParam   = nullptr;
    juce::AudioParameterFloat*  outputTrimParam = nullptr;
    juce::AudioParameterBool*   bypassParam     = nullptr;
    juce::AudioParameterBool*   gainMatchParam  = nullptr;
    juce::AudioParameterBool*   linkBypassParam = nullptr;
    juce::AudioParameterFloat*  preClipHpfParam = nullptr;

    // Running RMS-difference (output - input, dB) tracked only while the
    // chain is processing. When the user toggles bypass with Gain Match
    // enabled, this is applied to the dry signal so the A/B comparison
    // is loudness-matched rather than "louder = better."
    std::atomic<float> matchGainDb { 0.0f };

    juce::AudioBuffer<float> preClipBuffer;

#if CTZ_PAID_BUILD
    // ---- Demo-mode silence interrupt ---------------------------------
    // Active when this is a paid build AND no valid license is present.
    // Every `demoInterruptIntervalSamples` audio samples (default 60 s),
    // the next `demoInterruptDurationSamples` (default 300 ms) of OUTPUT
    // are forced to silence. Counters cross block boundaries so a 64-
    // sample buffer at 48 kHz behaves identically to a 1024-sample buffer.
    //
    // Placement: silencing happens AFTER all audio-chain processing but
    // BEFORE output metering / LUFS / TP, so all visible meters reflect
    // the actual (silenced) output. That's the explicit "demo dip" the
    // user can see in the metering, reinforcing the prompt to buy.
    //
    // Hard-coded `isDemo = true` until the license-check stubs land.
    // Once they do, isDemo flips to false when a valid LS key is cached.
    bool isDemo                              = true;
    int  demoInterruptIntervalSamples        = 0;  // computed in prepareToPlay
    int  demoInterruptDurationSamples        = 0;  // computed in prepareToPlay
    int  demoSamplesSinceLastInterrupt       = 0;
    int  demoSamplesIntoCurrentInterrupt     = 0;
    bool demoInInterrupt                     = false;

    void processDemoMode(juce::AudioBuffer<float>& buffer) noexcept;
#endif

    // Pre-clipper high-pass: 2nd-order Butterworth, one per channel.
    // Coefficients are redesigned in `updateHpfIfChanged()` only when the
    // user moves the slider — no per-sample recomputation.
    juce::dsp::IIR::Filter<float> preClipHpfL, preClipHpfR;
    float currentHpfHz = -1.0f;
    void updateHpfIfChanged(double sampleRate);

    // Three pre-configured oversamplers (2x, 4x, 8x). Index 0/1/2 = factors
    // 1/2/3 in the parameter (1 = 2x, 2 = 4x, 3 = 8x; param index 0 = Off
    // bypasses oversampling entirely). Built in prepareToPlay so changing
    // factor mid-playback is just a pointer swap — no allocation.
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    int   currentOsFactor = -1;  // last factor index applied to latency
    void  updateLatencyIfChanged();

    // ---- GR delay-compensation -----------------------------------------
    //
    // The oversampling chain delays the audio path by the downsampler's
    // FIR group delay (~16 samples at 2x, ~24 at 4x, ~32 at 8x). That
    // delay is reported to the host via setLatencySamples() so the host
    // time-aligns our output with other tracks -- but the same delay
    // existed inside our OWN code, comparing preClipBuffer (captured
    // pre-upsampler) against the post-clip buffer (captured post-
    // downsampler) sample-by-sample. The result: phantom GR readings
    // during transient decays, ranging from -10 dB on light material
    // to -50 dB on heavy bass-driven transients.
    //
    // The fix: a DelayLine that delays preClipBuffer by the same number
    // of samples as the OS downsampler. After delay, the pre and post
    // streams refer to the same physical instant in the audio, and the
    // bin-peak comparison in GRHistory produces correct values.
    //
    // Delay is updated on the fly when the user changes OS factor;
    // setDelay() is cheap (no buffer reallocation) since we sized the
    // line for the max OS factor in prepareToPlay.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> preClipDelay;
    juce::AudioBuffer<float> delayedPreBuffer;
    int currentOsLatencySamples = 0;

    void writeToScope(const juce::AudioBuffer<float>& pre,
                      const juce::AudioBuffer<float>& post) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipToZeroProcessor)
};
