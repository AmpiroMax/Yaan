#!/usr/bin/env python3
#
# Created: 10:08:2026 - 01:53:17
# Last updated: 10:08:2026 - 01:53:17
# File: engine/platform/audio/tools/gen_placeholder_sounds.py
#
# Responsibility:
# - Synthesizes the PLACEHOLDER sound set (footsteps per surface class, jump/
#   land/splash one-shots, the wind loop) as .wav files, stdlib only (Rule 24:
#   agents install nothing). Deterministic: fixed seed, same bytes every run.
#
# Dependencies:
# - Uses: Python stdlib (wave, struct, math, random, argparse, pathlib).
# - Used by: run by hand; outputs committed to games/daggerfall_n/assets/audio/.
#   Consumed at runtime via IAudio::load_sound (paths are content, app-wired).
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - THESE ARE PLACEHOLDERS. Real recorded footsteps/wind are a later asset
#   pass (recorded in the audio module README). The point today is that the
#   surfaces DIFFER convincingly and the step event has a same-tick voice —
#   not that any single file sounds shippable.
# - Regenerate with:  python3 engine/platform/audio/tools/gen_placeholder_sounds.py
#
# UPD:
# - 10:08:2026 - 01:53:17: Initial set: 4 takes x 5 surface classes, jump,
#                          land soft/hard, water entry, seamless 8 s wind loop.

from __future__ import annotations

import argparse
import math
import random
import struct
import wave
from pathlib import Path

RATE = 22050
SEED = 20260810


