#include "PluginProcessor.h"
#include "PluginEditor.h"

ClipToZeroProcessor::ClipToZeroProcessor()
    : AudioProcessor(BusesProperties()
                       .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", Param::createLayout())
{
    targetPeakParam  = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter(Param::targetPeak));
    inputGainParam   = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter(Param::inputGain));
    driveParam       = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter(Param::drive));
    clipTypeParam    = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(Param::clipType));
    outputTrimParam  = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter(Param::outputTrim));
    bypassParam      = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter(Param::bypass));
    preClipHpfParam  = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter(Param::preClipHpf));
    jassert(targetPeakParam && inputGainParam && driveParam && clipTypeParam
            && outputTrimParam && bypassParam && preClipHpfParam);
}

void ClipToZeroProcessor::updateHpfIfChanged(double sampleRate) {
    const float hz = preClipHpfParam->get();
    if (std::abs(hz - currentHpfHz) < 0.01f) return;

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, hz);
    preClipHpfL.coefficients = coeffs;
    preClipHpfR.coefficients = coeffs;
    currentHpfHz = hz;
}

void ClipToZeroProcessor::prepareToPlay(double sr, int spb) {
    inputMeter.prepare(sr, 2);
    outputMeter.prepare(sr, 2);
    autoGain.prepare(sr);
    lufs.prepare(sr, 2);
    clipper.setCeiling(1.0f);

    preClipBuffer.setSize(2, spb, false, true, true);
    scopeFifo.reset();

    // Force a redesign on next process call (sample rate may have changed).
    currentHpfHz = -1.0f;
    preClipHpfL.reset();
    preClipHpfR.reset();
}

bool ClipToZeroProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    if (l.getMainInputChannelSet() != l.getMainOutputChannelSet()) return false;
    const auto& set = l.getMainInputChannelSet();
    return set == juce::AudioChannelSet::mono()
        || set == juce::AudioChannelSet::stereo();
}

void ClipToZeroProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    // Pre-gain metering always runs — even on bypass — so you can see the
    // raw incoming signal regardless of plugin state.
    inputMeter.process(buffer);

    if (!bypassParam->get()) {
        // Auto-gain analyzer captures the raw peak.
        autoGain.process(buffer);

        // 1. Stage the signal: input gain to bring peak to target.
        const float inputDb = inputGainParam->get();
        buffer.applyGain(juce::Decibels::decibelsToGain(inputDb));

        // 1.5. Pre-clipper high-pass. Below ~20 Hz the filter is effectively
        //      a no-op, so we skip the per-sample loop entirely to save CPU
        //      when the user hasn't enabled it. Bypass threshold is 20.5 Hz
        //      to avoid jitter right at the minimum.
        updateHpfIfChanged(getSampleRate());
        if (currentHpfHz > 20.5f) {
            const int numChForHpf = juce::jmin(buffer.getNumChannels(), 2);
            for (int ch = 0; ch < numChForHpf; ++ch) {
                auto& filter = (ch == 0 ? preClipHpfL : preClipHpfR);
                float* x = buffer.getWritePointer(ch);
                for (int i = 0, n2 = buffer.getNumSamples(); i < n2; ++i)
                    x[i] = filter.processSample(x[i]);
            }
        }

        // 1.6. Apply drive gain into the clipper.
        const float driveDb = driveParam->get();
        buffer.applyGain(juce::Decibels::decibelsToGain(driveDb));

        // 2. Snapshot pre-clip signal for the scope (no allocation:
        //    preallocated buffer with avoidReallocating=true).
        const int numCh = juce::jmin(buffer.getNumChannels(), 2);
        const int n     = buffer.getNumSamples();
        preClipBuffer.setSize(numCh, n, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            preClipBuffer.copyFrom(ch, 0, buffer, ch, 0, n);

        // 3. Clip at 0 dBFS ceiling.
        clipper.setType(static_cast<Clipper::Type>(clipTypeParam->getIndex()));
        clipper.process(buffer);

        // 4. Push to scope.
        writeToScope(preClipBuffer, buffer);

        // 5. Output trim.
        buffer.applyGain(juce::Decibels::decibelsToGain(outputTrimParam->get()));
    }

    // Output metering and LUFS always run on the post-chain (or post-bypass)
    // signal — that way the readouts reflect what the host actually receives.
    outputMeter.process(buffer);
    lufs.process(buffer);
}

void ClipToZeroProcessor::writeToScope(const juce::AudioBuffer<float>& pre,
                                       const juce::AudioBuffer<float>& post) noexcept {
    const int n     = pre.getNumSamples();
    const int numCh = pre.getNumChannels();

    int start1, size1, start2, size2;
    scopeFifo.prepareToWrite(n, start1, size1, start2, size2);

    auto mixDown = [&](const juce::AudioBuffer<float>& src,
                       std::array<float, scopeSize>& dst,
                       int sOff, int sLen, int srcOff) {
        for (int i = 0; i < sLen; ++i) {
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) s += src.getSample(ch, srcOff + i);
            dst[sOff + i] = (numCh > 0) ? (s / static_cast<float>(numCh)) : 0.0f;
        }
    };

    mixDown(pre,  scopePre,  start1, size1, 0);
    mixDown(post, scopePost, start1, size1, 0);
    mixDown(pre,  scopePre,  start2, size2, size1);
    mixDown(post, scopePost, start2, size2, size1);

    scopeFifo.finishedWrite(size1 + size2);
}

juce::AudioProcessorEditor* ClipToZeroProcessor::createEditor() {
    return new ClipToZeroEditor(*this);
}

void ClipToZeroProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void ClipToZeroProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ClipToZeroProcessor();
}
