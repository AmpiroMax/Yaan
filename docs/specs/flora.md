<!--
Created: 09:08:2026 - 19:02:07
Last updated: 09:08:2026 - 19:13:19
-->
<!--
UPD:
- 09:08:2026 - 19:02:07: Created the flora zone spec (stage 4, user request в38
                         "параметрическая система ветвления, породы описываются
                         данными"): parametric branching system, species
                         parameter sets, neighbour interaction, LOD ladder, and
                         the cross-zone consequences of the tall-tree ruling
                         (design §5.7).
- 09:08:2026 - 19:12:54: Folded in the same-session cross-zone results: design's
                         §5.8/§5.9 rulings (canopy BAND + CANOPY_TRUNK_PATH_MAX,
                         near-half wedge ban, maturity mix, snag + willow
                         approved, giants folded into the maturity tier); core's
                         peak-height sweep and its corrected 52-64 m scope, plus
                         flora's finding that the crag's flank relief is
                         expressed as a FRACTION of peak height (the coupling
                         that makes the "dead end" look structural); sim's
                         capsule ruling, the missing static-capsule call, the
                         no-jump consequence for logs, and the 16384 body
                         budget; the user's forest-floor brief (fallen trees,
                         big bushes, more dead wood, small trees very rare).
- 09:08:2026 - 19:13:19: Peak-height dead end RESOLVED and the record corrected:
                         core refuted flora's ridge_amp_frac hypothesis, then
                         found the real defect (the C1 test counted the crag as
                         its own occluder, forcing 0.000 above ~60 m). Clearance
                         RISES with peak height (0.751 -> 0.915 over 52-200 m);
                         the taller canopy never broke C1 (0.751 vs 0.60 floor),
                         so the -0.048/+0.011 deltas are void and marked as such.
                         LANDSCAPE 1.3's "do not retry" note is void.
-->

# Flora — tree and plant geometry (agent spec)

Seven sections per the agent-spec contract (Q35). This document is the durable
knowledge of the zone: **agent sessions die, specs do not.** A successor with
only this file must be able to continue without re-deriving anything.

**One-line zone statement:** design owns WHICH species exist and WHERE they may
stand; core owns the pass that places instances; render owns drawing them;
**flora owns what a tree IS as geometry** — the branching system and the
per-species parameter sets.

---

## 1. Zone of responsibility

