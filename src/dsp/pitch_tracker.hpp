// Bass Suite — shared pitch tracker
// SPDX-License-Identifier: GPL-3.0-or-later
//
// YIN (cumulative mean normalised difference) on a decimated copy of the
// signal.  Plain autocorrelation is biased towards short lags when the
// fundamental is weak, which is exactly a low bass string, and locks onto a
// harmonic; for a divider that then means toggling on the wrong one.  The
// window is long enough for a down-tuned low B and the decimation keeps the
// cost of that window small.  Mirrors stomp::modules::PitchTracker in
// pedal-dsp, tuned for the bass range.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace bass {

class PitchTracker {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    decim_ = std::max(1, static_cast<int>(std::lround(sr_ / 8000.0f)));
    rate_ = sr_ / static_cast<float>(decim_);
    ring_.assign(kWindow, 0.0f);
    scratch_.assign(static_cast<size_t>(kWindow / 2 + 2), 0.0f);
    write_ = 0;
    filled_ = 0;
    hopCounter_ = 0;
    decimCounter_ = 0;
    decimSum_ = 0.0f;
    period_ = 0.0f;
    hz_ = 0.0f;
    confidence_ = 0.0f;
    updates_ = 0;
  }

  void reset() noexcept {
    std::fill(ring_.begin(), ring_.end(), 0.0f);
    write_ = 0;
    filled_ = 0;
    hopCounter_ = 0;
    decimCounter_ = 0;
    decimSum_ = 0.0f;
    period_ = 0.0f;
    hz_ = 0.0f;
    confidence_ = 0.0f;
    updates_ = 0;
  }

  inline void process(float mono) noexcept {
    // Box average over the decimation factor, which is a crude but cheap
    // anti-alias: bass pick noise has enough top end to confuse the tracker
    // if it folds down.
    decimSum_ += mono;
    if (++decimCounter_ < decim_) return;
    decimCounter_ = 0;
    ring_[static_cast<size_t>(write_)] = decimSum_ / static_cast<float>(decim_);
    decimSum_ = 0.0f;
    write_ = (write_ + 1) % kWindow;
    filled_ = std::min(filled_ + 1, kWindow);
    if (filled_ < kWindow) return;
    if (++hopCounter_ >= kHop) {
      hopCounter_ = 0;
      analyze();
    }
  }

  // Period in output samples (0 when nothing has been tracked yet).
  float periodSamples() const noexcept { return period_; }
  float hz() const noexcept { return hz_; }
  float confidence() const noexcept { return confidence_; }
  bool valid() const noexcept { return period_ > 0.0f; }
  // Increments whenever a new estimate lands, so consumers can rebuild
  // filters per estimate instead of per sample.
  unsigned updates() const noexcept { return updates_; }

private:
  // 128 ms at 8 kHz: four cycles of a low B, with enough overlap left at the
  // longest lag for the CMND dip.  A longer window tracks lower notes but
  // leaves the octave untracked for that much longer at load.
  static constexpr int kWindow = 1024;
  static constexpr int kHop = 256;
  static constexpr float kMinHz = 26.0f;
  static constexpr float kMaxHz = 400.0f;

  void analyze() noexcept {
    float w[kWindow];
    for (int i = 0; i < kWindow; ++i) {
      w[i] = ring_[static_cast<size_t>((write_ + i) % kWindow)];
    }
    const int minLag = std::max(2, static_cast<int>(rate_ / kMaxHz));
    const int maxLag =
        std::min(kWindow / 2, static_cast<int>(rate_ / kMinHz));
    if (maxLag <= minLag + 2) return;

    double energy = 0.0;
    for (int i = 0; i < kWindow; ++i) energy += static_cast<double>(w[i]) * w[i];
    if (energy < 1.0e-6) return;

    for (int lag = 1; lag <= maxLag; ++lag) {
      double diff = 0.0;
      for (int i = 0; i + lag < kWindow; ++i) {
        const double d = static_cast<double>(w[i]) - w[i + lag];
        diff += d * d;
      }
      scratch_[static_cast<size_t>(lag)] = static_cast<float>(diff);
    }

    double running = 0.0;
    scratch_[0] = 1.0f;
    for (int lag = 1; lag <= maxLag; ++lag) {
      running += scratch_[static_cast<size_t>(lag)];
      scratch_[static_cast<size_t>(lag)] =
          static_cast<float>(running > 0.0
                                 ? scratch_[static_cast<size_t>(lag)] * lag / running
                                 : 1.0);
    }

    // The first dip below the threshold is the fundamental: later dips are its
    // multiples, and picking one of those divides by two.
    int chosen = 0;
    for (int lag = minLag; lag < maxLag; ++lag) {
      const float v = scratch_[static_cast<size_t>(lag)];
      if (v < 0.15f && v <= scratch_[static_cast<size_t>(lag - 1)] &&
          v <= scratch_[static_cast<size_t>(lag + 1)]) {
        chosen = lag;
        break;
      }
    }
    if (!chosen) {
      int best = minLag;
      for (int lag = minLag; lag <= maxLag; ++lag) {
        if (scratch_[static_cast<size_t>(lag)] <
            scratch_[static_cast<size_t>(best)]) {
          best = lag;
        }
      }
      if (scratch_[static_cast<size_t>(best)] < 0.35f) chosen = best;
    }
    if (!chosen) return;  // keep the last period

    // Parabolic refinement around the dip.
    float refined = static_cast<float>(chosen);
    if (chosen > 1 && chosen < maxLag) {
      const float a = scratch_[static_cast<size_t>(chosen - 1)];
      const float b = scratch_[static_cast<size_t>(chosen)];
      const float c = scratch_[static_cast<size_t>(chosen + 1)];
      const float denom = a - 2.0f * b + c;
      if (std::fabs(denom) > 1.0e-9f) {
        const float delta = 0.5f * (a - c) / denom;
        refined += std::max(-0.5f, std::min(0.5f, delta));
      }
    }

    const float period = refined * static_cast<float>(decim_);
    // Follow a new note quickly; the divider only needs the period, not a
    // perfectly still estimate.
    period_ = period_ > 0.0f ? period_ + (period - period_) * 0.6f : period;
    hz_ = period_ > 0.0f ? sr_ / period_ : 0.0f;
    confidence_ = 1.0f - scratch_[static_cast<size_t>(chosen)];
    ++updates_;
  }

  float sr_ = 48000.0f;
  float rate_ = 8000.0f;
  int decim_ = 6;
  int write_ = 0;
  int filled_ = 0;
  int hopCounter_ = 0;
  int decimCounter_ = 0;
  float decimSum_ = 0.0f;
  float period_ = 0.0f;
  float hz_ = 0.0f;
  float confidence_ = 0.0f;
  unsigned updates_ = 0;
  std::vector<float> ring_;
  std::vector<float> scratch_;
};

} // namespace bass
