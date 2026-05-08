#include "PluginEditor.h"

namespace {
    juce::String formatDb(float db, int decimals = 1, const juce::String& suffix = " dB") {
        return juce::String(db, decimals) + suffix;
    }

    juce::String formatLUFS(float l) {
        if (l <= -99.0f) return "  -inf";
        return juce::String(l, 1);
    }
}

ClipToZeroEditor::ClipToZeroEditor(ClipToZeroProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      inputMeterComp (p.inputMeter,  "IN"),
      outputMeterComp(p.outputMeter, "OUT"),
      scope(p)
{
    setSize(720, 540);

    // ---- styling ----
    auto styleSlider = [](juce::Slider& s, const juce::String& suffix = " dB") {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 22);
        s.setTextValueSuffix(suffix);
    };
    styleSlider(targetPeakSlider, " dBFS");
    styleSlider(inputGainSlider);
    styleSlider(driveSlider);
    styleSlider(outputTrimSlider);

    auto styleSubtleLabel = [](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    };
    styleSubtleLabel(targetPeakLabel, "Target");
    styleSubtleLabel(inputGainLabel,  "Input Gain");
    styleSubtleLabel(driveLabel,      "Drive");
    styleSubtleLabel(clipTypeLabel,   "Clip Type");
    styleSubtleLabel(outputTrimLabel, "Output Trim");

    clipTypeBox.addItemList({ "Hard", "Soft" }, 1);

    autoGainStatus.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    autoGainStatus.setJustificationType(juce::Justification::centredLeft);

    autoGainButton.onClick = [this] {
        processor.autoGain.startMeasurement(2.0);
        autoGainStatus.setText("Measuring 2.0 s ...", juce::dontSendNotification);
        wasMeasuring = true;
    };

    // ---- LUFS panel ----
    lufsHeading.setText("LUFS", juce::dontSendNotification);
    lufsHeading.setColour(juce::Label::textColourId, juce::Colours::white);
    lufsHeading.setJustificationType(juce::Justification::centredLeft);

    auto setupLufsValue = [](juce::Label& l) {
        l.setColour(juce::Label::textColourId, juce::Colours::white);
        l.setJustificationType(juce::Justification::centredLeft);
        l.setFont(juce::Font(juce::FontOptions(13.0f)));
    };
    setupLufsValue(momentaryLabel);
    setupLufsValue(shortTermLabel);
    setupLufsValue(integratedLabel);

    resetLufsButton.onClick = [this] {
        processor.lufs.requestResetIntegrated();
    };

    // ---- add ----
    addAndMakeVisible(targetPeakLabel);
    addAndMakeVisible(targetPeakSlider);
    addAndMakeVisible(inputGainLabel);
    addAndMakeVisible(inputGainSlider);
    addAndMakeVisible(driveLabel);
    addAndMakeVisible(driveSlider);
    addAndMakeVisible(clipTypeLabel);
    addAndMakeVisible(clipTypeBox);
    addAndMakeVisible(outputTrimLabel);
    addAndMakeVisible(outputTrimSlider);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(autoGainButton);
    addAndMakeVisible(autoGainStatus);
    addAndMakeVisible(inputMeterComp);
    addAndMakeVisible(outputMeterComp);
    addAndMakeVisible(scope);
    addAndMakeVisible(lufsHeading);
    addAndMakeVisible(momentaryLabel);
    addAndMakeVisible(shortTermLabel);
    addAndMakeVisible(integratedLabel);
    addAndMakeVisible(resetLufsButton);

    // ---- APVTS attachments (after addAndMakeVisible so styling is applied) ----
    targetPeakAttach = std::make_unique<SliderAttach>(p.apvts, Param::targetPeak, targetPeakSlider);
    inputGainAttach  = std::make_unique<SliderAttach>(p.apvts, Param::inputGain,  inputGainSlider);
    driveAttach      = std::make_unique<SliderAttach>(p.apvts, Param::drive,      driveSlider);
    outputTrimAttach = std::make_unique<SliderAttach>(p.apvts, Param::outputTrim, outputTrimSlider);
    clipTypeAttach   = std::make_unique<ComboAttach> (p.apvts, Param::clipType,   clipTypeBox);
    bypassAttach     = std::make_unique<ButtonAttach>(p.apvts, Param::bypass,     bypassButton);

    startTimerHz(15);
}

ClipToZeroEditor::~ClipToZeroEditor() = default;

