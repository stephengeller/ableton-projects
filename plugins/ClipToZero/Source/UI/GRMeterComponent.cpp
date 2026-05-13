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

    // No gutter. The bars + grid + "now" tick fill the full strip width
    // so the GR strip's time axis lines up exactly with the scope above.
    // Scale labels are overlaid on the right edge with dark pill
    // backgrounds for readability against the red bars.

    auto dbToY = [&](float db) {
        const float t = juce::jlimit(0.0f, 1.0f, db / displayFloorDb);
        return bounds.getY() + t * bounds.getHeight();
    };

    // 0 dB top rail across the full width.
    g.setColour(Theme::scopeRail);
    g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), 1.0f);

    // -6 / -12 / -24 dB grid lines. -3 dropped because it sits right
    // under the top readout row at this strip height and produces a
    // crowded look. Grid colour lifted from the near-invisible
    // Theme::scopeGrid (alpha 0.04) to an explicit ~0.18 alpha of
    // scopeLabelDim so the user can actually trace each grid line.
    const juce::Colour gridColour = Theme::scopeLabelDim.withAlpha(0.18f);
    for (float dB : { -6.0f, -12.0f, -24.0f }) {
        const float y = dbToY(dB);
        g.setColour(gridColour);
        g.fillRect(bounds.getX(), y, bounds.getWidth(), 1.0f);
    }

    // ---- Red fill bars ------------------------------------------------
    // Each pixel column maps to a bin in `latest`. We pick the most-
    // negative GR in the bins that span that pixel so transient spikes
    // remain visible at any zoom. Drawn across the full strip width so
    // the rightmost pixel = newest bin = same X as the scope's "now"
    // marker above.
    const int pxLeft  = static_cast<int>(std::floor(bounds.getX()));
    const int pxRight = static_cast<int>(std::ceil (bounds.getRight()));
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
            const float barH = t * bounds.getHeight();
            g.fillRect(static_cast<float>(pxLeft + p), bounds.getY(), 1.0f, barH);
        }
    }

    // ---- Held-peak hairline -------------------------------------------
    // Mirrors what the numeric readout in the top-left corner says, but
    // as a horizontal line across the strip. Hangs out where the worst-
    // recent peak sat, following the same hold + decay envelope as the
    // numeric value.
    if (displayedPeakDb < -0.5f) {
        const float y = dbToY(displayedPeakDb);
        g.setColour(Theme::overload.withAlpha(0.55f));
        g.fillRect(bounds.getX(), y, bounds.getWidth(), 1.0f);
    }

    // ---- "Now" tick ---------------------------------------------------
    // Right at the right edge of the bounds -- the same X position as
    // the scope's "now" indicator above. (Pre-v0.5.7 this sat 26 px
    // inset due to the scale-label gutter, which made the GR peaks look
    // shifted 26 px left of the scope's clipped regions.)
    g.setColour(Theme::scopeNowLine);
    g.fillRect(bounds.getRight() - 1.0f, bounds.getY(), 1.0f, bounds.getHeight());

    // ---- Top-left readout: 'GR  -X.X dB' combined --------------------
    // Inlined into a single label so the right edge stays free for the
    // scale numbers. 'GR' renders dim, the dB value renders brighter
    // (and red when there's actual clipping) so the visual hierarchy
    // stays clear.
    {
        auto topLeft = getLocalBounds().reduced(4, 1).removeFromTop(12);
        g.setFont(Theme::mono(9.0f));

        constexpr int grLabelW = 20;
        g.setColour(Theme::scopeLabelDim);
        g.drawText("GR", topLeft.removeFromLeft(grLabelW), juce::Justification::centredLeft);

        const auto valueText = (displayedPeakDb >= -0.05f)
                                  ? juce::String("0.0 dB")
                                  : juce::String(displayedPeakDb, 1) + " dB";
        g.setColour(displayedPeakDb < -0.5f ? Theme::overload : Theme::scopeLabelMid);
        g.drawText(valueText, topLeft, juce::Justification::centredLeft);
    }

    // ---- Right-edge scale labels (overlay with dark pills) -----------
    // Each dB value gets a small dark rounded pill behind it so it
    // remains readable when red bars happen to be drawn at that height.
    // 8.5pt bold + bright cream text is a big readability bump from the
    // previous 7.5pt dim text floating naked over the bars.
    {
        constexpr float labelW = 28.0f;
        constexpr float labelH = 12.0f;
        g.setFont(Theme::mono(8.5f, juce::Font::bold));

        for (float dB : { -6.0f, -12.0f, -24.0f }) {
            const float y = dbToY(dB);
            const juce::Rectangle<float> pill(bounds.getRight() - labelW - 2.0f,
                                              y - labelH * 0.5f,
                                              labelW, labelH);
            g.setColour(Theme::bgDeeper.withAlpha(0.80f));
            g.fillRoundedRectangle(pill, 2.0f);
            g.setColour(Theme::scopeLabelMid);
            g.drawText(juce::String(static_cast<int>(dB)),
                       pill.reduced(3.0f, 0.0f),
                       juce::Justification::centredRight);
        }
    }
}
