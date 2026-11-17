// Bass Suite — stereo plate-ish FDN reverb
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Four-line feedback delay network with damping, pre-delay and a wet/dry mix.
// The same network serves the Plate/Spring switch for now; Spring is darker
// and shorter.  Bypass keeps the network running so trails survive.

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "common.hpp"
#include "params.hpp"

namespace bass {

class Reverb {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate;
    static const float kL[4] = {29.7f, 37.1f, 41.1f, 43.7f};
    static const float kR[4] = {31.3f, 38.9f, 42.7f, 45.1f};
    for (int i = 0; i < 4; ++i) {
      lineL_[i].size = std::max(8, static_cast<int>(sr_ * kL[i] * 0.001f));
      lineR_[i].size = std::max(8, static_cast<int>(sr_ * kR[i] * 0.001f));
      lineL_[i].buf.assign(static_cast<size_t>(lineL_[i].size), 0.0f);
      lineR_[i].buf.assign(static_cast<size_t>(lineR_[i].size), 0.0f);
      lineL_[i].write = 0;
      lineR_[i].write = 0;
    }
    preSize_ = std::max(16, static_cast<int>(sr_ * 0.2f) + 2);
    preL_.assign(static_cast<size_t>(preSize_), 0.0f);
    preR_.assign(static_cast<size_t>(preSize_), 0.0f);
    preWrite_ = 0;
    pre_.setTime(50.0f, sr_);
    pre_.reset(0.0f);
    mix_.setTime(20.0f, sr_);
    mix_.reset(0.0f);
    decay_.setTime(30.0f, sr_);
    decay_.reset(0.7f);
    reset();
  }

  void setTargets(const Controls& c) noexcept {
    on_ = c.reverbOn;
    float decay = 0.55f + 0.42f * clampf(c.reverbDecay, 0.0f, 1.0f);
    float toneHz = 500.0f * std::pow(20.0f, clampf(c.reverbTone, 0.0f, 1.0f));
    if (c.reverbMode == 1) {  // Spring: darker and a little shorter.
      decay *= 0.92f;
      toneHz *= 0.65f;
    }
    decay_.setTarget(decay);
    mix_.setTarget(clampf(c.reverbMix, 0.0f, 1.0f));
    pre_.setTarget(clampf(c.reverbPreDelay, 0.0f, 1.0f) * 0.2f * sr_);
    dampG_ = 1.0f - std::exp(-2.0f * kPi * clampf(toneHz, 200.0f, sr_ * 0.45f) / sr_);
  }

  void process(float& l, float& r) noexcept {
    // Pre-delay.
    preL_[preWrite_] = l;
    preR_[preWrite_] = r;
    const int preSamples = static_cast<int>(std::lround(pre_.next()));
    int read = preWrite_ - preSamples;
    while (read < 0) {
      read += preSize_;
    }
    const float inL = preL_[read];
    const float inR = preR_[read];
    preWrite_ = (preWrite_ + 1) % preSize_;

    float dL[4], dR[4];
    for (int i = 0; i < 4; ++i) {
      dL[i] = lineL_[i].buf[lineL_[i].write];
      dR[i] = lineR_[i].buf[lineR_[i].write];
      dampL_[i] += (dL[i] - dampL_[i]) * dampG_;
      dampR_[i] += (dR[i] - dampR_[i]) * dampG_;
      dL[i] = dampL_[i];
      dR[i] = dampR_[i];
    }

    const float g = decay_.next();
    float fbL[4], fbR[4];
    fbL[0] = (dL[0] + dL[1] + dL[2] + dL[3]) * 0.5f * g;
    fbL[1] = (dL[0] - dL[1] + dL[2] - dL[3]) * 0.5f * g;
    fbL[2] = (dL[0] + dL[1] - dL[2] - dL[3]) * 0.5f * g;
    fbL[3] = (dL[0] - dL[1] - dL[2] + dL[3]) * 0.5f * g;
    fbR[0] = (dR[0] + dR[1] + dR[2] + dR[3]) * 0.5f * g;
    fbR[1] = (dR[0] - dR[1] + dR[2] - dR[3]) * 0.5f * g;
    fbR[2] = (dR[0] + dR[1] - dR[2] - dR[3]) * 0.5f * g;
    fbR[3] = (dR[0] - dR[1] - dR[2] + dR[3]) * 0.5f * g;

    for (int i = 0; i < 4; ++i) {
      lineL_[i].buf[lineL_[i].write] = inL * 0.35f + fbL[i];
      lineR_[i].buf[lineR_[i].write] = inR * 0.35f + fbR[i];
      lineL_[i].write = (lineL_[i].write + 1) % lineL_[i].size;
      lineR_[i].write = (lineR_[i].write + 1) % lineR_[i].size;
    }

    float wetL = (dL[0] + dL[1] + dL[2] + dL[3]) * 0.18f;
    float wetR = (dR[0] + dR[1] + dR[2] + dR[3]) * 0.18f;
    // A touch of cross-feed keeps the tail wide without a hard ping-pong.
    const float crossL = 0.85f * wetL + 0.15f * wetR;
    const float crossR = 0.85f * wetR + 0.15f * wetL;
    wetL = crossL;
    wetR = crossR;

    const float mix = on_ ? mix_.next() : 0.0f;
    l = l * (1.0f - mix) + wetL * mix;
    r = r * (1.0f - mix) + wetR * mix;
  }

  void reset() noexcept {
    for (int i = 0; i < 4; ++i) {
      std::fill(lineL_[i].buf.begin(), lineL_[i].buf.end(), 0.0f);
      std::fill(lineR_[i].buf.begin(), lineR_[i].buf.end(), 0.0f);
      lineL_[i].write = 0;
      lineR_[i].write = 0;
      dampL_[i] = 0.0f;
      dampR_[i] = 0.0f;
    }
    std::fill(preL_.begin(), preL_.end(), 0.0f);
    std::fill(preR_.begin(), preR_.end(), 0.0f);
    preWrite_ = 0;
  }

private:
  struct Line {
    std::vector<float> buf;
    int size = 0;
    int write = 0;
  };

  float sr_ = 48000.0f;
  Line lineL_[4];
  Line lineR_[4];
  float dampL_[4] = {};
  float dampR_[4] = {};
  float dampG_ = 0.3f;
  std::vector<float> preL_;
  std::vector<float> preR_;
  int preSize_ = 0;
  int preWrite_ = 0;
  Smoothed pre_;
  Smoothed mix_;
  Smoothed decay_;
  bool on_ = false;
};

} // namespace bass
