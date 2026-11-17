// Bass Suite — Guitarix/CAPS-derived passive tone stack
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Port of Guitarix's `tonestack(tse)` (GPL-2.0-or-later).  The component
// values originate from CAPS (Tim Goetze), based on D. T. Yeh's work.

#pragma once

#include <algorithm>
#include <cmath>

namespace bass {

struct ToneStackSpec {
  float r1 = 250000.0f;  // treble pot
  float r2 = 1000000.0f; // bass pot
  float r3 = 25000.0f;   // middle pot
  float r4 = 56000.0f;   // slope resistor
  float c1 = 250.0e-12f;
  float c2 = 20.0e-9f;
  float c3 = 20.0e-9f;
};

// Third-order IIR derived from the passive network.  The bass control uses the
// same exponential taper as Guitarix (bass 0..1).
class ToneStack {
public:
  void set(const ToneStackSpec& spec, float bass, float middle, float treble,
           float sampleRate) noexcept {
    const double t = std::clamp(static_cast<double>(treble), 0.0, 1.0);
    const double m = std::clamp(static_cast<double>(middle), 0.0, 1.0);
    const double l = std::exp((static_cast<double>(bass) - 1.0) * 3.4);

    const double C1 = spec.c1;
    const double C2 = spec.c2;
    const double C3 = spec.c3;
    const double R1 = spec.r1;
    const double R2 = spec.r2;
    const double R3 = spec.r3;
    const double R4 = spec.r4;

    const double b1 = t * C1 * R1 + m * C3 * R3 +
                      l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);

    const double b2 =
        t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) -
        m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) +
        m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) +
        l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4) +
        l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) +
        (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);

    const double b3 =
        l * m * (C1 * C2 * C3 * R1 * R2 * R3 +
                 C1 * C2 * C3 * R2 * R3 * R4) -
        m * m * (C1 * C2 * C3 * R1 * R3 * R3 +
                 C1 * C2 * C3 * R3 * R3 * R4) +
        m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) +
        t * C1 * C2 * C3 * R1 * R3 * R4 -
        t * m * C1 * C2 * C3 * R1 * R3 * R4 +
        t * l * C1 * C2 * C3 * R1 * R2 * R4;

    const double a0 = 1.0;
    const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) +
                      m * C3 * R3 + l * (C1 * R2 + C2 * R2);

    const double a2 =
        m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 +
             C2 * C3 * R3 * R3) +
        l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) -
        m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) +
        l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 +
             C2 * C3 * R2 * R4) +
        (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 +
         C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);

    const double a3 =
        l * m * (C1 * C2 * C3 * R1 * R2 * R3 +
                 C1 * C2 * C3 * R2 * R3 * R4) -
        m * m * (C1 * C2 * C3 * R1 * R3 * R3 +
                 C1 * C2 * C3 * R3 * R3 * R4) +
        m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 -
             C1 * C2 * C3 * R1 * R3 * R4) +
        l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

    const double c = 2.0 * static_cast<double>(sampleRate);
    const double c2v = c * c;
    const double c3v = c2v * c;

    const double B0 = -b1 * c - b2 * c2v - b3 * c3v;
    const double B1 = -b1 * c + b2 * c2v + 3.0 * b3 * c3v;
    const double B2 = b1 * c + b2 * c2v - 3.0 * b3 * c3v;
    const double B3 = b1 * c - b2 * c2v + b3 * c3v;
    const double A0 = -a0 - a1 * c - a2 * c2v - a3 * c3v;
    const double A1 = -3.0 * a0 - a1 * c + a2 * c2v + 3.0 * a3 * c3v;
    const double A2 = -3.0 * a0 + a1 * c + a2 * c2v - 3.0 * a3 * c3v;
    const double A3 = -a0 + a1 * c - a2 * c2v + a3 * c3v;

    if (std::fabs(A0) < 1.0e-30) {
      return;
    }
    const double inv = 1.0 / A0;
    b0_ = B0 * inv;
    b1_ = B1 * inv;
    b2_ = B2 * inv;
    b3_ = B3 * inv;
    a1_ = A1 * inv;
    a2_ = A2 * inv;
    a3_ = A3 * inv;
  }

  void reset() noexcept {
    x1_ = x2_ = x3_ = 0.0;
    y1_ = y2_ = y3_ = 0.0;
  }

  float process(float x) noexcept {
    const double xd = x;
    double y = b0_ * xd + b1_ * x1_ + b2_ * x2_ + b3_ * x3_ -
               a1_ * y1_ - a2_ * y2_ - a3_ * y3_;
    if (!std::isfinite(y)) {
      reset();
      return 0.0f;
    }
    x3_ = x2_;
    x2_ = x1_;
    x1_ = xd;
    y3_ = y2_;
    y2_ = y1_;
    y1_ = y;
    return static_cast<float>(y);
  }

private:
  double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0, b3_ = 0.0;
  double a1_ = 0.0, a2_ = 0.0, a3_ = 0.0;
  double x1_ = 0.0, x2_ = 0.0, x3_ = 0.0;
  double y1_ = 0.0, y2_ = 0.0, y3_ = 0.0;
};

} // namespace bass