Per Rule 25 the code lives inside `engine/render`, which is render's directory.
The boundary was negotiated directly with the render agent (Rule 25: "needing a
change there → message the owner"); the agreement is recorded in §4.

**Files owned by flora:**

| File | Contents |
|---|---|
| `engine/render/sources/FloraSpecies.h` | `SpeciesParams` struct, `FloraSpecies` enum, envelope/foliage enums |
| `engine/render/sources/FloraSpecies.cpp` | the per-species parameter tables (§3.2) |
| `engine/render/sources/ProcFlora.h` | public builder API (§2) |
| `engine/render/sources/ProcFlora.cpp` | skeleton growth + mesh emission (§3.1) |
| `tests/render/ProcFloraTests.cpp` | the invariant suite (§6) |

**Not owned, must not be edited without the owner's ack:** `ProcMesh.{h,cpp}`,
`ScatterBatcher.{h,cpp}`, `RenderSystem`, `engine/render/CMakeLists.txt`,
`tests/render.cmake` (render); anything under `engine/world` (core);
`docs/design/LANDSCAPE.md` (design); `docs/NUMBERS.md` (lead).

**What flora decides:** trunk and branch geometry, branch placement rules,
foliage cluster shape and count, silhouette envelopes, per-species numbers,
per-instance variation, neighbour response (crown shyness, lean, understory),
the LOD ladder's *geometry* at each level, triangle spend.

**What flora does not decide:** see §7.

---

## 2. Public interface

Frozen for the stage once render acks (Rule 26). Everything is a pure,
deterministic function — no GPU, no ECS, no globals, no file IO.

```cpp
namespace dfn::render {

/// Approved catalog (design, §5.8/§5.9). NOTE: there is no ElderOak — a giant
/// is DaleOak with maturity > 1, one system rather than two (design's ruling).
enum class FloraSpecies : uint8_t {
    DaleOak, HighlandPine, RiverBirch, ValeWillow, Snag,
    Bush, BigBush, FallenLog, Deadfall,
};

enum class FloraLod : uint8_t { Full, Reduced, Silhouette };

/// Per-instance shape modifiers. Computed from the neighbourhood, NOT from
/// core's ScatterInstance (which is frozen — Rule 26 — and gains no fields).
struct FloraShape {
    float maturity   = 1.0f;          ///< 0.35 sapling .. 1.5 elder; scales height
    glm::vec2 lean_dir{0.0f};         ///< unit, direction the crown leans toward
    float lean       = 0.0f;          ///< rad, 0 .. FLORA_LEAN_MAX
    glm::vec2 shy_dir{0.0f};          ///< unit, direction of strongest crowding
    float shyness    = 0.0f;          ///< 0..1, crown pullback along shy_dir
    bool  understory = false;         ///< raise crown base, narrow crown, shorten
};

/// The canonical builder. Deterministic in (species, variant, shape, lod).
[[nodiscard]] MeshData build_flora_mesh(FloraSpecies species, uint32_t variant,
                                        const FloraShape& shape, FloraLod lod);

/// Neighbour analysis: derives a FloraShape for every tree instance in `all`
/// (a chunk's instances plus, optionally, a border margin from neighbours).
/// Pure; O(n) with a uniform grid.
[[nodiscard]] std::vector<FloraShape>
analyse_neighbourhood(std::span<const math::ScatterInstance> all);

/// Species metadata other zones may need without pulling in the tables.
[[nodiscard]] float species_nominal_height(FloraSpecies);   ///< m
[[nodiscard]] float species_crown_radius(FloraSpecies);     ///< m, nominal
[[nodiscard]] float species_crown_base(FloraSpecies);       ///< m, nominal
[[nodiscard]] float species_trunk_radius(FloraSpecies);     ///< m, at the base

/// Number of pre-built skeleton variants per species (variant = hash % this).
inline constexpr uint32_t FLORA_VARIANTS = 12;

} // namespace dfn::render
```

`variant` is `hash(quantized world position) % FLORA_VARIANTS`, chosen by the
caller. Twelve skeletons per species is the whole trick that makes unique-looking
forests affordable: cost is O(species × variants × lods), not O(instances), while
`FloraShape` supplies the per-instance difference for free during the bake.

---

## 3. Internal design

### 3.1 The generator — four stages

**Stage A — trunk.** A tapered polygonal tube swept along a curve. Radius at
parameter `t` is `r_base * (1 - t)^taper_exp` clamped so the top never falls
below the shadow floor (§3.5). `trunk_sweep` bends the axis by a constant rate,
so a trunk is an arc, never a stick. `trunk_count` > 1 splits the base into a
clump: stems start on a circle of `trunk_spread` and converge-then-diverge, each
with its own sweep phase. Multi-stem is a *number*, not special-case code — this
is the user's "сколько стволов".

**Stage B — branch skeleton.** Recursive, `generations` deep. For each parent
segment, children attach between `branch_start_frac[g]` and 1.0 along it.
Azimuths follow the golden angle (137.5°) plus jitter — a regular fan reads as a
radio mast, and phyllotaxis is what makes the difference at no cost. Conifers
override this with **whorls**: N branches at the same height, repeated at a
fixed vertical interval (that is what a pine actually is, and it is what makes
the tiered silhouette). Per child: pitch `branch_angle[g]` from the parent axis,
length `parent_len * length_decay[g]`, radius `parent_radius_at_attach *
radius_ratio[g]`.

Each branch is then swept in short segments, and two forces bend it as it grows
(both are per-unit-length blends of the direction vector, so they accumulate
along the branch and produce curves, not kinks):

- **Phototropism** — blend toward `+Y`. Inside a stand light comes from above;
  for an edge tree (`shy_dir` present) it blends toward the open side instead.
  This is why edge trees lean out over a clearing, which is the readable signal
  that a forest has an edge.
- **Gravity droop** — blend toward `-Y`, scaled by distance from the branch base
  and *inversely* by current radius: thin tips droop, thick limbs do not. A
  negative coefficient gives conifer upsweep. A large positive coefficient with a
  Weeping envelope gives a willow. One number, three tree families.

**Stage C — foliage.** Clusters at terminal tips, plus optional clusters along
the last woody generation for dense species. Cluster shape per species:
`Blob` (faceted ellipsoid), `ConeShell` (a tier skirt for conifers), `Fan`
(flat cards for sparse crowns). Clusters are the cheapest triangles that carry
value, and they are what casts the shadow (§3.5).

**Stage D — silhouette envelope.** The part that makes this survive 640×360.
Branch target lengths are clipped so tips land on a species envelope
(`Sphere`, `Cone`, `Vase`, `Column`, `Weeping`), parameterised by
`crown_base_frac` and `crown_width_frac`. The skeleton supplies structure; the
envelope *guarantees* the species reads at 8 px (LANDSCAPE §1.5). **Emergent
silhouettes are unreliable and we cannot afford unreliability at this
resolution** — this is the single most important design decision in the zone.

### 3.2 Species = a set of numbers + a silhouette intent

The parameter struct (abbreviated; the full field list is `SpeciesParams` in
`FloraSpecies.h`):

| Group | Fields |
|---|---|
| Trunk | `height_min/max`, `trunk_radius_frac`, `taper_exp`, `trunk_sweep`, `trunk_count_min/max`, `trunk_spread`, `trunk_sides`, `trunk_segments` |
| Envelope | `envelope`, `crown_base_frac`, `crown_width_frac` |
| Branching | `generations`, `branch_count[]`, `branch_angle[]`, `branch_start_frac[]`, `length_decay[]`, `radius_ratio[]`, `whorled`, `phototropism`, `droop`, `min_branch_diameter` |
| Foliage | `foliage_shape`, `cluster_count`, `cluster_radius_frac`, `cluster_slices`, `cluster_bands` |
| Value | `trunk_color`, `foliage_color`, `foliage_color_alt` |
| Neighbours | `shyness`, `lean_response` |

Working values for the three catalog species, derived from design's §5.7 ruling
(**reference the NUMBERS.md constant names in code, never these literals** —
they are here so a successor can see the shape of a species at a glance):

| | Dale Oak | Highland Pine | River Birch |
|---|---|---|---|
| Height (m) | 24–32 | 28–38 | 16–22 |
| Envelope | Sphere (wider than tall) | Cone, 3 tiers | Vase / Column |
| `crown_base_frac` | 0.40 | 0.38 | 0.45 |
| `crown_width_frac` | 0.45 → 11–14 m | 0.22 → 6–8 m | 0.30 → 5–7 m |
| `trunk_radius_frac` | 0.022 → ⌀1.2 m | 0.016 → ⌀1.1 m | 0.013 → ⌀0.5 m |
| Trunks | 1 | 1 | **2–3 (clump)** |
| Branching | phyllotaxis, 2 woody gens | **whorled**, 2 gens | phyllotaxis, 1 gen |
| `droop` | mild positive | negative (upsweep) | mild positive |
| Silhouette intent | ball on a stump | narrow triangle, tip ≥ 1.5 m wide | slim **pale** trunk, small loose crown |
| Value role | mid-green, near-black trunk | darkest flora value | **brightest** flora value |

Silhouette identity outranks realism (design's ruling, and §1.5's arithmetic):
at 100 m these three must be separable by outline and value alone.

### 3.3 Neighbour interaction

The user's brief asked for it explicitly ("как близко разные деревья рядом
стоят… как они друг с другом взаимодействуют"). All of it is computed by
`analyse_neighbourhood` from the instance array **render already has**, so
`math::ScatterInstance` gains no fields and the frozen core↔render contract is
untouched (Rule 26).

- **Crown shyness.** For a neighbour at distance `d` with crown radii `r₁, r₂`:
  `overlap = r₁ + r₂ − d`. If positive, the crown radius along that azimuth is
  pulled back by `min(overlap/2, shyness · r₁)`. Real canopies do this; more to
  the point, interpenetrating crown blobs read as one mud-coloured mass at low
  resolution, and separated crowns read as trees.
- **Lean away from crowding.** `lean_dir = −normalize(Σ neighbour_dir / d²)`,
  `lean = min(lean_response · crowding, FLORA_LEAN_MAX)`. Edge trees lean into
  the open; interior trees stand straight. This is the visual difference between
  a forest edge and a forest interior, and it costs one vector.
- **Understory.** A young instance under a crowded canopy gets
  `crown_base_frac + 0.10`, `crown_width_frac × 0.8`, `height × 0.6` — drawn out
  and reaching, which is what suppressed trees actually look like.
- **Maturity mix.** Approved by design and landed as `TREE_MATURITY_GIANT_PCT`
  / `_MATURE_PCT` / `_YOUNG_PCT` (15/60/25). It answers a real risk: at
  44 trees/ha with every tree at nominal size a stand reads as a *plantation*.
  The mix restores "the forest is full" (пустота — наш враг) without putting
  canopy back.
  **Re-weighting proposed after the user's forest-floor brief** («мелкие деревья
  очень редкие», «гигантским и могучим»): giants 15 → 25 %, mature 60 → 67 %,
  young 25 → 8 %. The young tier existed to carry EYE-LEVEL FILL; the same brief
  supplies better material for that job — big bushes, fallen logs, snags — which
  cost fewer triangles than a sapling and say *old forest* where a sapling says
  *nursery*. With design (their catalog); build against the constant names, and
  a successor should read whichever values NUMBERS.md carries, not these.
