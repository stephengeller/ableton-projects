#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/LevelMeter.h"
#include "Theme.h"

// One row of Variant F's compact horizontal meters: small channel label
// (L/R) on the left, a thin track filled with RMS + peak overlay + peak
// hold tick + 0 dBFS marker, numeric peak readout on the right that flips
// to overload-red when peak > 0 dBFS.
//
// Display range: -60 to +6 dBFS (matches the design source).
class HorizontalMeter : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    HorizontalMeter(LevelMeter& source, juce::String label, int channel);
    ~HorizontalMeter() override;

    void paint(juce::Graphics&) override;
    void timerCallback() override;

private:
    LevelMeter&  meter;
    juce::String label;
    int          channel;

    float displayPeakDb = -100.0f;
    float displayRmsDb  = -100.0f;
    float displayHoldDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HorizontalMeter)
};
