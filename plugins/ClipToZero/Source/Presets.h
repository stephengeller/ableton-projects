#pragma once

#include <juce_core/juce_core.h>

// Factory presets shipped with ClipToZero. The dropdown in the brand bar
// lists these in display order. Each preset sets SIX audio-shaping
// parameters; everything else is user/UI state that presets deliberately
// leave alone.
//
// Explicitly preserved across preset changes:
//   * inputGain  -- the user's Auto-Gain result lives here. Wiping it on
//                   every preset swap would destroy the staging the user
//                   already did with Auto-Gain (or by hand). Presets are
//                   about the post-staging clipper personality, not about
//                   what level you're feeding into it.
//   * bypass, gainMatch, linkBypass, scope window, headroom, spectrum
//     mode, hint visibility -- UI / behaviour preferences. Applying 'Drum
//     Bus' to a linked-bypass setup must not un-link it, etc.
//
// Adding a preset: append a row to kPresets below. The popup menu walks
// the array in order; index 0 is reserved for 'Init' (the restore-
// defaults affordance).
//
// Parameter slot is documented inline. clipTypeIdx and osFactorIdx match
// the Parameters.h string-array ordering:
//   clipType: 0=Hard, 1=Soft, 2=Poly, 3=Tube
//   osFactor: 0=Off,  1=2x,   2=4x,   3=8x

struct Preset {
    juce::String name;
    juce::String description;  // tooltip text in the menu

    float targetPeak;          // -12.0 .. 0.0   dBFS
    float drive;               //   0.0 .. 24.0  dB
    int   clipTypeIdx;         //   0 .. 3
    int   osFactorIdx;         //   0 .. 3
    float outputTrim;          // -12.0 .. 12.0  dB
    float preClipHpfHz;        // 20.0  .. 500.0 Hz  (20 = off)
};

inline const Preset kPresets[] = {
    // ---- Init: reset audio-shaping params to factory default. ---------
    // (inputGain is NOT touched -- whatever the user staged with
    //  Auto-Gain stays.)
    { "Init",
      "Reset clipper personality controls to factory defaults. Input "
      "gain is preserved -- your Auto-Gain staging stays.",
      /*targetPeak*/  0.0f,
      /*drive*/       0.0f,
      /*clipType*/    0,         // Hard
      /*osFactor*/    0,         // Off
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  20.0f      // OFF position
    },

    // ---- Drum Bus -----------------------------------------------------
    { "Drum Bus",
      "Hard clip + 4 dB drive. Sub-cut at 35 Hz so rumble doesn't eat "
      "your ceiling headroom. No oversampling -- aliasing on drums is "
      "barely audible and keeping it off saves CPU when you're stacking "
      "this across kick, snare, and the bus.",
      /*targetPeak*/ -1.0f,
      /*drive*/       4.0f,
      /*clipType*/    0,
      /*osFactor*/    0,
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  35.0f
    },

    // ---- Vocal --------------------------------------------------------
    { "Vocal",
      "Soft (tanh) clip with 2 dB drive. HPF at 80 Hz cleans up "
      "proximity-effect rumble. 2x oversampling keeps the upper "
      "register clean without much CPU cost.",
      /*targetPeak*/  0.0f,
      /*drive*/       2.0f,
      /*clipType*/    1,         // Soft (tanh)
      /*osFactor*/    1,         // 2x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  80.0f
    },

    // ---- Bass ---------------------------------------------------------
    { "Bass",
      "Tube (asymmetric) clip with 3 dB drive. Even-harmonic colour "
      "warms up the low end. No HPF -- bass needs its lows. 2x "
      "oversampling is plenty for bass-frequency content.",
      /*targetPeak*/  0.0f,
      /*drive*/       3.0f,
      /*clipType*/    3,         // Tube
      /*osFactor*/    1,         // 2x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  20.0f
    },

    // ---- Synth Tame ---------------------------------------------------
    { "Synth Tame",
      "Poly (cubic soft knee) clip with 3 dB drive. Smooths hot synth "
      "transients without obvious distortion artefacts. HPF at 80 Hz "
      "cleans low rumble from sub-osc leakage.",
      /*targetPeak*/  0.0f,
      /*drive*/       3.0f,
      /*clipType*/    2,         // Poly
      /*osFactor*/    1,         // 2x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  80.0f
    },

    // ---- Master Subtle ------------------------------------------------
    { "Master Subtle",
      "Hard clip with 1 dB drive at 4x oversampling. Adds +1-2 dB of "
      "apparent loudness almost transparently. Target -0.3 dBFS leaves "
      "TP headroom so the streaming codec's reconstruction doesn't "
      "push past 0 dBTP.",
      /*targetPeak*/ -0.3f,
      /*drive*/       1.0f,
      /*clipType*/    0,         // Hard
      /*osFactor*/    2,         // 4x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  25.0f
    },

    // ---- Master Loud --------------------------------------------------
    { "Master Loud",
      "Hard clip with 4 dB drive at 4x oversampling. Pushes integrated "
      "LUFS up toward the -9 dBFS range streaming services normalise "
      "to. Target -0.5 dBFS for TP safety; HPF 30 Hz protects more "
      "headroom under heavier drive.",
      /*targetPeak*/ -0.5f,
      /*drive*/       4.0f,
      /*clipType*/    0,
      /*osFactor*/    2,         // 4x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  30.0f
    },

    // ---- Surgical (Mastering) -----------------------------------------
    { "Surgical",
      "Hard clip with barely-there 0.5 dB drive at 8x oversampling. "
      "For mastering passes where you want maximum-quality peak taming "
      "without colour. Use last in the chain.",
      /*targetPeak*/  0.0f,
      /*drive*/       0.5f,
      /*clipType*/    0,
      /*osFactor*/    3,         // 8x
      /*outputTrim*/  0.0f,
      /*preClipHpf*/  20.0f
    },
};

inline constexpr int kNumPresets = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
