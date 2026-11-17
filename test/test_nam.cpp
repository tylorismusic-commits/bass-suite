// Bass Suite — NAM loader smoke test
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsp/nam_model.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

#ifndef NAM_TEST_MODEL
#define NAM_TEST_MODEL "test/fixtures/wavenet.nam"
#endif

int main() {
  for (double sr : {44100.0, 48000.0, 96000.0}) {
    bass::NamDSP nam;
    if (!nam.load(NAM_TEST_MODEL, sr, 512)) {
      std::fprintf(stderr, "NAM load failed at %.0f Hz\n", sr);
      return 1;
    }
    if (!nam.valid() || nam.name().empty()) {
      std::fprintf(stderr, "NAM model did not report a name\n");
      return 1;
    }
    std::vector<float> left(512), right(512);
    double energy = 0.0;
    for (int block = 0; block < 8; ++block) {
      for (int i = 0; i < 512; ++i) {
        const double t = (block * 512 + i) / sr;
        left[i] = right[i] =
            static_cast<float>(0.1 * std::sin(2.0 * 3.14159265358979 * 220.0 * t));
      }
      nam.process(left.data(), right.data(), 512);
      for (int i = 0; i < 512; ++i) {
        if (!std::isfinite(left[i]) || !std::isfinite(right[i])) {
          std::fprintf(stderr, "NAM output is not finite\n");
          return 1;
        }
        energy += static_cast<double>(left[i]) * left[i];
      }
    }
    if (energy <= 0.0) {
      std::fprintf(stderr, "NAM output is silent at %.0f Hz\n", sr);
      return 1;
    }
  }
  std::printf("amp_nam: ok\n");
  return 0;
}
