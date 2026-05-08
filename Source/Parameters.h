#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Param {
    inline constexpr auto targetPeak = "targetPeak";
    inline constexpr auto inputGain  = "inputGain";
    inline constexpr auto drive      = "drive";
    inline constexpr auto clipType   = "clipType";
    inline constexpr auto outputTrim = "outputTrim";
    inline constexpr auto bypass     = "bypass";

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

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{clipType, 1}, "Clip Type",
            juce::StringArray{"Hard", "Soft"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{outputTrim, 1}, "Output Trim",
            juce::NormalisableRange<float>{-12.0f, 12.0f, 0.01f}, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{bypass, 1}, "Bypass", false));

        return { params.begin(), params.end() };
    }
}
