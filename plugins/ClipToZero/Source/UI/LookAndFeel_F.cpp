#include "LookAndFeel_F.h"
#include <cmath>

LookAndFeel_F::LookAndFeel_F() {
    // Apply a few global overrides so JUCE's default popups, scrollbars, and
    // tooltips honour the F palette without us having to reach into them.
    setColour(juce::ResizableWindow::backgroundColourId, Theme::bg);
    setColour(juce::PopupMenu::backgroundColourId,        Theme::bg);
    setColour(juce::PopupMenu::textColourId,              Theme::textBody);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::accent.withAlpha(0.20f));
    setColour(juce::PopupMenu::highlightedTextColourId,   Theme::textBright);
    setColour(juce::Label::textColourId,                  Theme::textBody);
    setColour(juce::Slider::textBoxOutlineColourId,       juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId,    juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxTextColourId,          Theme::textBright);
    setColour(juce::TextButton::buttonColourId,           juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonOnColourId,         Theme::accent);
    setColour(juce::TextButton::textColourOffId,          Theme::textBody);
    setColour(juce::TextButton::textColourOnId,           Theme::bg);
    setColour(juce::ComboBox::backgroundColourId,         Theme::bgDeeper);
    setColour(juce::ComboBox::outlineColourId,            Theme::borderStrong);
    setColour(juce::ComboBox::textColourId,               Theme::textBody);
    setColour(juce::ComboBox::buttonColourId,             Theme::accent);
    setColour(juce::ComboBox::arrowColourId,              Theme::accent);
}

void LookAndFeel_F::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                     float sliderPosProportional,
                                     float rotaryStartAngle, float rotaryEndAngle,
                                     juce::Slider& slider) {
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 4.0f;
    const float radius = diameter * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();

    const float arcThickness = juce::jmax(2.0f, diameter * 0.08f);
    const float arcRadius = radius - arcThickness * 0.5f;

    const float toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Detect bipolar: range straddles zero. The value-arc then starts at the
    // 0 position rather than the min end of the sweep.
    const auto minVal = static_cast<float>(slider.getMinimum());
    const auto maxVal = static_cast<float>(slider.getMaximum());
    const bool bipolar = (minVal < 0.0f && maxVal > 0.0f);
    const float centerProportional = bipolar
        ? juce::jlimit(0.0f, 1.0f, (0.0f - minVal) / (maxVal - minVal))
        : 0.0f;
    const float fromAngle = rotaryStartAngle + centerProportional * (rotaryEndAngle - rotaryStartAngle);

    // ---- Outer track (full 270° sweep) ----
    {
        juce::Path track;
        track.addCentredArc(cx, cy, arcRadius, arcRadius,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(Theme::border);
        g.strokePath(track, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::butt));
    }

    // ---- Value arc ----
    {
        juce::Path arc;
        const float a0 = juce::jmin(fromAngle, toAngle);
        const float a1 = juce::jmax(fromAngle, toAngle);
        if (a1 > a0 + 0.001f) {
            arc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, a0, a1, true);
            g.setColour(Theme::accent);
            g.strokePath(arc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }
    }

    // ---- Knob face: radial gradient + indicator line ----
    const float faceRadius = radius - arcThickness - 2.0f;
    if (faceRadius > 1.0f) {
        juce::ColourGradient grad(juce::Colour(0xff2a2d2a), cx - faceRadius * 0.3f, cy - faceRadius * 0.4f,
                                  juce::Colour(0xff0e0f0e), cx + faceRadius * 0.6f, cy + faceRadius * 0.6f,
                                  true);
        g.setGradientFill(grad);
        g.fillEllipse(cx - faceRadius, cy - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);

        // Indicator: a short tick from the centre toward the rim, rotated
        // to match the knob position (-π/2 because JUCE 0 angle points up).
        juce::Path indicator;
        const float tickLen = faceRadius * 0.55f;
        const float tickWidth = juce::jmax(1.5f, faceRadius * 0.08f);
        indicator.addRectangle(-tickWidth * 0.5f, -faceRadius + 2.0f,
                               tickWidth, tickLen);
        g.setColour(Theme::accent);
        g.fillPath(indicator, juce::AffineTransform::rotation(toAngle).translated(cx, cy));
    }
}

void LookAndFeel_F::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                     float sliderPos, float minSliderPos, float maxSliderPos,
                                     juce::Slider::SliderStyle style, juce::Slider& slider) {
    juce::ignoreUnused(minSliderPos, maxSliderPos);

    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();

    // We only intend to use horizontal linear sliders for the F design;
    // fall back to JUCE's default for anything else so we don't break
    // unexpected callers.
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                                         sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const float trackHeight = 2.0f;
    const float midY = bounds.getCentreY();
    const float trackY = midY - trackHeight * 0.5f;

    // Track (full).
    g.setColour(Theme::border);
    g.fillRoundedRectangle(bounds.getX(), trackY, bounds.getWidth(), trackHeight, 1.0f);

    // Fill (start to handle).
    const float fillEnd = juce::jlimit(bounds.getX(), bounds.getRight(), sliderPos);
    if (fillEnd > bounds.getX() + 0.5f) {
        g.setColour(Theme::accent);
        g.fillRoundedRectangle(bounds.getX(), trackY, fillEnd - bounds.getX(), trackHeight, 1.0f);
    }

    // Handle: dot with dark inner ring + lime outer ring.
    const float handleR  = 4.0f;
    const float handleX  = juce::jlimit(bounds.getX() + handleR, bounds.getRight() - handleR, sliderPos);
    g.setColour(Theme::accent);
    g.fillEllipse(handleX - handleR - 1.0f, midY - handleR - 1.0f,
                  (handleR + 1.0f) * 2.0f, (handleR + 1.0f) * 2.0f);
    g.setColour(Theme::bg);
    g.fillEllipse(handleX - handleR, midY - handleR, handleR * 2.0f, handleR * 2.0f);
    g.setColour(Theme::textBright);
    g.fillEllipse(handleX - handleR + 1.0f, midY - handleR + 1.0f,
                  (handleR - 1.0f) * 2.0f, (handleR - 1.0f) * 2.0f);
}

