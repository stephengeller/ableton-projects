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
    activeBins    = target;
    currentPeakDb = processor.grHistory.getRecentPeakGrDb();
    repaint();
}

void GRMeterComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(Theme::bgDeeper);

    // Top edge: subtle 0 dB reference line, matching the scope's rail style.
    g.setColour(Theme::scopeRail);
    g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), 1.0f);

    // ---- Red fill bars ------------------------------------------------
    // Each pixel column maps to a bin in `latest`. We pick the most-
    // negative GR in the bins that span that pixel so transient spikes
    // remain visible at any zoom.
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

    // ---- "Now" tick + corner readouts ---------------------------------
    g.setColour(Theme::scopeNowLine);
    g.fillRect(bounds.getRight() - 1.0f, bounds.getY(), 1.0f, bounds.getHeight());

    g.setColour(Theme::scopeLabelDim);
    g.setFont(Theme::mono(9.0f));
    g.drawText("GR",
               getLocalBounds().reduced(4, 1).removeFromTop(12),
               juce::Justification::topLeft);

    // Numeric readout: current peak GR over the recent window.
    const auto valueText = (currentPeakDb >= -0.05f)
                              ? juce::String("0.0 dB")
                              : juce::String(currentPeakDb, 1) + " dB";
    g.setColour(currentPeakDb < -0.5f ? Theme::overload : Theme::scopeLabelMid);
    g.drawText(valueText,
               getLocalBounds().reduced(4, 1).removeFromTop(12),
               juce::Justification::topRight);
}
