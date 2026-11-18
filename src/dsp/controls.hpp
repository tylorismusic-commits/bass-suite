// Bass Suite — controls.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace bass {

// Display order of the amp tabs.  The model value is part of the saved-session
// format: only ever append here, never renumber.
enum class Model : int {
  Studio = 0,   // clean solid-state DI
  FlipTop = 1,  // warm vintage tube
  SVT = 2,      // big, tight classic tube
  Modern = 3,   // aggressive modern bass
  Count = 4
};

// Every host control is normalised 0..1 unless the parameter table says
// otherwise.  Model-specific switches live in shared slots and are shown only
// on the models that have them.
struct Controls {
  int model = static_cast<int>(Model::SVT);
  bool enabled = true;
  bool tunerOn = false;
  float input = 0.5f;       // input trim
  float gain = 0.55f;       // preamp gain / drive
  float bass = 0.5f;
  float mid = 0.45f;
  float treble = 0.55f;
  float presence = 0.5f;
  float resonance = 0.5f;
  float master = 0.45f;
  float output = 0.5f;
  bool ampBypass = false;   // skip preamp, tone stack and power amp
  bool namOn = false;       // use a loaded NAM model in place of the amp
  int namModel = 0;         // 1-based NAM library index
  float namLevel = 0.5f;    // -12..+12 dB trim around the model normalization
  bool gateOn = false;
  float gateThreshold = 0.55f;  // mapped -80..-20 dBFS
  float gateDecay = 0.4f;       // mapped 20..1000 ms

  // Cab.
  int cab = 0;              // 0 factory, 1 user IR
  int cabSet = 3;           // 0 1x15, 1 4x10, 2 2x12, 3 8x10
  int cabMic = 0;           // 0 dynamic, 1 ribbon, 2 blend
  int userIr[4] = {0, 0, 0, 0};  // 1-based user library index per amp
  float cabLevel = 0.6f;    // mapped to -24..+6 dB
  float cabLowCut = 0.0f;   // mapped to 20..300 Hz
  float cabHighCut = 1.0f;  // mapped to 2k..20k Hz
  bool cabBypass = false;

  // Octave pedal.
  bool octOn = false;
  float octLevel = 0.35f;
  float octDry = 1.0f;
  float octTone = 0.5f;

  // Output EQ / filters.
  float outEqLow = 0.5f;
  float outEqLowMid = 0.5f;
  float outEqHighMid = 0.5f;
  float outEqHigh = 0.5f;
  float outHpf = 0.0f;
  float outLpf = 1.0f;

  // Comp pedal.
  bool compOn = true;
  float compAmount = 0.35f;
  float compAttack = 0.35f;
  float compRelease = 0.45f;
  float compMix = 1.0f;

  // Drive pedal (the TS9 core with a bass-safe clean blend).
  bool odOn = false;
  float odDrive = 0.25f;
  float odTone = 0.5f;
  float odLevel = 0.55f;
  float odMix = 0.6f;

  // Global.
  bool limiterOn = false;
  float limiterThreshold = 1.0f;  // 1.0 = 0 dB
};

// Metering published by the engine, in dB.
struct Meters {
  float inPeakDb = -120.0f;
  float outPeakDb = -120.0f;
  float inPeakLDb = -120.0f;
  float inPeakRDb = -120.0f;
  float outPeakLDb = -120.0f;
  float outPeakRDb = -120.0f;
  float outVuDb = -120.0f;
  float tunerHz = 0.0f;
  float tunerCents = 0.0f;
  float tunerConfidence = 0.0f;
  int tunerNote = -1;
};

} // namespace bass
