#include "HorizontalMeter.h"

namespace {
    constexpr float minDb = -60.0f;
    constexpr float maxDb =   6.0f;

    inline float dbToNorm(float db) noexcept {
        return juce::jlimit(0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    }
}

HorizontalMeter::HorizontalMeter(LevelMeter& source, juce::String l, int ch)
    : meter(source), label(std::move(l)), channel(ch) {
    startTimerHz(30);
}

HorizontalMeter::~HorizontalMeter() = default;

void HorizontalMeter::timerCallback() {
    displayPeakDb = meter.getPeakDb(channel);
    displayRmsDb  = meter.getRmsDb(channel);
    displayHoldDb = meter.getPeakHoldDb(channel);
    repaint();
}

void HorizontalMeter::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    // Left: 10px label. Right: 38px numeric. Middle: the bar.
    auto labelArea = bounds.removeFromLeft(10);
    auto numericArea = bounds.removeFromRight(40);
    bounds.removeFromLeft(6);
    bounds.removeFromRight(6);

    // Channel label.
    g.setColour(Theme::textDim);
    g.setFont(Theme::mono(9.5f));
    g.drawText(label, labelArea, juce::Justification::centredLeft);

    const bool overload = displayPeakDb > 0.0f;

    // ---- Bar background ------------------------------------------------
    auto bar = bounds.withHeight(6).withCentre({ bounds.getCentreX(), bounds.getCentreY() });
    g.setColour(juce::Colour(0xff1a1c19));
    g.fillRoundedRectangle(bar.toFloat(), 1.0f);

    // ---- RMS fill (primary) -------------------------------------------
    {
        const float rmsT = dbToNorm(displayRmsDb);
        const int rmsW = juce::roundToInt(rmsT * bar.getWidth());
        if (rmsW > 0) {
            g.setColour(overload ? Theme::overload : Theme::textBright);
            g.fillRoundedRectangle(juce::Rectangle<int>(bar.getX(), bar.getY(), rmsW, bar.getHeight()).toFloat(), 1.0f);
        }
    }

    // ---- Peak overlay (semi-transparent over RMS) ----------------------
    {
        const float peakT = dbToNorm(displayPeakDb);
        const int peakW = juce::roundToInt(peakT * bar.getWidth());
        if (peakW > 0) {
            g.setColour((overload ? Theme::overload : Theme::accent).withAlpha(0.55f));
            g.fillRoundedRectangle(juce::Rectangle<int>(bar.getX(), bar.getY(), peakW, bar.getHeight()).toFloat(), 1.0f);
        }
    }

    // ---- Peak hold tick (1px white) -----------------------------------
    {
        const float holdT = dbToNorm(displayHoldDb);
        const int holdX = bar.getX() + juce::roundToInt(holdT * bar.getWidth());
        g.setColour(juce::Colours::white);
        g.fillRect(holdX, bar.getY() - 1, 1, bar.getHeight() + 2);
    }

    // ---- 0 dBFS reference tick ----------------------------------------
    {
        const float zeroT = dbToNorm(0.0f);
        const int zeroX = bar.getX() + juce::roundToInt(zeroT * bar.getWidth());
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.fillRect(zeroX, bar.getY() - 2, 1, bar.getHeight() + 4);
    }

    // ---- Numeric readout ------------------------------------------------
    g.setColour(overload ? Theme::overloadDim : Theme::textBright);
    g.setFont(Theme::mono(9.5f));
    auto txt = (displayPeakDb <= -99.5f) ? juce::String("-inf")
                                         : juce::String(displayPeakDb, 1);
    g.drawText(txt, numericArea, juce::Justification::centredRight);
}
