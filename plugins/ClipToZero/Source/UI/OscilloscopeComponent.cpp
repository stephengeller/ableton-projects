#include "OscilloscopeComponent.h"
#include "../Parameters.h"
#include <limits>

OscilloscopeComponent::OscilloscopeComponent(ClipToZeroProcessor& p)
    : processor(p) {
    displayPre .reserve(maxScopeSamples);
    displayPost.reserve(maxScopeSamples);
    // 120 Hz: ProMotion displays update at 120 Hz, and even on 60 Hz panels
    // we just produce one extra (coalesced-away) frame per vsync — cheap.
    // Combined with the shift-and-append timeline below this gives smooth
    // visible motion at every zoom level.
    startTimerHz(120);
}

OscilloscopeComponent::~OscilloscopeComponent() = default;

void OscilloscopeComponent::timerCallback() {
    auto& fifo = processor.scopeFifo;

    // ---- Resolve the desired window length in samples ----
    const float scopeMs = processor.apvts.getRawParameterValue(Param::scopeLen)->load();
    const double sr     = processor.getSampleRate();
    int          target = static_cast<int>(std::round(sr * scopeMs / 1000.0));
    target = juce::jlimit(64, juce::jmin(maxScopeSamples, ClipToZeroProcessor::scopeSize - 256), target);

    // ---- Resize the persistent display buffers if scope length changed ----
    // We preserve the rightmost (newest) samples so adjusting the zoom slider
    // doesn't blank the screen — the right edge is always "now".
    const int prevSize = static_cast<int>(displayPre.size());
    if (prevSize != target) {
        std::vector<float> newPre (target, 0.0f);
        std::vector<float> newPost(target, 0.0f);
        const int copyN = juce::jmin(prevSize, target);
        if (copyN > 0) {
            std::memcpy(newPre .data() + (target - copyN),
                        displayPre .data() + (prevSize - copyN),
                        static_cast<size_t>(copyN) * sizeof(float));
            std::memcpy(newPost.data() + (target - copyN),
                        displayPost.data() + (prevSize - copyN),
                        static_cast<size_t>(copyN) * sizeof(float));
        }
        displayPre  = std::move(newPre);
        displayPost = std::move(newPost);
        activeSamples = target;
    }

    // ---- Pull whatever new audio has arrived since last tick ----
    // Key change vs. the old code: we DON'T drain the FIFO down to a fresh
    // `target` slab each frame. We keep `displayPre/Post` as the current
    // visible state, scroll it left by the number of new samples, and append
    // the new audio at the right. That way every timer tick produces visible
    // motion proportional to elapsed audio time — which at wide zoom is what
    // makes the timeline look like it's actually moving instead of jumping.
    const int avail = fifo.getNumReady();

    if (avail == 0) {
        // No new audio this tick — repaint anyway so the corner readouts
        // refresh if the user just dragged a parameter.
        repaint();
        return;
    }

    if (avail >= target) {
        // FIFO has at least a full window. This happens after a long pause
        // (window closed, plugin just loaded, or scope length just shrank).
        // Drop the older samples and overwrite the entire buffer with the
        // newest `target`.
        const int toSkip = avail - target;
        if (toSkip > 0) {
            int s1, n1, s2, n2;
            fifo.prepareToRead(toSkip, s1, n1, s2, n2);
            fifo.finishedRead(n1 + n2);
        }
        int s1, n1, s2, n2;
        fifo.prepareToRead(target, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) {
            displayPre [i] = processor.scopePre [s1 + i];
            displayPost[i] = processor.scopePost[s1 + i];
        }
        for (int i = 0; i < n2; ++i) {
            displayPre [n1 + i] = processor.scopePre [s2 + i];
            displayPost[n1 + i] = processor.scopePost[s2 + i];
        }
        fifo.finishedRead(n1 + n2);
    } else {
        // Normal scrolling: shift the window left by `avail`, drop the
        // oldest `avail` samples, append `avail` new ones at the right edge.
        // memmove handles the overlapping source/dest correctly.
        const int newCount = avail;
        const int keep     = target - newCount;
        std::memmove(displayPre .data(),
                     displayPre .data() + newCount,
                     static_cast<size_t>(keep) * sizeof(float));
        std::memmove(displayPost.data(),
                     displayPost.data() + newCount,
                     static_cast<size_t>(keep) * sizeof(float));

        int s1, n1, s2, n2;
        fifo.prepareToRead(newCount, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) {
            displayPre [keep + i] = processor.scopePre [s1 + i];
            displayPost[keep + i] = processor.scopePost[s1 + i];
        }
        for (int i = 0; i < n2; ++i) {
            displayPre [keep + n1 + i] = processor.scopePre [s2 + i];
            displayPost[keep + n1 + i] = processor.scopePost[s2 + i];
        }
        fifo.finishedRead(n1 + n2);
    }

    activeSamples = target;
    repaint();
}

void OscilloscopeComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0a0a0a));

    // Compute the vertical mapping from the user's headroom setting.
    //   maxAmp   = highest amplitude that fits at the edge of the scope
    //   ampScale = pixels per amplitude unit
    // 0 dBFS rails then sit at midY ± ampScale (since amplitude 1.0 = 0 dBFS).
    const float headroomDb = processor.apvts.getRawParameterValue(Param::vertHeadroom)->load();
    const float maxAmp     = juce::Decibels::decibelsToGain(headroomDb);
    const float midY       = bounds.getCentreY();
    const float halfH      = bounds.getHeight() * 0.5f - 4.0f;
    const float ampScale   = halfH / juce::jmax(0.001f, maxAmp);

    // Background: centre line + 0 dBFS reference rails.
    g.setColour(juce::Colours::darkgrey.withAlpha(0.5f));
    g.drawLine(bounds.getX(), midY, bounds.getRight(), midY, 1.0f);

    const float yPlus0  = midY - ampScale;
    const float yMinus0 = midY + ampScale;
    g.setColour(juce::Colours::red.withAlpha(0.45f));
    g.drawLine(bounds.getX(), yPlus0,  bounds.getRight(), yPlus0,  1.0f);
    g.drawLine(bounds.getX(), yMinus0, bounds.getRight(), yMinus0, 1.0f);

    auto drawCornerReadouts = [&] {
        g.setColour(juce::Colour(0xff808080));
        g.setFont(10.0f);
        g.drawText("+" + juce::String(headroomDb, 1) + " dB",
                   getLocalBounds().reduced(6).removeFromTop(14),
                   juce::Justification::topLeft);
        if (activeSamples > 0) {
            const float ms = static_cast<float>(activeSamples) * 1000.0f
                             / static_cast<float>(juce::jmax(1, (int)processor.getSampleRate()));
            g.drawText(juce::String(ms, 1) + " ms",
                       getLocalBounds().reduced(6).removeFromTop(14),
                       juce::Justification::topRight);
        }
    };

    if (activeSamples == 0) {
        drawCornerReadouts();
        return;
    }

    // Pick rendering strategy based on samples-per-pixel density.
    const float samplesPerPixel = static_cast<float>(activeSamples) / bounds.getWidth();
    if (samplesPerPixel <= 2.0f) drawZoomedIn (g, bounds, midY, ampScale);
    else                          drawZoomedOut(g, bounds, midY, ampScale);

    // "Now" indicator — subtle vertical bar at the right edge to reinforce
    // the timeline metaphor: newest sample is here, time scrolls leftward.
    g.setColour(juce::Colour(0xff60c060).withAlpha(0.45f));
    g.drawLine(bounds.getRight() - 0.5f, bounds.getY(),
               bounds.getRight() - 0.5f, bounds.getBottom(), 1.0f);

    drawCornerReadouts();
}

void OscilloscopeComponent::drawZoomedIn(juce::Graphics& g,
                                          juce::Rectangle<float> bounds,
                                          float midY,
                                          float ampScale) const {
    auto sampleToY = [&](float s) {
        return juce::jlimit(bounds.getY(), bounds.getBottom(), midY - s * ampScale);
    };

    const float step = bounds.getWidth() / static_cast<float>(juce::jmax(1, activeSamples - 1));

    juce::Path prePath;
    prePath.startNewSubPath(bounds.getX(), sampleToY(displayPre[0]));
    for (int i = 1; i < activeSamples; ++i)
        prePath.lineTo(bounds.getX() + i * step, sampleToY(displayPre[i]));
    g.setColour(juce::Colours::grey.withAlpha(0.55f));
    g.strokePath(prePath, juce::PathStrokeType(1.0f));

    juce::Path postPath;
    postPath.startNewSubPath(bounds.getX(), sampleToY(displayPost[0]));
    for (int i = 1; i < activeSamples; ++i)
        postPath.lineTo(bounds.getX() + i * step, sampleToY(displayPost[i]));
    g.setColour(juce::Colour(0xffe6e6e6));
    g.strokePath(postPath, juce::PathStrokeType(1.4f));

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

void OscilloscopeComponent::drawZoomedOut(juce::Graphics& g,
                                           juce::Rectangle<float> bounds,
                                           float midY,
                                           float ampScale) const {
    const int   pxLeft  = static_cast<int>(std::floor(bounds.getX()));
    const int   pxRight = static_cast<int>(std::ceil (bounds.getRight()));
    const int   pxWidth = juce::jmax(1, pxRight - pxLeft);

    auto sampleToY = [&](float s) {
        return juce::jlimit(bounds.getY(), bounds.getBottom(), midY - s * ampScale);
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

    // Clipped-region highlights: where pre's min/max exceeded post's, draw
    // red between the two ranges so the "shaved" area is visible at any zoom.
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
