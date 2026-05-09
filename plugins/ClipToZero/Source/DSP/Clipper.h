#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

class Clipper {
public:
    enum class Type { Hard = 0, Soft = 1 };

    void  setType(Type t) noexcept       { type = t; }
    void  setCeiling(float linear) noexcept { ceiling = juce::jmax(0.001f, linear); }

    void  process(juce::AudioBuffer<float>& buffer) noexcept;

    int   getClippedSampleCount() const noexcept { return clippedCount; }

private:
    Type  type        = Type::Hard;
    float ceiling     = 1.0f; // 0 dBFS
    int   clippedCount = 0;
};