- **Giants are not a species.** An elder tree is `DaleOak` with `maturity` > 1
  (design's ruling: one system, not two). Giants are excluded from L0 sight
  wedges entirely — a 48 m occluder belongs nowhere near a landmark sightline.

**Known limitation:** `analyse_neighbourhood` sees one chunk unless render feeds
it a border margin from adjacent chunks. Without the margin, trees within a crown
radius of a chunk edge get no shyness or lean from across the seam. The error is
bounded (one crown radius) and invisible from any distance, but it is real and it
is recorded here so a successor does not rediscover it as a bug.

### 3.4 The forest floor (user brief, this stage)

Verbatim: «не менее чем втрое [реже], пусть лес будет ощущаться гигантским и
могучим, но не очень частым, и конечно в лесу перепады высот, обрывы,
поваленные деревья большие и маленькие, кусты, большие кусты, сухие мертвые
деревья мелкие деревья очень редкие».

"At least 3× sparser" is a **floor, not a target** — design's 12–18 m spacing
(~80 % cut) clears it. The operative word is *могучим*: the stand must feel
mighty, which is the envelope and giant-tier work, not a density number. And
because the canopy opens up, the floor must carry the scene — this is the same
phenomenon core flagged from the other side (the ray that now reaches the
landmark is the ray that now shows the player 150 m of bare ground).

Classes, all flora geometry:

| Class | Size | Tris | Job |
|---|---|---|---|
| `Bush` | 1–1.5 m | 60–120 | ground texture, softens forest edges |
| `BigBush` | 2.5–4 m | 120–180 | **an obstacle**: breaks a sightline, makes the player pick a side |
| `FallenLog` | 8–14 m long, ⌀0.8–1.4 m | 80–120 | "this forest is old"; walk around or climb over |
| `Deadfall` | 2–4 m, ⌀0.35–0.5 m | ~40 | scatter detail; floored at 0.35 m so it still casts |
| `Snag` | full tree height, no crown | 30–60 | stark vertical; legal at full height inside a sight wedge |

`BigBush` is deliberately **not a scaled `Bush`** — the user named them
separately and they do different jobs. A 1.2 m bush is texture; a 3.5 m bush is
a decision. That is what makes a wood feel *navigated* rather than *crossed*.

`FallenLog` is nearly free: it is the trunk generator's own output, rotated,
half-sunk with the ground query, and it is the cheapest age signal in the
medium. One geometric rule that matters more than it sounds: **a log lies
ACROSS the fall line, not along it.** Downhill it reads as a stick; across a
slope it reads as a fallen tree and becomes something to climb.

**Log collision — sim's rulings, and one of them changes the design.** Shape is
a **capsule**, not a cylinder: Jolt has it natively, and rounded ends let a
sliding player capsule come off cleanly where a cylinder's rim catches and
produces the "stuck on nothing" bug. A half-sunk log is a capsule on its side
with the lower half buried, which costs nothing because the player never reaches
it. **But в35's 1.1 m jump does not exist yet** — the character has
`PLAYER_STEP_HEIGHT` 0.35 m, so today a 1 m log is not a choice between around
and over, it is a **wall**. Therefore, binding until jump lands: big logs never
form a closed ring or a continuous line across a corridor. With no jump that is
a fence, and a player reads a fence as the world being broken, not as density.
Log height and placement get re-tuned when jump arrives — a threshold only works
once the verb exists. Small deadfall is non-physical (tripping on twigs is not
a feature). Crowns never get collision, ever.

Terrain relief and cliffs inside forests are core + design work, not flora's,
but they are the first thing that will exercise the lean and phototropism
rules, because a tree on a cliff edge leaning out over it is both a real
botanical behaviour and a strong L2 guide.

### 3.5 Two hard geometric floors

**Shadow floor — minimum branch diameter 0.35 m.** Render measured
(spec/render.md, thin-caster fix) that with `SHADOW_MAP_SIZE` 4096 over
`SHADOW_HALF_EXTENT_M` 320, `SHADOW_TEXEL_M` = 0.15625 m and **a caster narrower
than ~2 texels (~0.31 m) casts nothing at all.** Branch recursion therefore
terminates at 0.35 m diameter and the remaining foliage attaches to the parent.
This is not a compromise: it is the same rule LANDSCAPE §1.5 already states
("no thin features at distance" — sub-pixel elements shimmer at low res), and
the same rule the triangle budget wants. Three independent reasons, one number.
**We do not model twigs. Ever.**

**Walkable floor — `CANOPY_CLEARANCE_MIN` = 2.2 m of clear trunk, always.**
Design's ruling and grill answer в32 (trunk solid, crown non-physical, crown
lower edge above player height). The tall three target 35–45 % of height, i.e.
9–13 m of clear trunk on an oak. Enforced by construction —
`crown_base_frac · height ≥ 2.2 m` is asserted per species and per instance, and
for drooping envelopes the check uses the *lowest foliage vertex*, not the crown
base, because that is what a player's head meets.

