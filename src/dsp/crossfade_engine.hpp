// Bass Suite — dual-engine amp switcher
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The single Engine has a short mute for model/IR changes.  For the plug-in
// path we run two complete engines: the standby is configured, reset and then
// warmed with the live input before it is heard.  The switch itself fades the
// old engine out, swaps at silence and fades the new one in.  An earlier
// equal-gain crossfade summed two differently-phased amps and produced a deep
// cancellation dip (heard as a pop) in the middle of every switch.

#pragma once

#include <algorithm>
#include <atomic>
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
    transition_ = 0;
    transitionGain_ = 1.0f;
    warmRemaining_ = 0;
    switchOutStep_ = 1.0f / std::max(1.0f, sr_ * 0.005f);
    switchInStep_ = 1.0f / std::max(1.0f, sr_ * 0.008f);
    warmSamples_ = std::max(1, static_cast<int>(sr_ * 0.015f));
    reload_.store(false, std::memory_order_relaxed);
    startupGain_ = 0.0f;
    startupStep_ = 1.0f / std::max(1.0f, sr_ * 0.050f);
    namMuteActive_ = false;
    namMuteIn_ = false;
    namApplyReady_ = false;
    namMuteGain_ = 1.0f;
    namMuteOutStep_ = 1.0f / std::max(1.0f, sr_ * 0.015f);
    namMuteInStep_ = 1.0f / std::max(1.0f, sr_ * 0.040f);
    namModelChanged_ = false;
    lastNamModel_.reset();
    hasControls_ = false;
    last_ = Controls{};
    scratchSize_ = 8192;
    scratchL_.assign(scratchSize_, 0.0f);
    scratchR_.assign(scratchSize_, 0.0f);
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
    if (model != lastNamModel_) {
      lastNamModel_ = model;
      namModelChanged_ = true;
    }
    if (a_) a_->setNamModel(model);
    if (b_) b_->setNamModel(model);
  }

  // Ask for a fresh switch even when the model and cab are unchanged.  Used
  // for preset and state loads, which replace many controls at once and would
  // otherwise step the running amp's gain/tone coefficients in place.
  void requestReload() noexcept {
    reload_.store(true, std::memory_order_relaxed);
  }

  void reset() noexcept {
    if (a_) a_->reset();
    if (b_) b_->reset();
    transition_ = 0;
    transitionGain_ = 1.0f;
    warmRemaining_ = 0;
    reload_.store(false, std::memory_order_relaxed);
    startupGain_ = 0.0f;
    namMuteActive_ = false;
    namMuteIn_ = false;
    namApplyReady_ = false;
    namMuteGain_ = 1.0f;
    namModelChanged_ = false;
    lastNamModel_.reset();
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
      namModelChanged_ = false;
      reload_.store(false, std::memory_order_relaxed);
      hasControls_ = true;
      return;
    }

    // Power is a true bypass and must be immediate; cancel any transition.
    if (c.enabled != last_.enabled) {
      if (c.enabled && !last_.enabled) {
        // Power-on returns from true bypass; ramp the processed path in.
        startupGain_ = 0.0f;
      }
      transition_ = 0;
      transitionGain_ = 1.0f;
      warmRemaining_ = 0;
      namMuteActive_ = false;
      namMuteIn_ = false;
      namApplyReady_ = false;
      namMuteGain_ = 1.0f;
      namModelChanged_ = false;
      reload_.store(false, std::memory_order_relaxed);
      if (a_) a_->setControls(c);
      if (b_) b_->setControls(c);
      last_ = c;
      return;
    }

    // Do not start or modify a switch while one is in flight.  The latest
    // controls are picked up on the block after the fade-in completes.
    if (transition_ != 0) {
      return;
    }

    const bool forced = reload_.load(std::memory_order_relaxed);

    // Two engines must never process the same NAM DSP at once.  With NAM
    // already on, use a short mute around cab/model/IR changes instead.
    if (c.namOn && last_.namOn) {
      if (namMuteActive_ || hardChange(last_, c) || namModelChanged_ ||
          forced) {
        if (forced) {
          reload_.store(false, std::memory_order_relaxed);
        }
        namPending_ = c;
        if (!namMuteActive_) {
          namMuteActive_ = true;
          namMuteIn_ = false;
          namMuteGain_ = 1.0f;
        }
        return;
      }
      activeEngine()->setControls(c);
      last_ = c;
      return;
    }

    const bool namToggle = c.namOn != last_.namOn;
    if (namToggle) {
      namMuteActive_ = false;
      namMuteIn_ = false;
      namApplyReady_ = false;
      namMuteGain_ = 1.0f;
      namModelChanged_ = false;
    }
    if (hardChange(last_, c) || namToggle || forced) {
      if (forced) {
        reload_.store(false, std::memory_order_relaxed);
      }
      Engine* standby = standbyEngine();
      standby->configure(c);
      standby->reset();
      transition_ = 1;
      transitionGain_ = 1.0f;
      warmRemaining_ = warmSamples_;
      return;
    }

    activeEngine()->setControls(c);
    last_ = c;
    namModelChanged_ = false;
  }

  void process(float* left, float* right, int frames, Meters& meters) noexcept {
    if (!a_) {
      return;
    }
    if (namApplyReady_) {
      activeEngine()->setControls(namPending_);
      last_ = namPending_;
      namApplyReady_ = false;
      namMuteIn_ = true;
      namModelChanged_ = false;
    }

    int offset = 0;
    while (offset < frames) {
      if (transition_ == 0) {
        activeEngine()->process(left + offset, right + offset, frames - offset,
                                meters);
        offset = frames;
        break;
      }
      if (transition_ == 1) {
        // Warm the standby with the live input while the old amp is still
        // heard.  Its filters, DC blockers and cab settle before the swap.
        const int n = std::min(frames - offset, warmRemaining_);
        if (n <= 0) {
          transition_ = 2;
          transitionGain_ = 1.0f;
          continue;
        }
        processStandby(left + offset, right + offset, n);
        activeEngine()->process(left + offset, right + offset, n, meters);
        warmRemaining_ -= n;
        offset += n;
        if (warmRemaining_ <= 0) {
          transition_ = 2;
          transitionGain_ = 1.0f;
        }
        continue;
      }
      if (transition_ == 2) {
        // Fade the old amp out.  No summing of two amps, so no cancellation.
        const int untilZero =
            std::max(1, static_cast<int>(std::ceil(
                            static_cast<double>(transitionGain_) /
                            static_cast<double>(switchOutStep_))));
        const int n = std::min(frames - offset, untilZero);
        processStandby(left + offset, right + offset, n);
        activeEngine()->process(left + offset, right + offset, n, meters);
        for (int i = 0; i < n; ++i) {
          transitionGain_ -= switchOutStep_;
          if (transitionGain_ < 0.0f) {
            transitionGain_ = 0.0f;
          }
          left[offset + i] *= transitionGain_;
          right[offset + i] *= transitionGain_;
        }
        offset += n;
        if (transitionGain_ <= 0.0f) {
          active_ ^= 1;
          standbyEngine()->reset();
          transition_ = 3;
          transitionGain_ = 0.0f;
        }
        continue;
      }
      // transition_ == 3: fade the new amp in.
      const int untilOne =
          std::max(1, static_cast<int>(std::ceil(
                          (1.0 - static_cast<double>(transitionGain_)) /
                          static_cast<double>(switchInStep_))));
      const int n = std::min(frames - offset, untilOne);
      activeEngine()->process(left + offset, right + offset, n, meters);
      for (int i = 0; i < n; ++i) {
        left[offset + i] *= transitionGain_;
        right[offset + i] *= transitionGain_;
        transitionGain_ += switchInStep_;
        if (transitionGain_ > 1.0f) {
          transitionGain_ = 1.0f;
        }
      }
      offset += n;
      if (transitionGain_ >= 1.0f) {
        transition_ = 0;
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

    // NAM cab/model changes use a short mute because both engines cannot
    // share the same NAM DSP during a switch.
    if (namMuteActive_) {
      for (int i = 0; i < frames; ++i) {
        if (!namMuteIn_) {
          namMuteGain_ -= namMuteOutStep_;
          if (namMuteGain_ <= 0.0f) {
            namMuteGain_ = 0.0f;
            namApplyReady_ = true;
          }
        } else {
          namMuteGain_ += namMuteInStep_;
          if (namMuteGain_ >= 1.0f) {
            namMuteGain_ = 1.0f;
            namMuteActive_ = false;
          }
        }
        left[i] *= namMuteGain_;
        right[i] *= namMuteGain_;
      }
    }
  }

private:
  void processStandby(const float* left, const float* right,
                      int frames) noexcept {
    Meters ignored{};
    int offset = 0;
    while (offset < frames) {
      const int n = std::min(frames - offset, scratchSize_);
      std::memcpy(scratchL_.data(), left + offset,
                  static_cast<size_t>(n) * sizeof(float));
      std::memcpy(scratchR_.data(), right + offset,
                  static_cast<size_t>(n) * sizeof(float));
      standbyEngine()->process(scratchL_.data(), scratchR_.data(), n, ignored);
      offset += n;
    }
  }

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
  // 0 idle, 1 warming the standby, 2 fading the old amp out, 3 fading in.
  int transition_ = 0;
  float transitionGain_ = 1.0f;
  int warmRemaining_ = 0;
  int warmSamples_ = 1;
  float switchOutStep_ = 1.0f / 384.0f;
  float switchInStep_ = 1.0f / 720.0f;
  std::atomic<bool> reload_{false};
  Controls last_{};
  bool hasControls_ = false;
  float sr_ = 48000.0f;
  float startupGain_ = 0.0f;
  float startupStep_ = 0.0f;
  bool namMuteActive_ = false;
  bool namMuteIn_ = false;
  bool namApplyReady_ = false;
  bool namModelChanged_ = false;
  std::shared_ptr<NamDSP> lastNamModel_;
  float namMuteGain_ = 1.0f;
  float namMuteOutStep_ = 0.0f;
  float namMuteInStep_ = 0.0f;
  Controls namPending_{};
  int scratchSize_ = 8192;
  std::vector<float> scratchL_;
  std::vector<float> scratchR_;
};

} // namespace bass
