#!/usr/bin/env python3
"""Bass Suite — parametric cabinet IR model.

This module is used by ``tools/make_bass_irs.py`` to generate original CC0
cabinet IRs from a parametric speaker/cab model.  No reference audio is copied
into the output.

Requires numpy and scipy.
"""

from __future__ import annotations

import json
import math
import os
from dataclasses import dataclass, asdict
from datetime import date
from typing import Iterable, Sequence

import numpy as np
from scipy import signal
from scipy.optimize import differential_evolution, minimize

SAMPLE_RATE = 48000
IR_TAPS = 2048
F_MIN = 60.0
F_MAX = 12000.0
GRID_POINTS = 600


# ---------------------------------------------------------------------------
# RBJ biquads
# ---------------------------------------------------------------------------

def _omega(f: float, sr: float) -> float:
    return 2.0 * math.pi * max(1.0, min(f, sr * 0.49)) / sr


def low_shelf(f: float, gain_db: float, q: float, sr: float):
    a = 10.0 ** (gain_db / 40.0)
    w = _omega(f, sr)
    cw, sw = math.cos(w), math.sin(w)
    alpha = sw / (2.0 * max(q, 0.05))
    two = 2.0 * math.sqrt(a) * alpha
    b0 = a * ((a + 1) - (a - 1) * cw + two)
    b1 = 2 * a * ((a - 1) - (a + 1) * cw)
    b2 = a * ((a + 1) - (a - 1) * cw - two)
    a0 = (a + 1) + (a - 1) * cw + two
    a1 = -2 * ((a - 1) + (a + 1) * cw)
    a2 = (a + 1) + (a - 1) * cw - two
    return np.array([b0, b1, b2]) / a0, np.array([1.0, a1 / a0, a2 / a0])


def high_shelf(f: float, gain_db: float, q: float, sr: float):
    a = 10.0 ** (gain_db / 40.0)
    w = _omega(f, sr)
    cw, sw = math.cos(w), math.sin(w)
    alpha = sw / (2.0 * max(q, 0.05))
    two = 2.0 * math.sqrt(a) * alpha
    b0 = a * ((a + 1) + (a - 1) * cw + two)
    b1 = -2 * a * ((a - 1) + (a + 1) * cw)
    b2 = a * ((a + 1) + (a - 1) * cw - two)
    a0 = (a + 1) - (a - 1) * cw + two
    a1 = 2 * ((a - 1) - (a + 1) * cw)
    a2 = (a + 1) - (a - 1) * cw - two
    return np.array([b0, b1, b2]) / a0, np.array([1.0, a1 / a0, a2 / a0])


def peaking(f: float, gain_db: float, q: float, sr: float):
    a = 10.0 ** (gain_db / 40.0)
    w = _omega(f, sr)
    cw, sw = math.cos(w), math.sin(w)
    alpha = sw / (2.0 * max(q, 0.05))
    b0 = 1 + alpha * a
    b1 = -2 * cw
    b2 = 1 - alpha * a
    a0 = 1 + alpha / a
    a1 = -2 * cw
    a2 = 1 - alpha / a
    return np.array([b0, b1, b2]) / a0, np.array([1.0, a1 / a0, a2 / a0])


def low_pass(f: float, q: float, sr: float):
    w = _omega(f, sr)
    cw, sw = math.cos(w), math.sin(w)
    alpha = sw / (2.0 * max(q, 0.05))
    b0 = (1 - cw) * 0.5
    b1 = 1 - cw
    b2 = (1 - cw) * 0.5
    a0 = 1 + alpha
    a1 = -2 * cw
    a2 = 1 - alpha
    return np.array([b0, b1, b2]) / a0, np.array([1.0, a1 / a0, a2 / a0])


def high_pass(f: float, q: float, sr: float):
    w = _omega(f, sr)
    cw, sw = math.cos(w), math.sin(w)
    alpha = sw / (2.0 * max(q, 0.05))
    b0 = (1 + cw) * 0.5
    b1 = -(1 + cw)
    b2 = (1 + cw) * 0.5
    a0 = 1 + alpha
    a1 = -2 * cw
    a2 = 1 - alpha
    return np.array([b0, b1, b2]) / a0, np.array([1.0, a1 / a0, a2 / a0])


