#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "DSP/LevelMeter.h"
#include "DSP/Clipper.h"
#include "DSP/AutoGainAnalyzer.h"

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

    // SPSC ring buffer feeding the oscilloscope.
    static constexpr int scopeSize = 4096;
    juce::AbstractFifo               scopeFifo { scopeSize };
    std::array<float, scopeSize>     scopePre  {};
    std::array<float, scopeSize>     scopePost {};

private:
    juce::AudioParameterFloat*  inputGainParam  = nullptr;
    juce::AudioParameterChoice* clipTypeParam   = nullptr;
    juce::AudioParameterFloat*  outputTrimParam = nullptr;
    juce::AudioParameterBool*   bypassParam     = nullptr;

    juce::AudioBuffer<float> preClipBuffer;

    void writeToScope(const juce::AudioBuffer<float>& pre,
                      const juce::AudioBuffer<float>& post) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipToZeroProcessor)
};
