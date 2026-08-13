<!--
Created: 13:08:2026 - 16:52:00
Last updated: 13:08:2026 - 16:52:00
-->
<!--
UPD:
- 13:08:2026 - 16:52:00: Created — the recipe and the numbers for the first
  non-zero GROUND_OCCLUSION_COUNT this project has read.
-->

# A1 — THE GROUND GETS FORMS (§10.1.3 F7)

`docs/acceptance/core-A1-forms-{BEFORE,AFTER}-0f0dfad+f1.png`

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
| AFTER (shipped) | 1 | **1** | 2 | 4 |

Floor is `GROUND_OCCLUSION_COUNT_MIN` = 3 read at percentile 5. **p5 = 1 is not
a pass.** It is the first non-zero this contract has ever read, after three
surrogates and one direct sweep produced zero.

Beside it, same run, same build:

| quantity | before | after | bound |
|---|---|---|---|
| detrended σ over 20 m at A1 | 0.353 m | 0.50 m | ≤ 1.20 m (ceiling holds, 2.4× margin) |
| hill-band anisotropy (§2.1) | 3.13 | 2.64 | ≥ 2.50 — **margin thinned to 5.6 %** |
| columns hiding any ground, 12 flattest legal standpoints × 16 azimuths | 76.6 % | 95.3 % | — |
| reachable ground over 400 m of the 2 m lattice | 99.9725 % | 99.9725 % | identical to five figures |

## Two warnings that belong ON this pair

1. **Do not compute percentages from these two pictures.** Measured on this same
   standpoint the day before: a re-shot copy of the SAME arm differs from itself
   by 11.9 % of pixels and the two arms differ by 4.7 %. The noise is larger
   than the signal. The pictures show WHICH FORMS ARE THERE; every number above
   comes from the generator.
2. **The population row changes its own sample.** `flattest_legal_standpoints`
   ranks by trend over the terrain being measured, so the twelve standpoints are
   not the same twelve in both arms. A1 is pinned and is the comparable one.