# ---------------------------------------------------------------------------
# Parameter vector
# ---------------------------------------------------------------------------

# name, low, high, unit
PARAM_SPACE: tuple[tuple[str, float, float, str], ...] = (
    ("res_f", 55.0, 140.0, "Hz"),
    ("res_gain", -4.0, 6.0, "dB"),
    ("res_q", 0.7, 4.0, ""),
    ("body_f", 90.0, 320.0, "Hz"),
    ("body_gain", -4.0, 6.0, "dB"),
    ("body_q", 0.5, 4.0, ""),
    ("baffle_f", 150.0, 650.0, "Hz"),
    ("baffle_gain", -6.0, 4.0, "dB"),
    ("presence_f", 1200.0, 4500.0, "Hz"),
    ("presence_gain", -5.0, 6.0, "dB"),
    ("presence_q", 0.4, 3.0, ""),
    ("breakup1_f", 1200.0, 3600.0, "Hz"),
    ("breakup1_gain", -7.0, 5.0, "dB"),
    ("breakup1_q", 2.0, 12.0, ""),
    ("breakup2_f", 3000.0, 5600.0, "Hz"),
    ("breakup2_gain", -7.0, 5.0, "dB"),
    ("breakup2_q", 2.0, 12.0, ""),
    ("breakup3_f", 5000.0, 8500.0, "Hz"),
    ("breakup3_gain", -9.0, 3.0, "dB"),
    ("breakup3_q", 2.0, 12.0, ""),
    ("hf_f", 4000.0, 13000.0, "Hz"),
    ("hf_gain", -20.0, 0.0, "dB"),
    ("lp_f", 5000.0, 17000.0, "Hz"),
    ("lp_q", 0.4, 1.3, ""),
    ("mic_low_f", 60.0, 220.0, "Hz"),
    ("mic_low_gain", -6.0, 6.0, "dB"),
    ("mic_hi_f", 4000.0, 11000.0, "Hz"),
    ("mic_hi_gain", -14.0, 3.0, "dB"),
    ("tilt", -3.0, 3.0, "dB"),
    ("gain", -8.0, 8.0, "dB"),
)

BOUNDS = [(lo, hi) for _, lo, hi, _ in PARAM_SPACE]


def params_dict(x: Sequence[float]) -> dict[str, float]:
    return {name: float(value) for (name, _, _, _), value in zip(PARAM_SPACE, x)}


def _filters(x: Sequence[float], sr: float):
    p = params_dict(x)
    return [
        high_pass(35.0, 0.707, sr),
        peaking(p["res_f"], p["res_gain"], p["res_q"], sr),
        peaking(p["body_f"], p["body_gain"], p["body_q"], sr),
        low_shelf(p["baffle_f"], p["baffle_gain"], 0.7, sr),
        peaking(p["presence_f"], p["presence_gain"], p["presence_q"], sr),
        peaking(p["breakup1_f"], p["breakup1_gain"], p["breakup1_q"], sr),
        peaking(p["breakup2_f"], p["breakup2_gain"], p["breakup2_q"], sr),
        peaking(p["breakup3_f"], p["breakup3_gain"], p["breakup3_q"], sr),
        high_shelf(p["hf_f"], p["hf_gain"], 0.7, sr),
        low_pass(p["lp_f"], p["lp_q"], sr),
        low_shelf(p["mic_low_f"], p["mic_low_gain"], 0.7, sr),
        high_shelf(p["mic_hi_f"], p["mic_hi_gain"], 0.7, sr),
        high_shelf(1000.0, p["tilt"], 0.7, sr),
    ]


def response_db(x: Sequence[float], sr: float, freqs: np.ndarray) -> np.ndarray:
    """Cascaded magnitude response in dB at the given frequencies."""
    h = np.ones_like(freqs, dtype=np.complex128)
    for b, a in _filters(x, sr):
        _, hh = signal.freqz(b, a, worN=freqs, fs=sr)
        h *= hh
    db = 20.0 * np.log10(np.maximum(np.abs(h), 1e-12))
    db += params_dict(x)["gain"]
    return db


def impulse_response(x: Sequence[float], sr: float = SAMPLE_RATE,
                     taps: int = IR_TAPS) -> np.ndarray:
    ir = np.zeros(taps, dtype=np.float64)
    ir[0] = 1.0
    for b, a in _filters(x, sr):
        ir = signal.lfilter(b, a, ir)
    ir *= 10.0 ** (params_dict(x)["gain"] / 20.0)
    return ir


