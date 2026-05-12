#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

class Clipper {
public:
    // Available clipping characters. Order matters: it's the index used
    // by the AudioParameterChoice in Parameters.h.
    enum class Type {
        Hard = 0,  // brick wall: clamp(x, +/-ceiling). Bright, gritty.
        Soft = 1,  // tanh: symmetric S-curve, asymptotic. Odd harmonics.
        Poly = 2,  // cubic soft-knee: tangent to the rail (slope 0 at top).
        Tube = 3   // asymmetric tanh: positive harder than negative.
                   //                  Adds even harmonics ("warmth").
    };

    void  setType(Type t) noexcept       { type = t; }
    void  setCeiling(float linear) noexcept { ceiling = juce::jmax(0.001f, linear); }

    void  process(juce::AudioBuffer<float>& buffer) noexcept;

    int   getClippedSampleCount() const noexcept { return clippedCount; }

private:
    Type  type        = Type::Hard;
    float ceiling     = 1.0f; // 0 dBFS
    int   clippedCount = 0;
};
