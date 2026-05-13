# ClipToZero

A clipper plugin for the **clip-to-zero gain-staging workflow** — peak / RMS / true-peak / LUFS metering, Auto-Gain to a target peak, drive into a hard / soft / poly / tube clipper at 0 dBFS, an oscilloscope with pre / post diff overlay, oversampling around the clipper, optional spectrum overlay, and a gain-reduction history strip.

VST3 / AU / Standalone, universal Apple Silicon + Intel. Built with [JUCE 8](https://juce.com).

---

## Install (one command)

macOS — open **Terminal** and paste:

```sh
curl -fsSL https://raw.githubusercontent.com/stephengeller/ableton-projects/main/plugins/ClipToZero/install.sh | sh
```

That's it. The script downloads the latest macOS zip from [Releases](https://github.com/stephengeller/ableton-projects/releases/latest), drops **ClipToZero.vst3** and **ClipToZero.component** into `~/Library/Audio/Plug-Ins/`, and strips the macOS quarantine flag so your DAW will actually load them.

After it finishes, **rescan plugins in your DAW** — the script prints the per-DAW command at the end.

> Pin a specific version with `CLIPTOZERO_VERSION=v0.4.0 sh -c "$(curl -fsSL https://raw.githubusercontent.com/stephengeller/ableton-projects/main/plugins/ClipToZero/install.sh)"`.
>
> Read [`install.sh`](./install.sh) before running if you want to know exactly what it does — it's ~100 lines of bash with no external dependencies.

**Updating** is the same one-liner — it replaces any existing copy in place.

---

## Install manually

Prefer doing it yourself? Download `ClipToZero-vX.Y.Z-mac.zip` from the [latest release](https://github.com/stephengeller/ableton-projects/releases/latest), unzip it, then run these in Terminal:

```sh
# Adjust the path to wherever you unzipped
cd ~/Downloads/ClipToZero-v0.4.0-mac

# Install VST3 (Ableton, Cubase, Reaper, Bitwig, FL Studio...)
ditto VST3/ClipToZero.vst3 ~/Library/Audio/Plug-Ins/VST3/ClipToZero.vst3

# Install AU (Logic Pro, GarageBand, MainStage)
ditto AU/ClipToZero.component ~/Library/Audio/Plug-Ins/Components/ClipToZero.component

# Required: tell macOS you trust these binaries
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/ClipToZero.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/ClipToZero.component
```

Only install the format your DAW uses. The zip also contains `Standalone/ClipToZero.app` — drag it into `/Applications/` if you want to run without a DAW.

<details>
<summary>If you'd rather drag-drop in Finder than copy in Terminal</summary>

1. Unzip the release zip.
2. In Finder, hit **⇧⌘G** ("Go to folder") and paste `~/Library/Audio/Plug-Ins/VST3/` — drag `ClipToZero.vst3` into that window.
3. Do the same for `~/Library/Audio/Plug-Ins/Components/` with `ClipToZero.component`.
4. **Still required**: open Terminal and run the two `xattr` lines above. Without that, macOS Gatekeeper silently blocks the plugins and they don't appear in your DAW's browser.

</details>

> **Why is the `xattr` step required?** ClipToZero isn't signed with a paid Apple Developer ID, so macOS marks the downloaded zip as quarantined. Stripping that flag tells macOS you trust the file. Every free indie plugin (Vital, Surge XT, etc.) needs the same step until the developer pays Apple's $99/year fee.

---

## DAW rescan

After installing, your DAW needs to rescan plugins to pick up ClipToZero:

| DAW              | How                                                                                         |
| ---------------- | ------------------------------------------------------------------------------------------- |
| **Ableton Live** | Preferences → Plug-Ins → Rescan Plug-Ins. Hold **Option** while clicking for forced rescan. |
| **Logic Pro**    | Restart Logic — AUs auto-rescan on launch.                                                  |
| **Reaper**       | Preferences → Plug-Ins → VST → Re-scan.                                                     |
| **Bitwig**       | Settings → Locations → Plug-ins → Rescan.                                                   |
| **FL Studio**    | Options → Manage Plugins → Find more plugins.                                               |
| **Cubase**       | Studio → VST Plug-in Manager → Update.                                                      |

The plugin appears under manufacturer **stephengeller** in your plugin browser, category **Distortion / Analyzer**.

If Logic doesn't show it, run `auval -v aufx Cz01 Sgel` in Terminal — `AU VALIDATION SUCCEEDED` confirms the plugin is healthy and the issue is Logic's AU cache.

---

## What it does

1. **Meter** the incoming signal — peak, RMS (300 ms), and peak-hold.
2. **Set a Target Peak** — defaults to 0 dBFS, can be lowered to leave headroom (e.g. -1 dBFS for streaming).
3. **Auto-Gain** — press once, the plugin captures the peak over a 2-second window and sets the input gain so that peak lands on the target. Works in both directions (raises quiet signal, lowers hot signal).
4. **Drive** — independent post-staging gain into the clipper. Output stays clamped to 0 dBFS by the clipper ceiling, so increasing Drive squashes peaks more aggressively without raising the channel meter.
5. **Pre-clip HPF** — optional 2nd-order Butterworth high-pass before the clipper. Off when the slider sits at 20 Hz. Cleans sub-bass so it doesn't eat your clipping headroom.
6. **Clip** — four transfer curves: **Hard** (clamp), **Soft** (tanh), **Poly** (cubic soft knee), **Tube** (asymmetric tanh). Optional **oversampling** (Off / 2× / 4× / 8×) around the clipper to suppress aliasing.
7. **Oscilloscope** — pre-clip signal (grey ghost) and post-clip signal (white), with red shading wherever the clipper shaved samples. Horizontal zoom (1 ms to 10 s, log-skewed) and vertical headroom zoom (0 to 24 dB above 0 dBFS).
8. **GR strip** — gain-reduction history under the scope, peak-held. Same time axis as the scope. (Hidden while oversampling is on; see release notes for v0.3.1.)
9. **Spectrum overlay** — translucent FFT spectrum over the scope (Off / Subtle / Bold).
10. **LUFS readout** — ITU-R BS.1770-4 / EBU R128. Momentary (400 ms), short-term (3 s), integrated (gated mean since last reset), and crest factor.
11. **True-peak** — 4×-oversampled `dBTP` in the output meter header. Amber above -1 dBTP, red above 0 dBTP.
12. **Gain-matched A/B bypass** — toggling Bypass applies the cached output-vs-input RMS difference to the dry signal so the comparison is loudness-fair, not "louder = better".

---

## Requirements

- macOS 10.13+ (universal Apple Silicon + Intel)
- A VST3- or AU-compatible DAW

Windows and Linux builds also ship with each [release](https://github.com/stephengeller/ableton-projects/releases/latest) — see the platform-specific zip's `INSTALL.md` for steps.

---

<details>
<summary>Build from source</summary>

If you'd rather build your own copy — locally-built binaries don't trigger Gatekeeper quarantine, so you skip the `xattr` step entirely.

```sh
brew install cmake   # one-time
cd plugins/ClipToZero
cmake -B build -G Xcode
cmake --build build --config Release
```

JUCE 8 is fetched automatically by CMake on the first build (pinned to 8.0.4). Successful Release builds auto-install to `~/Library/Audio/Plug-Ins/{VST3,Components}/` via `COPY_PLUGIN_AFTER_BUILD`.

For Windows / Linux toolchain walkthroughs and the per-OS packaging scripts, see [`dist/BUILD-FROM-SOURCE.md`](./dist/BUILD-FROM-SOURCE.md).

</details>

<details>
<summary>Cutting a release</summary>

```sh
git tag v0.4.0
git push --tags
```

Pushing a `v*` tag triggers `.github/workflows/build.yml`, which builds macOS + Windows + Linux in parallel, runs each platform's `dist/package-*` script, and attaches the resulting zips to the GitHub Release.

Local one-shot packaging (macOS host only):

```sh
./dist/package-mac.sh           # rebuild + zip
./dist/package-mac.sh --skip-build   # zip an existing build
./dist/package-mac.sh --release      # also push to the GitHub release via gh
```

</details>

<details>
<summary>UI design reference</summary>

The current UI implements **Variant F · Stages** from the Claude Design exploration bundle:

- **Design URL**: <https://api.anthropic.com/v1/design/h/LYRs9XYR5GvBoZL062Ewtg?open_file=ClipToZero+VST.html>
- F was chosen out of four explorations (A · Verdict, B · Signal Path, C · Console, F · Stages) — it synthesises A's "scope is the product" aesthetic with B's flow-language, plus explicit numbered workflow steps and full horizontal + vertical scope zoom.
- Key F elements: black / lime monospace palette (Inter for chrome, JetBrains Mono for numerics), three numbered stage lanes (Stage to 0 → Drive into clipper → Judge by LUFS) that highlight as the user progresses, horizontal meters, rotary knobs with value-arc rendering, scope with pre/post diff-fill in red, headroom-aware vertical scaling.

</details>

<details>
<summary>Source layout</summary>

```
Source/
├── PluginProcessor.{h,cpp}      APVTS, processBlock, scope feed, OS routing
├── PluginEditor.{h,cpp}         GUI layout, auto-gain trigger, stage state
├── Parameters.h                 parameter IDs + ranges + APVTS layout
├── DSP/
│   ├── LevelMeter.{h,cpp}       peak + RMS + peak-hold per channel
│   ├── Clipper.{h,cpp}          hard / soft / poly / tube transfer curves
│   ├── AutoGainAnalyzer.{h,cpp} 2 s peak capture → target-aware gain
│   ├── LUFSMeter.{h,cpp}        ITU-R BS.1770-4 K-weighted, M/S/I, gated
│   ├── GRHistory.{h,cpp}        per-bin peak gain-reduction ring buffer
│   ├── SpectrumAnalyzer.{h,cpp} 2048-point FFT, Hann window
│   └── TruePeakMeter.{h,cpp}    4× FIR-upsampled dBTP analysis tap
└── UI/
    ├── Theme.h                  colour palette + font helpers
    ├── LookAndFeel_F.{h,cpp}    custom Knob / Button / Slider rendering
    ├── StageLane.{h,cpp}        numbered stage cards with auto-progression
    ├── HorizontalMeter.{h,cpp}  compact per-channel meter row
    ├── OscilloscopeComponent.{h,cpp} pre/post traces, diff highlight
    ├── GRMeterComponent.{h,cpp} GR history strip with grid + held peak
    ├── LufsBox.{h,cpp}          M / S / I / CR numeric readouts
    ├── Knob.{h,cpp}             rotary knob with value-arc + draggable label
    └── TransferCurveComponent.{h,cpp} (unused; kept for future variants)
```

</details>

---

## License

MIT — see [LICENSE](../../LICENSE). JUCE 8 is fetched at build time under its own licence (GPL3 for free use, commercial otherwise).
