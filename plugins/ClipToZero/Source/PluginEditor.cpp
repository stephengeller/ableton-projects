#include "PluginEditor.h"

namespace {
    juce::String formatLUFS(float l) {
        return (l <= -69.0f) ? juce::String("-inf") : juce::String(l, 1);
    }

    juce::String fmtDb(float v, int decimals = 1, bool signedDb = false) {
        if (signedDb && v > 0.0f) return "+" + juce::String(v, decimals) + " dB";
        return juce::String(v, decimals) + " dB";
    }
}

ClipToZeroEditor::ClipToZeroEditor(ClipToZeroProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      scope(p),
      grMeter(p),
      inputMeterL (p.inputMeter,  "L", 0),
      inputMeterR (p.inputMeter,  "R", 1),
      outputMeterL(p.outputMeter, "L", 0),
      outputMeterR(p.outputMeter, "R", 1)
{
    setLookAndFeel(&laf);

    // Resizable. The bottom-right corner gets a visible drag handle.
    setResizable(true, true);
    setResizeLimits(minWidth, minHeight, maxWidth, maxHeight);

    // Restore the user's last-chosen size from APVTS state (or fall back
    // to the design's intended 720x580). Saved back in resized().
    const int savedW = static_cast<int>(processor.apvts.state.getProperty("editorWidth",  720));
    const int savedH = static_cast<int>(processor.apvts.state.getProperty("editorHeight", 580));
    setSize(juce::jlimit(minWidth,  maxWidth,  savedW),
            juce::jlimit(minHeight, maxHeight, savedH));

    // ---- Brand bar buttons ---------------------------------------------
    // Clip-type now has 4 options (Hard / Soft / Poly / Tube). A simple
    // toggle would require 4 clicks to cycle, so we open a popup menu
    // instead — matches host-DAW conventions for multi-state controls.
    // The "dropdown" property tells LookAndFeel_F to render a small
    // chevron at the right edge so the menu-ness is visible.
    clipTypeButton.setClickingTogglesState(false);
    clipTypeButton.getProperties().set("dropdown", true);
    clipTypeButton.onClick = [this] {
        // The CLIP-XXX button now opens a TWO-section menu: Clip Curve
        // on top, Oversampling below. Both settings are "how the clipper
        // is configured" so keeping them under one menu spares space in
        // an increasingly busy brand bar.
        auto* clipChoice = dynamic_cast<juce::AudioParameterChoice*>(
            processor.apvts.getParameter(Param::clipType));
        auto* osChoice = dynamic_cast<juce::AudioParameterChoice*>(
            processor.apvts.getParameter(Param::osFactor));
        if (clipChoice == nullptr) return;

        juce::PopupMenu menu;
        menu.addSectionHeader("Clip Curve");
        for (int i = 0; i < clipChoice->choices.size(); ++i)
            menu.addItem(100 + i, clipChoice->choices[i],
                         /*enabled=*/true, /*ticked=*/i == clipChoice->getIndex());
        if (osChoice != nullptr) {
            menu.addSeparator();
            menu.addSectionHeader("Oversampling");
            for (int i = 0; i < osChoice->choices.size(); ++i)
                menu.addItem(200 + i, osChoice->choices[i],
                             /*enabled=*/true, /*ticked=*/i == osChoice->getIndex());
        }

        menu.showMenuAsync(juce::PopupMenu::Options()
                               .withTargetComponent(&clipTypeButton)
                               .withMinimumWidth(160),
                           [clipChoice, osChoice](int result) {
                               if (result <= 0) return;  // menu dismissed
                               auto applyChoice = [](juce::AudioParameterChoice* choice, int idx) {
                                   const int total = choice->choices.size();
                                   if (total <= 1) return;
                                   choice->beginChangeGesture();
                                   choice->setValueNotifyingHost(
                                       static_cast<float>(idx) / static_cast<float>(total - 1));
                                   choice->endChangeGesture();
                               };
                               if (result >= 100 && result < 200)
                                   applyChoice(clipChoice, result - 100);
                               else if (result >= 200 && result < 300 && osChoice != nullptr)
                                   applyChoice(osChoice, result - 200);
                           });
    };
    addAndMakeVisible(clipTypeButton);

    bypassButton.setClickingTogglesState(true);
    bypassButton.getProperties().set("variant", "warning");
    addAndMakeVisible(bypassButton);
    bypassAttach = std::make_unique<ButtonAttach>(p.apvts, Param::bypass, bypassButton);

    // Tiny chevron button to the right of BYPASS. Opens a popup menu with
    // the Gain-Match A/B toggle. Lives here because gain-matching is
    // semantically about bypass-time A/B fairness — there's no reason for
    // it to sit elsewhere in the editor.
    bypassMenuButton.setClickingTogglesState(false);
    bypassMenuButton.getProperties().set("dropdown", true);
    bypassMenuButton.onClick = [this] {
        auto* gp = processor.apvts.getParameter(Param::gainMatch);
        if (gp == nullptr) return;
        const bool currentlyOn = gp->getValue() >= 0.5f;

        juce::PopupMenu menu;
        menu.addItem(1, "Gain-Matched A/B", true, currentlyOn);
        menu.showMenuAsync(juce::PopupMenu::Options()
                               .withTargetComponent(&bypassMenuButton)
                               .withMinimumWidth(160),
                           [gp, currentlyOn](int result) {
                               if (result != 1) return;
                               gp->beginChangeGesture();
                               gp->setValueNotifyingHost(currentlyOn ? 0.0f : 1.0f);
                               gp->endChangeGesture();
                           });
    };
    addAndMakeVisible(bypassMenuButton);

    // ---- Scope ---------------------------------------------------------
    addAndMakeVisible(scope);
    addAndMakeVisible(grMeter);

    // ---- Zoom sliders --------------------------------------------------
    auto styleZoom = [](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    };
    styleZoom(scopeLengthSlider);
    styleZoom(vertHeadroomSlider);

    auto styleSubtleLabel = [](juce::Label& l, const juce::String& text, juce::Justification j) {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, Theme::textDim);
        l.setFont(Theme::mono(8.5f, juce::Font::bold));
        l.setJustificationType(j);
    };
    styleSubtleLabel(scopeLengthLabel,  "<> ZOOM",       juce::Justification::centredLeft);
    styleSubtleLabel(vertHeadroomLabel, "^v ABOVE 0 dB", juce::Justification::centredLeft);

    auto styleValueLabel = [](juce::Label& l) {
        l.setColour(juce::Label::textColourId, Theme::textBright);
        l.setFont(Theme::mono(10.0f));
        l.setJustificationType(juce::Justification::centredRight);
    };
    styleValueLabel(scopeLengthValue);
    styleValueLabel(vertHeadroomValue);

    addAndMakeVisible(scopeLengthLabel);
    addAndMakeVisible(scopeLengthSlider);
    addAndMakeVisible(scopeLengthValue);
    addAndMakeVisible(vertHeadroomLabel);
    addAndMakeVisible(vertHeadroomSlider);
    addAndMakeVisible(vertHeadroomValue);

    // VIEW dropdown — lives at the right end of the zoom row, alongside
    // the other "scope view" controls. Two-section menu: spectrum mode
    // (Off/Subtle/Bold) on top, view toggles (Show hints) below.
    viewMenuButton.setClickingTogglesState(false);
    viewMenuButton.getProperties().set("dropdown", true);
    viewMenuButton.onClick = [this] {
        auto* specChoice = dynamic_cast<juce::AudioParameterChoice*>(
            processor.apvts.getParameter(Param::spectrumMode));
        auto* hintsBool = dynamic_cast<juce::AudioParameterBool*>(
            processor.apvts.getParameter(Param::showHints));
        if (specChoice == nullptr) return;

        juce::PopupMenu menu;
        menu.addSectionHeader("Spectrum");
        for (int i = 0; i < specChoice->choices.size(); ++i)
            menu.addItem(100 + i, specChoice->choices[i],
                         /*enabled=*/true, /*ticked=*/i == specChoice->getIndex());

        if (hintsBool != nullptr) {
            menu.addSeparator();
            menu.addSectionHeader("View");
            menu.addItem(200, "Show stage hints",
                         /*enabled=*/true, /*ticked=*/hintsBool->get());
        }

        menu.showMenuAsync(juce::PopupMenu::Options()
                               .withTargetComponent(&viewMenuButton)
                               .withMinimumWidth(180),
                           [specChoice, hintsBool](int result) {
                               if (result <= 0) return;
                               if (result >= 100 && result < 200) {
                                   const int newIdx = result - 100;
                                   const int total  = specChoice->choices.size();
                                   if (total <= 1) return;
                                   specChoice->beginChangeGesture();
                                   specChoice->setValueNotifyingHost(
                                       static_cast<float>(newIdx) / static_cast<float>(total - 1));
                                   specChoice->endChangeGesture();
                               } else if (result == 200 && hintsBool != nullptr) {
                                   const bool currently = hintsBool->get();
                                   hintsBool->beginChangeGesture();
                                   hintsBool->setValueNotifyingHost(currently ? 0.0f : 1.0f);
                                   hintsBool->endChangeGesture();
                               }
                           });
    };
    addAndMakeVisible(viewMenuButton);

    scopeLengthAttach = std::make_unique<SliderAttach>(p.apvts, Param::scopeLen,    scopeLengthSlider);
    vertHeadroomAttach = std::make_unique<SliderAttach>(p.apvts, Param::vertHeadroom, vertHeadroomSlider);

    scopeLengthSlider.onValueChange = [this] {
        const auto v = static_cast<float>(scopeLengthSlider.getValue());
        const auto txt = (v < 10.0f) ? juce::String(v, 1) + " ms"
                       : (v < 100.0f) ? juce::String(v, 1) + " ms"
                                      : juce::String(v, 0) + " ms";
        scopeLengthValue.setText(txt, juce::dontSendNotification);
    };
    vertHeadroomSlider.onValueChange = [this] {
        vertHeadroomValue.setText(juce::String(vertHeadroomSlider.getValue(), 1) + " dB",
                                  juce::dontSendNotification);
    };
    scopeLengthSlider.onValueChange();
    vertHeadroomSlider.onValueChange();

    // ---- Meters --------------------------------------------------------
    auto headerStyle = [](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, Theme::textDim);
        l.setFont(Theme::mono(8.5f, juce::Font::bold));
        l.setJustificationType(juce::Justification::centredLeft);
    };
    headerStyle(inputMetersHeader,  "INPUT - L/R");
    headerStyle(outputMetersHeader, "OUTPUT - L/R");
    inputMetersTarget.setColour(juce::Label::textColourId, Theme::textVeryDim);
    inputMetersTarget.setFont(Theme::mono(8.5f));
    inputMetersTarget.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(inputMetersHeader);
    addAndMakeVisible(inputMetersTarget);
    addAndMakeVisible(outputMetersHeader);
    addAndMakeVisible(inputMeterL);
    addAndMakeVisible(inputMeterR);
    addAndMakeVisible(outputMeterL);
    addAndMakeVisible(outputMeterR);

    // ---- Stage 1 -------------------------------------------------------
    addAndMakeVisible(lane1);
    target.slider().setRange(-12.0, 0.0, 0.1);
    target.slider().setDoubleClickReturnValue(true, 0.0);
    inputGain.slider().setDoubleClickReturnValue(true, 0.0);
    lane1.addAndMakeVisible(target);
    lane1.addAndMakeVisible(inputGain);

    autoGainButton.getProperties().set("variant", "primary");
    autoGainButton.onClick = [this] {
        processor.autoGain.startMeasurement(autoGainWindowSeconds);
        measurementStartMs = juce::Time::getMillisecondCounter();
        wasMeasuring = true;
        updateAutoGainButton();
    };
    lane1.addAndMakeVisible(autoGainButton);

    targetAttach    = std::make_unique<SliderAttach>(p.apvts, Param::targetPeak, target.slider());
    inputGainAttach = std::make_unique<SliderAttach>(p.apvts, Param::inputGain,  inputGain.slider());

    // ---- Stage 2 -------------------------------------------------------
    addAndMakeVisible(lane2);
    drive.slider().setDoubleClickReturnValue(true, 0.0);
    trim .slider().setDoubleClickReturnValue(true, 0.0);
    hpf  .slider().setDoubleClickReturnValue(true, 20.0);  // 20 Hz = bypassed
    hpf  .setMinValueLabel("OFF");                          // explicit UI cue
    lane2.addAndMakeVisible(hpf);
    lane2.addAndMakeVisible(drive);
    lane2.addAndMakeVisible(trim);
    driveAttach      = std::make_unique<SliderAttach>(p.apvts, Param::drive,      drive.slider());
    outputTrimAttach = std::make_unique<SliderAttach>(p.apvts, Param::outputTrim, trim.slider());
    hpfAttach        = std::make_unique<SliderAttach>(p.apvts, Param::preClipHpf, hpf.slider());

    // ---- Stage reset handlers (clicking the green checkmark) ----------
    // Resetting the parameters that determine "done"-ness lets the
    // auto-progression state machine naturally re-evaluate and demote
    // the lane back to active/idle. No extra "manually reset" flag needed.
    lane1.onResetClicked = [this] {
        autoGainHasResult = false;
        if (auto* p = processor.apvts.getParameter(Param::inputGain)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(0.0f));
            p->endChangeGesture();
        }
    };
    lane2.onResetClicked = [this] {
        if (auto* p = processor.apvts.getParameter(Param::drive)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(0.0f));
            p->endChangeGesture();
        }
    };

    // ---- Stage 3 -------------------------------------------------------
    addAndMakeVisible(lane3);
    lane3.addAndMakeVisible(momentaryBox);
    lane3.addAndMakeVisible(shortTermBox);
    lane3.addAndMakeVisible(integratedBox);
    lane3.addAndMakeVisible(crestBox);
    resetLufsButton.onClick = [this] {
        processor.lufs.requestResetIntegrated();
        // Brief confirmation: swap text to "CLEARED" for resetConfirmationMs,
        // then timerCallback reverts. Gives a visible signal that the
        // otherwise-invisible-on-the-LUFS-meter action actually fired.
        resetClickedAtMs = juce::Time::getMillisecondCounter();
        resetLufsButton.setButtonText("CLEARED");
    };
    lane3.addAndMakeVisible(resetLufsButton);

    updateClipTypeButtonText();
    updateAutoGainButton();
    updateStageStates();
    applyTooltips();

    // Sync the initial hint visibility from the loaded parameter value.
    syncShowHintsIfChanged();

    startTimerHz(15);
}

