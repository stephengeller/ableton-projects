#include "LufsBox.h"

LufsBox::LufsBox(juce::String l, juce::String s)
    : letter(std::move(l)), subtitle(std::move(s)) {}

void LufsBox::setValue(float lufs) {
    if (std::abs(value - lufs) < 0.05f) return;  // noise gate to avoid useless repaints
    value = lufs;
    repaint();
}

void LufsBox::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(Theme::border);
    g.drawRoundedRectangle(bounds, 2.0f, 1.0f);

    auto inner = getLocalBounds().reduced(2);

    // Letter (top).
    g.setColour(Theme::textDim);
    g.setFont(Theme::mono(8.0f));
    auto top = inner.removeFromTop(12);
    g.drawText(letter, top, juce::Justification::centred);

    // Subtitle (bottom).
    auto bottom = inner.removeFromBottom(11);
    g.setColour(Theme::textVeryDim);
    g.setFont(Theme::mono(7.5f));
    g.drawText(subtitle, bottom, juce::Justification::centred);

    // Value (centred in the remaining space).
    g.setColour(Theme::textBright);
    g.setFont(Theme::mono(14.0f));
    const auto valueText = (value <= -69.0f) ? juce::String("-inf")
                                             : juce::String(value, 1);
    g.drawText(valueText, inner, juce::Justification::centred);
}
