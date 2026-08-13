#!/usr/bin/env python3.11
"""
Created: 13:08:2026 - 20:12:00
Last updated: 13:08:2026 - 20:12:00
Module: tools
File: tools/measure_occlusion_real.py

Responsibility:
- GROUND_OCCLUSION_COUNT, run on ANY height field -- including real lidar ground.
  A line-for-line port of `hidden_pockets_of` from
  tests/core/GroundReliefTests.cpp: same 0.5 m march, same running-envelope rule,
  same 1.5 m minimum run, same 5-60 m band, same 1.7 m eye. The only thing that
  changes is the field it is handed.

Key items:
- pockets_along(), flattest_standpoints(), score().

Dependencies:
- python3.11 with numpy + Pillow. Reads the lidar cache written by
  tools/measure_terrain_stats.py and the .f32 dumps from tools/dump_heightmap.cpp.

AI Agents Notice (must follow):
- THIS TOOL EXISTS BECAUSE OF WHAT IT FOUND, and the finding must travel with it:
  NOT ONE of the seven real landscapes measured reaches
  GROUND_OCCLUSION_COUNT_MIN = 3 at percentile 5 -- all seven read 0, including a
  458 m Colorado mountain flank -- while our shipped ground reads median 3 and
  hides something in 99.7 % of columns. See docs/design/TERRAIN_REFERENCE.md.
  Before that threshold is cited again, read section 1 of that file.
- Keep this in step with the C++ by hand: it is a PORT, and a port that drifts
  from its original is worse than no port at all.
"""
"""
UPD:
- 13:08:2026 - 20:12:00: Created -- the instrument behind
  docs/design/TERRAIN_REFERENCE.md. Seven square kilometres of real 1 m lidar,
  put on our own 2 m grid and read with our own probes.
"""

import glob
import json
import math
import os
import sys

import numpy as np
from PIL import Image

import tempfile
CACHE = os.environ.get("DFN_DEM_CACHE",
                       os.path.join(tempfile.gettempdir(), "dfn_demcache"))
OURS = os.environ.get("DFN_HEIGHT_DUMPS",
                      os.path.join(tempfile.gettempdir(), "dfn_height_dumps"))
EYE = 1.7
STEP = 0.5
NEAR, FAR = 5.0, 60.0


def load_f32(path):
    a = np.fromfile(path, dtype=np.float32).astype(np.float64)
    n = int(round(math.sqrt(a.size)))
    return a.reshape(n, n)


