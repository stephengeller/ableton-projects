#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// Composite control for a Variant F knob: a rotary Slider with a value
// readout below and an uppercase name label below that. The rotary
// rendering itself comes from LookAndFeel_F::drawRotarySlider — this
// component just composes it with its labels.
class Knob : public juce::Component {
public:
    Knob(juce::String name, juce::String unit, int decimals = 1, bool big = false, bool showSign = false);
    ~Knob() override = default;

    juce::Slider& slider() { return s; }

    void resized() override;

private:
    juce::Slider s;
    juce::Label  valueLabel;
    juce::Label  nameLabel;
    juce::String unit;
    int          decimals;
    bool         big;
    bool         showSign;

    void refreshValueLabel();
};
