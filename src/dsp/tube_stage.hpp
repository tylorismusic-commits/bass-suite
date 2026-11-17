// Bass Suite — Guitarix-derived tube stage
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Port of Guitarix's `tubestageF` / `tubestageP` (GPL-2.0-or-later,
// Copyright (C) 2012 Hermann Meyer, Andreas Degert, Pete Shorthose,
// Steve Poskitt).  The table lookup itself lives in guitarix_tables.cpp so the
// large generated tables are compiled exactly once.

#pragma once

#include <cmath>

#include "biquad.hpp"
#include "common.hpp"

namespace bass {

// Must match the enum in third_party/guitarix/DSP/valve.h.  The static_assert
// in guitarix_tables.cpp keeps the two in sync.
enum class TubeTable : int {
  k12AX7_68k = 0,
  k12AX7_250k = 1,
  k6V6_68k = 2,
  k6V6_250k = 3,
  k12AU7_68k = 4,
  k12AU7_250k = 5,
  k6DJ8_68k = 6,
  k6DJ8_250k = 7,
  k12AT7_68k = 8,
  k12AT7_250k = 9,
  k6C16_68k = 10,
  k6C16_250k = 11,
  k6L6CG_68k = 12,
  k6L6CG_250k = 13,
  kEL34_68k = 14,
  kEL34_250k = 15,
  k12AY7_68k = 16,
  k12AY7_250k = 17,
  kJJECC83S_68k = 18,
  kJJECC83S_250k = 19,
  kJJECC99_68k = 20,
  kJJECC99_250k = 21,
  kEL84_68k = 22,
  kEL84_250k = 23,
  kEF86_68k = 24,
  kEF86_250k = 25,
  kSVEL34_68k = 26,
  kSVEL34_250k = 27,
  Count = 28
};

double guitarix_ftube(int table, double vgk) noexcept;
double guitarix_ranode(int table, double vgk) noexcept;

struct StageSpec {
  TubeTable tube = TubeTable::k12AX7_68k;
  float vplus = 250.0f;
  float divider = 40.0f;
  float fck = 86.0f;       // cathode-bypass cutoff
  float rk = 2700.0f;      // cathode resistor
  float vk0 = 1.581656f;   // cathode bias
  float rp = 100000.0f;    // plate resistor
};

// One common-cathode stage.  The feedback path is the cathode network:
//   v   = x + fb
//   out = Ftube(v - Vk0) + (VkC - Vplus)
//   fb  = lowpass(out * Rk / (Rp + Ranode(v - Vk0)), fck)
//   y   = highpass(out / divider)
class TubeStage {
public:
  void prepare(const StageSpec& spec, float sampleRate) noexcept {
    spec_ = spec;
    sr_ = sampleRate;
    vkc_ = spec.vk0 * (spec.rp / spec.rk);
    lpfk_.set(spec.fck, sampleRate);
    anti_.lowPass(std::min(sampleRate * 0.45f, 20000.0f), 0.707f,
                  sampleRate);
    hpf_.set(20.0f, sampleRate);
    reset();
  }

  void reset() noexcept {
    fb_ = 0.0;
    lpfk_.reset();
    anti_.reset();
    hpf_.reset();
  }

  float process(float x) noexcept {
    x = anti_.process(x);
    const double v = static_cast<double>(x) + fb_;
    const double vgk = v - static_cast<double>(spec_.vk0);
    const double ft = guitarix_ftube(static_cast<int>(spec_.tube), vgk);
    double out = ft + (vkc_ - static_cast<double>(spec_.vplus));
    const double ra = guitarix_ranode(static_cast<int>(spec_.tube), vgk);
    const double target =
        out * (static_cast<double>(spec_.rk) /
               (static_cast<double>(spec_.rp) + ra));
    fb_ = static_cast<double>(lpfk_.process(static_cast<float>(target)));
    out /= static_cast<double>(spec_.divider);
    return hpf_.process(static_cast<float>(out));
  }

  const StageSpec& spec() const noexcept { return spec_; }

private:
  StageSpec spec_;
  float sr_ = 48000.0f;
  double vkc_ = 0.0;
  double fb_ = 0.0;
  OnePoleLP lpfk_;
  Biquad anti_;
  HighPass2 hpf_;
};

} // namespace bass
