# ClipToZero — Install Guide

A clipper plugin for the Clip-to-Zero gain-staging workflow.
Three numbered stages: stage your input to 0 dBFS, drive into the clipper, judge the loudness.

This zip contains the binaries for **one platform** — the filename tells you which:

- `ClipToZero-vX.Y.Z-mac.zip` → universal Apple Silicon + Intel macOS build
- `ClipToZero-vX.Y.Z-windows.zip` → 64-bit Windows VST3
- `ClipToZero-vX.Y.Z-linux.zip` → 64-bit Linux VST3

Pick the section below that matches the zip you downloaded.

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

### 1. Drop the VST3 into the right folder

Inside the zip you'll find `VST3\ClipToZero.vst3` (a folder bundle, not a single file). Copy it to **one** of these locations:

| Folder                                 | Notes                           |
| -------------------------------------- | ------------------------------- |
| `C:\Program Files\Common Files\VST3\`  | System-wide; needs admin rights |
| `%LOCALAPPDATA%\Programs\Common\VST3\` | User-only; no admin needed      |

Most DAWs scan both. Pick whichever you can write to.

### 2. (Optional) Standalone app

`Standalone\ClipToZero.exe` runs without a DAW — useful for quick testing. Copy it anywhere and double-click.

### 3. Rescan plugins in your DAW

- **Ableton Live**: Preferences → Plug-Ins → Rescan Plug-Ins.
- **Cubase / Reaper / FL Studio**: each has a "Rescan VST3" option in plugin preferences.

### Windows SmartScreen warning

The first time you run the standalone, Windows may show a "Microsoft Defender SmartScreen prevented an unrecognised app from starting" dialog. Click **More info** → **Run anyway**. Same root cause as macOS Gatekeeper: the binary isn't signed with a paid certificate.

VST3 plugins loaded by your DAW don't trigger this dialog because the DAW is the running app, not the plugin.

---

## Linux install

### 1. Drop the VST3 into the right folder

Inside the zip:

```sh
mkdir -p ~/.vst3
cp -r VST3/ClipToZero.vst3 ~/.vst3/
```

For a system-wide install: `sudo cp -r VST3/ClipToZero.vst3 /usr/lib/vst3/`.

### 2. (Optional) Standalone

`Standalone/ClipToZero` is a standalone executable. Make it executable and run:

```sh
chmod +x Standalone/ClipToZero
./Standalone/ClipToZero
```

### 3. Rescan in your DAW

Bitwig / Reaper / Ardour all have plugin rescan in their preferences — point at `~/.vst3` and rescan.

---

## Troubleshooting

**Plugin doesn't appear after rescan.**

- macOS: check the quarantine flag is cleared (`xattr -l <path>` should show no `com.apple.quarantine` line).
- Windows: confirm the `.vst3` folder is directly under `Common Files\VST3\` and not nested an extra level deep.
- Linux: confirm the file mode is readable (`ls -l ~/.vst3/ClipToZero.vst3`).

**DAW crashes on plugin scan.** Open an issue on the GitHub repo with your DAW name and version, OS version, and the contents of `BUILD_INFO.txt` (next to this file). On macOS, also include the most recent crash file from `~/Library/Logs/DiagnosticReports/`.

**Plugin loads but produces silence.** Check that Bypass isn't accidentally on (top-right toggle in the plugin GUI), and that Output Trim isn't all the way down.

---

## Build provenance

`BUILD_INFO.txt` next to this file records the exact version, git commit, and build host that produced these binaries. Useful when reporting bugs — paste its contents into any issue.

For source builds and the full per-OS toolchain walkthrough, see `BUILD-FROM-SOURCE.md` in the GitHub repo:

<https://github.com/stephengeller/ableton-projects/blob/main/plugins/ClipToZero/dist/BUILD-FROM-SOURCE.md>
