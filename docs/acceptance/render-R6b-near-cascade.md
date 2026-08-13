<!--
Created: 13:08:2026 - 16:30:00
Last updated: 13:08:2026 - 16:30:00
-->
<!--
UPD:
- 13:08:2026 - 16:30:00: Created. R6b's suspect 1 (SHADOW_TEXEL_M) built as a near
  cascade, measured against a same-binary control, and left OFF by default: it
  buys +0.010 of local contrast at the dapple's own scale for +22 % of the
  frame. The result that outlives it is why — our near forest floor spans
  1.15x p90/p10 with the shadow either on or off, against reference 03's 3.23x,
  so there is no sun on it for a finer map to interrupt. The gate has moved to
  canopy transmission (flora) and floor material tone (ground).
-->

# R6b — THE NEAR SHADOW CASCADE: BUILT, MEASURED, AND LEFT OFF

Follow-up to `docs/specs/render.md` §R6b, which diagnosed the missing canopy
dapple as a GRAIN problem and named the near cascade as suspect 1's remedy.
The cascade is now built. **It is off by default, and this file is why.**

---

## 1. The arithmetic, on the real numbers

| quantity | value |
|---|---|
| `SHADOW_TEXEL_M` = 2 x 320 / 4096 | **0.1563 m** |
| this file's own thin-caster rule (>= 2 texels) | **0.3125 m** |
| `SHADOW_NORMAL_OFFSET_M` (receiver push-off, both sides) | 0.1563 m |
| **leaf MASK texel** — `LEAF_ATLAS_TILE_PX` 64 on a 3.0-5.5 m card | **0.047-0.086 m** |
| authored rim bites (`HOLE_LATTICE` 7 over 64 px, FloraCards) | 0.4-0.9 m = 2.6-5.8 texels |
| authored interior gaps (`GAP_RADIUS` 0.095 of a 5.5 m card) | ~1.0 m = 6.4 texels |

**What cannot cast a shadow today: everything narrower than 0.31 m.** In the
canopy that is every twig and every gap between leaf cards, and — the sharper
form of the same statement — **the shadow map undersamples the leaf mask by
2-3x.** The mask is a 64x64 image whose own texel is 0.047-0.086 m and the map
cannot resolve one of them, so the ragged rim the mask draws at its own
resolution reaches the ground as nothing at all. Only what flora deliberately
authored ABOVE render's floor survives: the 0.4-0.9 m bites and the one or two
1 m interior gaps. That is a low-pass filter with a 0.31 m cutoff, and it is
exactly why the shadow's measured contribution RISES with block size.

The near cascade: 4096 over a 40 m half extent = **0.0195 m per texel, 8x
finer**, thin-caster floor 0.039 m — below the leaf mask's own texel, so the
mask goes from undersampled to oversampled. `SHADOW_NEAR_NORMAL_OFFSET_M` is
denominated in NEAR texels for the same reason the far one is: reusing the far
0.156 m push-off on a 0.0195 m map would erode eight texels off every hole and
spend the whole gain before the first fragment.

## 2. The recipe

ONE binary, three arms, everything but the shadow identical. This is not
optional bookkeeping — see §5.

```
DFN_TOUR=1 DFN_TOUR_DIR=<dir> DFN_INTERNAL_RES=640x360 DFN_PALETTE=0 \
DFN_FLORA_PROBE=1 DFN_FLORA_PITCH=-0.30 DFN_WIND_FREEZE=3.0 DFN_CLOUD=0 \
DFN_SUN_SHADOW=<1|0> DFN_SHADOW_NEAR=<1|0> \
  build_render/engine/app/dfn_app
then tools/archive_frame.py <shot> <out> 640      (4x4 box average, README rule)
then tools/measure_dapple.py <out> 0,140,640,355 8,16,24,40 4 <label>
```

| arm | `DFN_SUN_SHADOW` | `DFN_SHADOW_NEAR` | frame |
|---|---|---|---|
| NULL — no sun shadow at all | 0 | — | `render-R6b-cascade-NULL-d9aeb0e+nc.png` |
| FAR — the shipped shadow, far map only | 1 | 0 | `render-R6b-cascade-FAR-d9aeb0e+nc.png` |
| NEAR — plus the cascade | 1 | 1 | `render-R6b-cascade-NEAR-d9aeb0e+nc.png` |

