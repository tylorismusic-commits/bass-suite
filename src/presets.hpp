// Bass Suite — factory presets
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

#include "params.hpp"

namespace bass {

// A preset sets the parameters it cares about; -1 means "leave as is".
struct PresetValue {
  int16_t param = -1;
  float value = 0.0f;
};

struct Preset {
  const char* name;
  const char* category;
  int model;
  PresetValue values[48];
};

inline constexpr Preset kPresets[] = {
    // --- Classic -----------------------------------------------------------
    {"SVT Classic", "Classic", 2,
     {{P_GAIN, 0.46f}, {P_BASS, 0.58f}, {P_MID, 0.46f}, {P_TREBLE, 0.52f},
      {P_PRESENCE, 0.46f}, {P_RESONANCE, 0.55f}, {P_MASTER, 0.50f},
      {P_OUTPUT, 0.50f}, {P_CAB, 0.0f}, {P_CAB_SET, 3.0f},
      {P_CAB_MIC, 0.0f}, {P_CAB_LEVEL, 0.62f}, {P_CAB_BYPASS, 0.0f},
      {P_COMP_ON, 1.0f}, {P_COMP_AMOUNT, 0.30f}, {P_COMP_ATTACK, 0.35f},
      {P_COMP_RELEASE, 0.45f}, {P_COMP_MIX, 0.80f},
      {P_OD_ON, 0.0f}, {P_OD_DRIVE, 0.20f}, {P_OD_TONE, 0.50f},
      {P_OD_LEVEL, 0.55f}, {P_OD_MIX, 0.60f},
      {P_GATE_ON, 0.0f}, {P_LIMITER_ON, 1.0f}, {P_LIMITER_THRESH, 0.90f},
      {P_NAM_ON, 0.0f}, {-1, 0.0f}}},
    {"Flip Top Warm", "Classic", 1,
     {{P_GAIN, 0.48f}, {P_BASS, 0.62f}, {P_MID, 0.55f}, {P_TREBLE, 0.42f},
      {P_PRESENCE, 0.40f}, {P_RESONANCE, 0.55f}, {P_MASTER, 0.52f},
      {P_OUTPUT, 0.50f}, {P_CAB, 0.0f}, {P_CAB_SET, 0.0f},
      {P_CAB_MIC, 1.0f}, {P_CAB_LEVEL, 0.62f}, {P_CAB_BYPASS, 0.0f},
      {P_COMP_ON, 1.0f}, {P_COMP_AMOUNT, 0.35f}, {P_COMP_ATTACK, 0.45f},
      {P_COMP_RELEASE, 0.50f}, {P_COMP_MIX, 0.85f},
      {P_OD_ON, 0.0f}, {P_OD_DRIVE, 0.15f}, {P_OD_TONE, 0.45f},
      {P_OD_LEVEL, 0.55f}, {P_OD_MIX, 0.55f},
      {P_GATE_ON, 0.0f}, {P_LIMITER_ON, 1.0f}, {P_LIMITER_THRESH, 0.90f},
      {P_NAM_ON, 0.0f}, {-1, 0.0f}}},
    {"Clean DI", "Clean", 0,
     {{P_GAIN, 0.50f}, {P_BASS, 0.50f}, {P_MID, 0.50f}, {P_TREBLE, 0.52f},
      {P_PRESENCE, 0.50f}, {P_RESONANCE, 0.50f}, {P_MASTER, 0.55f},
      {P_OUTPUT, 0.50f}, {P_CAB, 0.0f}, {P_CAB_SET, 0.0f},
      {P_CAB_MIC, 0.0f}, {P_CAB_LEVEL, 0.60f}, {P_CAB_BYPASS, 1.0f},
      {P_COMP_ON, 1.0f}, {P_COMP_AMOUNT, 0.20f}, {P_COMP_ATTACK, 0.30f},
      {P_COMP_RELEASE, 0.40f}, {P_COMP_MIX, 0.70f},
      {P_OD_ON, 0.0f}, {P_OD_DRIVE, 0.10f}, {P_OD_TONE, 0.50f},
      {P_OD_LEVEL, 0.55f}, {P_OD_MIX, 0.50f},
      {P_GATE_ON, 0.0f}, {P_LIMITER_ON, 1.0f}, {P_LIMITER_THRESH, 0.92f},
      {P_NAM_ON, 0.0f}, {-1, 0.0f}}},
    {"Motown", "Classic", 1,
     {{P_GAIN, 0.42f}, {P_BASS, 0.58f}, {P_MID, 0.58f}, {P_TREBLE, 0.38f},
      {P_PRESENCE, 0.35f}, {P_RESONANCE, 0.60f}, {P_MASTER, 0.55f},
      {P_OUTPUT, 0.50f}, {P_CAB, 0.0f}, {P_CAB_SET, 0.0f},
      {P_CAB_MIC, 1.0f}, {P_CAB_LEVEL, 0.60f}, {P_CAB_BYPASS, 0.0f},
      {P_COMP_ON, 1.0f}, {P_COMP_AMOUNT, 0.45f}, {P_COMP_ATTACK, 0.55f},
      {P_COMP_RELEASE, 0.55f}, {P_COMP_MIX, 0.90f},
      {P_OD_ON, 0.0f}, {P_OD_DRIVE, 0.10f}, {P_OD_TONE, 0.40f},
      {P_OD_LEVEL, 0.55f}, {P_OD_MIX, 0.50f},
      {P_GATE_ON, 0.0f}, {P_LIMITER_ON, 1.0f}, {P_LIMITER_THRESH, 0.90f},
      {P_NAM_ON, 0.0f}, {-1, 0.0f}}},
    {"Rock", "Rock", 2,
     {{P_GAIN, 0.52f}, {P_BASS, 0.55f}, {P_MID, 0.48f}, {P_TREBLE, 0.56f},
      {P_PRESENCE, 0.52f}, {P_RESONANCE, 0.52f}, {P_MASTER, 0.50f},
      {P_OUTPUT, 0.50f}, {P_CAB, 0.0f}, {P_CAB_SET, 3.0f},
      {P_CAB_MIC, 0.0f}, {P_CAB_LEVEL, 0.62f}, {P_CAB_BYPASS, 0.0f},
      {P_COMP_ON, 1.0f}, {P_COMP_AMOUNT, 0.30f}, {P_COMP_ATTACK, 0.30f},
      {P_COMP_RELEASE, 0.40f}, {P_COMP_MIX, 0.80f},
      {P_OD_ON, 1.0f}, {P_OD_DRIVE, 0.18f}, {P_OD_TONE, 0.50f},
      {P_OD_LEVEL, 0.55f}, {P_OD_MIX, 0.50f},
      {P_GATE_ON, 0.0f}, {P_LIMITER_ON, 1.0f}, {P_LIMITER_THRESH, 0.88f},
      {P_NAM_ON, 0.0f}, {-1, 0.0f}}},
    {"Punk", "Rock", 2,
     {{P_GAIN, 0.64f}, {P_BASS, 0.50f}, {P_MID, 0.55f}, {P_TREBLE, 0.62f},
      {P_PRESENCE, 0.62f}, {P_RESONANCE, 0.45f}, {P_MASTER, 0.46f},
      {P_OUTPUT, 0.50f}, {P_CAB, 0.0f}, {P_CAB_SET, 1.0f},
      {P_CAB_MIC, 0.0f}, {P_CAB_LEVEL, 0.64f}, {P_CAB_BYPASS, 0.0f},
      {P_COMP_ON, 1.0f}, {P_COMP_AMOUNT, 0.35f}, {P_COMP_ATTACK, 0.25f},
      {P_COMP_RELEASE, 0.35f}, {P_COMP_MIX, 0.85f},
      {P_OD_ON, 1.0f}, {P_OD_DRIVE, 0.38f}, {P_OD_TONE, 0.58f},
      {P_OD_LEVEL, 0.55f}, {P_OD_MIX, 0.45f},
      {P_GATE_ON, 0.0f}, {P_LIMITER_ON, 1.0f}, {P_LIMITER_THRESH, 0.86f},
      {P_NAM_ON, 0.0f}, {-1, 0.0f}}},
    {"Metal", "Modern", 3,
     {{P_GAIN, 0.62f}, {P_BASS, 0.48f}, {P_MID, 0.58f}, {P_TREBLE, 0.60f},
      {P_PRESENCE, 0.62f}, {P_RESONANCE, 0.42f}, {P_MASTER, 0.42f},
      {P_OUTPUT, 0.44f}, {P_CAB, 0.0f}, {P_CAB_SET, 1.0f},
      {P_CAB_MIC, 0.0f}, {P_CAB_LEVEL, 0.64f}, {P_CAB_BYPASS, 0.0f},
      {P_COMP_ON, 1.0f}, {P_COMP_AMOUNT, 0.30f}, {P_COMP_ATTACK, 0.20f},
      {P_COMP_RELEASE, 0.30f}, {P_COMP_MIX, 0.85f},
      {P_OD_ON, 1.0f}, {P_OD_DRIVE, 0.48f}, {P_OD_TONE, 0.58f},
      {P_OD_LEVEL, 0.55f}, {P_OD_MIX, 0.40f},
      {P_GATE_ON, 1.0f}, {P_GATE_THRESH, 0.45f}, {P_GATE_DECAY, 0.35f},
      {P_LIMITER_ON, 1.0f}, {P_LIMITER_THRESH, 0.84f},
      {P_NAM_ON, 0.0f}, {-1, 0.0f}}},
    {"Funk", "Funk", 0,
     {{P_GAIN, 0.46f}, {P_BASS, 0.46f}, {P_MID, 0.56f}, {P_TREBLE, 0.60f},
      {P_PRESENCE, 0.58f}, {P_RESONANCE, 0.50f}, {P_MASTER, 0.55f},
      {P_OUTPUT, 0.60f}, {P_CAB, 0.0f}, {P_CAB_SET, 1.0f},
      {P_CAB_MIC, 2.0f}, {P_CAB_LEVEL, 0.62f}, {P_CAB_BYPASS, 0.0f},
      {P_COMP_ON, 1.0f}, {P_COMP_AMOUNT, 0.55f}, {P_COMP_ATTACK, 0.15f},
      {P_COMP_RELEASE, 0.30f}, {P_COMP_MIX, 0.85f},
      {P_OCT_ON, 1.0f}, {P_OCT_LEVEL, 0.45f}, {P_OCT_DRY, 0.95f},
      {P_OCT_TONE, 0.55f},
      {P_OD_ON, 0.0f}, {P_OD_DRIVE, 0.10f}, {P_OD_TONE, 0.60f},
      {P_OD_LEVEL, 0.55f}, {P_OD_MIX, 0.50f},
      {P_GATE_ON, 0.0f}, {P_LIMITER_ON, 1.0f}, {P_LIMITER_THRESH, 0.92f},
      {P_NAM_ON, 0.0f}, {-1, 0.0f}}},
    {"Session", "Studio", 2,
     {{P_GAIN, 0.46f}, {P_BASS, 0.54f}, {P_MID, 0.50f}, {P_TREBLE, 0.52f},
      {P_PRESENCE, 0.48f}, {P_RESONANCE, 0.52f}, {P_MASTER, 0.50f},
      {P_OUTPUT, 0.50f}, {P_CAB, 0.0f}, {P_CAB_SET, 2.0f},
      {P_CAB_MIC, 2.0f}, {P_CAB_LEVEL, 0.62f}, {P_CAB_BYPASS, 0.0f},
      {P_COMP_ON, 1.0f}, {P_COMP_AMOUNT, 0.35f}, {P_COMP_ATTACK, 0.40f},
      {P_COMP_RELEASE, 0.45f}, {P_COMP_MIX, 0.80f},
      {P_OD_ON, 0.0f}, {P_OD_DRIVE, 0.12f}, {P_OD_TONE, 0.48f},
      {P_OD_LEVEL, 0.55f}, {P_OD_MIX, 0.50f},
      {P_GATE_ON, 0.0f}, {P_LIMITER_ON, 1.0f}, {P_LIMITER_THRESH, 0.90f},
      {P_NAM_ON, 0.0f}, {-1, 0.0f}}},
};

constexpr int kPresetCount =
    static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

// Apply the startup preset to a freshly constructed Shared.
inline void applyStartupPreset(Shared& shared) {
  shared.set(P_MODEL, static_cast<double>(kPresets[0].model));
  for (const PresetValue& v : kPresets[0].values) {
    if (v.param >= 0 && v.param < static_cast<int>(P_COUNT)) {
      shared.set(static_cast<uint32_t>(v.param), static_cast<double>(v.value));
    }
  }
}

} // namespace bass
