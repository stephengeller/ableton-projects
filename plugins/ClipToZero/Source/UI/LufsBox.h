#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// One of the three small loudness readouts in Stage 3 of Variant F.
// Layout (top to bottom):
//   small letter (M / S / I)
//   big numeric value (or "-inf")
//   tiny subtitle ("momentary" / "3-sec" / "gated")
class LufsBox : public juce::Component, public juce::SettableTooltipClient {
public:
    LufsBox(juce::String letter, juce::String subtitle);

    void setValue(float lufs);

    void paint(juce::Graphics&) override;

private:
    juce::String letter;
    juce::String subtitle;
    float        value = -100.0f;
};
