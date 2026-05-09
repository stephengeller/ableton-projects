#include "StageLane.h"

StageLane::StageLane(int n, juce::String t, juce::String h)
    : number(n), title(std::move(t)), hint(std::move(h)) {
    setInterceptsMouseClicks(false, true);
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

        // Dot: filled when done, ringed when active, dim ring otherwise.
        const auto dotF = dotArea.toFloat();
        if (done) {
            g.setColour(Theme::accent);
            g.fillEllipse(dotF);
            g.setColour(Theme::bg);
            g.setFont(Theme::mono(9.0f, juce::Font::bold));
            g.drawText("X", dotArea, juce::Justification::centred);  // simple checkmark substitute
            // Draw a proper checkmark via path for crispness.
            juce::Path tick;
            const float cx = dotF.getCentreX();
            const float cy = dotF.getCentreY();
            tick.startNewSubPath(cx - 3.0f, cy);
            tick.lineTo(cx - 1.0f, cy + 2.5f);
            tick.lineTo(cx + 3.5f, cy - 2.5f);
            g.setColour(Theme::bg);
            g.fillRect(dotArea);  // wipe the X placeholder
            g.setColour(Theme::accent);
            g.fillEllipse(dotF);
            g.setColour(Theme::bg);
            g.strokePath(tick, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
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
    if (hint.isNotEmpty()) {
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
    inner.removeFromTop(16);     // header row
    inner.removeFromTop(6);      // gap
    if (hint.isNotEmpty()) {
        inner.removeFromTop(28); // hint
        inner.removeFromTop(2);
    }
    inner.removeFromBottom(22);  // status
    contentBounds = inner;
}
