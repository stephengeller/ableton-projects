#include "GRMeterVertical.h"
#include "Theme.h"

GRMeterVertical::GRMeterVertical(ClipToZeroProcessor& p) : processor(p) {
    startTimerHz(60);  // matches the horizontal strip's update cadence
}

GRMeterVertical::~GRMeterVertical() = default;

void GRMeterVertical::timerCallback() {
    const float recentPeak = processor.grHistory.getRecentPeakGrDb();
    const double nowMs     = juce::Time::getMillisecondCounterHiRes();
    const double deltaSec  = (lastTimerMs > 0.0) ? (nowMs - lastTimerMs) * 0.001 : 0.0;
    lastTimerMs = nowMs;

    // --- Held-peak with hold + decay (matches GRMeterComponent) ---------
    if (recentPeak < displayedPeakDb) {
        displayedPeakDb  = recentPeak;
        secondsSincePeak = 0.0;
    } else {
        secondsSincePeak += deltaSec;
        if (secondsSincePeak > holdSeconds) {
            const float maxStep = decayDbPerSec * static_cast<float>(deltaSec);
            displayedPeakDb = juce::jmin(recentPeak, displayedPeakDb + maxStep);
        }
    }

    // --- Lightly-smoothed current GR for the bar fill height -----------
    // 50/50 EMA -- enough to remove single-frame flicker while still
    // tracking the actual GR motion responsively.
    currentGrDb = currentGrDb * 0.5f + recentPeak * 0.5f;

    repaint();
}

void GRMeterVertical::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(Theme::bgDeeper);

    // ---- Header (top): 'GR' label + numeric peak readout ---------------
    // Two stacked rows so the readout has enough vertical room to use a
    // larger font that's easy to read at a glance.
    auto headerArea = bounds.removeFromTop(30.0f);

    auto labelRow   = headerArea.removeFromTop(13.0f);
    g.setColour(Theme::scopeLabelDim);
    g.setFont(Theme::mono(9.0f, juce::Font::bold));
    g.drawText("GR", labelRow, juce::Justification::centred);

    auto readoutRow = headerArea;
    const auto readoutText = (displayedPeakDb >= -0.05f)
                                ? juce::String("0.0")
                                : juce::String(displayedPeakDb, 1);
    g.setColour(displayedPeakDb < -0.5f ? Theme::overload : Theme::scopeLabelMid);
    g.setFont(Theme::mono(11.0f, juce::Font::bold));
    g.drawText(readoutText, readoutRow, juce::Justification::centred);

    // ---- Bar area (rest of the bounds) --------------------------------
    auto barRow = bounds.reduced(2.0f, 4.0f);

    // Reserve left column for scale labels (-3, -6, -12, -24).
    auto scaleColumn = barRow.removeFromLeft(16.0f);
    barRow.removeFromLeft(2.0f);  // gap between labels and bar
    barRow.removeFromRight(2.0f); // tiny right padding so the bar isn't flush

    // Bar background (the empty / unfilled portion of the meter).
    g.setColour(juce::Colour(0xff1a1c19));
    g.fillRoundedRectangle(barRow, 2.0f);

    // dB-to-Y mapping. 0 dB at the top of the bar, displayFloorDb (-36)
    // at the bottom. GR is negative, so deeper reduction = more fill
    // from the top growing down.
    auto dbToY = [&](float db) {
        const float t = juce::jlimit(0.0f, 1.0f, db / displayFloorDb);
        return barRow.getY() + t * barRow.getHeight();
    };

    // ---- Grid lines + scale labels ------------------------------------
    g.setFont(Theme::mono(7.5f, juce::Font::bold));
    for (float dB : { -3.0f, -6.0f, -12.0f, -24.0f }) {
        const float y = dbToY(dB);

        // Subtle horizontal grid line across the bar.
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRect(barRow.getX(), y, barRow.getWidth(), 1.0f);

        // Right-aligned label in the scale column.
        g.setColour(Theme::scopeLabelDim);
        g.drawText(juce::String(static_cast<int>(dB)),
                   juce::Rectangle<float>(scaleColumn.getX(),
                                          y - 5.0f,
                                          scaleColumn.getWidth(),
                                          10.0f),
                   juce::Justification::centredRight);
    }

    // ---- Red GR fill from the top -------------------------------------
    // currentGrDb is lightly smoothed; bar height proportional to abs(GR).
    const float currentClamped = juce::jlimit(displayFloorDb, 0.0f, currentGrDb);
    const float fillT          = juce::jlimit(0.0f, 1.0f, currentClamped / displayFloorDb);
    const float fillH          = fillT * barRow.getHeight();
    if (fillH > 0.5f) {
        g.setColour(Theme::overload.withAlpha(0.85f));
        g.fillRect(barRow.getX(), barRow.getY(), barRow.getWidth(), fillH);
    }

    // ---- Held-peak hairline -------------------------------------------
    // Brighter than the fill, sits at displayedPeakDb position. Hangs
    // out at the worst-recent peak for `holdSeconds` then decays
    // upward toward 0 dB at `decayDbPerSec`.
    const float peakClamped = juce::jlimit(displayFloorDb, 0.0f, displayedPeakDb);
    if (peakClamped < -0.05f) {
        const float y = dbToY(peakClamped);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillRect(barRow.getX(), y - 0.5f, barRow.getWidth(), 1.5f);
    }
}
