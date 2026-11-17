// Bass Suite — stereo delay
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tape-style stereo delay: fractional read, feedback low-pass, slow
// modulation, ping-pong.  Delay time follows the host tempo or a free time.
// When bypassed the line keeps running so trails survive.

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "common.hpp"
#include "params.hpp"

namespace bass {

class Delay {
public:
  void prepare(float sampleRate, float maxMs = 2000.0f) noexcept {
    sr_ = sampleRate;
    maxSamples_ = std::max(64, static_cast<int>(sr_ * maxMs * 0.001f) + 4);
    bufferL_.assign(static_cast<size_t>(maxSamples_), 0.0f);
    bufferR_.assign(static_cast<size_t>(maxSamples_), 0.0f);
    write_ = 0;
    time_.setTime(100.0f, sr_);
    feedback_.setTime(20.0f, sr_);
    mix_.setTime(20.0f, sr_);
    time_.reset(0.25f * sr_);
    feedback_.reset(0.3f);
    mix_.reset(0.0f);
    toneL_.set(4000.0f, sr_);
    toneR_.set(4000.0f, sr_);
    modPhase_ = 0.0f;
    modInc_ = 2.0f * kPi * 0.3f / sr_;
    reset();
  }

  void setTargets(const Controls& c, float bpm) noexcept {
    on_ = c.delayOn;
    pingpong_ = c.delayPingPong;
    const float beats = delayDivisionBeats(c.delayDiv);
    float ms;
    if (c.delaySync) {
      const float tempo = (std::isfinite(bpm) && bpm > 1.0f) ? bpm : 120.0f;
      ms = 60000.0f / tempo * beats;
    } else {
      ms = delayTimeMs(c.delayTime);
    }
    ms = clampf(ms, 1.0f, static_cast<float>(maxSamples_ - 3) * 1000.0f / sr_);
    time_.setTarget(ms * 0.001f * sr_);
    feedback_.setTarget(clampf(c.delayFeedback, 0.0f, 0.95f));
    mix_.setTarget(clampf(c.delayMix, 0.0f, 1.0f));
    const float toneHz = 200.0f * std::pow(40.0f, clampf(c.delayTone, 0.0f, 1.0f));
    toneL_.set(toneHz, sr_);
    toneR_.set(toneHz, sr_);
    modDepth_ = clampf(c.delayMod, 0.0f, 1.0f) * 0.003f * sr_;
  }

  void process(float& l, float& r) noexcept {
    float d = time_.next();
    if (modDepth_ > 0.0f) {
      d += modDepth_ * std::sin(modPhase_);
      modPhase_ += modInc_;
      if (modPhase_ > 2.0f * kPi) {
        modPhase_ -= 2.0f * kPi;
      }
    }
    d = clampf(d, 1.0f, static_cast<float>(maxSamples_ - 2));

    const float wetL = toneL_.process(read(bufferL_, d));
    const float wetR = toneR_.process(read(bufferR_, d));
    const float fb = feedback_.next();
    float fbL = wetL * fb;
    float fbR = wetR * fb;
    if (pingpong_) {
      std::swap(fbL, fbR);
    }
    bufferL_[write_] = l + fbL;
    bufferR_[write_] = r + fbR;
    write_ = (write_ + 1) % maxSamples_;

    const float mix = on_ ? mix_.next() : 0.0f;
    l = l * (1.0f - mix) + wetL * mix;
    r = r * (1.0f - mix) + wetR * mix;
  }

  void reset() noexcept {
    std::fill(bufferL_.begin(), bufferL_.end(), 0.0f);
    std::fill(bufferR_.begin(), bufferR_.end(), 0.0f);
    write_ = 0;
    modPhase_ = 0.0f;
    toneL_.reset();
    toneR_.reset();
  }

private:
  float read(const std::vector<float>& buf, float delaySamples) const noexcept {
    float pos = static_cast<float>(write_) - delaySamples;
    while (pos < 0.0f) {
      pos += static_cast<float>(maxSamples_);
    }
    const int i0 = static_cast<int>(pos) % maxSamples_;
    const int i1 = (i0 + 1) % maxSamples_;
    const float frac = pos - std::floor(pos);
    return buf[static_cast<size_t>(i0)] * (1.0f - frac) +
           buf[static_cast<size_t>(i1)] * frac;
  }

  float sr_ = 48000.0f;
  int maxSamples_ = 96000;
  int write_ = 0;
  std::vector<float> bufferL_;
  std::vector<float> bufferR_;
  Smoothed time_;
  Smoothed feedback_;
  Smoothed mix_;
  OnePoleLP toneL_;
  OnePoleLP toneR_;
  float modPhase_ = 0.0f;
  float modInc_ = 0.0f;
  float modDepth_ = 0.0f;
  bool on_ = false;
  bool pingpong_ = false;
};

} // namespace bass
