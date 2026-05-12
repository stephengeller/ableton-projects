#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// Custom JUCE LookAndFeel for the Variant F · Stages design.
//
// Renders:
//   - Rotary sliders as knob faces with an external value-arc (lime).
//     Bipolar sliders (range straddles 0) draw the arc starting from the
//     0 position; unipolar from the min end.
//   - Linear horizontal sliders as a thin track with a lime fill and
//     dot-style handle (used for the zoom sliders under the scope).
//   - TextButtons with two flavours selectable via a Component property:
//     "primary" = lime fill / dark text (Auto-Gain), default = outlined.
//   - ComboBoxes with a dark, monospace-styled popup.
class LookAndFeel_F : public juce::LookAndFeel_V4 {
public:
    LookAndFeel_F();
    ~LookAndFeel_F() override = default;

    // Rotary slider — F's signature knob.
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;

    // Linear slider — used for the two zoom sliders below the scope.
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    // Buttons — primary (filled) and default (outlined).
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    // Labels — default to the body text colour and Inter font.
    void drawLabel(juce::Graphics&, juce::Label&) override;

    // ComboBox — dark popup for clip type selection (if used).
    void drawComboBox(juce::Graphics&, int width, int height,
                      bool isButtonDown, int buttonX, int buttonY,
                      int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;

    // Resize-handle in the bottom-right corner, themed to the F palette.
    void drawCornerResizer(juce::Graphics&, int w, int h,
                           bool isMouseOver, bool isMouseDragging) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LookAndFeel_F)
};
