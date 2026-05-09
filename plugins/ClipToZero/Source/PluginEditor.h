#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/LookAndFeel_F.h"
#include "UI/StageLane.h"
#include "UI/HorizontalMeter.h"
#include "UI/OscilloscopeComponent.h"
#include "UI/TransferCurveComponent.h"
#include "UI/LufsBox.h"
#include "UI/Knob.h"

class ClipToZeroEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ClipToZeroEditor(ClipToZeroProcessor&);
    ~ClipToZeroEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    ClipToZeroProcessor& processor;
    LookAndFeel_F        laf;

    // ---- Brand bar (top) ------------------------------------------------
    juce::TextButton clipTypeButton { "CLIP-HARD" };
    juce::TextButton bypassButton   { "BYPASS" };

    // ---- Scope + zoom controls -----------------------------------------
    OscilloscopeComponent scope;
    juce::Slider          scopeLengthSlider, vertHeadroomSlider;
    juce::Label           scopeLengthLabel,  vertHeadroomLabel;
    juce::Label           scopeLengthValue,  vertHeadroomValue;

    // ---- Meters --------------------------------------------------------
    juce::Label     inputMetersHeader;
    juce::Label     inputMetersTarget;   // "target X.X dBFS" (right-aligned)
    juce::Label     outputMetersHeader;
    HorizontalMeter inputMeterL, inputMeterR;
    HorizontalMeter outputMeterL, outputMeterR;

    // ---- Stage 1: Stage to 0 -------------------------------------------
    StageLane lane1 { 1, "Stage to 0",
                       "Get the loudest peak to 0 dBFS. Auto-Gain measures for 2 s and writes the gain - or set Input by hand." };
    Knob target { "Target", " dB", 1, false, false };
    Knob inputGain { "Input",  " dB", 2, true,  true };
    juce::TextButton autoGainButton { "AUTO-GAIN" };

    // ---- Stage 2: Drive into clipper -----------------------------------
    StageLane lane2 { 2, "Drive into clipper",
                       "Push Drive until you hear the signal break in a way you don't like - then back off. Output stays bounded at 0 dBFS." };
    Knob drive { "Drive", " dB", 2, true,  false };
    Knob trim  { "Trim",  " dB", 2, false, true };
    TransferCurveComponent transferCurve;

    // ---- Stage 3: Judge by LUFS ----------------------------------------
    StageLane lane3 { 3, "Judge by LUFS",
                       "Optional - watch integrated LUFS to land on a loudness target without going further than feels right." };
    LufsBox momentaryBox  { "M", "momentary" };
    LufsBox shortTermBox  { "S", "3-sec"     };
    LufsBox integratedBox { "I", "gated"     };
    juce::TextButton resetLufsButton { "RESET INTEGRATED" };

    // ---- APVTS attachments ---------------------------------------------
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttach> targetAttach, inputGainAttach, driveAttach, outputTrimAttach,
                                  scopeLengthAttach, vertHeadroomAttach;
    std::unique_ptr<ButtonAttach> bypassAttach;

    // ---- Editor-only state ---------------------------------------------
    bool wasMeasuring        = false;
    bool autoGainHasResult   = false;
    float lastAutoGainPeakDb = -100.0f;
    float lastAutoGainGainDb = 0.0f;
    juce::uint32 measurementStartMs = 0;
    static constexpr float autoGainWindowSeconds = 2.0f;

    void updateClipTypeButtonText();
    void updateAutoGainButton();
    void updateStageStates();
    void updateLufsAndStatus();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipToZeroEditor)
};
