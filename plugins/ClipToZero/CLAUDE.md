# CLAUDE.md — ClipToZero operating manual

Reference for Claude (or future-you) working on this plugin. Distilled
from the development arc that ran v0.1 through v0.6.x.

This document captures architectural decisions, conventions, and the
gotchas that bit during real development. Read it before making
non-trivial changes; it'll save you from rediscovering things the hard
way.

---

## What this plugin is

A clipper VST3 / AU / Standalone built around the "clip-to-zero" gain-
staging workflow: stage your input so the loudest peaks land near 0
dBFS, drive into a clipper, judge by LUFS rather than perceived
loudness. The UI implements **Variant F — Stages**: three numbered
"stage lanes" that highlight as the user works through the workflow.

Built with JUCE 8 (pinned to 8.0.4 in `CMakeLists.txt`), CMake, C++17.

---

## Quick start

```sh
# From plugins/ClipToZero/
brew install cmake          # one-time
cmake -B build -G Xcode
cmake --build build --config Release
auval -v aufx Cz01 Sgel     # AU validation (must say AU VALIDATION SUCCEEDED)
```

Successful builds auto-install to:

- `~/Library/Audio/Plug-Ins/VST3/ClipToZero.vst3`
- `~/Library/Audio/Plug-Ins/Components/ClipToZero.component`

**macOS won't reload a .vst3 / .component dylib while a DAW has it
loaded.** Quit the DAW fully (Cmd+Q, not just close project) to pick
up a fresh build. This trips up testing constantly — when "my change
didn't apply" feels like a bug, check this first.

---

## Audio chain in `processBlock`

The order matters and isn't obvious from the file. Each step modifies
`buffer` in place. References to "pre" / "post" elsewhere in the
codebase mean pre-clipper / post-clipper specifically.

```
1. Input metering (always, even on bypass)
2. If bypassed + Gain Match on: apply running RMS-match gain to dry
3. If not bypassed:
   a. autoGain.process(buffer)      — peak capture, doesn't modify
   b. Apply Input Gain (dB)
   c. Pre-clip HPF (if active)
   d. Apply Drive (dB)
   e. Snapshot preClipBuffer = buffer
   f. Clip:
      - OS Off: clipper.process(buffer) at native rate
      - OS On: upsample → clipper.process(osBuffer) → downsample
   g. Delay-compensate preClipBuffer by OS latency (see preClipDelay)
   h. writeToScope(delayedPre, buffer)
   i. grHistory.process(delayedPre, buffer, clipperWasActive)
   j. spectrum.pushSamples(buffer)
4. CTZ_PAID_BUILD: demo-mode silence interrupt (if isDemo)
5. outputMeter.process(buffer)
6. lufs.process(buffer)
7. truePeakOut.process(buffer)
8. Apply Output Trim (dB)
```

**Key invariant**: `preClipBuffer` is captured AFTER input gain + HPF +
drive, just before clipping. So it represents "what enters the clipper",
not "raw input". The delay-comp line aligns it temporally with `buffer`
when OS is on.

---

## Visual conventions

### Theme.h is the single source of truth

All colours come from `Source/UI/Theme.h`. Never hardcode a colour in a
component — derive it from the theme. Changes to the theme cascade
through the whole UI (scope, meters, lanes, buttons) atomically.

Palette identity:

- **Background**: near-black `#0c0d0c` (very slight green tint)
- **Lime accent** `#a8d860`: primary action / "this is on" / value-arc
- **Cream text bright** `#e6f0c2`: numeric readouts, post-clip waveform
- **Grey text dim** `#7d8674` / `#9ea692` / `#5d6457`: hierarchy of labels
- **Red overload** `#ff5a50`: peaks above 0 dBFS, clipped regions
- **Orange bypass** `#ffaa50`: bypass-active fill, DEMO badge
- **Grey scopePre** `#c3c3c3` at 0.30 alpha: pre-clip overlay

When adding a new visual, derive from these. Resist introducing new
hues unless there's a genuinely new semantic role.

### Typography split

- **Inter** for chrome / labels / narrative copy
- **JetBrains Mono** for numerics, dB readouts, code, technical labels

The split is deliberate — anywhere a _number_ appears, it's mono. This
is enforced everywhere; preserve it.

### Held-peak is the universal numeric cadence

