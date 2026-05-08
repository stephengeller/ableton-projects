#include "Clipper.h"
#include <cmath>

void Clipper::process(juce::AudioBuffer<float>& buffer) noexcept {
    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    clippedCount = 0;

    if (type == Type::Hard) {
        for (int ch = 0; ch < numCh; ++ch) {
            float* x = buffer.getWritePointer(ch);
            for (int i = 0; i < n; ++i) {
                const float in  = x[i];
                const float out = juce::jlimit(-ceiling, ceiling, in);
                if (out != in) ++clippedCount;
                x[i] = out;
            }
        }
    } else {
        // Soft clip: ceiling * tanh(x / ceiling).
        // Linear for small inputs, asymptotic to ±ceiling. tanh is C-infinity
        // smooth, so no ringing/aliasing-spike at the threshold.
        const float invC = 1.0f / ceiling;
        const float softZone = ceiling * 0.7f; // call it "clipping" once we're in the bend

        for (int ch = 0; ch < numCh; ++ch) {
            float* x = buffer.getWritePointer(ch);
            for (int i = 0; i < n; ++i) {
                const float in = x[i];
                const float out = ceiling * std::tanh(in * invC);
                if (std::abs(in) > softZone) ++clippedCount;
                x[i] = out;
            }
        }
    }
}
