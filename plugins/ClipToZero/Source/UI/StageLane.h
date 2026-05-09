#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// One of the three numbered "lanes" in Variant F · Stages — a card with
// an indicator dot (number or checkmark), an uppercase title, a plain-
// language hint, a content area (children added by the editor), and a
// live status line at the bottom.
//
// Visual states:
//   * idle    : dim border, gray title
//   * active  : lime border + subtle lime tint, lime title (the next step
//               the user should focus on)
//   * done    : indicator dot becomes a filled checkmark; border returns
//               to idle styling so attention shifts to the *next* step
class StageLane : public juce::Component {
public:
    enum class State { Idle, Active, Done };

    StageLane(int number, juce::String title, juce::String hint);
    ~StageLane() override = default;

    void setState(State s);
    void setStatus(const juce::String& text);

    // The rectangle the editor should use to lay out the controls in this
    // lane. Updates after each `resized()`.
    juce::Rectangle<int> getContentBounds() const noexcept { return contentBounds; }

    // Fires when the user clicks the indicator dot while the lane is in
    // the Done state. Editor uses this to reset the parameters that
    // contribute to "done"-ness (e.g. clear Auto-Gain result, return
    // Input Gain or Drive to 0) so the auto-progression starts over.
    std::function<void()> onResetClicked;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    int number = 1;
    juce::String title;
    juce::String hint;
    State state = State::Idle;
    juce::String statusText;

    juce::Rectangle<int> contentBounds;
    juce::Rectangle<int> dotBounds;
    bool dotHovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StageLane)
};
