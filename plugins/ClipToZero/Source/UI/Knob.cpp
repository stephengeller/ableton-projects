#include "Knob.h"
#include <cmath>

Knob::Knob(juce::String name, juce::String u, int dec, bool b, bool sign)
    : unit(std::move(u)), decimals(dec), big(b), showSign(sign)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    // 270° sweep: -3π/4 to +3π/4 around the top.
    constexpr float pi = juce::MathConstants<float>::pi;
    s.setRotaryParameters(-0.75f * pi, 0.75f * pi, true);
    s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    s.setVelocityModeParameters(1.0, 1, 0.0, false);
    addAndMakeVisible(s);

    // Refresh the value-label first, then forward to any user-provided
    // callback. The editor previously assigned to s.onValueChange directly,
    // which clobbered this setup and broke the Drive readout — go through
    // the Knob::onChange member instead.
    s.onValueChange = [this] {
        refreshValueLabel();
        if (onChange) onChange(static_cast<float>(s.getValue()));
    };

    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setColour(juce::Label::textColourId, Theme::textBright);
    valueLabel.setFont(Theme::mono(10.0f));
    addAndMakeVisible(valueLabel);

    nameLabel.setText(name.toUpperCase(), juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setColour(juce::Label::textColourId, Theme::textDim);
    nameLabel.setFont(Theme::mono(8.5f, juce::Font::bold));
    addAndMakeVisible(nameLabel);

    refreshValueLabel();
}

void Knob::refreshValueLabel() {
    auto v = static_cast<float>(s.getValue());
    juce::String prefix;
    if (showSign && v > 0.0f) prefix = "+";
    else if (showSign && std::abs(v) < 0.005f) prefix = "";  // avoid +0.0
    valueLabel.setText(prefix + juce::String(v, decimals) + unit, juce::dontSendNotification);
}

void Knob::resized() {
    auto r = getLocalBounds();
    const int knobSize = big ? 52 : 42;
    auto knobArea = r.removeFromTop(knobSize);
    s.setBounds(knobArea.withSizeKeepingCentre(knobSize, knobSize));

    r.removeFromTop(2);
    valueLabel.setBounds(r.removeFromTop(14));
    nameLabel .setBounds(r.removeFromTop(12));
}