ClipToZeroEditor::~ClipToZeroEditor() {
    setLookAndFeel(nullptr);
}

void ClipToZeroEditor::updateClipTypeButtonText() {
    const auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
        processor.apvts.getParameter(Param::clipType));
    if (choice == nullptr) return;
    const int idx = choice->getIndex();
    if (idx < 0 || idx >= choice->choices.size()) return;
    clipTypeButton.setButtonText("CLIP-" + choice->choices[idx].toUpperCase());
}

void ClipToZeroEditor::updateAutoGainButton() {
    const bool measuring = processor.autoGain.isMeasuring();
    autoGainButton.getProperties().set("measuring", measuring);
    if (measuring) {
        const float elapsed = (juce::Time::getMillisecondCounter() - measurementStartMs) * 0.001f;
        const float remaining = juce::jmax(0.0f, autoGainWindowSeconds - elapsed);
        autoGainButton.setButtonText("MEAS " + juce::String(remaining, 1) + "s");
    } else {
        autoGainButton.setButtonText("AUTO-GAIN");
    }
    autoGainButton.repaint();
}

void ClipToZeroEditor::updateStageStates() {
    const float target_dB = processor.apvts.getRawParameterValue(Param::targetPeak)->load();
    const float inputGain_dB = processor.apvts.getRawParameterValue(Param::inputGain)->load();
    const float drive_dB = processor.apvts.getRawParameterValue(Param::drive)->load();

    const float peakInDb = juce::jmax(processor.inputMeter.getPeakDb(0),
                                      processor.inputMeter.getPeakDb(1));
    const bool stagedNearTarget = autoGainHasResult
        || (peakInDb + inputGain_dB >= target_dB - 1.5f
            && peakInDb + inputGain_dB <= target_dB + 0.5f);
    const bool measuring = processor.autoGain.isMeasuring();
    const bool step1Done = autoGainHasResult || (std::abs(inputGain_dB) > 0.001f && stagedNearTarget);
    const bool step2Done = drive_dB >= 0.5f;
    const bool step1Active = !step1Done && !measuring;
    const bool step2Active = step1Done && !step2Done && !measuring;
    const bool step3Active = step2Done;

    lane1.setState(step1Done   ? StageLane::State::Done
                  : step1Active ? StageLane::State::Active
                                : StageLane::State::Idle);
    lane2.setState(step2Done   ? StageLane::State::Done
                  : step2Active ? StageLane::State::Active
                                : StageLane::State::Idle);
    lane3.setState(step3Active ? StageLane::State::Active : StageLane::State::Idle);

    // Status lines
    if (autoGainHasResult) {
        lane1.setStatus("peak " + fmtDb(lastAutoGainPeakDb, 1)
                       + " -> applied " + fmtDb(lastAutoGainGainDb, 2, true));
    } else if (measuring) {
        lane1.setStatus("capturing peak over 2 s window...");
    } else {
        lane1.setStatus("play the loudest section, then press.");
    }

    if (drive_dB > 0.001f) {
        const float cutDb = peakInDb + inputGain_dB + drive_dB;
        lane2.setStatus("cutting -" + juce::String(juce::jmax(0.0f, cutDb), 1)
                       + " dB off peaks - ceiling 0 dBFS");
    } else {
        lane2.setStatus("turn drive up to slam transients past 0 dBFS.");
    }

    inputMetersTarget.setText("target " + juce::String(target_dB, 1) + " dBFS",
                              juce::dontSendNotification);
    inputMetersTarget.setColour(juce::Label::textColourId,
                                stagedNearTarget ? Theme::accent : Theme::textVeryDim);
}

