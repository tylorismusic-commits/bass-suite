// Bass Suite — sub-octave pedal
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A classic analog-style flip-flop divider with a tracking low-pass, a tone
// low-pass and a parallel dry path.  It is monophonic by design; chords and
// very high notes are not its job.
//
// The pre-filter used to be a fixed 600 Hz, which passes the second and third
// harmonics of a low bass note almost untouched; on a bright pluck those add
// enough zero crossings to toggle the divider twice per cycle, so the pedal
// jumped between the sub and the fundamental and sometimes produced nothing
// usable at all on a low B.  It now follows the tracked fundamental the same
// way the pedal-suite octave does.

#pragma once

#include "biquad.hpp"
#include "common.hpp"
#include "controls.hpp"
#include "pitch_tracker.hpp"

namespace bass {

class OctavePedal {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    tracker_.prepare(sr_);
    lastPitchUpdate_ = 0;
    tuneCounter_ = 0;
    cutoff_ = kDefaultCutoff;
    targetCutoff_ = kDefaultCutoff;
    preLp1_.lowPass(cutoff_, 0.707f, sr_);
    preLp2_.lowPass(cutoff_, 0.707f, sr_);
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
    tracker_.reset();
    preLp1_.reset();
    preLp2_.reset();
    lastPitchUpdate_ = 0;
    tuneCounter_ = 0;
    cutoff_ = kDefaultCutoff;
    targetCutoff_ = kDefaultCutoff;
    preLp1_.lowPass(cutoff_, 0.707f, sr_);
    preLp2_.lowPass(cutoff_, 0.707f, sr_);
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

    tracker_.process(mono);
    if (tracker_.updates() != lastPitchUpdate_) {
      lastPitchUpdate_ = tracker_.updates();
      if (tracker_.valid() && tracker_.confidence() > 0.4f) {
        // An interval above the fundamental: high enough to keep the attack,
        // low enough to leave one zero crossing per cycle at the divider.
        targetCutoff_ = clampf(1.3f * tracker_.hz(), kMinCutoff, kMaxCutoff);
      }
    }

    // Retune in small steps.  A coefficient jump on a running filter clicks,
    // and a note change can move the cut-off most of an octave.  Drop fast
    // (an open filter is what toggles the divider) and open gently.
    if (++tuneCounter_ >= 32) {
      tuneCounter_ = 0;
      if (std::fabs(targetCutoff_ - cutoff_) > 0.1f) {
        const float k = targetCutoff_ < cutoff_ ? 0.5f : 0.2f;
        cutoff_ += k * (targetCutoff_ - cutoff_);
        preLp1_.lowPass(cutoff_, 0.707f, sr_);
        preLp2_.lowPass(cutoff_, 0.707f, sr_);
      }
    }

    // Two stages (24 dB/oct): a bass pickup can put its second harmonic
    // almost as high as the fundamental, and one stage leaves enough of it to
    // toggle the divider twice per cycle.
    const float tracked = preLp2_.process(preLp1_.process(mono));
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
  // Conservative until the tracker has an estimate: low enough to leave the
  // fundamental, high enough to keep some attack.
  static constexpr float kDefaultCutoff = 120.0f;
  static constexpr float kMinCutoff = 24.0f;
  static constexpr float kMaxCutoff = 500.0f;

  float sr_ = 48000.0f;
  PitchTracker tracker_;
  Biquad preLp1_, preLp2_;
  OnePoleLP toneL_, toneR_, tone2L_, tone2R_;
  Smoothed level_, dry_, enable_;
  float cutoff_ = kDefaultCutoff;
  float targetCutoff_ = kDefaultCutoff;
  unsigned lastPitchUpdate_ = 0;
  int tuneCounter_ = 0;
  float subSign_ = 1.0f;
  float prev_ = 0.0f;
  float env_ = 0.0f;
  float envAttack_ = 0.0f;
  float envRelease_ = 0.0f;
};

} // namespace bass
