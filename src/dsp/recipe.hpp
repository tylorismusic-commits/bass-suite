// Bass Suite — bass amp recipes
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The stage structure is the production structure; the values are first-pass
// bass voicings to be tuned against reference recordings.  Display names are
// generic; real products are only referenced in the docs.

#pragma once

#include "tube_stage.hpp"
#include "tone_stack.hpp"

namespace bass {

enum class ToneStackKind {
  Studio,
  FlipTop,
  SVT,
  Modern
};

inline ToneStackSpec toneStackSpec(ToneStackKind kind) noexcept {
  switch (kind) {
    case ToneStackKind::Studio:
      // Broad, clean DI voicing.
      return {250000.0f, 1000000.0f, 25000.0f, 100000.0f, 250.0e-12f,
              22.0e-9f, 22.0e-9f};
    case ToneStackKind::FlipTop:
      // Warm, mid-forward vintage bass.
      return {250000.0f, 1000000.0f, 25000.0f, 82000.0f, 270.0e-12f,
              47.0e-9f, 47.0e-9f};
    case ToneStackKind::SVT:
      // Big low end, firm mids.
      return {250000.0f, 1000000.0f, 25000.0f, 100000.0f, 250.0e-12f,
              100.0e-9f, 47.0e-9f};
    case ToneStackKind::Modern:
      // Brighter, aggressive upper mids.
      return {250000.0f, 1000000.0f, 25000.0f, 68000.0f, 470.0e-12f,
              22.0e-9f, 22.0e-9f};
  }
  return {};
}

struct AmpRecipe {
  const char* name;
  int stages;
  StageSpec stage[6];
  ToneStackKind tone;
  float preGain;
  float powerDrive;
  float powerFeedback;
  float powerSag;
  float outputTrim;
  float makeupDb;
  bool powerTable;
  bool solidState;
};

inline const AmpRecipe& recipeFor(int model) noexcept {
  static const StageSpec s1 =
      StageSpec{TubeTable::k12AX7_68k, 250.0f, 40.0f, 86.0f, 2700.0f,
                1.581656f, 100000.0f};
  static const StageSpec s2 =
      StageSpec{TubeTable::k12AX7_250k, 250.0f, 40.0f, 132.0f, 1500.0f,
                1.204285f, 100000.0f};
  static const StageSpec s3 =
      StageSpec{TubeTable::k12AX7_68k, 250.0f, 40.0f, 120.0f, 1000.0f,
                1.0f, 100000.0f};
  static const StageSpec s4 =
      StageSpec{TubeTable::k12AX7_68k, 250.0f, 40.0f, 100.0f, 820.0f,
                0.9f, 100000.0f};

  static const AmpRecipe recipes[4] = {
      // Studio DI: clean solid-state path, no cab colour by default.
      {"Studio DI", 0, {}, ToneStackKind::Studio, 1.20f, 0.0f, 0.0f, 0.0f,
       0.85f, 8.0f, false, true},
      // Flip Top: warm two-stage vintage tube.
      {"Flip Top", 2, {s1, s2}, ToneStackKind::FlipTop, 0.50f, 0.75f, 0.20f,
       0.18f, 0.80f, 16.0f, false, false},
      // SVT: three-stage big clean tube.
      {"SVT", 3, {s1, s2, s3}, ToneStackKind::SVT, 0.42f, 0.95f, 0.25f,
       0.12f, 0.72f, 18.0f, false, false},
      // Modern: four-stage aggressive upper-mid voice.
      {"Modern", 4, {s1, s2, s3, s4}, ToneStackKind::Modern, 0.60f, 1.15f,
       0.10f, 0.08f, 0.78f, 16.0f, false, false},
  };
  const int m = model < 0 ? 0 : (model > 3 ? 3 : model);
  return recipes[m];
}

} // namespace bass
