#include "OscilloscopeComponent.h"
#include "../Parameters.h"
#include "Theme.h"
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

    // Refresh the spectrum overlay's FFT bins. Runs on the GUI thread —
    // the audio thread just pushes samples into a ring buffer; this is
    // where the heavy lifting (FFT + windowing + per-bin smoothing)
    // actually happens. Cheap enough for 120 Hz, but the SpectrumAnalyzer
    // gates itself internally so it only transforms when fftSize new
    // samples have accumulated.
    processor.spectrum.computeIfReady();

    repaint();
}

void OscilloscopeComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(Theme::bg);

    // Vertical mapping from the user's headroom parameter:
    //   maxAmp   = amplitude that fits at the edge of the scope
    //   ampScale = pixels per amplitude unit (so 1.0 amp = ampScale pixels)
    // The 0 dBFS rails (sample = ±1) sit at midY ± ampScale.
    const float headroomDb = processor.apvts.getRawParameterValue(Param::vertHeadroom)->load();
    const float maxAmp     = juce::Decibels::decibelsToGain(headroomDb);
    const float midY       = bounds.getCentreY();
    const float halfH      = bounds.getHeight() * 0.5f - 4.0f;
    const float ampScale   = halfH / juce::jmax(0.001f, maxAmp);

    drawBackground(g, bounds, midY, ampScale);
    // Spectrum FILL sits BEHIND the time-domain traces so the wave still
    // pops visually. The outline draws later, on top of everything else.
    drawSpectrumFill(g, bounds);

    if (activeSamples > 0) {
        const float samplesPerPixel = static_cast<float>(activeSamples) / bounds.getWidth();
        if (samplesPerPixel <= 2.0f) drawZoomedIn (g, bounds, midY, ampScale);
        else                          drawZoomedOut(g, bounds, midY, ampScale);
    }

    // Outline goes ON TOP of the time-domain — keeps the spectrum's shape
    // legible even under heavy clipping when the diff-fill is busy.
    drawSpectrumOutline(g, bounds);

    drawOverlays(g, headroomDb);
}

juce::Path OscilloscopeComponent::computeSpectrumPath(juce::Rectangle<float> bounds) const {
    juce::Path path;
    const auto* mags = processor.spectrum.getMagnitudesDb();
    const double sr  = processor.spectrum.getSampleRate();
    if (mags == nullptr || sr <= 0.0) return path;

    constexpr float dbTop      = 0.0f;
    constexpr float dbBottom   = -60.0f;
    constexpr float minFreqHz  = 20.0f;
    const     float nyquistHz  = static_cast<float>(sr) * 0.5f;
    const     float logMin     = std::log(minFreqHz);
    const     float logMax     = std::log(juce::jmax(nyquistHz, minFreqHz + 1.0f));

    auto dbToHeight = [&](float db) {
        const float t = juce::jlimit(0.0f, 1.0f, (db - dbBottom) / (dbTop - dbBottom));
        return t * bounds.getHeight();
    };

    const int   pxLeft  = static_cast<int>(std::floor(bounds.getX()));
    const int   pxRight = static_cast<int>(std::ceil (bounds.getRight()));
    const float bottom  = bounds.getBottom();

    path.startNewSubPath(static_cast<float>(pxLeft), bottom);
    for (int p = pxLeft; p <= pxRight; ++p) {
        const float xNorm   = (p - pxLeft) / juce::jmax(1.0f, bounds.getWidth());
        const float freq    = std::exp(logMin + xNorm * (logMax - logMin));
        const float binPosF = freq * SpectrumAnalyzer::fftSize / static_cast<float>(sr);
        const int   bin0    = juce::jlimit(0, SpectrumAnalyzer::numBins - 1, static_cast<int>(std::floor(binPosF)));
        const int   bin1    = juce::jlimit(0, SpectrumAnalyzer::numBins - 1, bin0 + 1);
        const float t       = binPosF - static_cast<float>(bin0);
        const float magDb   = mags[bin0] * (1.0f - t) + mags[bin1] * t;
        const float h       = dbToHeight(magDb);
        path.lineTo(static_cast<float>(p), bottom - h);
    }
    path.lineTo(static_cast<float>(pxRight), bottom);
    path.closeSubPath();
    return path;
}

