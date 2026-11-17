#!/usr/bin/env python3
"""Generate Bass Suite factory cab IRs.

The factory IRs are original CC0 content.  They are built from a parametric
speaker/cab model (resonance, body, baffle, cone breakup, high-frequency rolloff
and microphone voicing), then converted to minimum phase with the real cepstrum.
No commercial IR, capture or reference audio is copied into the output.

Run with the same Python environment used by Amp Suite's IR tools:

    /tmp/opencode/amp-venv/bin/python tools/make_bass_irs.py
"""

from __future__ import annotations

import json
import math
import os
import sys

import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ir_model  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    "assets", "irs")
TAPS_48 = 2048
TAPS_96 = 4096


def p(**kwargs):
    """Build a parameter vector in PARAM_SPACE order."""
    base = {
        "res_f": 60.0, "res_gain": 2.0, "res_q": 1.1,
        "body_f": 140.0, "body_gain": 1.0, "body_q": 1.0,
        "baffle_f": 260.0, "baffle_gain": -1.5,
        "presence_f": 2200.0, "presence_gain": 0.0, "presence_q": 0.9,
        "breakup1_f": 2000.0, "breakup1_gain": -3.0, "breakup1_q": 4.0,
        "breakup2_f": 3600.0, "breakup2_gain": -5.0, "breakup2_q": 5.0,
        "breakup3_f": 6500.0, "breakup3_gain": -9.0, "breakup3_q": 6.0,
        "hf_f": 5500.0, "hf_gain": -6.0,
        "lp_f": 6000.0, "lp_q": 0.75,
        "mic_low_f": 110.0, "mic_low_gain": 0.5,
        "mic_hi_f": 5800.0, "mic_hi_gain": -3.0,
        "tilt": -0.25, "gain": -6.0,
    }
    base.update(kwargs)
    return [base[name] for name, _, _, _ in ir_model.PARAM_SPACE]


CABS = {
    "1x15": dict(res_f=55.0, res_gain=3.0, res_q=1.1,
                 body_f=120.0, body_gain=2.0, body_q=1.0,
                 baffle_f=250.0, baffle_gain=-2.0,
                 presence_f=1800.0, presence_gain=-2.0,
                 breakup1_f=1800.0, breakup1_gain=-4.0,
                 breakup2_f=3500.0, breakup2_gain=-6.0,
                 breakup3_f=6000.0, breakup3_gain=-10.0,
                 hf_f=5000.0, hf_gain=-8.0, lp_f=5000.0, lp_q=0.70),
    "4x10": dict(res_f=65.0, res_gain=1.5, res_q=1.4,
                 body_f=170.0, body_gain=1.0, body_q=1.2,
                 baffle_f=320.0, baffle_gain=-1.0,
                 presence_f=2600.0, presence_gain=2.0,
                 breakup1_f=2400.0, breakup1_gain=-2.0,
                 breakup2_f=4000.0, breakup2_gain=-4.0,
                 breakup3_f=7000.0, breakup3_gain=-8.0,
                 hf_f=6000.0, hf_gain=-4.0, lp_f=7000.0, lp_q=0.80),
    "2x12": dict(res_f=58.0, res_gain=2.5, res_q=1.2,
                 body_f=140.0, body_gain=1.5, body_q=1.0,
                 baffle_f=280.0, baffle_gain=-1.5,
                 presence_f=2200.0, presence_gain=0.0,
                 breakup1_f=2000.0, breakup1_gain=-3.0,
                 breakup2_f=3600.0, breakup2_gain=-5.0,
                 breakup3_f=6500.0, breakup3_gain=-9.0,
                 hf_f=5500.0, hf_gain=-6.0, lp_f=6000.0, lp_q=0.75),
    "8x10": dict(res_f=50.0, res_gain=3.5, res_q=1.0,
                 body_f=110.0, body_gain=2.5, body_q=1.0,
                 baffle_f=220.0, baffle_gain=-2.0,
                 presence_f=3000.0, presence_gain=3.0,
                 breakup1_f=2800.0, breakup1_gain=-1.0,
                 breakup2_f=4500.0, breakup2_gain=-2.0,
                 breakup3_f=8000.0, breakup3_gain=-6.0,
                 hf_f=7000.0, hf_gain=-2.0, lp_f=8000.0, lp_q=0.80),
}

MICS = {
    "57": dict(mic_low_f=100.0, mic_low_gain=0.0, mic_hi_f=6500.0,
               mic_hi_gain=0.0, tilt=0.0),
    "121": dict(mic_low_f=120.0, mic_low_gain=1.0, mic_hi_f=5000.0,
                mic_hi_gain=-6.0, tilt=-0.5),
    "blend": dict(mic_low_f=110.0, mic_low_gain=0.5, mic_hi_f=5800.0,
                  mic_hi_gain=-3.0, tilt=-0.25),
}


def write_wav(path: str, ir: np.ndarray, sr: int) -> None:
    peak = float(np.max(np.abs(ir))) if ir.size else 0.0
    if peak > 1.0e-9:
        ir = ir * (0.90 / peak)
    sf.write(path, ir.astype(np.float32), sr, subtype="PCM_24")


def main() -> int:
    manifest = {}
    for cab, cab_params in CABS.items():
        out_dir = os.path.join(ROOT, cab)
        os.makedirs(out_dir, exist_ok=True)
        manifest[cab] = {
            "license": "CC0-1.0",
            "method": "parametric bass-cab magnitude synthesis, minimum phase",
            "taps": {"48k": TAPS_48, "96k": TAPS_96},
            "mics": {},
        }
        for mic, mic_params in MICS.items():
            params = p(**cab_params, **mic_params)
            for sr, taps in ((48000, TAPS_48), (96000, TAPS_96)):
                ir = ir_model.impulse_response(params, sr=sr, taps=taps)
                ir = ir_model.minimum_phase(ir, taps=taps)
                name = f"cab_{cab}_{mic}" + ("_96k" if sr == 96000 else "")
                write_wav(os.path.join(out_dir, name + ".wav"), ir, sr)
            manifest[cab]["mics"][mic] = {
                "file_48k": f"cab_{cab}_{mic}.wav",
                "file_96k": f"cab_{cab}_{mic}_96k.wav",
                "params": mic_params,
            }
        print(f"wrote {cab}: {', '.join(MICS)}")
    with open(os.path.join(ROOT, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
        f.write("\n")
    print(f"wrote {os.path.join(ROOT, 'manifest.json')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
