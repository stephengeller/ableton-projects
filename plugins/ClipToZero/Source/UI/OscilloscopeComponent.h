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

    // Backing storage sized to match the processor-side ring buffer so the
    // editor can display the full 5 s max window at 192 kHz (= 960 000 samples).
    static constexpr int maxScopeSamples = 1048576;
    std::vector<float> displayPre;
    std::vector<float> displayPost;
    int                activeSamples = 0;

    // ---- Render helpers ------------------------------------------------
    void drawBackground (juce::Graphics&, juce::Rectangle<float>, float midY, float ampScale) const;
    void drawZoomedIn   (juce::Graphics&, juce::Rectangle<float>, float midY, float ampScale) const;
    void drawZoomedOut  (juce::Graphics&, juce::Rectangle<float>, float midY, float ampScale) const;
    void drawOverlays   (juce::Graphics&, float headroomDb) const;
};
