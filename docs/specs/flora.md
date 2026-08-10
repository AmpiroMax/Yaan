<!--
Created: 09:08:2026 - 19:02:07
Last updated: 10:08:2026 - 11:07:33
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
- 09:08:2026 - 19:18:05: design's final rulings (LANDSCAPE 5.8/5.9/5.10)
                         fully folded in: near/far wedge split REPLACED by the
                         crown-occlusion test, giants ALLOWED in wedges (one
                         per wedge, repoussoir), C4 scoped to masses not near
                         vegetation; maturity 25/60/12/3 with design's
                         correction that the young tier was doing mid-canopy
                         layering, not ground fill; snag two-material split;
                         BigBush/FallenLog/Deadfall densities; cliff-edge trees.
                         NEW flora finding: GROUND_SINK_FRAC 0.12 cannot cover
                         0.84 m of ground drop under a 1.2 m trunk on max slope
                         — trunks gain a root flare.
- 09:08:2026 - 19:35:17: GENERATOR LANDED (ProcFlora/FloraSpecies + 12-case
                         suite, 31064 assertions green; render acked the
                         boundary and made ProcMesh's tri/quad/pack public).
                         Two envelope bugs caught and recorded in new §3.7:
                         branches overshot the species HEIGHT band (a
                         cross-zone contract), and the envelope's RADIAL clip
                         was missing entirely (oaks 24.5 m wide vs 10-16,
                         pines 24.9 vs 6-9), including the env==0 apex trap.
                         Measured output table added to §6.
- 09:08:2026 - 19:50:35: VISUAL VERIFICATION caught a third and worse envelope
                         bug: the crown did not exist. Branches under the
                         0.35 m shadow floor terminated WITHOUT re-attaching
                         their foliage (birch primaries are 0.168 m), so
                         birches rendered as bare poles while 31k assertions
                         passed. Fixed by emitting foliage at terminated
                         branches AND distributing the crown over the envelope
                         independently of the skeleton. New §3.7 "THE PATTERN":
                         all three bugs were rules stated in full and
                         implemented in half. New invariant: every canopy
                         species HAS a crown, measured by foliage AREA.
- 09:08:2026 - 19:54:01: Fourth defect, visible only in a frame: the birch
                         crown was foliage but not a MASS (helix distribution
                         read as stacked discs). Clusters now fill the upper
                         crown through its volume and overlap. Oak/pine
                         verified good in 06_overview: full crowns, trunks
                         beneath, dappled floor, legible spacing.
- 09:08:2026 - 19:56:29: Fifth defect (same symptom, deeper cause): the
                         envelope containment rule slid over-sized clusters to
                         the trunk axis instead of shrinking them, stacking the
                         birch crown into a drill bit. Clusters now shrink to
                         fit; birch uses 5 large clusters, 660 -> 450 tris.
- 09:08:2026 - 20:00:11: USER DIRECTION supersedes the solid-cluster
                         approach: foliage becomes flat alpha-cutout CARDS,
                         see-through canopy, wind-ready. New §3.8 designs the
                         geometry/layout half (crossed FIXED cards, not
                         billboards; shrink-don't-slide; vertex ALPHA carries
                         the sway weight so the frozen Vertex is untouched).
                         Pipeline questions sent to render BEFORE building.
                         Birch drill-bit chase STOPPED under Rule 28 — the
                         approach it belonged to is being replaced.
- 09:08:2026 - 20:03:46: New §3.9 for design's banded-massif ruling
                         (§2.8): bench-width arithmetic (their 6 m is fine —
                         the inner side of a bench is a riser, not a drop, so
                         the setback was double-counted), occupancy proposed as
                         a DUTY CYCLE rather than a per-bench count, and the
                         value-contrast rule that trees go near the LIP (sky
                         behind) not the riser (rock behind).
- 09:08:2026 - 20:08:47: New §3.10 — measured the user's reference
                         photos rather than describing them. Beer-Lambert
                         k~0.84/m; porosity is a RIM effect (core 1.6-2.9%%
                         sky, not "porous everywhere"); tracery reads by 2.54x
                         VALUE contrast, not transparency; and 99%% of branch
                         widths fall below render's 0.31 m mask floor, so the
                         lace cannot be mask detail. Corrected my own hollow-
                         interior guess too.
- 09:08:2026 - 20:10:24: New §3.11 — seasons foresight only, nothing
                         built. Colour becomes a per-species palette indexed by
                         season; the MESH STORES AN INDEX, not a colour, so a
                         season is a table swap and not a world regen. Notes
                         that vertex colour has no free channel left (RGB=sway,
                         A=sky visibility), and that winter bare-branch is
                         nearly free since the skeleton already exists.
- 09:08:2026 - 20:21:13: LEAF CARDS BUILT (new §3.8a). Render answered the
                         per-card colour question — the mask atlas is laid out
                         SHAPE x COLOUR, so the card's uv carries both and
                         colour costs no vertex bytes; their reasons for
                         rejecting a spare UV channel and a per-draw palette
                         uniform are recorded, because they are what a
                         successor cannot reconstruct. Broadleaf foliage is now
                         crossed alpha-cutout cards in a SECOND mesh stream
                         (the two render programs read vertex colour with
                         opposite meanings, so merging them is a bug that looks
                         like an optimisation). New FloraCards.{h,cpp} carries
                         the procedurally generated mask atlas — mostly opaque,
                         eroded at the edges, with one or two PLACED interior
                         gaps rather than noise-thresholded ones so their size
                         is guaranteed above render's feature floor. Winter is
                         implemented as one boolean; summer and autumn are
                         byte-identical geometry. Two containment defects found
                         and fixed (corner reach vs half-width; card top edge
                         vs centre), both the §3.7 pattern. Four wiring edits
                         made inside render's zone under an explicit
                         lead-granted Rule 25 exception while that zone was
                         unowned; recorded in each file's UPD, reviewed and
                         kept by the incoming render owner.
- 09:08:2026 - 21:02:00: FRAME READ (Rule 27), and it splits. The oak/willow
                         card canopy WORKS — 02_river_ford is dark trunks
                         against a bright leafy mass with sky between the
                         leaves and clear stem space beneath, which is the
                         user's brief and the reference's value contrast at the
                         same time. The BIRCH failed a fourth time and is
                         STOPPED under Rule 28 (new §3.7 defect 6): measured
                         foliage boxes give oak 1.65 and willow 1.51 tall-to-
                         wide against birch 2.65, so the birch crown is a
                         COLUMN by construction and the three previous fixes
                         were all answers to "what goes in the box" when the
                         box was wrong. Escalated to lead and design because
                         both remedies move THEIR numbers. Added a general
                         legibility floor (no card below a quarter of the crown
                         radius) — a guard, not the cure.
- 09:08:2026 - 21:18:02: BIRCH RESOLVED, and my own numbers corrected. design
                         ruled (a) and adopted the aspect ceiling as a §5
                         acceptance rule measured on the BUILT tree; the lead
                         landed CROWN_ASPECT_MAX and a birch crown-base band.
                         Implemented: crown base derived from the ceiling,
                         birch base 0.58, crown widths CALIBRATED against the
                         built tree (oak 0.45->0.48, birch 0.30->0.52 — the
                         birch had drifted a third under its 5-7 m brief with a
                         green suite), and the vertical card clamps fixed to use
                         the card's CORNER reach rather than its half-height —
                         the third instance of enforcing a rule on the notional
                         element instead of on the thing that reaches. Birch
                         aspect 2.30 -> 1.02, oak 1.53 -> 1.28. Frame reshot:
                         the birch now reads as a small rounded crown on a pale
                         bole, no detached fragments, and the oak stand is
                         fuller. FIRST-REPORT ERROR CORRECTED: the original
                         table pooled 12 variants into one bounding box and
                         inflated every aspect by ~15 % (2.65 vs the true 2.30);
                         re-sent to design and the lead, and pooling variants is
                         now a named trap in §6.
- 09:08:2026 - 21:40:17: CROWN_ASPECT_MAX verified against the REGISTRY, not
                         against anyone's prose: the constant is 1.8 and always
                         was (the lead landed 2.0 at 21:12:24 and refined it to
                         1.8 fifty-one seconds later; design read NUMBERS.md
                         between those two edits and relayed the stale value,
                         and I repeated it back). Nothing needed changing —
                         every citation in this zone is the NAME, no literal —
                         and a fresh constants regeneration plus a full rebuild
                         confirms the suite and the geometry were validated
                         against 1.8 throughout. The derivation was never the
                         binding constraint for any species, so no tree moved.
                         Lesson kept: when prose and NUMBERS.md disagree, the
                         registry wins and costs one grep to check.
- 10:08:2026 - 00:03:11: THE USER REJECTED ALL THREE TREES AND THE GENERATOR WAS
                         REPLACED. Full record in the new `docs/specs/
                         flora_algorithms.md` (flora-owned): literature review
                         with citations, the algorithm choice and why, the
                         measured diagnosis, what changed, and the invariants.
                         In one line each — oaks: the crown was distributed over
                         the envelope *"independently of the skeleton"* and the
                         average leaf card floated 2.60 m from any wood (worst
                         6.91); conifers: the crown was 2-3 solid cones swept on
                         the trunk axis, which is what a skirt IS; birch: 2-3
                         bare pale poles with a tuft, which is what a palm IS.
                         Crowns are now grown by SPACE COLONIZATION (Runions,
                         Lane & Prusinkiewicz 2007) into the species envelope, so
                         detached foliage is unrepresentable rather than merely
                         forbidden; conifers get an explicit WHORL generator,
                         because a conifer is monopodial and rhythmic rather than
                         competitive. Branch radii come from the PIPE MODEL.
                         §3.1 stage B/C and §3.6's triangle table are superseded
                         by flora_algorithms.md §4-§5; §3.8a's card contracts,
                         §3.10's photograph measurements and §3.11's seasons are
                         all UNCHANGED and still current.
- 10:08:2026 - 02:43:32: LANDSCAPE STAGE, flora's first three tasks landed
                         (7e497d9, 62d59c5). §5.10 BUILT AS OBJECTS: snag =
                         broken blunt top + truncated stubs, SPLIT as one
                         geometry / two materials (SnagPale seeds as Snag,
                         byte-identity asserted); logs = butt swell, upturned
                         root plate (big class), snapped stubs, upper-side moss
                         in cell-noise patches, ground contact per buried span
                         with the floating cylinder as control. WHICH QUANTITY
                         separates a snag from a winter tree was measured
                         before the threshold: limb reach CANNOT (oak winter
                         0.106-0.136 vs snag 0.072-0.121 of height, overlap at
                         every threshold); off-axis FACE COUNT does (snag 7-45,
                         oak 95-179, willow 95-197); winter birch 36-61 is
                         honestly adjacent and scoped out by name.
                         flora_maturity_for() = the 25/60/12/3 draw's one home
                         (bands are now REGISTRY rows, lead's Rule 35 ruling).
                         Card foliage >= 3 planes per cluster at EVERY LOD
                         (render-spec floor: angular coverage vs the worst
                         azimuth; pine sprays 2 -> 3, Full 632 -> 686 tris).
                         New §3.12: the CLUMP FIELD (в19г, design-blessed;
                         FloraField.h) and the RICH EDGE SET (в8/в19в; seven
                         patch species + StuntedPine; FloraEdgeRules.h). Suite
                         instrument bug fixed: cards_of() strode 6 verts per
                         4-vert card. Mechanism bug fixed: emit_card_cluster
                         hard-wired the CANOPY clearance into every card
                         species; the floor is now the tree's own (krummholz
                         carries foliage to the ground by classification).
                         §7's "no ground cover" is amended — the stage brief
                         moved ground cover into flora; grass awaits the
                         lead's Task 4 gate.
- 10:08:2026 - 10:56:08: CLARK-EVANS R MEASURED across all five clump classes
                         (core's request, BR-4) and added to §3.12. Four
                         classes pass CLUMP_R_CLUMPED_MAX comfortably
                         (0.38-0.53); GRASS IS MARGINAL — 0.776 mean, one seed
                         at 0.811 breaching 0.80 — and it is marginal BY
                         AUTHORSHIP: coverage 0.55 means over half the ground
                         carries grass, and a pattern covering half the ground
                         cannot be strongly clumped. The CONTROL also found an
                         instrument bias nobody had priced: the same placement
                         machinery with NO field measures R = 1.134, not 1.000,
                         because a jittered grid is more regular than Poisson.
                         So the threshold's DENOMINATOR is unstated (ideal
                         Poisson vs the same placement unclumped) and the two
                         give opposite verdicts for grass. Escalated to design
                         and the lead; not tuned, because buying the number
                         would mean bare earth between tufts.
- 10:08:2026 - 11:07:33: BOTH ESCALATIONS RULED (design §1.7, 9b2f58b; lead's
                         row f201bac) and the rulings implemented. BR-4 moves
                         onto R_norm with CLUMP_R_NORM_MAX 0.85 — and what
                         condemned the old row was design's OWN even-field
                         clause failing the case it exists to admit (Rule 30a),
                         not grass. Design then escalated a defect one level
                         down that I had missed: THE CONTROL IS ITSELF
                         DENSITY-DEPENDENT, since a jittered lattice loses
                         regularity as it is thinned. Re-measured per class
                         (control 1.052 at coverage 0.09 rising to 1.136 at
                         0.35); low-coverage classes were over-divided and
                         rise, grass falls, NO VERDICT FLIPS, all five pass.
                         New §3.13: path-class margin richness on the
                         maintenance fiction — one threshold plus an ORDERING
                         (hint >= dirt > cobble), BR-3 scoped to the
                         unmaintained classes so a cobbled street failing the
                         ratio is a PASS. Two inversions of earlier work in
                         this same spec: §3.12's edge FLOOR is precisely what
                         would garden a cobbled gutter, so it is scoped by the
                         per-class column; and the weight scales the edge PEAK
                         and never the base presence, because A KEPT VERGE IS
                         NOT BARE GROUND.
-->

# Flora — tree and plant geometry (agent spec)

> **READ `docs/specs/flora_algorithms.md` FIRST if you are here about tree
> SHAPE.** It supersedes §3.1 stages B-D and §3.6 as of 10.08.2026. This file
> remains current for the zone boundary (§1), the public interface (§2),
> neighbour interaction (§3.3), the forest floor (§3.4), the two hard floors
> (§3.5), the leaf-card contracts (§3.8a), the reference-photo measurements
> (§3.10) and seasons (§3.11).

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
| `engine/render/sources/ProcFlora.cpp` | assembly: trunk, crown, LOD, logs |
| `engine/render/sources/FloraSkeleton.{h,cpp}` | the GROWERS: space colonization, whorls, pipe model |
| `engine/render/sources/FloraBuild.{h,cpp}` | Tree state + the geometry primitives and containment |
| `engine/render/sources/FloraNeighbours.cpp` | `analyse_neighbourhood` |
| `engine/render/sources/FloraCards.{h,cpp}` | the leaf mask atlas + card emitter |
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
  **Final tiers (design, after the user's forest-floor brief):
  25 % giant / 60 % mature / 12 % sub-mature / 3 % sapling.**
  Flora proposed 25/67/8 on the argument that the young tier was carrying
  eye-level fill and the brief had supplied better material for that job.
  **Design corrected the premise and the correction is worth more than the
  proposal:** a "young" tree at 0.5–0.7× of a 28 m tree is 14–20 m — its crown
  sits *above* eye level and *below* the main canopy, so it was never doing
  ground fill at all. It was doing **mid-canopy layering**, which is what makes
  a wood feel deep instead of a colonnade with debris on the floor. So the tier
  splits: genuinely small (sapling, 0.4–0.6×) drops to 3 % and satisfies
  «мелкие деревья очень редкие», while a sub-mature band survives at 12 % to
  keep the middle of the vertical section populated. Ground fill moves to the
  §3.4 classes, as argued. **Do not collapse sub-mature back into sapling** —
  they are different structural jobs that happen to share a scale multiplier.
- **Giants are not a species.** An elder tree is `DaleOak` with `maturity` > 1
  (design's ruling: one system, not two). Giants ARE allowed in L0 sight
  wedges, one per wedge — see §4; an early version of this spec excluded them.

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

All approved by design (LANDSCAPE §5.10) with the densities below:

| Class | Size | Tris | Density | Job |
|---|---|---|---|---|
| `Bush` | 1–1.5 m | 60–120 | `BUSH_EDGE_DENSITY` | ground texture, softens forest edges |
| `BigBush` | 2.5–4 m | 120–180 | 8–15/ha in masses, clearing rims, scarp bases, stream banks; **never in a corridor mask** | **an obstacle**: breaks a sightline, makes the player pick a side |
| `FallenLog` | 8–14 m, ⌀0.8–1.4 m | 80–120 | 3–8/ha; **excluded from corridors** until sim ships vaulting | "this forest is old"; walk around or climb over |
| `Deadfall` | 2–4 m, ⌀0.35–0.5 m | ~40 | 15–30/ha, corridors allowed (half-sunk, under step height) | scatter detail; floored at 0.35 m so it still casts |
| `Snag` (in masses) | full tree height, no crown | 30–60 | 1.5–3/ha (~one per 30–80 m) | **weathered grey-brown** — texture and atmosphere |
| `Snag` (open ground) | full tree height, no crown | 30–60 | 0.25–0.5/ha | **pale bone** — a legitimate L2 guide |

**The snag split is the model for how to take a user request without discarding
a composition rule.** Design's constraint was that pale dead wood is the highest
flora value in the scene, so a common snag becomes a false landmark; the user
asked for more dead wood. Rather than trade one against the other, the same
asset got two materials and two densities: *a pale snag alone in a meadow is a
landmark; a grey snag in a wood is weather.* The user gets dense dead wood where
they walk and the rule survives contact intact.

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

**Cliff-edge trees — approved, deliberate, and constrained.** A tree leaning out
over a drop is a superb silhouette and a real L2 guide. Design's rules:
mature/giant only (a sapling on a cliff reads as an accident, not a statement),
root plate set back ≥ 1.5 m from the edge so it never floats when the scarp is
voxelised, lean **10–20° outward** (0.17–0.35 rad — note this is a *separate,
larger* parameter than the crowding lean of §3.3, which is capped near 0.12 rad),
scarps ≥ 3 m only, and **one per scarp segment** — a row reads as planted.
Design's §2.7 adds meso relief (25–60 m wavelength, 1.5–4 m amplitude) and
scarps (2–5 m, 0.5–1.5/ha in forest) as general terrain, so the slope, lean and
phototropism rules finally have terrain to exercise instead of a gentle roll.

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

**Ground floor — a root flare, not a sink hack.** `ScatterBatcher` currently
buries every instance by `GROUND_SINK_FRAC` = 0.12 × scale to hide the downhill
gap under a mesh placed at a single sample height. That is ~0.12 m, and it does
not survive the new sizes: a ⌀1.2 m trunk on `TREE_SLOPE_MAX` (0.61 rad) spans
`1.2 × tan(35°)` ≈ **0.84 m of ground drop across its own base**, before §2.7's
new micro relief (0.3–0.6 m) and meso relief (1.5–4 m) are added. The old
⌀1.1 m trunk already needed 0.77 m, so this was marginal before and merely
invisible at small scale.
Flora's fix, in geometry rather than in a fudge factor: the trunk gains a
**root flare** — the bottom ~1.2 m widens to roughly 1.6× base radius and
extends ~1.0 m *below* the sample height. The skirt buries itself in whatever
the terrain does, real trees have exactly this shape, and it is what makes
design's "root plate set back ≥ 1.5 m so it never floats when the scarp is
voxelised" achievable rather than aspirational. Raised with render, since
`GROUND_SINK_FRAC` is theirs and should probably drop once the flare exists.

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
- **Sight wedges — FINAL RULING (design §5.9, after the C1 correction). The
  near/far half-split is GONE.** It is replaced by the single test that already
  governed the castle: **no tree may occlude the landmark's CROWN (its top
  third) from a corridor standpoint; occluding its FLANK is fine.** One notion
  of acceptable occlusion across architecture and vegetation. If per-tree crown
  testing proves expensive for core, the old near-half ban is an acceptable
  *fallback* — but it is the fallback, not the intent.
  - **Giants are now ALLOWED in sight wedges**, at most **one per wedge**.
    Flora argued for the re-rule after the C1 debt evaporated and design
    granted it: an off-axis elder in the middle distance **gives the landmark
    scale**, which is what a distant landmark most needs and most rarely gets.
    That is repoussoir, and excluding it deleted our best depth cue exactly
    where depth matters. *One elder frames; three elders screen.*
  - **C4 governs MASSES and built structures, not individual near vegetation.**
    Design's sharpening: a 48 m crown 100 m away subtends more than the mountain
    behind it and **that is fine** — nobody mistakes a near tree for a distant
    massif. C4's real target was always the foothill pine wall.
  - Snags may stand at full height inside a wedge: no crown, nothing to
    out-angle with.
  **This entry supersedes an earlier one in this spec.** A successor finding the
  near/far split described as current is reading a stale copy.
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

### 3.8 Foliage cards — the approach that REPLACES solid clusters

User direction, verbatim: «хочу деревья с кронами не шариками, а с листвой …
надо сделать ствол, листву плоскими прозрачными большими плоскими наборами
листочков / хочу чтобы сквозь листву можно было смотреть, хочу чтобы она якобы
перемещалась и шуршала / деревья очень больная и важная для меня вещь».

**This supersedes the solid blob clusters of §3.1 stage C.** It is not a tuning
of them — the user is rejecting the volume itself, so no amount of reshaping a
blob satisfies it. Three requirements, in priority order:

1. Foliage is **flat alpha-cutout cards** carrying clusters of leaves.
2. You can **see through the canopy** — sky and light between the leaves. This
   is the actual point, and it is why cards are the only option: a solid cluster
   can never do it however well shaped.
3. The foliage **moves**. Wind is render's vertex shader; the rustle is audio
   and belongs to a later stage. Flora's job is to build cards wind CAN move.

**Card geometry.** One card = a quad = 2 triangles. Cards are grouped into
**crossed clusters**: 2–3 quads intersecting at angles around a shared centre,
so the group reads as volume from any azimuth. Cards are **fixed-orientation,
never camera-facing billboards** — billboards rotate visibly at 640×360,
shimmer under palette quantization, and are wrong in the shadow pass by
construction (a card turned to face the eye casts a rotating shadow).
Orientation comes from the card's position in the crown: normal roughly outward
from the crown axis, plus a deterministic tilt so the crown does not read as a
set of concentric shells.

**Layout.** Cards replace clusters one-for-one in `scatter_envelope_clusters`
and at branch tips — the existing envelope machinery (§3.1 stage D) is
unchanged and still what guarantees the silhouette. Card SIZE scales with the
local envelope radius, and the §3.7 lesson applies directly: when a card does
not fit, **shrink it, never slide it to the axis**.

**Triangle budget stops being the binding currency.** A crossed pair is 4 tris
against ~15 for one blob cluster: an oak's foliage goes from ~330 tris to
~80–160. The cost moves entirely into **overdraw**, which alpha testing makes
worse by disabling early-Z, and which flora has never counted. Render owns that
budget; the numbers in §3.6 are about to stop being the useful metric here.

**The alpha mask is procedural (Q13, no image files):** lobed leaf shapes
arranged around a stem axis, generated as a texture like everything else.
Ownership with render — it is their ProcTexture path.

**Reference images are coming from the user.** The instruction is therefore to
build the MECHANISM and keep leaf shape and card layout cheap to retune — do
not polish a look that is about to be specified.

### 3.8a Leaf cards AS BUILT — the contracts a successor must not break

Landed 09:08:2026 20:21. Files: `FloraCards.{h,cpp}` (new), `ProcFlora.*`,
`FloraSpecies.*`, and four wiring edits inside render's zone made under an
explicit, recorded lead-granted Rule 25 exception while that zone was unowned.

**TWO STREAMS, AND MERGING THEM IS A BUG THAT LOOKS LIKE AN OPTIMISATION.**
`build_flora_mesh` returns `FloraMesh { MeshData wood; MeshData cards; }`, and
`ScatterBatches` grew a matching `foliage` buffer. The reason is not batching,
it is semantics: on render's `"prop"` program a vertex's colour IS its albedo;
on their `"foliage"` program the same four bytes are WIND DATA and the albedo
comes from the leaf atlas. Same bytes, opposite meaning. Anyone who "notices"
that the two buffers could be one has not read both shaders.

**Channel map (render's contract — do not deviate):**

| chan | meaning | granularity |
|---|---|---|
| r | sway weight, 0 at the attachment, 1 at the free edge | **per VERTEX** |
| g | per-instance wind phase | per instance |
| b | value jitter | **per CARD** — all four vertices identical |
| a | sky visibility (render's interior lighting) | left at 255, never touched |
| uv | mask atlas tile = the (shape, colour) pair | per card |

r must be a per-vertex FIELD or the card translates rigidly and reads as a
flag; b must be per-card or the jitter becomes a gradient across the card. Both
are asserted in the suite, because both are the kind of thing a later reader
"tidies up" in the wrong direction.

Sway is computed as distance from an attachment point — the stem axis at the
crown base — normalised by the cluster's own reach. One continuous field, so
the trunk barely moves, the outer crown moves fully, and each card gets an
inner-to-outer gradient for free.

**PER-CARD COLOUR: render's ruling, and why the alternatives died.** The mask
atlas is laid out **SHAPE x COLOUR** (4 shape columns x 8 tone rows), so a
card's uv already selects both and colour costs **zero extra vertex bytes**.
Recorded because the reasoning is what a successor cannot reconstruct:
a spare UV channel would unfreeze `platform::Vertex`, which every mesher,
shader and the backend vertex layout depend on — a real Rule 26 sync and a
tree-wide rebuild to buy something available free; and a per-card palette
uniform, more elegant on paper, dies because scatter bakes ONE merged buffer
per chunk, so all species and all cards in a chunk are a single draw and a
per-draw uniform cannot vary within it. **The tone rows are therefore a GLOBAL
table, not per-species** (oak x3, birch x2, willow x2, conifer x1); each
species owns a contiguous band. That is not a compromise, it is what
one-chunk-one-draw implies.

Shape and tone are drawn INDEPENDENTLY per card. That is the point of the
layout: the same leaf outline appears light and dark in one crown, which is
what a crown that is 79-86 % leaf in its core needs in order to read as volume
(§3.10) — a value that is welded to a shape cannot do it.

**The mask, and why it is not lace.** Masks are **mostly opaque with the
porosity spent at the EDGES**, per §3.10's measurement. Concretely:

- the outline is an ellipse scalloped by two harmonics (big lobes), then
  ERODED by a noise field whose threshold ramps from 0.93 in the core to 0.50
  at the rim — that erosion is where the porosity budget goes, and it is what
  makes the silhouette bitten rather than a blob;
- **interior gaps are PLACED, not noise-thresholded**: one or two per tile at
  ~0.19 of the card, about 1 m on a 5.5 m oak card, three times render's
  ~0.31 m feature floor. Placing them guarantees the SIZE. A noise threshold
  tuned for a 2-3 % area budget produces gaps at whatever size the noise
  happens to give, which is exactly how "a few real holes" degenerates into the
  lace that fails twice (invisible in the direct view, aliasing in the shadow
  map);
- measured on the generated tiles: enclosed gaps are 0.2-3.3 % of the body,
  against the reference interior's 1.6-2.9 %. Deliberately at the generous end,
  because the user asked for see-through and the photo is only evidence about
  structure;
- **alpha is strictly binary.** The material is an alpha TEST at 640x360 under
  a 64-colour palette post; a soft edge becomes dither, i.e. noise on
  few-pixel geometry (render's PALETTE SIGNAL STRENGTH rule);
- rgb carries the tone quantized to **three flat shades** with a baked
  top-lit/rim-lit gradient. This is the "spend the effort on the leaf MASS and
  its lighting" instruction made concrete: the card has its own form before any
  scene light touches it.

**Card geometry.** A crossed cluster is 1-3 quads sharing a centre, normals
spread over 180° of azimuth from the outward direction, with ALTERNATING
elevation tilt — a cluster of purely vertical planes is invisible from directly
above, which is precisely the view a player gets of a crown from a hillside.
Fixed orientation, never camera-facing: a billboard rotates visibly at 640x360,
shimmers under palette quantization, and is wrong in the shadow pass by
construction. Branch tips get ONE card, not a trio — the crown MASS is the
envelope scatter's job (§3.7.3), the tip card is only the visual join.

**Two containment rules that cost a debugging round each, so they are written
down.** (1) The envelope and the species width band must be checked against the
card's **CORNER reach**, not its half-width — the diagonal is what pokes out,
and the birch broke its 7 m band by 0.11 m until this was fixed. (2) The height
band must be checked against the card's **top edge**, not its centre — same
species, same afternoon, 22.46 m against a 22 m maximum. Both are the §3.7
pattern again: a rule enforced on the notional element rather than on the thing
that actually reaches.

**Winter is one boolean and it is implemented**: deciduous species emit no
cards, the skeleton is generated regardless, and the bare tree IS the winter
tree. Summer and autumn produce **byte-identical geometry** — asserted — so
those two seasons are a texture regeneration and nothing else.

**Overdraw is now the binding cost, not triangles.** An oak is ~36 crown cards
plus ~20 tip cards; triangle counts went DOWN while every foliage fragment is
now alpha-tested, which disables early-Z. The per-species lever is
`cluster_count`. §3.6's triangle table has stopped being the useful metric for
foliage.

**Deliberately NOT done this stage: the conifer.** The pine keeps solid cone
tiers so the verification frame carries both treatments side by side and
answers whether needles need cards, instead of the answer being guessed. The
atlas already generates a `NeedleFan` column and the pine already names it.
Bushes stay solid by design's ruling (§5: only tree foliage is cards).

### 3.9 Vegetation on the banded massif (design §2.8)

Ravenscar becomes a banded contour massif: near-vertical risers (≥ 55°,
8–15 m) alternating with walkable benches (≤ 25°, 6–30 m), split by aretes.
`TREE_SLOPE_MAX` (0.61 rad) excludes the risers automatically, so **vegetation
collects on the benches in horizontal lines**. That look must be deliberate.

**Bench width arithmetic (flora's answer to design's question).** The setback
applies to the OUTER lip only — the inner side of a bench is the base of a
riser going *up*, not a fall, and a tree at a cliff base is a real thing. It
needs only the flare's own radius so the trunk is not embedded in rock:

    legal axis band = W − r_flare − (CLIFF_SETBACK + r_flare)

with `r_flare` = `species_trunk_radius()` (already includes the flare, so no
one has to re-derive it): pine 0.84 m, oak 0.99 m, birch 0.40 m, big bush
0.16 m; giants ×1.5. A 6 m bench leaves a pine **2.81 m** of lateral freedom —
enough to look scattered rather than surveyed. Derived floors proposed to
design: `BENCH_VEG_WIDTH_MIN` 5.0 m (below it, bushes and grass only) and
`BENCH_VEG_WIDTH_MIN_GIANT` 7.0 m. **The vegetation floor is below the terrain
floor, so no terrain change is needed.**

**Occupancy, not a count.** Design proposed one cluster per bench segment.
A count does not survive scale — one cluster on a 200 m bench reads as a potted
plant exactly as badly as a continuous line reads as landscaping, and both are
the same failure: vegetation that does not respond to the mountain. Proposed
instead: `BENCH_VEG_DUTY_MAX` 0.25, `BENCH_CLUSTER_LENGTH_MAX` 25 m,
`BENCH_CLUSTER_GAP_MIN` 40 m, and — the one worth defending —
`BENCH_BARE_FRACTION_MIN` 0.40: **at least 40 % of benches carry nothing.**
What makes a stand above a 12 m drop extraordinary is that the ledges above and
below it are bare; a dutiful cluster on every bench is still landscaping, just
at lower density.

**Place toward the LIP, not the riser.** A tree at the inner edge of a bench has
a cliff face behind it — dark on dark, no silhouette, invisible at any
distance. The same tree at the setback limit has SKY behind it and its crown
overhangs the drop. This is §1.5's skyline rule applied at band scale, and it
is the entire reason to vegetate benches: place within the outer ~40 % of the
legal band.

**Treeline should snap to the nearest band lip** rather than sit at a flat
elevation, which would cut mid-riser and leave half-vegetated cliff faces
reading as a mowing line. Raised with design and core; costs nothing since the
lips are already known.

### 3.10 What the reference photos actually measure

The user supplied three photos (`tree_images_examples/`): autumn broadleaves
shot from below against sky. Adjectives about them were turning into
requirements, so the crown was measured in pixels instead. **Two readings were
wrong, including mine**, and the measurement corrected both.

**Transmittance vs depth** (sky seen through the crown, columns whose
background is sky; two exposures of the same maple agree within 3 pts):

    depth into crown   photo 3   photo 2
      0 – 40 px         23.6 %    22.8 %
     40 – 80            22.9      24.2
     80 – 120           10.3      13.6
    120 – 160            7.3      10.8
    160 – 200            2.9       3.8
    200 +              0.5–4.3   0.7–3.0

Clean Beer-Lambert decay to an asymptote. Fitted extinction **k ≈ 0.84 per
metre** (half-depth 0.83 m; T < 3 % beyond ~3 m of penetration), scaling the
390 px crown radius to a ~6 m maple.

**1. "Porous everywhere, no solid core" is false.** Sky through the crown
interior is 1.6–2.9 %; only the outer ~25 % of the radius reaches 16–28 %.
**Porosity is a RIM phenomenon.** The core is 79–86 % leaf — nearly opaque.
My own opposite guess (foliage on terminal twigs only, hollow interior) was
equally wrong: the interior is full.

**2. The tracery reads by VALUE CONTRAST, not transparency.** Measured
luminance: branch 50, leaf 135, sky 235 — **branch:leaf = 2.54x**. The skeleton
is visible because it is the darkest thing in the frame against a bright
backlit leaf field, *not* because sky shows through. Dark limbs plus bright
foliage reproduce the look; see-through cards do not, and are not what is
happening in the photo.

**3. The fine tracery cannot survive 640x360.** Median branch width is 2 px in
a 780 px crown = **0.03 m**. A 12 m crown at 27.7 m fills 240 px, putting the
median branch at **0.62 px** — sub-pixel at every gameplay distance. Worse,
**99 % of branch widths are below render's 0.31 m mask-feature floor** (p90
0.08 m, p95 0.12 m, p99 0.23 m). The lace is therefore unrepresentable *as mask
detail by render's own rule*; only trunk and primary limbs can read, and they
must be **geometry**, not alpha-mask features.

**4. Do not calibrate palette from these photos.** The same tree in two frames
gives leaf/dark splits of 76/10 and 53/40 purely on exposure.

**5. Framing caveat.** All three are shot from below, close, against bright
sky — the most flattering possible angle for canopy porosity. Gameplay is
1.7 m eye height at 10–100 m, mostly against *terrain*. Against a dark
hillside the rim gaps show dark ground rather than bright sky and the 2.54x
contrast collapses. **The reference look is guaranteed only on the skyline**,
which §1.5 already governs.

**6. Two distance regimes, not one look.** Photo 1 (canopy mass at distance) has
no readable tracery at all: what carries it is colour variation between crowns
and a lumpy collective outline. Photos 2–3 (one near tree) carry tracery and rim
porosity. These are different requirements for different LOD bands.

**7. Conifers stay green in all three autumn photos** — the pine palette does
not depend on the pending seasons question, so pine work is unblocked either
way.

### 3.11 Seasons — making the ruling cheap to apply (not building it)

User wants autumn, summer and winter, to be game-designed later. Nothing is
built now; what follows is only the shape that keeps a later palette ruling a
data swap instead of a world regeneration.

**Foliage colour is a per-species PALETTE INDEXED BY SEASON**, with exactly one
entry today. Zero cost now, and it is the difference between adding a season
and rewriting one.

**Where the colour lives — and the constraint that decides it.** Vertex colour
is *already fully spent*: RGB carries sway weight and phase, A is render's
sky-visibility channel. **There is no free channel, so colour cannot live in
vertex colour.** Baking it into the leaf mask is also wrong — it multiplies
textures by species x season *and* destroys per-card jitter, since one texture
is one colour.

So: **the mesh stores an INDEX, not a colour.** A per-card scalar in 0..1
selects a position in a palette ramp; the material resolves
`colour = palette[species][season].sample(index)`. Consequences:

- a season change swaps a small table — **no mesh regenerated, no world rebuilt**;
- per-card jitter is baked once and is season-independent;
- widening jitter per season is free, because the palette entry is a **ramp,
  not a colour**. Autumn gets a wide spread (§3.10: several values in one crown,
  strong tree-to-tree variation), summer a narrow one. This is exactly the
  "widen the range per season" requirement, and it falls out of the
  representation rather than needing a mechanism.

**ANSWERED and BUILT (render, 09:08:2026).** The colour lives in the **mask
atlas tile**: the atlas is laid out SHAPE x COLOUR, so the uv the card already
carries selects both. Zero extra bytes, and colour is not welded to shape. The
two rejected alternatives and their reasons are recorded in §3.8a — read them
before proposing either again. The season requirement is met in full: a season
change regenerates ONE texture and re-uploads it; no mesh is regenerated, no
chunk rebuilt, no baked jitter invalidated.

**Winter is the cheapest season and it is already in hand.** Bare deciduous
branches = *do not emit foliage cards*; the skeleton is generated regardless.
That is one boolean (`has_foliage`) beside the palette in the same table, false
for deciduous in winter and true for conifers, which retain needles. Snow is
render's and core's. §3.10 measured that conifers stay green in all three autumn
photos, so pine's only season delta is snow — pine is season-stable otherwise.

**Do not calibrate the autumn palette from the reference photos** — §3.10, the
same tree gives leaf/dark splits of 76/10 and 53/40 on exposure alone.

### 3.12 The clump field and the rich edge set (landscape stage, 10.08.2026)

**The clump field (в19г, design-blessed with amendments; `FloraField.h`).**
Clumping is an AUTHORED FIELD, not randomness: per ground-cover class
(Flowers, Mushrooms, Moss, GrassTufts, Pebbles) a seeded low-frequency field
that scatter density MULTIPLIES by. The authorship is three registry rows per
class — `CLUMP_WAVELENGTH/COVERAGE/CONTRAST_<CLASS>` — and the three
load-bearing mechanisms a successor must not "simplify" away:

1. **The raw field is RANK-EQUALIZED to uniform [0,1]** through a
   deterministic CDF table. This is what makes COVERAGE exact (0.18 means the
   top 18 % of ground, precisely) and it is Rule 31 satisfied by construction;
   the un-equalized bell is the suite's failing control. Bare value noise
   never leaves its middle band — the massif model already lived that defect.
2. **Composition order is design's and binding:** `density = base × clump ×
   edge_gradient × exclusions`, and near a path the edge gradient FLOORS the
   field (`clump_field_edged`) so a coverage gap can never bare a margin —
   BR-3 holds whatever the field says. The trodden centre is core's exclusion.
3. **Mushroom rings are a SECOND STAGE under the field** (parent-child, not
   more noise), and ring-vs-cluster parity is legible from the PARENT SEED
   (even = ring) so core's find promotion is deterministic. Design's
   requirement: the promotion predicate lives with FIND placement (BR-5/BR-6
   siting can fail), never inside the parity.

Destination: `engine/core/math` under core's ownership (their DAG ruling —
engine/world cannot include engine/render); the file is dependency-free for
that move, and until it lands the flora-zone copy is the only one. Core also
takes `flora_maturity_for` in the same move and ports the Rule 31 tests.

**CLARK-EVANS R, MEASURED (BR-4, core's request, 10.08.2026).** R = observed
mean nearest-neighbour distance / the Poisson expectation `1/(2√ρ)`;
`CLUMP_R_CLUMPED_MAX` = 0.8. Placement modelled as core described it —
jittered-grid candidates accepted with probability = `clump_field` — with an
interior-buffer edge correction, three seeds, 240 m square:

| class | coverage | contrast | R (mean, range) | vs 0.80 |
|---|---|---|---|---|
| GrassTufts | 0.55 | 0.35 | 0.776 (0.723–0.811) | **marginal, one seed breaches** |
| Moss | 0.22 | 0.55 | 0.529 (0.494–0.576) | passes |
| Pebbles | 0.15 | 0.65 | 0.466 (0.399–0.527) | passes |
| Flowers | 0.18 | 0.75 | 0.515 (0.448–0.565) | passes |
| Mushrooms | 0.10 | 0.85 | 0.383 (0.335–0.429) | passes |

**Two findings, and neither was "tune grass until it passes".**

1. **R IS A PROPERTY OF THE PLACEMENT, NOT OF THE FIELD ALONE, and the
   control proves it.** The same machinery with the field replaced by a
   constant measures **R = 1.134, not 1.000** — a jittered grid is *more
   regular* than Poisson, so roughly 0.13 of every raw number is the placement
   machinery rather than the field.
2. **Grass is marginal BY AUTHORSHIP, not by defect.** A class authored at
   coverage 0.55 carries something on over half the ground; a pattern that
   covers half the ground *cannot* be strongly clumped, which is arithmetic
   rather than tuning. R tracks the authored contrast monotonically, so the
   metric is meaningful and grass is simply the least clumped class on purpose
   («широкие волны»). Raising its contrast would buy the number by putting
   bare earth between tufts — a different meadow. **Escalated, not tuned.**

**DESIGN'S RULING (10.08.2026, LANDSCAPE §1.7; `CLUMP_R_NORM_MAX` = 0.85
replaces `CLUMP_R_CLUMPED_MAX` = 0.80).** The bar moves onto the NORMALISED
quantity `R_norm = R(field on) / R(same placement, field constant)`. Two
points worth more than the number:

- **What condemned the old row was not grass — it was design's own even-field
  clause.** BR-4 demanded «R ≈ 1 where the field says even», and the correct
  pass case measures 1.134 on this machinery, so *the clause failed the case
  it exists to admit* (Rule 30a). That indicts the quantity with grass struck
  from the table entirely. Normalised, the even-field case is 1.0 by
  construction: **the rejected instance becomes the denominator**, which is
  the tidiest form a control can take.
- **0.85 is DERIVED, not translated.** 0.80 through the control would give
  0.705, under which grass's worst seed (0.715) still breaches — translating a
  bar across a change of quantity would have smuggled the original error
  through. It is set from the rejected instance at 1.000 and the worst
  authored seed at 0.715, a few seed-noise widths off the reject.

**THE CONTROL IS ITSELF DENSITY-DEPENDENT — re-measured per class.** A
jittered lattice loses regularity as it is thinned (accept everything and you
measure the lattice; accept one in ten and the survivors approach Poisson), so
one constant-field control cannot serve five classes spanning coverage
0.09–0.35. Re-taken with the constant set to each class's OWN mean, so
numerator and denominator differ in exactly one thing:

| class | field mean | R field-on | control | **R_norm** |
|---|---|---|---|---|
| Mushrooms | 0.087 | 0.383 | 1.052 | **0.364** |
| Pebbles | 0.121 | 0.466 | 1.065 | **0.437** |
| Flowers | 0.150 | 0.515 | 1.075 | **0.478** |
| Moss | 0.163 | 0.529 | 1.080 | **0.490** |
| GrassTufts | 0.352 | 0.776 | 1.136 | **0.683** |

The control rises monotonically with acceptance rate (1.052 → 1.136), which is
the density dependence made visible. The correction runs one way as predicted:
low-coverage classes were over-divided and rise, grass's denominator grows and
its number falls. **No verdict flips; all five pass 0.85, worst grass seed
0.714.** Correctness owed to the quantity, not a gate.

### 3.13 Path-class margin richness — the maintenance fiction

**Design's ruling (10.08.2026): A RICH MARGIN IS WHAT GROWS WHERE NOBODY
SWEEPS.** Cobble through a settlement is swept by the people who live there; a
generator that gardens a town gutter has made maintenance invisible. Stated as
**one threshold plus an ORDERING**, deliberately not four per-class constants
— four rows would be four things to tune, while «less tended means more
overgrown» is what the fiction actually claims:

- `RICH_EDGE_RATIO` keeps its single value and is **measured on the
  hint-path**, the specimen class it was written for;
- the others are held to their order only: **hint ≥ dirt > cobble**, with a
  dirt road required to *show* a peak but not to reach 3×, and cobble showing
  none;
- **stone steps get their own clause**: moss in the shaded joints, flowers
  absent. Not the ratio.

**BR-3's ratio is therefore SCOPED to the unmaintained classes: a cobbled
street failing it is a PASS**, and a suite that reds there is measuring the
rule's scope rather than the world.

Two implementation consequences, both of which invert something built earlier
in this same spec — worth reading before "simplifying" either:

1. **§3.12's mechanism 2 is exactly what would garden a cobbled gutter.** The
   edge gradient FLOORS the clump field so a coverage gap can never bare a
   margin — i.e. *the machinery installed to guarantee BR-3 is the machinery
   that breaks this ruling.* The floor is therefore scoped by the same
   per-class column: zero on the maintained classes.
2. **A KEPT VERGE IS NOT BARE GROUND.** §1.1 does not stop at the town gate,
   and a margin suppressed to nothing would re-make «земля плоская и мёртвая»
   inside the settlement — the complaint this stage exists to answer. So the
   weight scales the edge PEAK, never the base presence: weight 0 means
   "ratio ≈ 1, no peak", never "no life". Maintenance reads by *where life
   survives a broom* — moss and weeds in joints, at wall bases, in the lee of
   steps and thresholds — and the swept ground between those pockets is what
   makes them read as **spared** rather than as leftover. Asserted: at
   richness 0 the margin falls back to exactly the field value, not to zero.

Schema is `PathClassRichness` on each edge rule (`FloraEdgeRules.h`). **The
ordinals are core's `PathClass` positions and nothing checks it**, because
`world` and `render` are siblings in the DAG and neither may include the
other; the mapping is pinned by a test and stops being a seam at the migration
already scheduled for this table (Rule 5, core's JSON reader), where both
declarations are visible and a `static_assert` replaces the convention.

**The rich edge set (в8/в19в; `FloraEdgeRules.h` + seven patch species).**
Species = `GroundForm` + numbers, the tree doctrine one level down. Design's
flower palette roles, adopted verbatim: CARPET blue-violet (hue vs grass,
luminance ~0.40 clear of the sky band), ACCENT white/yellow (the VALUE
carrier — what makes a margin read lit), JEWEL deep red (a PLACEMENT BUDGET,
never common scatter — asserted in the suite), UMBEL pale greenish cream
(water margins, the birch bank-line's ground echo — re-coloured off the
accent after the pairwise floor caught them 0.14 apart: one species in two
habitats is not two species). Plus MossPatch, Mushroom, PebbleCluster, and
`StuntedPine` — the §5.12 talus krummholz, same whorl generator with dwarf
numbers (first cut read as a SAPLING; squatness, not smallness, is the dwarf
read). "Boulder with moss" is a COMPOSITION — stone + MossPatch on its shade
azimuth — not a new mesh.

Two rules earned here, both the shared-helper pattern (Rule 32):

- **Attachment applies at 0.2 m exactly as at 20 m**: flower heads and caps
  must touch their tuft or stem; asserted with a floated-head control. The
  no-detached-foliage complaint does not have a minimum size.
- **`emit_card_cluster` applied CANOPY_CLEARANCE_MIN to every card species**
  — invisible until the first non-canopy card species existed, then three
  symptoms from one mechanism (foliage shoved to 2.2 m, cards shrunk against
  the reduced span, cards torn off their anchors). The clearance floor is now
  the TREE's own (`Tree::clearance_floor`), zero for non-canopy card species
  by classification, exactly like bushes.

Edge-placement RULES ARE DATA (`FLORA_EDGE_RULES`): per (species, habitat) a
lateral band from the feature edge, a per-100 m density, the clump class it
multiplies, and the association (shade-of-stone, shade-of-trunk,
near-find-only). Built before paths exist so the day core's generator lands,
the margins are ready; the eventual home is core's JSON reader (Rule 5), this
header is the normative content.

### 3.7 Two bugs the suite caught — read this before touching the envelope

Both were in the envelope code, both were invisible to the eye in a wireframe,
and both would have surfaced as someone else's problem.

1. **Branches overshot the species height band** (oak topped 38.7 m against a
   32 m `OAK_HEIGHT_MAX`). The trunk was built to full nominal height and
   branches then grew *above* it. This is not cosmetic: the height band is a
   **cross-zone contract** — core's canopy occlusion and design's C4 arithmetic
   both key off `OAK/PINE/BIRCH_HEIGHT_MAX` — so the geometry was silently
   taller than the model every validator checks against. Fixed with
   `trunk_height_frac()`: broadleaf leaders dissolve into the crown at ~68 % of
   height (which is also the botanically correct shape), conifer leaders are the
   top, plus a hard envelope clamp on every branch segment and cluster.
2. **The envelope's radial clip was missing.** §3.1 stage D says branch lengths
   are clipped to the envelope; only the *vertical* half was implemented. Oaks
   came out 24.5 m wide against a 10–16 m brief and pines 24.9 m against 6–9 m.
   Width is load-bearing: **design derived `TREE_SPACING_FOREST` (12–18 m) FROM
   the crown width**, so a crown at double spec silently turns the thinned
   forest the user asked for back into a closed one.
   Inside that fix, a second-order trap worth naming: the clip began
   `if (p.y <= crown_base || env <= 0) return p;`. The `env <= 0` early-out was
   meant for crownless species but a **cone's envelope legitimately goes to zero
   at the apex**, so it disabled clipping exactly at the tip — one whorl branch
   stuck 7.6 m out of the top of a pine whose entire crown is 4 m in radius.
   `env == 0` must clamp, not skip.

3. **The crown did not exist at all** — caught only by the first rendered
   frame, after 31 000 assertions passed over a tree with zero leaves.
   §3.5 says branches under the shadow floor are not modelled *"and the
   remaining foliage attaches to the parent"*. The termination was implemented;
   the attachment was not. Measured primary-branch diameters against the 0.35 m
   floor: oak 0.468 (survives), pine 0.317, **birch 0.168**, willow 0.336 — so
   for three of the four canopy species every branch terminated instantly and
   took the entire crown with it. Birches rendered as bare curved poles. The
   pine survived only because its foliage is cone tiers rather than branch-tip
   clusters.
   Fixed twice over: foliage is emitted at a terminated branch's base, **and**
   `scatter_envelope_clusters()` distributes the crown over the envelope
   independently of the skeleton. That second half is the durable fix — foliage
   is what READS at distance (silhouette and value, §1.5); the branch skeleton
   is structure you only resolve up close, and a crown must not depend on it.

4. **The crown was foliage, but not a MASS.** After bug 3 was fixed the birches
   had leaves and still failed the brief: one cluster per evenly-spaced height
   at a fixed radius, azimuths on the golden angle, is a *helix* — on a tall
   narrow vase crown it rendered as a ladder of separated discs climbing the
   trunk. Correct at "foliage exists", wrong at "a crown reads as one mass at
   640x360", which is the only thing §1.5 actually cares about. Fixed by
   filling the upper crown (`crown_fill_start`), spreading clusters through the
   crown VOLUME rather than one shell, and enlarging the birch cluster radius
   until neighbours overlap. Note the test suite could not have caught this —
   foliage area, envelope containment and budgets were all already green. Some
   defects are only visible in a frame, which is why Rule 27 exists.

5. **The containment rule chose the wrong remedy.** Bug 4's fix made the birch
   crown denser without curing it — it still read as a stack of plates. The
   cause underneath: `emit_cluster` kept a cluster inside the envelope by
   **sliding its centre inward**. A birch's vase envelope is ~0.86 m in radius
   low in the crown while its clusters were 1.57 m, so every over-sized cluster
   slid all the way onto the trunk axis — twelve spheres at twelve heights, all
   centred on the axis, which is a stack by construction.
   Fixed by shrinking clusters to fit instead of sliding them, plus 5 large
   clusters rather than 12 small ones (a crown 5.6 m wide and 10 m tall reads as
   one mass only if the clusters are about as wide as the crown; many small
   clusters in a narrow crown can only be a stack — that is geometry, not
   tuning). Birch dropped 660 → 450 tris in the process.
   **The general lesson:** when a constraint can be satisfied by changing an
   element's SIZE or its POSITION, which one you sacrifice is a silhouette
   decision, not a geometry one. Preserving the declared radius mattered far
   less than preserving the arrangement, and the code preserved the wrong one.

6. **The birch crown was a COLUMN, and the container was always the problem —
   RESOLVED by design's ruling after four failed attempts inside this zone.**
   Cards did not cure it. In the first card frame the birch crowns read as
   narrow vertical sausages of foliage on a pale pole.

   **Measured, per variant, on the BUILT tree, foliage bounding box only.**
   *(A first report of these numbers pooled all 12 variants into one box, which
   measures the variant HEIGHT SPREAD as if it were one crown's shape and
   inflated every figure by ~15 %. Corrected within the hour and re-sent to
   design and the lead. The correction did not change the diagnosis, and the
   ruling had been made on the right side of the corrected band — but a
   measurement that is quoted in NUMBERS.md has to be right, and pooling
   variants is now a named trap in §6.)*

   | species | before | after | design band |
   |---|---|---|---|
   | DaleOak | 1.53 | **1.28** | — |
   | ValeWillow | 1.37 | **1.25** | — |
   | RiverBirch | **2.30** | **1.02** | — |
   | HighlandPine | 4.23 | 4.23 | exempt: a cone is MEANT to be narrow |

   **The cause was arithmetic, not distribution.** Design's crown width (5-7 m)
   and crown base (<= 45 % of a 16-22 m height) together defined a container
   ~1.8:1 before a single cluster was placed, and the built tree came out
   2.3:1. **This is why three previous fixes all failed**: bug 4 changed the
   distribution, bug 5 changed the shrink rule, and this stage changed the
   medium from solid blobs to cards — three different answers to "what goes in
   the box" when the box was wrong. *When three fixes at one level all fail,
   the defect is at the level above.* Design records the same diagnosis about
   the mountain (§2.8.1, «купол сидел в ЗАДАНИИ, а не в отрисовке»), so this is
   the second instance and now a cross-cutting lesson rather than an anecdote.

   **Design's ruling, and it is better than the exception flora asked for.**
   `CROWN_BASE_FRACTION` was silently doing TWO jobs: clear trunk height (a
   walkability goal — and §1.3 measured it visibility-insensitive to three
   decimals, so it was never a sightline parameter) and, purely by being a
   fraction of height, the crown's ASPECT RATIO. They pull apart on any species
   whose crown is narrow relative to its height; the birch was merely the first
   to expose it. So: `_MIN` 0.35 stays as a WALKABILITY FLOOR, `_MAX` 0.45 stops
   being a binding cap, and each species' crown base is **derived** as the
   smallest value at or above the floor that satisfies a new
   `CROWN_ASPECT_MAX`. No per-species taxonomy to maintain — it just derives.

   **`CROWN_ASPECT_MAX` is MEASURED ON THE BUILT TREE, and that is the whole
   point.** The birch's container was 1.8:1 while the tree it produced was
   2.3:1, so a ceiling checked against the parameters would have passed the
   tree that fails. *(Numerical coincidence worth naming before it confuses
   someone: the container ratio 1.8 and the constant's value 1.8 are unrelated.
   The container figure is a MEASUREMENT of the old birch; the constant is
   design's chosen ceiling. Cite the NAME, never either literal — Rule 14, and
   the value moved once already while this section was being written.)* The generator now asserts it on itself at every build
   (§6), which is where this should have been caught instead of on the fourth
   screenshot. The value is **provisional**: the evidence is that 1.53 reads
   and 2.30 does not, and the band between is untested. **The frame outranks
   the number — if something under the ceiling still reads as a column, the
   ceiling moves, not the tree.**

   **Two flora-side defects the same measurement exposed, both the §3.7
   pattern:**
   - *Vertical clamps used the card's half-height, but a card is tilted in
     elevation and rolled in its own plane, so what reaches is its CORNER.*
     Cards hung below the crown base and pushed the measured box outside the
     container — the exact quantity the new ceiling is measured on. Third
     instance of "enforce on the thing that actually reaches".
   - *Crown width was a BAND and only its maximum was ever asserted.* The birch
     had drifted to 3.6-4.5 m against a 5-7 m brief — a third narrower than its
     brief, and the other half of why it read as a column — with a green suite
     throughout. Both bounds are now tested, at nominal size only, because
     design's maturity tiers scale trees x0.4..x1.5 and take crowns outside any
     band by construction.
   - Foliage never reaches the envelope's widest point (containment holds the
     card's corner inside, and the widest ring sits where a card would overshoot
     the crown top), so `crown_width_frac` is now **calibrated against the built
     tree**, not against the envelope: oak 0.45 -> 0.48, birch 0.30 -> 0.52.

   Also added: a legibility floor — a card shrunk by containment below a quarter
   of the crown radius is not emitted, because it renders as a detached scrap
   rather than as part of a crown. A guard, not the cure; it did not change the
   frame.

**THE PATTERN — read this before changing the envelope or the floors.** All
three bugs are the same failure: **a rule stated in full in this spec, and
implemented in half.** Vertical clip without radial clip. Height band enforced
downward but not upward. Branch termination without foliage re-attachment. In
each case the implemented half was tested and held; the unimplemented half had
no test and drifted 2–3× out of brief, or to zero. When a rule here has two
clauses, write a test per clause — and note that all three survived a green
suite, so "tests pass" is not evidence that a rule is implemented.

The three now have regression tests (§6), including the one that was missing
entirely: *does this species have a crown at all*.

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
9. **Ground contact** — the root flare reaches at least 0.9 m below the model
   origin, so the skirt buries itself on real terrain (§3.5).
10. **Crown width bands** — added after the radial-clip bug (§3.7): oak ≤ 16 m,
    pine ≤ 9 m, birch ≤ 7 m, willow ≤ 16 m across every variant.
11. **Crown shyness has an effect** — a shy instance reaches less far toward its
    neighbour than a plain one. A parameter with no measurable effect is a
    parameter that will be quietly broken later.
12. **Every canopy species HAS a crown** — foliage area ≥ one crown
    cross-section, and the highest foliage above `crown_base`. Measured as
    **area, not vertex count**: a conifer's cone tiers cover the whole envelope
    with 72 vertices, so a vertex-share threshold fails the pine while passing
    a bald oak. Area is what the eye integrates, so area is what is asserted.

13. **Crown aspect ceiling** — `CROWN_ASPECT_MAX` on the GENERATED foliage
    bounding box, broadleaf only, conifers exempt as a property of their
    silhouette brief. This is design's §5 acceptance rule and the one that
    would have caught the birch on the first build instead of the fourth
    screenshot.
14. **Crown width has a FLOOR as well as a ceiling** — design's widths are
    BANDS and only the maximum was ever asserted, which let the birch drift a
    third under its brief with a green suite. Checked at nominal size only:
    maturity tiers scale trees x0.4..x1.5 and take crowns outside any band by
    construction.
15. **No card below a quarter of the crown radius** — containment shrinks
    cards, and past that point one stops joining the mass and starts reading as
    a detached scrap hanging under the crown.

**MEASURE PER VARIANT, NEVER POOLED.** Pooling the 12 variants into one
bounding box measures the variant HEIGHT SPREAD as if it were one crown's
shape. It inflated the first report of the birch aspect by ~15 % (2.65 against
a true 2.30) and that wrong number reached NUMBERS.md before it was corrected.
Anything shaped like "measure the built tree" is per variant.

**Measured output at the time of writing** (max across 12 variants; the suite
pins these as bands, this table is the snapshot):

| Species | tris Full / Reduced / Silhouette | height | width |
|---|---|---|---|
| DaleOak | 240 / 200 / 54 | 20.6–27.5 m | 14.5 m |
| HighlandPine | 536 / 386 / 48 | 28.1–37.7 m | 7.6 m |
| RiverBirch | 180 / 180 / 54 | 14.2–17.7 m | 4.8 m |
| ValeWillow | 230 / 190 / 54 | 12.1–16.9 m | 14.8 m |
| Snag | 108 / 92 / 30 | 10.2–19.1 m | 8.8 m |
| Bush / BigBush | 78 / 114 | 1.0–1.5 / 2.8–3.9 m | 1.8 / 4.0 m |
| FallenLog / Deadfall | 48 / 30 | ⌀0.6–1.0 / 0.2–0.3 m | 13.7 / 3.7 m long |

Every species is inside `TREE_TRI_BUDGET_MAX` = 700 with the pine the most
expensive at 536. **Known conservative gap:** the oak's mesh top (20.6 m on the
smallest variant) sits below its nominal band floor of 24 m, because foliage
clusters stop short of `crown_top`. This errs in the SAFE direction — core
models the canopy taller than it is, so C1 validation is pessimistic rather than
optimistic — but it should be closed when the first frame is read.

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
- **Grass, flowers, micro scatter.** SUPERSEDED 10.08.2026 by the landscape
  stage brief: flora now owns everything that grows and lies on the ground —
  the §3.12 patch species, the clump field, and (pending the lead's Task 4
  gate) grass tufts. Placement remains core's; drawing remains render's. An
  earlier version of this bullet excluded ground cover from the zone; a
  successor reading old citations of it is reading a stale copy.
- **Structures.** `build_site_mesh` and everything in LANDSCAPE §6 stays with
  render.
- **Content data files.** Species tables live in C++ today (like `ProcMesh`);
  moving them to `assets/` per Rule 5 is a scheduled future step (§5 item 2),
  not a silent omission.
