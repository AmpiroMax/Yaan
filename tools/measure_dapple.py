#!/usr/bin/env python3
"""
Created: 12:08:2026 - 23:01:25
Last updated: 12:08:2026 - 23:01:25
Module: tools
File: tools/measure_dapple.py

Responsibility:
- Measure R6's SECOND HALF (REFERENCE_FRAMES.md): "Frame 03's forest floor is
  entirely made of soft dappled shadow." The quantity is LOCAL contrast at the
  dapple's own scale — how much light and shade INTERLEAVE across a ground
  surface — with any smooth trend removed, because a floor that simply gets
  darker with distance is the thing this must not score.

Key items:
- dapple(): local light/dark ratio. Blocks of `block` px, then within each
  window of `win` x `win` blocks the brightest quarter over the darkest
  quarter. Averaged over windows.
- Scale-free: `block` and `win` are given, and the recipe records them, but the
  default derives `block` from the BOX WIDTH so a 1200 px reference and our
  640 px frame are read at the same fraction of the surface.
- selftest(): the Rule 48 arm — a pure gradient must score ~1.00 and a
  checkerboard must score high, on synthetic pixels, no frame needed.

Dependencies:
- Uses: python3 stdlib only + tools/archive_frame.read_png (Rule 24: no Pillow,
  no numpy).
- Used by: render's R6b work.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- **THIS IS A SECOND INSTRUMENT AND NOT A SECOND MODE OF
  tools/measure_light_split.py** (Rule 41). That file measures the HUE of the
  light; this measures the SPATIAL TEXTURE of the shadow. An instrument aimed
  at a colour does not accept a claim about a pattern.
- **WHY THE TREND HAS TO GO, AND IT IS THE WHOLE FILE.** The obvious quantity —
  luma range over a ground box — was tried and is wrong. Our own forest floor
  scores 1.81x on it, close to reference 03's 2.28x, while having no dapple at
  all: the range is a SMOOTH gradient from near ground to the treeline, aerial
  perspective plus slope shading. A criterion a gradient can satisfy is not
  measuring dapple. Restricting the comparison to a small WINDOW removes it:
  a smooth trend is nearly constant inside one window and divides out, while
  interleaved light and shade do not.
- LOCATED BY LUMA, WHICH IS NOT THE PROPERTY UNDER TEST (Rule 47) — the
  property is the SPATIAL ARRANGEMENT of luma, and within a window every block
  is used, brightest and darkest alike. Nothing is classified as "shadow".
- **BLOCK SIZE IS PART OF THE CLAIM.** Below the material's texture scale this
  measures the texture; far above the dapple's scale it averages the dapple
  away. Both are printed. A reading at one block size is not a result — sweep
  it, which is what `sweep` mode is for, and quote the curve.
- THE CONTROL THIS FILE NEEDS FROM ITS CALLER (Rule 48): the zero-dose arm is
  THE SAME VANTAGE WITH THE SUN SHADOW OFF. Ground texture, slope shading and
  material dither all survive that arm, so the shipped number alone means
  nothing; the dapple is the DIFFERENCE. Do not quote an absolute.

UPD:
- 12:08:2026 - 23:01:25: Created for R6b, after the plain luma-range version scored our
  shadowless forest floor at 1.81x against reference 03's 2.28x — a 79 % "pass"
  for a picture with no dapple in it whatsoever, because the range was a
  distance gradient. Same family as the defect recorded in
  tools/measure_light_split.py: a criterion that its own null case satisfies.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from archive_frame import read_png  # noqa: E402

# The quantiser's own weights (fs_upscale.sc / DFN_LUMA_WEIGHTS).
LUMA_W = (0.30, 0.59, 0.11)


def luma(r, g, b):
    return LUMA_W[0] * r + LUMA_W[1] * g + LUMA_W[2] * b


def block_luma(w, h, ch, px, box, block):
    """Grid of block-mean lumas. Non-overlapping: overlapping blocks would
    correlate neighbours inside a window and damp the very contrast under
    test."""
    x0, y0, x1, y1 = box
    x0, y0 = max(0, x0), max(0, y0)
    x1, y1 = min(w, x1), min(h, y1)
    rows = []
    for by in range(y0, y1 - block + 1, block):
        row = []
        for bx in range(x0, x1 - block + 1, block):
            s = 0.0
            for y in range(by, by + block):
                base = y * w * ch
                for x in range(bx, bx + block):
                    i = base + x * ch
                    s += luma(px[i], px[i + 1], px[i + 2])
            row.append(s / (block * block))
        if row:
            rows.append(row)
    return rows


def dapple(grid, win=4):
    """Local light/dark ratio, averaged over windows of win x win blocks.

    Inside one window the brightest quarter of the blocks over the darkest
    quarter. A smooth trend is nearly constant across a window and cancels; an
    interleaved pattern does not. Windows step by half so an edge falling on a
    window boundary does not decide the answer.
    """
    if not grid or len(grid) < win or len(grid[0]) < win:
        return None, 0
    step = max(1, win // 2)
    ratios = []
    for r0 in range(0, len(grid) - win + 1, step):
        for c0 in range(0, len(grid[0]) - win + 1, step):
            vals = sorted(grid[r][c]
                          for r in range(r0, r0 + win)
                          for c in range(c0, c0 + win))
            k = max(1, len(vals) // 4)
            lo = sum(vals[:k]) / k
            hi = sum(vals[-k:]) / k
            ratios.append(hi / max(lo, 0.01))
    if not ratios:
        return None, 0
    return sum(ratios) / len(ratios), len(ratios)


def measure(path, box, block, win=4):
    w, h, ch, px = read_png(path)
    grid = block_luma(w, h, ch, px, box, block)
    d, n = dapple(grid, win)
    return d, n, (len(grid), len(grid[0]) if grid else 0)


def run(path, box, blocks, win=4, label=""):
    print(f"{Path(path).name}  DAPPLE  box {box[0]},{box[1]},{box[2]},{box[3]}"
          f"  window {win}x{win} blocks" + (f"  {label}" if label else ""))
    print("   block px   blocks     windows   LOCAL LIGHT/DARK")
    for block in blocks:
        d, n, shape = measure(path, box, block, win)
        if d is None:
            print(f"   {block:8d}   {shape[0]}x{shape[1]:<6}  too few for a"
                  f" {win}x{win} window")
            continue
        print(f"   {block:8d}   {shape[0]}x{shape[1]:<6} {n:8d}   {d:8.3f}x")


def selftest():
    """Rule 48, on synthetic pixels: the null case must NOT pass.

    Gradient arm: a floor that only gets darker with distance. Must land at
    ~1.00 — this is the arm the naive luma-range version failed.
    Dapple arm: light and shade interleaved at the block scale. Must be high.
    """
    def grid_from(fn, rows=32, cols=32):
        return [[fn(r, c) for c in range(cols)] for r in range(rows)]

    # A strong smooth gradient: 4x darker at one end than the other.
    grad = grid_from(lambda r, c: 40.0 + 120.0 * (r / 31.0))
    # Dapple: same mean, alternating light and shade in 2-block patches.
    dap = grid_from(lambda r, c: 40.0 if ((r // 2) + (c // 2)) % 2 else 160.0)
    # Both at once, so the instrument is shown to read the dapple THROUGH a
    # gradient — which is the real frame.
    both = grid_from(lambda r, c: (40.0 + 120.0 * (r / 31.0))
                     * (0.4 if ((r // 2) + (c // 2)) % 2 else 1.0))
    g, _ = dapple(grad)
    d, _ = dapple(dap)
    b, _ = dapple(both)
    print(f"  GRADIENT only (4x end to end, no dapple):  {g:6.3f}x")
    print(f"  DAPPLE only   (4x, interleaved):           {d:6.3f}x")
    print(f"  BOTH                                        {b:6.3f}x")
    ok = g < 1.25 and d > 3.0
    print(f"  null control: {'PASS' if ok else 'FAIL'}"
          "  (a pure gradient must not score as dapple — Rule 48)")
    return 0 if ok else 1


def main(argv):
    if len(argv) > 1 and argv[1] == "selftest":
        raise SystemExit(selftest())
    if len(argv) < 3:
        raise SystemExit(
            "usage: measure_dapple.py <frame.png> <x0,y0,x1,y1> [block[,block...]]"
            " [win] [label]\n"
            "       measure_dapple.py selftest\n"
            "  the box, the block sizes and the window are the recipe — record them")
    box = tuple(int(t) for t in argv[2].split(","))
    if len(argv) > 3:
        blocks = [int(t) for t in argv[3].split(",")]
    else:
        # Scale-free default: fractions of the box width, so a 1200 px
        # reference and our 640 px frame are read at the same fraction of the
        # surface in front of the camera.
        bw = box[2] - box[0]
        blocks = [max(2, bw // d) for d in (80, 40, 27, 16)]
    win = int(argv[4]) if len(argv) > 4 else 4
    run(argv[1], box, blocks, win, argv[5] if len(argv) > 5 else "")


if __name__ == "__main__":
    main(sys.argv)
