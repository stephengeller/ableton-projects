#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/LookAndFeel_F.h"
#include "UI/StageLane.h"
#include "UI/HorizontalMeter.h"
#include "UI/OscilloscopeComponent.h"
#include "UI/GRMeterVertical.h"
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

    // Window-size constraints. Min set to the dimensions where the F
    // layout actually works without text truncation or button overlap;
    // max is generous but bounded so the scope's per-pixel paint cost
    // doesn't run wild on a 5k display.
    static constexpr int minWidth  = 720;
    static constexpr int minHeight = 560;
    static constexpr int maxWidth  = 1600;
    static constexpr int maxHeight = 1200;

private:
    ClipToZeroProcessor& processor;
    LookAndFeel_F        laf;

    // ---- Brand bar (top) ------------------------------------------------
    juce::TextButton clipTypeButton   { "CLIP-HARD" };
    juce::TextButton bypassButton     { "BYPASS" };
    juce::TextButton bypassMenuButton;  // chevron-only, opens gain-match toggle popup
    // Chain-icon toggle sitting immediately LEFT of BYPASS. Always
    // visible (lime icon when on, dim outline when off). Click toggles
    // the linkBypass APVTS param -- so the bulk action items in
    // bypassMenuButton's popup are still available, but the per-instance
    // toggle now has its own dedicated button right next to the action
    // it affects. Drawn by LookAndFeel_F when the "linkIcon" property
    // is set.
    juce::TextButton linkBypassButton { "" };
    // PRESET dropdown -- lives in the left half of the brand bar, just
    // right of the CLIP-TO-ZERO logo. Opens a popup with the factory
    // presets from Presets.h.
    juce::TextButton presetButton     { "PRESETS" };

    // ---- Scope + zoom controls -----------------------------------------
    OscilloscopeComponent scope;
    // Pro-L2-style vertical GR meter. Lives to the right of the scope,
    // spans the scope's height. Added v0.5.9 alongside a horizontal GR
    // strip for A/B comparison; the horizontal strip was removed in
    // v0.6.0 once the vertical bar proved to be the keeper.
    GRMeterVertical       grMeterVertical;
    juce::Slider          scopeLengthSlider, vertHeadroomSlider;
    juce::Label           scopeLengthLabel,  vertHeadroomLabel;
    juce::Label           scopeLengthValue,  vertHeadroomValue;
    // Spectrum overlay settings dropdown, lives at the right end of the
    // zoom row. Opens a popup with Off / Subtle / Bold choices.
    // 'VIEW' dropdown — houses the spectrum mode AND the show-hints
    // toggle. Renamed from 'SPEC' once it grew beyond just spectrum.
    juce::TextButton      viewMenuButton { "VIEW" };

    // ---- Meters --------------------------------------------------------
    juce::Label     inputMetersHeader;
    juce::Label     inputMetersTarget;   // "target X.X dBFS" (right-aligned)
    juce::Label     outputMetersHeader;
    // True-peak readout on the output header row. Mirrors the visual
    // treatment of inputMetersTarget — small mono font, right-aligned —
    // so the two meter columns stay symmetric. Lights up in overload-red
    // once TP crosses 0 dBTP, which for a clipper output is the "DAC will
    // distort here" line.
    juce::Label     outputMetersTp;
    HorizontalMeter inputMeterL, inputMeterR;
    HorizontalMeter outputMeterL, outputMeterR;

    // ---- Stage 1: Stage to 0 -------------------------------------------
    // Hint strings shortened to one-liners that always fit at min size.
    // The longer original strings stayed alive as tooltip text on the
    // stage's indicator dot, see ClipToZeroEditor::tooltipForStage().
    StageLane lane1 { 1, "Stage to 0",
                       "Press Auto-Gain, or set Input by hand." };
    Knob target { "Target", " dB", 1, false, false };
    Knob inputGain { "Input",  " dB", 2, true,  true };
    juce::TextButton autoGainButton { "AUTO-GAIN" };

    // ---- Stage 2: Drive into clipper -----------------------------------
    StageLane lane2 { 2, "Drive into clipper",
                       "Push Drive until it breaks, then back off." };
    Knob hpf   { "HPF",   " Hz", 0, false, false };
    Knob drive { "Drive", " dB", 1, true,  false };
    Knob trim  { "Trim",  " dB", 1, false, true };

    // ---- Stage 3: Judge by LUFS ----------------------------------------
    StageLane lane3 { 3, "Judge by LUFS",
                       "Optional - watch I to land on a loudness target." };
    LufsBox momentaryBox  { "M",  "400 ms" };
    LufsBox shortTermBox  { "S",  "3 s"    };
    LufsBox integratedBox { "I",  "gated"  };
    // Crest factor = peak - RMS in dB. Output-side, so it's the "verdict
    // metric" parallel to LUFS — tells you how much dynamic range survived
    // the clipping. 3 dB ~= pure sine / heavily smashed, 12+ dB = dynamic.
    LufsBox crestBox      { "CR", "P-R"    };
    juce::TextButton resetLufsButton  { "RESET INTEGRATED" };
    // Gain-Match A/B toggle moved out of Stage 3 into the BYPASS dropdown
    // (top-right brand bar). The parameter still exists in APVTS for host
    // automation; the popup-menu callback drives it directly.

    // ---- APVTS attachments ---------------------------------------------
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttach> targetAttach, inputGainAttach, driveAttach, outputTrimAttach,
                                  scopeLengthAttach, vertHeadroomAttach, hpfAttach;
    std::unique_ptr<ButtonAttach> bypassAttach;
    std::unique_ptr<ButtonAttach> linkBypassAttach;
    // (gainMatch is driven via the BYPASS dropdown menu, no attachment.)

    // ---- Editor-only state ---------------------------------------------
    bool wasMeasuring        = false;
    bool autoGainHasResult   = false;
    float lastAutoGainPeakDb = -100.0f;
    float lastAutoGainGainDb = 0.0f;
    juce::uint32 measurementStartMs = 0;
    static constexpr float autoGainWindowSeconds = 2.0f;

    // Tracks the inline-hints toggle so we only push setShowHint into
    // the lanes when the parameter actually changes.
    bool lastShowHints = true;

    // (Previously: lastOsFactorIdx for OS-driven re-layout when GR strip
    // was hidden during OS modes. Now obsolete -- v0.5.4 fixed the
    // underlying alignment so the strip is always shown.)

    // (Previously: lastLinkBypass for timer-driven indicator repaint.
    // Now obsolete -- linkBypassButton handles its own redraws via the
    // APVTS ButtonAttachment.)

    // Timestamp of the last RESET INTEGRATED click. Used to flash a
    // brief "CLEARED" confirmation on the button so the user has visible
    // feedback that the (otherwise-invisible) action happened.
    juce::uint32 resetClickedAtMs = 0;
    static constexpr juce::uint32 resetConfirmationMs = 1500;

    // Tooltips appear when the user hovers over any control set via
    // setTooltip(). One TooltipWindow per editor manages them all.
    juce::TooltipWindow tooltipWindow { this };

    void updateClipTypeButtonText();
    void updateAutoGainButton();
    void updateStageStates();
    void updateLufsAndStatus();
    void applyTooltips();
    void syncShowHintsIfChanged();

    // Apply the indexed factory preset (see Source/Presets.h) by setting
    // the seven audio-shaping APVTS parameters via setValueNotifyingHost.
    // Also clears the editor-local autoGainHasResult flag, since the
    // preset's choice of inputGain supersedes whatever Auto-Gain captured.
    void applyPreset(int presetIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipToZeroEditor)
};
