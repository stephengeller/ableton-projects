#include "TransferCurveComponent.h"
#include <cmath>

void TransferCurveComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);

    // Recessed well.
    g.setColour(Theme::bgDeeper);
    g.fillRoundedRectangle(bounds, 2.0f);
    g.setColour(Theme::border);
    g.drawRoundedRectangle(bounds, 2.0f, 1.0f);

    auto inner = bounds.reduced(2.0f);

    // ---- Centre crosshair (where input = output = 0) ------------------
    g.setColour(Theme::border);
    g.drawHorizontalLine(juce::roundToInt(inner.getCentreY()), inner.getX(), inner.getRight());
    g.drawVerticalLine  (juce::roundToInt(inner.getCentreX()), inner.getY(), inner.getBottom());

    // ---- 0 dBFS rails (dashed red, top and bottom of inner area) ------
    {
        const float dashLen = 2.5f;
        const float dashGap = 2.5f;
        g.setColour(Theme::overload.withAlpha(0.40f));
        const float yTop = inner.getY() + inner.getHeight() * 0.10f;
        const float yBot = inner.getY() + inner.getHeight() * 0.90f;
        for (float x = inner.getX(); x < inner.getRight(); x += dashLen + dashGap) {
            const float w = juce::jmin(dashLen, inner.getRight() - x);
            g.fillRect(x, yTop, w, 1.0f);
            g.fillRect(x, yBot, w, 1.0f);
        }
    }

    // ---- Transfer curve in lime ---------------------------------------
    g.setColour(Theme::accent);
    juce::Path curve;

    const float left   = inner.getX();
    const float right  = inner.getRight();
    const float midX   = inner.getCentreX();
    const float midY   = inner.getCentreY();
    const float yTop   = inner.getY() + inner.getHeight() * 0.10f;
    const float yBot   = inner.getY() + inner.getHeight() * 0.90f;

    // The drive factor controls how wide the linear region is. At 0 dB drive,
    // the linear region spans the whole inner width; at +24 dB it narrows to
    // about 10 % around the centre, with the rest being "clipped" plateau.
    const float driveT = juce::jlimit(0.0f, 1.0f, driveDb / 24.0f);

    if (clipType == ClipType::Hard) {
        // Piecewise linear: flat at the bottom plateau, rising line through
        // centre, flat at the top plateau. Knee positions move toward midX
        // as drive increases.
        const float halfLinear = inner.getWidth() * 0.5f * (1.0f - driveT * 0.85f);
        const float kneeLeft  = midX - halfLinear * 0.5f;
        const float kneeRight = midX + halfLinear * 0.5f;
        curve.startNewSubPath(left, yBot);
        curve.lineTo(kneeLeft, yBot);
        curve.lineTo(midX, midY);
        curve.lineTo(kneeRight, yTop);
        curve.lineTo(right, yTop);
    } else {
        // Smooth tanh-style. Bezier handles control how aggressive the bend
        // is — they pull toward (kneeLeft, yBot) and (kneeRight, yTop).
        // As drive rises the handles move inward, sharpening the bend.
        const float handleSpread = inner.getWidth() * 0.30f * (1.0f - driveT * 0.7f);
        curve.startNewSubPath(left, yBot);
        curve.quadraticTo(midX - handleSpread, yBot, midX, midY);
        curve.quadraticTo(midX + handleSpread, yTop, right, yTop);
    }
    g.strokePath(curve, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::butt));

    // ---- Label ---------------------------------------------------------
    g.setColour(Theme::textVeryDim);
    g.setFont(Theme::mono(8.0f));
    g.drawText((clipType == ClipType::Hard ? "HARD" : "SOFT") + juce::String(" - CURVE"),
               getLocalBounds().reduced(4, 2),
               juce::Justification::bottomLeft);
}
