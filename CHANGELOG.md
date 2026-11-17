# Changelog

## 0.1.0 — 2026-11-17

- First working Bass Suite vertical slice: CLAP + LV2, 1040×640 panel.
- Four bass amps: Studio DI, Flip Top, SVT, Modern. SVT is the default.
- Four generated CC0 cabs: 1×15, 4×10, 2×12, 8×10, with Dynamic/Ribbon/Blend
  mic voicings.
- Comp, Octave and Drive pedals, with a parallel blend on the drive.
- OUT page: 4-band EQ, HPF/LPF with a response curve, limiter, output trim
  and stereo meters.
- User IR browser and per-amp user IR slots.
- Dedicated NAM page reusing the Amp Suite runtime and browser.
- Presets: SVT Classic, Flip Top Warm, Clean DI, Motown, Rock, Punk, Metal,
  Funk, Session.
- Removed the unused Amp Suite delay/reverb engine and the guitar-only
  parameters; moved atomic shared-pointer handoff to C++20.
- Tests: DSP, panel, layout, geometry, TTL, CLAP and LV2 host tests.
