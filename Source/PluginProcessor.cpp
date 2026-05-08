#include "PluginProcessor.h"
#include "PluginEditor.h"

ClipToZeroProcessor::ClipToZeroProcessor()
    : AudioProcessor(BusesProperties()
                       .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", Param::createLayout())
{
    inputGainParam   = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter(Param::inputGain));
    clipTypeParam    = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(Param::clipType));
    outputTrimParam  = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter(Param::outputTrim));
    bypassParam      = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter(Param::bypass));
    jassert(inputGainParam && clipTypeParam && outputTrimParam && bypassParam);
}

void ClipToZeroProcessor::prepareToPlay(double sr, int spb) {
    inputMeter.prepare(sr, 2);
    outputMeter.prepare(sr, 2);
    autoGain.prepare(sr);
    clipper.setCeiling(1.0f);

    preClipBuffer.setSize(2, spb, false, true, true);
    scopeFifo.reset();
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

    if (bypassParam->get()) {
        inputMeter.process(buffer);
        outputMeter.process(buffer);
        return;
    }

    // 1. Pre-gain metering (this is what tells the user where the raw signal sits).
    inputMeter.process(buffer);

    // 1b. Auto-gain analyzer captures the raw peak.
    autoGain.process(buffer);

    // 2. Apply input gain.
    buffer.applyGain(juce::Decibels::decibelsToGain(inputGainParam->get()));

    // 3. Snapshot pre-clip signal for the scope (no allocation: preallocated buffer).
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);
    const int n     = buffer.getNumSamples();
    preClipBuffer.setSize(numCh, n, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        preClipBuffer.copyFrom(ch, 0, buffer, ch, 0, n);

    // 4. Clip.
    clipper.setType(static_cast<Clipper::Type>(clipTypeParam->getIndex()));
    clipper.process(buffer);

    // 5. Push to scope.
    writeToScope(preClipBuffer, buffer);

    // 6. Output trim.
    buffer.applyGain(juce::Decibels::decibelsToGain(outputTrimParam->get()));

    // 7. Output metering.
    outputMeter.process(buffer);
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
