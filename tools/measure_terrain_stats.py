#!/usr/bin/env python3.11
"""
Created: 13:08:2026 - 20:10:00
Last updated: 13:08:2026 - 20:10:00
Module: tools
File: tools/measure_terrain_stats.py

Responsibility:
- The instrument behind docs/design/TERRAIN_REFERENCE.md: download REAL 1 m
  lidar ground (USGS 3DEP ImageServer, public, no key), put it on OUR grid, and
  read the same statistics off real land and off our own dumped heightmaps.

Key items:
- fetch(): one square kilometre of real bare-earth lidar, cached on disk.
- analyse(): slope distribution, axial direction spread, 2-D power spectrum,
  band power shares, structure function.

Dependencies:
- python3.11 with numpy + Pillow (NOT the system python3, which has neither).
- Our side needs a heightmap dump: see tools/dump_heightmap.cpp.

AI Agents Notice (must follow):
- EVERY statistic here is reported WITH THE ARM IT WAS READ AT, and real land and
  ours are compared only after both are resampled to the same 2 m grid
  (Rule 50: an instrument whose arm is shorter than its subject aliases it).
- Never upsample a patch to match a finer one: an upsampled patch invents the
  band you are about to measure. resample() refuses to.
"""
"""
UPD:
- 13:08:2026 - 20:10:00: Created -- the instrument behind
  docs/design/TERRAIN_REFERENCE.md. Seven square kilometres of real 1 m lidar,
  put on our own 2 m grid and read with our own probes.
"""


# --- part 1: fetching real ground ------------------------------------------
import json
import math
import os
import sys
import urllib.request

import numpy as np
from PIL import Image

# Nothing this tool downloads or dumps belongs in the repo. Both directories are
# overridable so a run can be pointed at a scratchpad.
import tempfile
CACHE = os.environ.get("DFN_DEM_CACHE",
                       os.path.join(tempfile.gettempdir(), "dfn_demcache"))
OURS = os.environ.get("DFN_HEIGHT_DUMPS",
                      os.path.join(tempfile.gettempdir(), "dfn_height_dumps"))
os.makedirs(CACHE, exist_ok=True)
os.makedirs(OURS, exist_ok=True)
R = 6378137.0


def merc(lat, lon):
    return R * math.radians(lon), R * math.log(math.tan(math.pi / 4 + math.radians(lat) / 2))


def fetch(name, lat, lon, side_m=1024, res_m=1.0):
    path = os.path.join(CACHE, f"3dep_{name}_{int(side_m)}m_{res_m:g}.tif")
    if not os.path.exists(path):
        cx, cy = merc(lat, lon)
        # Web Mercator distances are stretched by 1/cos(lat); ask for a bbox
        # whose GROUND side is side_m.
        k = 1.0 / math.cos(math.radians(lat))
        half = side_m * k / 2.0
        n = int(round(side_m / res_m))
        url = ("https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/"
               "ImageServer/exportImage?"
               f"bbox={cx-half},{cy-half},{cx+half},{cy+half}&bboxSR=3857&"
               f"size={n},{n}&imageSR=3857&format=tiff&pixelType=F32&"
               "interpolation=RSP_BilinearInterpolation&f=image")
        req = urllib.request.Request(url, headers={"User-Agent": "terrain-stats/1.0"})
        with urllib.request.urlopen(req, timeout=180) as r:
            data = r.read()
        with open(path, "wb") as f:
            f.write(data)
    a = np.array(Image.open(path)).astype(np.float64)
    return a, res_m


SITES = {
    # name: (lat, lon, what it is)
    "iowa_farmland":     (41.55, -93.90, "rolling Iowa cropland — the flat open field, worked"),
    "vermont_hills":     (43.92, -72.62, "Green Mountain foothills — dissected humid temperate"),
    "kentucky_dissect":  (37.42, -83.22, "Cumberland Plateau — the most gully-dissected land in the study"),
    "wisconsin_driftless": (43.55, -90.75, "Driftless Area — coulees, benches and steep hollows"),
    "colorado_flank":    (39.65, -105.45, "Front Range flank — steep, above the tree line in places"),
    "willamette_valley": (44.60, -123.20, "Willamette valley floor — flat bottom under hills"),
    "shenandoah_ridge":  (38.65, -78.40, "Blue Ridge — long parallel ridges, the REAL corduroy control"),
}