**The clearance rule governs canopy, not obstacles.** A bush, a big bush, a log
and a deadfall are things you walk *around*; they are exempt by definition. The
rule exists so the player never pushes through foliage at head height under a
tree — it is not a ban on eye-level content, which the forest floor now needs
(§3.4). Confusing the two would delete exactly the fill the user asked for.

The crown-base rule is the heart of the stage. Design's diagnosis, recorded here
because it is the reason this zone exists: the complaint was "trees are small",
but the defect was that an 8 m oak had its crown starting at 2.7 m, so the player
pushed *through* foliage — scrub, not forest. Height alone would have produced
taller scrub. What makes a forest feel tall is walking **under** a canopy with
light between the stems.

### 3.6 Triangle budget and the LOD ladder

`TREE_TRI_BUDGET_MAX` = **700** (landed in NUMBERS.md with the big-world batch;
design's revision, not requested by flora). **700 is enough** — costed, not
hoped, and the headroom is not a licence to spend:

| Part | Count | Tris each | Total |
|---|---|---|---|
| Trunk (5 sides × 6 segments) | 1 | 60 | 60 |
| Primary branches (4 sides × 2–3 segs) | 4–5 | 16–24 | 80–120 |
| Secondary branches (4 sides × 2 segs) | 10–12 | 16 | 160–192 |
| Foliage clusters (5 slices × 2 bands) | 20–24 | 15 | 300–360 |
| | | **total** | **~640–700** |

For scale, the meshes this zone replaces are **oak 75, pine 34, birch 52 tris**
against a 500 budget — 7–15 % of the allowance. That is the arithmetic behind
"большие полигоны, считай пустые сферы": the trees were not merely low-poly, the
budget was never spent. Conifers come in cheaper (cone-shell tiers are ~24 tris
for the whole envelope), so the pine has room for real whorled branches.

| LOD | Budget | Geometry |
|---|---|---|
| `Full` | ≤ 700 | complete skeleton + clusters |
| `Reduced` | ≤ 280 (≈40 %) | trunk + primaries + 8 clusters |
| `Silhouette` | ≤ 120 | trunk column + one envelope shell — *today's mesh, which is exactly right at that range* |
| (billboard) | render's | beyond a distance render sets |

**LOD selection is render's decision, not flora's.** Flora supplies the geometry
for each level; render decides which chunk ring gets which.

Scene arithmetic for the change (per 256 m chunk, `FOREST_COVERAGE` 0.30):
old density ≈ 465 trees/chunk × 75 tris ≈ 35 k tris; new density at 12–18 m
spacing ≈ 100 trees/chunk × 700 ≈ 70 k tris at `Full`, and far chunks drop to
`Silhouette` at 12 k. Across a `CHUNK_LOAD_RADIUS` = 2 ring the total lands near
today's, with trees that have branches.

---

## 4. Dependencies

**Uses:** `engine/core/math` (`ScatterSpecies`, `ScatterInstance`, glm),
`engine/platform/render/interfaces/IRenderer.h` (`Vertex`),
`engine/render/sources/ProcMesh.h` (`MeshData`; `tri`/`quad`/`pack` primitives
per the render agreement), the generated `Constants.h` (Rule 14 — never a
literal for anything that lives in NUMBERS.md).

**Used by:** `ScatterBatcher` (render), `ProcFloraTests`.

**Boundary agreement with render (proposed; items marked ACK-PENDING until
render replies — a successor must check the UPD log below for the resolution):**

1. `ProcMesh.h` exposes `tri`, `quad`, `pack`, or flora duplicates ~20 lines
   locally. ACK-PENDING.
2. `ScatterBatcher::mesh_of()` routes the three tree species to
   `build_flora_mesh(species, variant, shape, lod)`; flora supplies
   `append_flora()` so render's `append_transformed` is untouched. ACK-PENDING.
3. `build_scatter_batches` optionally receives neighbour-chunk instances within
   ~20 m of the border, for cross-seam neighbour analysis (§3.3 limitation).
   ACK-PENDING.
4. `engine/render/CMakeLists.txt` and `tests/render.cmake` gain the new files —
   render's files, render's edit. ACK-PENDING.

**Cross-zone items — status, and the reasoning behind each, because the
reasoning is what a successor cannot reconstruct:**

- **core / design — canopy occlusion is a BAND, not a column. RESOLVED,
  core implementing.** `canopy_height_at()` returned a solid ground-to-top
  column. That model was correct only while crowns started at 2.7 m; with crown
  base at 35–45 % the canopy is a **slab** with open trunk space beneath, and the
  C1 eye-to-landmark ray rises with distance, so near-half forest passes cleanly
  *under* the crown base. Design accepted it as their own bug (§1.3 rewritten)
  and added `CANOPY_TRUNK_PATH_MAX` = 250 m: trunks transparent, but a ray that
  accumulates 250 m of forest traversal (~1.5 expected trunk hits) counts as
  blocked, because deep inside a big wood you genuinely cannot orient.
  The load-bearing arithmetic: at 44 trees/ha with ⌀1.4 m trunks a ray crossing
  150 m of forest expects **0.92 trunk hits** — a dappled partial occluder, not
  a wall. Core also measured that crown base 35 % and 40 % give identical C1 to
  three decimals: the base fraction is not the sensitive parameter. Do not tune
  it hoping for visibility.
  **Superseded numbers — do not quote them:** an early measurement put the
  taller canopy at C1 −0.048 with the band model recovering only +0.011,
  implying the seed was under the floor. Both came from a defective C1 test
  (see the dead-end entry below) and are void. The corrected measurement is
  **C1 = 0.751 against a 0.60 floor** with §5.7 heights on seed 1. **The taller
  canopy never broke C1.** There is no visibility debt against this stage.
- **`OAK_MAX_H` / `PINE_MAX_H` / `BIRCH_MAX_H`** in `WorldgenScatter.cpp` are
  hardcoded copies of the old §5 size rows. **BLOCKED**: the §5.7 heights and
  the 12–18 m spacing had not landed in NUMBERS.md at the time of writing
  (`TREE_SPACING_FOREST` still read 5–8 and no tree-height constants existed).
  Core switches them to generated constants in the same pass as the band model.
  A successor should check this first — if it is still open, the meshes and the
  occlusion model are silently disagreeing.
- **The "raising the peak lowers clearance" dead end — RESOLVED. It was never
  real; it was a bug in the C1 test.** Corrected measurement (core, after the
  chain below), landmark excluded from its own occlusion:

  | peak (m) | 52 | 70 | 90 | 115 | 150 | 200 |
  |---|---|---|---|---|---|---|
  | C1 | 0.751 | 0.783 | 0.849 | 0.865 | 0.895 | 0.915 |

  **Clearance RISES with peak height.** Design's `L0_RELIEF` 110–120 is
  affordable, and flora's worst case (a 38 m pine at 140 m from the crag centre
  needing `peak_y` > ~84 m from a 400 m standpoint) is satisfied with margin.
  LANDSCAPE §1.3's "do not retry" note is void — a successor who finds it still
  standing should check with design before believing it.

  **The chain, recorded because the reasoning is the valuable part.** (1) §1.3
  banned raising the peak. (2) Flora observed the finding was measured when
  canopy was 12–18 m and questioned its scope. (3) Core swept and found
  monotonic decline in every canopy regime, attributing it to the crag's own
  flanks. (4) Flora found that in `crag_height()` (`WorldgenMacro.cpp`) the
  ridged flank relief is `ridge_amp_frac * (1 - prof)` multiplying
  `prof * peak_height` — **an occluder expressed as a fraction of the thing it
  occludes** — and proposed an absolute-metres form as a discriminating test.
  (5) Core built it and **refuted it**: fraction / fixed 13 m / sqrt-scaled all
  gave identical results to three decimals. But the sweep returned exactly
  0.000 above 52 m in all three, and a cliff to precisely zero is not how a real
  effect fails — which is what sent core looking further. (6) **The actual bug:
  the C1 test counted the crag's own body as an occluder of the crag.** The aim
  point is peak + 8 m fixed while `LANDMARK_CLEARANCE_FACTOR` multiplies against
  terrain essentially at peak height, so near-summit ground out-angles the
  summit once `0.2 * (peak - eye) > 8 m` — above roughly a 60 m peak the test
  returns 0.000 for every standpoint regardless of the world.

  **Lesson worth more than the result: a wrong hypothesis with a stated
  mechanism and a cheap discriminating test was what exposed the real bug.**
  The refutation was the useful output, not the failure. Flora's parameterization
  was kept anyway (`ridge_amp_meters` is now the default, tuned to reproduce the
  52 m crag exactly) — coupling an occluder's size to what it occludes is a bad
  idea even when it is not the bug, and at a 115 m Ravenscar the fractional form
  would have inflated flank relief 2.2× for nothing. Watch `LR_LOBE_RATIO` for
  the spike-with-a-skirt failure the absolute form could in principle cause;
  design's anti-dome invariant already tests for it.

  Beer-Lambert opacity (design's ruling, one consistent notion of "blocks"
  across C1 and C4) is still coming, but it now rescues nothing — so **the
  maturity mix and the young sub-lattice can be designed for how they LOOK,
  not for how much light they let through to a validator.**
