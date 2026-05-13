#include "StageLane.h"

StageLane::StageLane(int n, juce::String t, juce::String h)
    : number(n), title(std::move(t)), hint(std::move(h)) {
    // (true, true) = lane catches clicks AND children still receive them.
    // We need clicks for the indicator-dot reset interaction.
    setInterceptsMouseClicks(true, true);
}

void StageLane::setState(State s) {
    if (state == s) return;
    state = s;
    repaint();
}

void StageLane::setStatus(const juce::String& text) {
    if (statusText == text) return;
    statusText = text;
    repaint();
}

void StageLane::setShowHint(bool show) {
    if (showHint == show) return;
    showHint = show;
    resized();  // contentBounds depends on hint visibility
    repaint();
}

void StageLane::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const bool active = (state == State::Active);
    const bool done   = (state == State::Done);

    // Background: very subtle lime tint when active, near-transparent
    // otherwise. The active state is what tells the user "this is where
    // your attention belongs right now".
    g.setColour(active ? Theme::accent.withAlpha(0.06f)
                       : juce::Colour::fromFloatRGBA(0.901f, 0.941f, 0.760f, 0.02f));
    g.fillRoundedRectangle(bounds, 4.0f);

    // Border.
    g.setColour(active ? Theme::accent : Theme::border);
    g.drawRoundedRectangle(bounds, 4.0f, active ? 1.0f : 1.0f);
    if (active) {
        // Subtle outer halo so the active card "lifts" off the canvas.
        g.setColour(Theme::accent.withAlpha(0.15f));
        g.drawRoundedRectangle(bounds.expanded(1.0f), 5.0f, 1.0f);
    }

    auto inner = getLocalBounds().reduced(12, 10);

    // ---- Header: indicator dot + uppercase title -----------------------
    {
        auto headerRow = inner.removeFromTop(16);
        const int dotSize = 16;
        auto dotArea = headerRow.removeFromLeft(dotSize).withHeight(dotSize);

        const auto dotF = dotArea.toFloat();
        if (done) {
            // Filled lime circle with a hand-drawn checkmark. When hovered,
            // we replace the checkmark with an "x" to signal "click to reset".
            g.setColour(Theme::accent);
            g.fillEllipse(dotF);

            const float cx = dotF.getCentreX();
            const float cy = dotF.getCentreY();
            g.setColour(Theme::bg);
            if (dotHovered) {
                // "x" — two crossed strokes
                juce::Path xMark;
                xMark.startNewSubPath(cx - 3.0f, cy - 3.0f); xMark.lineTo(cx + 3.0f, cy + 3.0f);
                xMark.startNewSubPath(cx + 3.0f, cy - 3.0f); xMark.lineTo(cx - 3.0f, cy + 3.0f);
                g.strokePath(xMark, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
                // Subtle outer ring to reinforce the "interactive" affordance.
                g.setColour(Theme::accent.withAlpha(0.45f));
                g.drawEllipse(dotF.expanded(2.0f), 1.0f);
            } else {
                juce::Path tick;
                tick.startNewSubPath(cx - 3.0f, cy);
                tick.lineTo(cx - 1.0f, cy + 2.5f);
                tick.lineTo(cx + 3.5f, cy - 2.5f);
                g.strokePath(tick, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
            }
        } else {
            g.setColour(active ? Theme::accent : Theme::borderVeryDim);
            g.drawEllipse(dotF.reduced(0.5f), 1.0f);
            g.setColour(active ? Theme::accent : Theme::textDim);
            g.setFont(Theme::mono(9.0f, juce::Font::bold));
            g.drawText(juce::String(number), dotArea, juce::Justification::centred);
        }

        headerRow.removeFromLeft(8); // gap between dot and title
        g.setColour(active ? Theme::accent : Theme::textBody);
        g.setFont(Theme::mono(9.5f, juce::Font::bold));
        g.drawText(title.toUpperCase(), headerRow, juce::Justification::centredLeft);
    }

    inner.removeFromTop(6); // breathing room

    // ---- Hint text -----------------------------------------------------
    if (hint.isNotEmpty() && showHint) {
        auto hintRow = inner.removeFromTop(28);
        g.setColour(Theme::textDim);
        g.setFont(Theme::sans(10.5f));
        g.drawFittedText(hint, hintRow, juce::Justification::topLeft, 2);
        inner.removeFromTop(2);
    }

    // ---- Status text at the bottom -------------------------------------
    {
        auto statusRow = inner.removeFromBottom(22);
        g.setColour(Theme::textDim);
        g.setFont(Theme::mono(9.0f));
        g.drawFittedText(statusText, statusRow, juce::Justification::topLeft, 2);
    }

    // Stash the remaining inner rect — that's where the editor will lay
    // out the controls for this lane. (Read in resized() too — paint() is
    // called more often, so we redo the maths here for safety.)
}

void StageLane::resized() {
    // Mirror the layout maths in paint() so the editor can request the
    // content rectangle without having to know about our header/hint/
    // status sizes.
    auto inner = getLocalBounds().reduced(12, 10);
    auto headerRow = inner.removeFromTop(16);
    dotBounds = headerRow.removeFromLeft(16).withHeight(16);  // matches paint()
    inner.removeFromTop(6);      // gap
    if (hint.isNotEmpty() && showHint) {
        inner.removeFromTop(28); // hint
        inner.removeFromTop(2);
    }
    inner.removeFromBottom(22);  // status
    contentBounds = inner;
}

void StageLane::mouseDown(const juce::MouseEvent& e) {
    if (state == State::Done && dotBounds.contains(e.getPosition()) && onResetClicked) {
        onResetClicked();
    }
}

void StageLane::mouseMove(const juce::MouseEvent& e) {
    const bool overDot = (state == State::Done) && dotBounds.contains(e.getPosition());
    if (overDot != dotHovered) {
        dotHovered = overDot;
        repaint(dotBounds.expanded(3));
    }
    setMouseCursor(overDot ? juce::MouseCursor::PointingHandCursor
                           : juce::MouseCursor::NormalCursor);
}

void StageLane::mouseExit(const juce::MouseEvent&) {
    if (dotHovered) {
        dotHovered = false;
        repaint(dotBounds.expanded(3));
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}
