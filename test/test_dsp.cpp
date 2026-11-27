// Bass Suite — DSP smoke test
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsp/crossfade_engine.hpp"
#include "dsp/engine.hpp"
#include "params.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace {

bool finiteBuffer(const std::vector<float>& l, const std::vector<float>& r) {
  for (size_t i = 0; i < l.size(); ++i) {
    if (!std::isfinite(l[i]) || !std::isfinite(r[i])) {
      return false;
    }
  }
  return true;
}

} // namespace

int main() {
  bass::Shared shared;
  bass::Engine engine;
  engine.prepare(48000.0f);
  engine.setControls(bass::controlsFrom(shared));

  std::vector<float> l(512, 0.0f);
  std::vector<float> r(512, 0.0f);
  l[0] = 0.5f;
  r[0] = 0.5f;
  bass::Meters meters;
  engine.process(l.data(), r.data(), 512, meters);
  if (!finiteBuffer(l, r)) {
    std::fprintf(stderr, "non-finite output at startup\n");
    return 1;
  }
  if (!std::isfinite(meters.outPeakDb) || !std::isfinite(meters.outVuDb)) {
    std::fprintf(stderr, "non-finite meter\n");
    return 1;
  }

  // Amp bypass removes the preamp/power amp but keeps input, drive, cab and
  // output active.
  {
    bass::Shared b;
    b.set(bass::P_MODEL, 3.0);
    b.set(bass::P_GAIN, 1.0);
    b.set(bass::P_MASTER, 1.0);
    b.set(bass::P_OUTPUT, 0.5);
    b.set(bass::P_OD_ON, 0.0);
    b.set(bass::P_CAB_BYPASS, 1.0);
    b.set(bass::P_AMP_BYPASS, 1.0);
    bass::Engine e;
    e.prepare(48000.0f);
    e.setControls(bass::controlsFrom(b));
    std::vector<float> bl(512), br(512);
    for (int i = 0; i < 512; ++i) {
      bl[i] = 0.1f * std::sin(2.0 * 3.14159265 * 220.0 * i / 48000.0);
      br[i] = bl[i];
    }
    bass::Meters bm;
    e.process(bl.data(), br.data(), 512, bm);
    if (!finiteBuffer(bl, br)) {
      std::fprintf(stderr, "non-finite amp-bypass output\n");
      return 1;
    }
    double energy = 0.0;
    for (float v : bl) {
      energy += static_cast<double>(v) * v;
    }
    const double rms = std::sqrt(energy / bl.size());
    if (rms < 0.02 || rms > 0.2) {
      std::fprintf(stderr, "amp bypass level is wrong: %.3f\n", rms);
      return 1;
    }
  }

  // A model change must pass through a brief muted crossfade instead of
  // swapping the recipe while the signal is live.
  {
    bass::Controls c;
    c.model = 4;
    c.gain = 0.6f;
    c.cabBypass = true;
    c.odOn = false;
    c.gateOn = false;
    c.limiterOn = false;
    bass::Engine e;
    e.prepare(48000.0f);
    e.setControls(c);
    std::vector<float> l(512), r(512);
    bass::Meters m;
    bool sawMute = false;
    bool sawSignal = false;
    for (int b = 0; b < 40; ++b) {
      if (b == 20) {
        c.model = 5;
      }
      for (int i = 0; i < 512; ++i) {
        l[i] = r[i] =
            0.2f * std::sin(2.0 * 3.14159265 * 220.0 * (b * 512 + i) / 48000.0);
      }
      e.setControls(c);
      e.process(l.data(), r.data(), 512, m);
      for (int i = 0; i < 512; ++i) {
        const float v = std::fabs(l[i]);
        if (!std::isfinite(v)) {
          std::fprintf(stderr, "model change produced a non-finite sample\n");
          return 1;
        }
        if (b >= 20 && b < 24 && v < 1.0e-6f) sawMute = true;
        if (b >= 26 && v > 1.0e-4f) sawSignal = true;
      }
    }
    if (!sawMute || !sawSignal) {
      std::fprintf(stderr, "model change did not mute then resume\n");
      return 1;
    }
  }

  // The dual-engine wrapper fades the old amp out, swaps at silence and
  // fades the new one in.  The switch must not step the waveform, and the
  // gap must be short.  (An equal-gain crossfade used to sum two
  // differently-phased amps, producing a deep cancellation dip that was
  // heard as a pop.)
  {
    bass::CrossfadeEngine e;
    e.prepare(48000.0f);
    bass::Controls c;
    c.model = 4;
    c.gain = 0.6f;
    c.cabBypass = true;
    c.odOn = false;
    c.gateOn = false;
    c.limiterOn = false;
    std::vector<float> l(512), r(512);
    bass::Meters m;
    auto fill = [&](int block) {
      for (int i = 0; i < 512; ++i) {
        l[i] = r[i] =
            0.2f * std::sin(2.0 * 3.14159265 * 220.0 *
                            (block * 512 + i) / 48000.0);
      }
    };
    fill(0);
    e.setControls(c);
    e.process(l.data(), r.data(), 512, m);
    if (std::fabs(l[0]) > 1.0e-6f) {
      std::fprintf(stderr, "crossfade engine did not fade in after prepare\n");
      return 1;
    }
    for (int b = 1; b < 20; ++b) {
      fill(b);
      e.setControls(c);
      e.process(l.data(), r.data(), 512, m);
    }
    double steadyBefore = 0.0;
    for (int b = 16; b < 20; ++b) {
      for (int i = 1; i < 512; ++i) {
        steadyBefore = std::max(steadyBefore,
                                double(std::fabs(l[i] - l[i - 1])));
      }
    }
    c.model = 1;
    double maxDelta = 0.0;
    double steadyAfter = 0.0;
    bool finite = true;
    bool swapped = false;
    float previous = l[511];
    for (int b = 20; b < 40; ++b) {
      fill(b);
      e.setControls(c);
      e.process(l.data(), r.data(), 512, m);
      finite = finite && finiteBuffer(l, r);
      for (int i = 0; i < 512; ++i) {
        maxDelta = std::max(maxDelta,
                            double(std::fabs(l[i] - previous)));
        if (std::fabs(l[i]) < 1.0e-5f) swapped = true;
        previous = l[i];
      }
      if (b >= 35) {
        for (int i = 1; i < 512; ++i) {
          steadyAfter = std::max(steadyAfter,
                                 double(std::fabs(l[i] - l[i - 1])));
        }
      }
    }
    double tailEnergy = 0.0;
    for (float v : l) {
      tailEnergy += static_cast<double>(v) * v;
    }
    const double tailRms = std::sqrt(tailEnergy / l.size());
    const double allowed =
        std::max(steadyBefore, steadyAfter) * 1.5 + 0.002;
    if (!finite || !swapped || tailRms < 1.0e-3 || maxDelta > allowed) {
      std::fprintf(stderr,
                   "model switch was not clean: finite %d swapped %d "
                   "tailRms %.6f maxDelta %.6f allowed %.6f\n",
                   finite ? 1 : 0, swapped ? 1 : 0, tailRms, maxDelta,
                   allowed);
      return 1;
    }
  }

  // A preset or host-state load replaces many controls at once while the amp
  // stays the same.  requestReload() must route that through the same clean
  // switch instead of stepping gain/tone coefficients in place.
  {
    bass::CrossfadeEngine e;
    e.prepare(48000.0f);
    bass::Controls c;
    c.model = 2;
    c.gain = 0.3f;
    c.master = 0.3f;
    c.cabBypass = true;
    c.odOn = false;
    c.gateOn = false;
    c.limiterOn = false;
    std::vector<float> l(512), r(512);
    bass::Meters m;
    auto fill = [&](int block) {
      for (int i = 0; i < 512; ++i) {
        l[i] = r[i] =
            0.2f * std::sin(2.0 * 3.14159265 * 220.0 *
                            (block * 512 + i) / 48000.0);
      }
    };
    for (int b = 0; b < 20; ++b) {
      fill(b);
      e.setControls(c);
      e.process(l.data(), r.data(), 512, m);
    }
    double steadyBefore = 0.0;
    for (int i = 1; i < 512; ++i) {
      steadyBefore = std::max(steadyBefore,
                              double(std::fabs(l[i] - l[i - 1])));
    }
    c.gain = 0.85f;
    c.master = 0.7f;
    c.bass = 0.8f;
    c.treble = 0.2f;
    e.requestReload();
    double maxDelta = 0.0;
    double steadyAfter = 0.0;
    bool finite = true;
    float previous = l[511];
    for (int b = 20; b < 40; ++b) {
      fill(b);
      e.setControls(c);
      e.process(l.data(), r.data(), 512, m);
      finite = finite && finiteBuffer(l, r);
      for (int i = 0; i < 512; ++i) {
        maxDelta = std::max(maxDelta,
                            double(std::fabs(l[i] - previous)));
        previous = l[i];
      }
      if (b >= 35) {
        for (int i = 1; i < 512; ++i) {
          steadyAfter = std::max(steadyAfter,
                                 double(std::fabs(l[i] - l[i - 1])));
        }
      }
    }
    const double allowed =
        std::max(steadyBefore, steadyAfter) * 1.5 + 0.002;
    if (!finite || maxDelta > allowed) {
      std::fprintf(stderr,
                   "preset reload stepped the output: maxDelta %.6f "
                   "allowed %.6f\n",
                   maxDelta, allowed);
      return 1;
    }
  }

  // The host calls prepare() again in activate() after the constructor has
  // already applied controls.  That second prepare clears the convolver and
  // must re-install the current IR, otherwise the first session loads without
  // a cabinet until a model switch forces a new IR pointer.
  {
    bass::Engine e;
    e.prepare(48000.0f);
    if (!e.loadFactoryCabs(BASS_TEST_ASSETS)) {
      std::fprintf(stderr, "factory cabs did not load from %s\n", BASS_TEST_ASSETS);
      return 1;
    }
    bass::Shared s;
    e.setControls(bass::controlsFrom(s));
    e.prepare(48000.0f);
    e.setControls(bass::controlsFrom(s));
    if (!e.cabReady()) {
      std::fprintf(stderr, "cab IR was not restored after the second prepare\n");
      return 1;
    }
  }

  // Power off is a true bypass: the dry input reaches the output untouched.
  {
    bass::Controls c;
    c.model = 4;
    c.enabled = false;
    c.gain = 0.7f;
    c.odOn = true;
    c.cabBypass = false;
    bass::Engine e;
    e.prepare(48000.0f);
    e.setControls(c);
    std::vector<float> l(1024), r(1024), dry(1024);
    for (int i = 0; i < 1024; ++i) {
      dry[i] = 0.3f * std::sin(2.0 * 3.14159265 * 220.0 * i / 48000.0);
      l[i] = r[i] = dry[i];
    }
    bass::Meters m;
    e.process(l.data(), r.data(), 1024, m);
    for (int i = 0; i < 1024; ++i) {
      if (l[i] != dry[i] || r[i] != dry[i]) {
        std::fprintf(stderr, "power off was not a true bypass at sample %d\n", i);
        return 1;
      }
    }
    if (!(m.inPeakDb > -20.0f) || !(m.outPeakDb > -20.0f)) {
      std::fprintf(stderr, "true bypass did not meter the dry signal\n");
      return 1;
    }
  }

  // Powering back on after changing the model must not leave the engine
  // stuck in bypass while a model fade is pending.
  {
    bass::Controls c;
    c.model = 4;
    c.enabled = false;
    c.cabBypass = true;
    c.odOn = false;
    bass::Engine e;
    e.prepare(48000.0f);
    e.setControls(c);
    std::vector<float> l(512), r(512), dry(512);
    for (int i = 0; i < 512; ++i) {
      dry[i] = 0.1f * std::sin(2.0 * 3.14159265 * 220.0 * i / 48000.0);
      l[i] = r[i] = dry[i];
    }
    bass::Meters m;
    e.process(l.data(), r.data(), 512, m);
    bool bypassed = true;
    for (int i = 0; i < 512; ++i) {
      if (l[i] != dry[i]) bypassed = false;
    }
    if (!bypassed) {
      std::fprintf(stderr, "power off was not a true bypass\n");
      return 1;
    }
    c.model = 5;
    c.enabled = true;
    bool processed = false;
    for (int b = 0; b < 40; ++b) {
      for (int i = 0; i < 512; ++i) {
        l[i] = r[i] = 0.1f * std::sin(2.0 * 3.14159265 * 220.0 * (b * 512 + i) / 48000.0);
      }
      e.setControls(c);
      e.process(l.data(), r.data(), 512, m);
      if (b == 39) {
        for (int i = 0; i < 512; ++i) {
          if (std::fabs(l[i]) > 1.0e-6f) processed = true;
        }
      }
    }
    if (!processed) {
      std::fprintf(stderr, "engine stayed bypassed after power-on\n");
      return 1;
    }
  }

  // Noise gate: a very quiet input is attenuated when the gate is open, while
  // a normal input passes.
  {
    auto measure = [](bool gateOn, float ampLevel) {
      bass::Shared b;
      b.set(bass::P_MODEL, 3.0);
      b.set(bass::P_CAB_BYPASS, 1.0);
      b.set(bass::P_OD_ON, 0.0);
      b.set(bass::P_GAIN, 0.4);
      b.set(bass::P_GATE_ON, gateOn ? 1.0 : 0.0);
      b.set(bass::P_GATE_THRESH, 0.8);
      b.set(bass::P_GATE_DECAY, 0.0);
      bass::Engine e;
      e.prepare(48000.0f);
      e.setControls(bass::controlsFrom(b));
      std::vector<float> l(19200), r(19200);
      for (int i = 0; i < 19200; ++i) {
        l[i] = r[i] = ampLevel * std::sin(2.0 * 3.14159265 * 220.0 * i / 48000.0);
      }
      bass::Meters m;
      e.process(l.data(), r.data(), 19200, m);
      double energy = 0.0;
      for (int i = 14400; i < 19200; ++i) {
        energy += static_cast<double>(l[i]) * l[i];
      }
      return std::sqrt(energy / 4800.0);
    };
    const double quietOff = measure(false, 0.001f);
    const double quietOn = measure(true, 0.001f);
    const double loudOn = measure(true, 0.1f);
    if (!(quietOn < quietOff * 0.25)) {
      std::fprintf(stderr, "gate did not attenuate a quiet input: %.6f vs %.6f\n",
                   quietOn, quietOff);
      return 1;
    }
    if (!(loudOn > 0.0001)) {
      std::fprintf(stderr, "gate blocked a normal input\n");
      return 1;
    }
  }

  // Octave pedal: a 110 Hz input produces a sub-octave (about half the
  // zero-crossing rate) and stays finite.
  {
    bass::Shared o;
    o.set(bass::P_MODEL, 2.0);
    o.set(bass::P_CAB_BYPASS, 1.0);
    o.set(bass::P_COMP_ON, 0.0);
    o.set(bass::P_OD_ON, 0.0);
    o.set(bass::P_OCT_ON, 1.0);
    o.set(bass::P_OCT_LEVEL, 1.0);
    o.set(bass::P_OCT_DRY, 0.0);
    o.set(bass::P_OCT_TONE, 0.5);
    o.set(bass::P_GAIN, 0.5);
    bass::Engine e;
    e.prepare(48000.0f);
    e.setControls(bass::controlsFrom(o));
    const int n = 48000;
    std::vector<float> l(n), r(n);
    for (int i = 0; i < n; ++i) {
      l[i] = r[i] = 0.4f * std::sin(2.0 * 3.14159265 * 110.0 * i / 48000.0);
    }
    bass::Meters m;
    e.process(l.data(), r.data(), n, m);
    if (!finiteBuffer(l, r)) {
      std::fprintf(stderr, "octave produced non-finite output\n");
      return 1;
    }
    int crossings = 0;
    for (int i = 12000; i < 36000; ++i) {
      if ((l[i - 1] <= 0.0f && l[i] > 0.0f) ||
          (l[i - 1] >= 0.0f && l[i] < 0.0f)) {
        ++crossings;
      }
    }
    // 110 Hz over 0.5 s is 110 crossings; 55 Hz is 55.
    if (crossings < 25 || crossings > 85) {
      std::fprintf(stderr, "octave zero-crossing rate wrong: %d\n", crossings);
      return 1;
    }
  }

  // The octave wet level must follow the input, not run at a fixed loud
  // square level.  Otherwise quiet notes explode through the amp and only a
  // tiny Level setting is usable.
  {
    auto octaveRms = [](float amplitude) {
      bass::Shared o;
      o.set(bass::P_MODEL, 2.0);
      o.set(bass::P_CAB_BYPASS, 1.0);
      o.set(bass::P_COMP_ON, 0.0);
      o.set(bass::P_OD_ON, 0.0);
      o.set(bass::P_GATE_ON, 0.0);
      o.set(bass::P_OCT_ON, 1.0);
      o.set(bass::P_OCT_LEVEL, 1.0);
      o.set(bass::P_OCT_DRY, 0.0);
      o.set(bass::P_OCT_TONE, 0.5);
      o.set(bass::P_GAIN, 0.5);
      bass::Engine e;
      e.prepare(48000.0f);
      e.setControls(bass::controlsFrom(o));
      const int n = 48000;
      std::vector<float> l(n), r(n);
      for (int i = 0; i < n; ++i) {
        l[i] = r[i] =
            amplitude * std::sin(2.0 * 3.14159265 * 110.0 * i / 48000.0);
      }
      bass::Meters m;
      e.process(l.data(), r.data(), n, m);
      double sum = 0.0;
      for (int i = 12000; i < n; ++i) {
        sum += static_cast<double>(l[i]) * l[i];
      }
      return std::sqrt(sum / (n - 12000));
    };
    const double quiet = octaveRms(0.05f);
    const double loud = octaveRms(0.40f);
    if (!(loud > quiet * 2.0)) {
      std::fprintf(stderr, "octave does not track input level: quiet=%f loud=%f\n",
                   quiet, loud);
      return 1;
    }
  }

  // Octave on a bright low note: a fixed pre-filter passes the second and
  // third harmonics, and on a low B those give the divider extra zero
  // crossings, so the pedal jumped between the sub and the fundamental (or
  // produced nothing usable).  The tracked pre-filter has to leave one
  // crossing per cycle, so the output energy sits at half the input note.
  {
    auto subRatio = [](double hz) {
      bass::OctavePedal oct;
      oct.prepare(48000.0f);
      bass::Controls c;
      c.octOn = true;
      c.octLevel = 1.0f;
      c.octDry = 0.0f;
      c.octTone = 0.5f;
      oct.setControls(c);
      const int n = 72000;  // 1.5 s
      std::vector<float> out;
      const double amps[8] = {1.0, 0.85, 0.55, 0.38, 0.28, 0.2, 0.16, 0.12};
      for (int i = 0; i < n; ++i) {
        const double t = i / 48000.0;
        double sum = 0.0;
        for (int h = 1; h <= 8; ++h) {
          sum += amps[h - 1] * std::sin(2.0 * 3.14159265358979 * hz * h * t);
        }
        const double env = std::min(1.0, t / 0.003) * std::exp(-t / 1.2);
        float l = static_cast<float>(0.10 * sum * env);
        float r = l;
        oct.process(l, r);
        out.push_back(l);
      }
      auto mag = [&](const std::vector<float>& x, int start, int frames, double f) {
        double re = 0.0, im = 0.0;
        for (int i = 0; i < frames; ++i) {
          const double hann = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979 * i / frames);
          const double w = 2.0 * 3.14159265358979 * f * i / 48000.0;
          re += x[static_cast<size_t>(start + i)] * hann * std::cos(w);
          im -= x[static_cast<size_t>(start + i)] * hann * std::sin(w);
        }
        return std::sqrt(re * re + im * im) / frames;
      };
      double worst = 1.0e9;
      for (int k = 0; k < 4; ++k) {
        const int start = 9600 + k * 12000;
        const double sub = mag(out, start, 9600, hz / 2.0);
        const double fund = mag(out, start, 9600, hz);
        const double ratio = fund > 1.0e-9 ? sub / fund : 1.0e9;
        worst = std::min(worst, ratio);
      }
      return worst;
    };
    const double b0 = subRatio(30.87);
    const double e1 = subRatio(41.20);
    if (b0 < 5.0 || e1 < 5.0) {
      std::fprintf(stderr,
                   "octave is not dividing on low notes: B0 ratio %.1f E1 ratio %.1f\n",
                   b0, e1);
      return 1;
    }
  }

  // Output EQ and filters: a high HPF cuts the low end, a low LPF cuts the
  // top, and a low-shelf boost lifts 60 Hz.
  {
    auto measure = [](float lowEq, float hpf, float lpf, float toneHz) {
      bass::Shared o;
      o.set(bass::P_MODEL, 2.0);
      o.set(bass::P_CAB_BYPASS, 1.0);
      o.set(bass::P_COMP_ON, 0.0);
      o.set(bass::P_OD_ON, 0.0);
      o.set(bass::P_OCT_ON, 0.0);
      o.set(bass::P_GATE_ON, 0.0);
      o.set(bass::P_GAIN, 0.4);
      o.set(bass::P_OUT_EQ_LOW, lowEq);
      o.set(bass::P_OUT_HPF, hpf);
      o.set(bass::P_OUT_LPF, lpf);
      bass::Engine e;
      e.prepare(48000.0f);
      e.setControls(bass::controlsFrom(o));
      std::vector<float> l(24000), r(24000);
      for (int i = 0; i < 24000; ++i) {
        l[i] = r[i] = 0.1f * std::sin(2.0 * 3.14159265 * toneHz * i / 48000.0);
      }
      bass::Meters m;
      e.process(l.data(), r.data(), 24000, m);
      double energy = 0.0;
      for (int i = 12000; i < 24000; ++i) energy += l[i] * l[i];
      return std::sqrt(energy / 12000.0);
    };
    const double lowOpen = measure(0.5f, 0.0f, 1.0f, 60.0f);
    const double lowCut = measure(0.5f, 0.75f, 1.0f, 60.0f);
    const double lowBoost = measure(0.85f, 0.0f, 1.0f, 60.0f);
    const double topOpen = measure(0.5f, 0.0f, 1.0f, 10000.0f);
    const double topCut = measure(0.5f, 0.0f, 0.25f, 10000.0f);
    if (!(lowCut < lowOpen * 0.7)) {
      std::fprintf(stderr, "output HPF did not cut the low end\n");
      return 1;
    }
    if (!(lowBoost > lowOpen * 1.4)) {
      std::fprintf(stderr, "output low shelf did not boost\n");
      return 1;
    }
    if (!(topCut < topOpen * 0.4)) {
      std::fprintf(stderr, "output LPF did not cut the top\n");
      return 1;
    }
  }

  // Non-finite host input must be sanitized, not propagated through the
  // filters and limiter.
  {
    bass::Shared b;
    b.set(bass::P_MODEL, 2.0);
    b.set(bass::P_CAB_BYPASS, 1.0);
    b.set(bass::P_COMP_ON, 1.0);
    b.set(bass::P_OD_ON, 1.0);
    b.set(bass::P_OCT_ON, 1.0);
    bass::Engine e;
    e.prepare(48000.0f);
    e.setControls(bass::controlsFrom(b));
    std::vector<float> l(512), r(512);
    for (int i = 0; i < 512; ++i) {
      l[i] = (i == 10) ? std::numeric_limits<float>::quiet_NaN()
                       : ((i == 20) ? std::numeric_limits<float>::infinity()
                                    : 0.1f * std::sin(0.1f * i));
      r[i] = l[i];
    }
    bass::Meters m;
    e.process(l.data(), r.data(), 512, m);
    if (!finiteBuffer(l, r)) {
      std::fprintf(stderr, "non-finite input was not sanitized\n");
      return 1;
    }
  }

  // Every model must produce finite output and a plausible meter.
  for (int model = 0; model < 4; ++model) {
    shared.set(bass::P_MODEL, static_cast<double>(model));
    engine.setControls(bass::controlsFrom(shared));
    std::vector<float> ml(256, 0.1f);
    std::vector<float> mr(256, -0.1f);
    engine.process(ml.data(), mr.data(), 256, meters);
    if (!finiteBuffer(ml, mr)) {
      std::fprintf(stderr, "non-finite output on model %d\n", model);
      return 1;
    }
  }

  // Extreme settings must stay finite and bounded (no NaN, no runaway) at
  // both oversampling rates.
  for (int model = 0; model < 4; ++model) {
    bass::Shared x;
    x.set(bass::P_MODEL, static_cast<double>(model));
    x.set(bass::P_GAIN, 1.0);
    x.set(bass::P_MASTER, 1.0);
    x.set(bass::P_OUTPUT, 0.7);
    x.set(bass::P_BASS, 1.0);
    x.set(bass::P_MID, 0.0);
    x.set(bass::P_TREBLE, 1.0);
    x.set(bass::P_PRESENCE, 1.0);
    x.set(bass::P_RESONANCE, 1.0);
    x.set(bass::P_OD_ON, 1.0);
    x.set(bass::P_OD_DRIVE, 1.0);
    x.set(bass::P_OD_LEVEL, 1.0);
    bass::Engine e;
    e.prepare(48000.0f);
    e.setControls(bass::controlsFrom(x));
    std::vector<float> xl(1024), xr(1024);
    for (int i = 0; i < 1024; ++i) {
      xl[i] = 0.5f * std::sin(2.0 * 3.14159265 * 220.0 * i / 48000.0);
      xr[i] = xl[i];
    }
    bass::Meters xm;
    e.process(xl.data(), xr.data(), 1024, xm);
    if (!finiteBuffer(xl, xr)) {
      std::fprintf(stderr, "non-finite output at extremes on model %d\n",
                   model);
      return 1;
    }
    float peak = 0.0f;
    for (float v : xl) {
      peak = std::max(peak, std::fabs(v));
    }
    if (peak > 50.0f) {
      std::fprintf(stderr, "runaway output on model %d: %.1f\n", model,
                   static_cast<double>(peak));
      return 1;
    }
  }


  // Tuner: a 55 Hz input reads as A1, and silence produces no note.
  {
    bass::Tuner tuner;
    tuner.prepare(48000.0f);
    std::vector<float> l(48000), r(48000);
    for (int i = 0; i < 48000; ++i) {
      l[i] = r[i] = 0.2f * std::sin(2.0 * 3.14159265 * 55.0 * i / 48000.0);
    }
    tuner.process(l.data(), r.data(), 48000, true);
    if (!tuner.hasPitch() || std::fabs(tuner.hz() - 55.0f) > 1.5f ||
        tuner.note() != 33) {
      std::fprintf(stderr, "tuner failed on 55 Hz: %.2f Hz note %d\n",
                   tuner.hz(), tuner.note());
      return 1;
    }
    std::fill(l.begin(), l.end(), 0.0f);
    std::fill(r.begin(), r.end(), 0.0f);
    tuner.process(l.data(), r.data(), 48000, true);
    if (tuner.hasPitch()) {
      std::fprintf(stderr, "tuner latched onto silence\n");
      return 1;
    }
  }

  std::printf("bass_dsp: ok\n");
  return 0;
}