- **Sight wedges — the two tests are different and must stay untangled.**
  Design's ruling: the tall three are banned from the **near** half of a wedge,
  where a tall tree subtends hugely and steals *dominance* (C4). The **far**
  half is where trees sit at ray height and steal *sight* (C1) — and the band
  model handles that correctly without a ban. Bushes, saplings and anything
  under ~8 m stay in the wedge, so it reads as young growth under old canopy
  rather than a mown lane. Giants are excluded from wedges entirely. Snags are
  the one flora that may stand at full height inside a wedge: no crown, nothing
  to out-angle with.
- **sim — answered in full.** Trunk collision radius changes with the trunk
  (в32: trunk solid, crown not); `species_trunk_radius()` exists so nobody
  tables a copy, and sim agreed to call it. Collision shape is a **capsule**
  for trunks and logs (§3.4). Two gaps recorded so a successor does not trip
  over them: (1) `IPhysics` today exposes only `create_static_box` and
  `create_terrain_mesh` — there is **no static capsule call yet**; sim lands it
  when flora is ready to spawn. **Do not ship boxed trunks as a placeholder** —
  a square cross-section is felt through the corners. (2) No jump yet, which
  turns big logs from a choice into a wall (§3.4).
  **Body budget — plan against it.** Sim's Jolt world is configured for 16384
  bodies. At 44 trees/ha with `CHUNK_LOAD_RADIUS` 2 (25 chunks × 6.55 ha) that
  is ≈ **7 200 resident trunk bodies** plus terrain, props and logs — it fits,
  without much room to spare. At the OLD 240 trees/ha it would have been
  ≈ 39 000 and would have blown the cap outright. **The forest thinning the user
  asked for on visual grounds is also what makes tree collision possible at
  all** — worth knowing, because it means the density is now load-bearing for
  two independent reasons and should not be quietly tightened again. Trunk
  bodies stay chunk-scoped so they die with their chunk; any future spacing
  decrease or load-radius increase goes to sim *before* it lands.
