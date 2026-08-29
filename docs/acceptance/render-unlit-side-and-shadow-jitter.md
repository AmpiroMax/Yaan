
# THE UNLIT SIDE, AND THE CRAWLING SHADOW

The user named several things at once. They are separate defects with separate
mechanisms, and this file keeps them apart because two of them are free to fix
and the third is not.

| his words | the quantity | before | after |
|---|---|---|---|
| "тёмные деревья, словно их нет, как чёрное пятно … цвет одинаковый" | luma p90/p10 across a shadowed bole | **1.01x** | **1.16x** |
| "дергаются, колеблются, мерцают по краям" при движении солнца | shadow grid slide per frame | **0.1720 texels** | **0.0037** |
| the same, as an event rate | full-texel steps per second | **11.5/s** | **0.1/s** |
| "из сильно больших квадратных блоков рисуются" | `SHADOW_TEXEL_M` | 0.156 m | **0.156 m — untouched** |

---

## 1. The unlit side had no form, and it could not have had any

`dfn_surface_light` gated every one of its terms on the sun, the moon or a lamp.
By day, in the shadow half-space, the whole function reduced to

```
light = u_ambientColor * sky_vis
```

**with no surface normal in it anywhere.** Every normal therefore returned the
same number and what remained on screen was the albedo texture times a constant.

Measured before anything was touched, on a bole standing in its own canopy's
shadow, 228 pixels strictly inside the bark (`tree_p0.62.png`, box `32,214,44,233`):

| surface | p10 | p50 | p90 | p90/p10 | distinct tones |
|---|---|---|---|---|---|
| **the bole, in shadow** | 21.8 | 21.8 | 22.1 | **1.01x** | **16 / 228** |
| the same bole, `DFN_SUN_SHADOW=0` | 21.8 | 37.8 | 38.1 | 1.75x | 21 |
| open sunlit ground, same frame | 44.6 | 96.8 | 145.5 | 3.26x | 233 / 1400 |
| a lit crown, same frame | 29.3 | 45.5 | 109.9 | 3.75x | 364 / 1080 |

The sixteen tones are the BARK, not the tree. Note also the second row: with the
shadow off the bole shows *two* flat plateaus, 21.8 and 38.1, not a gradient —
which is the finding in §3.

**The fix.** Two directions added to the fill, both chosen so that neither can
darken the thing being complained about:

- `min(n.y, 0.0)` — only UNDERSIDES lose light. The sky-over-ground cue where it
  belongs, on overhangs, and zero on everything vertical.
- `dot(n, sun_horizontal)` — the sun's AZIMUTH with its vertical component
  removed. Exactly 0 on level ground, so the surface `u_ambientColor` was
  calibrated on does not move at all; it sweeps ±`FILL_SUN` around a bole with
  zero mean, so a trunk gains a lit side and a dark side without losing a luma
  of its own mean.

Recipe — one binary, two arms, `DFN_FILL_UP=0 DFN_FILL_SUN=0` being the control:

```
DFN_TOUR=1 DFN_TOUR_DIR=<dir> DFN_INTERNAL_RES=640x360 DFN_PALETTE=0 \
DFN_FLORA_PROBE=1 DFN_FLORA_EYE=620,878 DFN_FLORA_PITCH=0.40 \
DFN_WIND_FREEZE=120 DFN_CLOUD=0 DFN_TIME=0.62 [DFN_FILL_UP=0 DFN_FILL_SUN=0] \
  build_render/engine/app/dfn_app
```

Frames `render-fill-direction-{OFF,ON}-4d04fdb.png`, and the 6x zoom pair
`render-fill-direction-ZOOM6-4d04fdb.png` (left OFF, right ON).

| box | arm | p90/p10 | mean |
|---|---|---|---|
| whole frame | OFF | 3.75x | 92.34 |
| whole frame | **ON** | **4.19x** | 82.59 → held |
| the bole | OFF | 1.01x | 22.18 |
| the bole | **ON** | **1.16x** | **23.25 — brighter, not dimmer** |
| level ground | — | unchanged by construction (`dot(up, sun_horizontal) == 0`) | |

### TWO WRONG SHAPES WERE SHIPPED AND MEASURED FIRST, AND BOTH ARE WORTH KEEPING

