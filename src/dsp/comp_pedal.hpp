// Bass Suite — simple bass compressor pedal
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A feed-forward peak compressor with a parallel mix.  It is deliberately
// simple and stable; the Comp Suite models can replace it in a later pass.

#pragma once

#include "common.hpp"
#include "controls.hpp"

namespace bass {

class CompPedal {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    mix_.setTime(15.0f, sr_);
    reset();
  }

  void setControls(const Controls& c) noexcept {
    amount_ = clampf(c.compAmount, 0.0f, 1.0f);
    // 2:1 at low amount, up to 8:1 at maximum.
    ratio_ = 2.0f + 6.0f * amount_;
    thresholdDb_ = -6.0f - 30.0f * amount_;
    attackCoef_ = 1.0f - timeCoeff(2.0f + 58.0f * c.compAttack, sr_);
    releaseCoef_ = 1.0f - timeCoeff(60.0f + 540.0f * c.compRelease, sr_);
    makeupDb_ = -thresholdDb_ * 0.45f;
    mix_.setTarget(clampf(c.compMix, 0.0f, 1.0f));
  }

  void reset() noexcept {
    env_ = 0.0f;
    gain_ = 1.0f;
    mix_.snap();
  }

  void process(float& l, float& r) noexcept {
    const float dryL = l;
    const float dryR = r;
    const float level = std::max(std::fabs(l), std::fabs(r));
    const float coef = level > env_ ? attackCoef_ : releaseCoef_;
    env_ += (level - env_) * coef;
    env_ = flush(env_);

    const float overDb = gainToDb(env_) - thresholdDb_;
    const float grDb = overDb > 0.0f ? -overDb * (1.0f - 1.0f / ratio_) : 0.0f;
    const float target = dbToGain(grDb + makeupDb_);
    gain_ += (target - gain_) * (target < gain_ ? attackCoef_ : releaseCoef_);
    gain_ = flush(gain_);

    const float mix = mix_.next();
    l = dryL + mix * (l * gain_ - dryL);
    r = dryR + mix * (r * gain_ - dryR);
  }

private:
  static float flush(float x) noexcept {
    return (x > -1.0e-15f && x < 1.0e-15f) ? 0.0f : x;
  }

  float sr_ = 48000.0f;
  float amount_ = 0.35f;
  float ratio_ = 4.0f;
  float thresholdDb_ = -16.0f;
  float makeupDb_ = 0.0f;
  float attackCoef_ = 0.01f;
  float releaseCoef_ = 0.001f;
  float env_ = 0.0f;
  float gain_ = 1.0f;
  Smoothed mix_;
};

} // namespace bass
