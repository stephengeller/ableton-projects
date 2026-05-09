# Clip To Zero

A clipper plugin for the Clip-to-Zero gain-staging workflow. Built with JUCE 8.

Replaces the dpMeter5 + GClip combo I was using by bundling everything into one device:

1. **Meter** the incoming signal (peak + 300 ms RMS, with peak hold).
2. **Set a Target Peak** (defaults to 0 dBFS, can be lowered to leave headroom — e.g. -1 dBFS for streaming).
3. **Auto-Gain** — press one button, the plugin captures the peak over a 2-second window and sets the input gain so that peak lands on the target. Works in both directions: raises gain if the signal is quiet, lowers it if hot.
4. **Drive** — independent post-staging gain into the clipper. Output is still clamped to 0 dBFS by the clipper ceiling, so increasing Drive squashes peaks more aggressively without raising the channel meter.
5. **Clip** — hard or soft (tanh) clipper at 0 dBFS.
6. **Visualize** — a scrolling oscilloscope (60 fps, newest sample on the right, scrolling leftward) shows the pre-clip signal (grey ghost) and the post-clip signal (white), with red shading wherever the clipper actually shaved samples. **Scope Zoom** controls the time window (1 ms to 500 ms, log-skewed); below ~2 samples/pixel the scope renders as smooth path-stroked traces, above that it switches to min/max decimation so transients stay visible at any zoom level. **Headroom** controls vertical zoom (0 to 24 dB above 0 dBFS visible) so when you're driving 12 dB into the clipper you can still see exactly how much overshoot is being shaved.
7. **LUFS readout** — ITU-R BS.1770-4 / EBU R128 loudness measurement on the output. Shows momentary (400 ms), short-term (3000 ms), and integrated (gated mean since last reset) values. Reset button clears just the integrated value; M and S keep running.

Builds **VST3** and **AU** (and Standalone).

## Building (macOS)

You already have Xcode and Homebrew. You only need CMake:

```sh
brew install cmake
```

Then from the project root:

```sh
cmake -B build -G Xcode
cmake --build build --config Release
```

JUCE is fetched automatically by CMake on the first run (pinned to 8.0.4) — no manual setup.

After a successful build, the plugin bundles will be at:

- VST3: `build/ClipToZero_artefacts/Release/VST3/ClipToZero.vst3`
- AU: `build/ClipToZero_artefacts/Release/AU/ClipToZero.component`
- Standalone: `build/ClipToZero_artefacts/Release/Standalone/ClipToZero.app`

JUCE installs them to the standard system locations as part of the build:

- `~/Library/Audio/Plug-Ins/VST3/ClipToZero.vst3`
- `~/Library/Audio/Plug-Ins/Components/ClipToZero.component`

Restart Ableton (or rescan plugins) and ClipToZero will appear under stephengeller / Distortion.

## Layout

```
Source/
├── PluginProcessor.{h,cpp}      # APVTS, processBlock, scope feed
├── PluginEditor.{h,cpp}         # GUI layout, auto-gain trigger, LUFS panel
├── Parameters.h                  # parameter IDs + ranges
├── DSP/
│   ├── LevelMeter.{h,cpp}       # peak + RMS + peak-hold per channel
│   ├── Clipper.{h,cpp}          # hard / soft (tanh) clip
│   ├── AutoGainAnalyzer.{h,cpp} # 2 s peak capture → target-aware gain
│   └── LUFSMeter.{h,cpp}        # ITU-R BS.1770-4 K-weighted, M/S/I, gated
└── UI/
    ├── MeterComponent.{h,cpp}    # vertical bar meter
    └── OscilloscopeComponent.{h,cpp} # pre/post traces, clipped-region highlight
```

## Notes for future me

- **No oversampling yet.** Hard clipping at 0 dBFS will alias above Nyquist and fold back into the audible range. Pro-L and StandardCLIP do 4× or 16× oversampling around the clipper. Adding it is a `juce::dsp::Oversampling<float>` wrapped around `clipper.process()`. Worth doing once the basic plugin is bedded in.
- **No true-peak (ISP) detection.** The peak meter shows sample-peak only; the meter can read -0.1 dBFS while the analog reconstruction overshoots to +0.5 dB. True peak needs 4× upsampling on the meter side too.
- **Auto-gain is non-destructive and target-aware.** It writes to the Input Gain parameter via `setValueNotifyingHost`, so it's automatable, undoable from the host, and you can tweak the result by hand afterwards. The target is the separate Target Peak parameter — change it before pressing Auto-Gain to stage to anywhere from -12 to 0 dBFS.
- **All parameters automatable.** Target Peak, Input Gain, Drive, Clip Type, Output Trim, Scope Zoom, Headroom — the host can sweep any of them mid-track from an automation lane (the scope ones are silly to automate but get free state save/restore).
- **LUFS implementation notes.** K-weighting uses JUCE's biquad designers (`Coefficients::makeHighShelf` + `makeHighPass`) at the actual playback sample rate, which redesigns the BS.1770 prototype at any rate. Integrated history is capped at ~1 hour (36000 × 100 ms blocks) on the heap as a `std::array<double, 36000>` member — no audio-thread allocations. Both gates (-70 LUFS absolute, -10 LU relative) are implemented per spec. Accuracy is within ~0.5 LU of strict BS.1770 reference implementations; close enough for setting clip-driven loudness targets, not for compliance reports.
