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
      inputMeterL (p.inputMeter,  "L", 0),
      inputMeterR (p.inputMeter,  "R", 1),
      outputMeterL(p.outputMeter, "L", 0),
      outputMeterR(p.outputMeter, "R", 1)
{
    setLookAndFeel(&laf);
    setSize(720, 580);

    // ---- Brand bar buttons ---------------------------------------------
    clipTypeButton.setClickingTogglesState(false);
    clipTypeButton.onClick = [this] {
        if (auto* param = processor.apvts.getParameter(Param::clipType)) {
            const auto current = static_cast<int>(param->getValue() + 0.5f);
            param->beginChangeGesture();
            param->setValueNotifyingHost(current == 0 ? 1.0f : 0.0f);
            param->endChangeGesture();
        }
    };
    addAndMakeVisible(clipTypeButton);

    bypassButton.setClickingTogglesState(true);
    bypassButton.getProperties().set("variant", "warning");
    addAndMakeVisible(bypassButton);
    bypassAttach = std::make_unique<ButtonAttach>(p.apvts, Param::bypass, bypassButton);

    // ---- Scope ---------------------------------------------------------
    addAndMakeVisible(scope);

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
    lane2.addAndMakeVisible(drive);
    lane2.addAndMakeVisible(trim);
    lane2.addAndMakeVisible(transferCurve);
    driveAttach      = std::make_unique<SliderAttach>(p.apvts, Param::drive,      drive.slider());
    outputTrimAttach = std::make_unique<SliderAttach>(p.apvts, Param::outputTrim, trim.slider());

    drive.slider().onValueChange = [this] {
        transferCurve.setDriveDb(static_cast<float>(drive.slider().getValue()));
    };
    drive.slider().onValueChange();

    // ---- Stage 3 -------------------------------------------------------
    addAndMakeVisible(lane3);
    lane3.addAndMakeVisible(momentaryBox);
    lane3.addAndMakeVisible(shortTermBox);
    lane3.addAndMakeVisible(integratedBox);
    resetLufsButton.onClick = [this] { processor.lufs.requestResetIntegrated(); };
    lane3.addAndMakeVisible(resetLufsButton);

    updateClipTypeButtonText();
    updateAutoGainButton();
    updateStageStates();
    startTimerHz(15);
}

ClipToZeroEditor::~ClipToZeroEditor() {
    setLookAndFeel(nullptr);
}

void ClipToZeroEditor::updateClipTypeButtonText() {
    const auto* p = processor.apvts.getParameter(Param::clipType);
    if (!p) return;
    const auto idx = static_cast<int>(p->getValue() + 0.5f);
    clipTypeButton.setButtonText(idx == 0 ? "CLIP-HARD" : "CLIP-SOFT");
    transferCurve.setClipType(idx == 0 ? TransferCurveComponent::ClipType::Hard
                                       : TransferCurveComponent::ClipType::Soft);
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
}

void ClipToZeroEditor::paint(juce::Graphics& g) {
    g.fillAll(Theme::bg);

    // Brand bar separator (thin line across the bottom of the brand row).
    auto br = getLocalBounds();
    br.removeFromTop(48);
    g.setColour(Theme::border);
    g.fillRect(0, 47, getWidth(), 1);

    // Logo + workflow caption.
    g.setColour(Theme::textBright);
    g.setFont(Theme::mono(13.0f, juce::Font::bold));
    auto logoArea = juce::Rectangle<int>(18, 16, 200, 16);

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

    g.setColour(Theme::textVeryDim);
    g.setFont(Theme::mono(8.5f, juce::Font::bold));
    g.drawText("STAGE -> DRIVE -> JUDGE",
               juce::Rectangle<int>(x + 14, 16, 220, 16),
               juce::Justification::centredLeft);

    // Sample-rate readout (right side of brand bar).
    g.setColour(Theme::textDim);
    g.setFont(Theme::mono(9.5f));
    const int sr = static_cast<int>(processor.getSampleRate());
    const auto srText = juce::String(sr / 1000) + "k";
    g.drawText("SR " + srText,
               juce::Rectangle<int>(getWidth() - 270, 16, 60, 16),
               juce::Justification::centredLeft);
}

void ClipToZeroEditor::resized() {
    auto r = getLocalBounds();

    // ---- Brand bar (fixed 48px) ---------------------------------------
    auto brand = r.removeFromTop(48);
    brand.removeFromTop(12);
    brand.removeFromBottom(8);
    auto brandRight = brand.removeFromRight(200);
    brandRight.removeFromRight(18);
    bypassButton.setBounds(brandRight.removeFromRight(80).withSizeKeepingCentre(80, 22));
    brandRight.removeFromRight(8);
    clipTypeButton.setBounds(brandRight.removeFromRight(82).withSizeKeepingCentre(82, 22));

    // ---- Scope (230px) -------------------------------------------------
    r.removeFromTop(6);
    auto scopeArea = r.removeFromTop(230).reduced(18, 0);
    scope.setBounds(scopeArea);

    // ---- Zoom controls row (28px) -------------------------------------
    r.removeFromTop(6);
    auto zoomRow = r.removeFromTop(28).reduced(18, 0);
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
        const int knobBlockW = 48;
        const int bigBlockW  = 58;
        auto col = lane1Content;
        target   .setBounds(col.removeFromLeft(knobBlockW).withSizeKeepingCentre(knobBlockW, 80));
        col.removeFromLeft(6);
        inputGain.setBounds(col.removeFromLeft(bigBlockW).withSizeKeepingCentre(bigBlockW, 80));
        col.removeFromLeft(8);
        autoGainButton.setBounds(col.withSizeKeepingCentre(col.getWidth(), 56)
                                    .withTrimmedTop(0));
    }

    // ---- Lane 2 contents -----------------------------------------------
    auto lane2Content = lane2.getContentBounds();
    {
        const int knobBlockW = 48;
        const int bigBlockW  = 58;
        auto col = lane2Content;
        drive.setBounds(col.removeFromLeft(bigBlockW).withSizeKeepingCentre(bigBlockW, 80));
        col.removeFromLeft(6);
        trim .setBounds(col.removeFromLeft(knobBlockW).withSizeKeepingCentre(knobBlockW, 80));
        col.removeFromLeft(6);
        transferCurve.setBounds(col.withSizeKeepingCentre(col.getWidth(), 56));
    }

    // ---- Lane 3 contents -----------------------------------------------
    auto lane3Content = lane3.getContentBounds();
    {
        auto top = lane3Content.removeFromTop(54);
        const int boxW = (top.getWidth() - 8) / 3;
        momentaryBox .setBounds(top.removeFromLeft(boxW));
        top.removeFromLeft(4);
        shortTermBox .setBounds(top.removeFromLeft(boxW));
        top.removeFromLeft(4);
        integratedBox.setBounds(top);

        lane3Content.removeFromTop(6);
        resetLufsButton.setBounds(lane3Content.removeFromTop(22));
    }
}
