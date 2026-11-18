// Bass Suite — params.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "dsp/controls.hpp"

namespace bass {

// Parameter ids are the CLAP parameter ids and the index into Shared::values.
// They are also the ordering of the LV2 control ports (port = 4 + id), so this
// enum must only ever be appended to.
enum ParamId : uint32_t {
  P_MODEL = 0,
  P_POWER,
  P_INPUT,
  P_GAIN,
  P_BASS,
  P_MID,
  P_TREBLE,
  P_PRESENCE,
  P_RESONANCE,
  P_MASTER,
  P_OUTPUT,
  P_CAB,
  P_CAB_MIC,
  P_CAB_LEVEL,
  P_CAB_LOWCUT,
  P_CAB_HIGHCUT,
  P_CAB_BYPASS,
  P_OD_ON,
  P_OD_DRIVE,
  P_OD_TONE,
  P_OD_LEVEL,
  P_OD_MIX,
  P_LIMITER_ON,
  P_LIMITER_THRESH,
  P_USER_IR_0,
  P_USER_IR_1,
  P_USER_IR_2,
  P_USER_IR_3,
  P_AMP_BYPASS,
  P_NAM_ON,
  P_NAM_MODEL,
  P_NAM_LEVEL,
  P_GATE_ON,
  P_GATE_THRESH,
  P_GATE_DECAY,
  P_CAB_SET,
  P_OCT_ON,
  P_OCT_LEVEL,
  P_OCT_DRY,
  P_OCT_TONE,
  P_COMP_ON,
  P_COMP_AMOUNT,
  P_COMP_ATTACK,
  P_COMP_RELEASE,
  P_COMP_MIX,
  P_OUT_EQ_LOW,
  P_OUT_EQ_LOW_MID,
  P_OUT_EQ_HIGH_MID,
  P_OUT_EQ_HIGH,
  P_OUT_HPF,
  P_OUT_LPF,
  P_COUNT
};

// Scale-point labels for a stepped parameter.  Values are always 0..count-1.
struct Labels {
  const char* const* names = nullptr;
  int count = 0;
};

template <size_t N>
constexpr Labels labels(const char* const (&arr)[N]) noexcept {
  return Labels{arr, static_cast<int>(N)};
}

// One table drives the DSP, CLAP params, editor and LV2 metadata (via
// tools/gen_ttl.cpp).  `symbol` is the LV2 port symbol and part of the saved
// session format: never rename it.
struct ParamSpec {
  const char* symbol;
  const char* name;
  const char* module;
  double min;
  double max;
  double def;
  bool stepped;
  bool hidden;
  bool toggled;
  const char* unit;
  Labels labels;
};

inline constexpr const char* kModelLabels[] = {
    "Studio DI", "Flip Top", "SVT", "Modern"};
inline constexpr const char* kCabLabels[] = {"Factory", "User"};
inline constexpr const char* kCabSetLabels[] = {
    "1×15", "4×10", "2×12", "8×10", "2×10", "1×12"};
inline constexpr const char* kCabMicLabels[] = {"Dynamic", "Ribbon", "Blend"};

inline constexpr ParamSpec kParamSpecs[P_COUNT] = {
    {"model", "Amp", "Amp", 0.0, 3.0, 2.0, true, false, false, nullptr,
     labels(kModelLabels)},
    {"power", "Power", "Amp", 0.0, 1.0, 1.0, true, false, true, nullptr, {}},
    {"input", "Input", "Amp", 0.0, 1.0, 0.5, false, false, false, nullptr, {}},
    {"gain", "Gain", "Amp", 0.0, 1.0, 0.55, false, false, false, nullptr, {}},
    {"bass", "Bass", "Amp", 0.0, 1.0, 0.5, false, false, false, nullptr, {}},
    {"mid", "Mid", "Amp", 0.0, 1.0, 0.45, false, false, false, nullptr, {}},
    {"treble", "Treble", "Amp", 0.0, 1.0, 0.55, false, false, false, nullptr,
     {}},
    {"presence", "Presence", "Amp", 0.0, 1.0, 0.5, false, false, false,
     nullptr, {}},
    {"resonance", "Resonance", "Amp", 0.0, 1.0, 0.5, false, false, false,
     nullptr, {}},
    {"master", "Master", "Amp", 0.0, 1.0, 0.45, false, false, false, nullptr,
     {}},
    {"output", "Output", "Amp", 0.0, 1.0, 0.5, false, false, false, nullptr,
     {}},
    {"cab", "Cabinet", "Cab", 0.0, 1.0, 0.0, true, false, false, nullptr,
     labels(kCabLabels)},
    {"cab_mic", "Cab Mic", "Cab", 0.0, 2.0, 0.0, true, false, false, nullptr,
     labels(kCabMicLabels)},
    {"cab_level", "Cab Level", "Cab", 0.0, 1.0, 0.6, false, false, false,
     nullptr, {}},
    {"cab_lowcut", "Cab Low Cut", "Cab", 0.0, 1.0, 0.0, false, false, false,
     nullptr, {}},
    {"cab_highcut", "Cab High Cut", "Cab", 0.0, 1.0, 1.0, false, false, false,
     nullptr, {}},
    {"cab_bypass", "Cab Bypass", "Cab", 0.0, 1.0, 0.0, true, false, true,
     nullptr, {}},
    {"drive_on", "Drive", "Drive", 0.0, 1.0, 0.0, true, false, true, nullptr, {}},
    {"drive_amount", "Drive", "Drive", 0.0, 1.0, 0.25, false, false, false,
     nullptr, {}},
    {"drive_tone", "Tone", "Drive", 0.0, 1.0, 0.5, false, false, false,
     nullptr, {}},
    {"drive_level", "Level", "Drive", 0.0, 1.0, 0.55, false, false, false,
     nullptr, {}},
    {"drive_mix", "Blend", "Drive", 0.0, 1.0, 0.60, false, false, false,
     nullptr, {}},
    {"limiter_on", "Limiter", "Limiter", 0.0, 1.0, 0.0, true, false, true,
     nullptr, {}},
    {"limiter_threshold", "Limiter Threshold", "Limiter", 0.0, 1.0, 1.0,
     false, false, false, nullptr, {}},
    {"user_ir_studio", "Studio DI User IR", "Cab", 0.0, 63.0, 0.0, true,
     false, false, nullptr, {}},
    {"user_ir_flip_top", "Flip Top User IR", "Cab", 0.0, 63.0, 0.0, true, false,
     false, nullptr, {}},
    {"user_ir_svt", "SVT User IR", "Cab", 0.0, 63.0, 0.0, true, false,
     false, nullptr, {}},
    {"user_ir_modern", "Modern User IR", "Cab", 0.0, 63.0, 0.0, true, false,
     false, nullptr, {}},
    {"amp_bypass", "Amp Bypass", "Amp", 0.0, 1.0, 0.0, true, false, true, nullptr,
     {}},
    {"nam_on", "NAM On", "NAM", 0.0, 1.0, 0.0, true, false, true, nullptr, {}},
    {"nam_model", "NAM Model", "NAM", 0.0, 63.0, 0.0, true, false, false,
     nullptr, {}},
    {"nam_level", "NAM Level", "NAM", 0.0, 1.0, 0.5, false, false, false,
     nullptr, {}},
    {"gate_on", "Gate On", "Gate", 0.0, 1.0, 0.0, true, false, true, nullptr, {}},
    {"gate_thresh", "Gate Threshold", "Gate", 0.0, 1.0, 0.55, false, false,
     false, nullptr, {}},
    {"gate_decay", "Gate Decay", "Gate", 0.0, 1.0, 0.4, false, false, false,
     nullptr, {}},
    {"cab_set", "Cabinet", "Cab", 0.0, 5.0, 3.0, true, false, false, nullptr,
     labels(kCabSetLabels)},
    {"oct_on", "Octave", "Octave", 0.0, 1.0, 0.0, true, false, true, nullptr, {}},
    {"oct_level", "Octave", "Octave", 0.0, 1.0, 0.28, false, false, false,
     nullptr, {}},
    {"oct_dry", "Dry", "Octave", 0.0, 1.0, 1.0, false, false, false, nullptr, {}},
    {"oct_tone", "Tone", "Octave", 0.0, 1.0, 0.5, false, false, false, nullptr, {}},
    {"comp_on", "Comp", "Comp", 0.0, 1.0, 1.0, true, false, true, nullptr, {}},
    {"comp_amount", "Amount", "Comp", 0.0, 1.0, 0.35, false, false, false,
     nullptr, {}},
    {"comp_attack", "Attack", "Comp", 0.0, 1.0, 0.35, false, false, false,
     nullptr, {}},
    {"comp_release", "Release", "Comp", 0.0, 1.0, 0.45, false, false, false,
     nullptr, {}},
    {"comp_mix", "Mix", "Comp", 0.0, 1.0, 1.0, false, false, false, nullptr, {}},
    {"out_eq_low", "Low", "Output", 0.0, 1.0, 0.5, false, false, false, nullptr, {}},
    {"out_eq_low_mid", "Low Mid", "Output", 0.0, 1.0, 0.5, false, false,
     false, nullptr, {}},
    {"out_eq_high_mid", "High Mid", "Output", 0.0, 1.0, 0.5, false, false,
     false, nullptr, {}},
    {"out_eq_high", "High", "Output", 0.0, 1.0, 0.5, false, false, false,
     nullptr, {}},
    {"out_hpf", "HPF", "Output", 0.0, 1.0, 0.0, false, false, false, nullptr, {}},
    {"out_lpf", "LPF", "Output", 0.0, 1.0, 1.0, false, false, false, nullptr, {}},
};

static_assert(sizeof(kParamSpecs) / sizeof(kParamSpecs[0]) == P_COUNT,
              "kParamSpecs must cover every parameter id");

// The control values, meters and dirty flags shared between the audio thread,
// the host thread and the editor.  Everything is atomic so the editor can run
// on the GUI thread without locks.
class Shared {
public:
  Shared() {
    for (uint32_t p = 0; p < P_COUNT; ++p) {
      values_[p].store(kParamSpecs[p].def, std::memory_order_relaxed);
      dirty_[p].store(false, std::memory_order_relaxed);
    }
    inPeakDb.store(-120.0f, std::memory_order_relaxed);
    outPeakDb.store(-120.0f, std::memory_order_relaxed);
    inPeakLDb.store(-120.0f, std::memory_order_relaxed);
    inPeakRDb.store(-120.0f, std::memory_order_relaxed);
    outPeakLDb.store(-120.0f, std::memory_order_relaxed);
    outPeakRDb.store(-120.0f, std::memory_order_relaxed);
    outVuDb.store(-120.0f, std::memory_order_relaxed);
    meterSerial.store(0, std::memory_order_relaxed);
    tunerOn.store(false, std::memory_order_relaxed);
    tunerHz.store(0.0f, std::memory_order_relaxed);
    tunerCents.store(0.0f, std::memory_order_relaxed);
    tunerConfidence.store(0.0f, std::memory_order_relaxed);
    tunerNote.store(-1, std::memory_order_relaxed);
  }

