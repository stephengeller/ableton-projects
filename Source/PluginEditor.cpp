#include "PluginEditor.h"

ClipToZeroEditor::ClipToZeroEditor(ClipToZeroProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      inputMeterComp (p.inputMeter,  "IN"),
      outputMeterComp(p.outputMeter, "OUT"),
      scope(p)
{
    setSize(720, 480);

    auto styleSlider = [](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 22);
        s.setTextValueSuffix(" dB");
    };
    styleSlider(inputGainSlider);
    styleSlider(outputTrimSlider);

    inputGainLabel.setText("Input Gain",  juce::dontSendNotification);
    outputTrimLabel.setText("Output Trim", juce::dontSendNotification);
    clipTypeLabel.setText("Clip Type",     juce::dontSendNotification);
    inputGainLabel.setColour(juce::Label::textColourId,  juce::Colours::lightgrey);
    outputTrimLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    clipTypeLabel.setColour(juce::Label::textColourId,   juce::Colours::lightgrey);

    clipTypeBox.addItemList({ "Hard", "Soft" }, 1);

    autoGainStatus.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    autoGainStatus.setJustificationType(juce::Justification::centredLeft);

    autoGainButton.onClick = [this] {
        processor.autoGain.startMeasurement(2.0);
        autoGainStatus.setText("Measuring 2.0 s ...", juce::dontSendNotification);
        wasMeasuring = true;
    };

    addAndMakeVisible(inputGainSlider);
    addAndMakeVisible(outputTrimSlider);
    addAndMakeVisible(inputGainLabel);
    addAndMakeVisible(outputTrimLabel);
    addAndMakeVisible(clipTypeLabel);
    addAndMakeVisible(clipTypeBox);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(autoGainButton);
    addAndMakeVisible(autoGainStatus);
    addAndMakeVisible(inputMeterComp);
    addAndMakeVisible(outputMeterComp);
    addAndMakeVisible(scope);

    inputGainAttach  = std::make_unique<SliderAttach>(p.apvts, Param::inputGain,  inputGainSlider);
    outputTrimAttach = std::make_unique<SliderAttach>(p.apvts, Param::outputTrim, outputTrimSlider);
    clipTypeAttach   = std::make_unique<ComboAttach> (p.apvts, Param::clipType,   clipTypeBox);
    bypassAttach     = std::make_unique<ButtonAttach>(p.apvts, Param::bypass,     bypassButton);

    startTimerHz(15);
}

ClipToZeroEditor::~ClipToZeroEditor() = default;

void ClipToZeroEditor::timerCallback() {
    // Detect the falling edge: we were measuring, now we're not. Apply the result.
    const bool nowMeasuring = processor.autoGain.isMeasuring();
    if (wasMeasuring && !nowMeasuring) {
        const float suggested = juce::jlimit(-24.0f, 24.0f, processor.autoGain.getSuggestedGainDb());

        // setValueNotifyingHost expects a 0..1 normalised value.
        if (auto* p = processor.apvts.getParameter(Param::inputGain)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(suggested));
            p->endChangeGesture();
        }

        autoGainStatus.setText("Set to " + juce::String(suggested, 2) + " dB",
                               juce::dontSendNotification);
    }
    wasMeasuring = nowMeasuring;
}

void ClipToZeroEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff141414));

    // Title bar.
    g.setColour(juce::Colour(0xfff0f0f0));
    g.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
    g.drawText("Clip To Zero", 14, 10, 240, 26, juce::Justification::topLeft);

    g.setColour(juce::Colour(0xff808080));
    g.setFont(11.0f);
    g.drawText("peak/RMS  •  auto-gain  •  clipper  •  scope",
               14, 32, 360, 14, juce::Justification::topLeft);
}

void ClipToZeroEditor::resized() {
    auto r = getLocalBounds().reduced(12);
    r.removeFromTop(40); // title

    // Top row: bypass on the right.
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

    // Input gain row: label | slider | status | auto button.
    auto gainRow = r.removeFromTop(28);
    inputGainLabel.setBounds(gainRow.removeFromLeft(80));
    autoGainButton.setBounds(gainRow.removeFromRight(100));
    gainRow.removeFromRight(8);
    autoGainStatus.setBounds(gainRow.removeFromRight(160));
    gainRow.removeFromRight(8);
    inputGainSlider.setBounds(gainRow);

    r.removeFromTop(6);

    // Clip type row.
    auto clipRow = r.removeFromTop(28);
    clipTypeLabel.setBounds(clipRow.removeFromLeft(80));
    clipTypeBox.setBounds(clipRow.removeFromLeft(140));

    r.removeFromTop(6);

    // Output trim row.
    auto outRow = r.removeFromTop(28);
    outputTrimLabel.setBounds(outRow.removeFromLeft(80));
    outputTrimSlider.setBounds(outRow);
}