void LookAndFeel_F::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                         const juce::Colour& backgroundColour,
                                         bool isHighlighted, bool isDown) {
    juce::ignoreUnused(backgroundColour);

    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool isOn = button.getToggleState();
    const bool primary = button.getProperties().getWithDefault("variant", "default").toString() == "primary";
    const bool warning = button.getProperties().getWithDefault("variant", "default").toString() == "warning";
    const float corner = 2.0f;

    juce::Colour fillColour, borderColour;
    if (warning && isOn) {
        fillColour   = Theme::bypassFill;
        borderColour = Theme::bypassFill;
    } else if (primary) {
        // Auto-Gain in idle/measuring state. When measuring we want the
        // hollow look; while idle we want the lime fill.
        const bool measuring = button.getProperties().getWithDefault("measuring", false);
        if (measuring) {
            fillColour   = juce::Colours::transparentBlack;
            borderColour = Theme::accent;
        } else {
            fillColour   = Theme::accent;
            borderColour = Theme::accent;
        }
    } else if (isOn) {
        fillColour   = Theme::accent.withAlpha(0.18f);
        borderColour = Theme::accent;
    } else {
        fillColour   = juce::Colours::transparentBlack;
        borderColour = Theme::borderStrong;
    }

    if (isDown)        fillColour = fillColour.darker(0.10f);
    else if (isHighlighted) fillColour = fillColour.brighter(0.05f);

    g.setColour(fillColour);
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(borderColour);
    g.drawRoundedRectangle(bounds, corner, 1.0f);
}

juce::Font LookAndFeel_F::getTextButtonFont(juce::TextButton& button, int buttonHeight) {
    juce::ignoreUnused(buttonHeight);
    const bool primary = button.getProperties().getWithDefault("variant", "default").toString() == "primary";
    return Theme::mono(primary ? 11.0f : 9.5f, juce::Font::bold);
}

void LookAndFeel_F::drawLabel(juce::Graphics& g, juce::Label& label) {
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (! label.isBeingEdited()) {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const auto font   = label.getFont();
        const auto colour = label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha);
        g.setColour(colour);
        g.setFont(font);

        auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, (int)((float)textArea.getHeight() / font.getHeight())),
                         label.getMinimumHorizontalScale());
    }

    if (label.isEnabled()) {
        g.setColour(label.findColour(juce::Label::outlineColourId));
        g.drawRect(label.getLocalBounds());
    }
}

void LookAndFeel_F::drawComboBox(juce::Graphics& g, int width, int height,
                                 bool /*isButtonDown*/, int /*buttonX*/, int /*buttonY*/,
                                 int /*buttonW*/, int /*buttonH*/, juce::ComboBox& box) {
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
    g.setColour(Theme::bgDeeper);
    g.fillRoundedRectangle(bounds, 2.0f);
    g.setColour(box.hasKeyboardFocus(false) ? Theme::accent : Theme::borderStrong);
    g.drawRoundedRectangle(bounds, 2.0f, 1.0f);

    // Tiny chevron on the right.
    const float arrowW = 6.0f;
    const float arrowH = 4.0f;
    const float ax = bounds.getRight() - 12.0f;
    const float ay = bounds.getCentreY() - 2.0f;
    juce::Path p;
    p.startNewSubPath(ax, ay);
    p.lineTo(ax + arrowW, ay);
    p.lineTo(ax + arrowW * 0.5f, ay + arrowH);
    p.closeSubPath();
    g.setColour(Theme::accent);
    g.fillPath(p);
}

juce::Font LookAndFeel_F::getComboBoxFont(juce::ComboBox&) {
    return Theme::mono(10.0f);
}

void LookAndFeel_F::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(8, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, Theme::textBright);
}

void LookAndFeel_F::drawCornerResizer(juce::Graphics& g, int w, int h,
                                      bool isMouseOver, bool isMouseDragging) {
    // Three short diagonal strokes in the bottom-right corner. Bright lime
    // when hovered/dragging, dim border colour at rest so it doesn't fight
    // for attention with the actual content.
    const auto strokeColour = (isMouseOver || isMouseDragging)
                                ? Theme::accent
                                : Theme::textVeryDim;
    g.setColour(strokeColour);

    const float spacing = 3.5f;
    for (int i = 0; i < 3; ++i) {
        const float offset = (i + 1) * spacing;
        const float x1 = static_cast<float>(w) - offset;
        const float y2 = static_cast<float>(h) - offset;
        g.drawLine(x1, static_cast<float>(h) - 2.0f,
                   static_cast<float>(w) - 2.0f, y2,
                   1.0f);
    }
}
