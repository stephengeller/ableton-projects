#include "OscilloscopeComponent.h"

OscilloscopeComponent::OscilloscopeComponent(ClipToZeroProcessor& p)
    : processor(p) {
    startTimerHz(30);
}

OscilloscopeComponent::~OscilloscopeComponent() = default;

void OscilloscopeComponent::timerCallback() {
    auto& fifo = processor.scopeFifo;
    const int avail = fifo.getNumReady();

    if (avail < displaySamples) {
        repaint();
        return;
    }

    // Drop everything older than the last `displaySamples` samples.
    if (avail > displaySamples) {
        const int toSkip = avail - displaySamples;
        int s1, n1, s2, n2;
        fifo.prepareToRead(toSkip, s1, n1, s2, n2);
        fifo.finishedRead(n1 + n2);
    }

    int start1, size1, start2, size2;
    fifo.prepareToRead(displaySamples, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i) {
        displayPre [i] = processor.scopePre [start1 + i];
        displayPost[i] = processor.scopePost[start1 + i];
    }
    for (int i = 0; i < size2; ++i) {
        displayPre [size1 + i] = processor.scopePre [start2 + i];
        displayPost[size1 + i] = processor.scopePost[start2 + i];
    }
    fifo.finishedRead(size1 + size2);

    repaint();
}

void OscilloscopeComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0a0a0a));

    // Centre line + 0 dBFS reference lines.
    const float midY = bounds.getCentreY();
    const float ampScale = bounds.getHeight() * 0.45f;
    g.setColour(juce::Colours::darkgrey.withAlpha(0.5f));
    g.drawLine(bounds.getX(), midY, bounds.getRight(), midY, 1.0f);

    g.setColour(juce::Colours::red.withAlpha(0.45f));
    g.drawLine(bounds.getX(), midY - ampScale, bounds.getRight(), midY - ampScale, 1.0f);
    g.drawLine(bounds.getX(), midY + ampScale, bounds.getRight(), midY + ampScale, 1.0f);

    auto sampleToY = [&](float s) -> float {
        return juce::jlimit(bounds.getY(), bounds.getBottom(),
                            midY - juce::jlimit(-1.5f, 1.5f, s) * ampScale);
    };

    const float step = bounds.getWidth() / static_cast<float>(displaySamples - 1);

    // Pre-clip: ghosted grey trace shows what *would* have happened without the clipper.
    juce::Path prePath;
    prePath.startNewSubPath(bounds.getX(), sampleToY(displayPre[0]));
    for (int i = 1; i < displaySamples; ++i)
        prePath.lineTo(bounds.getX() + i * step, sampleToY(displayPre[i]));
    g.setColour(juce::Colours::grey.withAlpha(0.55f));
    g.strokePath(prePath, juce::PathStrokeType(1.0f));

    // Post-clip: bright trace, the actual output.
    juce::Path postPath;
    postPath.startNewSubPath(bounds.getX(), sampleToY(displayPost[0]));
    for (int i = 1; i < displaySamples; ++i)
        postPath.lineTo(bounds.getX() + i * step, sampleToY(displayPost[i]));
    g.setColour(juce::Colour(0xffe6e6e6));
    g.strokePath(postPath, juce::PathStrokeType(1.4f));

    // Highlight the gap between pre and post wherever the clipper actually
    // shaved samples — this is the "amount of clipping" the user wants to see.
    g.setColour(juce::Colours::red.withAlpha(0.55f));
    for (int i = 0; i < displaySamples; ++i) {
        const float diff = displayPre[i] - displayPost[i];
        if (std::abs(diff) > 0.001f) {
            const float x = bounds.getX() + i * step;
            const float yPre  = sampleToY(displayPre [i]);
            const float yPost = sampleToY(displayPost[i]);
            g.drawLine(x, yPre, x, yPost, juce::jmax(1.0f, step + 0.5f));
        }
    }
}
