<!--
Created: 13:08:2026 - 16:52:00
Last updated: 13:08:2026 - 17:34:00
-->
<!--
UPD:
- 13:08:2026 - 16:52:00: Created — the recipe and the numbers for the first
  non-zero GROUND_OCCLUSION_COUNT this project has read.
- 13:08:2026 - 17:06:00: The grain question — the anisotropy ratio measured at
  four rulers on one world, plus the frame at the pitch the probe scores worst.
- 13:08:2026 - 17:34:00: THE SHIPPED PAIR IS NOW `+f2` (aa55c1c): the approved
  18 m draw pitch, the wander, the ending talwegs and the angled tributaries.
  The `+f1` pair stays as the first-light state — the frame in which this
  contract first read anything but zero.
-->

# A1 — THE GROUND GETS FORMS (§10.1.3 F7)

**Shipped pair: `core-A1-forms-{BEFORE,AFTER}-aa55c1c+f2.png`.**
`+f1` (0f0dfad) is kept beside it as the FIRST-LIGHT state — the frame in which
`GROUND_OCCLUSION_COUNT` first read anything but zero, before the draw pitch was
approved at 18 m and before the draws stopped being a washboard.

**Both arms are the SAME BINARY.** The "before" arm is not an older build and not
an archived frame: it is this build with the pass held at identity through its
own doors, which is the only kind of control that cannot drift (Rule 27, Rule
30). At those settings the generator reproduces the pinned testbed heightmap
digest **byte for byte** — that equality is what makes the arm a control rather
than a similar-looking picture.

## Recipe

One binary, `build_core/engine/app/dfn_app` at commit `0f0dfad`. The camera is
not ours — it is the archived lowland frame's, unchanged since render pinned it
for a haze question:

    # AFTER (shipped)
    DFN_MASSIF_PROBE=1 DFN_MASSIF_EYE=51,650 DFN_TOUR=1 DFN_TOUR_DIR=<dir> \
    DFN_INTERNAL_RES=640x360 DFN_PALETTE=0 DFN_TIME=0.5 DFN_CLOUD=0 DFN_MENU=0 \
    ./build_core/engine/app/dfn_app

    # BEFORE (the control: the same binary with the operator at identity)
    DFN_TERRACE_STRENGTH=0 DFN_DRAW_DEPTH=0 <...the same line...>

then `tools/archive_frame.py <dir>/00_massif_verdict.png <name>.png`.

## What must be visible for this pair to mean anything

F7's failure statement, verbatim: *the frame fails if the ground runs unbroken
from the player's feet to the tree line — one continuous shaded surface with no
edge in it.*

- **BEFORE**: exactly that. One green sheet from the boulders in the near field
  to the tree line; the only edges in the frame belong to objects standing on
  the ground, not to the ground.
- **AFTER**: a bench lip crossing the whole frame at mid distance with the
  ground beyond it in a different tone, a bank with rock in it at centre, and a
  tree line that stands behind a rise instead of on the same sheet.

## The numbers, all established in the generator (Rule 47)

`GROUND_OCCLUSION_COUNT` at the pinned A1 standpoint, counted by raycast in the
5–60 m band, per frame column, **on the 2 m heightmap the world is actually
drawn and collided on**:

| | min | p5 | median | max |
|---|---|---|---|---|
| BEFORE (forms at identity) | 0 | 0 | 0 | 1 |
| `+f1`, first light (24 m pitch) | 1 | 1 | 2 | 4 |
| **AFTER, shipped (`+f2`, 18 m pitch)** | **2** | **2** | **3** | **6** |

Floor is `GROUND_OCCLUSION_COUNT_MIN` = 3 read at percentile 5. **p5 = 2 is not
a pass.** It is what three surrogates and one direct wavelength sweep could not
produce at all: every one of them read zero.

All four order statistics move together between `+f1` and `+f2`, which is what
separates a real shift from one column's speckle — that is the ground the pitch
change was approved on.

