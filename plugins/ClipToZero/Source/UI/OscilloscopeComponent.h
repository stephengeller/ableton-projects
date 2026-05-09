#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "../PluginProcessor.h"

class OscilloscopeComponent : public juce::Component, private juce::Timer {
public:
    explicit OscilloscopeComponent(ClipToZeroProcessor& p);
    ~OscilloscopeComponent() override;

    void paint(juce::Graphics&) override;
    void timerCallback() override;

private:
    ClipToZeroProcessor& processor;

    // Backing storage sized to the maximum scope window we ever expect.
    // 500 ms at 192 kHz is 96 000 samples — `maxScopeSamples` gives headroom.
    static constexpr int maxScopeSamples = 131072;
    std::vector<float> displayPre;
    std::vector<float> displayPost;
    int                activeSamples = 0;

    void drawZoomedIn (juce::Graphics&, juce::Rectangle<float>) const;
    void drawZoomedOut(juce::Graphics&, juce::Rectangle<float>) const;
};
