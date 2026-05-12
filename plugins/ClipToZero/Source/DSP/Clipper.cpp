#include "Clipper.h"
#include <cmath>

namespace {
    // Cubic soft-knee, normalised so y(1.5) = 1 with y'(1.5) = 0.
    // Solving y = a*x - b*x^3 with both constraints gives
    //   a = 1, b = 1/6.75 (= 0.1481...).
    // Below |x| = 1.5 the curve is smooth and approaches the ceiling
    // tangentially; above, we hard-clip to keep the output bounded.
    inline float cubicSoftClip(float x) noexcept {
        constexpr float kneeEnd = 1.5f;
        constexpr float invSix75 = 1.0f / 6.75f;
        if (x >=  kneeEnd) return  1.0f;
        if (x <= -kneeEnd) return -1.0f;
        return x - invSix75 * x * x * x;
    }

    // Asymmetric tanh — positive half clamped harder than negative.
    // Different "strengths" on each side mean the waveform is no longer
    // mirror-symmetric, which generates even-order harmonics (2, 4, 6...)
    // perceived as "tube warmth."
    inline float tubeClip(float x) noexcept {
        if (x > 0.0f) return std::tanh(x * 1.5f) / 1.5f;  // gentler positive
        return std::tanh(x);                              // standard negative
    }
}

void Clipper::process(juce::AudioBuffer<float>& buffer) noexcept {
    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    clippedCount = 0;

    const float invC     = 1.0f / ceiling;
    const float softZone = ceiling * 0.7f;

    switch (type) {
        case Type::Hard: {
            for (int ch = 0; ch < numCh; ++ch) {
                float* x = buffer.getWritePointer(ch);
                for (int i = 0; i < n; ++i) {
                    const float in  = x[i];
                    const float out = juce::jlimit(-ceiling, ceiling, in);
                    if (out != in) ++clippedCount;
                    x[i] = out;
                }
            }
            break;
        }

        case Type::Soft: {
            for (int ch = 0; ch < numCh; ++ch) {
                float* x = buffer.getWritePointer(ch);
                for (int i = 0; i < n; ++i) {
                    const float in  = x[i];
                    const float out = ceiling * std::tanh(in * invC);
                    if (std::abs(in) > softZone) ++clippedCount;
                    x[i] = out;
                }
            }
            break;
        }

        case Type::Poly: {
            for (int ch = 0; ch < numCh; ++ch) {
                float* x = buffer.getWritePointer(ch);
                for (int i = 0; i < n; ++i) {
                    const float in  = x[i];
                    const float out = ceiling * cubicSoftClip(in * invC);
                    if (std::abs(in) > softZone) ++clippedCount;
                    x[i] = out;
                }
            }
            break;
        }

        case Type::Tube: {
            for (int ch = 0; ch < numCh; ++ch) {
                float* x = buffer.getWritePointer(ch);
                for (int i = 0; i < n; ++i) {
                    const float in  = x[i];
                    const float out = ceiling * tubeClip(in * invC);
                    if (std::abs(in) > softZone) ++clippedCount;
                    x[i] = out;
                }
            }
            break;
        }
    }
}
