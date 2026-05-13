#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

// Pro-L2-style vertical GR meter. Shows current gain reduction as a red
// bar growing down from 0 dB at the top, plus a held-peak hairline that
// follows the same hold + decay envelope the horizontal GR strip
// (GRMeterComponent) uses for its numeric readout.
//
// Added in v0.5.9 as an A/B alternative to the existing horizontal GR
// strip. Both run side by side temporarily; once we know which one is
// the keeper, the loser gets removed and the keeper takes the freed
// real estate.
//
// Same data source as the horizontal strip: processor.grHistory. The
// hold/decay state is duplicated locally rather than shared because (1)
// it's tiny, (2) keeping the meters independent means each can have its
// own timer phase / repaint cadence, (3) refactoring to share would
// require widening GRHistory's API.
class GRMeterVertical : public juce::Component, private juce::Timer {
public:
    explicit GRMeterVertical(ClipToZeroProcessor& p);
    ~GRMeterVertical() override;

    void paint(juce::Graphics&) override;
    void timerCallback() override;

private:
    ClipToZeroProcessor& processor;

    // The red bar's height tracks this -- a lightly smoothed instantaneous
    // GR reading. Smoothing is just enough to remove single-frame flicker
    // without lagging the actual GR motion.
    float currentGrDb = 0.0f;

    // Held-peak with hold + decay envelope, matching the horizontal
    // GRMeterComponent. Drawn as a white hairline at the peak position
    // AND printed as the numeric readout.
    float displayedPeakDb   = 0.0f;
    double secondsSincePeak = 0.0;
    double lastTimerMs      = 0.0;

    // Same vertical range conventions as the horizontal strip.
    static constexpr float displayFloorDb = -36.0f;
    static constexpr float holdSeconds    = 1.5f;
    static constexpr float decayDbPerSec  = 8.0f;
};
