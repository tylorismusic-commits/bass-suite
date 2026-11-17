// Bass Suite — user IR library and convolution test
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsp/engine.hpp"
#include "dsp/user_ir.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include <algorithm>
#include <unistd.h>

namespace {

void writeWav(const std::string& path, const std::vector<float>& samples,
              int sampleRate) {
  std::ofstream out(path, std::ios::binary);
  const uint16_t channels = 1;
  const uint16_t bits = 16;
  const uint32_t dataSize =
      static_cast<uint32_t>(samples.size() * sizeof(int16_t));
  const uint32_t riffSize = 36u + dataSize;
  out.write("RIFF", 4);
  out.write(reinterpret_cast<const char*>(&riffSize), 4);
  out.write("WAVEfmt ", 8);
  const uint32_t fmtSize = 16;
  const uint16_t format = 1;
  const uint32_t rate = static_cast<uint32_t>(sampleRate);
  const uint32_t byteRate = rate * channels * bits / 8;
  const uint16_t blockAlign = channels * bits / 8;
  out.write(reinterpret_cast<const char*>(&fmtSize), 4);
  out.write(reinterpret_cast<const char*>(&format), 2);
  out.write(reinterpret_cast<const char*>(&channels), 2);
  out.write(reinterpret_cast<const char*>(&rate), 4);
  out.write(reinterpret_cast<const char*>(&byteRate), 4);
  out.write(reinterpret_cast<const char*>(&blockAlign), 2);
  out.write(reinterpret_cast<const char*>(&bits), 2);
  out.write("data", 4);
  out.write(reinterpret_cast<const char*>(&dataSize), 4);
  for (float f : samples) {
    const double clamped = std::min(std::max(static_cast<double>(f), -1.0),
                                    1.0);
    const int16_t v = static_cast<int16_t>(clamped * 32000.0);
    out.write(reinterpret_cast<const char*>(&v), 2);
  }
}

double energy(const std::vector<float>& b) {
  double sum = 0.0;
  for (float v : b) {
    sum += static_cast<double>(v) * v;
  }
  return sum;
}

} // namespace

int main() {
  const std::string dir =
      "/tmp/bass-suite-user-ir-" + std::to_string(::getpid());
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    std::fprintf(stderr, "could not create temp dir\n");
    return 1;
  }

  std::vector<float> ir(2048, 0.0f);
  for (size_t i = 0; i < ir.size(); ++i) {
    const double t = static_cast<double>(i) / 48000.0;
    ir[i] = static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * 1200.0 * t) *
                               std::exp(-t * 40.0));
  }
  ir[0] += 2.0f;
  writeWav(dir + "/Test Cabinet.wav", ir, 48000);

  bass::UserIrLibrary library;
  library.start(dir, 48000.0f);
  library.rescanNow();
  if (library.names().size() != 1 ||
      library.nameForIndex(1) != "Test Cabinet.wav") {
    std::fprintf(stderr, "user IR list did not load\n");
    return 1;
  }
  std::string name;
  std::vector<unsigned char> bytes;
  if (!library.bytesForIndex(1, name, bytes) || bytes.size() < 44) {
    std::fprintf(stderr, "user IR bytes were not cached\n");
    return 1;
  }

  bass::Engine engine;
  engine.prepare(48000.0f);
  engine.setUserIrBank(library.bank());
  bass::Controls controls;
  controls.model = 4;
  controls.cab = 2;
  controls.userIr[4] = 1;
  engine.setControls(controls);

  std::vector<float> left(512, 0.0f);
  std::vector<float> right(512, 0.0f);
  left[0] = 0.5f;
  right[0] = 0.5f;
  bass::Meters meters;
  engine.process(left.data(), right.data(), 512, meters);
  if (!std::isfinite(energy(left)) || energy(left) <= 0.0 ||
      !std::isfinite(energy(right)) || energy(right) <= 0.0) {
    std::fprintf(stderr, "user IR convolution produced no output\n");
    return 1;
  }

  // State restoration path: importing an embedded WAV writes it back into the
  // user directory and returns a usable selection index.
  const int imported = library.importWav("Imported.wav", bytes, 0);
  if (imported < 1 || library.names().size() < 2) {
    std::fprintf(stderr, "user IR import failed\n");
    return 1;
  }

  library.stop();
  std::filesystem::remove_all(dir, ec);
  std::printf("amp_user_ir: ok\n");
  return 0;
}
