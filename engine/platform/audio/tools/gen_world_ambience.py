#!/usr/bin/env python3
#
# Created: 28:08:2026 - 13:53:00
# Last updated: 28:08:2026 - 13:53:00
# File: engine/platform/audio/tools/gen_world_ambience.py
#
# Responsibility:
# - Synthesizes the WORLD AMBIENCE loops that emitters carry: leaf rustle per
#   tree family (broadleaf/conifer) at three wind steps, and moving water
#   (small stream, wide river). Seamless loops, stdlib only (Rule 24: agents
#   install nothing). Deterministic: fixed seed, same bytes every run.
#
# Dependencies:
# - Uses: Python stdlib (wave, struct, math, random, argparse, pathlib) and
#   ffmpeg on PATH for the .wav -> .ogg step (already used across the project).
# - Used by: run by hand; outputs committed to
#   games/daggerfall_n/assets/audio/world/. Consumed at runtime via
#   IAudio::load_sound, attached to tree-canopy and water emitters.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - NO loudnorm ON THESE FILES. loudnorm compresses dynamics; on a background
#   loop that makes the rustle "breathe" audibly on every gust. Mastering here
#   is plain peak normalisation, and the measured peak/RMS of each file is
#   printed so the engine sets its gain from numbers, not by ear.
# - Loops are seamless BY CONSTRUCTION (tail cross-faded onto head) and the
#   seam is VERIFIED by measurement at the end of the run, not by listening.
# - Regenerate with:
#     python3 engine/platform/audio/tools/gen_world_ambience.py
#
# UPD:
# - 28:08:2026 - 13:53:00: Initial set: leaves_broad_1..3, leaves_conifer_1..3,
#               stream_small, river_wide. Replaces the source-less wind_loop.
#               (Штампы дополнены до часов зоной «звук от источника» — она же
#               кладёт файл в историю; содержимое генератора не тронуто.)

from __future__ import annotations

import argparse
import math
import random
import struct
import subprocess
import wave
from pathlib import Path

RATE = 48000
SEED = 20260828
LOOP_SECONDS = 12.0          # long enough that the ear does not catch the period
FADE_SECONDS = 1.5           # tail wrapped onto head to close the loop
PEAK = 0.5                   # -6 dBFS: background beds leave headroom for events
# Wind steps MUST get louder with strength. Normalising each file to the same
# peak separately destroyed exactly that: step 2 came out quieter than step 1.
# So the three steps of a family share ONE gain, and carry these ratios.
STEP_GAIN = (0.55, 0.78, 1.00)


# --- helpers ----------------------------------------------------------------

def noise(rng: random.Random, n: int) -> list[float]:
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def lowpass(xs: list[float], cutoff_hz: float) -> list[float]:
    """One-pole low pass. Two of them in series give a gentler, more natural
    slope than one steep filter — foliage has no sharp spectral edge."""
    a = 1.0 - math.exp(-2.0 * math.pi * cutoff_hz / RATE)
    y, out = 0.0, []
    for x in xs:
        y += a * (x - y)
        out.append(y)
    return out


def highpass(xs: list[float], cutoff_hz: float) -> list[float]:
    return [x - y for x, y in zip(xs, lowpass(xs, cutoff_hz))]


def band(xs: list[float], lo_hz: float, hi_hz: float) -> list[float]:
    return highpass(lowpass(lowpass(xs, hi_hz), hi_hz), lo_hz)


def resonator(xs: list[float], freq_hz: float, q: float) -> list[float]:
    """Narrow resonance — a water droplet is a short ring at one pitch."""
    w = 2.0 * math.pi * freq_hz / RATE
    r = math.exp(-w / (2.0 * q))
    a1, a2 = 2.0 * r * math.cos(w), -r * r
    y1 = y2 = 0.0
    out = []
    for x in xs:
        y = x + a1 * y1 + a2 * y2
        y2, y1 = y1, y
        out.append(y)
    return out


