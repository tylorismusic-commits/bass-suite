// Bass Suite — cab IR convolution and factory bank
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Direct FIR convolution is used first: 2048 taps at 48 kHz / 4096 at 96 kHz
// is light enough for one cab and keeps the signal path zero-latency.  The
// bank loads the generated CC0 factory IRs from the plugin bundle.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "wav.hpp"

namespace bass {

struct CabIR {
  std::vector<float> h;
};

// Normalize an impulse to unity magnitude at 1 kHz.  This keeps cab and mic
// switching level-consistent regardless of how the source files were
// normalized.
inline void normalizeIrAt1k(std::vector<float>& h, int sampleRate) noexcept {
  if (h.empty() || sampleRate <= 0) {
    return;
  }
  const double w =
      2.0 * 3.14159265358979323846 * 1000.0 / static_cast<double>(sampleRate);
  double re = 0.0;
  double im = 0.0;
  for (size_t i = 0; i < h.size(); ++i) {
    re += static_cast<double>(h[i]) * std::cos(w * static_cast<double>(i));
    im += static_cast<double>(h[i]) * std::sin(w * static_cast<double>(i));
  }
  const double mag = std::sqrt(re * re + im * im);
  if (mag > 1.0e-6) {
    const float g = static_cast<float>(1.0 / mag);
    for (float& v : h) {
      v *= g;
    }
  }
}

class CabConvolver {
public:
  void prepare(int maxTaps = 4096) noexcept {
    int size = 1;
    while (size < maxTaps) size <<= 1;
    size_ = size;
    mask_ = size_ - 1;
    historyL_.assign(static_cast<size_t>(size_), 0.0f);
    historyR_.assign(static_cast<size_t>(size_), 0.0f);
    write_ = 0;
    ir_ = nullptr;
    taps_ = 0;
  }

  void setIR(const CabIR* ir) noexcept {
    ir_ = ir;
    taps_ = ir ? static_cast<int>(ir->h.size()) : 0;
    reset();
  }

  void reset() noexcept {
    std::fill(historyL_.begin(), historyL_.end(), 0.0f);
    std::fill(historyR_.begin(), historyR_.end(), 0.0f);
    write_ = 0;
  }

  bool active() const noexcept { return ir_ && taps_ > 0; }

  void process(float& l, float& r) noexcept {
    if (!active()) {
      return;
    }
    historyL_[write_] = l;
    historyR_[write_] = r;
    float yl = 0.0f;
    float yr = 0.0f;
    int idx = write_;
    const int n = std::min(taps_, size_);
    for (int k = 0; k < n; ++k) {
      const float h = ir_->h[static_cast<size_t>(k)];
      yl += h * historyL_[idx];
      yr += h * historyR_[idx];
      idx = (idx - 1) & mask_;
    }
    write_ = (write_ + 1) & mask_;
    l = yl;
    r = yr;
  }

private:
  const CabIR* ir_ = nullptr;
  int taps_ = 0;
  int size_ = 0;
  int mask_ = 0;
  int write_ = 0;
  std::vector<float> historyL_;
  std::vector<float> historyR_;
};

class CabBank {
public:
  enum Set {
    OneByFifteen = 0,
    FourByTen = 1,
    TwoByTwelve = 2,
    EightByTen = 3,
    Count = 4
  };

  bool load(const std::string& dir, float sampleRate) noexcept {
    static const char* const names[Count] = {
        "1x15", "4x10", "2x12", "8x10"};
    static const char* const mics[3] = {"57", "121", "blend"};
    const int targetRate = static_cast<int>(std::lround(sampleRate));
    const bool prefer96 = sampleRate >= 72000.0f;
    bool loadedAny = false;
    for (int s = 0; s < Count; ++s) {
      for (int m = 0; m < 3; ++m) {
        std::vector<float> data;
        std::string base = dir + "/" + names[s] + "/cab_" + names[s] + "_" +
                           mics[m];
        bool ok = false;
        if (prefer96) {
          ok = readWavMono(base + "_96k.wav", data, targetRate);
        }
        if (!ok) {
          ok = readWavMono(base + ".wav", data, targetRate);
        }
        if (ok && !data.empty()) {
          normalizeIrAt1k(data, targetRate);
          sets_[s].mic[m].h = std::move(data);
          loadedAny = true;
        } else {
          sets_[s].mic[m].h.clear();
        }
      }
    }
    return loadedAny;
  }

  const CabIR* get(int set, int mic) const noexcept {
    if (set < 0 || set >= Count) {
      return nullptr;
    }
    if (mic < 0 || mic > 2) {
      mic = 0;
    }
    const CabIR& ir = sets_[set].mic[mic];
    return ir.h.empty() ? nullptr : &ir;
  }

  bool present() const noexcept {
    for (const auto& s : sets_) {
      for (const auto& m : s.mic) {
        if (!m.h.empty()) return true;
      }
    }
    return false;
  }

private:
  struct SetData {
    CabIR mic[3];
  };
  std::array<SetData, Count> sets_;
};

} // namespace bass
