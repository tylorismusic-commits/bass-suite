# Generated factory cabinet IRs

These are **original CC0** bass-cabinet impulse responses. They are synthesized
from a parametric speaker/cab model (low-frequency resonance, body, baffle,
cone breakup, high-frequency rolloff and microphone voicing), then converted to
minimum phase with the real cepstrum. No commercial IR, capture or reference
audio is copied into the output.

## Files

- `1x15`, `4x10`, `2x12`, `8x10`, `2x10`, `1x12`
- Each cab has three mic voicings: `57` (dynamic), `121` (ribbon), `blend`.
- 48 kHz files are 2048 taps; 96 kHz files are 4096 taps.
- The engine normalizes every IR to unity magnitude at 1 kHz.

## Regenerate

Use the same Python environment as the Amp Suite IR tools:

```sh
/tmp/opencode/amp-venv/bin/python tools/make_bass_irs.py
```

or create a local venv:

```sh
python3 -m venv .venv
.venv/bin/pip install numpy scipy soundfile
.venv/bin/python tools/make_bass_irs.py
```

The generator writes `assets/irs/<cab>/cab_<cab>_<mic>.wav`, the 96 kHz
variants, and the aggregate `assets/irs/manifest.json`.
