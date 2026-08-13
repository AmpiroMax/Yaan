#!/usr/bin/env python3.11
"""
Created: 13:08:2026 - 20:55:00
Last updated: 13:08:2026 - 20:55:00
Module: tools
File: tools/measure_drainage.py

Responsibility:
- THE ONE MEASURE A JITTERED COMB CANNOT FAKE: Horton's bifurcation ratio R_b,
  plus drainage density and the slope-area relation, computed from a real
  drainage network extracted off a height field.

Key items:
- fill_depressions(): priority-flood, so every cell has somewhere to send water.
- d8(): flow directions and drainage-area accumulation.
- strahler(): stream orders over the extracted channel network.
- report(): R_b, drainage density, channel count by order.

Dependencies:
- python3.11 with numpy + Pillow. Reads the same two sources as
  tools/measure_terrain_stats.py: the 3DEP lidar cache and our .f32 dumps.

AI Agents Notice (must follow):
- WHY THIS EXISTS, from docs/design/TERRAIN_REFERENCE.md: every other statistic
  we have (spectrum, spacing CV, direction spread) can be passed by scattering
  the POSITIONS of a comb of parallel furrows. R_b cannot: it asks whether the
  channels MEET. Real networks read 3.0-5.0; a comb reads ~1 because parallel
  furrows never join. That makes it the rejected-sample test Rule 45 asks for,
  and the rejected sample is our own ground at commit 9888746 -- reproducible
  through the pass's own doors (DFN_DRAW_DEPTH, DFN_TERRACE_STRENGTH).
- The channel threshold is a CHOICE and it moves R_b. Report it with every
  number and sweep it before believing any single value.
"""
"""
UPD:
- 13:08:2026 - 20:55:00: Created -- the rejected-sample instrument for stage 2
  of the furrow work.
"""
import glob
import heapq
import math
import os
import sys
import tempfile

import numpy as np
from PIL import Image
import json

CACHE = os.environ.get("DFN_DEM_CACHE", os.path.join(tempfile.gettempdir(), "dfn_demcache"))
OURS = os.environ.get("DFN_HEIGHT_DUMPS", os.path.join(tempfile.gettempdir(), "dfn_height_dumps"))

# D8 neighbourhood, in (dz, dx), with the step length each one costs.
NB = [(-1, 0, 1.0), (1, 0, 1.0), (0, -1, 1.0), (0, 1, 1.0),
      (-1, -1, math.sqrt(2)), (-1, 1, math.sqrt(2)),
      (1, -1, math.sqrt(2)), (1, 1, math.sqrt(2))]


def fill_depressions(h):
    """Priority-flood (Barnes et al. 2014). Water must have somewhere to go, or
    accumulation pools in pits and no network forms at all."""
    n, m = h.shape
    out = h.copy()
    seen = np.zeros((n, m), dtype=bool)
    pq = []
    for i in range(m):
        for j in (0, n - 1):
            heapq.heappush(pq, (float(h[j, i]), j, i))
            seen[j, i] = True
    for j in range(n):
        for i in (0, m - 1):
            if not seen[j, i]:
                heapq.heappush(pq, (float(h[j, i]), j, i))
                seen[j, i] = True
    eps = 1e-4
    while pq:
        e, j, i = heapq.heappop(pq)
        for dj, di, _ in NB:
            z, x = j + dj, i + di
            if z < 0 or z >= n or x < 0 or x >= m or seen[z, x]:
                continue
            seen[z, x] = True
            v = max(float(out[z, x]), e + eps)
            out[z, x] = v
            heapq.heappush(pq, (v, z, x))
    return out


def d8(h, res):
    """Steepest-descent receiver per cell, then drainage area by processing
    cells from high to low (a topological order for a descent graph)."""
    n, m = h.shape
    recv = np.full((n, m, 2), -1, dtype=np.int32)
    for j in range(n):
        for i in range(m):
            best, bj, bi = 0.0, -1, -1
            for dj, di, L in NB:
                z, x = j + dj, i + di
                if z < 0 or z >= n or x < 0 or x >= m:
                    continue
                s = (h[j, i] - h[z, x]) / (L * res)
                if s > best:
                    best, bj, bi = s, z, x
            recv[j, i] = (bj, bi)
    area = np.full((n, m), res * res, dtype=np.float64)
    order = np.argsort(h.ravel())[::-1]
    for idx in order:
        j, i = divmod(int(idx), m)
        bj, bi = recv[j, i]
        if bj >= 0:
            area[bj, bi] += area[j, i]
    return recv, area


