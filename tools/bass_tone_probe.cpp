#include <algorithm>
// Bass Suite — tone-stack level probe
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsp/recipe.hpp"

#include <cmath>
#include <cstdio>

int main() {
  constexpr float sr = 48000.0f;
  constexpr int n = 48000;
  const char* names[] = {"Bassman", "JTM45", "JCM800", "AC30", "Peavey",
                         "Mesa",    "Soldano", "Bogner", "Roland", "MLead"};
  for (int k = 0; k < 10; ++k) {
    bass::ToneStack ts;
    ts.set(bass::toneStackSpec(static_cast<bass::ToneStackKind>(k)), 0.5f, 0.45f,
           0.55f, sr);
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
      const float x =
          0.25f * std::sin(2.0 * 3.14159265358979 * 1000.0 * i / sr);
      const float y = ts.process(x);
      if (i > n / 2) {
        sum += static_cast<double>(y) * y;
      }
    }
    const double rms = std::sqrt(sum / (n / 2));
    std::printf("%-8s 1 kHz gain %6.1f dB\n", names[k],
                20.0 * std::log10(std::max(rms / 0.25 / 0.70710678, 1e-9)));
  }
  return 0;
}
