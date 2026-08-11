#!/usr/bin/env python3
"""
Created: 11:08:2026 - 23:58:40
Last updated: 11:08:2026 - 23:58:40
Module: tools
File: tools/measure_ground_colour.py

Responsibility:
- Measure R5 (REFERENCE_FRAMES.md): "ground colour is multi-hue at SEVERAL
  SCALES, with no readable tile". R5 names TWO defects and they are not the
  same defect, so this file reports them as two columns of one table rather
  than as one score:
    (a) ONE TONE — how far the ground's colour travels at all.
    (b) A READABLE TILE — whether patches the size of the material's repeat
        all look the SAME as each other, which is what makes a tile findable
        by eye. A tile is readable exactly when the coarse column is flat.

Key items:
- scales(): block-mean spread at several block sizes, CHROMA and LUMA reported
  SEPARATELY. This separation is the whole instrument (see below).
- main()/`scales` mode; `refs` mode runs a box on several frames at once.

Dependencies:
- Uses: python3 stdlib only + tools/archive_frame.read_png (Rule 24: no Pillow,
  no numpy).
- Used by: render's R5 acceptance (docs/acceptance/), before and after, and by
  the reference frames that set the target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- READ THE NOTICE IN tools/measure_aerial.py BEFORE ADDING A MODE HERE. That
  file caught the same defect FIVE times in one day: **a metric may not locate
  its subject by the property under test.** This file obeys it the cheap way —
  IT CLASSIFIES NOTHING. The box is the recipe, every pixel inside it counts,
  and there is no "is this ground" test anywhere. A hue filter would have been
  the sixth instance: the property under test IS the hue.
- WHY CHROMA AND LUMA ARE NEVER SUMMED. The ground's VALUE moves for reasons
  that have nothing to do with its colour — sun angle, slope shading, cloud
  shadow, haze. A single "colour spread" number therefore passes a monochrome
  green field lit unevenly, which is EXACTLY today's frame and EXACTLY the
  thing the user is complaining about. Hue is the material; value is the light.
  Report them apart or report nothing.
- THE COARSE COLUMN IS THE TILE TEST, AND IT NEEDS NO KNOWLEDGE OF THE TILE.
  If every patch of ground a tile wide has the same mean colour as every other,
  the material is one stamp repeated, and the eye finds the stamp. So the
  coarse chroma spread of a tiled material is ~0 BY CONSTRUCTION, whatever the
  tile's period is and wherever its seams fall. That is why this instrument
  does not have to find the seams — a defect nobody can locate reliably in a
  perspective frame.
- SCALE-FREE BLOCK SIZES. Blocks are fractions of the BOX WIDTH, not pixel
  counts, because the reference frames are 900 px wide and ours is 640: at
  fixed pixel sizes the two would be measured at different physical scales and
  the comparison would be arithmetic on unlike quantities.
- THE CONTROL THIS FILE NEEDS FROM ITS CALLER (Rule 48): the zero-dose arm.
  Whatever change is made to the ground, shooting it at zero amplitude must
  return the BEFORE numbers. A criterion that still passes at zero dose is
  measuring the light or the terrain, not the material.

UPD:
- 11:08:2026 - 23:58:40: Created for R5. The diagnosis it was built to serve was
  settled first, by frames rather than by this file: DFN_TERRAIN_TILES 8/32/128
  moved the blob scale by exactly the same factor in both directions, so the
  pattern on our ground is the MATERIAL TILE and not the palette dither, the
  ordered splat dither or the coverage AA. This file measures what is left.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from archive_frame import read_png  # noqa: E402

# The quantiser's own weights (fs_upscale.sc / DFN_LUMA_WEIGHTS), the metric
# every brightness rule in this project is written in.
LUMA_W = (0.30, 0.59, 0.11)

# Design's frozen ruler (NUMBERS PALETTE_SHADE_STEP_REF 0.0784) in 0..255 units.
# Quoted here because the ceiling on the LUMA column comes from it: two regions
# are SEPARATE at LANDMARK_SEPARATION_STEPS_MIN = 2 rulers (§1.3b), so a ground
# tint whose coarse luma spread reaches 2 rulers has stopped being one surface
# and become two materials — which is the ruling that killed the previous
# render-side mottling (TerrainMesher UPD 09:08:2026 - 14:11:37).
SHADE_STEP = 0.0784 * 255.0


def luma(r, g, b):
    return LUMA_W[0] * r + LUMA_W[1] * g + LUMA_W[2] * b


def opponents(r, g, b):
    """The two chroma axes, in the same 0..255 units as luma.

    Deliberately opponent axes and not HSV hue: hue is undefined at low
    saturation and wraps, so its standard deviation is meaningless exactly
    where our ground lives (a nearly-grey frame would score huge). These two
    are linear, defined everywhere, and zero for any neutral.
    """
    return r - g, 0.5 * (r + g) - b


def stats(values):
    n = len(values)
    if n == 0:
        return 0.0, 0.0
    m = sum(values) / n
    var = sum((v - m) ** 2 for v in values) / n
    return m, var ** 0.5


def block_means(w, h, ch, px, box, block_px, stride=None):
    """Mean (luma, rg, yb) of every whole block inside the box.

    `stride` defaults to HALF the block. Overlapping blocks are not a way of
    inventing samples: a ground box is short (it is a band of a 360-row frame)
    and at the coarse size a non-overlapping walk yields four or five blocks,
    on which a standard deviation is noise. The overlap costs correlation
    between neighbours, which biases the spread DOWNWARD — the safe direction
    for a metric whose failure mode is over-claiming.
    """
    x0, y0, x1, y1 = box
    x0, y0 = max(0, x0), max(0, y0)
    x1, y1 = min(w, x1), min(h, y1)
    step = max(1, block_px // 2) if stride is None else stride
    out = []
    for by in range(y0, y1 - block_px + 1, step):
        for bx in range(x0, x1 - block_px + 1, step):
            sl = srg = syb = 0.0
            for y in range(by, by + block_px):
                row = y * w * ch
                for x in range(bx, bx + block_px):
                    i = row + x * ch
                    r, g, b = px[i], px[i + 1], px[i + 2]
                    sl += luma(r, g, b)
                    a, c = opponents(r, g, b)
                    srg += a
                    syb += c
            n = block_px * block_px
            out.append((sl / n, srg / n, syb / n))
    return out


def scales(path, box, divisors=(64, 16, 4)):
    """Block-mean spread at several block sizes, chroma and luma apart.

    `divisors` are fractions of the BOX WIDTH, coarse block last, so the same
    call means the same physical thing on a 900 px reference and on our 640 px
    frame.
    """
    w, h, ch, px = read_png(path)
    box_w = min(w, box[2]) - max(0, box[0])
    box_h = min(h, box[3]) - max(0, box[1])
    rows = []
    for d in divisors:
        # Clamped to the box HEIGHT: a ground box is a band, and a block taller
        # than the band has no samples at all — which prints as 0.00 and reads
        # as "perfectly uniform", the exact opposite of what it means.
        block = max(1, min(box_w // d, box_h))
        means = block_means(w, h, ch, px, box, block)
        _, sd_l = stats([m[0] for m in means])
        _, sd_rg = stats([m[1] for m in means])
        _, sd_yb = stats([m[2] for m in means])
        # The two chroma axes combine as a plane distance, not a sum: they are
        # orthogonal directions of one 2-D colour offset.
        chroma = (sd_rg ** 2 + sd_yb ** 2) ** 0.5
        rows.append({"block_px": block, "blocks": len(means),
                     "luma_sd": sd_l, "chroma_sd": chroma,
                     "rg_sd": sd_rg, "yb_sd": sd_yb,
                     "luma_rulers": sd_l / SHADE_STEP})
    return rows


def print_scales(path, box, rows, label=""):
    print(f"{Path(path).name}  GROUND COLOUR  box {box[0]},{box[1]},{box[2]},{box[3]}"
          + (f"  {label}" if label else ""))
    print("   block   n   CHROMA sd   luma sd  (= rulers)")
    for i, r in enumerate(rows):
        tag = ["  <- fine: in-tile texture",
               "  <- mid",
               "  <- COARSE: patch-to-patch. Flat here = a readable tile"]
        note = tag[i] if i < len(tag) else ""
        print(f"  {r['block_px']:5d} {r['blocks']:5d}   {r['chroma_sd']:8.2f}"
              f"  {r['luma_sd']:8.2f}   {r['luma_rulers']:5.2f}{note}")
    if len(rows) >= 2 and rows[0]["chroma_sd"] > 0.0:
        keep = rows[-1]["chroma_sd"] / rows[0]["chroma_sd"]
        print(f"  SURVIVAL coarse/fine chroma {keep:.2f}"
              "   (1.00 = colour structure at every scale; ~0 = one stamp)")


def main(argv):
    if len(argv) > 1 and argv[1] == "refs":
        # One box, several frames: the reference target and our frame read by
        # the same instrument at the same fractions of the box.
        box = tuple(int(t) for t in argv[2].split(","))
        for frame in argv[3:]:
            print_scales(frame, box, scales(frame, box))
            print()
        return
    if len(argv) < 3:
        raise SystemExit(
            "usage: measure_ground_colour.py <frame.png> <x0,y0,x1,y1> [label]\n"
            "       measure_ground_colour.py refs <x0,y0,x1,y1> <frame.png>...")
    frame = argv[1]
    box = tuple(int(t) for t in argv[2].split(","))
    print_scales(frame, box, scales(frame, box), argv[3] if len(argv) > 3 else "")


if __name__ == "__main__":
    main(sys.argv)
