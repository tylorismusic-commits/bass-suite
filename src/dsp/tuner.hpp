// Bass Suite — chromatic tuner
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Lightweight autocorrelation tuner.  It only runs while the TUNE page is
// open, decimates to ~12 kHz and analyses a 1024-sample window every ~21 ms,
// so the audio thread cost stays small.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "common.hpp"

namespace bass {

class Tuner {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    decimation_ = std::max(1, static_cast<int>(std::lround(sr_ / 12000.0f)));
    srDec_ = sr_ / static_cast<float>(decimation_);
    lowpass_.set(1800.0f, sr_);
    buffer_.assign(kWindow, 0.0f);
    work_.assign(kWindow, 0.0f);
    correlation_.assign(static_cast<size_t>(maxLag()) + 2, 0.0f);
    reset();
  }

  void reset() noexcept {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    write_ = 0;
    filled_ = 0;
    decCount_ = 0;
    hopCount_ = 0;
    hz_ = 0.0f;
    cents_ = 0.0f;
    confidence_ = 0.0f;
    note_ = -1;
    smoothedHz_ = 0.0f;
    havePitch_ = false;
    lowpass_.reset();
  }

  void process(const float* left, const float* right, int frames,
               bool active) noexcept {
    if (!active) {
      if (note_ >= 0 || confidence_ > 0.0f) {
        reset();
      }
      return;
    }
    if (!left || !right || frames <= 0) {
      return;
    }
    for (int i = 0; i < frames; ++i) {
      const float mono = 0.5f * (left[i] + right[i]);
      const float filtered = lowpass_.process(mono);
      if (++decCount_ < decimation_) {
        continue;
      }
      decCount_ = 0;
      buffer_[static_cast<size_t>(write_)] = filtered;
      write_ = (write_ + 1) % kWindow;
      filled_ = std::min(filled_ + 1, kWindow);
      if (++hopCount_ >= kHop && filled_ >= kWindow) {
        hopCount_ = 0;
        analyse();
      }
    }
  }

  bool hasPitch() const noexcept { return note_ >= 0 && confidence_ > 0.55f; }
  float hz() const noexcept { return hz_; }
  float cents() const noexcept { return cents_; }
  int note() const noexcept { return note_; }
  float confidence() const noexcept { return confidence_; }

private:
  static constexpr int kWindow = 1024;
  static constexpr int kHop = 256;
  static constexpr float kMinHz = 28.0f;
  static constexpr float kMaxHz = 1400.0f;

  int maxLag() const noexcept {
    return std::min(kWindow / 2 - 1,
                    static_cast<int>(std::ceil(srDec_ / kMinHz)));
  }

  void analyse() noexcept {
    // Re-order the circular buffer oldest-to-newest.
    for (int i = 0; i < kWindow; ++i) {
      work_[static_cast<size_t>(i)] =
          buffer_[static_cast<size_t>((write_ + i) % kWindow)];
    }
    double mean = 0.0;
    for (float v : work_) {
      mean += v;
    }
    mean /= kWindow;
    double energy = 0.0;
    for (float& v : work_) {
      v = static_cast<float>(v - mean);
      energy += static_cast<double>(v) * v;
    }
    if (std::sqrt(energy / kWindow) < 0.0015) {
      hz_ = 0.0f;
      cents_ = 0.0f;
      confidence_ = 0.0f;
      note_ = -1;
      havePitch_ = false;
      return;
    }

    const int lo = std::max(2, static_cast<int>(srDec_ / kMaxHz));
    const int hi = maxLag();
    if (hi <= lo) {
      return;
    }
    float best = 0.0f;
    for (int lag = lo; lag <= hi; ++lag) {
      double ac = 0.0;
      double n1 = 0.0;
      double n2 = 0.0;
      const int n = kWindow - lag;
      for (int i = 0; i < n; ++i) {
        const double a = work_[static_cast<size_t>(i)];
        const double b = work_[static_cast<size_t>(i + lag)];
        ac += a * b;
        n1 += a * a;
        n2 += b * b;
      }
      const float r =
          static_cast<float>(ac / std::sqrt(n1 * n2 + 1.0e-20));
      correlation_[static_cast<size_t>(lag)] = r;
      best = std::max(best, r);
    }
    if (best < 0.55f) {
      hz_ = 0.0f;
      cents_ = 0.0f;
      confidence_ = best;
      note_ = -1;
      havePitch_ = false;
      return;
    }

    // Prefer the earliest strong peak: this avoids locking onto a harmonic
    // when the fundamental is present but slightly weaker.
    int peak = -1;
    const float threshold = std::max(0.55f, best * 0.86f);
    for (int lag = lo + 1; lag < hi; ++lag) {
      const float r = correlation_[static_cast<size_t>(lag)];
      if (r >= threshold &&
          r >= correlation_[static_cast<size_t>(lag - 1)] &&
          r >= correlation_[static_cast<size_t>(lag + 1)]) {
        peak = lag;
        break;
      }
    }
    if (peak < 0) {
      return;
    }
    const float a = correlation_[static_cast<size_t>(peak - 1)];
    const float b = correlation_[static_cast<size_t>(peak)];
    const float c = correlation_[static_cast<size_t>(peak + 1)];
    const float denom = a - 2.0f * b + c;
    const float delta = std::fabs(denom) > 1.0e-9f
                            ? 0.5f * (a - c) / denom
                            : 0.0f;
    const float lag = static_cast<float>(peak) + delta;
    if (lag <= 1.0f) {
      return;
    }
    float detected = srDec_ / lag;
    if (!std::isfinite(detected) || detected < kMinHz || detected > kMaxHz) {
      return;
    }

    if (havePitch_ && smoothedHz_ > 0.0f) {
      const float centsDiff =
          1200.0f * std::log2(detected / smoothedHz_);
      if (std::fabs(centsDiff) < 90.0f) {
        detected = 0.75f * smoothedHz_ + 0.25f * detected;
      }
    }
    smoothedHz_ = detected;
    havePitch_ = true;
    hz_ = detected;
    confidence_ = b;
    const float midi = 69.0f + 12.0f * std::log2(detected / 440.0f);
    const int nearest = static_cast<int>(std::lround(midi));
    note_ = std::clamp(nearest, 0, 127);
    const float noteHz = 440.0f * std::pow(2.0f, (nearest - 69) / 12.0f);
    cents_ = 1200.0f * std::log2(detected / noteHz);
  }

  float sr_ = 48000.0f;
  float srDec_ = 12000.0f;
  int decimation_ = 4;
  OnePoleLP lowpass_;
  std::vector<float> buffer_;
  std::vector<float> work_;
  std::vector<float> correlation_;
  int write_ = 0;
  int filled_ = 0;
  int decCount_ = 0;
  int hopCount_ = 0;
  float hz_ = 0.0f;
  float cents_ = 0.0f;
  float confidence_ = 0.0f;
  float smoothedHz_ = 0.0f;
  int note_ = -1;
  bool havePitch_ = false;
};

} // namespace bass