void ClipToZeroEditor::updateLufsAndStatus() {
    momentaryBox .setValue(processor.lufs.getMomentaryLUFS());
    shortTermBox .setValue(processor.lufs.getShortTermLUFS());
    integratedBox.setValue(processor.lufs.getIntegratedLUFS());

    // Crest factor: peak dB - RMS dB on the louder output channel.
    // If either reading is at "no signal" (-100), report -inf rather than
    // a misleading near-zero value.
    const float outPeakDb = juce::jmax(processor.outputMeter.getPeakDb(0),
                                       processor.outputMeter.getPeakDb(1));
    const float outRmsDb  = juce::jmax(processor.outputMeter.getRmsDb(0),
                                       processor.outputMeter.getRmsDb(1));
    const float crestDb = (outPeakDb <= -69.0f || outRmsDb <= -69.0f)
                            ? -100.0f
                            : outPeakDb - outRmsDb;
    crestBox.setValue(crestDb);

    const float intLufs = processor.lufs.getIntegratedLUFS();
    if (intLufs > -69.0f) {
        const char* tag = intLufs > -10.0f ? "streaming-loud"
                       : intLufs > -16.0f ? "modern-loud"
                                          : "broadcast-safe";
        lane3.setStatus("integrated " + juce::String(intLufs, 1) + " LUFS - " + tag);
    } else {
        lane3.setStatus("integrated converging - wait or reset.");
    }
}

