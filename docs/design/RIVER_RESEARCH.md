<!--
Created: 09:08:2026 - 19:27:37
Last updated: 09:08:2026 - 19:27:37
-->
<!--
UPD:
- 09:08:2026 - 19:27:37: Research deliverable — survey of production and research river-generation techniques (Houdini erosion solvers, droplet/grid hydraulic erosion, D8/D-infinity flow accumulation, curvature-driven meander migration, braided channels, river-first terrain inversion), each costed against our streamed deterministic voxel pipeline; ranked staged recommendation (asymmetric curvature carve -> meander relaxation -> flow-accumulation tributaries -> floodplain belt -> optional amplification); implementer constraint checklist; explicit rejection list. No engine code touched.
-->

# RIVER_RESEARCH.md — how rivers are actually generated, and what we should take

**Status:** research only. No engine code was written or changed for this
document. Everything below is measured against the shipped P2 implementation in
`engine/world/sources/WorldgenHydrology.cpp` (build side) and
`engine/world/sources/WorldgenWater.cpp` (per-sample query side), and against
`docs/design/LANDSCAPE.md` §3 and `docs/specs/core.md`.

Driving request: *«нашёл интересный генератор реки / houdini simulation какой-то
есть, надо поискать, чтобы реку рисовать более естественно»* — the river should
look natural, and Houdini-style procedural/simulation approaches exist.

The short answer, stated up front so the rest can be read as evidence:

> **The thing that makes our river look artificial is not the lack of an erosion
> simulation. It is that our channel is a symmetric trapezoid whose lateral
> shape is value noise.** Real rivers read as real because their cross-section is
> *asymmetric in a way that correlates with the bend* — cut bank outside, point
> bar inside — and because their bends are the product of a lateral process, not
> of a wiggle function. Both are cheap, bounded, deterministic, and fit our
> existing station/query machinery. Erosion simulation is the expensive answer to
> a different question, and it is the one that fits our architecture worst.

---

## 0. What we do today, in the survey's own vocabulary

Reading our code against the literature, our P2 pass is already a recognisable —
if minimal — hydrology pipeline. Naming the parts correctly is what makes the
gaps visible.

