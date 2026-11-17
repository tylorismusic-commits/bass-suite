// Bass Suite — dual-engine crossfade wrapper
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The single Engine has a short mute for model/IR changes.  For the plug-in
// path we instead run two complete engines and equal-power crossfade between
// them, so a model or cab change never has to pass through silence.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "engine.hpp"

namespace bass {

class CrossfadeEngine {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate;
    if (!a_) {
      a_ = std::make_unique<Engine>();
      b_ = std::make_unique<Engine>();
    }
    a_->prepare(sr_);
    b_->prepare(sr_);
    active_ = 0;
    phase_ = 0;
    crossfadeCount_ = 0;
    crossfadeSamples_ = std::max(1, static_cast<int>(sr_ * 0.040f));
    startupGain_ = 0.0f;
    startupStep_ = 1.0f / std::max(1.0f, sr_ * 0.050f);
    hasControls_ = false;
    last_ = Controls{};
    scratchSize_ = 8192;
    oldL_.assign(scratchSize_, 0.0f);
    oldR_.assign(scratchSize_, 0.0f);
    newL_.assign(scratchSize_, 0.0f);
    newR_.assign(scratchSize_, 0.0f);
  }

  bool loadFactoryCabs(const std::string& dir) noexcept {
    const bool a = a_ && a_->loadFactoryCabs(dir);
    const bool b = b_ && b_->loadFactoryCabs(dir);
    return a && b;
  }

  bool cabReady() const noexcept {
    return a_ && b_ && a_->cabReady() && b_->cabReady();
  }

  void setUserIrBank(std::shared_ptr<const UserIrBank> bank) noexcept {
    if (a_) a_->setUserIrBank(bank);
    if (b_) b_->setUserIrBank(bank);
  }

  void setNamModel(std::shared_ptr<NamDSP> model) noexcept {
    if (a_) a_->setNamModel(model);
    if (b_) b_->setNamModel(model);
  }



  void reset() noexcept {
    if (a_) a_->reset();
    if (b_) b_->reset();
    phase_ = 0;
    crossfadeCount_ = 0;
    startupGain_ = 0.0f;
    hasControls_ = false;
  }

  uint32_t latencySamples() const noexcept { return 0; }

  void setControls(const Controls& c) noexcept {
    if (!a_) {
      return;
    }
    if (!hasControls_) {
      activeEngine()->configure(c);
      activeEngine()->reset();
      last_ = c;
      hasControls_ = true;
      return;
    }

    // Power is a true bypass and must be immediate; cancel any transition.
    if (c.enabled != last_.enabled) {
      phase_ = 0;
      if (a_) a_->setControls(c);
      if (b_) b_->setControls(c);
      last_ = c;
      return;
    }

    // Two engines must never process the same NAM DSP at once, so cab/model
    // changes with NAM already on keep the single-engine fade path.
    if (c.namOn && last_.namOn) {
      phase_ = 0;
      activeEngine()->setControls(c);
      last_ = c;
      return;
    }

    const bool namToggle = c.namOn != last_.namOn;
    if (hardChange(last_, c) || namToggle) {
      if (phase_ == 0) {
        Engine* standby = standbyEngine();
        standby->configure(c);
        standby->reset();
        crossfadeCount_ = 0;
        phase_ = 1;
      } else {
        // Keep the current transition; the host will send the latest set
        // again next block and a new crossfade can start after this one.
      }
      return;
    }

    activeEngine()->setControls(c);
    last_ = c;
  }

  void process(float* left, float* right, int frames, Meters& meters) noexcept {
    if (!a_) {
      return;
    }
    if (phase_ == 0) {
      activeEngine()->process(left, right, frames, meters);
    } else {
      Meters oldMeters{};
      Meters newMeters{};
      int offset = 0;
      while (offset < frames) {
        const int n = std::min(frames - offset, scratchSize_);
        for (int i = 0; i < n; ++i) {
          const float l = left[offset + i];
          const float r = right[offset + i];
          oldL_[static_cast<size_t>(i)] = l;
          oldR_[static_cast<size_t>(i)] = r;
          newL_[static_cast<size_t>(i)] = l;
          newR_[static_cast<size_t>(i)] = r;
        }
        activeEngine()->process(oldL_.data(), oldR_.data(), n, oldMeters);
        standbyEngine()->process(newL_.data(), newR_.data(), n, newMeters);
        for (int i = 0; i < n; ++i) {
          const int k = std::min(crossfadeCount_ + i, crossfadeSamples_);
          const float t = static_cast<float>(k) /
                          static_cast<float>(crossfadeSamples_);
          // Linear crossfade: the two engines see the same input, so their
          // outputs are correlated and sum to one without a mid-fade bump.
          const float gOld = 1.0f - t;
          const float gNew = t;
          left[offset + i] = oldL_[static_cast<size_t>(i)] * gOld +
                             newL_[static_cast<size_t>(i)] * gNew;
          right[offset + i] = oldR_[static_cast<size_t>(i)] * gOld +
                              newR_[static_cast<size_t>(i)] * gNew;
        }
        crossfadeCount_ += n;
        offset += n;
      }
      meters = oldMeters;
      if (crossfadeCount_ >= crossfadeSamples_) {
        active_ ^= 1;
        phase_ = 0;
        crossfadeCount_ = 0;
        standbyEngine()->reset();
        last_ = activeEngine()->currentControls();
      }
    }

    // First block after prepare()/reset(): ramp in so the host cannot start
    // on a filter/cab transient.  A powered-off engine stays true bypass.
    if (!activeEngine()->currentControls().enabled) {
      startupGain_ = 1.0f;
    } else if (startupGain_ < 1.0f) {
      for (int i = 0; i < frames; ++i) {
        left[i] *= startupGain_;
        right[i] *= startupGain_;
        startupGain_ += startupStep_;
        if (startupGain_ > 1.0f) {
          startupGain_ = 1.0f;
        }
      }
    }
  }

private:
  Engine* activeEngine() noexcept {
    return active_ == 0 ? a_.get() : b_.get();
  }
  Engine* standbyEngine() noexcept {
    return active_ == 0 ? b_.get() : a_.get();
  }

  static bool hardChange(const Controls& a, const Controls& b) noexcept {
    return a.model != b.model || a.cab != b.cab || a.cabMic != b.cabMic ||
           a.cabSet != b.cabSet || a.cabBypass != b.cabBypass ||
           a.ampBypass != b.ampBypass ||
           std::memcmp(a.userIr, b.userIr, sizeof(a.userIr)) != 0;
  }

  std::unique_ptr<Engine> a_;
  std::unique_ptr<Engine> b_;
  int active_ = 0;
  int phase_ = 0;  // 0 idle, 1 equal-power crossfade
  int crossfadeCount_ = 0;
  int crossfadeSamples_ = 1;
  Controls last_{};
  bool hasControls_ = false;
  float sr_ = 48000.0f;
  float startupGain_ = 0.0f;
  float startupStep_ = 0.0f;
  int scratchSize_ = 8192;
  std::vector<float> oldL_;
  std::vector<float> oldR_;
  std::vector<float> newL_;
  std::vector<float> newR_;
};

} // namespace bass
