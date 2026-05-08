#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/MeterComponent.h"
#include "UI/OscilloscopeComponent.h"

class ClipToZeroEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ClipToZeroEditor(ClipToZeroProcessor&);
    ~ClipToZeroEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    ClipToZeroProcessor& processor;

    juce::Slider     inputGainSlider, outputTrimSlider;
    juce::Label      inputGainLabel,  outputTrimLabel, clipTypeLabel;
    juce::ComboBox   clipTypeBox;
    juce::ToggleButton bypassButton  { "Bypass" };
    juce::TextButton autoGainButton  { "Auto-Gain" };
    juce::Label      autoGainStatus;

    using SliderAttach   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach    = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttach> inputGainAttach, outputTrimAttach;
    std::unique_ptr<ComboAttach>  clipTypeAttach;
    std::unique_ptr<ButtonAttach> bypassAttach;

    MeterComponent       inputMeterComp, outputMeterComp;
    OscilloscopeComponent scope;

    bool wasMeasuring = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipToZeroEditor)
};