def gusts(rng: random.Random, n: int, depth: float, rate_hz: float) -> list[float]:
    """Wind is not steady: it breathes. Sum of a few slow random waves, so the
    period never repeats audibly inside the loop."""
    parts = []
    for k in range(4):
        f = rate_hz * (0.6 + 0.5 * k) * rng.uniform(0.8, 1.25)
        ph = rng.uniform(0.0, 2.0 * math.pi)
        parts.append((f, ph, rng.uniform(0.6, 1.0)))
    out = []
    for i in range(n):
        t = i / RATE
        v = sum(a * math.sin(2.0 * math.pi * f * t + ph) for f, ph, a in parts)
        v /= sum(a for _, _, a in parts)
        out.append(1.0 - depth + depth * (0.5 + 0.5 * v))
    return out


def sprinkle(rng: random.Random, n: int, count: int, make) -> list[float]:
    """Scatter short events over the buffer, wrapping past the end so the
    density stays even across the loop seam."""
    out = [0.0] * n
    for _ in range(count):
        at = rng.randrange(n)
        ev = make(rng)
        for j, v in enumerate(ev):
            out[(at + j) % n] += v
    return out


def normalize(xs: list[float], peak: float) -> list[float]:
    m = max(abs(x) for x in xs) or 1.0
    k = peak / m
    return [x * k for x in xs]


def close_loop(xs: list[float], fade_n: int) -> list[float]:
    """Wrap the tail onto the head with an equal-power cross-fade. After this
    the last sample flows into the first, so the loop has no click."""
    body = xs[:-fade_n]
    tail = xs[-fade_n:]
    for i in range(fade_n):
        a = math.cos(0.5 * math.pi * i / fade_n)      # tail fading out
        b = math.sin(0.5 * math.pi * i / fade_n)      # head fading in
        body[i] = body[i] * b + tail[i] * a
    return body


# --- voices -----------------------------------------------------------------

def leaf_clatter(bright: float):
    """One leaf slapping another: a very short bright burst. Broadleaf foliage
    is made of these; conifers have almost none, needles do not slap."""
    def make(rng: random.Random) -> list[float]:
        n = int(RATE * rng.uniform(0.004, 0.014))
        env = [math.exp(-6.0 * i / n) for i in range(n)]
        raw = band(noise(rng, n), 600.0 * bright, 4200.0 * bright)
        return [v * e * rng.uniform(0.5, 1.0) for v, e in zip(raw, env)]
    return make


def droplet(lo: float, hi: float):
    """A water droplet: a short ring whose pitch rises as the bubble collapses."""
    def make(rng: random.Random) -> list[float]:
        n = int(RATE * rng.uniform(0.010, 0.045))
        f = rng.uniform(lo, hi)
        env = [math.exp(-9.0 * i / n) for i in range(n)]
        exc = [rng.uniform(-1.0, 1.0) * e for e in env]
        ring = resonator(exc, f, q=rng.uniform(8.0, 26.0))
        return [v * 0.06 for v in ring]
    return make


def leaves(rng: random.Random, n: int, conifer: bool, step: int) -> list[float]:
    """step 1..3 — wind strength. Stronger wind means louder, brighter, gustier
    and, for broadleaf, far more leaf-on-leaf clatter.

    Bands are deliberately LOW. The first pass sat at 6-9 kHz centroid and read
    as plain hiss: real foliage carries its energy around 3-4 kHz, and conifers
    only about 1.5 kHz above the broadleaves — not an octave above."""
    depth = (0.30, 0.45, 0.60)[step - 1]
    bright = (0.80, 1.00, 1.25)[step - 1]
    if conifer:
        # needles: a narrow, high, even hiss — no clatter, gentler breathing
        bed = band(noise(rng, n), 900.0 * bright, 5200.0 * bright)
        bed = [v * 0.55 for v in bed]
        clatter = sprinkle(rng, n, (18, 55, 120)[step - 1], leaf_clatter(bright * 1.3))
        clatter = [v * 0.25 for v in clatter]
        env = gusts(rng, n, depth * 0.8, rate_hz=0.16)
    else:
        # broad leaves: wider band, lower centre, and the clatter carries it
        bed = band(noise(rng, n), 380.0 * bright, 3400.0 * bright)
        bed = [v * 0.6 for v in bed]
        clatter = sprinkle(rng, n, (140, 420, 950)[step - 1], leaf_clatter(bright))
        env = gusts(rng, n, depth, rate_hz=0.12)
    return [(b + c) * e for b, c, e in zip(bed, clatter, env)]