def write_wav(path: Path, samples: list[float]) -> None:
    """16-bit mono PCM. Samples are clamped floats in [-1, 1]."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        frames = bytearray()
        for s in samples:
            s = max(-1.0, min(1.0, s))
            frames += struct.pack("<h", int(s * 32767))
        w.writeframes(bytes(frames))


def normalize(samples: list[float], peak: float) -> list[float]:
    m = max((abs(s) for s in samples), default=1.0)
    if m == 0.0:
        return samples
    k = peak / m
    return [s * k for s in samples]


def lowpass(samples: list[float], cutoff_hz: float) -> list[float]:
    """One-pole lowpass. Cheap and stable; placeholder-grade on purpose."""
    if cutoff_hz >= RATE / 2:
        return samples[:]
    a = 1.0 - math.exp(-2.0 * math.pi * cutoff_hz / RATE)
    out, y = [], 0.0
    for s in samples:
        y += a * (s - y)
        out.append(y)
    return out


def highpass(samples: list[float], cutoff_hz: float) -> list[float]:
    low = lowpass(samples, cutoff_hz)
    return [s - l for s, l in zip(samples, low)]


def env_decay(n: int, attack_s: float, tau_s: float) -> list[float]:
    """Linear attack, exponential decay envelope of length n."""
    attack = max(1, int(attack_s * RATE))
    out = []
    for i in range(n):
        a = min(1.0, i / attack)
        d = math.exp(-max(0, i - attack) / (tau_s * RATE))
        out.append(a * d)
    return out


def noise(rng: random.Random, n: int) -> list[float]:
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def thump(freq_hz: float, n: int, sweep: float = 0.6) -> list[float]:
    """Decaying sine with a downward pitch sweep — the body of an impact."""
    out, phase = [], 0.0
    for i in range(n):
        f = freq_hz * (1.0 - (1.0 - sweep) * i / n)
        phase += 2.0 * math.pi * f / RATE
        out.append(math.sin(phase) * math.exp(-i / (0.05 * RATE)))
    return out


def mix(*tracks: list[float]) -> list[float]:
    n = max(len(t) for t in tracks)
    out = [0.0] * n
    for t in tracks:
        for i, s in enumerate(t):
            out[i] += s
    return out


def scaled(track: list[float], k: float) -> list[float]:
    return [s * k for s in track]


# --- Footstep recipes: what makes each surface READ as itself -----------------
# grass: soft rustle over a small thud (blades brush before weight lands)
# rock:  sharp broadband click, short ring, solid thud (hard contact)
# sand:  no click at all, a slow "shf" that keeps hissing as grains shift
# blend (grass/rock): gravel — a burst of discrete micro-impacts
# waterbed: a plop (pitch-swept bubble) plus a splash tail


def step_grass(rng: random.Random) -> list[float]:
    n = int(0.16 * RATE)
    rustle = [s * e for s, e in zip(lowpass(noise(rng, n), 900), env_decay(n, 0.004, 0.045))]
    body = lowpass(thump(110, n), 300)
    return normalize(mix(scaled(rustle, 0.8), scaled(body, 0.5)), 0.55)


def step_rock(rng: random.Random) -> list[float]:
    n = int(0.11 * RATE)
    click = [s * e for s, e in zip(highpass(noise(rng, n), 1500), env_decay(n, 0.001, 0.012))]
    ring = [s * e for s, e in zip(lowpass(noise(rng, n), 3500), env_decay(n, 0.002, 0.03))]
    body = thump(150, n)
    return normalize(mix(scaled(click, 0.9), scaled(ring, 0.4), scaled(body, 0.5)), 0.6)


def step_sand(rng: random.Random) -> list[float]:
    n = int(0.22 * RATE)
    hiss = [s * e for s, e in zip(lowpass(noise(rng, n), 1400), env_decay(n, 0.02, 0.09))]
    # grain shift: slow amplitude wobble so the tail is not a clean fade
    wob = [1.0 + 0.35 * math.sin(2 * math.pi * 13.0 * i / RATE + rng.random()) for i in range(n)]
    return normalize([h * w for h, w in zip(hiss, wob)], 0.5)


def step_gravel(rng: random.Random) -> list[float]:
    n = int(0.15 * RATE)
    out = [0.0] * n
    for _ in range(rng.randint(6, 9)):  # discrete micro-impacts
        start = int(rng.uniform(0.0, 0.08) * RATE)
        m = int(rng.uniform(0.008, 0.022) * RATE)
        grain = [s * e for s, e in zip(lowpass(noise(rng, m), rng.uniform(1200, 3000)),
                                       env_decay(m, 0.001, 0.006))]
        for i, s in enumerate(grain):
            if start + i < n:
                out[start + i] += s * rng.uniform(0.4, 1.0)
    body = lowpass(thump(130, n), 350)
    return normalize(mix(out, scaled(body, 0.4)), 0.58)


def step_water(rng: random.Random) -> list[float]:
    n = int(0.26 * RATE)
    plop = thump(240, int(0.08 * RATE), sweep=0.35)
    splash = [s * e for s, e in zip(highpass(noise(rng, n), 600), env_decay(n, 0.006, 0.07))]
    return normalize(mix(scaled(plop, 0.8), scaled(splash, 0.6)), 0.6)


def jump_whoosh(rng: random.Random) -> list[float]:
    n = int(0.18 * RATE)
    body = lowpass(highpass(noise(rng, n), 250), 1100)
    envl = [math.sin(math.pi * i / n) ** 2 for i in range(n)]  # swell and fade
    return normalize([s * e for s, e in zip(body, envl)], 0.4)


def land(rng: random.Random, hard: bool) -> list[float]:
    dur = 0.28 if hard else 0.16
    n = int(dur * RATE)
    body = lowpass(thump(70 if hard else 90, n), 220)
    burst = [s * e for s, e in zip(lowpass(noise(rng, n), 1800 if hard else 900),
                                   env_decay(n, 0.001, 0.03 if hard else 0.018))]
    return normalize(mix(scaled(body, 1.0), scaled(burst, 0.7 if hard else 0.45)),
                     0.75 if hard else 0.55)


def splash_enter(rng: random.Random) -> list[float]:
    n = int(0.45 * RATE)
    plop = thump(180, int(0.12 * RATE), sweep=0.3)
    wash = [s * e for s, e in zip(highpass(noise(rng, n), 400), env_decay(n, 0.01, 0.13))]
    return normalize(mix(scaled(plop, 0.9), scaled(wash, 0.8)), 0.7)


def wind_loop(rng: random.Random, seconds: float = 8.0) -> list[float]:
    """Seamless: the filter modulation is periodic with the loop length AND the
    tail is crossfaded into the head, so the seam is inaudible twice over."""
    n = int(seconds * RATE)
    raw = noise(rng, n)
    out = []
    y = 0.0
    for i, s in enumerate(raw):
        t = i / RATE
        # two LFOs, both integer cycles per loop -> periodic by construction
        m = 0.5 + 0.3 * math.sin(2 * math.pi * t / seconds * 2.0) \
                + 0.2 * math.sin(2 * math.pi * t / seconds * 5.0)
        cutoff = 250.0 + 550.0 * max(0.0, m)
        a = 1.0 - math.exp(-2.0 * math.pi * cutoff / RATE)
        y += a * (s - y)
        out.append(y * (0.6 + 0.4 * m))
    fade = int(0.5 * RATE)  # crossfade the seam
    for i in range(fade):
        k = i / fade
        out[i] = out[i] * k + out[n - fade + i] * (1.0 - k)
    return normalize(out[: n - fade], 0.5)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate placeholder sounds")
    parser.add_argument("--out", default="games/daggerfall_n/assets/audio",
                        help="output directory (repo-relative by default)")
    args = parser.parse_args()
    out = Path(args.out)
    rng = random.Random(SEED)

    surfaces = {
        "grass": step_grass,
        "rock": step_rock,
        "sand": step_sand,
        "gravel": step_gravel,   # SurfaceClass::GrassRockBlend
        "water": step_water,     # SurfaceClass::WaterBed (wading steps)
    }
    for name, recipe in surfaces.items():
        for take in range(1, 5):
            write_wav(out / f"footstep_{name}_{take}.wav", recipe(rng))

    write_wav(out / "jump_takeoff.wav", jump_whoosh(rng))
    write_wav(out / "land_soft.wav", land(rng, hard=False))
    write_wav(out / "land_hard.wav", land(rng, hard=True))
    write_wav(out / "splash_enter.wav", splash_enter(rng))
    write_wav(out / "wind_loop.wav", wind_loop(rng))
    print(f"wrote {5 * 4 + 5} files to {out}")


if __name__ == "__main__":
    main()