  double get(uint32_t p) const noexcept {
    return values_[p].load(std::memory_order_relaxed);
  }

  // Clamps to the parameter range and marks the value for the host.
  void set(uint32_t p, double v) noexcept {
    if (!std::isfinite(v)) {
      return;
    }
    const ParamSpec& s = kParamSpecs[p];
    v = std::min(std::max(v, s.min), s.max);
    if (s.stepped) {
      v = static_cast<double>(static_cast<int>(v + (v >= 0 ? 0.5 : -0.5)));
    }
    values_[p].store(v, std::memory_order_relaxed);
    dirty_[p].store(true, std::memory_order_release);
  }

  // Stores a value that came from the host, without marking it dirty.
  void setFromHost(uint32_t p, double v) noexcept {
    if (!std::isfinite(v)) {
      return;
    }
    const ParamSpec& s = kParamSpecs[p];
    v = std::min(std::max(v, s.min), s.max);
    values_[p].store(v, std::memory_order_relaxed);
  }

  bool takeDirty(uint32_t p) noexcept {
    return dirty_[p].exchange(false, std::memory_order_acquire);
  }

  // A preset or host-state load replaces many controls at once.  The audio
  // thread turns this into a full engine switch so the running amp never
  // steps its gain/tone coefficients in place.
  void requestReload() noexcept {
    reload_.store(true, std::memory_order_release);
  }
  bool takeReload() noexcept {
    return reload_.exchange(false, std::memory_order_acquire);
  }

