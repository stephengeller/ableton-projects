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
    osFactorParam    = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(Param::osFactor));
    outputTrimParam  = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter(Param::outputTrim));
    bypassParam      = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter(Param::bypass));
    gainMatchParam   = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter(Param::gainMatch));
    preClipHpfParam  = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter(Param::preClipHpf));
    jassert(targetPeakParam && inputGainParam && driveParam && clipTypeParam
            && osFactorParam && outputTrimParam && bypassParam
            && gainMatchParam && preClipHpfParam);
}

void ClipToZeroProcessor::updateLatencyIfChanged() {
    const int factorIdx = osFactorParam->getIndex();  // 0/1/2/3
    if (factorIdx == currentOsFactor) return;
    currentOsFactor = factorIdx;
    int latency = 0;
    if (factorIdx >= 1 && factorIdx <= 3 && oversamplers[factorIdx - 1])
        latency = static_cast<int>(oversamplers[factorIdx - 1]->getLatencyInSamples());
    setLatencySamples(latency);
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
    grHistory.prepare(sr);
    spectrum.prepare(sr);
    clipper.setCeiling(1.0f);

    preClipBuffer.setSize(2, spb, false, true, true);
    scopeFifo.reset();

    // Force a redesign on next process call (sample rate may have changed).
    currentHpfHz = -1.0f;
    preClipHpfL.reset();
    preClipHpfR.reset();

    // Build the three oversamplers (2x = 1 stage, 4x = 2 stages, 8x = 3
    // stages). Linear-phase FIR for the cleanest result; latency is
    // reported via setLatencySamples so the host can compensate. Each
    // initProcessing allocates internal scratch — done here once per
    // sample-rate / buffer-size change, never during audio processing.
    using OS = juce::dsp::Oversampling<float>;
    for (size_t i = 0; i < oversamplers.size(); ++i) {
        const size_t stages = i + 1;  // 2x = 1 stage, 4x = 2, 8x = 3
        oversamplers[i] = std::make_unique<OS>(2, stages,
                                                OS::filterHalfBandFIREquiripple,
                                                /*isMaxQuality=*/true,
                                                /*useIntegerLatency=*/true);
        oversamplers[i]->initProcessing(static_cast<size_t>(spb));
        oversamplers[i]->reset();
    }
    currentOsFactor = -1;  // force latency re-check on next processBlock
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

    const bool bypassed = bypassParam->get();
    if (bypassed && gainMatchParam->get()) {
        // Gain-matched bypass: apply the cached output-vs-input RMS
        // difference so the dry signal A/Bs at the same loudness the
        // processed signal would have. The difference is updated below
        // ONLY when not bypassed (chicken-and-egg: we can only measure
        // the gap when the chain is actually running).
        const float gain = juce::Decibels::decibelsToGain(matchGainDb.load());
        if (std::abs(gain - 1.0f) > 0.001f)
            buffer.applyGain(gain);
    }

    if (!bypassed) {
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
        // Oversampling reduces aliasing artefacts that hard-clipping at the
        // native rate would generate above Nyquist (and fold back into the
        // audible range). For non-Off factors we upsample, clip at the
        // higher rate, then downsample. setLatencySamples() informs the
        // host so it can time-compensate other tracks.
        clipper.setType(static_cast<Clipper::Type>(clipTypeParam->getIndex()));
        updateLatencyIfChanged();

        const int factorIdx = osFactorParam->getIndex();
        if (factorIdx == 0 || factorIdx > static_cast<int>(oversamplers.size())) {
            // Off — clip at the native rate (legacy behaviour).
            clipper.process(buffer);
        } else {
            auto& os = *oversamplers[factorIdx - 1];
            juce::dsp::AudioBlock<float> nativeBlock(buffer);
            auto osBlock = os.processSamplesUp(nativeBlock);

            // Wrap the up-sampled block as an AudioBuffer so Clipper can
            // process it without an API change. Channel pointers come from
            // the oversampler's internal buffer.
            std::array<float*, 2> osCh {};
            const int osChannels = juce::jmin(static_cast<int>(osBlock.getNumChannels()), 2);
            for (int ch = 0; ch < osChannels; ++ch)
                osCh[ch] = osBlock.getChannelPointer(ch);
            juce::AudioBuffer<float> osBuffer(osCh.data(), osChannels,
                                              static_cast<int>(osBlock.getNumSamples()));
            clipper.process(osBuffer);

            os.processSamplesDown(nativeBlock);  // writes back into `buffer`
        }

        // 4. Push to scope + GR history (both compare preClipBuffer vs the
        //    just-clipped buffer; the GR history sees how much each sample
        //    was shaved by the clipper).
        //
        // GR history is intentionally skipped when oversampling is on:
        // the OS downsample FIR introduces ~30 samples of group delay,
        // misaligning preClipBuffer (captured pre-upsampler) and buffer
        // (post-downsampler) such that per-bin peak comparison produces
        // phantom GR readings. Disabled until we have a delay-compensated
        // implementation; the editor also hides the strip in this state.
        writeToScope(preClipBuffer, buffer);
        if (factorIdx == 0)
            grHistory.process(preClipBuffer, buffer);
        spectrum.pushSamples(buffer);    // post-clip spectrum, for the overlay

        // 5. Output trim.
        buffer.applyGain(juce::Decibels::decibelsToGain(outputTrimParam->get()));
    }

    // Output metering and LUFS always run on the post-chain (or post-bypass)
    // signal — that way the readouts reflect what the host actually receives.
    outputMeter.process(buffer);
    lufs.process(buffer);

    // ---- Gain-match tracking ------------------------------------------
    // Only update when actually processing: in bypass mode the input and
    // output meters see the same signal and the difference is always 0,
    // which would erase the cached value.
    if (!bypassed) {
        const float inRms  = juce::jmax(inputMeter.getRmsDb(0),  inputMeter.getRmsDb(1));
        const float outRms = juce::jmax(outputMeter.getRmsDb(0), outputMeter.getRmsDb(1));
        // Require both channels to have actual signal (> -50 dB RMS) so
        // a quiet section doesn't poison the cached value with noise-floor
        // differences. Smooth with alpha=0.02 (~500 ms time constant at
        // typical block rates) so the value is stable for A/B.
        if (inRms > -50.0f && outRms > -50.0f) {
            // gain to ADD to the dry signal so dry matches wet:
            //   if wet is louder than dry, this is positive (boost dry)
            //   if wet is quieter than dry, this is negative (cut dry)
            const float instMatch = outRms - inRms;
            const float current   = matchGainDb.load();
            matchGainDb.store(current * 0.98f + instMatch * 0.02f);
        }
    }
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