void ClipToZeroEditor::timerCallback() {
    // Auto-gain falling-edge: write the suggested gain back to the host.
    const bool nowMeasuring = processor.autoGain.isMeasuring();
    if (wasMeasuring && !nowMeasuring) {
        const float t = processor.apvts.getRawParameterValue(Param::targetPeak)->load();
        const float peakDb = processor.autoGain.getMeasuredPeakDb();
        const float suggested = juce::jlimit(-24.0f, 24.0f,
                                             processor.autoGain.getSuggestedGainDb(t));

        if (auto* gp = processor.apvts.getParameter(Param::inputGain)) {
            gp->beginChangeGesture();
            gp->setValueNotifyingHost(gp->convertTo0to1(suggested));
            gp->endChangeGesture();
        }
        autoGainHasResult = true;
        lastAutoGainPeakDb = peakDb;
        lastAutoGainGainDb = suggested;
    }
    wasMeasuring = nowMeasuring;

    updateClipTypeButtonText();   // reflects external automation too
    updateAutoGainButton();
    updateStageStates();
    updateLufsAndStatus();
    syncShowHintsIfChanged();

    // Show/hide the GR strip based on the current oversampling state.
    // (GR isn't reliable with OS on -- see PluginProcessor::processBlock.)
    if (auto* osParam = dynamic_cast<juce::AudioParameterChoice*>(
            processor.apvts.getParameter(Param::osFactor))) {
        const int idx = osParam->getIndex();
        if (idx != lastOsFactorIdx) {
            lastOsFactorIdx = idx;
            resized();  // re-layout: GR strip appears/disappears
        }
    }

    // Revert RESET INTEGRATED button text after the confirmation window.
    if (resetClickedAtMs > 0) {
        const auto elapsed = juce::Time::getMillisecondCounter() - resetClickedAtMs;
        if (elapsed > resetConfirmationMs) {
            resetLufsButton.setButtonText("RESET INTEGRATED");
            resetClickedAtMs = 0;
        }
    }
}

