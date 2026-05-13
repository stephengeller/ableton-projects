#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

// Horizontal time-series strip showing gain reduction (clipping) over the
// same time window as the scope above it. Each pixel column represents a
// slice of time; height is the GR magnitude (top = 0 dB, bottom = -24 dB
// or whatever max scale is set). Red fill grows downward when the clipper
// is actively cutting.
class GRMeterComponent : public juce::Component, private juce::Timer {
public:
    explicit GRMeterComponent(ClipToZeroProcessor& p);
    ~GRMeterComponent() override;

    void paint(juce::Graphics&) override;
    void timerCallback() override;

private:
    ClipToZeroProcessor& processor;

    // Reused scratch buffer for reading from GRHistory. Sized to the GR
    // buffer max so we never reallocate; resized down per frame to the
    // current scope window.
    std::vector<float> latest;
    int   activeBins   = 0;

    // Numeric-readout peak hold/decay. Bars use raw history; only the
    // corner number uses this smoothed value so transient hits stay
    // readable.
    float  displayedPeakDb  = 0.0f;
    double secondsSincePeak = 0.0;
    double lastTimerMs      = 0.0;

    static constexpr float displayFloorDb = -24.0f;  // bottom of strip
    static constexpr float holdSeconds    = 1.5f;    // how long to hold a transient peak
    static constexpr float decayDbPerSec  = 8.0f;    // linear decay rate after hold expires
};
