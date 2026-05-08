#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Param {
    inline constexpr auto inputGain  = "inputGain";
    inline constexpr auto clipType   = "clipType";
    inline constexpr auto outputTrim = "outputTrim";
    inline constexpr auto bypass     = "bypass";

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{inputGain, 1}, "Input Gain",
            juce::NormalisableRange<float>{-24.0f, 24.0f, 0.01f}, 0.0f,
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
