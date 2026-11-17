// Bass Suite — offline level probe
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsp/engine.hpp"
#include "params.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

int main(int argc, char** argv) {
  const bool od = argc > 1 && std::strcmp(argv[1], "--od") == 0;
  constexpr float sr = 48000.0f;
  constexpr int n = 48000;
  for (int model = 0; model < 4; ++model) {
    bass::Shared s;
    s.set(bass::P_MODEL, static_cast<double>(model));
    s.set(bass::P_OD_ON, od ? 1.0 : 0.0);
    s.set(bass::P_INPUT, 0.5);
    s.set(bass::P_GAIN, 0.55);
    s.set(bass::P_BASS, 0.5);
    s.set(bass::P_MID, 0.45);
    s.set(bass::P_TREBLE, 0.55);
    s.set(bass::P_PRESENCE, 0.5);
    s.set(bass::P_RESONANCE, 0.5);
    s.set(bass::P_MASTER, 0.5);
    s.set(bass::P_OUTPUT, 0.5);
    s.set(bass::P_CAB_BYPASS, 0.0);

    bass::Engine e;
    e.prepare(sr);
    e.loadFactoryCabs("assets/irs");
    e.setControls(bass::controlsFrom(s));
    std::vector<float> l(n), r(n);
    for (int i = 0; i < n; ++i) {
      l[i] = 0.25f * std::sin(2.0 * 3.14159265358979 * 1000.0 * i / sr);
      r[i] = l[i];
    }
    bass::Meters m;
    e.process(l.data(), r.data(), n, m);
    double peak = 0.0;
    double sum = 0.0;
    for (int i = n / 2; i < n; ++i) {
      peak = std::max(peak, static_cast<double>(std::fabs(l[i])));
      sum += static_cast<double>(l[i]) * l[i];
    }
    const double rms = std::sqrt(sum / (n / 2));
    // THD at -24 dBFS input.
    std::vector<float> tl(n), tr(n);
    for (int i = 0; i < n; ++i) {
      tl[i] = 0.063f * std::sin(2.0 * 3.14159265358979 * 1000.0 * i / sr);
      tr[i] = tl[i];
    }
    bass::Engine e2;
    e2.prepare(sr);
    e2.loadFactoryCabs("assets/irs");
    e2.setControls(bass::controlsFrom(s));
    e2.process(tl.data(), tr.data(), n, m);
    double mag[11] = {};
    for (int h = 1; h <= 10; ++h) {
      double re = 0.0, im = 0.0;
      for (int i = n / 2; i < n; ++i) {
        const double ph = 2.0 * 3.14159265358979 * h * 1000.0 * i / sr;
        re += tl[i] * std::cos(ph);
        im += tl[i] * std::sin(ph);
      }
      mag[h] = 2.0 * std::sqrt(re * re + im * im) / (n / 2);
    }
    double thd = 0.0;
    for (int h = 2; h <= 10; ++h) {
      thd += mag[h] * mag[h];
    }
    thd = std::sqrt(thd) / std::max(mag[1], 1e-12);
    std::printf("model %s  peak %7.1f dBFS  rms %7.1f dBFS  THD %5.1f%%\n",
                bass::kModelLabels[model], 20.0 * std::log10(std::max(peak, 1e-9)),
                20.0 * std::log10(std::max(rms, 1e-9)), thd * 100.0);
  }
  return 0;
}