Beside it, same run, same build:

| quantity | before | after | bound |
|---|---|---|---|
| detrended σ over 20 m at A1 | 0.353 m | 0.678 m | ≤ 1.20 m (ceiling holds, 1.8× margin) |
| hill-band anisotropy (§2.1) | 3.13 | 2.64 | ≥ 2.50 — and see «THE GRAIN QUESTION» below: the floor does not bind at this scale, ruled on the ruler measurement |
| columns hiding any ground, 12 flattest legal standpoints × 16 azimuths | 76.6 % | **96.4 %** | — |
| reachable ground over 400 m of the 2 m lattice | 99.9725 % | 99.9725 % | identical to five figures |
| bank-direction spread (axial, per window) | — | 0.408 | 0.16 is corduroy, 0.80 is no direction at all |

## Two warnings that belong ON this pair

1. **Do not compute percentages from these two pictures.** Measured on this same
   standpoint the day before: a re-shot copy of the SAME arm differs from itself
   by 11.9 % of pixels and the two arms differ by 4.7 %. The noise is larger
   than the signal. The pictures show WHICH FORMS ARE THERE; every number above
   comes from the generator.
2. **The population row changes its own sample.** `flattest_legal_standpoints`
   ranks by trend over the terrain being measured, so the twelve standpoints are
   not the same twelve in both arms. A1 is pinned and is the comparable one.

---

# THE GRAIN QUESTION — evidence, not a ruling (§2.1's row is design's)

Switching this pass on drops §2.1's anisotropy probe from **3.13 to 2.64**
against a 2.50 floor. The obvious reading is that the forms erase the land's
grain — an isotropic octave did exactly that once before (3.61 → 2.22). Two
independent measurements say that is NOT what is happening here.

## 1. The ratio as a function of the RULER, on ONE world

§2.1's probe takes gradients with a **±6 m arm on a 12 m lattice**. The draws sit
at a **15–24 m pitch** — one to two samples per cycle, at or past Nyquist, where
a regular lineation aliases into a beat whose direction is arbitrary. So the
same structure tensor was read at four arms, varying the ruler instead of the
world (`test_ground_relief`, "…as a function of the RULER"):

| arm | forms ON | forms OFF | ON/OFF |
|---|---|---|---|
| ±6 m (**the probe's own arm**) | 2.636 | 3.128 | **0.843** |
| ±12 m | 2.135 | 2.361 | 0.904 |
| ±24 m | 1.928 | 1.940 | **0.993** |
| ±48 m | 1.717 | 1.776 | 0.967 |

**The cost is confined to the scale band of the forms themselves.** At every
ruler long enough to see the hill band — which is what §2.1's clause is about,
«холмы — вытянутые гряды, не круглые бугры» — the forms cost between 0.7 % and
3 %. The 16 % appears only at the arm that cannot resolve them.

The world-side sweep says the same thing from the other side: the ratio recovers
monotonically as the draw pitch moves away from the sampling pitch — 14 m: 2.18,
16 m: 2.42, 24 m: 2.64, 29 m: 2.89, 36 m: 2.95.

## 2. What the eye says, on the arm the probe likes least

`core-A1-grain-DRAWPITCH14-DIAG-0f0dfad.png` — the pinned A1 standpoint, same
binary, one variable (`DFN_DRAW_CELL=14`), the setting the probe scores **2.18**,
its worst. Against the shipped frame (2.64) that ground reads **MORE** lineated,
not less: long parallel bands run across the whole mid field, where the shipped
frame has fewer and more isolated edges. If anything the 2.18 frame's fault is
the opposite one — the lineation is too REGULAR, a washboard, which is a
manufactured look and a real objection, but it is not a loss of direction.

**Both lines point the same way: the probe's reading falls where the eye's
reading rises.** Under either explanation — aliasing, or a genuine loss of fine-
scale gradient coherence — the property §2.1 names is untouched at the scale it
names it. Handed to design as evidence; the row and its instrument are theirs.