  bool isDirty(uint32_t p) const noexcept {
    return dirty_[p].load(std::memory_order_acquire);
  }

  bool anyDirty() const noexcept {
    for (uint32_t p = 0; p < P_COUNT; ++p) {
      if (dirty_[p].load(std::memory_order_acquire)) {
        return true;
      }
    }
    return false;
  }

  std::atomic<float> inPeakDb;
  std::atomic<float> outPeakDb;
  std::atomic<float> inPeakLDb;
  std::atomic<float> inPeakRDb;
  std::atomic<float> outPeakLDb;
  std::atomic<float> outPeakRDb;
  std::atomic<float> outVuDb;
  std::atomic<uint32_t> meterSerial;
  std::atomic<bool> tunerOn;
  std::atomic<float> tunerHz;
  std::atomic<float> tunerCents;
  std::atomic<float> tunerConfidence;
  std::atomic<int> tunerNote;

private:
  std::atomic<double> values_[P_COUNT];
  std::atomic<bool> dirty_[P_COUNT];
  std::atomic<bool> reload_{false};
};

// Maps the host-facing parameter values to the DSP controls.
inline Controls controlsFrom(const Shared& s) {
  Controls c;
  c.model = static_cast<int>(s.get(P_MODEL) + 0.5);
  c.enabled = s.get(P_POWER) > 0.5;
  c.input = static_cast<float>(s.get(P_INPUT));
  c.gain = static_cast<float>(s.get(P_GAIN));
  c.bass = static_cast<float>(s.get(P_BASS));
  c.mid = static_cast<float>(s.get(P_MID));
  c.treble = static_cast<float>(s.get(P_TREBLE));
  c.presence = static_cast<float>(s.get(P_PRESENCE));
  c.resonance = static_cast<float>(s.get(P_RESONANCE));
  c.master = static_cast<float>(s.get(P_MASTER));
  c.output = static_cast<float>(s.get(P_OUTPUT));
  c.cab = static_cast<int>(s.get(P_CAB) + 0.5);
  c.cabSet = static_cast<int>(s.get(P_CAB_SET) + 0.5);
  c.cabMic = static_cast<int>(s.get(P_CAB_MIC) + 0.5);
  c.cabLevel = static_cast<float>(s.get(P_CAB_LEVEL));
  c.cabLowCut = static_cast<float>(s.get(P_CAB_LOWCUT));
  c.cabHighCut = static_cast<float>(s.get(P_CAB_HIGHCUT));
  c.cabBypass = s.get(P_CAB_BYPASS) > 0.5;
  c.tunerOn = s.tunerOn.load(std::memory_order_relaxed);
  c.octOn = s.get(P_OCT_ON) > 0.5;
  c.octLevel = static_cast<float>(s.get(P_OCT_LEVEL));
  c.octDry = static_cast<float>(s.get(P_OCT_DRY));
  c.octTone = static_cast<float>(s.get(P_OCT_TONE));
  c.compOn = s.get(P_COMP_ON) > 0.5;
  c.compAmount = static_cast<float>(s.get(P_COMP_AMOUNT));
  c.compAttack = static_cast<float>(s.get(P_COMP_ATTACK));
  c.compRelease = static_cast<float>(s.get(P_COMP_RELEASE));
  c.compMix = static_cast<float>(s.get(P_COMP_MIX));
  c.outEqLow = static_cast<float>(s.get(P_OUT_EQ_LOW));
  c.outEqLowMid = static_cast<float>(s.get(P_OUT_EQ_LOW_MID));
  c.outEqHighMid = static_cast<float>(s.get(P_OUT_EQ_HIGH_MID));
  c.outEqHigh = static_cast<float>(s.get(P_OUT_EQ_HIGH));
  c.outHpf = static_cast<float>(s.get(P_OUT_HPF));
  c.outLpf = static_cast<float>(s.get(P_OUT_LPF));
  c.odOn = s.get(P_OD_ON) > 0.5;
  c.odDrive = static_cast<float>(s.get(P_OD_DRIVE));
  c.odTone = static_cast<float>(s.get(P_OD_TONE));
  c.odLevel = static_cast<float>(s.get(P_OD_LEVEL));
  c.odMix = static_cast<float>(s.get(P_OD_MIX));
  c.limiterOn = s.get(P_LIMITER_ON) > 0.5;
  c.limiterThreshold = static_cast<float>(s.get(P_LIMITER_THRESH));
  for (int i = 0; i < 4; ++i) {
    c.userIr[i] = static_cast<int>(s.get(P_USER_IR_0 + i) + 0.5);
  }
  c.ampBypass = s.get(P_AMP_BYPASS) > 0.5;
  c.namOn = s.get(P_NAM_ON) > 0.5;
  c.namModel = static_cast<int>(s.get(P_NAM_MODEL) + 0.5);
  c.namLevel = static_cast<float>(s.get(P_NAM_LEVEL));
  c.gateOn = s.get(P_GATE_ON) > 0.5;
  c.gateThreshold = static_cast<float>(s.get(P_GATE_THRESH));
  c.gateDecay = static_cast<float>(s.get(P_GATE_DECAY));
  return c;
}

// --- Display helpers -------------------------------------------------------



inline float cabLowCutHz(float v) noexcept {
  return 20.0f * std::pow(15.0f, v);
}

inline float cabHighCutHz(float v) noexcept {
  return 2000.0f * std::pow(10.0f, v);
}

inline float cabLevelDb(float v) noexcept {
  return -24.0f + 30.0f * v;
}

inline float outHpfHz(float v) noexcept {
  return 20.0f * std::pow(15.0f, v);
}

inline float outLpfHz(float v) noexcept {
  return 2000.0f * std::pow(10.0f, v);
}

} // namespace bass
