#include "MeterComponent.h"

namespace {
    constexpr float minDb = -60.0f;
    constexpr float maxDb =   6.0f;

    inline int dbToY(float db, juce::Rectangle<int> area) {
        const float t = juce::jlimit(0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        return area.getBottom() - static_cast<int>(t * area.getHeight());
    }
}

MeterComponent::MeterComponent(LevelMeter& source, juce::String l)
    : meter(source), label(std::move(l)) {
    startTimerHz(30);
}

MeterComponent::~MeterComponent() = default;

void MeterComponent::timerCallback() {
    for (int ch = 0; ch < 2; ++ch) {
        displayPeakDb[ch] = meter.getPeakDb(ch);
        displayRmsDb [ch] = meter.getRmsDb(ch);
        displayHoldDb[ch] = meter.getPeakHoldDb(ch);
    }
    repaint();
}

void MeterComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xff0a0a0a));

    g.setColour(juce::Colours::lightgrey);
    g.setFont(11.0f);
    g.drawText(label, bounds.removeFromTop(14), juce::Justification::centred);

    auto numericArea = bounds.removeFromBottom(14);

    auto meterArea = bounds.reduced(4, 2);
    const int channelWidth = meterArea.getWidth() / 2;

    for (int ch = 0; ch < 2; ++ch) {
        auto chRect = meterArea.removeFromLeft(channelWidth - 1);
        meterArea.removeFromLeft(2);

        g.setColour(juce::Colour(0xff181818));
        g.fillRect(chRect);

        // RMS: filled bar.
        const int yRms = dbToY(displayRmsDb[ch], chRect);
        g.setColour(juce::Colour(0xff2f7f3f));
        g.fillRect(chRect.getX(), yRms, chRect.getWidth(), chRect.getBottom() - yRms);

        // Peak: thick coloured bar.
        const int yPeak = dbToY(displayPeakDb[ch], chRect);
        g.setColour(colourForDb(displayPeakDb[ch]));
        g.fillRect(chRect.getX(), yPeak, chRect.getWidth(), 2);

        // Peak hold: 1px line, sticks for 1.5s.
        const int yHold = dbToY(displayHoldDb[ch], chRect);
        g.setColour(colourForDb(displayHoldDb[ch]).withAlpha(0.85f));
        g.fillRect(chRect.getX(), yHold, chRect.getWidth(), 1);

        // 0 dBFS reference line.
        const int y0 = dbToY(0.0f, chRect);
        g.setColour(juce::Colours::red.withAlpha(0.45f));
        g.fillRect(chRect.getX(), y0, chRect.getWidth(), 1);
    }

    // Numeric peak readout (the higher of the two channels).
    g.setColour(juce::Colours::lightgrey);
    g.setFont(10.0f);
    const float showDb = juce::jmax(displayPeakDb[0], displayPeakDb[1]);
    const auto txt = (showDb <= -99.0f) ? juce::String("-inf")
                                        : juce::String(showDb, 1) + " dB";
    g.drawText(txt, numericArea, juce::Justification::centred);
}

juce::Colour MeterComponent::colourForDb(float db) {
    if (db >=   0.0f) return juce::Colours::red;
    if (db >=  -3.0f) return juce::Colour(0xffffaa33);
    if (db >= -12.0f) return juce::Colour(0xffe0d040);
    return juce::Colour(0xff60c060);
}
