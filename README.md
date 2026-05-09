# ableton-projects

Personal collection of audio tooling I've built for use in Ableton Live —
VSTs, Max for Live devices, and assorted tinkering.

## Structure

| Directory                        | What's in it                                                           |
| -------------------------------- | ---------------------------------------------------------------------- |
| [`plugins/`](plugins/)           | Native VST3 / AU plugins built from C++ with [JUCE](https://juce.com). |
| [`max-for-live/`](max-for-live/) | Max for Live (`.amxd`) devices.                                        |

More categories will appear here as I add them (effect racks, instrument
racks, sample packs, drum kits, etc.).

## Plugins

| Plugin                           | Format                 | Purpose                                                                                                                                                                                                                                               |
| -------------------------------- | ---------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [ClipToZero](plugins/ClipToZero) | VST3 / AU / Standalone | Clip-to-zero workflow utility — peak/RMS metering, auto-gain to a target peak, drive into a hard/soft clipper at 0 dBFS, oscilloscope with pre/post overlay, ITU-R BS.1770 / EBU R128 LUFS metering. Replaces the dpMeter5 + GClip combo I was using. |

## Max for Live

(none yet)

## Building plugins

Each plugin in `plugins/` is a standalone JUCE/CMake project — see its own
README for build instructions. The general pattern on macOS:

```sh
cd plugins/<name>
brew install cmake          # one-time
cmake -B build -G Xcode
cmake --build build --config Release
```

JUCE is fetched automatically by CMake on first build, and successful Release
builds auto-install to `~/Library/Audio/Plug-Ins/{VST3,Components}/`.

## License

MIT — see [LICENSE](LICENSE). Individual subprojects may pin to JUCE under
its own licence (GPL3 for free use, commercial licence otherwise).