def water(rng: random.Random, n: int, wide: bool) -> list[float]:
    """A stream chatters with single droplets; a river is mass — the droplets
    merge into one broad rush and only the low end tells them apart."""
    if wide:
        bed = band(noise(rng, n), 120.0, 2600.0)
        bed = [v * 0.85 for v in bed]
        drops = sprinkle(rng, n, 380, droplet(220.0, 1100.0))
        env = gusts(rng, n, 0.16, rate_hz=0.07)       # slow swell of the current
    else:
        bed = band(noise(rng, n), 420.0, 4200.0)
        bed = [v * 0.45 for v in bed]
        drops = sprinkle(rng, n, 1500, droplet(500.0, 2600.0))
        env = gusts(rng, n, 0.28, rate_hz=0.19)
    return [(b + d) * e for b, d, e in zip(bed, drops, env)]


# --- output -----------------------------------------------------------------

def write_wav(path: Path, xs: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)                    # mono on purpose: the spatialiser
        w.setsampwidth(2)                    # builds the stereo image itself
        w.setframerate(RATE)
        w.writeframes(b"".join(
            struct.pack("<h", max(-32768, min(32767, int(x * 32767.0)))) for x in xs))


def seam_error(xs: list[float]) -> float:
    """How loud the loop point is compared with the material around it.
    We measure the sample-to-sample jump at the wrap and compare it with the
    biggest jumps found elsewhere: at or below 1.0 the seam cannot be heard."""
    jumps = [abs(xs[i + 1] - xs[i]) for i in range(len(xs) - 1)]
    jumps_sorted = sorted(jumps)
    typical = jumps_sorted[int(len(jumps_sorted) * 0.999)]
    wrap = abs(xs[0] - xs[-1])
    return wrap / (typical or 1e-9)


def db(x: float) -> float:
    return 20.0 * math.log10(x) if x > 0 else -120.0


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="games/daggerfall_n/assets/audio/world")
    ap.add_argument("--keep-wav", action="store_true")
    args = ap.parse_args()
    root = Path(__file__).resolve().parents[4]
    out = root / args.out

    fade_n = int(RATE * FADE_SECONDS)
    n = int(RATE * LOOP_SECONDS) + fade_n      # cross-fade eats the tail, so add it back
    families = [
        ("leaves_broad", [(f"leaves_broad_{s}", (lambda r, s=s: leaves(r, n, False, s)),
                           STEP_GAIN[s - 1]) for s in (1, 2, 3)]),
        ("leaves_conifer", [(f"leaves_conifer_{s}", (lambda r, s=s: leaves(r, n, True, s)),
                             STEP_GAIN[s - 1]) for s in (1, 2, 3)]),
        ("water", [("stream_small", (lambda r: water(r, n, False)), 0.85),
                   ("river_wide", (lambda r: water(r, n, True)), 1.00)]),
    ]

    print(f"{'файл':22} {'секунд':>7} {'пик дБ':>8} {'средн. дБ':>10} {'стык':>7} {'КБ':>7}")
    for fam, members in families:
        built = []
        for name, make, g in members:
            rng = random.Random(SEED + sum(ord(c) for c in name))
            built.append((name, [v * g for v in close_loop(make(rng), fade_n)]))
        # одно усиление на семейство: иначе нормализация сравняет ступени ветра
        loudest = max(max(abs(v) for v in xs) for _, xs in built) or 1.0
        k = PEAK / loudest
        for name, raw in built:
            xs = [v * k for v in raw]
            wav = out / f"{name}.wav"
            write_wav(wav, xs)
            ogg = out / f"{name}.ogg"
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(wav),
                            "-c:a", "libvorbis", "-q:a", "5", "-ar", str(RATE),
                            "-ac", "1", str(ogg)], check=True)
            if not args.keep_wav:
                wav.unlink()
            rms = math.sqrt(sum(x * x for x in xs) / len(xs))
            print(f"{name + '.ogg':22} {len(xs)/RATE:7.2f} {db(max(abs(x) for x in xs)):8.1f} "
                  f"{db(rms):10.1f} {seam_error(xs):7.2f} {ogg.stat().st_size/1024:7.0f}")
    print("\nстык: отношение скачка на месте склейки к самым большим скачкам внутри "
          "материала. Единица и ниже — склейку не слышно.")
    print(f"каталог: {out}")


if __name__ == "__main__":
    main()
