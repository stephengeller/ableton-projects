#pragma once

#include <juce_core/juce_core.h>

// Factory presets shipped with ClipToZero. The dropdown in the brand bar
// lists these in display order. Each preset sets seven audio-shaping
// parameters; everything else (bypass, gain-match, link-bypass, scope
// window, headroom, spectrum mode, hint visibility) is user/UI state
// that presets deliberately leave alone -- so applying 'Drum Bus' to a
// linked-bypass setup doesn't surprise the user by un-linking it, etc.
//
// Adding a preset: append a row to kPresets below. The popup menu walks
// the array in order; index 0 is what 'Init' typically lives at and is
// the reset target if you want a 'restore defaults' affordance.
//
// Parameter slot is documented inline. clipTypeIdx and osFactorIdx match
// the Parameters.h string-array ordering:
//   clipType: 0=Hard, 1=Soft, 2=Poly, 3=Tube
//   osFactor: 0=Off,  1=2x,   2=4x,   3=8x

struct Preset {
    juce::String name;
    juce::String description;  // tooltip text in the menu

    float targetPeak;          // -12.0 .. 0.0   dBFS
    float inputGain;           // -24.0 .. 24.0  dB
    float drive;               //   0.0 .. 24.0  dB
    int   clipTypeIdx;         //   0 .. 3
    int   osFactorIdx;         //   0 .. 3
    float outputTrim;          // -12.0 .. 12.0  dB
    float preClipHpfHz;        // 20.0  .. 500.0 Hz  (20 = off)
};

inline const Preset kPresets[] = {
    // ---- Init: reset every audio-shaping param to factory default. ----
    { "Init",
      "Reset all controls to factory defaults.",
      /*targetPeak*/  0.0f,
      /*inputGain*/   0.0f,
      /*drive*/       0.0f,
      /*clipType*/    0,         // Hard
      /*osFactor*/    0,         // Off
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  20.0f      // OFF position
    },

    // ---- Drum Bus -----------------------------------------------------
    // Aggressive transient taming on the kick/snare bus. Hard clip
    // preserves the snap; sub-cut at 35 Hz removes thump-rumble that
    // wastes headroom. No oversampling -- aliasing isn't audible on
    // percussive material and the CPU saving matters when stacked
    // across many drum tracks.
    { "Drum Bus",
      "Hard clip + 4 dB drive. Sub-cut at 35 Hz so rumble doesn't eat "
      "your ceiling headroom. No oversampling -- aliasing on drums is "
      "barely audible and keeping it off saves CPU when you're stacking "
      "this across kick, snare, and the bus.",
      /*targetPeak*/ -1.0f,
      /*inputGain*/   0.0f,
      /*drive*/       4.0f,
      /*clipType*/    0,
      /*osFactor*/    0,
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  35.0f
    },

    // ---- Vocal --------------------------------------------------------
    // Gentle saturation that preserves vocal character. Soft (tanh)
    // curve adds harmonics smoothly without the snap of hard clipping;
    // 2x oversampling cheaply suppresses aliasing in the upper register
    // where vocals live. HPF at 80 Hz cuts proximity-effect chest rumble.
    { "Vocal",
      "Soft (tanh) clip with 2 dB drive. HPF at 80 Hz cleans up "
      "proximity-effect rumble. 2x oversampling keeps the upper "
      "register clean without much CPU cost.",
      /*targetPeak*/  0.0f,
      /*inputGain*/   0.0f,
      /*drive*/       2.0f,
      /*clipType*/    1,         // Soft (tanh)
      /*osFactor*/    1,         // 2x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  80.0f
    },

    // ---- Bass ---------------------------------------------------------
    // Adds warmth + tames sub spikes. Tube (asymmetric tanh) produces
    // even-harmonic content which thickens low end. No HPF -- bass
    // needs the lows. 2x oversampling is enough; bass content is well
    // below Nyquist even at 44.1 kHz.
    { "Bass",
      "Tube (asymmetric) clip with 3 dB drive. Even-harmonic colour "
      "warms up the low end. No HPF -- bass needs its lows. 2x "
      "oversampling is plenty for bass-frequency content.",
      /*targetPeak*/  0.0f,
      /*inputGain*/   0.0f,
      /*drive*/       3.0f,
      /*clipType*/    3,         // Tube
      /*osFactor*/    1,         // 2x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  20.0f
    },

    // ---- Synth Tame ---------------------------------------------------
    // Smoothing hot synth transients without obvious distortion. Poly
    // (cubic soft knee) gives a gentle rolloff into clipping that
    // sounds rounder than hard clip on sustained synth tones. HPF
    // optional but useful on bright leads.
    { "Synth Tame",
      "Poly (cubic soft knee) clip with 3 dB drive. Smooths hot synth "
      "transients without obvious distortion artefacts. HPF at 80 Hz "
      "cleans low rumble from sub-osc leakage.",
      /*targetPeak*/  0.0f,
      /*inputGain*/   0.0f,
      /*drive*/       3.0f,
      /*clipType*/    2,         // Poly
      /*osFactor*/    1,         // 2x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  80.0f
    },

    // ---- Master Subtle ------------------------------------------------
    // Transparent loudness push for the master bus. Very gentle drive
    // (1 dB) + Hard clip + 4x oversampling = a clean +1-2 dB loudness
    // bump with minimal harmonic distortion. Target peak at -0.3 leaves
    // a sliver of TP headroom for streaming codec safety.
    { "Master Subtle",
      "Hard clip with 1 dB drive at 4x oversampling. Adds +1-2 dB of "
      "apparent loudness almost transparently. Target -0.3 dBFS leaves "
      "TP headroom so the streaming codec's reconstruction doesn't "
      "push past 0 dBTP.",
      /*targetPeak*/ -0.3f,
      /*inputGain*/   0.0f,
      /*drive*/       1.0f,
      /*clipType*/    0,         // Hard
      /*osFactor*/    2,         // 4x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  25.0f
    },

    // ---- Master Loud --------------------------------------------------
    // Streaming-loud target. More drive (4 dB) pushes integrated LUFS
    // up toward the -10/-9 range that streaming services normalise to.
    // Still Hard clip + 4x OS for clean, transparent clipping. HPF a
    // bit higher to protect more headroom under heavier drive.
    { "Master Loud",
      "Hard clip with 4 dB drive at 4x oversampling. Pushes integrated "
      "LUFS up toward the -9 dBFS range streaming services normalise "
      "to. Target -0.5 dBFS for TP safety; HPF 30 Hz protects more "
      "headroom under heavier drive.",
      /*targetPeak*/ -0.5f,
      /*inputGain*/   0.0f,
      /*drive*/       4.0f,
      /*clipType*/    0,
      /*osFactor*/    2,         // 4x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  30.0f
    },

    // ---- Surgical (Mastering) -----------------------------------------
    // For mastering passes where you want to tame the absolute peak
    // transients with maximum possible quality. 8x oversampling
    // suppresses aliasing artefacts to inaudible levels. Drive 0.5 dB
    // -- the clipper is barely tickling the ceiling. Used at the
    // VERY end of a chain after everything else is set.
    { "Surgical",
      "Hard clip with barely-there 0.5 dB drive at 8x oversampling. "
      "For mastering passes where you want maximum-quality peak taming "
      "without colour. Use last in the chain.",
      /*targetPeak*/  0.0f,
      /*inputGain*/   0.0f,
      /*drive*/       0.5f,
      /*clipType*/    0,
      /*osFactor*/    3,         // 8x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  20.0f
    },
};

inline constexpr int kNumPresets = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
