#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Centralised colour + font definitions for Variant F · Stages.
//
// All values mirror the Claude Design source at:
//   https://api.anthropic.com/v1/design/h/LYRs9XYR5GvBoZL062Ewtg
//   (open_file=ClipToZero+VST.html, see vst-stages.jsx)
//
// JUCE's juce::Colour takes 0xAARRGGBB; the helpers below let us paste
// CSS-style hex codes directly so the source code matches the design file.
namespace Theme {

    namespace Detail {
        constexpr juce::uint32 fromRGB(juce::uint32 rgb) noexcept {
            return 0xff000000u | rgb;
        }
        constexpr juce::uint32 fromRGBA(juce::uint32 rgb, juce::uint8 alpha) noexcept {
            return (static_cast<juce::uint32>(alpha) << 24) | (rgb & 0x00ffffffu);
        }
    }

    // ---- Backgrounds & borders ------------------------------------------
    inline const juce::Colour bg            { Detail::fromRGB(0x0c0d0c) }; // window background
    inline const juce::Colour bgDeeper      { Detail::fromRGB(0x0a0a0a) }; // recessed wells
    inline const juce::Colour border        { Detail::fromRGB(0x1d1f1c) }; // 1px frame between sections
    inline const juce::Colour borderStrong  { Detail::fromRGB(0x2a2d2a) }; // button outlines
    inline const juce::Colour borderVeryDim { Detail::fromRGB(0x3a3d36) }; // inactive step ring

    // ---- Text shades, dimmest -> brightest ------------------------------
    inline const juce::Colour textVeryDim { Detail::fromRGB(0x5d6457) }; // tertiary captions
    inline const juce::Colour textDim     { Detail::fromRGB(0x7d8674) }; // muted hints
    inline const juce::Colour textBody    { Detail::fromRGB(0x9ea692) }; // labels
    inline const juce::Colour textBright  { Detail::fromRGB(0xe6f0c2) }; // numeric readouts

    // ---- Brand / accent / state colours ---------------------------------
    inline const juce::Colour accent      { Detail::fromRGB(0xa8d860) }; // lime — primary action
    inline const juce::Colour overload    { Detail::fromRGB(0xff5a50) }; // peak > 0 dBFS
    inline const juce::Colour overloadDim { Detail::fromRGB(0xff7068) }; // numeric at clip
    inline const juce::Colour bypassFill  { Detail::fromRGB(0xffaa50) }; // bypass-active

    // ---- Scope-specific palette -----------------------------------------
    inline const juce::Colour scopePre       { juce::Colour::fromFloatRGBA(180.0f/255, 210.0f/255, 150.0f/255, 0.4f) };
    inline const juce::Colour scopePost      { Detail::fromRGB(0xe6f0c2) };
    inline const juce::Colour scopeDiff      { juce::Colour::fromFloatRGBA(255.0f/255,  90.0f/255,  80.0f/255, 0.55f) };
    // 0 dBFS rail. Lifted from 0.22 -> 0.50 alpha in v0.5.5 because the
    // line was vanishing into the cream-coloured waveform fills whenever
    // the scope was busy with clipped material -- the user couldn't see
    // where 0 dB was when it mattered most.
    inline const juce::Colour scopeRail      { juce::Colour::fromFloatRGBA(230.0f/255, 240.0f/255, 194.0f/255, 0.50f) };
    inline const juce::Colour scopeNowLine   { juce::Colour::fromFloatRGBA(168.0f/255, 216.0f/255,  96.0f/255, 0.30f) };
    inline const juce::Colour scopeGrid      { juce::Colour::fromFloatRGBA(255.0f/255, 255.0f/255, 255.0f/255, 0.04f) };
    inline const juce::Colour scopeLabelDim  { juce::Colour::fromFloatRGBA(230.0f/255, 240.0f/255, 194.0f/255, 0.40f) };
    inline const juce::Colour scopeLabelMid  { juce::Colour::fromFloatRGBA(230.0f/255, 240.0f/255, 194.0f/255, 0.60f) };

    // ---- Fonts ----------------------------------------------------------
    // JetBrains Mono is bundled on macOS/Linux dev installs of JUCE 8 only
    // when explicitly added — fall back gracefully if the system can't find
    // it, but try the canonical names first.
    inline juce::Font mono(float height, juce::Font::FontStyleFlags style = juce::Font::plain) {
        auto opts = juce::FontOptions(height)
                       .withName("JetBrains Mono")
                       .withFallbacks({ "Menlo", "Monaco", "Courier New" });
        if (style & juce::Font::bold)   opts = opts.withStyle("Bold");
        if (style & juce::Font::italic) opts = opts.withStyle("Italic");
        return juce::Font(opts);
    }

    inline juce::Font sans(float height, juce::Font::FontStyleFlags style = juce::Font::plain) {
        auto opts = juce::FontOptions(height)
                       .withName("Inter")
                       .withFallbacks({ "Helvetica Neue", "Helvetica", "Arial" });
        if (style & juce::Font::bold)   opts = opts.withStyle("Bold");
        if (style & juce::Font::italic) opts = opts.withStyle("Italic");
        return juce::Font(opts);
    }

} // namespace Theme