- **lead** — NUMBERS.md entries for the §5.7 heights and spacing (Rule 14).
  Already landed: `CANOPY_CLEARANCE_MIN`, `CANOPY_TRUNK_PATH_MAX`,
  `TREE_MATURITY_*_PCT`, `SNAG_DENSITY_HA_*`, `TREE_TRI_BUDGET_MAX` = 700,
  `WORLDGEN_MAX_HEIGHT` = 400.

---

## 5. Step-by-step plan

1. **Boundary + spec** (this document, and the four ACK-PENDING items with
   render). No code before the ack — Rule 25.
2. **`FloraSpecies.{h,cpp}`** — the parameter struct and the three catalog
   species. Pure data, compile-only. (Future, Rule 5: these tables belong in
   `games/daggerfall_n/assets/` as TOML so a species needs no recompile. C++
   tables now for the same reason `ProcMesh` uses them — flagged, not forgotten.)
3. **`ProcFlora.cpp` stage A+D** — trunk + envelope shell. At this point
   `Silhouette` LOD is complete and already matches today's quality; safe
   incremental landing point.
4. **Stage B** — branch skeleton with phototropism, droop, phyllotaxis and
   whorls. `Full` and `Reduced` LODs.
5. **Stage C** — foliage clusters; triangle budget enforcement.
6. **`analyse_neighbourhood`** — uniform grid, shyness / lean / understory /
   maturity.