Every numeric readout in the UI follows the same envelope: snap up to
the new peak, hold for 1.5 seconds, decay linearly. This is enforced
across:

- Input L / R numerics (HorizontalMeter, via LevelMeter.getPeakHoldDb)
- Output L / R numerics (same)
- Output TP readout (PluginEditor uses TruePeakMeter.getTruePeakHoldDb)
- GR meter (vertical bar's numeric, GRMeterVertical)

**Bar fills** stay on the _instantaneous_ peak for fast visual feedback
(the bar momentarily flashes red the moment a signal crosses 0 dBFS).
**Text** uses the held peak for readability.

If you add a new readout, follow the same pattern. Mixed cadences
fatigue users.

### LookAndFeel property tags

`Source/UI/LookAndFeel_F.cpp::drawButtonBackground` branches on
`button.getProperties()` to render different button styles. Current
tags:

| Tag                                | Effect                                                        |
| ---------------------------------- | ------------------------------------------------------------- |
| `"variant" = "primary"`            | Lime-filled accent button (AUTO-GAIN)                         |
| `"variant" = "warning"`            | Orange-filled when toggled on (BYPASS)                        |
| `"variant" = "default"` (or unset) | Outlined-only                                                 |
| `"measuring" = true`               | Hollow during AUTO-GAIN's capture window                      |
| `"dropdown" = true`                | Adds chevron at right edge (CLIP-XXX, VIEW, BYPASS-menu)      |
| `"linkIcon" = true`                | Renders chain-link glyph instead of text (Link Bypass toggle) |

Pattern is "tag-then-style" — add a new tag, branch on it in the
LookAndFeel. Avoid subclassing TextButton unless you genuinely need
new behaviour, not just new appearance.

---

## InstanceRegistry pattern

`Source/InstanceRegistry.{h,cpp}` is a process-wide singleton that
tracks every live `ClipToZeroProcessor` instance in the host. Used for
cross-instance features (currently: Link Bypass).

To add a new cross-instance feature:

1. Define the per-instance state on the processor (e.g., an APVTS bool)
2. Read the registry from the editor or processor:
   ```cpp
   InstanceRegistry::get().forEachOther(&processor, [](ClipToZeroProcessor* other) {
       // do something with other
   });
   ```
3. Hold the SpinLock for the entire `forEachOther` walk. The destructor
   of `ClipToZeroProcessor` calls `unregisterInstance`, which acquires
   the same lock — so any instance you're iterating is guaranteed alive
   for the duration of your lambda.

**Avoid re-entry into the registry from within `forEachOther`** — the
SpinLock is not recursive, and same-thread re-acquire will spin
forever. This was the v0.5.1 deadlock bug. The fix:
`ScopedBroadcastGuard` thread-local flag pattern, see
`InstanceRegistry.h`.

---

## Build flavours

CMake option `CTZ_PAID_BUILD` toggles paid-build features:

```sh
# Free / dev build (default)
cmake -B build -G Xcode
cmake --build build --config Release

# Paid / demo build (silence interrupt + DEMO badge + license stubs)
cmake -B build-paid -G Xcode -DCTZ_PAID_BUILD=ON
cmake --build build-paid --config Release
```

The paid build does NOT auto-install (so it doesn't overwrite your dev
binary). Manually copy from `build-paid/ClipToZero_artefacts/Release/`
to your Library plug-in folders if you want to test it in a DAW.

`CTZ_PAID_BUILD=ON` is gated on a `#if CTZ_PAID_BUILD` compile-time
define. The license-check stubs are intentionally NOT yet implemented —
those should be written against a live Lemon Squeezy sandbox once that
account is set up. See `notes/LAUNCH_CHECKLIST.md` for the launch path.

---

## Critical gotchas

### `juce::dsp::DelayLine` requires push-first-then-pop

```cpp
// WRONG (returns stale values at delay 0; off by one at delay > 0):
dst[i] = delay.popSample(ch);
delay.pushSample(ch, src[i]);

// RIGHT:
delay.pushSample(ch, src[i]);
dst[i] = delay.popSample(ch);
```

JUCE's `DelayLine` reads at `(readPos + delayInt)` before decrementing,
so popping before pushing reads the position that hasn't been written
yet. For delay=0, this returns ZEROS forever (bug we shipped through
v0.5.4 → v0.5.9, fixed v0.6.0).

### APVTS `ButtonAttachment` fires onClick programmatically

When you `setValueNotifyingHost` on a button-bound parameter from
elsewhere (e.g., broadcasting Link Bypass to other instances), JUCE's
`ButtonAttachment::parameterChanged` calls `setToggleState(value,
sendNotificationSync)`. That **synchronously fires** `sendClickMessage`,
which fires `onClick`.

So broadcasting a param change to another instance triggers that
instance's onClick handler. If onClick does anything that touches a
shared lock (like re-entering `InstanceRegistry::forEachOther`),
you'll deadlock on a non-recursive lock.

Fix pattern: thread-local recursion guard. See
`InstanceRegistry::ScopedBroadcastGuard` for the canonical usage.

### `auval` cannot test multi-instance bugs

`auval -v aufx Cz01 Sgel` instantiates exactly ONE plugin instance and
runs it through the AU validation suite. Cross-instance bugs (Link
Bypass deadlocks, shared-state races, etc.) are invisible to it.

For Link Bypass changes specifically, **manually test with 2+ instances
in Ableton** before declaring a fix shipped. The v0.5.0 → v0.5.1
deadlock hotfix happened because the v0.5.0 design was AU-validated
but never multi-instance smoke-tested.

### GR comparison must be gated on actual clipper activity

`GRHistory::process` takes a third `clipperWasActive` parameter. **This
flag is the gate that distinguishes "the clipper did work" from "post
< pre for any reason".** Without it, the OS upsample/downsample FIR's
small frequency-response artifacts on high-frequency content (hi-hats)
register as phantom GR even when no clipping happens.

The flag is computed in PluginProcessor after clipper.process:

```cpp
const auto clipType = clipper.getType();
const bool clipperWasActive = (clipType != Clipper::Type::Hard)
                               || clipper.getClippedSampleCount() > 0;
```

Hard mode: gated on whether the clipper shaved any samples this block.
Soft / Poly / Tube: always true (those curves compress every non-zero
sample, so GR is always meaningful).

If you add a new clip type, decide which bucket it belongs to.

### macOS Gatekeeper quarantines unsigned downloads

The build is ad-hoc signed by JUCE. Downloads from GitHub Releases get
the `com.apple.quarantine` xattr, which makes Gatekeeper block them.
`install.sh` strips this. Manual installs need:

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/ClipToZero.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/ClipToZero.component
```

Will be eliminated when we pay for Apple Developer ID + notarisation —
see `notes/LAUNCH_CHECKLIST.md`.

---

## File index

Where to find things:

```
Source/
├── PluginProcessor.{h,cpp}      processBlock, APVTS, scope feed, OS routing
├── PluginEditor.{h,cpp}         GUI layout, brand bar, stage state, tooltips
├── Parameters.h                 APVTS layout + ParamID constants
├── Presets.h                    Factory preset data (inline const array)
├── InstanceRegistry.{h,cpp}     Process-wide live-instance registry
│
├── DSP/
│   ├── LevelMeter.{h,cpp}       Peak + RMS + peak-hold per channel
│   ├── Clipper.{h,cpp}          Hard / Soft / Poly / Tube transfer curves
│   ├── AutoGainAnalyzer.{h,cpp} 2 s peak-capture → target-aware gain
│   ├── LUFSMeter.{h,cpp}        ITU-R BS.1770-4 K-weighted M/S/I gated
│   ├── GRHistory.{h,cpp}        Per-bin GR comparison, 1 ms bins
│   ├── SpectrumAnalyzer.{h,cpp} 2048-point FFT, Hann window
│   └── TruePeakMeter.{h,cpp}    4× FIR-upsampled dBTP analysis tap
│
└── UI/
    ├── Theme.h                  Palette + font helpers (SINGLE SOURCE OF TRUTH)
    ├── LookAndFeel_F.{h,cpp}    Knob / Button / Slider rendering
    ├── StageLane.{h,cpp}        Three numbered stage cards
    ├── HorizontalMeter.{h,cpp}  Compact per-channel I/O meter row
    ├── GRMeterVertical.{h,cpp}  Pro-L2-style vertical GR meter
    ├── OscilloscopeComponent.{h,cpp} Pre/post traces, clipped highlight
    ├── LufsBox.{h,cpp}          M/S/I/CR numeric readouts
    ├── Knob.{h,cpp}             Rotary knob + value arc + drag label
    └── (TransferCurveComponent.{h,cpp} kept around but unused)

notes/                           Development docs (not user-facing)
├── MONETIZATION.md              Strategy: pricing, channels, license model
├── LAUNCH_CHECKLIST.md          Day-of operational sequence for paid launch
└── CLAUDE_DESIGN_PROMPT.md      Prompt for designing cliptozero.com

dist/                            Per-OS packaging scripts + install guide
website/                         Cloudflare Pages source for cliptozero.com
build/                           Free / dev build (auto-installs to ~/Library)
build-paid/                      Paid / demo build (does NOT auto-install)
```

---

## Manual test recipes

These cover the cases `auval` can't:

**Multi-instance Link Bypass (Pre-flight for any Link Bypass change)**

1. Drop 3 ClipToZero instances on different tracks.
2. Click the chevron right of BYPASS on each, enable Link Bypass.
3. Verify lime chain icon appears on each next to BYPASS.
4. Click BYPASS on any one → all three should toggle in unison.
5. **No freeze.** The host should remain responsive.
6. Click "Disable Link Bypass on all instances" — chains should all go off.

**OS factor sweep (Pre-flight for any GR / scope change)**

1. Hot drum-bus material, OS at Off. Verify GR=0 with drive=0; GR shows
   real reduction with drive=6.
2. Switch OS to 2x / 4x / 8x. Verify the same on each — GR=0 at drive=0,
   real GR at drive=6.
3. Toggle factor mid-playback. Verify no audio glitches beyond the
   ~32-sample OS-FIR warm-up period.

**Clip-curve sweep**

1. Same material. Cycle through Hard / Soft / Poly / Tube.
2. Verify the scope's CLIPPED red region tracks the actual shaving.
3. Verify the harmonic spectrum (VIEW → Spectrum: Bold) changes
   character with the curve.

**Preset round-trip**

1. Save a project with each preset applied.
2. Reopen — verify parameter values restored exactly.
3. Verify presets preserve Input Gain (this is the v0.5.3 contract).

**Auto-Gain accuracy**

1. Loud material, peak ~+4 dB.
2. Target = 0 dB. Press Auto-Gain.
3. Verify Input Gain ends at ~-4 dB and the captured peak readout
   matches.

---

## Release process

```sh
git tag v0.X.Y                  # annotated tag with message; see git log -1
git push origin main
git push origin v0.X.Y
```

Pushing a `v*` tag fires `.github/workflows/build.yml`, which:

- Builds macOS / Windows / Linux in parallel via the matrix
- Runs `dist/package-*` per platform
- Attaches each platform's zip to a GitHub Release

The one-liner installer (`install.sh`) queries the latest-release API
on every run, so users get the new version automatically the next time
they execute the curl pipe-to-sh.

**Convention**: every tag is **annotated** (`git tag -a`) with a message
listing the changes since the previous tag. `git tag -n99` gives a full
release tour. Lightweight tags (without -a) are a documentation gap —
don't make them.

**Convention**: every commit message has a substantive body. Subject is
`Topic: short imperative description`. Body explains why and what,
links cause to effect, calls out trade-offs. This makes `git log`
readable as project history.

---

## Things I keep forgetting that I shouldn't

- **Check `git status` before committing.** The `.DS_Store` file in
  `dist/` shows up regularly; it's gitignored at root but still appears
  in `?? `. Ignore it; don't add.
- **`auval` returns 0 even when sub-targets print BUILD FAILED.** The
  final `** BUILD SUCCEEDED **` and `AU VALIDATION SUCCEEDED` lines are
  the trustworthy signals. `grep "error:"` can false-positive on benign
  output.
- **The PostToolUse formatter sometimes reformats Markdown.** Read a
  file after Write if I plan another Edit on it; the formatter pass can
  shift line numbers.
- **`git pull` silently produces no output when already in sync.** Empty
  stdout/stderr is normal, not an error.
- **macOS-style Library path:** `~/Library/Audio/Plug-Ins/VST3` and
  `~/Library/Audio/Plug-Ins/Components` (note the hyphen + capitalised
  "Plug-Ins").