void OscilloscopeComponent::drawSpectrumFill(juce::Graphics& g, juce::Rectangle<float> bounds) const {
    const int mode = static_cast<int>(processor.apvts.getRawParameterValue(Param::spectrumMode)->load());
    if (mode == 0) return;  // Off

    // Subtle = 0.10, Bold = 0.20. Bold reads through the wave better even
    // when occluded; Subtle stays unobtrusive.
    const float fillAlpha = (mode == 2) ? 0.20f : 0.10f;
    auto path = computeSpectrumPath(bounds);
    g.setColour(Theme::accent.withAlpha(fillAlpha));
    g.fillPath(path);
}

void OscilloscopeComponent::drawSpectrumOutline(juce::Graphics& g, juce::Rectangle<float> bounds) const {
    const int mode = static_cast<int>(processor.apvts.getRawParameterValue(Param::spectrumMode)->load());
    if (mode == 0) return;

    // Outline alpha + thickness scale with mode. The outline draws on top
    // of everything (after time-domain traces) so the spectrum shape
    // remains visible during heavy clipping.
    const float outlineAlpha = (mode == 2) ? 0.80f : 0.40f;
    const float outlineWidth = (mode == 2) ? 1.3f  : 0.8f;
    auto path = computeSpectrumPath(bounds);
    g.setColour(Theme::accent.withAlpha(outlineAlpha));
    g.strokePath(path, juce::PathStrokeType(outlineWidth));
}

void OscilloscopeComponent::drawBackground(juce::Graphics& g, juce::Rectangle<float> bounds,
                                           float midY, float ampScale) const {
    // Subtle 10 x 6 grid — F's design has a hint of grid in the canvas.
    g.setColour(Theme::scopeGrid);
    const int vCount = 10;
    const int hCount = 6;
    for (int i = 1; i < vCount; ++i) {
        const float x = bounds.getX() + bounds.getWidth() * (float)i / (float)vCount;
        g.fillRect(x, bounds.getY(), 1.0f, bounds.getHeight());
    }
    for (int i = 1; i < hCount; ++i) {
        const float y = bounds.getY() + bounds.getHeight() * (float)i / (float)hCount;
        g.fillRect(bounds.getX(), y, bounds.getWidth(), 1.0f);
    }

    // Centre line.
    g.setColour(Theme::border);
    g.fillRect(bounds.getX(), midY - 0.5f, bounds.getWidth(), 1.0f);

    // 0 dBFS rails — dashed, so they read as "limits" rather than waveform.
    // Right-edge "0 dB" labels mark where the rails sit so they're not
    // lost in a heavily-clipped scope (the dashed line alone disappears
    // into the cream waveform fills when the audio is hot).
    {
        const float yPlus0  = midY - ampScale;
        const float yMinus0 = midY + ampScale;
        g.setColour(Theme::scopeRail);
        const float dashLen = 3.0f, dashGap = 4.0f;
        for (float x = bounds.getX(); x < bounds.getRight(); x += dashLen + dashGap) {
            const float w = juce::jmin(dashLen, bounds.getRight() - x);
            g.fillRect(x, yPlus0,  w, 1.0f);
            g.fillRect(x, yMinus0, w, 1.0f);
        }

        // Tiny "0 dB" label hugging the right edge, sitting just above
        // (positive) or below (negative) the rail so the rail itself
        // stays an uninterrupted dashed line.
        g.setColour(Theme::scopeLabelMid);
        g.setFont(Theme::mono(8.5f, juce::Font::bold));
        constexpr float labelW = 28.0f;
        constexpr float labelH = 10.0f;
        const float labelX = bounds.getRight() - labelW - 3.0f;
        g.drawText("0 dB",
                   juce::Rectangle<float>(labelX, yPlus0 - labelH - 1.0f, labelW, labelH),
                   juce::Justification::centredRight);
        g.drawText("0 dB",
                   juce::Rectangle<float>(labelX, yMinus0 + 1.0f, labelW, labelH),
                   juce::Justification::centredRight);
    }
}

