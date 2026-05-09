# ClipToZero — Install Guide

A clipper plugin for the Clip-to-Zero gain-staging workflow.
Three numbered stages: stage your input to 0 dBFS, drive into the clipper, judge the loudness.

This package contains the **macOS** build (universal: Apple Silicon + Intel).
For **Windows** or **Linux**, see "Building from source" at the bottom.

---

## macOS install

### 1. Drop the plugins into the right folders

In Finder, hit **⇧⌘G** ("Go to folder") and paste these paths one at a time, dragging the matching plugin in:

| Plugin                      | Goes in                                | Used by                       |
| --------------------------- | -------------------------------------- | ----------------------------- |
| `VST3/ClipToZero.vst3`      | `~/Library/Audio/Plug-Ins/VST3/`       | Ableton, Cubase, Reaper, etc. |
| `AU/ClipToZero.component`   | `~/Library/Audio/Plug-Ins/Components/` | Logic, GarageBand, MainStage  |
| `Standalone/ClipToZero.app` | Applications (or wherever)             | Run without a DAW             |

You only need the one(s) your DAW uses.

### 2. Clear the macOS quarantine flag (REQUIRED)

ClipToZero isn't signed with a paid Apple Developer ID, so macOS marks it as untrusted on download. Without this step, your DAW will silently skip it.

Open Terminal and paste:

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/ClipToZero.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/ClipToZero.component
```

(Skip whichever line corresponds to a plugin format you didn't install.)

If you also want to use the standalone app:

```sh
xattr -dr com.apple.quarantine /Applications/ClipToZero.app
```

### 3. Rescan plugins in your DAW

- **Ableton Live**: Preferences → Plug-Ins → Rescan Plug-Ins (hold **Option** while clicking for a forced rescan).
- **Logic Pro**: it auto-rescans AUs on next launch. If it doesn't show up, run `auval -v aufx Cz01 Sgel` in Terminal — if that prints `AU VALIDATION SUCCEEDED`, the plugin is fine and you may need to reset Logic's AU cache.
- **Reaper**: Preferences → Plug-Ins → VST → "Re-scan".

You should find the plugin under manufacturer **stephengeller** in your DAW's plugin browser.

### Why the security warning?

Free indie plugins like this one don't have an Apple Developer ID ($99/year). The `xattr` command tells macOS "yes, I trust this binary" by removing the quarantine attribute that the OS attaches to anything downloaded from the internet. It's normal for free plugins.

---

## Windows install

This zip doesn't contain Windows binaries. Two paths:

### Option A — wait for a pre-built release

Pre-built Windows binaries will be published as GitHub Releases once the CI pipeline is set up. Check:

<https://github.com/stephengeller/ableton-projects/releases>

### Option B — build from source (~10 minutes)

You need: Git, Visual Studio 2022 (Community is free), CMake.

```bat
git clone https://github.com/stephengeller/ableton-projects
cd ableton-projects\plugins\ClipToZero
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

The plugin will be at `build\ClipToZero_artefacts\Release\VST3\ClipToZero.vst3`.
Copy it to `C:\Program Files\Common Files\VST3\` and restart your DAW.

Full instructions and troubleshooting are in `BUILD-FROM-SOURCE.md` in the source repo.

---

## Linux install

Same path as Windows option B, but with `cmake -B build` (default Make/Ninja generator) and `cmake --build build --config Release`. The `.vst3` lives at `build/ClipToZero_artefacts/Release/VST3/ClipToZero.vst3` — drop into `~/.vst3/`.

---

## Troubleshooting

**Plugin doesn't appear after rescan.** Check the quarantine flag is cleared (`xattr -l <path>` should show no `com.apple.quarantine` line). Also check the plugin actually got copied to the right folder.

**DAW crashes on plugin scan.** Open an issue on the GitHub repo with your DAW name and version, macOS version, and the contents of `~/Library/Logs/DiagnosticReports/<your-DAW>*.crash` (most recent file).

**Plugin loads but produces silence.** Check that Bypass isn't accidentally on (top-right toggle in the plugin GUI), and that Output Trim isn't all the way down.

---

## Build provenance

See `BUILD_INFO.txt` next to this file for the exact version, git commit, and build host that produced the binaries you're holding. Useful when reporting bugs.
