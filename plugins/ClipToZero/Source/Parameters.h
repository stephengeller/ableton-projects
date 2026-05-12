#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Param {
    inline constexpr auto targetPeak = "targetPeak";
    inline constexpr auto inputGain  = "inputGain";
    inline constexpr auto drive      = "drive";
    inline constexpr auto clipType   = "clipType";
    inline constexpr auto outputTrim = "outputTrim";
    inline constexpr auto bypass       = "bypass";
    inline constexpr auto gainMatch    = "gainMatch";
    inline constexpr auto preClipHpf   = "preClipHpfHz";
    inline constexpr auto scopeLen     = "scopeLengthMs";
    inline constexpr auto vertHeadroom = "vertHeadroomDb";

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        // Target peak: where the input gain should bring the signal to.
        // Auto-Gain computes `gain = target - measuredPeak`. Range stops at 0
        // because the clipper ceiling is 0 dBFS — targeting above that just
        // means clipping before you even enter the clipper stage.
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{targetPeak, 1}, "Target Peak",
            juce::NormalisableRange<float>{-12.0f, 0.0f, 0.1f}, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dBFS")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{inputGain, 1}, "Input Gain",
            juce::NormalisableRange<float>{-24.0f, 24.0f, 0.01f}, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        // Drive: post-staging gain into the clipper. Output is still clamped
        // to 0 dBFS by the clipper ceiling, so increasing Drive squashes
        // peaks more aggressively without raising the channel meter.
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{drive, 1}, "Drive",
            juce::NormalisableRange<float>{0.0f, 24.0f, 0.01f}, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        // Clip Type ordering must match Clipper::Type enum in DSP/Clipper.h.
        // Hard / Soft preserved as indices 0/1 so any saved project state
        // from earlier versions round-trips unchanged.
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{clipType, 1}, "Clip Type",
            juce::StringArray{"Hard", "Soft", "Poly", "Tube"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{outputTrim, 1}, "Output Trim",
            juce::NormalisableRange<float>{-12.0f, 12.0f, 0.01f}, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{bypass, 1}, "Bypass", false));

        // Gain-matched A/B bypass: when ON (default), the bypassed signal
        // is multiplied by the running output-vs-input RMS difference so
        // toggling bypass is a fair loudness comparison instead of a
        // 'louder = better' cognitive trap. OFF gives traditional raw bypass.
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{gainMatch, 1}, "Gain Match", true));

        // Pre-clipper high-pass filter. Removes sub-bass that would otherwise
        // eat clipping headroom and produce ugly low-frequency artefacts.
        // At the minimum (20 Hz) the filter is bypassed entirely (audible
        // content starts above 20 Hz anyway, so anything below is overhead).
        // Log skew so the typical sweet spot (30-150 Hz) is in the middle
        // half of the slider's travel.
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{preClipHpf, 1}, "Pre-Clip HPF",
            juce::NormalisableRange<float>{20.0f, 500.0f, 1.0f, 0.30f}, 20.0f,
            juce::AudioParameterFloatAttributes().withLabel("Hz")));

        // Scope window length in milliseconds. Skewed so that the lower
        // ranges (where most clipping detail lives) get proportionally more
        // slider real-estate than the long-window end of the range.
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{scopeLen, 1}, "Scope Length",
            juce::NormalisableRange<float>{1.0f, 10000.0f, 0.1f, 0.22f}, 10000.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")));

        // Vertical headroom above 0 dBFS visible on the scope. Higher = more
        // room to see clipping overshoots (when the input/drive is hot);
        // lower = signal fills more of the scope (better for low-clip
        // material). 6 dB is a sensible default — top quarter of the scope
        // is "above the rails".
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{vertHeadroom, 1}, "Vert. Headroom",
            juce::NormalisableRange<float>{0.0f, 24.0f, 0.1f}, 6.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        return { params.begin(), params.end() };
    }
}