void OscilloscopeComponent::drawOverlays(juce::Graphics& g, float headroomDb) const {
    auto inner = getLocalBounds();

    // ---- Top-left: WIN / HEAD readouts (two lines) --------------------
    {
        g.setColour(Theme::scopeLabelMid);
        g.setFont(Theme::mono(9.5f));
        const float ms = (activeSamples > 0)
            ? static_cast<float>(activeSamples) * 1000.0f
              / static_cast<float>(juce::jmax(1, (int)processor.getSampleRate()))
            : 0.0f;
        const auto winText  = juce::String("WIN  ") + (ms < 10.0f ? juce::String(ms, 1) : juce::String(ms, 0)) + " ms";
        // Design source uses '±'; we keep ASCII so Ableton's text renderer
        // doesn't mojibake it (see commit 6a4d807).
        const auto headText = juce::String("HEAD +/-") + juce::String(headroomDb, 1) + " dB";
        g.drawText(winText,  inner.reduced(10, 8).removeFromTop(12), juce::Justification::topLeft);
        g.drawText(headText, inner.reduced(10, 8).withTop(inner.getY() + 22).removeFromTop(12),
                   juce::Justification::topLeft);
    }

    // ---- Top-right: PRE / POST / CLIPPED legend -----------------------
    {
        g.setFont(Theme::mono(9.5f));
        auto top = inner.reduced(10, 8).removeFromTop(12);
        // Right-to-left layout for legend.
        auto cur = top;
        auto drawSwatchAndText = [&](juce::Colour swatch, const juce::String& text) {
            const int textW = Theme::mono(9.5f).getStringWidth(text) + 4;
            const int swW = 10;
            auto seg = cur.removeFromRight(textW + swW + 8);
            g.setColour(swatch);
            g.fillRect(seg.getX(), seg.getCentreY() - 1, swW, 2);
            g.setColour(Theme::scopeLabelMid);
            g.drawText(text, seg.withTrimmedLeft(swW + 2), juce::Justification::centredLeft);
        };
        // Drawn right-to-left so the order in the rendered output is
        // PRE | POST | CLIPPED reading left-to-right.
        drawSwatchAndText(Theme::overload,  "CLIPPED");
        drawSwatchAndText(Theme::scopePost, "POST");
        drawSwatchAndText(Theme::scopePre.withAlpha(0.7f), "PRE");
    }

    // ---- "Now" indicator at the right edge ----------------------------
    g.setColour(Theme::scopeNowLine);
    g.fillRect((float)inner.getRight() - 1.0f, (float)inner.getY(), 1.0f, (float)inner.getHeight());

    // ---- Bottom: time-axis labels -------------------------------------
    {
        g.setColour(Theme::scopeLabelDim);
        g.setFont(Theme::mono(9.0f));
        const float ms = (activeSamples > 0)
            ? static_cast<float>(activeSamples) * 1000.0f
              / static_cast<float>(juce::jmax(1, (int)processor.getSampleRate()))
            : 0.0f;
        const auto leftLabel = juce::String("-") + (ms < 10.0f ? juce::String(ms, 1) : juce::String(ms, 0)) + " ms";
        auto bottom = inner.reduced(10, 6).removeFromBottom(12);
        g.drawText(leftLabel, bottom, juce::Justification::bottomLeft);
        g.drawText("now",     bottom, juce::Justification::bottomRight);
    }
}

