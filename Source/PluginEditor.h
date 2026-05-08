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

    // Sliders / controls
    juce::Slider     targetPeakSlider, inputGainSlider, driveSlider, outputTrimSlider, scopeLengthSlider;
    juce::Label      targetPeakLabel,  inputGainLabel,  driveLabel,  outputTrimLabel, clipTypeLabel, scopeLengthLabel;
    juce::ComboBox   clipTypeBox;
    juce::ToggleButton bypassButton  { "Bypass" };
    juce::TextButton autoGainButton  { "Auto-Gain" };
    juce::Label      autoGainStatus;

    // LUFS panel
    juce::Label      lufsHeading;
    juce::Label      momentaryLabel, shortTermLabel, integratedLabel;
    juce::TextButton resetLufsButton { "Reset I" };

    // Meter components
    MeterComponent       inputMeterComp, outputMeterComp;
    OscilloscopeComponent scope;

    // APVTS attachments
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttach> targetPeakAttach, inputGainAttach, driveAttach, outputTrimAttach, scopeLengthAttach;
    std::unique_ptr<ComboAttach>  clipTypeAttach;
    std::unique_ptr<ButtonAttach> bypassAttach;

    bool wasMeasuring = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipToZeroEditor)
};
