#!/usr/bin/env python3
"""
Module: tools
File: tools/measure_light_split.py

Responsibility:
- Measure R6's FIRST HALF (REFERENCE_FRAMES.md): "Key is warm, shade goes cool
  and blue" (frames 01, 02, 03, 14). The quantity is the HUE OF THE LIGHT THE
  KEY ADDS, taken the same way on a reference frame and on ours: on ONE
  material, the per-channel ratio between the part the key reaches and the part
  only the fill reaches. Zero means the shadow is the same colour as the
  sunlight, only darker.

Key items:
- warm_split(): 100 * ((r+g)/2 - b) / L of the key/shade RATIO. The ratio is
  what makes it albedo-free; see the notice.
- pair(): THE PRIMARY INSTRUMENT — two boxes of the same material either side
  of a CAST SHADOW EDGE, placed by geometry, given as arguments.
- scan(): the secondary — block means over one box, darkest against brightest.
  For dappled ground, where no box can hold one side of the edge.
- selftest(): the Rule 48 zero-dose arm and the tonemap bound, no frame needed.
- CALIBRATED: on our own frame `pair` returns +14.02 against the +14.01 that
  LOOKDEV_SUN_COLOR and LOOKDEV_AMBIENT_COLOR predict analytically.

Dependencies:
- Uses: python3 stdlib only + tools/archive_frame.read_png (Rule 24: no Pillow,
  no numpy).
- Used by: render's R6 acceptance (artifacts/acceptance/render-R6-warm-cool-*), and
  by the reference frames that set the target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- READ THE NOTICE IN tools/measure_aerial.py FIRST. That file caught one defect
  FIVE times in a day: **a metric may not locate its subject by the property
  under test** (Rule 47). This file's subject is the LIGHT'S HUE and it locates
  its samples by LUMA or by a hand-placed box. Those are different quantities,
  which is what makes it legal, and it is the same separation
  tools/measure_ground_colour.py insists on ("hue is the material; value is the
  light"). Nothing here decides "this pixel is in shadow" from its colour.
- **THE FIRST VERSION OF THIS FILE WAS WRONG AND IT IS WORTH THE PARAGRAPH.**
  It binned a box's pixels into luma DECILES and compared the chromaticity of
  the ends. On reference frame 14 it reported -36.5, i.e. a large cool-key
  split. The same cobbles measured across an actual cast shadow edge reported
  **+0.32**. The whole -36.5 was the material: within one cobble texture the
  dark pixels are mortar and crevice and the bright pixels are stone tops, so
  the "dark decile" and the "bright decile" are DIFFERENT ALBEDOS and the
  deltas were a texture statistic wearing the light's clothes. Sixth instance
  of Rule 47 in this zone. The cure is the one Rule 47 names: hold everything
  that is not the subject IDENTICAL between the two arms — here, the material —
  and the only way to be sure of that is a cast shadow edge, or blocks coarse
  enough to average the texture away.
- **WHY THE RATIO AND NOT THE CHROMATICITY DIFFERENCE — THIS IS THE RULE 48
  ZERO-DOSE ARM.** Take grass albedo (60,110,40) under ONE WHITE LIGHT: sun
  gives (57,105,38), fill gives (21,39,14). Raw yellow-blue chroma is 43 and
  16 — a delta of +27 "warm key, cool shade" from a scene with no colour in its
  light at all. Even chromaticity per pixel is not enough on a real frame. The
  per-channel RATIO is: it divides the albedo out exactly, so a colourless
  light split returns exactly 0. `selftest` asserts it.
- **A PAIR WITHOUT BOTH LIGHTS CANNOT FAIL** (Rule 27), so `pair` and `scan`
  REFUSE a luma range below 1.5x rather than printing a number. Two boxes that
  are both in sun differ by texture alone, and their split is noise; the first
  pair tried on our own frame came back 0.99x and would have read +1.20.
- **THE BOXES ARE THE RECIPE.** Record them next to the frame. Place them by
  looking at where the shadow edge is, never by a threshold in code.
- WHAT THIS CANNOT SEPARATE: a reference frame's tonemap and bloom desaturate
  highlights, which biases the ratio for a coloured albedo. `selftest` prints
  the size of that bias so a reading can be discounted against it; our own
  renderer has neither, which is why our arm is the calibrated one.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from archive_frame import read_png  # noqa: E402

# The quantiser's own weights (fs_upscale.sc / DFN_LUMA_WEIGHTS), the metric
# every brightness rule in this project is written in.
LUMA_W = (0.30, 0.59, 0.11)

# Below this the two boxes do not hold two different lights and no reading from
# them can fail (Rule 27). Set at 1.5x because our own shipped shadow, the
# shallowest thing either side of this comparison, is 1.79x.
MIN_LUMA_RANGE = 1.5

def luma(r, g, b):
    return LUMA_W[0] * r + LUMA_W[1] * g + LUMA_W[2] * b

def warmth(r, g, b):
    """Yellow-blue chromaticity, percent of luma. + = leans yellow/warm."""
    return 100.0 * (0.5 * (r + g) - b) / max(luma(r, g, b), 1e-6)

def redness(r, g, b):
    """Red-green chromaticity, same units. The second axis of one offset."""
    return 100.0 * (r - g) / max(luma(r, g, b), 1e-6)

def warm_split(shade_rgb, sun_rgb):
    """THE NUMBER. Hue of the light the key adds, over the light already there.

    0 = the shadow is the same colour as the sunlight and merely darker.
    + = warm key over cool fill (what R6 asserts).
    - = cool key over warm fill.
    """
    ratio = [sun_rgb[i] / max(shade_rgb[i], 0.5) for i in range(3)]
    return warmth(*ratio), redness(*ratio), ratio

def box_mean(path, box):
    w, h, ch, px = read_png(path)
    x0, y0, x1, y1 = box
    s = [0.0, 0.0, 0.0]
    n = 0
    for y in range(max(0, y0), min(h, y1)):
        row = y * w * ch
        for x in range(max(0, x0), min(w, x1)):
            i = row + x * ch
            s[0] += px[i]
            s[1] += px[i + 1]
            s[2] += px[i + 2]
            n += 1
    if n == 0:
        raise SystemExit(f"{path}: box {box} is empty")
    return [v / n for v in s], n

def pair(path, shadow_box, sun_box, label=""):
    """Same material, two boxes, one either side of a CAST SHADOW EDGE.

    The albedo distribution is held identical by picking the two boxes on the
    same surface a short way apart — the one arrangement in a photographed
    frame where that is true by construction rather than by hope.
    """
    a, na = box_mean(path, shadow_box)
    c, nc = box_mean(path, sun_box)
    ws, rs, ratio = warm_split(a, c)
    rng = luma(*c) / max(luma(*a), 0.01)
    print(f"{Path(path).name}  LIGHT SPLIT (pair)"
          + (f"  {label}" if label else ""))
    print(f"   shadow box {shadow_box}  n={na}"
          f"   RGB {a[0]:6.1f},{a[1]:6.1f},{a[2]:6.1f}  warmth {warmth(*a):6.2f}")
    print(f"   sun    box {sun_box}  n={nc}"
          f"   RGB {c[0]:6.1f},{c[1]:6.1f},{c[2]:6.1f}  warmth {warmth(*c):6.2f}")
    print(f"   luma range {rng:.2f}x   added-light ratio"
          f" {ratio[0]:.3f} {ratio[1]:.3f} {ratio[2]:.3f}")
    if rng < MIN_LUMA_RANGE:
        print(f"   REFUSED: luma range below {MIN_LUMA_RANGE}x — these two boxes"
              " do not hold two different lights, so no reading from them can"
              " fail (Rule 27).")
        return None
    print(f"   WARM_SPLIT {ws:+.2f}   red_split {rs:+.2f}")
    return ws

def scan(path, box, block, frac=0.15, label=""):
    """Block means over one box, darkest `frac` against brightest `frac`.

    For DAPPLED ground, where the lit and shaded parts interleave at a scale no
    box can separate. `block` must be COARSER THAN THE MATERIAL'S TEXTURE or
    this reproduces the deleted decile mode's defect: it is printed so that a
    reader can check it against the frame. Located by luma, which is not the
    property under test.
    """
    w, h, ch, px = read_png(path)
    x0, y0, x1, y1 = box
    x0, y0 = max(0, x0), max(0, y0)
    x1, y1 = min(w, x1), min(h, y1)
    step = max(1, block // 2)
    blocks = []
    for by in range(y0, y1 - block + 1, step):
        for bx in range(x0, x1 - block + 1, step):
            s = [0.0, 0.0, 0.0]
            for y in range(by, by + block):
                row = y * w * ch
                for x in range(bx, bx + block):
                    i = row + x * ch
                    s[0] += px[i]
                    s[1] += px[i + 1]
                    s[2] += px[i + 2]
            n = block * block
            m = [v / n for v in s]
            blocks.append((luma(*m), m))
    if len(blocks) < 8:
        raise SystemExit(f"{path}: box {box} holds {len(blocks)} blocks of"
                         f" {block} px — too few to sort")
    blocks.sort(key=lambda t: t[0])
    k = max(1, int(len(blocks) * frac))

    def avg(part):
        return [sum(b[1][j] for b in part) / len(part) for j in range(3)]

    a, c = avg(blocks[:k]), avg(blocks[-k:])
    ws, rs, ratio = warm_split(a, c)
    rng = luma(*c) / max(luma(*a), 0.01)
    print(f"{Path(path).name}  LIGHT SPLIT (scan)  box {box}  block {block} px"
          f"  n={len(blocks)}" + (f"  {label}" if label else ""))
    print(f"   darkest {frac:.0%}  RGB {a[0]:6.1f},{a[1]:6.1f},{a[2]:6.1f}"
          f"  warmth {warmth(*a):6.2f}")
    print(f"   brightest{frac:.0%}  RGB {c[0]:6.1f},{c[1]:6.1f},{c[2]:6.1f}"
          f"  warmth {warmth(*c):6.2f}")
    print(f"   luma range {rng:.2f}x   added-light ratio"
          f" {ratio[0]:.3f} {ratio[1]:.3f} {ratio[2]:.3f}")
    if rng < MIN_LUMA_RANGE:
        print(f"   REFUSED: luma range below {MIN_LUMA_RANGE}x (Rule 27).")
        return None
    print(f"   WARM_SPLIT {ws:+.2f}   red_split {rs:+.2f}")
    return ws

def selftest():
    """THE ZERO-DOSE ARM (Rule 48) and the tonemap bound, no frame needed.

    Arm A: TWO albedos, one WHITE light. Must return exactly 0 for both — a
    criterion that reports a split here is measuring the albedo, and the first
    version of this file did.
    Arm B: our shipped lookdev. Must move, and its value is what our frame is
    then checked against.
    Arm C: arm A put through a Reinhard tonemap and an sRGB encode, which is
    what a reference frame has been through. Not a control that can pass or
    fail — a BOUND on how much of a reference reading is the pipeline.
    """
    albedos = [(60.0, 110.0, 40.0), (150.0, 140.0, 120.0)]

    def arm(key, amb, tone=False):
        out = []
        for a in albedos:
            def px(t):
                v = [(amb[c] + t * key[c]) * a[c] / 255.0 for c in range(3)]
                if tone:
                    v = [x / (1.0 + x) for x in v]
                    v = [(x / 12.92 if x <= 0.04045
                          else ((x + 0.055) / 1.055) ** 2.4) for x in v]
                    v = [x ** (1.0 / 2.2) for x in v]
                return [255.0 * x for x in v]
            out.append(warm_split(px(0.0), px(1.0))[0])
        return out

    white_key, white_amb = (0.62, 0.62, 0.62), (0.36, 0.36, 0.36)
    warm_key, cool_amb = (0.62, 0.595, 0.546), (0.34, 0.36, 0.40)
    a = arm(white_key, white_amb)
    b = arm(warm_key, cool_amb)
    c = arm(white_key, white_amb, tone=True)
    print("ZERO DOSE  (white key, white ambient, two albedos):")
    print("   WARM_SPLIT " + "  ".join(f"{v:+.4f}" for v in a))
    print("SHIPPED LOOKDEV  (sun 1.00/0.96/0.88, ambient 0.34/0.36/0.40):")
    print("   WARM_SPLIT " + "  ".join(f"{v:+.4f}" for v in b))
    print("TONEMAP BOUND  (zero dose through Reinhard + sRGB, as a reference frame is):")
    print("   WARM_SPLIT " + "  ".join(f"{v:+.4f}" for v in c)
          + "   <- how much of a REFERENCE reading is its pipeline")
    ok = all(abs(v) < 0.01 for v in a) and all(abs(v) > 5.0 for v in b)
    print(f"   zero-dose control: {'PASS' if ok else 'FAIL'}"
          " (must be 0.00, and the shipped arm must move — Rule 48)")
    return 0 if ok else 1

def main(argv):
    if len(argv) > 1 and argv[1] == "selftest":
        raise SystemExit(selftest())
    if len(argv) > 3 and argv[1] == "pair":
        boxes = [tuple(int(t) for t in argv[i].split(",")) for i in (3, 4)]
        pair(argv[2], boxes[0], boxes[1], argv[5] if len(argv) > 5 else "")
        return
    if len(argv) > 3 and argv[1] == "scan":
        box = tuple(int(t) for t in argv[3].split(","))
        block = int(argv[4]) if len(argv) > 4 else 24
        scan(argv[2], box, block, label=argv[5] if len(argv) > 5 else "")
        return
    raise SystemExit(
        "usage: measure_light_split.py pair <frame.png> <shadow_box> <sun_box> [label]\n"
        "       measure_light_split.py scan <frame.png> <box> [block_px] [label]\n"
        "       measure_light_split.py selftest\n"
        "  boxes are x0,y0,x1,y1 and are part of the recipe — record them")

if __name__ == "__main__":
    main(sys.argv)