void OscilloscopeComponent::drawZoomedIn(juce::Graphics& g,
                                          juce::Rectangle<float> bounds,
                                          float midY,
                                          float ampScale) const {
    auto sampleToY = [&](float s) {
        return juce::jlimit(bounds.getY(), bounds.getBottom(), midY - s * ampScale);
    };

    const float step = bounds.getWidth() / static_cast<float>(juce::jmax(1, activeSamples - 1));

    // ---- Diff polygon: closed shape between pre and post traces -------
    // Trace pre forward, then post in reverse, close the path. The fill
    // covers exactly the area "shaved" by the clipper.
    {
        juce::Path diff;
        diff.startNewSubPath(bounds.getX(), sampleToY(displayPre[0]));
        for (int i = 1; i < activeSamples; ++i)
            diff.lineTo(bounds.getX() + i * step, sampleToY(displayPre[i]));
        for (int i = activeSamples - 1; i >= 0; --i)
            diff.lineTo(bounds.getX() + i * step, sampleToY(displayPost[i]));
        diff.closeSubPath();
        g.setColour(Theme::scopeDiff);
        g.fillPath(diff);
    }

    // Pre-clip ghost trace. Theme::scopePre is tuned at low alpha (0.30)
    // for drawZoomedOut's filled overlay; that's too faint for a 1 px
    // stroke at this zoom level, so we bump alpha at the call site to
    // keep the line readable. Same colour, just more opaque for the
    // line-rendering case.
    juce::Path prePath;
    prePath.startNewSubPath(bounds.getX(), sampleToY(displayPre[0]));
    for (int i = 1; i < activeSamples; ++i)
        prePath.lineTo(bounds.getX() + i * step, sampleToY(displayPre[i]));
    g.setColour(Theme::scopePre.withAlpha(0.70f));
    g.strokePath(prePath, juce::PathStrokeType(1.0f));

    // Post-clip foreground trace.
    juce::Path postPath;
    postPath.startNewSubPath(bounds.getX(), sampleToY(displayPost[0]));
    for (int i = 1; i < activeSamples; ++i)
        postPath.lineTo(bounds.getX() + i * step, sampleToY(displayPost[i]));
    g.setColour(Theme::scopePost);
    g.strokePath(postPath, juce::PathStrokeType(1.4f));
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

    // Render order is intentional and matters:
    //
    //   1) POST  -- the actual output, dominant cream filled bars.
    //   2) CLIPPED -- red bars filling the headroom gap between POST top
    //                 and PRE top (and the mirrored bottom).
    //   3) PRE  -- a translucent grey filled bar from preLo to preHi,
    //              drawn LAST so it stays visible above POST/CLIPPED.
    //
    // History of this layer:
    //
    //   * Pre-v0.5.8 PRE was drawn FIRST as a filled bar, occluded by
    //     POST and CLIPPED in their respective regions -- never visible.
    //
    //   * v0.5.8 swapped to a thin contour drawn LAST -- visible on
    //     sustained material but looked terrible on transient material
    //     because the per-column max envelope alternates between zero
    //     (silence) and peak (transient), producing visual-noise spikes.
    //
    //   * v0.5.10 (this version) reverts to a filled bar but drawn LAST
    //     as a translucent (alpha 0.30) overlay. In the headroom region
    //     where there's no POST or CLIPPED behind it, the grey wash is
    //     visible solo, showing where pre extended. In POST/CLIPPED
    //     regions, the wash gently tints the layer below without
    //     hiding it. Transient material reads as broad soft grey
    //     regions (the height of each transient's pre envelope) rather
    //     than spikes. Continuous material reads as a soft halo
    //     surrounding the cream wave.

    // ---- POST: filled cream bar per column -------------------------
    g.setColour(Theme::scopePost);
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

    // ---- CLIPPED: red highlights where pre exceeded post ----------
    g.setColour(Theme::scopeDiff);
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

    // ---- PRE: translucent grey overlay fill (drawn LAST) ------------
    // Vertical line per column from preLo to preHi at the theme's low
    // alpha. This is the SAME geometry that pre-v0.5.8 used for PRE,
    // but pre-v0.5.8 drew this layer FIRST (so POST and CLIPPED
    // overdrew it). Drawing it LAST means the wash sits on top of
    // the other layers and stays visible -- subtle enough not to
    // dominate, distinct enough to read as "where pre would have been
    // if the clipper hadn't shaved it".
    g.setColour(Theme::scopePre);
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
}