7. **Tests** (§6), then **one** visual verification frame (§6), then report.
8. **Approved additions** (design ruled the catalog, §5.8/§5.9):
   - **Snag** — 30–60 tris, no crown; the only flora that may legally stand at
     full height inside a sight wedge. Design's constraint: pale dead wood is
     the highest flora value in the scene, so a common snag becomes a *false L2
     guide*. Landed sparse (`SNAG_DENSITY_HA_MIN/MAX` 2–4 ha). The user has
     since asked for more dead wood, so flora proposed splitting it — denser
     inside forest masses where a snag never competes for an open-ground
     sightline, unchanged rarity outside, with a slightly weathered (less pale)
     value. Design's ruling pending; the composition rule does not stop being
     true because more of it was requested.
   - **Vale willow / alder** — the droop parameter's showcase and a *dark* mass
     at the waterline. It fixes a real flaw: every water body was flagged by
     birch, so all water read alike. Design's split — **birch = moving/clear
     water** (river banks, fords), **willow = still/slow water** (lake shores,
     pond rims, slack bends). A player who learns that dark drooping mass means
     still water has learned to read the landscape.
   - **Giants** — folded into the maturity tier, *not* a catalog row (§3.3).
   - **BigBush, FallenLog, Deadfall** — from the user's forest-floor brief
     (§3.4); catalog rows requested from design.

