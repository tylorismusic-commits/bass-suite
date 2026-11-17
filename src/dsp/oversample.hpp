// Bass Suite — 2x/4x oversampling
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A small polyphase FIR half-band pair for the nonlinear power stage.  The
// 31-tap Blackman-windowed low-pass is short enough for per-sample use and
// keeps the aliases well below the noise floor at guitar bandwidths.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace bass {

class Oversampler2x {
public:
  static constexpr int kTaps = 31;

  void prepare() noexcept {
    // Ideal low-pass at half the original Nyquist (fc = 0.25 of the
    // oversampled rate), Blackman-windowed, DC-normalized.
    double h[kTaps];
    double sum = 0.0;
    constexpr double kPi = 3.14159265358979323846;
    for (int n = 0; n < kTaps; ++n) {
      const int m = n - (kTaps - 1) / 2;
      const double sinc = m == 0 ? 0.5 : std::sin(0.5 * kPi * m) / (kPi * m);
      const double w =
          0.42 - 0.5 * std::cos(2.0 * kPi * n / (kTaps - 1)) +
          0.08 * std::cos(4.0 * kPi * n / (kTaps - 1));
      h[n] = sinc * w;
      sum += h[n];
    }
    if (std::fabs(sum) > 1.0e-12) {
      for (int n = 0; n < kTaps; ++n) {
        h_[n] = static_cast<float>(h[n] / sum);
      }
    }
    reset();
  }

  void reset() noexcept {
    std::fill(up_, up_ + kTaps + 2, 0.0f);
    std::fill(down_, down_ + kTaps + 2, 0.0f);
  }

  // Consumes one base-rate sample and produces two oversampled samples.
  void up(float x, float& a, float& b) noexcept {
    for (int i = kTaps + 1; i >= 2; --i) {
      up_[i] = up_[i - 2];
    }
    up_[1] = x;
    up_[0] = 0.0f;
    a = 2.0f * dot(up_, 0);
    b = 2.0f * dot(up_, 1);
  }

  // Consumes two oversampled samples and produces one base-rate sample.
  float down(float a, float b) noexcept {
    for (int i = kTaps + 1; i >= 2; --i) {
      down_[i] = down_[i - 1];
    }
    down_[1] = a;
    down_[0] = b;
    return dot(down_, 0);
  }

private:
  float dot(const float* x, int offset) const noexcept {
    float y = 0.0f;
    for (int k = 0; k < kTaps; ++k) {
      y += h_[k] * x[offset + k];
    }
    return y;
  }

  float h_[kTaps] = {};
  // Two extra slots let the up history shift by two and the down history by
  // one without a modulo index.
  float up_[kTaps + 2] = {};
  float down_[kTaps + 2] = {};
};

} // namespace bass
