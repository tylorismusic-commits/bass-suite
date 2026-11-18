// Bass Suite — sub-octave pedal
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A classic analog-style flip-flop divider with a tracking low-pass, a tone
// low-pass and a parallel dry path.  It is monophonic by design; chords and
// very high notes are not its job.

#pragma once

#include "common.hpp"
#include "controls.hpp"

namespace bass {

class OctavePedal {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    track_.set(600.0f, sr_);
    toneL_.set(1200.0f, sr_);
    toneR_.set(1200.0f, sr_);
    tone2L_.set(1200.0f, sr_);
    tone2R_.set(1200.0f, sr_);
    envAttack_ = timeCoeff(4.0f, sr_);
    envRelease_ = timeCoeff(70.0f, sr_);
    level_.setTime(10.0f, sr_);
    dry_.setTime(10.0f, sr_);
    enable_.setTime(8.0f, sr_);
    reset();
  }

  void setControls(const Controls& c) noexcept {
    level_.setTarget(clampf(c.octLevel, 0.0f, 1.0f));
    dry_.setTarget(clampf(c.octDry, 0.0f, 1.0f));
    enable_.setTarget(c.octOn ? 1.0f : 0.0f);
    const float hz = 400.0f * std::pow(8.0f, clampf(c.octTone, 0.0f, 1.0f));
    toneL_.set(hz, sr_);
    toneR_.set(hz, sr_);
    tone2L_.set(hz, sr_);
    tone2R_.set(hz, sr_);
  }

  void reset() noexcept {
    track_.reset();
    toneL_.reset();
    toneR_.reset();
    tone2L_.reset();
    tone2R_.reset();
    level_.snap();
    dry_.snap();
    enable_.snap();
    subSign_ = 1.0f;
    prev_ = 0.0f;
    env_ = 0.0f;
  }

  void process(float& l, float& r) noexcept {
    const float mono = 0.5f * (l + r);
    const float tracked = track_.process(mono);
    constexpr float kHysteresis = 0.004f;
    if (tracked > kHysteresis && prev_ <= kHysteresis) {
      subSign_ = -subSign_;
    }
    prev_ = tracked;

    // Track the input envelope, so quiet notes get a quiet sub-octave.  The
    // old fixed 0.65 square made the pedal only usable at very low Level.
    const float envTarget = std::fabs(tracked);
    const float envCoeff = envTarget > env_ ? envAttack_ : envRelease_;
    env_ = envTarget + envCoeff * (env_ - envTarget);
    const float amp = 0.50f * clampf(env_ * 3.0f - 0.02f, 0.0f, 1.0f);
    const float square = subSign_ * amp;
    // Two cascaded poles round the square into a bass sub-octave instead of a
    // fizzy buzzer.
    const float wetL = tone2L_.process(toneL_.process(square));
    const float wetR = tone2R_.process(toneR_.process(square));
    const float dryAmt = dry_.next();
    const float wetAmt = level_.next();
    const float enable = enable_.next();
    const float outL = l * dryAmt + wetL * wetAmt;
    const float outR = r * dryAmt + wetR * wetAmt;
    l += enable * (outL - l);
    r += enable * (outR - r);
  }

private:
  float sr_ = 48000.0f;
  OnePoleLP track_;
  OnePoleLP toneL_, toneR_, tone2L_, tone2R_;
  Smoothed level_, dry_, enable_;
  float subSign_ = 1.0f;
  float prev_ = 0.0f;
  float env_ = 0.0f;
  float envAttack_ = 0.0f;
  float envRelease_ = 0.0f;
};

} // namespace bass
