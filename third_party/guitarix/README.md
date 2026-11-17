# Guitarix DSP subset

Vendored from **Guitarix** (`https://github.com/brummer10/guitarix`, `master`),
for Amp Suite's tube-stage and tone-stack foundation.

## License

Most files are GPL-2.0-or-later; some Guitarix files are GPL-3.0-or-later, so
the combined work is GPL-3.0-or-later. The original copyright headers are
preserved. See `COPYING`.

## Included

| Path | Use |
|---|---|
| `DSP/valve.h` | table lookup / interpolation, `Ftube`, `Ranode` |
| `DSP/tube_tables/*.cc` | 12AX7, 12AT7, 12AU7, 12AY7, 6V6, 6L6GC, EL34, EL84, EF86, KT88, etc. |
| `faust/guitarix.lib` | `tubestageF/P`, `tonestack` formulas (reference) |
| `faust/tonestack.dsp` | tone-stack component values (Bassman, JTM45, JCM800, AC30, Peavey, Mesa, Soldano, Bogner, Roland, …) |
| `DSP/plexipowerampel34_table.h` | Plexi EL34 power-amp transfer table |
| `DSP/trany.h` | output transformer reference |
| `ampsim/gxpoweramp.dsp`, `ampsim/gx_poweramp.cpp` | power-amp reference |
| `plugins/ts9nonlin.*`, `plugins/ts9sim.dsp` | TS9 / 808 reference |
| `LV2/gx_chorus.cpp`, `LV2/gx_delay.cpp`, `LV2/gx_zita_rev1.cc` | chorus, delay, reverb reference |

The tone-stack values originate from CAPS (Tim Goetze), based on D. T. Yeh's
work. Preserve that attribution.

## Integration notes

- `src/dsp/guitarix_tables.cpp` includes `valve.h` exactly once and defines
  `__rt_data` empty. `valve.h` includes the generated `.cc` tables, so the
  `third_party/guitarix/DSP/tube_tables` directory is on the include path.
- `src/dsp/tube_stage.hpp` is the C++ port of `tubestageF/P`.
- `src/dsp/tone_stack.hpp` is the C++ port of the Guitarix/CAPS tone-stack
  filter; the component values live in `src/dsp/recipe.hpp`.
- The Guitarix UI, waf build system, and album IR packs are **not** vendored.
- No TONE3000 or commercial IR content is included here.
