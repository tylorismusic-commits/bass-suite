# Changelog

## 0.1.3 — 2026-11-18

Preset handling is now complete.

- DUPLICATE copies the selected factory or user preset; factory copies capture
  the current state.
- EXPORT writes the current preset to `~/Documents/Bass Suite Presets`.
- Drag a `.bass-suite` file onto the panel to import it; collisions get a
  `(2)` suffix.
- Rename conflicts show inline feedback; overwriting a different existing
  preset asks for a second Enter.
- The dirty marker follows the live parameters, including host automation.
- The list rescans on open and every two seconds while open.
- Imported/exported values are sanitised before reaching the engine.

## 0.1.2 — 2026-11-18

- User presets in the preset browser. SAVE stores the complete bass, cab,
  pedal and output state under `~/.config/BassSuite/presets`; user presets are
  listed after the factory bank, and can be renamed or deleted from the
  browser footer. Loading a user preset uses the click-free switch.

## 0.1.1 — 2026-11-18

- Amp switching no longer pops. The two engines were summed with an
  equal-gain crossfade, so two differently-phased amps partly cancelled and
  the level collapsed by up to 34 dB during a switch. The standby engine is
  now warmed with the live input, the old amp fades out in 5 ms, the engines
  swap at silence, and the new amp fades in over 8 ms.
- Preset and host-state loads that keep the same amp use the same clean
  switch instead of stepping the running amp's gain and tone coefficients.
- Cab/IR changes while NAM is on now mute and fade, and resetting the idle
  crossfade engine no longer wipes the active engine's NAM state.
- Added mid-note CLAP model-switch, same-amp preset-reload and NAM-switch
  continuity tests.

## 0.1.4 — 2026-11-19

- New chromatic tuner page, opened with the **TUNE** pill. It shows the note
  and octave, cents, frequency and a ±50-cent bar, with a green in-tune state
  and TUNE UP / TUNE DOWN hints. The detector taps the input before Comp,
  Octave and Drive and only runs while the page is open.
- Hide NAM's internal symbols so Bass Suite can load after Amp Suite in the
  same host process without aborting on duplicate NAM registration.
- Make the octave wet level follow the input envelope and round the square
  with a 12 dB/oct tone filter; raise the default octave setting.
- Remove the duplicate OUTPUT caption on the OUT page.
- Clicking the already-selected amp while another page is open returns to the
  amp page; clicking a different amp keeps the current page open.

## 0.1.0 — 2026-11-17

- First working Bass Suite vertical slice: CLAP + LV2, 1040×640 panel.
- Four bass amps: Studio DI, Flip Top, SVT, Modern. SVT is the default.
- Six generated CC0 cabs: 1×15, 4×10, 2×12, 8×10, 2×10, 1×12, with
  Dynamic/Ribbon/Blend mic voicings.
- Comp, Octave and Drive pedals, with a parallel blend on the drive.
- OUT page: 4-band EQ, HPF/LPF with a response curve, limiter, output trim
  and stereo meters.
- User IR browser and per-amp user IR slots.
- Dedicated NAM page reusing the Amp Suite runtime and browser.
- Fifteen presets: SVT Classic, Flip Top Warm, Clean DI, Motown, Rock, Punk,
  Metal, Funk, Session, SVT Driven, Modern Metal, Fretless, Slap, Dub and
  Synth Bass. Levels matched on synthetic and real bass loops.
- Removed the unused Amp Suite delay/reverb engine and the guitar-only
  parameters; moved atomic shared-pointer handoff to C++20.
- Bug-catching pass: non-finite input sanitization; malformed WAV rejection;
  user-IR state slot bounds; NAM FIFO preallocation on the audio thread; UI
  NAM/amp and user-IR/cab state fixes.
- Tests: DSP, panel, layout, geometry, TTL, CLAP and LV2 host tests.
