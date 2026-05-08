#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/LevelMeter.h"

class MeterComponent : public juce::Component, private juce::Timer {
public:
    MeterComponent(LevelMeter& source, juce::String label);
    ~MeterComponent() override;

    void paint(juce::Graphics&) override;
    void timerCallback() override;

private:
    LevelMeter&    meter;
    juce::String   label;

    float displayPeakDb[2] { -100.0f, -100.0f };
    float displayRmsDb [2] { -100.0f, -100.0f };
    float displayHoldDb[2] { -100.0f, -100.0f };

    static juce::Colour colourForDb(float db);
};