def strahler(recv, channel):
    """Strahler order over the channel network, and the count per order."""
    n, m = channel.shape
    order = np.zeros((n, m), dtype=np.int32)
    donors = {}
    for j in range(n):
        for i in range(m):
            if not channel[j, i]:
                continue
            bj, bi = recv[j, i]
            if bj >= 0 and channel[bj, bi]:
                donors.setdefault((bj, bi), []).append((j, i))
    # process channel cells from high to low elevation is equivalent to
    # processing donors before receivers; use a simple upstream-count peel.
    indeg = {}
    cells = [(j, i) for j in range(n) for i in range(m) if channel[j, i]]
    for c in cells:
        indeg[c] = len(donors.get(c, []))
    stack = [c for c in cells if indeg[c] == 0]
    for c in stack:
        order[c] = 1
    head = 0
    while head < len(stack):
        c = stack[head]
        head += 1
        bj, bi = recv[c[0], c[1]]
        if bj < 0 or not channel[bj, bi]:
            continue
        ds = (int(bj), int(bi))
        ups = [order[u] for u in donors.get(ds, []) if order[u] > 0]
        indeg[ds] -= 1
        if indeg[ds] == 0:
            mx = max(ups) if ups else 1
            order[ds] = mx + 1 if ups.count(mx) > 1 else mx
            stack.append(ds)
    return order


def links_per_order(order, recv, channel):
    """A LINK is a maximal run of one order, which is what Horton counts --
    counting CELLS instead would just measure resolution."""
    n, m = order.shape
    counts = {}
    for j in range(n):
        for i in range(m):
            o = order[j, i]
            if o == 0:
                continue
            bj, bi = recv[j, i]
            up_same = False
            for dj, di, _ in NB:
                z, x = j + dj, i + di
                if z < 0 or z >= n or x < 0 or x >= m:
                    continue
                if channel[z, x] and order[z, x] == o and tuple(recv[z, x]) == (j, i):
                    up_same = True
                    break
            if not up_same:  # this cell starts a link of its order
                counts[int(o)] = counts.get(int(o), 0) + 1
    return counts


def report(h, res, name, thresh_m2=None):
    h = h.astype(np.float64)
    filled = fill_depressions(h)
    recv, area = d8(filled, res)
    out = {"name": name, "res_m": res, "n": int(h.shape[0])}
    rows = []
    total = h.shape[0] * h.shape[1] * res * res
    for t in (thresh_m2 or [2000.0, 5000.0, 20000.0]):
        ch = area > t
        if ch.sum() < 20:
            rows.append((t, float("nan"), 0.0, {}))
            continue
        o = strahler(recv, ch)
        cnt = links_per_order(o, recv, ch)
        ratios = []
        for k in sorted(cnt):
            if k + 1 in cnt and cnt[k + 1] > 0:
                ratios.append(cnt[k] / cnt[k + 1])
        rb = float(np.mean(ratios)) if ratios else float("nan")
        dd = ch.sum() * res / total * 1e6 / 1e3  # km of channel per km2
        rows.append((t, rb, dd, cnt))
    out["rows"] = [{"threshold_m2": t, "R_b": rb, "drainage_density_km_km2": dd,
                    "links_by_order": c} for t, rb, dd, c in rows]
    return out


def load_f32(p):
    a = np.fromfile(p, dtype=np.float32).astype(np.float64)
    n = int(round(math.sqrt(a.size)))
    return a.reshape(n, n)


def down(h, ri, ro):
    f = int(round(ro / ri))
    if f <= 1:
        return h, ri
    n = (h.shape[0] // f) * f
    return h[:n, :n].reshape(n // f, f, n // f, f).mean(axis=(1, 3)), ri * f


def main():
    grid = float(sys.argv[1]) if len(sys.argv) > 1 else 4.0
    print(f"{'patch':26s} {'thresh':>8} {'R_b':>6} {'D_d':>7}  links by Strahler order")
    print("-" * 96)
    res_all = {}
    idx = os.path.join(CACHE, "3dep_index.json")
    if os.path.exists(idx):
        for name, meta in json.load(open(idx)).items():
            p = os.path.join(CACHE, f"3dep_{name}_{int(meta['side_m'])}m_{meta['res_m']:g}.tif")
            if not os.path.exists(p):
                continue
            h, r = down(np.array(Image.open(p)).astype(np.float64), meta["res_m"], grid)
            rep = report(h, r, "REAL " + name)
            res_all[rep["name"]] = rep
            for row in rep["rows"]:
                print(f"{rep['name']:26s} {row['threshold_m2']:8.0f} {row['R_b']:6.2f} "
                      f"{row['drainage_density_km_km2']:7.1f}  {row['links_by_order']}",
                      flush=True)
    print()
    for p in sorted(glob.glob(os.path.join(OURS, "*.f32"))):
        nm = os.path.basename(p)[:-4]
        ri = 4.0 if nm.startswith("world") else 2.0
        h, r = down(load_f32(p), ri, max(grid, ri))
        rep = report(h, r, "OURS " + nm)
        res_all[rep["name"]] = rep
        for row in rep["rows"]:
            print(f"{rep['name']:26s} {row['threshold_m2']:8.0f} {row['R_b']:6.2f} "
                  f"{row['drainage_density_km_km2']:7.1f}  {row['links_by_order']}", flush=True)
    json.dump(res_all, open(os.path.join(CACHE, "drainage.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