1. `1 + up*n.y + sun*dot(n,s)` — zero-mean over a SPHERE of normals, and I wrote
   that it therefore could not move the frame's mean. **A frame is not a sphere
   of normals, it is mostly ground, and ground faces up:** whole-frame mean
   84.81 → 87.27, open ground 88.90 → 98.18. A 10 % brightening of the world
   smuggled in under a claim of preservation.
2. The same, divided by the fill an up-facing normal gets. That fixes the mean
   honestly, and then every normal that is NOT up can only lose light: the bole
   went 22.18 → 19.42 and the crown 68.21 → 59.72, i.e. **12 % darker**, for a
   bole p90/p10 of 1.01x → 1.05x. Physically defensible and **backwards for the
   defect** — the man is looking at trees that read as absences.

## 2. The shadow crawled because the texel snap was half a fix

The volume centre is `floor()`ed onto the texel lattice **in light space**, and
light space turns with the sun, so the lattice rotates under every receiver. The
eye's light-space coordinate is an ABSOLUTE world position (~1050 m at the
testbed's centre), so one frame of sun — 1.8e-5 rad at `DAY_LENGTH_SECONDS`
2880 — slides it 19 mm, an eighth of a texel, and the `floor()` crosses a
boundary every few frames. **Each crossing moves the entire shadow map one
texel, 0.156 m, at once.**

For scale: the sun's own motion in one frame displaces a 20 m caster's shadow by
**0.36 mm**. The grid was sliding seventy-five times that.

| quantum | mean slide/frame | median | steps ≥ half a texel |
|---|---|---|---|
| 0 (before) | 0.1720 texels | 0.0938 | **11.5 per second** |
| **0.00182 rad (shipped)** | **0.0037 texels** | **0.0000** | **0.1 per second** |

**The quantum is derived, not fitted**, which matters because it is the one
number here that could have been fitted: at a snap the shadow steps by the
quantum in ANGLE, so a receiver r metres away steps r × quantum and subtends the
quantum from the eye regardless of r. Budget half a pixel of the internal
target — 0.5 × fov_y 1.309 / 360 = **0.00182 rad**. The sweep agrees rather than
chooses: 0.0005 / 0.001 / 0.002 / 0.005 all take the median to zero.

### THIS COULD NOT BE MEASURED ON FRAMES, AND A 26-RUN SWEEP DIED PROVING IT

The defect lives BETWEEN two frames of ONE run, and every capture door in this
project takes one frame per process. A 13-step sun sweep × 2 arms was built and
gave a tidy-looking 1.5–3 % edge flicker per frame — and then its control ran:

**two IDENTICAL runs at the same sun position disagree by 32.83 % of the shadow
mask**, because streaming and LOD state differ every launch. Ten times the
effect under test. The whole sweep was noise, including the two 59 % outliers
that had looked like the signal.

So it is measured on the arithmetic instead — `tools/measure_shadow_jitter.cpp`,
mirroring `update_shadow` with the same glm, the same floats and the same
`floor()`. It is C++ rather than the usual stdlib Python (Rule 24) for one
reason: the quantity turns on a `floor()` of a float and double precision would
put boundary cases on the other side.

**The control should have been the first thing run, not the last.** Same family
as the running shimmer and the dungeon flicker: a defect between frames, and an
instrument that compares across processes.

## 3. What is NOT fixed, and whose it is

**The bole is a FIVE-SIDED PRISM, flat-shaded.** `FloraBuild::tube_segment` says
so in its own comment — "faces own vertices" — so a trunk presents two visible
faces and two flat normals. Pushed to the ceiling (`DFN_FILL_SUN=1.0`) it renders
as exactly two plateaus, 21.1 and 28.2, with a hard edge between them. **No
lighting term can draw a cylinder on geometry that has none**, which is why the
bole gains 1.01x → 1.16x and not more. Radial per-vertex normals would cost zero
triangles — the same mesh, only the normals averaged around each ring. Handed to
flora, who are rewriting exactly that file this afternoon.

**`SHADOW_TEXEL_M` = 0.156 m is untouched**, deliberately. "Большие квадратные
блоки" is a resolution question and resolution has a price: the near cascade
measured **+22 % frame time** for it (`render-R6b-near-cascade.md`) and ships
off. Snapping is free and was therefore done first, exactly as instructed. What
the snap cannot do is make the blocks smaller — those are two different defects
inside one sentence, and the third one is still open.
