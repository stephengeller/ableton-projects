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

class ClipToZeroProcessor : public juce::AudioProcessor {
public:
    ClipToZeroProcessor();
    ~ClipToZeroProcessor() override = default;

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
    juce::AudioParameterFloat*  outputTrimParam = nullptr;
    juce::AudioParameterBool*   bypassParam     = nullptr;
    juce::AudioParameterBool*   gainMatchParam  = nullptr;
    juce::AudioParameterFloat*  preClipHpfParam = nullptr;

    // Running RMS-difference (output - input, dB) tracked only while the
    // chain is processing. When the user toggles bypass with Gain Match
    // enabled, this is applied to the dry signal so the A/B comparison
    // is loudness-matched rather than "louder = better."
    std::atomic<float> matchGainDb { 0.0f };

    juce::AudioBuffer<float> preClipBuffer;

    // Pre-clipper high-pass: 2nd-order Butterworth, one per channel.
    // Coefficients are redesigned in `updateHpfIfChanged()` only when the
    // user moves the slider — no per-sample recomputation.
    juce::dsp::IIR::Filter<float> preClipHpfL, preClipHpfR;
    float currentHpfHz = -1.0f;
    void updateHpfIfChanged(double sampleRate);

    void writeToScope(const juce::AudioBuffer<float>& pre,
                      const juce::AudioBuffer<float>& post) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipToZeroProcessor)
};
