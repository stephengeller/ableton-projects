#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "InstanceRegistry.h"

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
    linkBypassParam  = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter(Param::linkBypass));
    preClipHpfParam  = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter(Param::preClipHpf));
    jassert(targetPeakParam && inputGainParam && driveParam && clipTypeParam
            && osFactorParam && outputTrimParam && bypassParam
            && gainMatchParam && linkBypassParam && preClipHpfParam);

    // Make ourselves visible to other ClipToZero instances in the same
    // host process. The InstanceRegistry singleton is how cross-instance
    // bypass broadcasts find their targets. Unregistered in the dtor.
    InstanceRegistry::get().registerInstance(this);
}

ClipToZeroProcessor::~ClipToZeroProcessor() {
    // Remove ourselves from the cross-instance registry FIRST, before
    // anything else in this destructor runs. Other instances doing a
    // bypass broadcast must not visit us once our APVTS starts being
    // torn down. InstanceRegistry::unregisterInstance is thread-safe;
    // a concurrent forEachOther holds the same SpinLock, so this call
    // blocks until any in-flight broadcast finishes.
    InstanceRegistry::get().unregisterInstance(this);
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
    truePeakOut.prepare(sr, 2, spb);
    clipper.setCeiling(1.0f);

#if CTZ_PAID_BUILD
    // 60-second interval, 300 ms silence window. Tuned to be annoying enough
    // to convert evaluators to buyers without making short auditioning passes
    // impossible. 300 ms is long enough to be obviously a feature, short
    // enough that a buyer testing a 4-bar loop will still hear the effect.
    constexpr double demoIntervalSeconds = 60.0;
    constexpr double demoDurationSeconds = 0.3;
    demoInterruptIntervalSamples    = juce::roundToInt(sr * demoIntervalSeconds);
    demoInterruptDurationSamples    = juce::roundToInt(sr * demoDurationSeconds);
    demoSamplesSinceLastInterrupt   = 0;
    demoSamplesIntoCurrentInterrupt = 0;
    demoInInterrupt                 = false;
#endif

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
    int maxOsLatency = 0;
    for (size_t i = 0; i < oversamplers.size(); ++i) {
        const size_t stages = i + 1;  // 2x = 1 stage, 4x = 2, 8x = 3
        oversamplers[i] = std::make_unique<OS>(2, stages,
                                                OS::filterHalfBandFIREquiripple,
                                                /*isMaxQuality=*/true,
                                                /*useIntegerLatency=*/true);
        oversamplers[i]->initProcessing(static_cast<size_t>(spb));
        oversamplers[i]->reset();
        maxOsLatency = juce::jmax(maxOsLatency,
                                  static_cast<int>(oversamplers[i]->getLatencyInSamples()));
    }
    currentOsFactor = -1;  // force latency re-check on next processBlock

    // Pre-clip delay line for GR alignment. Sized to the worst-case OS
    // latency (8x) + one block of headroom so setDelay() can move the
    // tap freely without allocating. juce::dsp::ProcessSpec is what
    // DelayLine.prepare expects; we report 2 channels because the
    // maximum input layout we support is stereo.
    juce::dsp::ProcessSpec spec { sr, static_cast<juce::uint32>(spb), 2 };
    preClipDelay.setMaximumDelayInSamples(maxOsLatency + spb + 1);
    preClipDelay.prepare(spec);
    preClipDelay.reset();
    delayedPreBuffer.setSize(2, spb, false, true, true);
    currentOsLatencySamples = -1;  // force resync on next processBlock
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

        // 4. Push to scope + GR history.
        //
        // GR history compares preClipBuffer (signal entering the clipper)
        // against buffer (signal leaving it). With OS on, the downsample
        // FIR delays the OUT side by getLatencyInSamples() native samples,
        // so a raw pre[i] vs post[i] comparison misaligns by that many
        // samples and produces phantom GR readings. The preClipDelay line
        // below delays preClipBuffer by exactly the same amount, so the
        // pre and post streams refer to the same physical instant and the
        // bin-peak comparison in GRHistory returns correct values.
        //
        // OS Off => latency = 0 => delay line is a passthrough, comparison
        // is unchanged from pre-OS behaviour. So this single code path
        // serves both OS-on and OS-off cases without branching.
        const int osLatency = (factorIdx >= 1
                               && factorIdx <= static_cast<int>(oversamplers.size())
                               && oversamplers[factorIdx - 1])
                              ? static_cast<int>(oversamplers[factorIdx - 1]->getLatencyInSamples())
                              : 0;
        if (osLatency != currentOsLatencySamples) {
            // Latency changed (OS factor swapped, or first call). Reset
            // the delay line so we don't bleed stale samples from the old
            // tap position into the new comparison.
            currentOsLatencySamples = osLatency;
            preClipDelay.setDelay(static_cast<float>(osLatency));
            preClipDelay.reset();
        }

        // Push the current pre sample into the delay line FIRST, then
        // pop the delayed value. Order matters:
        //
        //   * juce::dsp::DelayLine reads at (writePos + delay) mod size.
        //     popSample-before-pushSample reads the buffer position that
        //     hasn't been written yet, returning a STALE value (initial
        //     zero for the first ~bufferSize calls, or wrapped-around-
        //     old data after that).
        //   * For delay = 0 (OS off) the buggy order returns ZEROS
        //     forever -- which makes pre look silent to GRHistory and
        //     suppresses real GR readings.
        //   * For delay > 0 (OS on) the buggy order is off by 1 sample,
        //     which on transient material like hi-hats can shift peaks
        //     across 1 ms bin boundaries and produce phantom GR.
        //
        // Push-first-then-pop reads the just-written sample at delay 0
        // (correct passthrough), and exactly-delay-samples-old at
        // delay > 0 (correct alignment with the OS downsampler's group
        // delay). Identified + fixed in v0.5.10.
        delayedPreBuffer.setSize(numCh, n, false, false, true);
        for (int ch = 0; ch < numCh; ++ch) {
            const float* src = preClipBuffer.getReadPointer(ch);
            float* dst       = delayedPreBuffer.getWritePointer(ch);
            for (int i = 0; i < n; ++i) {
                preClipDelay.pushSample(ch, src[i]);
                dst[i] = preClipDelay.popSample(ch);
            }
        }

        // Was the clipper actually doing work this block? For Hard mode,
        // 'work' means at least one sample was shaved (getClippedSampleCount
        // > 0). For Soft / Poly / Tube the curve compresses every non-zero
        // sample by some amount, so 'work' is always true for those modes.
        //
        // This flag gates GRHistory so phantom GR from the OS-chain FIR's
        // small frequency-response artifacts on unclipped material doesn't
        // get reported. The user noticed this specifically: OS off → GR
        // reads 0 when nothing's clipping (correct); OS on → GR was
        // reading -2 to -3 dB on unclipped hi-hat material (phantom).
        const auto clipType = clipper.getType();
        const bool clipperWasActive = (clipType != Clipper::Type::Hard)
                                       || clipper.getClippedSampleCount() > 0;

        writeToScope(delayedPreBuffer, buffer);
        grHistory.process(delayedPreBuffer, buffer, clipperWasActive);
        spectrum.pushSamples(buffer);    // post-clip spectrum, for the overlay

        // 5. Output trim.
        buffer.applyGain(juce::Decibels::decibelsToGain(outputTrimParam->get()));
    }

