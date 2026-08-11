#!/usr/bin/env python3
"""
Created: 12:08:2026 - 00:29:55
Last updated: 12:08:2026 - 00:29:55
Module: tools
File: tools/measure_speckle.py

Responsibility:
- THE SHIMMER, as a number: the fraction of screen pixels that FLIP by more than
  a luma threshold across ONE frame of running. It is the user's «при беге
  трясет» in the only form a pair of frames can carry, and it is the recipe
  already frozen into NUMBERS' FLORA_SPECKLE_NEAR_CANOPY_PCT — written down as
  a tool because until now it existed only as a prose recipe, and a number
  nobody can re-take is a number nobody can check.

Key items:
- speckle(): flipped-pixel fraction and the worst flip, over a box or the frame.

Dependencies:
- Uses: python3 stdlib only + tools/archive_frame.read_png (Rule 24).
- Used by: any zone adding small or high-frequency detail. Render used it on the
  R5 ground change.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE CONTROL IS NOT OPTIONAL AND IT IS CHEAP: shoot the SAME arm with
  DFN_FLORA_STEP unset. The eye has not moved, so the answer must be 0.000 %.
  Anything else is streaming or animation jitter contaminating the run, and
  every number off that run is worth nothing until it comes back zero.
- DFN_WIND_FREEZE=120 OR THE MEASUREMENT IS OF THE WIND. Measured by the
  predecessor: with the eye still, the rustle alone gives 0.008 % under crowns
  against 0.864 % from eye motion. One comparison at live wind cannot separate
  them, which is what the freeze is for.
- AIM IT AT YOUR OWN SUBJECT (Rule 41). The canopy rows in NUMBERS were taken
  under crowns and at a treeline. A claim about the GROUND is a different
  subject and needs its own standpoint and its own baseline — the canopy
  numbers are not a threshold anything else may be read against.

UPD:
- 12:08:2026 - 00:29:55: Created for R5's cost side. The R5 ground change raises
  the spatial frequency of the ground (a second grass fetch at an
  incommensurate scale), and that is exactly the class that shimmers, so it is
  measured rather than argued.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from archive_frame import read_png  # noqa: E402

LUMA_W = (0.30, 0.59, 0.11)
# The frozen threshold of the NUMBERS rows, so a ground number and a canopy
# number are at least the same KIND of quantity even though they are not
# comparable across standpoints.
DEFAULT_THRESHOLD = 64.0


def luma(r, g, b):
    return LUMA_W[0] * r + LUMA_W[1] * g + LUMA_W[2] * b


def speckle(a_path, b_path, box=None, threshold=DEFAULT_THRESHOLD):
    wa, ha, ca, pa = read_png(a_path)
    wb, hb, cb, pb = read_png(b_path)
    if (wa, ha) != (wb, hb):
        raise SystemExit(f"geometry mismatch {wa}x{ha} vs {wb}x{hb}")
    x0, y0, x1, y1 = box if box else (0, 0, wa, ha)
    x0, y0 = max(0, x0), max(0, y0)
    x1, y1 = min(wa, x1), min(ha, y1)
    flipped = 0
    total = 0
    worst = 0.0
    for y in range(y0, y1):
        for x in range(x0, x1):
            ia = (y * wa + x) * ca
            ib = (y * wb + x) * cb
            d = abs(luma(pa[ia], pa[ia + 1], pa[ia + 2])
                    - luma(pb[ib], pb[ib + 1], pb[ib + 2]))
            if d > worst:
                worst = d
            if d > threshold:
                flipped += 1
            total += 1
    return {"flipped": flipped, "total": total, "worst": worst,
            "pct": 100.0 * flipped / max(total, 1)}


def main(argv):
    if len(argv) < 3:
        raise SystemExit("usage: measure_speckle.py <still.png> <stepped.png> "
                         "[x0,y0,x1,y1] [threshold]")
    box = None
    if len(argv) > 3 and argv[3] != "-":
        box = tuple(int(t) for t in argv[3].split(","))
    thr = float(argv[4]) if len(argv) > 4 else DEFAULT_THRESHOLD
    m = speckle(argv[1], argv[2], box, thr)
    print(f"{Path(argv[1]).name} -> {Path(argv[2]).name}"
          + (f"  box {box}" if box else "  whole frame"))
    print(f"  SPECKLE {m['pct']:.3f} %  ({m['flipped']}/{m['total']} px flip "
          f"more than {thr:.0f} luma)   worst flip {m['worst']:.1f}")


if __name__ == "__main__":
    main(sys.argv)
