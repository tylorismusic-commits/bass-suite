// Bass Suite — geometry test
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/geometry.hpp"

#include <cmath>
#include <cstdio>

int main() {
  const auto full = bass::sizeForScale(1.0);
  if (full.width != 1040 || full.height != 640) {
    std::fprintf(stderr, "1x size is wrong\n");
    return 1;
  }
  const auto half = bass::sizeForScale(0.5);
  if (half.width != 520 || half.height != 320) {
    std::fprintf(stderr, "0.5x size is wrong\n");
    return 1;
  }
  const auto snapped = bass::snapSize(1200, 700);
  const double ratio =
      static_cast<double>(snapped.width) / static_cast<double>(snapped.height);
  if (std::fabs(ratio - 1040.0 / 640.0) > 1e-9) {
    std::fprintf(stderr, "aspect ratio is not preserved\n");
    return 1;
  }
  if (std::fabs(bass::clampScale(0.49) - 0.5) > 1e-9) {
    std::fprintf(stderr, "scale clamp failed\n");
    return 1;
  }
  std::printf("amp_geometry: ok\n");
  return 0;
}
