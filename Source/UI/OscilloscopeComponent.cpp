#include "OscilloscopeComponent.h"
#include "../Parameters.h"
#include <limits>

OscilloscopeComponent::OscilloscopeComponent(ClipToZeroProcessor& p)
    : processor(p) {
    displayPre .reserve(maxScopeSamples);
    displayPost.reserve(maxScopeSamples);
    startTimerHz(30);
}

OscilloscopeComponent::~OscilloscopeComponent() = default;

void OscilloscopeComponent::timerCallback() {
    auto& fifo = processor.scopeFifo;

    // How many samples does the scope-length parameter want to display?
    const float scopeMs = processor.apvts.getRawParameterValue(Param::scopeLen)->load();
    const double sr     = processor.getSampleRate();
    int          target = static_cast<int>(std::round(sr * scopeMs / 1000.0));
    target = juce::jlimit(64, juce::jmin(maxScopeSamples, ClipToZeroProcessor::scopeSize - 256), target);

    const int avail = fifo.getNumReady();
    if (avail < target) {
        repaint();
        return;
    }

    // Drop everything older than the last `target` samples.
    if (avail > target) {
        const int toSkip = avail - target;
        int s1, n1, s2, n2;
        fifo.prepareToRead(toSkip, s1, n1, s2, n2);
        fifo.finishedRead(n1 + n2);
    }

    int start1, size1, start2, size2;
    fifo.prepareToRead(target, start1, size1, start2, size2);

    displayPre .resize(target);
    displayPost.resize(target);

    for (int i = 0; i < size1; ++i) {
        displayPre [i] = processor.scopePre [start1 + i];
        displayPost[i] = processor.scopePost[start1 + i];
    }
    for (int i = 0; i < size2; ++i) {
        displayPre [size1 + i] = processor.scopePre [start2 + i];
        displayPost[size1 + i] = processor.scopePost[start2 + i];
    }
    fifo.finishedRead(size1 + size2);
    activeSamples = target;

    repaint();
}

void OscilloscopeComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0a0a0a));

    // Centre + 0 dBFS reference rails.
    const float midY     = bounds.getCentreY();
    const float ampScale = bounds.getHeight() * 0.45f;
    g.setColour(juce::Colours::darkgrey.withAlpha(0.5f));
    g.drawLine(bounds.getX(), midY, bounds.getRight(), midY, 1.0f);
    g.setColour(juce::Colours::red.withAlpha(0.45f));
    g.drawLine(bounds.getX(), midY - ampScale, bounds.getRight(), midY - ampScale, 1.0f);
    g.drawLine(bounds.getX(), midY + ampScale, bounds.getRight(), midY + ampScale, 1.0f);

    if (activeSamples == 0) return;

    // Pick rendering strategy based on samples-per-pixel density. Below 2 spp
    // direct line drawing is still readable; above that we switch to min/max
    // decimation so transients aren't lost in overplotting.
    const float pixelWidth = bounds.getWidth();
    const float samplesPerPixel = static_cast<float>(activeSamples) / pixelWidth;

    if (samplesPerPixel <= 2.0f) drawZoomedIn (g, bounds);
    else                          drawZoomedOut(g, bounds);

    // Subtle scope-length readout in the top-right corner.
    g.setColour(juce::Colour(0xff808080));
    g.setFont(10.0f);
    const float ms = static_cast<float>(activeSamples) * 1000.0f
                     / static_cast<float>(juce::jmax(1, (int)processor.getSampleRate()));
    g.drawText(juce::String(ms, 1) + " ms",
               getLocalBounds().reduced(6).removeFromTop(14),
               juce::Justification::topRight);
}

void OscilloscopeComponent::drawZoomedIn(juce::Graphics& g, juce::Rectangle<float> bounds) const {
    const float midY     = bounds.getCentreY();
    const float ampScale = bounds.getHeight() * 0.45f;
    auto sampleToY = [&](float s) {
        return juce::jlimit(bounds.getY(), bounds.getBottom(),
                            midY - juce::jlimit(-1.5f, 1.5f, s) * ampScale);
    };

    const float step = bounds.getWidth() / static_cast<float>(juce::jmax(1, activeSamples - 1));

    // Pre-clip: ghosted grey trace — what would have happened without the clipper.
    juce::Path prePath;
    prePath.startNewSubPath(bounds.getX(), sampleToY(displayPre[0]));
    for (int i = 1; i < activeSamples; ++i)
        prePath.lineTo(bounds.getX() + i * step, sampleToY(displayPre[i]));
    g.setColour(juce::Colours::grey.withAlpha(0.55f));
    g.strokePath(prePath, juce::PathStrokeType(1.0f));

    // Post-clip: bright trace — actual output.
    juce::Path postPath;
    postPath.startNewSubPath(bounds.getX(), sampleToY(displayPost[0]));
    for (int i = 1; i < activeSamples; ++i)
        postPath.lineTo(bounds.getX() + i * step, sampleToY(displayPost[i]));
    g.setColour(juce::Colour(0xffe6e6e6));
    g.strokePath(postPath, juce::PathStrokeType(1.4f));

    // Highlight the gap between pre and post wherever the clipper actually
    // shaved samples — this is the "amount of clipping" the user wants to see.
    g.setColour(juce::Colours::red.withAlpha(0.55f));
    for (int i = 0; i < activeSamples; ++i) {
        const float diff = displayPre[i] - displayPost[i];
        if (std::abs(diff) > 0.001f) {
            const float x = bounds.getX() + i * step;
            const float yPre  = sampleToY(displayPre [i]);
            const float yPost = sampleToY(displayPost[i]);
            g.drawLine(x, yPre, x, yPost, juce::jmax(1.0f, step + 0.5f));
        }
    }
}

