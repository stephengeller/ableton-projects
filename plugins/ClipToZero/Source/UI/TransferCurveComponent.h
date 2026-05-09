#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// A mini transfer-function visualisation for the clipper. Lives next to
// the Drive knob in Stage 2. Updates whenever clip type or drive changes;
// it's purely cosmetic (a static shape sketch, not a sample-accurate
// render) — its job is to communicate "hard knee vs soft knee" and
// "harder drive => narrower linear band" at a glance.
class TransferCurveComponent : public juce::Component {
public:
    enum class ClipType { Hard = 0, Soft = 1 };

    void setClipType(ClipType t)  { clipType = t; repaint(); }
    void setDriveDb(float db)     { driveDb = db; repaint(); }

    void paint(juce::Graphics&) override;

private:
    ClipType clipType = ClipType::Hard;
    float    driveDb  = 0.0f;
};