void ClipToZeroEditor::syncShowHintsIfChanged() {
    auto* p = processor.apvts.getRawParameterValue(Param::showHints);
    if (p == nullptr) return;
    const bool want = p->load() > 0.5f;
    if (want == lastShowHints) return;
    lastShowHints = want;
    lane1.setShowHint(want);
    lane2.setShowHint(want);
    lane3.setShowHint(want);
}

void ClipToZeroEditor::applyTooltips() {
    // Set on every interactive control so the user can hover and learn
    // what each knob/button does without leaving the editor. A single
    // juce::TooltipWindow member handles the display/dismiss timing.
    target   .setTooltip("Auto-Gain target peak in dBFS (where Auto-Gain stages Input to).");
    inputGain.setTooltip("Pre-clipper input gain. Press Auto-Gain to set automatically.");
    autoGainButton.setTooltip("Capture peak over 2 seconds and stage Input to Target.");
    hpf      .setTooltip("Pre-clipper high-pass. OFF at the minimum. Cleans sub-bass before clipping.");
    drive    .setTooltip("Post-stage gain into the clipper. Output stays bounded by the 0 dBFS ceiling.");
    trim     .setTooltip("Output gain after the clipper.");
    scopeLengthSlider.setTooltip("Time window shown on the scope (1 ms to 10 s).");
    vertHeadroomSlider.setTooltip("How many dB above 0 dBFS the scope shows (so heavy clipping stays visible).");
    clipTypeButton   .setTooltip("Clip curve (Hard / Soft / Poly / Tube) and oversampling factor.");
    bypassButton     .setTooltip("Bypass all processing.");
    bypassMenuButton .setTooltip("Bypass options - toggle Gain-Matched A/B level compensation.");
    viewMenuButton   .setTooltip("View settings - spectrum overlay mode and stage-hint visibility.");
    resetLufsButton  .setTooltip("Clear the accumulated Integrated LUFS measurement.");

    // Per-lane tooltip (shown when hovering the indicator dot or empty
    // lane area; knob/button tooltips win over their own bounds).
    lane1.setTooltip("Stage 1 - Stage to 0: get the loudest peak to the target dBFS. "
                     "Press Auto-Gain to capture the peak over 2 s and apply automatically, "
                     "or drag the Input knob by hand.");
    lane2.setTooltip("Stage 2 - Drive into clipper: push Drive until you hear the signal "
                     "break in a way you don't like, then back off. Output stays bounded "
                     "at 0 dBFS by the clipper ceiling.");
    lane3.setTooltip("Stage 3 - Judge by LUFS: optional. Watch the Integrated LUFS to land "
                     "on a loudness target without driving further than feels right.");

    // Meters and LUFS readouts.
    inputMeterL .setTooltip("Input peak / RMS (left channel), pre-processing.");
    inputMeterR .setTooltip("Input peak / RMS (right channel), pre-processing.");
    outputMeterL.setTooltip("Output peak / RMS (left channel), post-processing.");
    outputMeterR.setTooltip("Output peak / RMS (right channel), post-processing.");
    momentaryBox .setTooltip("Momentary loudness (ITU-R BS.1770, 400 ms window).");
    shortTermBox .setTooltip("Short-term loudness (3 s window).");
    integratedBox.setTooltip("Integrated loudness - gated mean since last reset.");
    crestBox     .setTooltip("Crest factor: peak minus RMS (dB). High = dynamic, low = squashed.");

    // Pointing-hand cursor on all interactive buttons -- the third signal
    // (after tooltip + LookAndFeel hover state) that says "this is clickable".
    auto pointerOnHover = [](juce::Component& c) {
        c.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    };
    pointerOnHover(autoGainButton);
    pointerOnHover(resetLufsButton);
    pointerOnHover(clipTypeButton);
    pointerOnHover(bypassButton);
    pointerOnHover(bypassMenuButton);
    pointerOnHover(viewMenuButton);
}

