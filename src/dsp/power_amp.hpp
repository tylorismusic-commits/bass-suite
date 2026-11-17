// Bass Suite — power amp stage
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Guitarix's Plexi EL34 power-amp table (GPL-2.0-or-later) with a feedback
// filter and a simple supply-sag envelope.  The table is a static transfer
// curve; the feedback filter and sag give it the time-dependent behaviour.

#pragma once

#include <algorithm>
#include <cmath>

#include "common.hpp"

namespace bass {

struct PowerAmpSpec {
  float drive = 1.0f;
  float feedback = 0.35f;
  float sag = 0.1f;
  float output = 0.8f;
  bool useTable = false;
};

// Implemented in power_amp.cpp; includes third_party/guitarix's Plexi table.
float guitarix_power_amp_clip(float x) noexcept;

class PowerAmp {
public:
  void prepare(float sampleRate, const PowerAmpSpec& spec) noexcept {
    sr_ = sampleRate;
    spec_ = spec;
    sagAttack_ = 1.0f - timeCoeff(12.0f, sr_);
    sagRelease_ = 1.0f - timeCoeff(140.0f, sr_);
    setPresence(0.5f);
    setResonance(0.5f);
    reset();
  }

  void setSpec(const PowerAmpSpec& spec) noexcept { spec_ = spec; }

  // Presence sits in the feedback network, as on the hardware: it changes R4
  // in Guitarix's feedback filter rather than applying a post-EQ shelf.
  void setPresence(float presence) noexcept {
    const double p = std::clamp(static_cast<double>(presence), 0.0, 1.0);
    constexpr double R3 = 100.0e3;
    constexpr double R5 = 4.7e3;
    constexpr double C3 = 100.0e-9;
    const double R4 = 22.0e3 * p;
    const double B0 = R5;
    const double B1 = R4 * R5 * C3;
    const double A0 = R3 + R5;
    const double A1 = C3 * R4 * R5 + C3 * R3 * R5 + C3 * R3 * R4;
    const double a = A0 + 2.0 * A1 * sr_;
    if (std::fabs(a) < 1.0e-30) {
      return;
    }
    // Denominator coefficient of z^-1 is (A0 - 2*A1*fs); the difference
    // equation subtracts a1_*y1, so a1_ is that value directly.  An extra
    // minus sign here turns the shelving feedback network into a near-Nyquist
    // resonator whose loop gain exceeds unity at high oversampling rates and
    // self-oscillates (heard as low-level warble and a "gated" feel).
    a1_ = static_cast<float>((A0 - 2.0 * A1 * sr_) / a);
    b0_ = static_cast<float>((B0 + 2.0 * B1 * sr_) / a);
    b1_ = static_cast<float>((B0 - 2.0 * B1 * sr_) / a);
  }

  // Resonance is a low-frequency trim in the NFB path: at high settings the
  // feedback is reduced at low frequencies, which lets the speaker/transformer
  // low end through; at low settings the loop is tighter.
  void setResonance(float resonance) noexcept {
    const double r = std::clamp(static_cast<double>(resonance), 0.0, 1.0);
    resonanceDepth_ = static_cast<float>((r - 0.5) * 1.0);
    const double fc = 140.0;
    resonanceCoeff_ = static_cast<float>(
        1.0 - std::exp(-2.0 * 3.14159265358979323846 * fc /
                       static_cast<double>(sr_)));
  }

  void reset() noexcept {
    x1_ = 0.0f;
    y1_ = 0.0f;
    previous_ = 0.0f;
    sag_ = 1.0f;
    resLp_ = 0.0f;
  }

  float process(float x) noexcept {
    float v = x * spec_.drive;
    float fb = feedbackFilter(previous_);
    resLp_ += (fb - resLp_) * resonanceCoeff_;
    fb -= resonanceDepth_ * resLp_;
    // The Guitarix feedback filter is narrow; keep the loop well inside its
    // stable range so a bad preset cannot latch the stage.
    const float fbAmount = std::min(spec_.feedback, 0.12f);
    v -= fbAmount * fb;

    const float level = clampf(std::fabs(v), 0.0f, 1.4f);
    const float target = 1.0f - spec_.sag * level;
    sag_ += (target - sag_) * (target < sag_ ? sagAttack_ : sagRelease_);
    v *= sag_;

    float y;
    if (spec_.useTable) {
      y = guitarix_power_amp_clip(v);
    } else {
      y = std::tanh(v * 2.0f);
    }
    y *= spec_.output;
    previous_ = y;
    return y;
  }

private:
  float feedbackFilter(float x) noexcept {
    const float y = b0_ * x + b1_ * x1_ - a1_ * y1_;
    if (!std::isfinite(y)) {
      x1_ = y1_ = 0.0f;
      return 0.0f;
    }
    x1_ = x;
    y1_ = y;
    return y;
  }

  PowerAmpSpec spec_;
  float sr_ = 48000.0f;
  float b0_ = 0.0f;
  float b1_ = 0.0f;
  float a1_ = 0.0f;
  float x1_ = 0.0f;
  float y1_ = 0.0f;
  float previous_ = 0.0f;
  float sag_ = 1.0f;
  float sagAttack_ = 0.1f;
  float sagRelease_ = 0.01f;
  float resLp_ = 0.0f;
  float resonanceDepth_ = 0.0f;
  float resonanceCoeff_ = 0.0f;
};

} // namespace bass
