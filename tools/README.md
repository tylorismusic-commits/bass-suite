# Bass Suite tools

- `gen_ttl.cpp` — generates the LV2 TTL from `params.hpp` and `presets.hpp`.
- `make_bass_irs.py` — generates the four CC0 factory cab IRs.
- `bass_probe.cpp` — offline amp level/THD probe.
- `bass_tone_probe.cpp` — tone-stack response probe.
- `ir_model.py` — parametric cab model used by `make_bass_irs.py`.

Generate the factory IRs:

```sh
/tmp/opencode/amp-venv/bin/python tools/make_bass_irs.py
```

or with a local venv:

```sh
python3 -m venv .venv
.venv/bin/pip install -r tools/requirements.txt
.venv/bin/python tools/make_bass_irs.py
```

Generate the TTL:

```sh
cmake --build build --target bass_ttl_gen
./build/bass_ttl_gen --root .
```

Probe the amps:

```sh
./build/bass_probe
./build/bass_tone_probe
```
