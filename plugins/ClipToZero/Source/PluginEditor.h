#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/LookAndFeel_F.h"
#include "UI/StageLane.h"
#include "UI/HorizontalMeter.h"
#include "UI/OscilloscopeComponent.h"
// TransferCurveComponent was used in the original F design but dropped in
// favour of a pre-clipper HPF knob in Stage 2 (see feat/03-hpf). Header kept
// available in case a future variant wants to bring it back.
#include "UI/LufsBox.h"
#include "UI/Knob.h"

class ClipToZeroEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ClipToZeroEditor(ClipToZeroProcessor&);
    ~ClipToZeroEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // Window-size constraints. Min is tight enough that all stage-card
    // controls still fit cleanly; max is generous but bounded so the
    // scope's per-pixel paint cost doesn't run wild on a 5k display.
    static constexpr int minWidth  = 600;
    static constexpr int minHeight = 500;
    static constexpr int maxWidth  = 1600;
    static constexpr int maxHeight = 1200;

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
    Knob hpf   { "HPF",   " Hz", 0, false, false };
    Knob drive { "Drive", " dB", 1, true,  false };
    Knob trim  { "Trim",  " dB", 1, false, true };

    // ---- Stage 3: Judge by LUFS ----------------------------------------
    StageLane lane3 { 3, "Judge by LUFS",
                       "Optional - watch integrated LUFS to land on a loudness target without going further than feels right." };
    LufsBox momentaryBox  { "M",  "400 ms" };
    LufsBox shortTermBox  { "S",  "3 s"    };
    LufsBox integratedBox { "I",  "gated"  };
    // Crest factor = peak - RMS in dB. Output-side, so it's the "verdict
    // metric" parallel to LUFS — tells you how much dynamic range survived
    // the clipping. 3 dB ~= pure sine / heavily smashed, 12+ dB = dynamic.
    LufsBox crestBox      { "CR", "P-R"    };
    juce::TextButton resetLufsButton  { "RESET I" };
    // Toggle: when on, bypassed signal is gain-compensated to match the
    // processed signal's loudness — eliminating the "louder = better"
    // bias when A/B-ing dry vs wet.
    juce::TextButton gainMatchButton  { "A/B MATCH" };

    // ---- APVTS attachments ---------------------------------------------
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttach> targetAttach, inputGainAttach, driveAttach, outputTrimAttach,
                                  scopeLengthAttach, vertHeadroomAttach, hpfAttach;
    std::unique_ptr<ButtonAttach> bypassAttach, gainMatchAttach;

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
