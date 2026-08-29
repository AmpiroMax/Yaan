
## 3. Water

Water does not exist in the engine yet; this section is the contract for its
first implementation (worldgen data + render/physics consumers decide their
own representations).

### 3.1 Rivers — must flow downhill

Compatible with our heightmap pipeline (fBm has no drainage; we impose it):

1. **Source:** deterministic argmax of the macro heightfield within the L0
   massif footprint, on a coarse 16 m grid.
2. **Descent trace:** greedy steepest-descent on the coarse grid with momentum
   (previous direction weighted in) to avoid zigzag; on hitting a local
   minimum: pond-and-spill (fill to the lowest saddle, continue from the
   spill) — Amit Patel's mapgen approach adapted to grids. Trace ends at the
   region/testbed edge or a lake.
3. **Smooth & resample:** Chaikin-smooth the polyline, resample stations every
   4 m; enforce target sinuosity ≥ 1.15 by amplitude of smoothing noise.
4. **Monotonic water surface, with flat reaches (user-ratified, грилл в23;
   semantics ratified stage-5 on core's diagnosis):** station water height =
   min(previous station, local terrain) — EXCEPT through a pond, because
   **a pond is not a separate water body: it is a FLAT REACH of the river**
   (плёс). The monotone pass does not descend *through* a pond; it goes flat
   across it and resumes min-descent from the reach level at the spill.
   **Reach level = min(spill-saddle level, the water level at which the river
   ENTERS the pond)** — every station inside the pond takes exactly that
   level, and pond cells whose terrain rises above the lowered level drain
   (the footprint shrinks; that is the rule working, not a defect). The
   invariant, sharpened rather than changed: **the water surface never gains
   height downstream, is CONSTANT across any standing body it passes through,
   and a pond's drawn level equals its swum level BY CONSTRUCTION — there is
   one authority for «where is the water», the reach level, and a pond whose
   level exceeds the entering river's level is unconstructible, not merely
   wrong.** A river that climbs is a failed seed; a pond storing a level of
   its own is the same failure. And because the fill level is an INPUT to the
   carve (the pond bed is cut from the reach level), moving the water moves
   the ground under it in the same pass, never as a follow-up — this is the
   lesson of the 7.98 m pond, stated as construction rather than as repair.
   Control (Rule 30): the pre-fix construction, which produced a pond 7.98 m
   above the river draining it, must FAIL this statement; the flat-reach
   construction cannot produce it (core ships that pair with the change).
5. **Carve:** trapezoid cross-section, depth `RIVER_DEPTH` = 1.5 m, width
   `RIVER_WIDTH` = 4–8 m growing from source to mouth, bank blend 2× width
   **(предложение — утвердить)**.
6. **Fords:** wherever a corridor (§2.4) crosses the river, *raise* the bed —
   a true raise, clamping the bed into the trapezoid band, not merely a limit
   on the carve — so depth ≤ `FORD_DEPTH_MAX` = 0.4 m. The ford-shallow zone
   covers **every station inside the corridor mask**, not just the crossing
   station: oblique crossings otherwise dip into neighboring stations' blend
   zones (stage-3b measured 0.48 m exactly that way). Additionally at least
   one ford per `FORD_SPACING` = 200–400 m of river length
   **(предложение — утвердить)**. Validation invariant (tested):
   max water depth inside any corridor = `FORD_DEPTH_MAX`. Until swimming
   exists (FUTURE), a river without fords is an illegal hard wall — rivers
   *shape* routes, never sever the POI graph.

### 3.2 Lakes and ponds

- A lake is a stamped basin: radial depression to `LAKE_DEPTH_MAX` = 4 m with
  a flat water plane at its rim-min height; the river may terminate in it or
  exit at its spill point. **Where the river flows THROUGH a lake, the plane
  obeys the same reach rule as a pond — plane = min(rim-min, the river's
  entry level)** — otherwise the lake rebuilds the exact defect the flat
  reach just removed, one water body over. Ponds are **no longer stamped
  basins: a pond is a flat reach of the river (§3.1 step 4)** — it has no
  level of its own, only the reach's — at radius 5–15 m.
- Testbed carries 1 lake (§7); region density FUTURE (needs the moisture
  field).
- Lake shores are prime real estate: the settlement rule (§3.4) and shore
  treatment (§3.3) both key off the same dist-to-water field.

### 3.3 Shoreline treatment

P3 computes per-sample `dist_to_water` (horizontal) and `height_above_water`.
Shore mask = `dist_to_water ≤ 6 m AND height_above_water ≤ 0.6 m`
**(предложение — утвердить: `SHORE_SAND_DIST` = 6 m,
`SHORE_SAND_HEIGHT` = 0.6 m)** → sand splat (§4), no grass micro, loose
stones ×2 density. Fords widen the sand band to both banks — the sand patch
*is* the ford's visual signpost (readable value contrast at distance: pale
sand vs dark water).

**Bed/mud band is hard-capped.** The full non-grass water treatment is:
submerged terrain (bed) + the sand band above — nothing else. Any
bed/mud/"WaterBed" classification band must stay within
`max(SHORE_SAND_DIST, 2 × local river width)` of the water edge
(≤ 16 m at max testbed width). Render's stage-3b probe measured the current
classifier at 2.74 % of the world against ~1 % actual water, producing
30–60 m mud flats that dominate riverside compositions — that is a violation
of this cap, fix on the classifier side (core). Wide dry flats are a *desert*
biome tool (FUTURE), never a default river treatment.

> **⚠ THE 2.74 % READING IS SUSPECT AND MUST BE RE-MEASURED (core's pond-fill
> fix, stage-4).** Core found and fixed a **16.6× duplication** in the pond
> fill — the testbed's 17,336 lake planes are now **1,043**, and **94.5 % of
> water cells were carrying multiple coplanar planes at conflicting heights.**
> So the drawn water surface could disagree with `water_surface_at`, and
> **every §3 figure measured against drawn water before that fix is
> provenance-dead**, exactly like the slope histogram measured on the old dome:
> - the **2.74 % WaterBed coverage** above,
> - anything derived from `height_above_water` near the shore, including §4's
>   `SHORE_SAND_HEIGHT` = 0.6 m band,
> - the §2.7 finding that ±0.3–0.6 m of micro-relief «dropped bank dips under
>   the water surface» — which may have been reading a *duplicated* plane at
>   the wrong height rather than the real one.
>
> **The cap itself does not move** — it is derived from `SHORE_SAND_DIST` and
> river width, not from the measurement. What is withdrawn is the *evidence of
> violation*, and it must be re-taken before anyone tunes the classifier
> against it. **Report the re-measured coverage; do not assume it merely
> shrank.**

**`dist_to_water` field range.** The P3 field must be valid to at least
`SETTLEMENT_WATER_DIST` (150 m) — the P4 site scorer and §3.4 rules consume
it at that range. A 60 m saturation cap (observed in stage-3b probes) breaks
settlement scoring; saturate at ≥ 150 m **(предложение — утвердить:
`DIST_TO_WATER_RANGE` = 150 m minimum)**.

### 3.4 Water and gameplay placement

- Settlements stand within `SETTLEMENT_WATER_DIST` = 150 m of fresh water
  **(предложение — утвердить)**; the P4 site scorer weights river-bend
  outer banks and lake shores highest (flat, water-adjacent, defensible view).
- Water is a natural POI-chain edge: a ford or a bridge (FUTURE) is itself an
  L2 guide.
- Dungeons love water edges (lakeshore cave) — 1 of the 3 testbed dungeons
  keys to water (§7).

---