---

## 6. How it is verified

**Unit suite** (`tests/render/ProcFloraTests.cpp`), every item an invariant a
successor can trust:

1. **Determinism** — same `(species, variant, shape, lod)` ⇒ byte-identical
   vertex/index buffers, across runs (Rule 13.1 discipline).
2. **Triangle budgets** — each species at each LOD under its cap; `Reduced` ≤
   40 % of `Full`; `Silhouette` ≤ 120.
3. **Canopy clearance** — lowest foliage vertex ≥ `CANOPY_CLEARANCE_MIN` for
   every species, every variant, every `FloraShape` including understory and
   full droop.
4. **Shadow floor** — no emitted branch tube has a diameter below the minimum;
   asserted on the skeleton, not eyeballed.
5. **Envelope conformance** — all foliage cluster centres lie within the species
   envelope (this is what guarantees the 8 px silhouette read).
6. **Size bands** — generated heights and crown widths inside the §5.7 bands.
7. **Mesh well-formedness** — no degenerate triangles, normals unit-length,
   flat shading preserved (faces own their vertices), y ≥ 0 at the trunk base.
8. **Neighbour analysis** — a tree with a close neighbour gets non-zero shyness
   pointing at it and lean pointing away; an isolated tree gets neither.
9. **Ground contact** — no gap under the trunk at the model origin.

**Visual verification (Rule 27).** ONE frame, 640×360, palette off, at a forest
edge chosen so that all three silhouettes, the under-canopy stem space, and the
open meadow beyond are in the same shot. The project forbids shooting
near-identical frames; one frame that answers the question beats six that
restate it. Checklist for that frame:

- Oak, pine and birch are separable by outline alone (§1.5, design's acceptance
  test).
- Clear trunk space under the canopy — the eye can see *through* the stand.
- Branches are visible as structure, not as crown texture.
- Trunks lay shadows (the thin-caster floor is respected).
- The forest edge reads as an edge: edge trees lean out, interior trees stand up.
- **The forest floor is not empty.** This is the failure core predicted and the
  one that will show first: opening the canopy lengthens sightlines through the
  trunk layer, and bare ground under giant trees is a worse result than the
  scrub we replaced. Logs, big bushes and snags must be carrying it.
- The stand reads as **mighty**, not merely tall — the user's word, and the
  giant tier is what delivers it.

---

## 7. What this zone does NOT do

- **Placement.** Where a tree stands, how many, on what slope, how far from
  water, inside which mask — core's P5 scatter pass, against design's rules.
  Flora never edits `engine/world`.
- **The catalog and the rules.** Which species exist, their size bands,
  densities, palette roles and placement law — design owns LANDSCAPE §5. Flora
  *proposes*; design rules.
- **Constants.** Every number that belongs in NUMBERS.md goes through the lead
  (Rule 14). Flora cites names.
- **Drawing.** Instancing, batching, draw submission, LOD *selection*, shadows,
  materials, palette, wind animation — render.
- **Collision.** Trunk capsules and the non-physical crown (в32) — sim. Flora
  only exposes `species_trunk_radius()`.
- **Grass, flowers, micro scatter.** Render-side instancing against core's
  density maps (LANDSCAPE §2.3). Flora owns trees, bushes and any future woody
  plant; it does not own ground cover.
- **Structures.** `build_site_mesh` and everything in LANDSCAPE §6 stays with
  render.
- **Content data files.** Species tables live in C++ today (like `ProcMesh`);
  moving them to `assets/` per Rule 5 is a scheduled future step (§5 item 2),
  not a silent omission.