void ClipToZeroEditor::paint(juce::Graphics& g) {
    g.fillAll(Theme::bg);

    // Brand bar separator (thin line across the bottom of the brand row).
    // Bar height is now 40 px (was 48) -- the 'STAGE -> DRIVE -> JUDGE'
    // workflow caption that used to sit beneath the logo is gone, since
    // the three numbered stage cards spell out the same thing.
    constexpr int brandBarH = 40;
    g.setColour(Theme::border);
    g.fillRect(0, brandBarH - 1, getWidth(), 1);

    // Logo: vertically centred in the now-slimmer brand bar.
    g.setColour(Theme::textBright);
    g.setFont(Theme::mono(13.0f, juce::Font::bold));
    const int logoY = (brandBarH - 16) / 2;  // centred
    auto logoArea = juce::Rectangle<int>(18, logoY, 200, 16);

    // We draw "CLIP" + accent dot + "TO" + accent dot + "ZERO" by hand to
    // place coloured dots between the segments.
    int x = logoArea.getX();
    auto drawSeg = [&](const juce::String& s, juce::Colour col) {
        g.setColour(col);
        const int w = Theme::mono(13.0f, juce::Font::bold).getStringWidth(s);
        g.drawText(s, x, logoArea.getY(), w + 2, logoArea.getHeight(), juce::Justification::centredLeft);
        x += w;
    };
    drawSeg("CLIP", Theme::textBright);
    drawSeg("-",    Theme::accent);
    drawSeg("TO",   Theme::textBright);
    drawSeg("-",    Theme::accent);
    drawSeg("ZERO", Theme::textBright);

    // Sample-rate readout (right side of brand bar).
    g.setColour(Theme::textDim);
    g.setFont(Theme::mono(9.5f));
    const int sr = static_cast<int>(processor.getSampleRate());
    const auto srText = juce::String(sr / 1000) + "k";
    g.drawText("SR " + srText,
               juce::Rectangle<int>(getWidth() - 270, logoY, 60, 16),
               juce::Justification::centredLeft);
}

