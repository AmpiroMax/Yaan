#!/usr/bin/env python3
"""
Created: 11:08:2026 - 13:32:12
Last updated: 11:08:2026 - 14:05:38
Module: tools
File: tools/measure_aerial.py

Responsibility:
- Measure AERIAL PERSPECTIVE (REFERENCE_FRAMES.md R1) in a rendered frame: how
  much a landform's value separates from the sky it stands against, and how
  much internal texture contrast it still carries, at a KNOWN range. Run over
  the same landform at several ranges it produces the R1 curve, which is the
  only thing that can make the R1 claim pass or fail.

Key items:
- measure_ground()/`ground` mode: THE LOWLAND, a separate claim and therefore a
  separate instrument (Rule 41) — the depth cue between a near and a far band
  of the SAME surface, which is what «плоско» actually means.
- STANDOUT: mean |L - L(sky)| over the WHOLE box, classifying nothing. This is
  the number R1 lives or dies by, and it is unclassified on purpose (see the
  note inside measure()).
- luma(): the QUANTISER's weights (0.30/0.59/0.11), the same metric dfn_env.sh
  writes every brightness rule in. A separation that lives in blue is nearly
  invisible to the palette pass, so measuring it in Euclidean RGB overstates it.
- measure(): |L_rock - L_sky| (SEPARATION) and stddev of L over rock pixels
  (TEXTURE), both in 0..255 luma units, plus chroma so a wash toward the sky
  HUE is visible even when the value happens to match.

Dependencies:
- Uses: python3 stdlib only (zlib, struct) + tools/archive_frame.read_png.
  No Pillow, no numpy (Rule 24).
- Used by: render's R1 acceptance (docs/acceptance/), before and after.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE RULE THIS FILE LEARNED THREE TIMES, EACH TIME THE HARD WAY:
  **A METRIC MAY NOT LOCATE ITS SUBJECT BY THE PROPERTY UNDER TEST.**
  Haze changes colour, so anything that finds "the mountain" by colour finds a
  DIFFERENT mountain in every arm. It cost `standout` 243 of its 328 pixels in
  the strong arm; it made `contour` report 2.61 luma for a plainly visible peak;
  it made `bands` measure the splat's dither instead of the benches. Positions —
  silhouette edges, band rows, every box — are GEOMETRY, identical in all arms:
  settle them once on the NO-HAZE control and read every arm at the same pixels.
- QUOTE `standout`, NOT `separation`. `separation` first segments the landform
  by colour, and haze CHANGES that colour — in the strong arm it re-found 85
  pixels instead of 328 and reported the survivors as if they were the mountain.
  It is kept only because texture needs some notion of "rock", and it is
  printed with its pixel count so a collapsing count is visible.
- Follow docs/ARCHITECTURE.md strictly.
- THE BOXES ARE PART OF THE RECIPE. They are arguments, not guesses inside the
  tool: a segmentation that re-finds "the mountain" by colour would silently
  re-find a DIFFERENT set of pixels once haze changes that colour, and then the
  before/after arms would not be measuring the same thing. Record the boxes
  next to the frame.
- The rock filter drops sky-blue and grass-green pixels inside the box, so the
  box only has to bound the landform, not trace it. It does NOT drop pixels for
  being pale: that is exactly the signal under test.

UPD:
- 11:08:2026 - 13:32:12: Created for R1 (aerial perspective) — the before arm measured the
  same crag at 250/500/900 m and the numbers came back identical, which is the
  finding rather than a calibration problem.
- 11:08:2026 - 13:42:29: `separation` demoted, `standout` added. The strong-haze arm broke the
  first version in the way a metric is supposed to break — visibly.
- 11:08:2026 - 13:54:50: `ground` mode added for the lowland claim. It is a SECOND
  instrument and not a second box for the first one: an instrument aimed at a
  ridge does not accept a claim about a valley (Rule 41).
- 11:08:2026 - 13:56:11: H1 (contour) and H2 (bands) modes for design's §10.9
  propositions. H1 reports its MINIMUM and H2 reports the HEM PAIR FIRST,
  because both propositions are about the weakest place and a mean would pass
  exactly the failure each was written to catch.
- 11:08:2026 - 14:03:07: contour edges now come from a CONTROL arm, not from each
  arm's own colours. Same defect as the first `standout`, in a new
  costume, caught the same way — by an arm whose answer was absurd.
- 11:08:2026 - 14:05:38: band ROWS also come from the control arm, and the strip is
  smoothed over 7 rows. Unsmoothed and self-detected, the mode found the
  splat's dither and reported 0.05 steps for a no-haze control whose
  bands are plainly visible — the third appearance of one defect: an
  instrument that locates its subject by the property under test.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from archive_frame import read_png  # noqa: E402

# The palette pass's own weights (fs_upscale.sc / DFN_LUMA_WEIGHTS).
LUMA_W = (0.30, 0.59, 0.11)


def luma(r, g, b):
    return LUMA_W[0] * r + LUMA_W[1] * g + LUMA_W[2] * b


def is_sky(r, g, b):
    # Sky and its haze are the only strongly blue-dominant thing in an outdoor
    # frame; clouds are near-neutral and bright, which the caller excludes by
    # shooting the arms with DFN_CLOUD=0.
    return b > r + 18 and b > g + 8


def is_grass(r, g, b):
    return g > r + 18 and g > b + 18


def box_pixels(w, h, ch, px, box):
    x0, y0, x1, y1 = box
    for y in range(max(0, y0), min(h, y1)):
        row = y * w * ch
        for x in range(max(0, x0), min(w, x1)):
            i = row + x * ch
            yield px[i], px[i + 1], px[i + 2]


def stats(values):
    n = len(values)
    if n == 0:
        return 0.0, 0.0
    m = sum(values) / n
    var = sum((v - m) ** 2 for v in values) / n
    return m, var ** 0.5


def measure(path, rock_box, sky_box):
    w, h, ch, px = read_png(path)

    # The sky this landform stands against, from a box of clean sky beside it.
    sky_l = [luma(r, g, b) for r, g, b in box_pixels(w, h, ch, px, sky_box)
             if is_sky(r, g, b)]
    sky_ref, _ = stats(sky_l)

    # THE PRIMARY NUMBER, AND IT CLASSIFIES NOTHING. Mean |L - L(sky)| over
    # EVERY pixel of the box. It falls to zero exactly when the landform has
    # become sky, which is the R1 outcome stated as a measurement.
    #
    # It replaced a version that first kept only "rock" pixels, and the
    # replacement was forced by a frame rather than by taste: in the strong-haze
    # arm the crest went blue, the filter dropped it as SKY, and the metric
    # reported a healthy separation computed from the 85 least-hazed pixels that
    # survived. A segmentation that re-finds the object by colour cannot measure
    # a change TO that colour — it silently redefines the object instead.
    box_dev = [abs(luma(r, g, b) - sky_ref)
               for r, g, b in box_pixels(w, h, ch, px, rock_box)]
    standout, _ = stats(box_dev)

    # Secondary, and read WITH the count: the surviving non-sky non-grass
    # pixels. Useful for texture, misleading on its own — see above.
    rock_l, rock_c = [], []
    for r, g, b in box_pixels(w, h, ch, px, rock_box):
        if is_sky(r, g, b) or is_grass(r, g, b):
            continue
        rock_l.append(luma(r, g, b))
        rock_c.append(max(r, g, b) - min(r, g, b))
    lm, ls = stats(rock_l)
    cm, _ = stats(rock_c)
    return {
        "box_px": max(0, rock_box[2] - rock_box[0]) * max(0, rock_box[3] - rock_box[1]),
        "standout": standout,
        "rock_px": len(rock_l),
        "sky_px": len(sky_l),
        "rock_luma": lm,
        "sky_luma": sky_ref,
        "separation": abs(lm - sky_ref),
        "texture": ls,
        "rock_chroma": cm,
    }



def measure_ground(path, near_box, far_box):
    """THE LOWLAND, WHICH IS A DIFFERENT CLAIM AND THEREFORE A DIFFERENT
    INSTRUMENT (Rule 41). `standout` asks how a RIDGE separates from the sky it
    stands against; a valley floor stands against nothing. What «плоско» means
    is that the ground far away looks like the ground underfoot, so the number
    is the DEPTH CUE between two bands of the same surface: how far the far one
    has travelled from the near one in value and in colourfulness.

    Both bands are ground. Nothing is classified and nothing is filtered — the
    boxes are the recipe, and they must be the same in both arms.
    """
    w, h, ch, px = read_png(path)

    def band(box):
        ls, cs = [], []
        for r, g, b in box_pixels(w, h, ch, px, box):
            ls.append(luma(r, g, b))
            cs.append(max(r, g, b) - min(r, g, b))
        lm, _ = stats(ls)
        cm, _ = stats(cs)
        return lm, cm, len(ls)

    near_l, near_c, near_n = band(near_box)
    far_l, far_c, far_n = band(far_box)
    return {
        "near_luma": near_l, "far_luma": far_l,
        "near_chroma": near_c, "far_chroma": far_c,
        "near_px": near_n, "far_px": far_n,
        # Signed on purpose: haze lifts a dark surface toward a brighter sky and
        # would DARKEN one lit brighter than the sky. The sign says which.
        "depth_luma": far_l - near_l,
        # Haze always removes colour. An unsigned drop here would hide a
        # regression that made the distance MORE saturated.
        "depth_chroma": near_c - far_c,
    }



# Design's ruler (LANDSCAPE §10.9, NUMBERS PALETTE_SHADE_STEP_REF 0.0784), in
# the 0..255 luma units this file measures in. H1 wants 2 steps, H2 wants 1.
SHADE_STEP = 0.0784 * 255.0


def find_edges(path, box):
    """Row of the silhouette's top edge per column, by colour classification.

    ONLY EVER RUN ON THE CONTROL ARM. The silhouette's POSITION is geometry —
    same camera, same world, same frame — so it is identical in every arm, and
    the edge rows are part of the recipe rather than something each arm
    rediscovers. See measure_contour() for what happens otherwise.
    """
    w, h, ch, px = read_png(path)
    x0, y0, x1, y1 = box
    edges = {}
    for x in range(max(0, x0), min(w, x1)):
        for y in range(max(0, y0), min(h, y1)):
            i = (y * w + x) * ch
            if not is_sky(px[i], px[i + 1], px[i + 2]):
                edges[x] = y
                break
    return edges


def measure_contour(path, box, edge_source=None):
    """H1 — SILHOUETTE AGAINST SKY, AND IT IS A MINIMUM, NOT A MEAN.
    §10.9 asks for >= 2 steps «по всему контуру», so the number that decides it
    is the WEAKEST place on the outline. A mean would pass a mountain whose
    shoulder has dissolved as long as its peak is dark, which is precisely the
    failure haze produces.

    Per column: the edge row, then body = the 3 px just inside, sky = the 3 px
    just outside (skipping one, because the edge pixel is a blend of both and
    would understate every column by its own antialiasing).

    THE EDGES COME FROM `edge_source`, AND THIS IS THE SECOND TIME THIS EXACT
    MISTAKE WAS MADE IN THIS FILE. The first version of `standout` segmented the
    landform by colour and, in the strong-haze arm, re-found 85 pixels instead of
    328. This mode was then written with the same flaw in a new costume: it
    walked down each column to the first non-sky pixel, and in the strong arm the
    hazed mountain classified AS SKY, so the walk continued INTO the mountain and
    compared rock against rock — reporting a median of 2.61 luma for a mountain
    that is plainly visible in the frame. A metric may not locate its subject by
    the property under test. The edges are therefore detected once, on the
    NO-HAZE control, and every arm samples the same rows.
    """
    w, h, ch, px = read_png(path)
    edges = find_edges(edge_source or path, box)
    deltas = []
    for x, edge in sorted(edges.items()):
        if edge - 4 < 0 or edge + 4 >= h:
            continue
        def mean_l(ya, yb):
            v = []
            for y in range(ya, yb):
                i = (y * w + x) * ch
                v.append(luma(px[i], px[i + 1], px[i + 2]))
            return sum(v) / len(v)
        deltas.append(abs(mean_l(edge + 1, edge + 4) - mean_l(edge - 4, edge - 1)))
    deltas.sort()
    if not deltas:
        return None
    n = len(deltas)
    p05 = deltas[max(0, int(0.05 * n) - 1)]
    return {
        "columns": n,
        "min": deltas[0],
        "p05": p05,
        "median": deltas[n // 2],
        "steps_min": deltas[0] / SHADE_STEP,
        "steps_p05": p05 / SHADE_STEP,
        "steps_median": deltas[n // 2] / SHADE_STEP,
    }


def _row_profile(path, box, smooth=7):
    """Row-mean luma of a strip, hem (bottom) first, lightly smoothed.

    Smoothing is not cosmetic: one row of a 640x360 frame is thinner than a band
    lip, and the splat's ordered dither puts several luma units of legitimate
    texture into every row. Unsmoothed, the profile's extrema are dither.
    """
    w, h, ch, px = read_png(path)
    x0, y0, x1, y1 = box
    rows = []
    for y in range(max(0, y0), min(h, y1)):
        v = []
        for x in range(max(0, x0), min(w, x1)):
            i = (y * w + x) * ch
            v.append(luma(px[i], px[i + 1], px[i + 2]))
        rows.append((y, sum(v) / len(v)))
    rows.reverse()  # the box is in image order; the HEM is at the bottom
    out = []
    for k in range(len(rows)):
        a = max(0, k - smooth // 2)
        b = min(len(rows), k + smooth // 2 + 1)
        out.append((rows[k][0], sum(r[1] for r in rows[a:b]) / (b - a)))
    return out


def find_band_rows(path, box, min_prominence=4.0):
    """Rows of the riser/bench extrema, hem first. CONTROL ARM ONLY.

    Same discipline as find_edges: where the bands ARE is geometry, identical in
    every arm, so it is settled once on the no-haze control and then every arm is
    read at the same rows. Detecting them per-arm would filter small pairs out of
    the very arms whose small pairs are the failure under test — a selection rule
    that quietly deletes its own evidence.
    """
    prof = _row_profile(path, box)
    ext = []
    for k in range(1, len(prof) - 1):
        if (prof[k][1] - prof[k - 1][1]) * (prof[k + 1][1] - prof[k][1]) <= 0:
            if not ext or abs(prof[k][1] - ext[-1][1]) >= min_prominence:
                ext.append(prof[k])
    return [r for r, _ in ext]


def measure_bands(path, box, band_rows):
    """H2 — RISER/BENCH RHYTHM, READ FROM THE HEM UP.
    §10.9 is explicit that this is measured AT THE LOWEST VISIBLE BAND PAIR and
    never on the flank mean, because the height lever protects the crown and
    starves the hem — so a flank average reports a pass exactly when the failure
    is sitting at the bottom of the frame. Pairs come back hem-first and the
    caller reads the first one.
    """
    prof = dict(_row_profile(path, box))
    pairs = []
    for k in range(len(band_rows) - 1):
        lo, hi = band_rows[k], band_rows[k + 1]
        if lo not in prof or hi not in prof:
            continue
        d = abs(prof[hi] - prof[lo])
        pairs.append({"row_lo": lo, "row_hi": hi, "delta": d,
                      "steps": d / SHADE_STEP})
    return pairs


def parse_box(s):
    v = [int(t) for t in s.split(",")]
    if len(v) != 4:
        raise SystemExit(f"box needs x0,y0,x1,y1 — got {s!r}")
    return tuple(v)


def main(argv):
    if len(argv) > 1 and argv[1] == "contour":
        m = measure_contour(argv[2], parse_box(argv[3]),
                            argv[4] if len(argv) > 4 else None)
        print(f"{Path(argv[2]).name}  H1 CONTOUR (need >= 2.00 steps everywhere)")
        if m is None:
            print("  no silhouette found in the box"); return
        print(f"  columns {m['columns']}  min |dL| {m['min']:6.2f} "
              f"= {m['steps_min']:5.2f} steps   p05 {m['p05']:6.2f} "
              f"= {m['steps_p05']:5.2f} steps   median {m['median']:6.2f} "
              f"= {m['steps_median']:5.2f} steps")
        print(f"  H1 {'PASS' if m['steps_min'] >= 2.0 else 'FAIL'} on the minimum")
        return

    if len(argv) > 1 and argv[1] == "bands":
        box = parse_box(argv[3])
        src = argv[4] if len(argv) > 4 else argv[2]
        pairs = measure_bands(argv[2], box, find_band_rows(src, box))
        print(f"{Path(argv[2]).name}  H2 BANDS, hem first "
              f"(need >= 1.00 step at the LOWEST pair)")
        for k, q in enumerate(pairs[:6]):
            tag = "  <-- THE HEM PAIR, this is H2" if k == 0 else ""
            print(f"  rows {q['row_lo']:3d}->{q['row_hi']:3d}  dL {q['delta']:6.2f}"
                  f"  = {q['steps']:5.2f} steps{tag}")
        if pairs:
            print(f"  H2 {'PASS' if pairs[0]['steps'] >= 1.0 else 'FAIL'} at the hem")
        return

    if len(argv) > 1 and argv[1] == "ground":
        if len(argv) < 5:
            raise SystemExit("usage: measure_aerial.py ground <frame.png> "
                             "<near x0,y0,x1,y1> <far x0,y0,x1,y1> [label]")
        frame, near, far = argv[2], parse_box(argv[3]), parse_box(argv[4])
        lab = argv[5] if len(argv) > 5 else ""
        m = measure_ground(frame, near, far)
        print(f"{Path(frame).name}  GROUND {lab}")
        print(f"  near L {m['near_luma']:7.2f} chroma {m['near_chroma']:6.2f}"
              f"  ({m['near_px']} px)")
        print(f"  far  L {m['far_luma']:7.2f} chroma {m['far_chroma']:6.2f}"
              f"  ({m['far_px']} px)")
        print(f"  DEPTH CUE  dL {m['depth_luma']:+7.2f}"
              f"   chroma drop {m['depth_chroma']:+7.2f}")
        return

    if len(argv) < 4:
        raise SystemExit(
            "usage: measure_aerial.py <frame.png> <rock x0,y0,x1,y1> "
            "<sky x0,y0,x1,y1> [range_m]\n"
            "       measure_aerial.py ground <frame.png> <near box> <far box>")
    frame, rock, sky = argv[1], parse_box(argv[2]), parse_box(argv[3])
    rng = argv[4] if len(argv) > 4 else "?"
    m = measure(frame, rock, sky)
    print(f"{Path(frame).name}  range={rng} m")
    print(f"  STANDOUT mean|L-Lsky| over the whole box {m['standout']:7.2f}"
          f"   (box {m['box_px']} px)")
    print(f"  [secondary] rock px {m['rock_px']}  L(rock) {m['rock_luma']:7.2f}"
          f"  L(sky) {m['sky_luma']:7.2f}")
    print(f"  [secondary] separation {m['separation']:7.2f}  "
          f"texture sd(L) {m['texture']:6.2f}  chroma {m['rock_chroma']:6.2f}")


if __name__ == "__main__":
    main(sys.argv)