| Our code | The literature calls it | What we are missing |
|---|---|---|
| `trace_descent` greedy 8-neighbour argmin on `grid.eff` | **D8 steepest-descent routing** (O'Callaghan & Mark 1984) | it traces *one* path, not a drainage network; no flow accumulation |
| pond-and-spill flood to the lowest saddle via a min-heap | **Priority-Flood depression filling** (Barnes et al. 2014), applied locally | we fill only the pits the single trace falls into, not the whole DEM |
| `ensure_sinuosity` — perpendicular value noise, 40 m feature size, amplitude grown 2→8 m until length ratio ≥ 1.15 | *nothing.* This is not a meander model | no curvature, no migration, no cutoffs, no oxbows |
| width `= (4 + 4·t)/2` half-width, `t` = normalised arclength | *nothing.* Real width follows **hydraulic geometry**, `W ∝ Q^b ∝ A^~0.5` | width is a ramp along the polyline, not a function of how much land drains in |
| trapezoid clamp `prof = t ≤ 0.5 ? 1 : (1−t)·2`, symmetric about the centreline | a symmetric **channel primitive** | strictly symmetric by construction — point bars and cut banks are *impossible* today, not merely absent |
| `w[i] = min(w[i−1], effective terrain)` | monotonic water surface — our hard invariant | fine; keep it |
| derived fords at corridor × trace crossings + `FORD_SPACING_MAX` gap fill | derived crossing points | fine; keep it, and keep it derived |
| coarse Dijkstra `dist_to_water` at 16 m | distance transform | fine |

Concrete numbers we are working inside (from `docs/NUMBERS.md`):
`WORLDGEN_HYDRO_GRID_STEP` 16 m, `RIVER_STATION_SPACING` 4 m,
`RIVER_SINUOSITY_MIN` **1.15**, `RIVER_WIDTH` 4→8 m, `RIVER_DEPTH` 1.5 m,
`RIVER_BANK_BLEND_FACTOR` 2×width, `FORD_DEPTH_MAX` 0.4 m, `FORD_SPAN` 6 m,
`SHORE_SAND_DIST` 6 m, `DIST_TO_WATER_RANGE` 150 m. Measured state: water
coverage ≈ 1.78 % of the world, hydrology context build ≈ 0.9 s at 21×21 chunks,
per-chunk generation ≈ 22–30 ms, voxel surface deviates from the heightfield by
≈ 2.3 cm mean.

Three structural facts about the current implementation drive every judgement
below:

1. **Hydrology is already a bounded world-level preprocess.** `build_hydrology`
   runs once inside `build_world_context`, over the whole domain at 16 m, and is
   cached by `ChunkManager`. A bounded preprocess is therefore *precedent, not a
   new architectural concept.* Memory scales as `(L/16)² × ~16 B`: 5.4 km domain
   ≈ 1.8 MB, 16 km ≈ 16 MB, 64 km ≈ 256 MB. The 16 m grid is the budget line.
2. **The per-sample side is stateless and O(1).** `water_sample_impl` does one
   nearest-station lookup in a 3×3 bin ring, then pure arithmetic. Chunk
   independence, the exact seam guarantee, and `generate_chunk` bit-identity all
   rest on this. Anything we add must land either as *more/better stations* or as
   *one more analytic term evaluated at the sample* — never as a per-chunk loop
   over the river.
3. **The world is voxel but the terrain is still a height function.** The chunk
   volume is built from the chunk's own heightmap and the extracted surface *is*
   the heightfield surface. So any change expressible as a height modification
   survives the voxel pipeline unchanged. Anything needing an overhang (a truly
   undercut cut bank) must go through the separate `WorldgenCarve` SDF-subtraction
   path — it cannot be smuggled into P2.

And one measured aesthetic fact: **1.15 is below the conventional threshold for a
meandering river.** The standard cutoff is sinuosity ≥ 1.5 (Leopold & Wolman
1957); below 1.5 a channel is classified "straight or sinuous". Our target is
literally set in the "not a meandering river" band, and it is reached by noise.

---

## 1. Survey

### 1.1 Houdini — what its erosion solvers actually compute

Houdini's terrain toolkit is heightfield-based (2D volumes = layers). The
relevant nodes:

- **HeightField Erode** ("Erode 3.0") — a grid-based iterative solver that runs
  hydraulic and thermal erosion over *frames*. It maintains multiple layers:
  `height`, `sediment` (water-deposited), `debris` (thermally weathered),
  `flow` (water magnitude) and `flow direction`. Hydro parameters are *Flow
  Force*, *Rainfall Coverage*, *Erosion Rate*, *Deposition Rate*, *Evaporation
  Rate*, *Erodability*; thermal parameters are *Weathering Force*, *Cut Angle*,
  *Repose Angle*. *Spread Iterations* controls how many material-transport
  iterations run per frame and, per SideFX, "strongly affects the terrain's look"
  — high values transport material further and level valleys.
  It runs in *frozen* mode (a whole range in one cook) or *iterative* mode
  (per-playbar-frame with state carried across frames).
- **HeightField Erode Hydro / Thermal / Precipitation** — the same solver split
  into single-purpose nodes.
- **HeightField Flow Field** — derives `flow` and `flow direction` layers from a
  height layer *without* simulating erosion. This is the cheap one, and it is
  the one whose output is most directly useful to us: it is a flow-routing
  operator, not a simulation.

What this means in practice: the beautiful Houdini terrain videos are **an
iterative, stateful, whole-grid sediment simulation run offline for hundreds to
thousands of iterations at high resolution**, whose *output is a baked
heightfield*. Rivers appear in it as an emergent consequence of where the water
went; they are not first-class objects, and their location is not knowable
without running the whole simulation.

SideFX documents the solver at parameter level, not algorithm level — the
internal scheme is not published. The description above (multi-layer, iterative,
grid-based) is what the docs state; I did not verify the numerical method.

### 1.2 Hydraulic erosion — the two families

**Grid / "virtual pipes" (shallow water).** Mei, Decaudin & Hu (2007) is the
canonical GPU formulation: each cell holds a water column; virtual pipes between
neighbours carry flux driven by height differences; the resulting velocity field
drives sediment capacity (`C = Kc · sin(α) · |v|`), erosion/deposition, and then
semi-Lagrangian sediment advection and evaporation. It is a proper PDE solve on a
regular grid; it produces coherent channel networks, deltas and deposition fans.
It is *the* method behind most terrain-tool erosion, Houdini's included in
spirit.

**Particle / droplet.** Musgrave, Kolb & Mace (1989) introduced erosion into
graphics terrain synthesis; the modern popular form is the droplet model from
Hans Theobald Beyer's 2015 TU München thesis (widely reimplemented, e.g. `erodr`,
Nick McDonald's and Job Talle's write-ups, Sebastian Lague's video): drop N
particles at random cells, each carries water + sediment, follows the bilinearly
interpolated gradient for a fixed step count, picks up material where capacity
exceeds load and deposits where it does not.

**Cost and fit for us.** Both are *iterations × cells*. The visual payoff is
gully/rill detail at the scale of the grid cell — which means the grid has to be
fine. At our 16 m grid, erosion features would be 16 m wide: too coarse to see as
anything but soft mush. To get 2 m detail we would need 64× the cells and then
hundreds of iterations on top: on a 16 km domain that is ~2.6·10⁸ cells ×
~500 iterations ≈ 10¹¹ cell-updates. That is an offline bake, measured in
minutes to hours, of a grid that does not fit in the memory budget.

Determinism: an iterative solver *is* deterministic given a fixed build and a
fixed serial evaluation order. The problem is fragility, not impossibility —
10⁸–10¹¹ accumulated float operations amplify any FMA/reassociation/parallel-
reduction difference chaotically, so the river's position becomes a function of
the compiler flags. Worse architecturally: **the output is a baked grid, not an
analytic field.** `terrain_height(ctx, world)` currently evaluates the world
per-point; a baked erosion grid replaces that with "sample and interpolate a
stored array", which we can do (`coarse_dist` already does it) but which turns
worldgen output data-heavy and makes the analytic seam guarantee an
interpolation guarantee instead.

### 1.3 Flow routing and drainage networks — D8, D-infinity, priority-flood, stream power

This is the cheap, well-understood, *non-simulation* half of hydrology, and it is
where our best value is.

- **D8** (O'Callaghan & Mark 1984): each cell drains to its lowest of 8
  neighbours. Simple; criticised for producing straight parallel flow paths on
  planar slopes because direction is quantised to 45°.
- **D-infinity** (Tarboton 1997): flow direction is a continuous angle taken from
  the steepest of the eight triangular facets in the 3×3 window, and flow is
  split between the two bracketing neighbours in inverse proportion to angle.
  Lower bias and mean-square error than D8 for contributing area.
- **Priority-Flood** (Barnes, Lehman & Mulla 2014): floods the DEM inward from
  the edges with a priority queue so every cell is guaranteed to drain; O(n) for
  integer, O(n log n) for float data. **Our pond-and-spill is this algorithm, run
  locally from a single pit.** Running it once over the whole grid is a strictly
  bounded extension of code we already have.
- **Flow accumulation**: with depressions filled and receivers known, contributing
  area `A` per cell comes from one topologically ordered pass.
- **Stream power** (`∂h/∂t = U − K·A^m·S^n`) with Braun & Willett's (2013) O(n)
  implicit stack ordering is the geomorphology-grade way to *evolve* terrain from
  uplift; Cordonnier et al. (2016) brought it into graphics ("Large Scale Terrain
  Generation from Tectonic Uplift and Fluvial Erosion"), generating dendritic
  networks, watersheds and ridges from a painted uplift map at low cost.

**What flow accumulation buys us visually, and nothing else does:** a river whose
**width is a function of drainage area** rather than of polyline length, and
**tributaries that join at plausible angles for free** — because a tributary
found by descent on the actual terrain necessarily arrives pointing downstream at
an acute angle. No junction-angle rule needs to be written; the angle is a
consequence.

### 1.4 Meander migration — curvature-driven lateral migration, cutoffs, oxbows

The classical kinematic model is Ikeda, Parker & Sawai (1981) / Howard & Knutson
(1984): local migration rate is a *weighted sum of upstream curvature*, which
reproduces the characteristic downstream skew of natural bends (bends lean
downstream; they are not sine waves). Sylvester, Durkin & Covault (2019, *Geology*
47(3):263–266, "High curvatures drive river meandering") showed from Landsat
time series in the Amazon basin that migration rate scales close to linearly with
local curvature, and published `meanderpy`, a compact reference implementation:
centreline resampled at fixed `deltas` (≈50 m), migration rate = `kl` × weighted
upstream curvature, channel width `W`, depth `D`, friction `Cf`, timestep `dt`;
**cutoff when two non-adjacent centreline nodes come within `crdist ≈ 2W`**, at
which point the loop is excised and retained as an oxbow with its own age.

The graphics-side state of the art is Paris, Guérin, Collon & Galin, *Authoring
and Simulating Meandering Rivers*, ACM TOG 42(6), SIGGRAPH Asia 2023: the same
physically-based migration equation **augmented with control terms** for (a) the
influence of landscape topography and (b) user-authored trajectory constraints,
plus explicit handling of the abrupt events — **cutoffs forming oxbow lakes, and
avulsions**. Source is MIT-licensed on GitHub (`aparis69/Meandering-rivers`),
though the released code is a rewrite and the authors warn results and timings
differ from the paper.

**Why this is the right shape for us.** It operates on a *polyline*, which is
exactly what `hydro.stations` already is. It is a bounded number of iterations
over a few thousand nodes — trivial next to our 0.9 s context build. It produces
precisely the visual vocabulary the request is asking for: bends with a coherent
wavelength that lean downstream, tight loops, and oxbow lakes as evidence that
the river has a history. And Paris's topography control term is the formal
version of "the river obeys the terrain rather than the terrain obeying a
spline".

Geometry to aim at (Leopold et al. 1964; Vermont DEC assessment protocol): meander
wavelength ≈ **10–14 channel widths**, radius of curvature / width modal value
**2–3**, meandering classification at sinuosity **≥ 1.5**. For our 4–8 m river:
wavelength ≈ 40–110 m, bend radius ≈ 10–25 m. Our 4 m station spacing resolves
that comfortably; our current 40 m noise feature size is in the right band by
accident, but its amplitude is a fitting parameter, not a bend.

### 1.5 Braided and anabranching channels

Multi-thread channels are classified against single-thread (straight/meandering)
by grain size, discharge and slope; braided (unstable multi-thread) versus
anabranching (stable multi-thread) is a further split (Kleinhans & van den Berg,
2011). Graphics work on braiding exists mainly as cellular braided-stream models
referenced by the meandering-rivers line of work; there is no widely adopted
graphics-side braided generator, and the topic sits behind patents in the
image-generation space.

**Fit for us: poor, and not for cost reasons.** Braiding requires a wide,
sediment-loaded, low-cohesion flat — an outwash plain or a glacial valley floor.
Our river is 4–8 m wide in a composed valley with a hamlet on it. Braiding at
that width reads as a swamp, not as a braid. It also multiplies the ford problem:
*n* threads = *n* crossings per corridor, and §3.1's "a river without fords is an
illegal hard wall" rule then has to hold *n* times. Park it; revisit only if a
glacial/outwash biome is ever designed.

### 1.6 The inversion — generate the river network first, grow the terrain around it

- **Génevaux, Galin, Guérin, Peytavie & Benes, "Terrain Generation Using
  Procedural Models Based on Hydrology", ACM TOG (SIGGRAPH 2013).** Rivers are
  the *modelling primitive*. A hierarchical drainage network is built as a
  geometric graph over the domain (expansion rules, Horton-Strahler ordering),
  watersheds are constructed from it, river types and trajectories are
  characterised, and only then is the terrain synthesised around the network from
  valley/hillslope primitives combined in a **construction tree**. The result is
  analytic and continuous — **evaluable per point, at any level of detail, with no
  precomputed grid**. That last property is exactly the property our engine wants.
- **Peytavie, Dupont, Guérin, Cortial, Benes, Gain & Galin, "Procedural
  Riverscapes", CGF 38(7) (Pacific Graphics 2019).** The non-inverted cousin:
  takes a bare-earth heightfield, derives hydrologically-inspired river network
  trajectories, **carves the riverbed into the terrain with width/depth/shape
  derived procedurally from the terrain and the river type**, and generates a
  matching real-time water-surface animation as a blend-flow tree. Fully
  automatic with optional interactive editing of trajectories and individual bed
  and flow primitives. (I could not fetch the full PDF — both mirrors exceeded the
  fetch size limit — so these specifics come from the published abstract and are
  flagged as such in §6.)
- **Schott, Paris, Fournier, Guérin & Galin, "Large-scale terrain authoring
  through interactive erosion simulation", ACM TOG (SIGGRAPH 2023)**, and
  **Schott, Galin, Guérin, Peytavie & Paris, "Terrain Amplification using
  Multi-scale Erosion", ACM TOG (SIGGRAPH 2024)** — the modern answer to "erosion
  detail without a global simulation": amplify an existing coarse terrain with
  erosion-derived detail at multiple scales.

The inversion is intellectually the best match to the complaint *"the river
should obey the terrain"* — because in it the terrain obeys the river, which is
what actually happens in nature at geological timescales. **But it is
incompatible with our terrain contract.** Our macro layer is composed, not
grown: the crag is an L0 landmark placed for the §1 visibility hierarchy, the
knoll and bluff are staged, the lake basin is stamped, the castle spur is
authored. Génevaux would generate all of that from the drainage graph and we
would lose every composition guarantee LANDSCAPE.md exists to enforce. What we
*can* take from it is the local idea: a **valley primitive blended around each
river station in a construction-tree fashion** — which is exactly stage 4 below.

### 1.7 What AAA actually shipped — the reality check

**Far Cry 5** (Etienne Carrier, GDC 2018, "Procedural World Generation of *Far
Cry 5*"; Houdini Engine tools embedded in the Ubisoft editor). Their freshwater
tool generated lakes, rivers, streams and waterfalls, and fed a water mask
downstream into the biome tool — a genuinely procedural ecosystem. But:

- **artists laid down a network of curves and splines to define rivers and other
  water bodies**; the procedural side generated the water surface, waterside
  assets and terrain texturing from the spline parameters;
- and among the lessons learned: *"sometimes manual control is preferred over
  automation (e.g. riverbed carving)"*.

So the flagship "procedural Houdini rivers" production case is **artist splines +
procedural dressing**, with automatic bed carving explicitly walked back. This is
worth internalising before anyone proposes a simulation: the industry's answer at
scale was *not* a solver. Our answer cannot be splines either — we have no
artists and the world is a function of a seed — but it means the bar we must
clear is "a well-shaped generated centreline dressed well", not "a physically
correct sediment budget".

---

## 2. Cost and fit, in our terms

Legend: **Preprocess** = whole-world pass in `build_world_context`; **Chunk** =
evaluable in `water_sample_impl`; **Det.** = survives our determinism contract;
**Voxel** = survives the SDF/surface-nets pipeline unchanged.

| Technique | What it adds visually that we lack | Where it runs | Whole heightfield resident? | Det. | Voxel | Verdict |
|---|---|---|---|---|---|---|
| **Asymmetric carve from station curvature** | point bars inside bends, cut banks outside; sand crescents for free via the existing shore mask | Preprocess (one float/station) + Chunk (O(1) extra math) | no — station list only | yes, pure function of existing data | yes, height-only | **Adopt first** |
| **Curvature-driven meander relaxation** (Howard–Knutson / Sylvester / Paris) | bends that lean downstream with a real wavelength; cutoffs; oxbow lakes | Preprocess, bounded iterations over ~10³ nodes | no — polyline only | yes with a *fixed* iteration count | yes | **Adopt second** |
| **Priority-Flood + D8 + flow accumulation** | tributaries at correct angles; width from drainage area; dendritic network | Preprocess, +2 arrays over the existing 16 m grid | already resident at 16 m | yes (heap ties by index, as today) | yes | **Adopt third** |
| **D-infinity** instead of D8 | slightly less directional bias in the far-field flow/moisture field | Preprocess | same | yes | yes | Optional polish; not visible at 16 m |
| **Floodplain / meander-belt valley primitive** (Génevaux-style, local) | the river reads as a *valley*, not a slot; terraces; a place for the hamlet | Preprocess (belt width/station) + Chunk (O(1)) | no | yes | yes | **Adopt fourth** |
| **Erosion-driven amplification** (Schott 2023/2024 spirit; Houdini Flow Field workflow) | gullies and rills on valley walls | Preprocess flow field (coarse) + per-chunk deterministic detail | coarse field only | yes *if* driven by a global field and pure per-position | yes | **Optional, last** |
| **Grid hydraulic erosion** (Mei et al.) as the terrain generator | emergent dendritic valleys, deposition fans | Preprocess only, and only at a resolution we cannot afford | **yes, at ≤4 m** — 64× our grid | fragile: 10⁸–10¹¹ accumulated float ops | output is a baked grid, not an analytic field | **Reject** |
| **Droplet erosion** | local rills; softening | same | yes | same fragility + RNG-ordered particle sequence | same | **Reject** as generator; possible as offline art tooling only |
| **Braided channels** | multi-thread river | Preprocess | no | yes | yes | **Reject for this valley** (wrong scale; *n*× the ford problem) |
| **River-first terrain inversion** (Génevaux full) | terrain that genuinely obeys the drainage network | Preprocess, whole domain | no (analytic!) | yes | yes | **Reject as the pipeline** — destroys the composed macro layer; take only its local valley primitive |
| **Houdini as a bake step** | whatever an artist makes | offline, outside the engine | n/a | n/a | n/a | **Reject** — kills seed-parametric worldgen |

Two cross-cutting notes:

- **Multiple river segments are nearly free on the query side.** `HydrologyData`
  already carries `segment_offsets`, and `water_sample_impl` takes the nearest
  station across all bins irrespective of segment. Tributaries therefore cost
  build-side work and new invariants, not new query machinery.
- **Everything in the "adopt" rows is a height modification.** None of it needs
  `WorldgenCarve`, none of it creates overhangs, and none of it changes the
  heightfield→SDF relationship the voxel stage depends on.

---

## 3. Ranked recommendation — cheapest convincing first

What "more natural" means, made concrete, so each stage can be judged:

1. bends that are not noise — coherent wavelength ~10–14 channel widths, leaning
   downstream, with a defensible radius-of-curvature/width ratio near 2–3;
2. **point bars** (gentle sandy shelf, inside of the bend) and **cut banks**
   (steep, deeper water, outside of the bend);
3. tributaries that join at acute, downstream-pointing angles;
4. a **floodplain** — a flat belt the river has clearly wandered across;
5. the river visibly obeying the terrain: it takes the low line, it widens where
   land drains into it, it straightens where it is steep.

### Stage 1 — Asymmetric cross-section from station curvature (**do this first**)

*Delivers: (2), and most of the perceived gain per unit of work in this document.*

Build side: for each station, compute a smoothed signed curvature from the
resampled polyline (a 3-point stencil at 4 m spacing, then a low-pass over
~5–9 stations — raw curvature after Chaikin + noise is not usable directly).
Store it alongside `half_width` and `carve_depth`.

Query side: replace the symmetric `prof` with a signed-lateral profile. Given the
signed offset of the sample across the centreline, the outer side (sign matching
the curvature) gets a steeper wall and the full depth carried closer to the bank;
the inner side gets a shallow shelf that rises *above* the water line over the
outer part of the half-width. Optionally bias the thalweg (deepest line) outward
by a fraction of the half-width.

Why this is the best first move:

- It is `O(1)` extra work per sample and one extra float per station.
- **The existing shore rule turns it into art for free.** The point-bar shelf is
  land within `SHORE_SAND_DIST` of water and under `SHORE_SAND_HEIGHT` above it,
  so §3.3 paints it sand automatically — a pale crescent on the inside of every
  bend, which is exactly the readable value contrast §3.3 already wants from
  fords. No new classification rule.
- The cut bank gives the outside of bends a hard edge and darker, deeper water —
  the single most recognisable "this is a real river" cue at walking distance.
- It does not touch the trace, so the monotonic invariant, the derived fords, the
  water coverage and every site score are unchanged by construction.

Non-negotiable detail: **asymmetry must fade to zero across the ford span.** The
trapezoid clamp today both cuts *and raises* the bed, and that raise is what
makes a ford a ford. An asymmetric profile must not reintroduce a deep outer
channel where a corridor crosses. Interpolate asymmetry to 0 over the same
`FORD_SPAN` ramp `carve_depth` already uses.

### Stage 2 — Replace the sinuosity jitter with a bounded meander relaxation

*Delivers: (1), plus oxbow lakes as a bonus landmark class.*

Take the Chaikin-smoothed descent polyline as the initial centreline and run a
**fixed** number of migration steps (start at 32–64; it is a tuning constant, not
a convergence loop):

- migration rate per node = `k ×` weighted sum of upstream curvature
  (Howard–Knutson; Sylvester et al. 2019 justify the near-linear curvature term);
- move each node along its local normal, then resample back to
  `RIVER_STATION_SPACING`;
- **topographic control term** (Paris et al. 2023): damp or veto a move whose
  destination terrain is higher than the station's water level plus a margin.
  This is the mechanism that makes the river hug the valley floor instead of
  swimming through a hillside, and it is what turns "spline on terrain" into
  "river in a valley";
- **cutoff detection**: when two non-adjacent nodes come within ≈ 2× channel
  width, excise the loop; emit the excised loop as an oxbow water body.

Then run the existing monotonic level solve — the current code order (jitter →
levels) is already correct and must be preserved.

Payoff and consequences:

- Sinuosity becomes a *measured output*, not an input the jitter is tuned to
  reach. **Recommend to design: change `RIVER_SINUOSITY_MIN` from a target the
  generator fabricates into a validation gate, and raise it toward 1.4–1.5** so a
  seed that produces a straight river is rejected rather than wiggled.
- Oxbows are the cheapest new POI class we will ever get: a still-water crescent,
  automatically ringed by willows under §5.9's "willow marks still water" rule,
  and automatically dressed by `dist_to_water`. They must be added to the
  distance-field seeding or the riparian split will not see them.
- Risk to watch: migration can push a station onto ground high enough that the
  clamp digs an unnaturally deep trench. Gate it — max carve below natural terrain
  ≤ some cap, else `ok = false`.

Cost: a few thousand nodes × ≤64 iterations ≈ 10⁵ operations. Immaterial against
the 0.9 s context build.

### Stage 3 — Priority-Flood + flow accumulation → tributaries and honest widths

*Delivers: (3) and half of (5).*

We already priority-flood locally and already run a Dijkstra over the same grid.
Extend to a full pass: fill the whole grid once (Barnes et al.), compute D8
receivers, accumulate contributing area in one topological pass, channelise above
an area threshold, and keep channels above a Strahler-order cut so the result is
a river with two or three tributaries rather than a hairball. Then:

- **width from drainage area** — `half_width ∝ A^~0.5`, clamped to the existing
  `RIVER_WIDTH_MIN..MAX` band — replacing `(WIDTH_MIN + (MAX−MIN)·t)`. The river
  is then wide *because land drains into it*, which is the whole of "the river
  obeys the terrain" in one line of code;
- tributaries fall out of the same descent code we already have, and their
  junction angles are correct without a junction rule.

New invariant required (state it before implementing): at a confluence the
tributary's final surface height must be ≥ the trunk's height at the junction,
and the trunk must remain non-increasing through it. The monotonic check becomes
per-segment plus a junction condition.

Fords must be re-derived across *all* segments — the existing corridor-crossing
code already loops stations, but the `FORD_SPACING_MAX` gap fill is written
against one continuous arclength and will need to be per-segment.

### Stage 4 — Floodplain / meander-belt valley primitive

*Delivers: (4) and the rest of (5).*

Stage 2 hands us, for free, the lateral envelope the centreline swept during
migration. Use it as a **belt half-width per station**, and apply a smooth,
shallow flattening of the terrain toward (water level + 0.5–2 m) inside the belt,
falling off to the natural macro height outside it — one more O(1) analytic term
in `water_sample_impl`, keyed to the same nearest-station lookup. That is the
Génevaux "valley primitive blended along the network" idea, scoped to one belt.

This is what stops the river reading as a groove cut into a hillside. It also
creates the flat, water-adjacent, buildable land that §3.4's settlement rule and
the P4 site scorer are already looking for — which is a benefit and a hazard:
**re-score the hamlet and check `BUILDING_WATER_MARGIN` against the new water and
the new grade** before calling it done.

### Stage 5 — Optional: erosion as *amplification*, never as the generator

If, after stages 1–4, valley walls still read as too smooth, add gully/rill detail
as a **deterministic per-position amplification** driven by a coarse global flow
field (Houdini's HeightField Flow Field workflow is the reference; Schott et al.
2024 is the research version). Hard requirement: it must be a pure function of
`(world position, seed, global coarse flow field)` with no iteration and no
neighbour state, so it stays chunk-local and seam-exact. If it cannot be written
that way, it does not ship.

---

## 4. Hard constraints — the checklist a candidate must pass

Any proposal below the line must be testable against every item. Numbered so
future work can cite them.

**C-1 Determinism (Rule 13.1).** Same params → byte-identical `.dfw`;
`generate_chunk(ctx, coord)` bit-identical to the full-run chunk. No unordered
container iteration; every heap/argmin tie broken by lowest index (the existing
`HeapEntry` pattern); **no convergence-tolerance loops** — iteration counts are
constants; no threading or SIMD reduction in any accumulation pass without a
fixed reduction order; randomness only through `WorldgenNoise`/`WorldGenRng`
streams.

**C-2 Bounded preprocess.** Hydrology stays a single pass in
`build_world_context` at ≤ the current 16 m grid resolution. A candidate that
needs a globally finer grid must state its memory at the largest planned domain
using `(L/step)² × bytes-per-cell` and get that number approved. Nothing may make
the preprocess depend on player position or streaming order.

**C-3 Chunk-local evaluation.** Every per-sample effect is computable from
`(world xz, station data, global coarse grids)` — no dependence on which chunk is
being generated and no neighbour-chunk state. Pinned by the existing exact-seam
test and by the `water_at` / `carve_height` identical-height test.

**C-4 Monotonic descent invariant.** `surface_height` non-increasing along every
segment; at a confluence the tributary level ≥ the trunk level at the junction.
Violation sets `ok = false` — a climbing river is a failed seed, never a shipped
one. The level solve stays *after* any centreline modification.

**C-5 Fords stay derived (§7.1a).** Fords are computed from the *final* geometry,
after migration and after tributaries — never tabled, never nudged to fit a site.
Every corridor × any-segment crossing yields a ford; the corridor mask stays
ford-shallow across its full width; per-segment gaps ≤ `FORD_SPACING_MAX`; max
water depth inside any corridor ≤ `FORD_DEPTH_MAX`. **Asymmetric carve and any
new lateral term must fade to zero across `FORD_SPAN`** so the bed-raise
guarantee is preserved by construction.

**C-6 Water coverage and the §3.3 mud cap.** Total water stays within budget
(1.78 % measured today — agree a ceiling with design before any stage lands). The
bed/mud band stays within `max(SHORE_SAND_DIST, 2 × local width)` of the water
edge. **Every `WaterBed` sample is covered by a drawable primitive** — oxbows and
any new pond-class body must ship a primitive, not just a fill level.

**C-7 Scatter invariants.** Zero instances in water; every scatter pass —
including forced ones — goes through `ScatterCtx::dry_enough` with the per-kind
margins. New water bodies must enter `dist_to_water` seeding, or the §5.9
birch/willow riparian split and the `BIRCH_WATER_DIST` band will not see them.
Point bars are sand: check that stone/bush scatter on a bar does not violate the
shore-band rules.

**C-8 Voxel survival.** Every change is a height modification, so the chunk SDF is
still built from the chunk's own heightmap and the extracted surface still *is*
the heightfield surface (current mean deviation ≈ 2.3 cm — it must not regress).
Anything requiring an overhang (a genuinely undercut bank) is a `WorldgenCarve`
P7 feature, proposed separately, never smuggled into P2.

**C-9 Derived-only placement holds.** Everything whose meaning depends on
generated geometry is derived from it: fords, dungeon entrances, riparian
vegetation, `CASTLE_FORD_COMMAND_DIST`. After any change to the trace, all of
them move — and the castle still commands *a* derived ford, the hamlet is still
within `SETTLEMENT_WATER_DIST` with `BUILDING_WATER_MARGIN` clearance over the
*new* water. Re-scored, never re-tabled.

**C-10 Performance.** Per-sample cost stays O(1) — one nearest-station lookup
plus arithmetic, never a loop over stations. Context build stays inside its
budget (0.9 s at 21×21 chunks today); per-chunk generation stays ≈ 30 ms.

**C-11 Failure is loud.** Every new invariant has an `ok = false` path and a test.
A seed that cannot satisfy them is rejected at generation time; nothing degrades
silently into a wrong-looking world.

---

## 5. What NOT to do

**Do not run an iterative erosion simulation as part of worldgen.** It is the
thing the impressive videos show and it is the worst fit we have: at a resolution
coarse enough to afford, its features are invisible; at a resolution fine enough
to see, it does not fit in memory or in the build budget; its output is a baked
grid rather than an analytic field, which weakens the per-point evaluation and
seam guarantees the whole streaming design rests on; and its determinism, while
technically achievable, becomes hostage to float reassociation across 10⁸+
accumulated operations. If we ever want erosion *detail*, take it as amplification
(stage 5), never as the process that decides where the river goes.

**Do not bake a Houdini (or any DCC) heightfield as the source of truth.** Our
world is a function of a seed. A baked terrain makes every worldgen change an
offline artist round-trip, breaks `generate_chunk` from params alone, and is
unbounded in size. Houdini is a fine place to *prototype* a look and read numbers
off; it is not a place our terrain can come from.

**Do not make the river a hand-authored spline.** This is what *Far Cry 5*
shipped and it worked for them — they had artists, a finite map, and an editor.
For us it fails the two things the request is actually about: the river would not
obey the terrain (it would be the terrain's boss), and the monotonic-descent
invariant would become a level-designer's problem instead of a generator's
guarantee. Note also their own lesson going the other way: manual riverbed carving
was preferred over their automated version — i.e. even with artists, the automatic
carve was the weak part. Ours has to be good.

**Do not run anything lateral or iterative per chunk.** Meandering, erosion,
sediment transport and flow accumulation are all non-local: two adjacent chunks
computing them independently will disagree, and the disagreement becomes a visible
crack in the extracted surface. Any such computation is a world-level preprocess
or it does not exist.

**Do not introduce convergence loops, adaptive timesteps, or tolerance-based
termination.** "Iterate until the change is below ε" is the classic
non-determinism vector: the iteration count becomes a function of the float
environment. Fixed counts only.

**Do not carve undercut/overhanging banks into P2.** The voxel volume is built
from the chunk heightmap; an overhang cannot be expressed there. It is possible
via `WorldgenCarve`'s SDF subtraction, but that is a separate, separately costed
feature with its own headroom and collision implications.

**Do not chase braided channels for this valley.** Wrong scale for a 4–8 m river,
wrong sediment regime for a composed pastoral valley, and it multiplies the ford
obligation by the number of threads.

**Do not adopt the full river-first inversion.** Génevaux et al. is excellent and
its analytic construction tree is genuinely the right kind of representation —
but it *generates* the terrain from the drainage network, and our macro layer is
composed to satisfy the §1 landmark hierarchy (L0 crag, staged knoll and bluff,
authored castle spur, stamped lake basin). Adopting it wholesale would trade every
composition guarantee for hydrological plausibility. Take the local valley
primitive; leave the inversion.

**Do not let any of this change water coverage, the mud cap, or scatter dryness
without re-measuring.** Those three have already produced shipped bugs (over-wide
mud flats, trees standing in a pond, a pine in the channel). Point bars,
floodplains and oxbows all touch exactly those systems.

---

## 6. Confidence and unverified claims

- **Verified from primary sources:** SideFX node documentation (Erode 3.0 layers
  and parameters, Flow Field, Erode Hydro/Thermal/Precipitation); Mei et al. 2007
  (title, venue, shallow-water/GPU approach); Musgrave et al. 1989; O'Callaghan &
  Mark 1984 and Tarboton 1997 (D8 / D-infinity definitions and the bias
  comparison); Barnes et al. 2014 (Priority-Flood, complexity); Braun & Willett
  2013 (O(n) implicit stream power); Génevaux et al. 2013 (venue, rivers-first
  framing, construction tree, analytic/continuous representation); Cordonnier et
  al. 2016 (uplift + stream power); Sylvester et al. 2019 (venue, curvature
  result) and `meanderpy` (model, parameters, cutoff at ≈2W, resampling); Paris
  et al. 2023 (venue, migration equation with control terms, cutoffs/oxbows and
  avulsions, MIT source); the meander geometry numbers (10–14 widths, R/W ≈ 2–3,
  sinuosity ≥ 1.5 = meandering).
- **Abstract-only, not read in full:** *Procedural Riverscapes* (Peytavie et al.
  2019) — both PDF mirrors exceeded the fetch size limit, so the pipeline
  description in §1.6 is from the published abstract. Treat its specifics as
  indicative. Likewise the Schott 2023/2024 papers are cited from the authors'
  publication list, not read.
- **Not verifiable:** the internal numerical scheme of Houdini's erode solver —
  SideFX documents parameters and layers, not the algorithm. §1.1's
  characterisation (grid-based, iterative, multi-layer) follows from the
  documented layers and *Spread Iterations* and is stated as such.
- **Second-hand:** the *Far Cry 5* spline detail comes from a well-known set of
  public notes on the GDC 2018 talk, not from the talk recording itself (GDC Vault
  is gated). The claim is consistent across the 80.lv coverage and the SideFX
  project page, but it is a transcription and should be treated as such.
- **My own estimates, not from sources:** all cost/memory figures in §0 and §2
  computed from our own constants and measured numbers; the erosion cell-update
  estimates in §1.2; the judgement that braiding reads as swamp at 4–8 m width.

---

## 7. Sources

Production / tools

- SideFX, HeightField Erode 3.0 — https://www.sidefx.com/docs/houdini/nodes/sop/heightfield_erode.html
- SideFX, HeightField Erode Hydro — https://www.sidefx.com/docs/houdini/nodes/sop/heightfield_erode_hydro.html
- SideFX, HeightField Erode Thermal — https://www.sidefx.com/docs/houdini/nodes/sop/heightfield_erode_thermal.html
- SideFX, HeightField Flow Field — https://www.sidefx.com/docs/houdini/nodes/sop/heightfield_flowfield.html
- SideFX, Erosion (heightfields guide) — https://www.sidefx.com/docs/houdini/heightfields/erosion.html
- Etienne Carrier, "Procedural World Generation of *Far Cry 5*", GDC 2018 — https://www.gdcvault.com/play/1025557/Procedural-World-Generation-of-Far ; notes: https://christianjmills.com/posts/procedural-tools-far-cry-5-notes/ ; coverage: https://80.lv/articles/houdini-procedural-world-generation-of-far-cry-5 ; SideFX project page: https://www.sidefx.com/community/far-cry-5/

Erosion

- Musgrave, Kolb & Mace, "The Synthesis and Rendering of Eroded Fractal Terrains", SIGGRAPH '89 — https://dl.acm.org/doi/10.1145/74333.74337
- Mei, Decaudin & Hu, "Fast Hydraulic Erosion Simulation and Visualization on GPU", PG 2007 — http://www-evasion.imag.fr/Publications/2007/MDH07/FastErosion_PG07.pdf
- Beyer, "Implementation of a method for hydraulic erosion", BSc thesis, TU München, 2015 — reference implementation: https://github.com/henrikglass/erodr ; write-up: https://nickmcd.me/2020/04/10/simple-particle-based-hydraulic-erosion/
- Schott, Paris, Fournier, Guérin & Galin, "Large-scale terrain authoring through interactive erosion simulation", ACM TOG (SIGGRAPH 2023) — https://aparis69.github.io/public_html/publications.html
- Schott, Galin, Guérin, Peytavie & Paris, "Terrain Amplification using Multi-scale Erosion", ACM TOG (SIGGRAPH 2024) — https://aparis69.github.io/public_html/publications.html

Flow routing and landscape evolution

- O'Callaghan & Mark 1984 (D8) and Tarboton 1997 (D-infinity) — overview and comparison: https://richdem.readthedocs.io/en/latest/flow_metrics.html ; Tarboton's own material: https://hydrology.usu.edu/dtarb/TarbotonBakerFinal.pdf
- Barnes, Lehman & Mulla, "Priority-Flood: An Optimal Depression-Filling and Watershed-Labeling Algorithm for Digital Elevation Models", *Computers & Geosciences* 62 (2014) — https://arxiv.org/abs/1511.04463
- Braun & Willett, "A very efficient O(n), implicit and parallel method to solve the stream power equation…", *Geomorphology* 180–181 (2013) — https://www.sciencedirect.com/science/article/abs/pii/S0169555X12004618 ; FastScape: https://fastscape.org/fastscapelib-fortran/

Meandering

- Sylvester, Durkin & Covault, "High curvatures drive river meandering", *Geology* 47(3), 2019 — https://pubs.geoscienceworld.org/gsa/geology/article/47/3/263/568705/High-curvatures-drive-river-meandering ; preprint: https://eartharxiv.org/9f2px/
- `meanderpy` (Howard & Knutson kinematic model, cutoffs, oxbows) — https://github.com/zsylvester/meanderpy
- Paris, Guérin, Collon & Galin, "Authoring and Simulating Meandering Rivers", ACM TOG 42(6), SIGGRAPH Asia 2023 — https://dl.acm.org/doi/10.1145/3618350 ; code (MIT): https://github.com/aparis69/Meandering-rivers ; video: https://www.youtube.com/watch?v=I9oWP34ZmcA
- Meander geometry (wavelength 10–14 widths; R/W ≈ 2–3; sinuosity ≥ 1.5 = meandering, Leopold & Wolman 1957) — https://dec.vermont.gov/sites/dec/files/wsm/rivers/docs/assessment-protocol-appendices/H-Appendix-H-04-Meander-Geometry.pdf ; Leopold, "River Meanders" (1966): https://facultyweb.kennesaw.edu/jdirnber/docs/Leopold%20Riv%20Meand%201966.pdf
- Point bar / cut bank / helical flow — https://geo.libretexts.org/Bookshelves/Geography_(Physical)/The_Environment_of_the_Earth's_Surface_(Southard)/05:_Rivers/5.09:_Morphology_and_Dynamics_of_Meandering_Streams

River-first and river-network terrain

- Génevaux, Galin, Guérin, Peytavie & Benes, "Terrain Generation Using Procedural Models Based on Hydrology", ACM TOG (SIGGRAPH 2013) — https://dl.acm.org/doi/10.1145/2461912.2461996 ; PDF: https://www.cs.purdue.edu/cgvlab/www/resources/papers/Genevaux-ACM_Trans_Graph-2013-Terrain_Generation_Using_Procedural_Models_Based_on_Hydrology.pdf ; video: https://www.youtube.com/watch?v=JCsj0v-wmIM
- Peytavie, Dupont, Guérin, Cortial, Benes, Gain & Galin, "Procedural Riverscapes", CGF 38(7), Pacific Graphics 2019 — https://perso.liris.cnrs.fr/eric.galin/Articles/2019-riverscapes.pdf ; video: https://www.youtube.com/watch?v=Xe2bjYaPMfY
- Cordonnier, Braun, Cani, Benes, Galin, Peytavie & Guérin, "Large Scale Terrain Generation from Tectonic Uplift and Fluvial Erosion", CGF 35(2), EG 2016 — https://onlinelibrary.wiley.com/doi/10.1111/cgf.12820

Channel patterns

- Kleinhans & van den Berg, "River channel and bar patterns explained and predicted…"/"Channel patterns: braided, anabranching, and single-thread", *Geomorphology* (2010) — https://www.sciencedirect.com/science/article/abs/pii/S0169555X10001893