def fetch_all(side=1024.0, res=1.0):
    """Download every site in SITES and write the index the study reads."""
    ok = {}
    for name, (lat, lon, what) in SITES.items():
        try:
            a, r = fetch(name, lat, lon, side, res)
            bad = int(np.sum(~np.isfinite(a)) + np.sum(a < -1000))
            print(f"{name:22s} {a.shape} relief {np.ptp(a):7.1f} m  voids {bad}", flush=True)
            ok[name] = {"lat": lat, "lon": lon, "what": what, "side_m": side, "res_m": r,
                        "relief_m": float(np.ptp(a)), "voids": bad}
        except Exception as e:  # noqa
            print(f"{name}: FAILED {e}", file=sys.stderr, flush=True)
    json.dump(ok, open(os.path.join(CACHE, "3dep_index.json"), "w"), indent=1)


# --- part 2: the instruments -----------------------------------------------
import glob
import json
import math
import os
import sys

import numpy as np
from PIL import Image

GRID = 2.0  # the common ruler, metres


def load_f32(path):
    a = np.fromfile(path, dtype=np.float32).astype(np.float64)
    n = int(round(math.sqrt(a.size)))
    return a.reshape(n, n)


def resample(h, res_in, res_out):
    """Box-average down to res_out (never up: an upsampled patch invents band)."""
    if abs(res_in - res_out) < 1e-6:
        return h, res_in
    f = int(round(res_out / res_in))
    if f < 1:
        return h, res_in  # refuse to upsample
    n = (h.shape[0] // f) * f
    return h[:n, :n].reshape(n // f, f, n // f, f).mean(axis=(1, 3)), res_in * f


def detrend_plane(h):
    ny, nx = h.shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    A = np.column_stack([xx.ravel(), yy.ravel(), np.ones(xx.size)])
    c, *_ = np.linalg.lstsq(A, h.ravel(), rcond=None)
    return h - (A @ c).reshape(ny, nx)


def grad(h, res, arm_m):
    k = max(1, int(round(arm_m / res)))
    arm = k * res
    gx = (h[:, 2 * k:] - h[:, :-2 * k]) / (2.0 * arm)
    gz = (h[2 * k:, :] - h[:-2 * k, :]) / (2.0 * arm)
    ny = min(gx.shape[0], gz.shape[0])
    nx = min(gx.shape[1], gz.shape[1])
    return gx[:ny, :nx], gz[:ny, :nx], arm


def axial_spread(gx, gz, res, win_m):
    mag = np.hypot(gx, gz)
    pos = mag[mag > 0]
    floor = 0.25 * np.median(pos) if pos.size else 0.0
    a2 = 2.0 * np.arctan2(gz, gx)
    w = np.where(mag > floor, mag, 0.0)
    cx, cz = w * np.cos(a2), w * np.sin(a2)
    k = max(4, int(round(win_m / res)))
    out = []
    ny, nx = mag.shape
    for j in range(0, ny - k + 1, k):
        for i in range(0, nx - k + 1, k):
            ww = w[j:j + k, i:i + k].sum()
            if ww <= 0:
                continue
            out.append(1.0 - math.hypot(cx[j:j + k, i:i + k].sum(),
                                        cz[j:j + k, i:i + k].sum()) / ww)
    return float(np.median(out)) if out else float("nan")


def spec(h, res):
    n = min(h.shape)
    n -= n % 2
    hh = detrend_plane(h[:n, :n])
    w = np.hanning(n)
    hh = hh * np.outer(w, w)
    P = np.abs(np.fft.fftshift(np.fft.fft2(hh))) ** 2
    c = n // 2
    yy, xx = np.mgrid[0:n, 0:n]
    kx, ky = xx - c, yy - c
    kr = np.hypot(kx, ky)
    th = np.arctan2(ky, kx) % math.pi
    kmax = c - 1
    rad = np.zeros(kmax + 1)
    cnt = np.zeros(kmax + 1)
    ri = kr.astype(int)
    m = ri <= kmax
    np.add.at(rad, ri[m], P[m])
    np.add.at(cnt, ri[m], 1)
    rad = np.where(cnt > 0, rad / np.maximum(cnt, 1), np.nan)
    L = n * res
    lam = np.full(kmax + 1, np.inf)
    lam[1:] = L / np.arange(1, kmax + 1)
    return P, kr, th, rad, lam, kmax, L


def band_k(lam, kmax, lo_m, hi_m):
    k = np.arange(1, kmax + 1)
    return k[(lam[1:kmax + 1] >= lo_m) & (lam[1:kmax + 1] <= hi_m)]


def analyse(h, res_in, name, what=""):
    h, res = resample(h, res_in, GRID)
    P, kr, th, rad, lam, kmax, L = spec(h, res)
    out = {"name": name, "what": what, "res_m": res, "n": int(h.shape[0]),
           "extent_m": round(h.shape[0] * res, 1), "relief_m": float(np.ptp(h))}

    # slope + direction at the arm that matters for a 5-60 m sight line
    gx, gz, arm = grad(h, res, 10.0)
    s = np.degrees(np.arctan(np.hypot(gx, gz)))
    out["slope_arm_m"] = arm
    out["slope_median_deg"] = float(np.median(s))
    out["slope_p90_deg"] = float(np.percentile(s, 90))
    out["slope_p99_deg"] = float(np.percentile(s, 99))
    for wm in (24, 48, 96, 192):
        out[f"axial_spread_{wm}m"] = axial_spread(gx, gz, res, float(wm))

    # power law over the band every patch resolves
    kk = band_k(lam, kmax, 40.0, 400.0)
    good = kk[np.isfinite(rad[kk]) & (rad[kk] > 0)]
    p = np.polyfit(np.log(good), np.log(rad[good]), 1)
    out["beta_40_400m"] = float(-p[0])

    def excess(lo, hi):
        k = band_k(lam, kmax, lo, hi)
        k = k[np.isfinite(rad[k]) & (rad[k] > 0)]
        if k.size < 2:
            return float("nan")
        pred = np.exp(np.polyval(p, np.log(k)))
        return float(np.mean(rad[k] / pred))

    out["excess_10_20m"] = excess(10.0, 20.0)
    out["excess_20_40m"] = excess(20.0, 40.0)
    out["excess_5_10m"] = excess(5.0, 10.0)

    def dircon(lo, hi):
        sel = (kr > 0)
        li = np.clip(kr.astype(int), 0, kmax)
        sel &= (lam[li] >= lo) & (lam[li] <= hi)
        if sel.sum() < 20:
            return float("nan")
        pw, tt = P[sel], th[sel]
        cs = float((pw * np.cos(2 * tt)).sum())
        sn = float((pw * np.sin(2 * tt)).sum())
        return math.hypot(cs, sn) / pw.sum()

    out["D_10_20m"] = dircon(10.0, 20.0)
    out["D_20_60m"] = dircon(20.0, 60.0)
    out["D_60_200m"] = dircon(60.0, 200.0)

    out["radial_profile"] = [{"lambda_m": round(float(lam[k]), 2),
                              "power": float(rad[k])}
                             for k in range(1, min(kmax, 200)) if np.isfinite(rad[k])]
    return out


HDR = (f"{'patch':26s} {'ext':>5} {'relief':>7} {'sl50':>5} {'sl90':>6} "
       f"{'ax24':>5} {'ax48':>5} {'ax96':>5} {'ax192':>5} {'beta':>5} "
       f"{'E5-10':>6} {'E10-20':>6} {'E20-40':>6} {'D10-20':>6} {'D20-60':>6}")


def line(a):
    return (f"{a['name']:26s} {a['extent_m']:5.0f} {a['relief_m']:7.1f} "
            f"{a['slope_median_deg']:5.2f} {a['slope_p90_deg']:6.2f} "
            f"{a['axial_spread_24m']:5.3f} {a['axial_spread_48m']:5.3f} "
            f"{a['axial_spread_96m']:5.3f} {a['axial_spread_192m']:5.3f} "
            f"{a['beta_40_400m']:5.2f} {a['excess_5_10m']:6.2f} "
            f"{a['excess_10_20m']:6.2f} {a['excess_20_40m']:6.2f} "
            f"{a['D_10_20m']:6.3f} {a['D_20_60m']:6.3f}")


def main():
    fetch_all()
    print()
    res = {}
    print(HDR)
    print("-" * len(HDR))
    idx_path = os.path.join(CACHE, "3dep_index.json")
    idx = json.load(open(idx_path)) if os.path.exists(idx_path) else {}
    for name, meta in idx.items():
        f = os.path.join(CACHE,
                         f"3dep_{name}_{int(meta['side_m'])}m_{meta['res_m']:g}.tif")
        if not os.path.exists(f):
            continue
        a = analyse(np.array(Image.open(f)).astype(np.float64), meta["res_m"],
                    "REAL " + name, meta["what"])
        res[a["name"]] = a
        print(line(a), flush=True)
    print()
    for path in sorted(glob.glob(os.path.join(OURS, "*.f32"))):
        nm = os.path.basename(path)[:-4]
        r = 2.0 if nm.startswith("plain") else 4.0
        if r != GRID:
            continue
        a = analyse(load_f32(path), r, "OURS " + nm)
        res[a["name"]] = a
        print(line(a), flush=True)
    json.dump(res, open(os.path.join(CACHE, "study.json"), "w"), indent=1)
    print("\nwrote study.json")


if __name__ == "__main__":
    main()
