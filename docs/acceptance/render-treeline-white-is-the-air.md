<!--
Created: 13:08:2026 - 18:05:00
Last updated: 13:08:2026 - 18:05:00
-->
<!--
UPD:
- 13:08:2026 - 18:05:00: Created. The white treeline is the AIR — haze plus the R2
  mist band — and nothing about it is foliage-specific. Overturns two earlier
  diagnoses with same-frame control arms: flora's "not haze, the hue survives"
  (g-b is the insensitive statistic; b/r reads 1.035 against the leaf's 0.54)
  and this zone's own 12.08 entry blaming alpha-to-coverage (worth a tenth of
  it). No code changed: the levers are two NUMBERS constants owned by the lead,
  and they are handed up with numbers rather than retuned here.
-->

# THE WHITE TREELINE IS THE AIR

**Verdict: it is aerial perspective plus the R2 mist band. There is no lighting
term applied to the foliage program at range — with the air off, flora's own
400 m frame is an ordinary dark green forest.** No code changed; the two levers
are `HAZE_SCALE_LENGTH` and `MIST_BAND_*`, both NUMBERS constants the lead
chose from frames (Rule 14), and this file exists to hand them a number they
did not have.

Frames (all `a976569`):
`render-treeline-AIR-SHIPPED` · `render-treeline-AIR-HAZE-ONLY` ·
`render-treeline-AIR-ALL-OFF` · `render-treeline-EYE1m7-SHIPPED` ·
`render-treeline-EYE1m7-AIR-OFF`.

## 1. The recipe — one binary, four doses, two vantages

```
# flora's distant vantage (eye 70 m, 400 m back) — their own pose, y/z/pitch edited
DFN_RESTORE=<speckle pose, pos_y=70 pos_z=1180 pitch=-0.08> \
DFN_CAPTURE_DIR=<dir> DFN_CAPTURE_AFTER=6 DFN_WIND_FREEZE=120 DFN_NULL_PHYSICS=1 \
DFN_INTERNAL_RES=640x360 DFN_PALETTE=0 DFN_TIME=0.5 DFN_MENU=0 \
[DFN_HAZE=1e8] [DFN_MIST=0] [DFN_MSAA=0]  build_render/engine/app/dfn_app

# the player's own eye height
DFN_TOUR=1 DFN_TOUR_DIR=<dir> DFN_FLORA_PROBE=2 DFN_WIND_FREEZE=120 DFN_CLOUD=0 \
DFN_INTERNAL_RES=640x360 DFN_PALETTE=0 [same three doses]
```

**`DFN_HAZE` IS A LENGTH, NOT A DOSE** (default 600 m): `DFN_HAZE=0` is
*infinite* haze and whites out the entire frame. Off is `1e8`. One arm was
thrown away learning this. **And `DFN_HAZE=1e8` silently disables the MIST BAND
too**, because both terms divide by the same `L` in
`dfn_aerial_transmittance` — so "haze off" and "all air off" are the same arm,
and isolating the two needs `DFN_MIST=0` as a separate dose.

## 2. The statistic, and why the earlier ones missed it

Measured on the greenest tenth of the canopy band — **flora's own instrument**,
so the disagreement is not about the sampling.

| statistic | leaf | sky | shipped canopy at 400 m | verdict |
|---|---|---|---|---|
| g − b | **+64** | −12 | **+21** | 3/4 of the green lead already gone |
| **b / r** | **0.54** | 1.13 | **1.035** | **indistinguishable from sky** |

flora read `g − b` = +14 at 400 m against +22 at 5–30 m and concluded the hue
survives. The comparison is the flaw, not the arithmetic: **the near band is not
a control**, it is a different population at a different distance and height.
Against the leaf's own +64 — which only an arm can give — three quarters of the
hue is gone. And `b/r` is the sensitive statistic here because the sky's blue is
the strongest single signal in the mix; it reads 1.035, i.e. sky.

## 3. The decomposition — one term removed at a time

Canopy band, greenest tenth. `b/r`: leaf 0.54, sky 1.13.

| arm | flora's vantage (eye 70 m) | player's eye (1.7 m) |
|---|---|---|
| SHIPPED | **1.035** | **0.943** |
| − coverage AA (`DFN_MSAA=0`) | 1.006 | 0.908 |
| − mist band (`DFN_MIST=0`) | 0.847 | 0.943 *(no change)* |
| − haze **and** mist (`DFN_HAZE=1e8`) | 0.549 | 0.639 |
| − all three | **0.540** = the leaf | **0.578** = the leaf |

- **Haze is the whole defect at the player's eye height.** The mist band moves
  it by 0.000, correctly: the band lives at 54–86 m and a level ray from 1.7 m
  never enters it.
- **The mist band roughly doubles the air at 70 m**, where the eye sits inside
  it (`MIST_BAND_HEIGHT` 70 m, `_THICKNESS` 32 m → 54–86 m, `_DENSITY` 4 = five
  times the ground air). This band has now eaten two probes: the sky probe at
  84 m (11.08) and flora's oak frame at 70 m.
- **Coverage AA is a distant third** — 0.943 → 0.908 at the player's eye. Its
  real effect is on the DISTRIBUTION, not the value, and that much is exactly
  what the 12.08 entry found: pixels in the middle of the leaf→sky line go
  **6.6 % → 16.0 %** of the band while the band MEAN is preserved to within one
  luma unit (78.7/102.8/124.2 against 79.0/103.0/125.4). It veils the canopy's
  edge; it does not raise its value.

## 4. Nothing here is foliage-specific

The claim under test was "a lighting term applied to the foliage program at
range, multiplicatively". It is not. With coverage AA held OFF in both arms so
canopy porosity cannot move, the fraction each surface is dragged toward the sky:

| surface (same frame, eye 70 m) | haze + mist | haze only |
|---|---|---|
| canopy crowns | 0.72 | 0.38 |
| ground at the trees' foot | 0.47 | 0.25 |
| ground, mid distance | 0.34 | 0.18 |

The mist band multiplies **ground and canopy alike** by the same ~1.9x. The
canopy is not treated differently — it merely SHOWS the same fractional drag far
more, because it is a dark, finely perforated mass against a bright sky, while
the ground is already bright and sits at low contrast against it. That is also
why the ground "looks fine" in the shipped frame while the trees look like snow.

## 5. For the lead — the two numbers, with what they cost

Not retuned here (Rule 14: both are NUMBERS constants, both were chosen from
frames by the lead, and neither choice was made while looking at a treeline).

- **`HAZE_SCALE_LENGTH` = 600 m.** At the ranges the player looks at a treeline
  this puts **28 % of the sky into the canopy at 200 m** and 37 % at 400 m. The
  600 m decision was argued from valley-depth and silhouette cues on massif
  frames; the canopy is the surface that pays for it, and it pays 3/4 of its hue.
- **`MIST_BAND_HEIGHT` 70 m / `_THICKNESS` 32 m / `_DENSITY` 4.** Inside the
  band the air is five times ground density over the same 600 m scale, i.e. an
  e-folding at 120 m: **standing in the band you can see 120 m.** The band is
  reachable — hills, the crag, any tower — and two probes have already been
  taken from inside it without noticing.

**What is NOT the answer: reverting coverage AA.** It costs a tenth of the
defect and it is what bought the running-shimmer fix.