Ground box `0,140,640,355`; window 4x4 blocks. Reference for the same
quantity: `images_examples/render/image copy 12.png` box `540,430,1190,600`
(frame 03's forest floor). n = 3 runs per arm; the spread column is what makes
the verdict readable, because a single run of ONE arm repeats to only
0.003-0.016.

## 3. What the cascade is worth

Local light/dark, mean of 3 runs per arm, one binary:

| block px | NULL | FAR | NEAR | **far map's contribution** | **CASCADE'S OWN** | run spread |
|---|---|---|---|---|---|---|
| 8  | 1.241 | 1.4387 | 1.4483 | +0.198 | **+0.010** | 0.003-0.005 |
| 16 | 1.320 | 1.6553 | 1.6557 | +0.335 | **+0.000** | 0.002-0.006 |
| 24 | 1.313 | 1.6733 | 1.6833 | +0.360 | **+0.010** | 0.008-0.012 |
| 40 | 1.294 | 1.7197 | 1.7660 | +0.426 | **+0.046** | 0.006-0.016 |

**The cascade's own spectrum RISES with block size too** (+0.010 at 8 px against
+0.046 at 40 px), which is the shape it was built to invert. Two of the four
numbers are inside their own run-to-run spread. It sharpens the edge of the
canopy-sized blob; it does not create fine grain.

Cost, same vantage, `DFN_FRAME_LOG`, last 200 presented frames, 7 runs per arm:

| | held the 120 Hz cap (p50 < 12 ms) | mean of means |
|---|---|---|
| cascade OFF | **6/7 runs** | 12.38 ms |
| cascade ON | **3/7 runs** | 15.06 ms (**+22 %**) |

Plus 33.5 MB of VRAM for a second 4096^2 D16 target.

**Verdict: not shipped on.** A vsync tier at the exact vantage the user's
running-stutter complaint lives at, bought with a tenth of what the far map
already delivers, is not a trade to make. `DFN_SHADOW_NEAR=1` turns it on; at 0
(the default) the near target is allocated at 4x4, its view is never touched,
and the frame is the one that shipped before this change.

## 4. WHY IT BUYS SO LITTLE — and this is the result, not the feature

Threshold-free, luma quantiles over the NEAR band (`0,300,640,355` — the ground
closest to the eye, i.e. precisely what a 40 m cascade covers):

| surface | p10 | p25 | p50 | p75 | p90 | **p90/p10** |
|---|---|---|---|---|---|---|
| **reference 03's forest floor** | 36.0 | 48.9 | 62.9 | 83.0 | 116.3 | **3.23x** |
| ours, sun shadow ON | 27.5 | 27.8 | 30.0 | 31.0 | 31.7 | **1.15x** |
| ours, sun shadow OFF | 49.6 | 49.7 | 53.1 | 56.0 | 57.3 | **1.15x** |

Read the two bottom rows together. Our canopy shadow takes an evenly lit floor
(a flat sheet at 53) to an evenly dark floor (a flat sheet at 30) **and leaves
nothing in between**: 97.3 % of the near band is above luma 45 with the shadow
off and 4.6 % with it on. The reference's floor spans 3.23x continuously — its
light and shade interleave everywhere.

**No shadow-map resolution can invent a middle tone that has no source.** A
finer map can only modulate sun that reaches the ground, and on our near forest
floor essentially none does. That is why the cascade moved the sunlit fraction
from 4.6 % to 5.2 % — it does open thin gaps the 0.31 m floor had closed, which
is the arithmetic working exactly as predicted — and why 5.2 % of a surface is
not a dapple.

**The gate has moved out of this zone.** It is now (a) how much sun the canopy
lets through — flora's card density and stand spacing, and note that
`FloraCards.cpp` sized its mask features against "render's ~0.31 m
mask-feature floor", a premise this change makes stale — and (b) how much tone
the floor material carries, which at 1.15x p90/p10 is nearly flat on its own.

## 5. Rule 47, and it nearly got a seventh scalp here

The first measurement of this change was a before/after across two binaries an
hour apart, and it looked like a triumph: 8 px 1.259 -> 1.441, 40 px contribution
falling for the first time. **All of it was somebody else's.** The
`DFN_SHADOW_NEAR=0` arm out of the SAME binary scored 1.417 at 8 px — within
0.024 of the cascade arm. Between the two builds a peer's canopy work had
landed, and the eye's own resolved height moved 17.42 -> 17.54 -> 16.23 m over
three rebuilds. A before/after across binaries in this tree does not measure
the change under test; it measures the week.

The cure is the one Rule 48 already states and this file now demonstrates: the
control has to vary INSIDE one binary. `DFN_SHADOW_NEAR` exists for that and
for nothing else, and it is the only reason the honest number in §3 is +0.010
rather than +0.190.
