# Clip To Zero

A clipper plugin for the Clip-to-Zero gain-staging workflow. Built with JUCE 8.

Replaces the dpMeter5 + GClip combo I was using by bundling the three steps into one device:

1. **Meter** the incoming signal (peak + 300 ms RMS, with peak hold).
2. **Auto-gain** — press one button, the plugin captures the peak over a 2-second window and sets the input gain so that peak hits 0 dBFS.
3. **Clip** — drive the result into a hard or soft (tanh) clipper at 0 dBFS.
4. **Visualize** — an oscilloscope shows the pre-clip signal (grey ghost) and the post-clip signal (white), with red shading wherever the clipper actually shaved samples.

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
├── PluginEditor.{h,cpp}         # GUI layout, auto-gain trigger
├── Parameters.h                  # parameter IDs + ranges
├── DSP/
│   ├── LevelMeter.{h,cpp}       # peak + RMS + peak-hold per channel
│   ├── Clipper.{h,cpp}          # hard / soft (tanh) clip
│   └── AutoGainAnalyzer.{h,cpp} # 2 s peak capture → suggested gain
└── UI/
    ├── MeterComponent.{h,cpp}    # vertical bar meter
    └── OscilloscopeComponent.{h,cpp} # pre/post traces, clipped-region highlight
```

## Notes for future me

- **No oversampling yet.** Hard clipping at 0 dBFS will alias above Nyquist and fold back into the audible range. Pro-L and StandardCLIP do 4× or 16× oversampling around the clipper. Adding it is a `juce::dsp::Oversampling<float>` wrapped around `clipper.process()`. Worth doing once the basic plugin is bedded in.
- **No true-peak (ISP) detection.** The peak meter shows sample-peak only; the meter can read -0.1 dBFS while the analog reconstruction overshoots to +0.5 dB. True peak needs 4× upsampling on the meter side too.
- **Auto-gain is non-destructive.** It writes to the Input Gain parameter via `setValueNotifyingHost`, so it's automatable, undoable from the host, and you can tweak the result by hand afterwards.
- **The Clip Type selector** is automatable — you can A/B hard vs soft mid-bar from a host automation lane.