void OscilloscopeComponent::drawZoomedOut(juce::Graphics& g, juce::Rectangle<float> bounds) const {
    // Min/max decimation: for each pixel column, scan the samples that fall in
    // it and record min/max. Drawing a vertical line between min and max
    // preserves transient peaks at any zoom level — Reaper / Audition style.
    const int   pxLeft  = static_cast<int>(std::floor(bounds.getX()));
    const int   pxRight = static_cast<int>(std::ceil (bounds.getRight()));
    const int   pxWidth = juce::jmax(1, pxRight - pxLeft);

    const float midY     = bounds.getCentreY();
    const float ampScale = bounds.getHeight() * 0.45f;
    auto sampleToY = [&](float s) {
        return juce::jlimit(bounds.getY(), bounds.getBottom(),
                            midY - juce::jlimit(-1.5f, 1.5f, s) * ampScale);
    };

    // Pre-clip ghost layer.
    g.setColour(juce::Colours::grey.withAlpha(0.55f));
    for (int p = 0; p < pxWidth; ++p) {
        const int i0 = static_cast<int>(static_cast<int64_t>(p)     * activeSamples / pxWidth);
        const int i1 = static_cast<int>(static_cast<int64_t>(p + 1) * activeSamples / pxWidth);
        if (i1 <= i0) continue;
        float lo =  std::numeric_limits<float>::infinity();
        float hi = -std::numeric_limits<float>::infinity();
        for (int i = i0; i < i1; ++i) {
            const float v = displayPre[i];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        const float x = static_cast<float>(pxLeft + p);
        g.drawLine(x, sampleToY(lo), x, sampleToY(hi), 1.0f);
    }

    // Post-clip output layer.
    g.setColour(juce::Colour(0xffe6e6e6));
    for (int p = 0; p < pxWidth; ++p) {
        const int i0 = static_cast<int>(static_cast<int64_t>(p)     * activeSamples / pxWidth);
        const int i1 = static_cast<int>(static_cast<int64_t>(p + 1) * activeSamples / pxWidth);
        if (i1 <= i0) continue;
        float lo =  std::numeric_limits<float>::infinity();
        float hi = -std::numeric_limits<float>::infinity();
        for (int i = i0; i < i1; ++i) {
            const float v = displayPost[i];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        const float x = static_cast<float>(pxLeft + p);
        g.drawLine(x, sampleToY(lo), x, sampleToY(hi), 1.0f);
    }

    // Clipped-region highlights: where pre exceeded post (clip cut sample), draw
    // red between the two min/max ranges so the "shaved" area is visible.
    g.setColour(juce::Colours::red.withAlpha(0.55f));
    for (int p = 0; p < pxWidth; ++p) {
        const int i0 = static_cast<int>(static_cast<int64_t>(p)     * activeSamples / pxWidth);
        const int i1 = static_cast<int>(static_cast<int64_t>(p + 1) * activeSamples / pxWidth);
        if (i1 <= i0) continue;
        float preLo  =  std::numeric_limits<float>::infinity();
        float preHi  = -std::numeric_limits<float>::infinity();
        float postLo =  std::numeric_limits<float>::infinity();
        float postHi = -std::numeric_limits<float>::infinity();
        for (int i = i0; i < i1; ++i) {
            const float a = displayPre[i];
            const float b = displayPost[i];
            if (a < preLo)  preLo  = a;
            if (a > preHi)  preHi  = a;
            if (b < postLo) postLo = b;
            if (b > postHi) postHi = b;
        }
        const bool clippedTop = preHi - postHi >  0.001f;
        const bool clippedBot = postLo - preLo >  0.001f;
        if (!clippedTop && !clippedBot) continue;
        const float x = static_cast<float>(pxLeft + p);
        if (clippedTop) g.drawLine(x, sampleToY(preHi), x, sampleToY(postHi), 1.0f);
        if (clippedBot) g.drawLine(x, sampleToY(preLo), x, sampleToY(postLo), 1.0f);
    }
}
