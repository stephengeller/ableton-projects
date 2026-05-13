#include "GRMeterComponent.h"
#include "../Parameters.h"
#include "Theme.h"

GRMeterComponent::GRMeterComponent(ClipToZeroProcessor& p) : processor(p) {
    latest.reserve(GRHistory::historySize);
    // 60 Hz repaint to keep visual motion in sync with the scope above.
    startTimerHz(60);
}

GRMeterComponent::~GRMeterComponent() = default;

void GRMeterComponent::timerCallback() {
    // The scope-length parameter is in milliseconds. Since each GR bin is
    // 1 ms, we just round and clamp to the history capacity.
    const float scopeMs = processor.apvts.getRawParameterValue(Param::scopeLen)->load();
    const int target = juce::jlimit(8, GRHistory::historySize,
                                    static_cast<int>(std::round(scopeMs)));
    latest.resize(target);
    processor.grHistory.readLatest(latest.data(), target);
    activeBins = target;

    // ---- Peak hold + decay on the numeric readout ---------------------
    // Three branches:
    //   1. New peak is worse than what's displayed -> snap up immediately
    //      and reset the hold timer (the user wants to read this value).
    //   2. Inside the hold window -> freeze the displayed value.
    //   3. Hold expired -> decay linearly toward the *current* recent peak
    //      so sustained clipping doesn't visually release while still
    //      active; we never decay past what's actually happening.
    const float recentPeak = processor.grHistory.getRecentPeakGrDb();
    const double nowMs     = juce::Time::getMillisecondCounterHiRes();
    const double deltaSec  = (lastTimerMs > 0.0) ? (nowMs - lastTimerMs) * 0.001 : 0.0;
    lastTimerMs = nowMs;

    if (recentPeak < displayedPeakDb) {
        // Worse hit — snap up + reset the hold timer.
        displayedPeakDb  = recentPeak;
        secondsSincePeak = 0.0;
    } else {
        secondsSincePeak += deltaSec;
        if (secondsSincePeak > holdSeconds) {
            const float maxStep = decayDbPerSec * static_cast<float>(deltaSec);
            // Decay toward recentPeak (which may be 0 if there's been no
            // recent activity, OR a smaller GR if clipping continues).
            // jmin keeps us from decaying *past* the current value.
            displayedPeakDb = juce::jmin(recentPeak, displayedPeakDb + maxStep);
        }
    }

    repaint();
}

void GRMeterComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(Theme::bgDeeper);

    // Reserve a right-edge gutter for the dB scale labels so they don't
    // overlap the red bars or the "now" tick. The bar/grid plotting uses
    // `plotBounds` (excluding the gutter); the labels draw inside it.
    constexpr float scaleGutterW = 26.0f;
    auto plotBounds  = bounds.withTrimmedRight(scaleGutterW);
    auto scaleBounds = bounds.removeFromRight(scaleGutterW);

    // ---- Reference grid + rails ---------------------------------------
    // 0 dB at the top (full strip height = displayFloorDb).
    auto dbToY = [&](float db) {
        const float t = juce::jlimit(0.0f, 1.0f, db / displayFloorDb);
        return plotBounds.getY() + t * plotBounds.getHeight();
    };

    // 0 dB top rail.
    g.setColour(Theme::scopeRail);
    g.fillRect(plotBounds.getX(), plotBounds.getY(), plotBounds.getWidth(), 1.0f);

    // -3 / -6 / -12 dB grid lines + matching dB scale labels in the
    // right-edge gutter. Lifted from Theme::scopeGrid (alpha 0.04, almost
    // invisible) to an explicit 0.12 so the user can actually read the
    // magnitude off the strip. The labels sit on the same baseline as
    // their respective grid lines so the eye can connect them.
    const juce::Colour gridColour = Theme::scopeLabelDim.withAlpha(0.18f);
    const juce::Colour labelColour = Theme::scopeLabelDim;
    g.setFont(Theme::mono(7.5f));

    for (float dB : { -3.0f, -6.0f, -12.0f, -24.0f }) {
        const float y = dbToY(dB);
        // Grid line stops at the gutter boundary so it doesn't run
        // through the label text.
        g.setColour(gridColour);
        g.fillRect(plotBounds.getX(), y, plotBounds.getWidth(), 1.0f);
        // Label sits in the gutter, vertically centred on the grid line.
        g.setColour(labelColour);
        g.drawText(juce::String(static_cast<int>(dB)),
                   juce::Rectangle<float>(scaleBounds.getX() + 1.0f, y - 5.0f,
                                          scaleBounds.getWidth() - 3.0f, 10.0f),
                   juce::Justification::centredRight);
    }

    // ---- Red fill bars ------------------------------------------------
    // Each pixel column maps to a bin in `latest`. We pick the most-
    // negative GR in the bins that span that pixel so transient spikes
    // remain visible at any zoom.
    const int pxLeft  = static_cast<int>(std::floor(plotBounds.getX()));
    const int pxRight = static_cast<int>(std::ceil (plotBounds.getRight()));
    const int pxWidth = juce::jmax(1, pxRight - pxLeft);

    g.setColour(Theme::scopeDiff);  // translucent red

    if (activeBins > 0) {
        for (int p = 0; p < pxWidth; ++p) {
            const int b0 = static_cast<int>(static_cast<int64_t>(p)     * activeBins / pxWidth);
            const int b1 = static_cast<int>(static_cast<int64_t>(p + 1) * activeBins / pxWidth);
            if (b1 <= b0) continue;

            float peakGr = 0.0f;
            for (int b = b0; b < b1; ++b) {
                if (latest[b] < peakGr) peakGr = latest[b];
            }
            if (peakGr >= -0.05f) continue;  // nothing to draw

            const float t = juce::jlimit(0.0f, 1.0f, peakGr / displayFloorDb);
            const float barH = t * plotBounds.getHeight();
            g.fillRect(static_cast<float>(pxLeft + p), plotBounds.getY(), 1.0f, barH);
        }
    }

    // ---- Held-peak hairline -------------------------------------------
    // Mirrors what the numeric corner readout says, but as a horizontal
    // line across the strip. Hangs out where the worst-recent peak sat,
    // following the same hold + decay envelope as the numeric value.
    if (displayedPeakDb < -0.5f) {
        const float y = dbToY(displayedPeakDb);
        g.setColour(Theme::overload.withAlpha(0.55f));
        g.fillRect(plotBounds.getX(), y, plotBounds.getWidth(), 1.0f);
    }

    // ---- "Now" tick + corner readouts ---------------------------------
    // Now-tick sits at the right edge of the PLOT region (just before the
    // gutter starts), so it lines up with the latest bin under it.
    g.setColour(Theme::scopeNowLine);
    g.fillRect(plotBounds.getRight() - 1.0f, plotBounds.getY(), 1.0f, plotBounds.getHeight());

    g.setColour(Theme::scopeLabelDim);
    g.setFont(Theme::mono(9.0f));
    g.drawText("GR",
               getLocalBounds().reduced(4, 1).removeFromTop(12),
               juce::Justification::topLeft);

    // Numeric readout: held peak GR (hold + decay). Drawn in the
    // top-right of the plot area (NOT the gutter -- gutter is reserved
    // for the scale labels).
    const auto valueText = (displayedPeakDb >= -0.05f)
                              ? juce::String("0.0 dB")
                              : juce::String(displayedPeakDb, 1) + " dB";
    g.setColour(displayedPeakDb < -0.5f ? Theme::overload : Theme::scopeLabelMid);
    g.drawText(valueText,
               juce::Rectangle<float>(plotBounds.getX() + 4.0f,
                                      plotBounds.getY() + 1.0f,
                                      plotBounds.getWidth() - 8.0f, 12.0f),
               juce::Justification::topRight);
}