#if CTZ_PAID_BUILD
    // Demo-mode silence interrupt. Runs AFTER the audio chain (or bypass
    // gain match) and BEFORE output metering, so the meters and LUFS show
    // the actual (silenced) output — the visible "demo dip" reinforces the
    // prompt to buy. Unconditional regardless of bypass: otherwise demo
    // limits would be trivially circumvented by toggling Bypass.
    if (isDemo)
        processDemoMode(buffer);
#endif

    // Output metering and LUFS always run on the post-chain (or post-bypass)
    // signal — that way the readouts reflect what the host actually receives.
    outputMeter.process(buffer);
    lufs.process(buffer);
    // True-peak: 4x-oversampled peak analysis of what the host gets. Adds no
    // audio-path latency (we never downsample); just consumes a few % CPU
    // for the FIR upsample of the analysis tap.
    truePeakOut.process(buffer);

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

#if CTZ_PAID_BUILD
void ClipToZeroProcessor::processDemoMode(juce::AudioBuffer<float>& buffer) noexcept {
    // Walk the block sample-by-sample in two phases:
    //   1. counting up to the next interrupt (normal audio passes through)
    //   2. inside an interrupt (silence the output for demoInterruptDurationSamples)
    // The loop handles interrupts that straddle a block boundary — the
    // per-block counters carry state across processBlock invocations.
    const int n = buffer.getNumSamples();
    if (n == 0 || demoInterruptIntervalSamples <= 0) return;

    int idx = 0;
    while (idx < n) {
        if (demoInInterrupt) {
            // We're inside a silence window — clear samples until either
            // the block or the interrupt ends, whichever comes first.
            const int remainingInInterrupt
                = demoInterruptDurationSamples - demoSamplesIntoCurrentInterrupt;
            const int silentLen = juce::jmin(n - idx, remainingInInterrupt);
            for (int ch = 0, numCh = buffer.getNumChannels(); ch < numCh; ++ch) {
                auto* x = buffer.getWritePointer(ch);
                std::fill(x + idx, x + idx + silentLen, 0.0f);
            }
            demoSamplesIntoCurrentInterrupt += silentLen;
            idx += silentLen;
            if (demoSamplesIntoCurrentInterrupt >= demoInterruptDurationSamples) {
                // Interrupt complete — exit the silence window and start
                // counting toward the next interval from zero.
                demoInInterrupt                 = false;
                demoSamplesIntoCurrentInterrupt = 0;
                demoSamplesSinceLastInterrupt   = 0;
            }
        } else {
            // Between interrupts — count down to the next one, leaving the
            // audio untouched. When the counter crosses the interval, the
            // next iteration enters the silence branch.
            const int remainingToInterrupt
                = demoInterruptIntervalSamples - demoSamplesSinceLastInterrupt;
            const int passLen = juce::jmin(n - idx, remainingToInterrupt);
            demoSamplesSinceLastInterrupt += passLen;
            idx += passLen;
            if (demoSamplesSinceLastInterrupt >= demoInterruptIntervalSamples) {
                // Flip into interrupt mode. The next iteration enters the
                // silencing branch from a clean zero-sample state — no
                // off-by-one with sentinel values.
                demoInInterrupt = true;
            }
        }
    }
}
#endif

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