def minimum_phase(ir: np.ndarray, taps: int = IR_TAPS) -> np.ndarray:
    """Minimum-phase IR with the same magnitude via the real cepstrum."""
    n = 1
    while n < max(len(ir) * 4, 8192):
        n *= 2
    h = np.fft.rfft(ir, n)
    log_mag = np.log(np.maximum(np.abs(h), 1e-12))
    cep = np.fft.irfft(log_mag, n)
    # Keep c[0], double the positive quefrency, zero the negative side.
    cep_min = np.zeros(n)
    cep_min[0] = cep[0]
    half = n // 2
    cep_min[1:half] = 2.0 * cep[1:half]
    if n % 2 == 0:
        cep_min[half] = cep[half]
    min_ir = np.fft.irfft(np.exp(np.fft.rfft(cep_min)), n)
    return min_ir[:taps]


# ---------------------------------------------------------------------------
# Reference analysis
# ---------------------------------------------------------------------------

def read_wav_mono(path: str, sr: int = SAMPLE_RATE) -> np.ndarray:
    import soundfile as sf
    data, rate = sf.read(path, always_2d=True, dtype="float64")
    mono = data.mean(axis=1)
    if rate != sr:
        g = math.gcd(int(rate), int(sr))
        mono = signal.resample_poly(mono, sr // g, rate // g)
    return mono


def trim_reference(ir: np.ndarray, sr: int = SAMPLE_RATE,
                   max_ms: float = 60.0) -> np.ndarray:
    """Keep the useful cab window, with a short fade to avoid a hard cut."""
    peak = int(np.argmax(np.abs(ir))) if ir.size else 0
    start = max(0, peak - int(0.002 * sr))
    end = min(len(ir), start + int(max_ms * 0.001 * sr))
    out = ir[start:end].copy()
    if out.size == 0:
        return out
    fade = min(out.size, int(0.005 * sr))
    if fade > 1:
        out[-fade:] *= np.linspace(0.0, 1.0, fade)[::-1]
    return out


def magnitude_db(ir: np.ndarray, sr: int, n_fft: int = 16384):
    h = np.fft.rfft(ir, n_fft)
    freqs = np.fft.rfftfreq(n_fft, 1.0 / sr)
    return freqs, 20.0 * np.log10(np.maximum(np.abs(h), 1e-12))


def log_grid() -> np.ndarray:
    return np.geomspace(F_MIN, F_MAX, GRID_POINTS)


def smooth_db(freqs: np.ndarray, db: np.ndarray, grid: np.ndarray,
              octaves: float = 1.0 / 6.0) -> np.ndarray:
    lf = np.log10(np.maximum(freqs, 1e-9))
    lg = np.log10(grid)
    interp = np.interp(lg, lf, db)
    spacing = lg[1] - lg[0]
    sigma = max(1.0, 0.5 * octaves * math.log10(2.0) / spacing)
    radius = max(1, int(3 * sigma))
    kernel = np.exp(-0.5 * (np.arange(-radius, radius + 1) / sigma) ** 2)
    kernel /= kernel.sum()
    padded = np.pad(interp, radius, mode="edge")
    return np.convolve(padded, kernel, mode="same")[radius:-radius]


def target_from_reference(paths: Iterable[str], sr: int = SAMPLE_RATE,
                          grid: np.ndarray | None = None) -> np.ndarray:
    if grid is None:
        grid = log_grid()
    shapes = []
    for path in paths:
        ir = trim_reference(read_wav_mono(path, sr), sr)
        if ir.size < 32:
            raise ValueError(f"reference too short: {path}")
        f, db = magnitude_db(ir, sr)
        shapes.append(smooth_db(f, db, grid))
    if not shapes:
        raise ValueError("no references")
    return np.mean(np.stack(shapes, axis=0), axis=0)


# ---------------------------------------------------------------------------
# Fitting
# ---------------------------------------------------------------------------

def weights(grid: np.ndarray) -> np.ndarray:
    w = np.ones_like(grid)
    w[(grid < 80.0)] = 0.5
    w[(grid > 8000.0)] = 0.35
    w[(grid >= 800.0) & (grid <= 6000.0)] = 1.25
    return w / w.mean()


def loss(x, target, sr, grid, w) -> float:
    model = smooth_db(grid, response_db(x, sr, grid), grid)
    d = (model - target) * w
    return float(np.mean(d * d))


def fit(target: np.ndarray, sr: int = SAMPLE_RATE, seed: int = 1234,
        maxiter: int = 140, popsize: int = 12,
        progress: bool = False) -> tuple[np.ndarray, dict]:
    grid = log_grid()
    w = weights(grid)

    def objective(x):
        return loss(x, target, sr, grid, w)

    result = differential_evolution(
        objective,
        BOUNDS,
        seed=seed,
        maxiter=maxiter,
        popsize=popsize,
        tol=1e-4,
        polish=False,
        workers=1,
        updating="immediate",
        disp=progress,
    )
    x = result.x
    refinement = minimize(
        objective,
        x,
        method="L-BFGS-B",
        bounds=BOUNDS,
        options={"maxiter": 250, "ftol": 1e-10},
    )
    if refinement.fun < result.fun:
        x = refinement.x
    model = smooth_db(grid, response_db(x, sr, grid), grid)
    diff = model - target
    metrics = {
        "mean_abs_db": float(np.mean(np.abs(diff))),
        "max_abs_db": float(np.max(np.abs(diff))),
        "rms_db": float(np.sqrt(np.mean(diff * diff))),
        "loss": float(objective(x)),
    }
    return x, metrics


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------

def write_wavs(x: Sequence[float], out_dir: str, name: str,
               sr: int = SAMPLE_RATE, taps: int = IR_TAPS,
               variants: Sequence[str] = ("57", "121", "blend"),
               rate_tag: str = "") -> list[str]:
    import soundfile as sf
    os.makedirs(out_dir, exist_ok=True)
    paths = []
    base = impulse_response(x, sr, taps)
    base = minimum_phase(base, taps)
    peak = float(np.max(np.abs(base)))
    if peak > 0:
        base *= (10.0 ** (-0.3 / 20.0)) / peak
    for variant in variants:
        ir = base.copy()
        if variant == "121":
            ir = minimum_phase(impulse_response(
                _variant_params(x, ribbon=True), sr, taps), taps)
            ir *= (10.0 ** (-0.3 / 20.0)) / max(float(np.max(np.abs(ir))), 1e-12)
        elif variant == "blend":
            r = minimum_phase(impulse_response(
                _variant_params(x, ribbon=True), sr, taps), taps)
            r *= (10.0 ** (-0.3 / 20.0)) / max(float(np.max(np.abs(r))), 1e-12)
            ir = 0.65 * base + 0.35 * r
            ir *= (10.0 ** (-0.3 / 20.0)) / max(float(np.max(np.abs(ir))), 1e-12)
        path = os.path.join(out_dir, f"cab_{name}_{variant}{rate_tag}.wav")
        sf.write(path, ir.astype(np.float32), sr, subtype="PCM_24")
        paths.append(path)
    return paths


def _variant_params(x: Sequence[float], ribbon: bool = False) -> np.ndarray:
    """A small mic/position variation so the three factory variants differ
    without becoming separate models."""
    p = params_dict(x)
    if ribbon:
        p["mic_hi_gain"] -= 1.6
        p["mic_hi_f"] *= 0.92
        p["presence_gain"] -= 0.5
    return np.array([p[name] for name, _, _, _ in PARAM_SPACE])


def write_manifest(out_dir: str, name: str, refs: Sequence[str],
                   x: Sequence[float], metrics: dict, extra: dict | None = None):
    manifest_path = os.path.join(out_dir, "manifest.json")
    old = {}
    if os.path.exists(manifest_path):
        try:
            with open(manifest_path, "r", encoding="utf-8") as f:
                old = json.load(f)
        except Exception:
            old = {}
    entry = {
        "name": name,
        "model_version": 1,
        "generated": date.today().isoformat(),
        "license": "CC0-1.0",
        "references": [os.path.basename(p) for p in refs],
        "reference_license": "T3K (not redistributed)",
        "parameters": params_dict(x),
        "metrics": metrics,
    }
    if extra:
        entry.update(extra)
    old[name] = entry
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(old, f, indent=2, sort_keys=True)
        f.write("\n")
    return manifest_path