void ClipToZeroEditor::resized() {
    auto r = getLocalBounds();

    // Persist the current size so it survives project save/reopen and
    // future plugin instances on the same project pick up the user's
    // last-chosen geometry. apvts.state is a ValueTree — non-parameter
    // properties ride along in getStateInformation/setStateInformation.
    processor.apvts.state.setProperty("editorWidth",  getWidth(),  nullptr);
    processor.apvts.state.setProperty("editorHeight", getHeight(), nullptr);

    // ---- Brand bar (fixed 40px) ---------------------------------------
    // Right-aligned cluster, peeling buttons off the right edge:
    //   [ ... clipType (with chevron) ][ bypass ][ bypassMenu ]
    // The clipType button now has a dropdown chevron, and BYPASS gets a
    // tiny chevron-only sibling for its gain-match menu.
    auto brand = r.removeFromTop(40);
    brand.removeFromTop(8);
    brand.removeFromBottom(7);
    auto brandRight = brand.removeFromRight(220);
    brandRight.removeFromRight(18);
    bypassMenuButton.setBounds(brandRight.removeFromRight(18).withSizeKeepingCentre(18, 22));
    bypassButton    .setBounds(brandRight.removeFromRight(72).withSizeKeepingCentre(72, 22));
    brandRight.removeFromRight(8);
    clipTypeButton  .setBounds(brandRight.removeFromRight(92).withSizeKeepingCentre(92, 22));

    // ---- Scope (flex height) ------------------------------------------
    // Fixed sections after scope:
    //   gap6 + [GR (24) + gap6 if shown] + zoom28 + gap10 + meter44 +
    //   gap10 + bottomPad12.
    // GR strip is hidden when oversampling is active (its per-bin peak
    // comparison gets phantom readings from the OS downsample FIR's
    // group delay); see PluginProcessor::processBlock comment.
    r.removeFromTop(6);
    constexpr int grStripH = 24;
    const bool grShown = (lastOsFactorIdx == 0);
    const int grSectionH = grShown ? (grStripH + 6) : 0;
    const int fixedAfterScope = grSectionH + 28 + 10 + 44 + 10 + 12;
    const int flexHeight = juce::jmax(280, r.getHeight() - fixedAfterScope);
    const int scopeH     = juce::jmax(120, static_cast<int>(flexHeight * 0.55f));
    auto scopeArea = r.removeFromTop(scopeH).reduced(18, 0);
    scope.setBounds(scopeArea);

    // ---- GR history strip (24px, conditional) -------------------------
    if (grShown) {
        grMeter.setVisible(true);
        r.removeFromTop(6);
        auto grArea = r.removeFromTop(grStripH).reduced(18, 0);
        grMeter.setBounds(grArea);
    } else {
        grMeter.setVisible(false);
    }

    // ---- Zoom controls row (28px) -------------------------------------
    r.removeFromTop(6);
    auto zoomRow = r.removeFromTop(28).reduced(18, 0);
    // Carve the VIEW dropdown off the right edge first, then split the
    // remaining width 50/50 between the two zoom sliders.
    viewMenuButton.setBounds(zoomRow.removeFromRight(60).withSizeKeepingCentre(60, 22));
    zoomRow.removeFromRight(10);
    auto zoomLeft  = zoomRow.removeFromLeft(zoomRow.getWidth() / 2 - 9);
    zoomRow.removeFromLeft(18);
    auto zoomRight = zoomRow;
    auto layoutZoom = [&](juce::Rectangle<int> area, juce::Label& lbl, juce::Slider& sl, juce::Label& val) {
        lbl.setBounds(area.removeFromLeft(80));
        val.setBounds(area.removeFromRight(58));
        area.removeFromLeft(6);
        area.removeFromRight(6);
        sl.setBounds(area);
    };
    layoutZoom(zoomLeft,  scopeLengthLabel, scopeLengthSlider, scopeLengthValue);
    layoutZoom(zoomRight, vertHeadroomLabel, vertHeadroomSlider, vertHeadroomValue);

    // ---- Meters row (44px) --------------------------------------------
    r.removeFromTop(10);
    auto metersRow = r.removeFromTop(44).reduced(18, 0);
    auto leftMeters  = metersRow.removeFromLeft(metersRow.getWidth() / 2 - 9);
    metersRow.removeFromLeft(18);
    auto rightMeters = metersRow;

    auto layoutMeterColumn = [](juce::Rectangle<int> area, juce::Label& header,
                                juce::Label* targetLabel,
                                HorizontalMeter& mL, HorizontalMeter& mR) {
        auto headerRow = area.removeFromTop(12);
        if (targetLabel != nullptr) {
            const int targetW = 110;
            targetLabel->setBounds(headerRow.removeFromRight(targetW));
        }
        header.setBounds(headerRow);
        area.removeFromTop(2);
        const int rowH = (area.getHeight() - 2) / 2;
        mL.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(2);
        mR.setBounds(area.removeFromTop(rowH));
    };
    layoutMeterColumn(leftMeters,  inputMetersHeader,  &inputMetersTarget, inputMeterL, inputMeterR);
    layoutMeterColumn(rightMeters, outputMetersHeader, nullptr,            outputMeterL, outputMeterR);

    // ---- Three stage lanes (flex remaining height) --------------------
    r.removeFromTop(10);
    r.removeFromBottom(12);
    auto lanesRow = r.reduced(18, 0);
    const int laneW = (lanesRow.getWidth() - 12) / 3;
    auto a = lanesRow.removeFromLeft(laneW);
    lanesRow.removeFromLeft(6);
    auto b = lanesRow.removeFromLeft(laneW);
    lanesRow.removeFromLeft(6);
    auto c = lanesRow;
    lane1.setBounds(a);
    lane2.setBounds(b);
    lane3.setBounds(c);

    // ---- Lane 1 contents -----------------------------------------------
    auto lane1Content = lane1.getContentBounds();
    {
        // Knobs are now 75 px tall (Knob's tighter spacing); the Auto-Gain
        // button shrinks from 56 to 40 px so it no longer visually
        // dominates the lane.
        const int knobBlockW = 48;
        const int bigBlockW  = 58;
        auto col = lane1Content;
        target   .setBounds(col.removeFromLeft(knobBlockW).withSizeKeepingCentre(knobBlockW, 75));
        col.removeFromLeft(6);
        inputGain.setBounds(col.removeFromLeft(bigBlockW).withSizeKeepingCentre(bigBlockW, 75));
        col.removeFromLeft(8);
        autoGainButton.setBounds(col.withSizeKeepingCentre(col.getWidth(), 40));
    }

    // ---- Lane 2 contents -----------------------------------------------
    auto lane2Content = lane2.getContentBounds();
    {
        // Stage 2 now hosts three knobs: HPF | Drive | Trim. With the
        // available width split evenly the value-text labels (e.g.
        // "+12.5 dB") have room to render without truncation.
        auto col = lane2Content;
        const int gap = 6;
        const int knobBlockW = (col.getWidth() - 2 * gap) / 3;
        hpf  .setBounds(col.removeFromLeft(knobBlockW).withSizeKeepingCentre(knobBlockW, 75));
        col.removeFromLeft(gap);
        drive.setBounds(col.removeFromLeft(knobBlockW).withSizeKeepingCentre(knobBlockW, 75));
        col.removeFromLeft(gap);
        trim .setBounds(col.removeFromLeft(knobBlockW).withSizeKeepingCentre(knobBlockW, 75));
    }

    // ---- Lane 3 contents -----------------------------------------------
    auto lane3Content = lane3.getContentBounds();
    {
        auto top = lane3Content.removeFromTop(54);
        const int gap = 4;
        const int boxW = (top.getWidth() - 3 * gap) / 4;
        momentaryBox .setBounds(top.removeFromLeft(boxW));
        top.removeFromLeft(gap);
        shortTermBox .setBounds(top.removeFromLeft(boxW));
        top.removeFromLeft(gap);
        integratedBox.setBounds(top.removeFromLeft(boxW));
        top.removeFromLeft(gap);
        crestBox     .setBounds(top);

        lane3Content.removeFromTop(6);
        // Gain-Match moved to the BYPASS dropdown (brand bar); RESET
        // takes the full row again.
        resetLufsButton.setBounds(lane3Content.removeFromTop(22));
    }
}