def resample(h, res_in, res_out):
    f = int(round(res_out / res_in))
    if f <= 1:
        return h, res_in
    n = (h.shape[0] // f) * f
    return h[:n, :n].reshape(n // f, f, n // f, f).mean(axis=(1, 3)), res_in * f


class Field:
    """Bilinear sampler over a grid, in metres."""

    def __init__(self, h, res):
        self.h = h
        self.res = res
        self.n = h.shape[0]

    def at(self, x, z):
        u, v = x / self.res, z / self.res
        i, j = np.floor(u).astype(int), np.floor(v).astype(int)
        i = np.clip(i, 0, self.n - 2)
        j = np.clip(j, 0, self.n - 2)
        fu, fv = u - i, v - j
        h = self.h
        return ((h[j, i] * (1 - fu) + h[j, i + 1] * fu) * (1 - fv)
                + (h[j + 1, i] * (1 - fu) + h[j + 1, i + 1] * fu) * fv)


def pockets_along(field, ex, ez, eye_y, dx, dz):
    ts = np.arange(1.0, FAR + 1e-6, STEP)
    hs = field.at(ex + dx * ts, ez + dz * ts)
    ang = (hs - eye_y) / ts
    env = -1e9
    hidden = False
    run = 0.0
    n = 0
    starts = []
    start = 0.0
    for t, a in zip(ts, ang):
        if a >= env:
            env = a
            if hidden and run >= 1.5 and t >= NEAR:
                n += 1
                starts.append(start)
            hidden = False
            run = 0.0
        else:
            if not hidden:
                start = t
            hidden = True
            run += STEP
    if hidden and run >= 1.5:
        n += 1
        starts.append(start)
    return n, starts


def flattest_standpoints(h, res, k, margin_m):
    """The flattest legal ground in the patch: lowest local trend over a 64 m
    disc, our own `flattest_legal_standpoints` in spirit — the complaint is
    about flat places, so the contract is read where the land is flattest."""
    m = int(margin_m / res)
    step = max(1, int(24.0 / res))
    cand = []
    n = h.shape[0]
    r = int(20.0 / res)
    for j in range(m, n - m, step):
        for i in range(m, n - m, step):
            w = h[j - r:j + r + 1, i - r:i + r + 1]
            gx = (w[:, -1] - w[:, 0]).mean() / (2 * r * res)
            gz = (w[-1, :] - w[0, :]).mean() / (2 * r * res)
            cand.append((math.hypot(gx, gz), i * res, j * res))
    cand.sort()
    return [(x, z) for _, x, z in cand[:k]]


def score(h, res, name, standpoints=24, azimuths=24):
    f = Field(h, res)
    margin = FAR + 8.0
    counts = []
    bins = [0] * 6
    sps = flattest_standpoints(h, res, standpoints, margin)
    for (sx, sz) in sps:
        eye_y = float(f.at(np.array([sx]), np.array([sz]))[0]) + EYE
        for c in range(azimuths):
            a = c * 2 * math.pi / azimuths
            n, starts = pockets_along(f, sx, sz, eye_y, math.cos(a), math.sin(a))
            counts.append(n)
            for s in starts:
                bins[min(5, max(0, int((s - 5.0) / 10.0)))] += 1
    counts.sort()
    # slope stats over the same patch at the 10 m arm
    k = max(1, int(round(10.0 / res)))
    gx = (h[:, 2 * k:] - h[:, :-2 * k]) / (2.0 * k * res)
    gz = (h[2 * k:, :] - h[:-2 * k, :]) / (2.0 * k * res)
    ny, nx = min(gx.shape[0], gz.shape[0]), min(gx.shape[1], gz.shape[1])
    s = np.degrees(np.arctan(np.hypot(gx[:ny, :nx], gz[:ny, :nx])))
    return {
        "name": name, "res_m": res, "columns": len(counts),
        "min": counts[0], "p5": counts[len(counts) // 20],
        "median": counts[len(counts) // 2], "p95": counts[len(counts) * 19 // 20],
        "max": counts[-1],
        "cols_with_any_pct": 100.0 * sum(1 for c in counts if c > 0) / len(counts),
        "bins_5_15_25_35_45_55": bins,
        "slope_median_deg": float(np.median(s)),
        "slope_p90_deg": float(np.percentile(s, 90)),
        "slope_p99_deg": float(np.percentile(s, 99)),
        "p90_over_p50": float(np.percentile(s, 90) / max(np.median(s), 1e-6)),
    }


HDR = (f"{'patch':26s} {'res':>4} {'min':>4} {'p5':>4} {'med':>4} {'p95':>4} {'max':>4} "
       f"{'any%':>6} {'sl50':>6} {'sl90':>6} {'p90/50':>7}  pockets by 10 m band")


def line(r):
    return (f"{r['name']:26s} {r['res_m']:4.1f} {r['min']:4d} {r['p5']:4d} {r['median']:4d} "
            f"{r['p95']:4d} {r['max']:4d} {r['cols_with_any_pct']:6.1f} "
            f"{r['slope_median_deg']:6.2f} {r['slope_p90_deg']:6.2f} {r['p90_over_p50']:7.2f}  "
            + "/".join(str(b) for b in r["bins_5_15_25_35_45_55"]))


def main():
    grid = float(sys.argv[1]) if len(sys.argv) > 1 else 2.0
    out = {}
    print(HDR)
    print("-" * 130)
    idx = os.path.join(CACHE, "3dep_index.json")
    if os.path.exists(idx):
        for name, meta in json.load(open(idx)).items():
            p = os.path.join(CACHE,
                             f"3dep_{name}_{int(meta['side_m'])}m_{meta['res_m']:g}.tif")
            if not os.path.exists(p):
                continue
            h = np.array(Image.open(p)).astype(np.float64)
            h, r = resample(h, meta["res_m"], grid)
            res = score(h, r, "REAL " + name)
            out[res["name"]] = res
            print(line(res), flush=True)
    print()
    for p in sorted(glob.glob(os.path.join(OURS, "*.f32"))):
        nm = os.path.basename(p)[:-4]
        r0 = 2.0 if nm.startswith("plain") else 4.0
        h = load_f32(p)
        h, r = resample(h, r0, max(grid, r0))
        res = score(h, r, "OURS " + nm)
        out[res["name"]] = res
        print(line(res), flush=True)
    json.dump(out, open(os.path.join(CACHE, "occlusion.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
