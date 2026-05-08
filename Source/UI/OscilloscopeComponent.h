#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

class OscilloscopeComponent : public juce::Component, private juce::Timer {
public:
    explicit OscilloscopeComponent(ClipToZeroProcessor& p);
    ~OscilloscopeComponent() override;

    void paint(juce::Graphics&) override;
    void timerCallback() override;

private:
    ClipToZeroProcessor& processor;

    static constexpr int displaySamples = 2048;
    std::array<float, displaySamples> displayPre  {};
    std::array<float, displaySamples> displayPost {};
};