void ClipToZeroEditor::timerCallback() {
    // ---- Auto-Gain: detect falling edge and apply target-aware gain ----
    const bool nowMeasuring = processor.autoGain.isMeasuring();
    if (wasMeasuring && !nowMeasuring) {
        const float target    = processor.apvts.getRawParameterValue(Param::targetPeak)->load();
        const float peakDb    = processor.autoGain.getMeasuredPeakDb();
        const float suggested = juce::jlimit(-24.0f, 24.0f, processor.autoGain.getSuggestedGainDb(target));

        if (auto* gp = processor.apvts.getParameter(Param::inputGain)) {
            gp->beginChangeGesture();
            gp->setValueNotifyingHost(gp->convertTo0to1(suggested));
            gp->endChangeGesture();
        }

        autoGainStatus.setText("peak " + formatDb(peakDb, 1) + " → gain " + formatDb(suggested, 2),
                               juce::dontSendNotification);
    }
    wasMeasuring = nowMeasuring;

    // ---- LUFS readouts ----
    momentaryLabel .setText("M  " + formatLUFS(processor.lufs.getMomentaryLUFS()),
                            juce::dontSendNotification);
    shortTermLabel .setText("S  " + formatLUFS(processor.lufs.getShortTermLUFS()),
                            juce::dontSendNotification);
    integratedLabel.setText("I  " + formatLUFS(processor.lufs.getIntegratedLUFS()),
                            juce::dontSendNotification);
}

void ClipToZeroEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff141414));

    g.setColour(juce::Colour(0xfff0f0f0));
    g.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
    g.drawText("Clip To Zero", 14, 10, 240, 26, juce::Justification::topLeft);

    g.setColour(juce::Colour(0xff808080));
    g.setFont(11.0f);
    g.drawText("peak/RMS  •  auto-gain  •  drive  •  clipper  •  scope  •  LUFS",
               14, 32, 480, 14, juce::Justification::topLeft);

    // Subtle dividers between sections — pure cosmetic, helps the eye group
    // controls into "metering / staging / clipping / output / loudness".
    auto drawRule = [&](int y) {
        g.setColour(juce::Colour(0xff282828));
        g.fillRect(12, y, getWidth() - 24, 1);
    };
    // The exact y values come from resized() — keep them in sync.
    drawRule(294);   // below the meters/scope row
    drawRule(360);   // below the staging block (target + input gain)
    drawRule(398);   // below the drive row
    drawRule(436);   // below the clip type / output trim row
}

void ClipToZeroEditor::resized() {
    auto r = getLocalBounds().reduced(12);
    r.removeFromTop(40); // title block

    // Bypass top right.
    auto top = r.removeFromTop(28);
    bypassButton.setBounds(top.removeFromRight(90));

    r.removeFromTop(6);

    // Meters / scope row.
    auto metersRow = r.removeFromTop(220);
    inputMeterComp .setBounds(metersRow.removeFromLeft(72));
    metersRow.removeFromLeft(8);
    outputMeterComp.setBounds(metersRow.removeFromRight(72));
    metersRow.removeFromRight(8);
    scope.setBounds(metersRow);

    r.removeFromTop(10);

    // ----- staging block -----
    // Row 1: Target peak | Auto-Gain button | status
    auto targetRow = r.removeFromTop(28);
    targetPeakLabel.setBounds(targetRow.removeFromLeft(80));
    auto targetSliderArea = targetRow.removeFromLeft(180);
    targetPeakSlider.setBounds(targetSliderArea);
    targetRow.removeFromLeft(12);
    autoGainButton.setBounds(targetRow.removeFromLeft(100));
    targetRow.removeFromLeft(8);
    autoGainStatus.setBounds(targetRow);

    r.removeFromTop(4);

    // Row 2: Input Gain
    auto gainRow = r.removeFromTop(28);
    inputGainLabel.setBounds(gainRow.removeFromLeft(80));
    inputGainSlider.setBounds(gainRow);

    r.removeFromTop(8);

    // ----- drive -----
    auto driveRow = r.removeFromTop(28);
    driveLabel.setBounds(driveRow.removeFromLeft(80));
    driveSlider.setBounds(driveRow);

    r.removeFromTop(8);

    // ----- clip type + output trim on one row -----
    auto clipTrimRow = r.removeFromTop(28);
    clipTypeLabel.setBounds(clipTrimRow.removeFromLeft(80));
    clipTypeBox.setBounds(clipTrimRow.removeFromLeft(120));
    clipTrimRow.removeFromLeft(20);
    outputTrimLabel.setBounds(clipTrimRow.removeFromLeft(80));
    outputTrimSlider.setBounds(clipTrimRow);

    r.removeFromTop(8);

    // ----- LUFS panel -----
    auto lufsRow = r.removeFromTop(28);
    lufsHeading.setBounds(lufsRow.removeFromLeft(50));
    momentaryLabel .setBounds(lufsRow.removeFromLeft(110));
    shortTermLabel .setBounds(lufsRow.removeFromLeft(110));
    integratedLabel.setBounds(lufsRow.removeFromLeft(110));
    resetLufsButton.setBounds(lufsRow.removeFromRight(80));
}
