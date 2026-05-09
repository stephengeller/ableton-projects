# Building ClipToZero from source

This document covers building the plugin yourself on **macOS**, **Windows**, and **Linux**.
The build is the same JUCE 8 + CMake setup on every OS — only the prerequisites and the cmake generator differ.

JUCE 8.0.4 itself is fetched automatically by CMake on first configure (≈ 80 MB shallow clone of <https://github.com/juce-framework/JUCE>). No manual JUCE setup needed.

---

## macOS

### Prerequisites

- Xcode (full install, not just Command Line Tools — the AU SDK headers come from Xcode)
- Homebrew (<https://brew.sh>)
- CMake: `brew install cmake`

### Build

```sh
git clone https://github.com/stephengeller/ableton-projects
cd ableton-projects/plugins/ClipToZero
cmake -B build -G Xcode
cmake --build build --config Release
```

First run takes 3–5 minutes (JUCE clone + module compile). Subsequent rebuilds are seconds.

### Output

The built artefacts:

- VST3: `build/ClipToZero_artefacts/Release/VST3/ClipToZero.vst3`
- AU: `build/ClipToZero_artefacts/Release/AU/ClipToZero.component`
- Standalone: `build/ClipToZero_artefacts/Release/Standalone/ClipToZero.app`

`COPY_PLUGIN_AFTER_BUILD TRUE` is set in `CMakeLists.txt`, so the build also auto-installs to `~/Library/Audio/Plug-Ins/{VST3,Components}/`.

After building, clear the macOS quarantine flag and rescan in your DAW (see `INSTALL.md`).

---

## Windows

### Prerequisites

- Git for Windows: <https://git-scm.com/download/win>
- Visual Studio 2022 (Community Edition is free): <https://visualstudio.microsoft.com/downloads/>
  - During installation, make sure "Desktop development with C++" workload is selected
- CMake: <https://cmake.org/download/> (or `winget install Kitware.CMake`)

### Build

Open a **Developer Command Prompt for VS 2022** (not regular cmd — this one has the compiler in its PATH):

```bat
git clone https://github.com/stephengeller/ableton-projects
cd ableton-projects\plugins\ClipToZero
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Output

The built VST3 will be at:

```
build\ClipToZero_artefacts\Release\VST3\ClipToZero.vst3
```

Copy that folder (it's a bundle) to **one** of these locations:

- `C:\Program Files\Common Files\VST3\` — system-wide install (requires admin)
- `%LOCALAPPDATA%\Programs\Common\VST3\` — user-only install

Then in your DAW: rescan plugins. In Ableton Live: Preferences → Plug-Ins → Rescan Plug-Ins.

### Notes

- AU and Standalone targets are macOS-only; on Windows the build only produces VST3.
- If CMake configure fails saying it can't find Visual Studio, double-check you're in the **Developer Command Prompt** and `cl.exe` is in your PATH.

---

## Linux

### Prerequisites (Ubuntu / Debian)

```sh
sudo apt update
sudo apt install -y git cmake g++ \
    libasound2-dev libjack-jackd2-dev \
    ladspa-sdk libcurl4-openssl-dev libfreetype-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev \
    libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev
```

(JUCE has a long list of X11 dev dependencies — these are them.)

### Build

```sh
git clone https://github.com/stephengeller/ableton-projects
cd ableton-projects/plugins/ClipToZero
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Output

```
build/ClipToZero_artefacts/Release/VST3/ClipToZero.vst3
```

Copy to `~/.vst3/` (user) or `/usr/lib/vst3/` (system, requires sudo). Rescan in your DAW.

---

## Troubleshooting all platforms

**`cmake -B build` fails immediately about FetchContent / git not found.**
Make sure git is on your PATH. CMake clones JUCE with a shallow `git clone`.

**Build succeeds but the plugin doesn't show up in the DAW.**

- Check it landed in the right folder for your DAW (paths above per OS).
- macOS: clear the quarantine flag (`xattr -dr com.apple.quarantine /path/to/ClipToZero.vst3`).
- Most DAWs need a **forced rescan** for newly-built plugins (in Ableton: hold **Option/Alt** while clicking Rescan).

**Build is slow.** First-time builds are 3–5 minutes because the entire JUCE framework has to compile. Incrementals are seconds. Use Ninja (`cmake -G Ninja`) for slightly faster Linux/macOS rebuilds.

**JUCE version mismatch / missing methods.**
Check `CMakeLists.txt` for the pinned version (currently `GIT_TAG 8.0.4`). Delete `build/` and reconfigure if you've been switching JUCE versions.

---

## Validation (macOS only)

Apple's `auval` tool runs the strict AU conformance suite — useful as a smoke test that the build is healthy:

```sh
auval -v aufx Cz01 Sgel
```

Look for `AU VALIDATION SUCCEEDED` at the end. Failures usually indicate audio-thread issues (allocations during render, denormals leaking, parameter range violations).
