<!--
Created: 09:08:2026 - 10:45:06
Last updated: 09:08:2026 - 15:33:48
-->
<!--
UPD:
- 09:08:2026 - 10:45:06: Created the landscape design bible (stage 3): composition principles, detail layers with worldgen pass order, water rules, terrain palette, flora and structure catalogs, testbed v2 plan, sources. All new numeric values are proposals pending NUMBERS.md approval.
- 09:08:2026 - 12:44:58: Amendments from render's stage-3b look-dev probes: C1/C4 visibility validation is now canopy-aware with a clearance factor and L0 sight wedges (pine wall buried the crag from town); riverbed/mud splat band capped (WaterBed 2.74% vs ~1% water was over-wide); dist_to_water field range requirement; §7.1 fords are now derived from the generated trace (fixed coords removed), seed-1 generated hydrology actuals recorded, hamlet flood-margin re-score flagged.
- 09:08:2026 - 13:15:01: Core implemented all three rulings — doc synced to built state: crag pines are radial ridge strips (closed annulus can never pass canopy-C1), peak-raising marked a dead-end knob, §3.1 fords strengthened (bed truly raised; whole corridor mask ford-shallow), §7.1a open items resolved (lake "sprawl" was pond overflow, true basin at target), new rule: water-adjacent placements are derived-only, never tabled.
- 09:08:2026 - 13:17:06: §2.1 landform anisotropy rule from user feedback (feature_requests.md Запрос 1): mid-scale hills must be elongated direction-coherent ridgelets, not round bumps; HILL_ANISOTROPY 2.0-3.0 proposed.
- 09:08:2026 - 13:18:17: §2.1 anisotropy sharpened per core's implementation intent (agreed at sync): mid octave only, drifting per-valley axis field (no global corduroy), recorded cautions — river trace will shift (safe under the §7.1a derived-only rule) and work is gated on HILL_ANISOTROPY landing in NUMBERS.md + lead scheduling.
- 09:08:2026 - 13:19:34: §2.1 technique decided (core + design): anisotropic input-stretch chosen over domain-warp — elongation along the axis, cross-axis rhythm pinned at 128 m (what corridors/C1 grid feel); domain-warp rejected (wiggles crests, dilutes the shared-axis read); ping-first threshold at ~100 m cross-axis compression.
- 09:08:2026 - 13:22:17: HILL_ANISOTROPY approved at 2.5 in NUMBERS.md (stage-3 close, sync №3) — §2.1 proposal marker removed, gates recorded as cleared, P1 retune scheduled with §2.1 as the contract.
- 09:08:2026 - 15:00:23: New §6.1 — castle (House Corvane's seat, story pitch A) ruled as L1-max staged inside the L0's angular footprint: crag keeps the skyline, castle reads against its body, flank occlusion allowed / crown occlusion forbidden, scored in C1 both as occluder and as attractor, binding fix order on C1 failure. Siting (spur pad, ford command, barrow proximity, composite POI), minimal-version mass table, mid-range readability ruling, testbed pad at (760,330); castle row added to §7.1.
- 09:08:2026 - 15:05:00: §6.1 folded in story's constraints for "Harrowward": gentry hall-castle mass program (horizontal hall + single solar vertical replaces the tall keep — also buys C1 clearance headroom), value-not-height doctrine tying the Ward to the crag's rock value, binding access invariant (graded ramp on the approach side within §2.4 corridor limits — a scarp-only pad is a failed placement), and a checkable Ward→Backbarrow sightline. CASTLE_KEEP_HEIGHT retired in favour of CASTLE_HALL_HEIGHT/CASTLE_SOLAR_HEIGHT/CASTLE_GATE_HEIGHT.
- 09:08:2026 - 15:08:24: §6.1.2 — gate orientation settled as valley-facing (story canon, BIBLE §5.1), and the two new castle invariants (approach ramp, yard/gate->barrow sightline) explicitly joined to the C1-guarded set: re-validated by the same canopy-aware raycast on every worldgen run, not once at authoring time (raised by story: a later pine retune could occlude either).
- 09:08:2026 - 15:22:13: C2 scope corrected (my error): POI_VISIBLE_COUNT is region-only per NUMBERS.md/Q46 and is unsatisfiable at testbed density alongside LANDMARK_VISIBILITY_MIN — general-bound citation withdrawn. Added Rule C2-testbed (no coequal crowd): max 2 attractors within a 2.0 subtended-size ratio, L0 exempt, composite POIs count once; occlude-and-reveal remains the real guarantee. Region bound unchanged.
- 09:08:2026 - 15:33:48: C2-testbed limit raised 2 -> 3 on measured evidence (seed-1 crowd is three threshold-scale marks at 8-11 px), with a tightening to 2 for large crowds (COEQUAL_LARGE_PX 24 px). Blessed core's three measurement definitions into the doc: apparent size = height/distance (not top elevation angle, one definition shared by C2/C4/R4), only >=SILHOUETTE_MIN_PX attractors compete, body-backed attractors exempt per standpoint with the raw count still reported.
-->

# LANDSCAPE.md — Landscape & World Design Bible

Owner: `design` (Rule 25). This document drives worldgen v2 and the placement
passes. Every rule here is written to be executable by a noise-based generator
or a deterministic placement pass (Q13: procedural base, hand-editing arrives
later via the editor). No rule requires hand sculpting.

Conventions used below:

- **(предложение — утвердить)** — a number invented in this document. It is a
  candidate for NUMBERS.md; the lead adds it after approval. Code must
  reference the future constant name, never the literal.
- **FUTURE** — depends on a system that does not exist yet (roads, quests,
  day cycle, swimming, region-scale biomes). Design intent recorded now so
  worldgen v2 does not paint itself into a corner.
- Existing constants are cited by NUMBERS.md name (`WALK_SPEED`, `CHUNK_SIZE`,
  `POI_TRAVEL_TIME`, ...). Units per Rule 14: meters, seconds, radians.

---

## 1. Composition principles

### 1.1 The one law: пустота — наш враг, but readable emptiness

Density beats area (DECISIONS §1, Q12/Q41). For landscape this means: the
player must **always have a reason to pick a direction**. The tool for that is
not more objects — it is *composition*: landmarks, occlusion, and reveal.
An empty meadow between two visible goals is pacing (a rest beat, per
level-design practice); an empty meadow with nothing on the horizon is a bug.

**Rule C1 — no dead horizon.** From any walkable point, at standing eye height
(`PLAYER_EYE_HEIGHT`), the player must see at least one *attractor*: the
dominant landmark, a secondary landmark, or a local guide (§1.3). On the
region this is the `POI_VISIBLE_COUNT` contract (1–3 simultaneously visible);
on the testbed the same rule holds with the tighter spacing of §1.2.
Verification: a worldgen validation pass samples a coarse grid of standpoints
and raycasts against the **occlusion heightfield** + landmark bounding shapes;
any standpoint with zero visible attractors fails the seed. The occlusion
heightfield is terrain **plus canopy**: every sample inside a forest mask or
under a placed tree adds that species' max height (render's stage-3b probes
proved terrain-only raycasts pass seeds where a pine wall buries the L0).
This is computable, not editorial.

**Rule C2 — never show everything.** The complement of C1. Breath of the
Wild does this with the "triangle rule": convex terrain masses (hills, crags)
occlude what is behind them, so content is revealed a couple of items at a
time as the player moves. Our gentle-hills base already gives convex rolls;
worldgen v2 must *keep* macro convexity between POIs — do not flatten the
land between two POIs into a plane where both plus three more are visible.

**Scope — corrected 09:08:2026 (design error, my import).** The absolute
bound `POI_VISIBLE_COUNT` = 1–3 is **region-scale only**: NUMBERS.md carries
"—" in its testbed column, and Q46 forbids applying one density contract to
the other. My original C2 text cited it as a general bound; that was an
overreach and is withdrawn. Two reasons it cannot govern the testbed:

1. **It is unsatisfiable alongside constants we already approved.** C1
   requires the L0 visible from ≥ `LANDMARK_VISIBILITY_MIN` (0.6) of open
   ground, so the L0 occupies one "visible attractor" slot almost everywhere
   by construction; C3 packs POIs at 180–270 m across a 1024 m testbed —
   3× the region's spacing. Holding ≤ 3 total would demand occlusion heavy
   enough to push C1 back under its own floor. The two rules pull in opposite
   directions at testbed density, and C1 wins: a valley whose landmark you
   cannot see is the worse failure.
2. **It is not caused by any placement.** Measured 5 attractors from the west
   meadows both with and without the castle — this is structural to the
   testbed's compactness, not a defect a placement pass introduced.

**Rule C2-testbed — no coequal crowd.** What actually overwhelms a player is
several attractors of *comparable* apparent size competing as equivalent
choices; a legible hierarchy of different scales reads as one composition with
depth (this is why the stage-3b tour frames read cleanly at 5 visible). So on
the testbed:

- At most `POI_COEQUAL_VISIBLE_MAX` = **3** attractors of comparable apparent
  size — within `COEQUAL_ANGLE_RATIO` = 2.0 of each other in subtended
  height — may be visible from any standpoint **(предложение — утвердить)**.
  **Tightens to 2 when the crowd is large:** if every member subtends
  ≥ `COEQUAL_LARGE_PX` = 24 px (3× the §1.5 readability threshold), only 2
  may compete **(предложение — утвердить)**. Three marks at the readability
  floor are a vista; three masses filling the view are a menu.
- **Apparent size means subtended height = object height / distance**, never
  the elevation angle of the object's top. The angle measure conflates size
  with ground elevation (a hut on a high shoulder scores as a landmark) and
  produced a phantom crowd in the first seed-1 measurement. Same measure
  governs C4 and §6.1.1 R4 — one definition across all three rules.
- **Only attractors that clear the §1.5 readability threshold compete**
  (`SILHOUETTE_MIN_PX` = 8). What you cannot resolve as a shape cannot
  compete for your attention; sub-threshold objects are texture, not choices.
- **Body-backed attractors are exempt per standpoint:** an attractor inside
  the L0's angular footprint and nearer than the peak does not count, because
  by R1 it reads against the crag's body and cannot claim the skyline. This
  is §6.1.1's siting mechanism applied at view time. The raw unexempted count
  is reported alongside and must stay visible in validation output — the
  exemption is an interpretation, and interpretations get audited.
- The **L0 is exempt** from the count: C1 mandates its ubiquity, so counting
  it against a visibility cap is self-contradictory.
- Composite POIs (hamlet, castle+barrow) count **once**, per §6.1.2.
- The real anti-overwhelm guarantee remains §1.4's occlude-and-reveal rule
  (each POI 30–80 % hidden from its approaches), which is already validated
  and unaffected.

Region scale keeps the absolute `POI_VISIBLE_COUNT` bound unchanged.

### 1.2 Spacing derived from our metrics

All spacing follows from `POI_TRAVEL_TIME` × `WALK_SPEED` (3.0 m/s):

| Context | POI_TRAVEL_TIME | Implied POI spacing (nearest neighbor) |
|---|---|---|
| Testbed | 60–90 s | **180–270 m** |
| Region | 180–300 s | **540–900 m** |

These are *derived*, not new constants — the doc uses `POI_SPACING` as
shorthand for `POI_TRAVEL_TIME * WALK_SPEED`. The two density contracts are
different on purpose (Q46); never tune one to match the other.

**Rule C3 — the POI chain.** Every POI must have at least one neighbor POI
within the spacing band above (graph built by nearest-neighbor links). A POI
whose nearest neighbor is beyond the band is isolated → the generator must
either move it or insert a minor POI (guide-scale, §1.3) between. This makes
the "walk 60–90 s, find something" promise checkable by a validation pass.

### 1.3 Landmark hierarchy (weenies, three tiers)

Disney's "weenie" principle — a tall, high-contrast landmark with long
sightlines pulls people toward it — is the backbone of Skyrim's and BotW's
navigation. Adapted to our scales:

| Tier | Name | Count | Visible from | Examples |
|---|---|---|---|---|
| L0 | **Dominant landmark** | exactly 1 per valley (testbed = 1 valley; region: 1 per ~2–3 km valley cell, FUTURE) | ≥ 60 % of open walkable ground **(предложение — утвердить: `LANDMARK_VISIBILITY_MIN` = 0.6)** | rocky crag + tower ruin, lone mountain, great tree |
| L1 | **Secondary landmarks** = POIs | testbed 6–9 (see §7); spacing per §1.2 | 150–400 m depending on silhouette | hamlet, shrine spire, dungeon entrance, lake |
| L2 | **Local guides** | continuous fabric, every 40–80 m of travel **(предложение — утвердить: `GUIDE_INTERVAL` = 40–80 m)** | 50–150 m | rock outcrop, tree cluster edge, ford, flower patch, lone birch |

- L0 orients the whole valley ("the crag is north"). It must sit high and
  break the skyline (§1.5).
- L1 are the destinations; the POI chain (C3) runs through them.
- L2 exist so the 60–90 s walk between L1s is never featureless; they are
  produced by the meso layer (§2) and are cheap.

**Rule C4 — hierarchy contrast.** BotW's scale lesson: the three tiers must be
*unambiguous* at a glance. Enforce by silhouette height: L0 ≥ 25 m above local
terrain; L1 = 5–15 m; L2 ≤ 5 m **(предложение — утвердить, encoded in the
feature stamps of worldgen v2)**. Nothing that is not the L0 may exceed L0's
apparent height from the main travel corridors — *including canopy*.

Enforcement (added after render's stage-3b probes showed 15 m foothill pines
out-angling the 52 m crag from every western/southern ground vantage):

- **Clearance factor:** from every validation standpoint that C1 credits with
  seeing the L0, the L0's subtended angle must exceed every intervening
  occluder (terrain + canopy) by ≥ 20 %
  **(предложение — утвердить: `LANDMARK_CLEARANCE_FACTOR` = 1.2)**.
- **L0 sight wedges:** P5 precomputes 2D wedges from each L1/POI standpoint to
  the L0 footprint edges. Inside a wedge, any candidate tree whose canopy top
  would subtend ≥ `L0 angle / LANDMARK_CLEARANCE_FACTOR` from that wedge's
  standpoint is rejected (cheap: only trees inside wedges are tested,
  deterministic). Terrain-side tuning knobs if wedge filtering thins a forest
  too much: widen the L0 stamp's treeless rockline band, or reshape the
  landmark-facing forest — core's choice per seed, the invariant is the
  clearance factor. Two knobs proved to be **dead ends** in the stage-3b
  implementation (do not retry): raising the peak *lowers* clearance (its own
  flank occluders grow at a 1.2× disadvantage), and the treeline is useless
  here (foothill terrain sits below any sane treeline). The effective lever
  is forest *shape*: a closed canopy annulus around an L0 can never pass
  canopy-C1 from valley ground — landmark-skirting forest must be broken
  into radial/ridge strips with gaps (see §7.1).

### 1.4 Draw-the-player rules

- **Occlude-and-reveal:** an L1 should be *partially* hidden from at least one
  main approach (behind a hill shoulder or forest edge) so rounding the bend
  produces a discovery. Implementable: when placing an L1, prefer candidate
  positions where visibility from the two nearest POIs is between 30 % and
  80 % of the approach path (raycast sampling), not 100 %.
- **Curved travel:** never let the shortest walkable line between two chained
  POIs be a straight unobstructed rule across flat ground; macro pass keeps at
  least one convex mass or water bend adjacent to each POI-graph edge so real
  paths curve (BotW "orbiting" behavior).
- **Reward the dead end:** any terrain pocket the generator creates (bowl,
  box-canyon end, lake far shore) must receive an L2-or-better reward in the
  placement pass, or be sealed off. A pocket is detectable: walkable region
  whose exits ≤ 1.
- **FUTURE (roads/quests):** roads reinforce, never replace, sightline
  guidance; quest markers do not exist — the landscape *is* the quest marker.

### 1.5 Readability under the Daggerfall look (low-res, first person)

Internal resolution is `INTERNAL_RES` = 640×360 working default, with 320×180
still on the table (sync №2). With `CAMERA_FOV_Y` = 1.309 rad (16:9 →
horizontal ≈ 1.87 rad):

- Angular resolution: ≈ **275 px/rad vertical, ≈ 340 px/rad horizontal**
  at 640×360; exactly half at 320×180.
- A silhouette needs ≥ ~8 px to read as a shape. Therefore:
  **min readable feature size ≈ distance / 30** at 640×360
  (≈ distance / 15 at 320×180) **(предложение — утвердить:
  `SILHOUETTE_MIN_PX` = 8; design math, not a runtime constant)**.
  - At 250 m (mid POI spacing): features ≥ 8 m read. A house (5–8 m) is
    marginal → hamlets read as *clusters* + smoke (FUTURE), shrine spires
    (10–14 m) read individually.
  - At 550 m (across the testbed): only ≥ 18 m features read → the L0 must be
    a terrain-scale mass (crag), with its man-made topper (tower) becoming
    legible on approach.
- **Skyline rule:** silhouettes read best against sky. L0 and L1 verticals
  (spire, tower, lone pine) are placed on convex ground so that from the main
  corridors their top third breaks the horizon line. Implementable: candidate
  scoring by "sky behind top of bounding box" raycast.
- **No thin features at distance:** sub-pixel elements (masts, thin branches,
  fences) shimmer at low res. Distant-readable assets use thick silhouettes
  (§5, §6); nothing structural thinner than ~0.5 m matters beyond 100 m.
- **Value contrast over hue:** with the limited palette, tiers separate by
  *value* (dark crag vs light sky, pale birch vs dark pines). Every landmark
  brief in §5/§6 states its value contrast against its usual backdrop.

---

## 2. Detail layers and worldgen pass order

Three layers, placed in strict pass order. Determinism per Rule 13.1: every
pass draws only from `WorldGenRng` streams keyed by (seed, cell/chunk, pass).
For scatter that must agree across chunk borders, cluster/patch centers live on
world-space jittered lattices keyed by lattice-cell coords (same trick as the
value-noise lattice) — a chunk computes any center whose radius touches it,
so neighbors agree without communication.

| # | Pass | Layer | Produces |
|---|---|---|---|
| P1 | Macro heightfield | macro | base fBm + feature stamps (crag, valley flattening, redistribution) |
| P2 | Hydrology | macro | river path + carve, lake basin, water surface data, shore mask |
| P3 | Surface classification | macro | biome/splat inputs per sample: slope, height, dist-to-water |
| P4 | Sites & structures | meso | flattened building pads, POI placement, POI-chain validation (C1–C4) |
| P5 | Meso scatter | meso | forest masses, tree clusters, clearings, rock outcrops |
| P6 | Micro scatter | micro | grass/flower/stone parameters (density maps; instances mostly render-side) |

Sites (P4) run **before** forests (P5) so settlements reserve their clearings;
forests run before micro so grass respects canopy. This mirrors Horizon Zero
Dawn's layered placement: coarse density/exclusion maps first, fine scatter
sampled against them.

### 2.1 Macro (mountains, ridgelines, water bodies, forest masses)

- **When:** P1–P2 (+ forest *mass outlines* decided in P5 from a coarse
  moisture-like noise field, but their masks are macro objects).
- **What:** current gentle-hills fBm (octaves 512/24, 128/6, 32/1.5 — already
  flagged for NUMBERS.md at sync №2) plus, in v2: one ridged-noise crag stamp
  per valley (L0), valley-floor redistribution (`pow`-curve toward flats, per
  procgen practice), river/lake carving.
- **Landform anisotropy (user feedback, feature_requests.md Запрос 1):**
  mid-scale hills must read as elongated, direction-coherent landforms —
  ridgelets with a legible long axis — never isotropic round bumps
  («холмики-сиськи» are explicitly rejected by the user). Implementable
  without hand sculpting: stretch **the mid-frequency octave only**
  (currently the 128 m / 6 m layer — it is what makes round bumps at hill
  scale) by `HILL_ANISOTROPY` (**approved in NUMBERS.md: 2.5**, stage-3
  close) along a per-valley axis field; the macro-roll and fine-texture octaves
  stay isotropic, and the ridged transform on the L0 stamp already covers
  crag flanks. **Technique decided (plan of record, core + design sync):
  anisotropic input-stretch, not domain-warp.** Input-stretch lengthens
  features *along* the axis (the requested elongation itself) while the
  cross-axis wavelength — the rhythm corridors and the C1 standpoint grid
  actually feel when crossing ridgelets — stays pinned at the current
  128 m. Domain-warp is the opposite trade (preserves average wavelength
  but wiggles crests and dilutes the shared-axis read) — rejected.
  Ping-first threshold: if axis-field drift locally compresses the
  cross-axis rhythm below ~100 m, core pings design before it lands. The axis field is a slowly-varying seeded angle: ridgelets
  share a long axis *locally* while the axis drifts across the map — a
  single global direction would read as corduroy. Recorded caution
  (core, stage-3b sync): warping the hill octave shifts drainage
  micro-shape — the seed-1 river trace WILL move; this is safe *only*
  because of the derived-only rule (§7.1a), which is exactly the case that
  rule exists for. Gates cleared at stage-3 close (sync №3):
  `HILL_ANISOTROPY` = 2.5 landed in NUMBERS.md and the P1 retune is
  scheduled — canopy-aware C1 re-validation is automatic in the suite,
  and a compliance pass over the retune tour frames follows. Acceptance: tour
  frames of open meadow show hills with an obvious long axis roughly
  agreeing with their neighbors.
- **Quantization warning (core contract):** all chunks share one quantization
  range (offset 0, scale MAX/65535). Raising the L0 above the current 31.5 m
  ceiling requires raising the shared range — **(предложение — утвердить:
  `WORLDGEN_MAX_HEIGHT` = 64 m)**. Height resolution stays ~1 mm; no contract
  change beyond the constant.
- **Density guidance:** exactly 1 L0 per valley; forest masses cover 25–40 %
  of walkable land **(предложение — утвердить: `FOREST_COVERAGE` = 0.25–0.40,
  testbed target 0.30)**; open meadow the rest minus water/rock.
- **Must never:** place macro mass so that it hides the L0 from more than
  40 % of open ground (violates C1/`LANDMARK_VISIBILITY_MIN`); create local
  minima with no hydrology resolution (§3); exceed walkable slope on all
  approaches to any POI (every POI keeps ≥ 1 approach corridor under 25°
  average slope — see critical-path rule §2.4).

### 2.2 Meso (hills, clearings, river bends, outcrops, tree clusters)

- **When:** P4–P5.
- **What & density (all предложение — утвердить):**
  - Forest interior clearings: one per 150–250 m of forest extent
    (`CLEARING_INTERVAL`), radius 15–30 m (`CLEARING_RADIUS`); clearings host
    L2 rewards (flower patch, stone, FUTURE camp).
  - Rock outcrops in open land: on a jittered lattice of cell 120 m
    (`OUTCROP_CELL` = 120 m, ~1 per 14 000 m² with 30 % skip chance); 2–6
    boulders each, 1–3 m; prefer convex ground and slope 15–35°.
  - Tree clusters outside forest masses: 3–7 trees, on a jittered lattice of
    cell 90 m in meadows (`MEADOW_CLUSTER_CELL` = 90 m, 40 % skip).
  - River bends: hydrology path smoothing keeps sinuosity ≥ 1.15 (path length
    / straight distance) so banks create pockets and reveal beats.
- **Must never:** block the POI-chain corridors (§2.4); violate the L0 sight
  wedges / clearance factor of C4 (checked by the canopy-aware raycast
  validation of C1 — terrain-only checks are insufficient, see C4); float
  above or intersect water.

### 2.3 Micro (grass, flowers, bushes, stones, sand patches)

- **When:** P6 computes deterministic *parameters* (density maps, patch
  centers); actual instancing is render-side against those maps (Horizon
  model), so micro never enters the .dfw entity list and never costs
  streaming-path ECS churn (Rule 11 friendly).
- **What & density (all предложение — утвердить):**
  - Grass cards: only within `GRASS_VIEW_DISTANCE` = 50 m of camera,
    0.5–1.5 cards/m² on grass-splat ground (`GRASS_DENSITY`); at low res more
    is visual noise.
  - Flower patches: centers on 60 m jittered lattice in open grass, 50 % skip;
    patch radius 3–8 m; 1–3 blossoms/m² inside (`FLOWER_PATCH_*`). One accent
    hue per patch (palette discipline).
  - Loose stones: 0.005–0.02 /m² on grass and dirt (`STONE_DENSITY`), size
    0.2–0.6 m.
  - Bushes: forest edges (within 10 m outside a forest mask edge) and clearing
    rims, 0.01–0.03 /m² there (`BUSH_EDGE_DENSITY`).
  - Sand patches: from the shore mask only (§3.3) — never freestanding inland
    at this stage.
- **Must never:** affect collision or the critical path (micro is
  walk-through by contract); hide interactables (Q11 highlight must stay
  visible over grass — cap grass height at 0.4 m, `GRASS_HEIGHT_MAX`);
  exceed the render micro budget (render zone owns the actual instance caps —
  these densities are the *design* ceiling).

### 2.4 Critical-path protection (applies to every layer)

The POI chain of C3 defines corridors: straight-ish bands 10 m wide
**(предложение — утвердить: `CORRIDOR_WIDTH` = 10 m)** between chained POIs,
refined by a cheap downhill-biased path trace. Inside a corridor:

- slope along the walking direction ≤ 25° average (under `PLAYER_MAX_SLOPE`
  with margin), no step > `PLAYER_STEP_HEIGHT`;
- no structures, no forest-density trees (isolated trees allowed if spacing
  ≥ 12 m), no boulders > 1 m;
- rivers crossed only at fords (§3.1).

Corridors are a *mask* consumed by P4–P6, not visible content. FUTURE: roads
will be built along a subset of corridors.

---

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
4. **Monotonic water surface:** station water height = min(previous station,
   local terrain) — **the water surface never gains height downstream. This is
   the invariant; a river that climbs is a failed seed.**
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
  exit at its spill point. Ponds (steps 2 pond-and-spill) are the same at
  radius 5–15 m.
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

## 4. Terrain palette (splat rules a shader can implement)

Inputs per sample, all computable in P3 or in-shader from the heightfield:
`height` (m), `slope` (rad), `dist_to_water`, `height_above_water`.
Priority order (first match wins), thresholds **предложение — утвердить**:

| Priority | Material | Rule | Constant names |
|---|---|---|---|
| 1 | **Sand** | shore mask: `dist_to_water ≤ 6` and `height_above_water ≤ 0.6` | `SHORE_SAND_DIST`, `SHORE_SAND_HEIGHT` |
| 2 | **Rock** | `slope ≥ 0.70 rad (40°)` — hard rock, also all L0 crag stamp area above its rockline | `SLOPE_ROCK_MIN` |
| 3 | **Grass→rock blend** | `0.52–0.70 rad (30°–40°)`: dithered blend (fits the palette look better than smooth lerp at low res) | `SLOPE_GRASS_MAX` = 0.52 |
| 4 | **Grass** | everything else below the treeline band | — |
| FUTURE | Dirt/path | road pass | — |
| FUTURE | Snow | region mountains above snowline | `SNOWLINE_HEIGHT` (region) |

Design rationale, binding for render:

- **Visual = gameplay truth.** `PLAYER_MAX_SLOPE` is 0.87 rad (~50°). The
  rock material starts at 40° so that by the time ground *looks* fully rock,
  it is nearly unwalkable; grass is always walkable. The player learns the
  material language instead of testing every slope. Never let grass render on
  slopes above `PLAYER_MAX_SLOPE`.
- **Dithered transitions**, not wide soft gradients: at 640×360 a soft blend
  band reads as smear; ordered dither matches the retro look and keeps the
  palette small (sync №2's palette flag).
- **Treeline (region, FUTURE for testbed):** trees stop at
  `TREELINE_HEIGHT` (region-scale, ~180 m proposal) and the grass→rock blend
  shifts 5° earlier above it, giving bald summits — the classic
  mountain-meets-sky read. The testbed's 64 m ceiling has no treeline; the
  crag gets bald via its rock stamp instead.
- Max 4 materials in the splat at once (render budget + palette discipline).

---

## 5. Flora catalog

Global rules: every species is a low-poly hard-edged mesh, 2–3 flat colors,
strong value separation (readability §1.5). Placement is Poisson-disc /
jittered-lattice per §2 (never raw high-frequency noise threshold — it
clumps). Trees never spawn on rock or sand splat, never inside building pads,
corridors, or water. Slope limit for all trees: `TREE_SLOPE_MAX` = 0.61 rad
(35°) **(предложение — утвердить)**. All densities предложение — утвердить.

### 5.1 Dale Oak (broadleaf — the valley tree)

- **Silhouette:** short thick trunk (1/3 of height), one wide rounded crown
  mass, wider than tall overall. Reads as "ball on a stump" at 8 px.
- **Size:** 8–12 m tall, crown 6–10 m wide.
- **Poly budget:** 300–500 tris (`TREE_TRI_BUDGET` family).
- **Palette:** mid-green crown, near-black trunk; value: darker than meadow
  grass, lighter than pines.
- **Placement:** valley floors and slopes < 20°; the default forest-mass tree
  (S/SE forest masses); Poisson spacing 5–8 m inside masses
  (`TREE_SPACING_FOREST`), clusters of 5–15 in meadows.
- **Clustering:** high — oaks define forest interiors and edges.

### 5.2 Highland Pine (conifer — the slope tree)

- **Silhouette:** narrow triangle, 2–3 stacked cone tiers, tip must survive
  quantization (make the top cone ≥ 1.5 m wide). Tall and pointed — the
  anti-oak.
- **Size:** 12–18 m tall, base 4–6 m wide.
- **Poly budget:** 150–300 tris (cones are cheap).
- **Palette:** dark blue-green, darkest flora value in the scene — pines are
  the *dark mass* tool for composition backdrops.
- **Placement:** slopes 10–35°, higher terrain, crag foothills and northern
  ridges; spacing 4–7 m; follows ridgelines in strips 20–60 m wide (great for
  leading lines toward the L0). Pines are the main C4 hazard: 12–18 m of
  canopy on foothills out-angles the L0 from valley standpoints — pine strips
  on landmark-facing shoulders are subject to the L0 sight-wedge filter (§1.3)
  before anything else.
- **Clustering:** strips and wedges, not blobs; a lone skyline pine is a
  legitimate L2 guide.

### 5.3 River Birch (accent — the water tree)

- **Silhouette:** slim pale trunk (readable!), small loose crown, slightly
  leaning. The *light-value* accent against dark water or pines.
- **Size:** 6–10 m tall, crown 3–4 m.
- **Poly budget:** 200–350 tris (trunk needs a few more sides for the pale
  read).
- **Palette:** near-white trunk (brightest flora value), light yellow-green
  crown.
- **Placement:** within 20 m of water only (`BIRCH_WATER_DIST` = 20 m),
  clusters of 3–7; marks rivers/lake at distance — a birch line = water line.
- **Clustering:** loose lines along banks; never deep forest.

### 5.4 Bush

- **Silhouette:** 1–1.5 m hemisphere lump; groups of 2–4 read better than
  singles. **Poly budget:** 60–120 tris. **Palette:** oak-green, slightly
  lighter.
- **Placement:** forest-mask edges (≤ 10 m outside) and clearing rims,
  0.01–0.03 /m² there (`BUSH_EDGE_DENSITY`); softens the tree/meadow
  boundary so forest masses do not read as walls of sticks.

### 5.5 Flowers

- **Silhouette:** 2-quad cross billboards, 8–16 tris, height ≤ 0.5 m.
- **Palette:** one accent hue per patch (white/red/blue family); the only
  saturated accents in open meadow — they read as L2 guides.
- **Placement:** patch system per §2.3 (`FLOWER_PATCH_*`); open grass splat
  only, slope < 15°, never under canopy, never in corridors' driving line
  (they may edge it — flowers *beside* the path pull the eye forward).

### 5.6 Grass

- **Silhouette:** simple card tufts, ≤ 0.4 m (`GRASS_HEIGHT_MAX`), 2 tris per
  card. **Palette:** exactly the underlying splat color family ±1 value step —
  grass is texture, not information.
- **Placement:** render-side within `GRASS_VIEW_DISTANCE` = 50 m,
  `GRASS_DENSITY` 0.5–1.5 /m² on grass splat only (§2.3); density fades to 0
  at the view distance edge (no popping line at low res — dither the fade).

---

## 6. Structures catalog (домики под разные задачи)

Global rules: structures are placed in P4 on flattened pads — pad = footprint
× 1.5, max original slope under pad 0.09 rad (5°)
**(предложение — утвердить: `BUILDING_PAD_SLOPE_MAX`)**; the pad flatten is a
worldgen terrain stamp (procedural, not hand sculpt). Door side faces the
hamlet common or the water/corridor. All buildings: hard-edged low-poly,
2–3 colors + roof color as the *purpose code* (readable at cluster distance).
Tri budgets предложение — утвердить (`HOUSE_TRI_BUDGET` family). Buildings
never spawn in corridors, on sand, or within 2 m of water surface height
(flood margin — `BUILDING_WATER_MARGIN` = 2 m vertical).

| Purpose | Footprint | Height / silhouette | Tris | Distance read | Placement |
|---|---|---|---|---|---|
| **Dwelling** (cottage) | 6×8 m | 5–6 m; single gable, one chimney | 200–400 | generic "house lump" — reads as part of cluster | hamlet only, 3–5 per hamlet |
| **Trader / shop** | 8×10 m | 6–7 m; gable + full-width porch awning, hanging sign | 300–500 | porch shadow line | hamlet, faces the common, adjacent to corridor entry |
| **Tavern** | 10×14 m | 8–9 m; two stories, L-shaped, two chimneys | 400–600 | the *big* roof of the cluster; FUTURE: smoke column = day/night guide | hamlet anchor: largest pad, at the common's head |
| **Storage / barn** | 8×12 m | 7–8 m; steep tall roof (roof = ⅔ of silhouette), full-height doors, no windows | 200–350 | tall dark roof triangle | hamlet edge or farmstead; rotated gable-on to prevailing view (distinct from dwellings) |
| **Shrine / temple** | 5×5 m | spire 10–14 m; smallest footprint, strongest vertical | 250–450 | breaks skyline — doubles as L1 landmark | *outside* the hamlet on a knoll within 100–250 m; skyline rule §1.5 |
| **Watchtower ruin** (L0 topper) | 4×4 m | 10–15 m broken cylinder/prism | 150–300 | crown of the L0 crag | exactly one, on the L0 (§7) |

**Grouping rules (предложение — утвердить):**

- **Hamlet** = 4–8 buildings (`HAMLET_SIZE`) around an open common of radius
  15–25 m (`HAMLET_COMMON_RADIUS`): 1 tavern + 1 trader + 3–5 dwellings +
  1–2 barns. Building spacing 4–10 m, orientations within ±30° of facing the
  common (regular enough to read as "village", irregular enough to not read
  as a grid).
- **Farmstead** = 1 dwelling + 1 barn, 8–15 m apart, common yard; the
  single-building variant in open land. Density: FUTURE for region (needs
  roads); testbed has none unless a POI-chain gap needs one.
- **Shrine** always solitary (never inside a hamlet) — it is a navigation
  instrument, not street furniture.
- The hamlet counts as **one** L1/POI regardless of building count.
- FUTURE: purpose distribution per settlement size (village, town) — after
  the gameplay grill on economy/quests.

---

### 6.1 Castle — the seat of state power (ruling, stage-3)

User request: the world needs a seat of state power, present even in the
minimal version. Story picked pitch A (*The Debt of Harrowmere*), so this is
**House Corvane's seat**, standing on the land whose barrow is a mass grave.
The crown's distant capital is referenced in fiction, not built (FUTURE).
Fiction constraint taken as given from story: castle on or beside the barrow —
that proximity is a designed asset. Interior and any town around it are **out
of scope here** (not designed, not blocked).

#### 6.1.1 The hierarchy ruling (the actual problem)

A castle is a weenie by construction, and Ravenscar Crag is our L0. Two
dominant silhouettes in one 1024×1024 valley must not compete. Ruling:

**The crag stays L0 and keeps the skyline. The castle is L1-max — the
strongest secondary landmark, staged *inside the crag's composition*, never
against it.** The mechanism is siting, not height limits alone:

- **R1 — Site the castle inside the L0's angular footprint.** From the
  valley's main standpoints (town, lake shore, corridors), the castle must lie
  within the crag's angular width, so it reads **against the crag's body,
  never against sky**. A silhouette that cannot reach the horizon cannot steal
  it. At the sited position this holds automatically: from Vaelmere the castle
  sits ≈ 85 m lateral of the town→peak line while the crag subtends ≈ ±0.38
  rad — the castle is a dark notch on the crag's flank, sub-threshold at that
  range by §1.5, and the crag alone crowns the valley.
- **R2 — The castle may occlude the L0's flank, never its crown.** Flank
  occlusion is the desired "one composition" read (fortress at the foot,
  mountain above). Crown occlusion (top third of the L0) from any
  C1-crediting standpoint is forbidden.
- **R3 — Skyline margin.** Castle top elevation ≤ L0 peak −
  `CASTLE_SKYLINE_MARGIN` = 12 m **(предложение — утвердить)**; at seed 1
  (peak 52 m) that is ≤ 40 m absolute, i.e. a pad at ≈ 24 m carries a keep of
  ≤ 15 m.
- **R4 — Dominance ratio.** From valley standpoints ≥ 300 m where both are
  visible, castle subtended height ≤ `CASTLE_SILHOUETTE_RATIO` = 0.6 × the
  L0's **(предложение — утвердить)**. Inside the final approach (< 300 m) the
  castle is *allowed* to fill the view — that is the reveal (§1.4), and the
  crag still crowns it because the castle stands on its foot.

**C1/C4 scoring — the castle counts BOTH ways, explicitly:**

1. **As an occluder:** its full mass enters the occlusion heightfield exactly
   like canopy (§1.3), and the `LANDMARK_CLEARANCE_FACTOR` = 1.2 test applies
   to it. **The castle may never be the reason the L0 fails C1.** Seed-1
   headroom is 0.018 (C1 = 0.618 vs floor 0.6) — effectively zero, so this is
   a real risk, not a formality.
2. **As an attractor:** it counts toward the C1 "≥ 1 attractor visible" test
   and toward `POI_VISIBLE_COUNT`'s upper bound of 3 (it can push a standpoint
   over the limit — check both directions).

**Fix order if inserting the castle drops C1 below the floor** (binding — do
not improvise): (1) lower the pad elevation, keeping tower height; (2) shift
the pad further around the crag's south flank, away from the town sightlines;
(3) reduce tower height last. **Never** raise the crag (proven at stage-3b to
*lower* clearance, §1.3) and never accept a C1 drop.

#### 6.1.2 Siting rules

- **Terrain:** a spur/bluff shoulder of the L0 massif — high enough to command,
  low enough for R3. Needs a terraced pad: `CASTLE_PAD_SIZE` = 60 m square,
  pad surface within `BUILDING_PAD_SLOPE_MAX` (5°), with a dedicated cut/fill
  allowance `CASTLE_PAD_CUT_MAX` = 6 m **(предложение — утвердить)** — a
  documented exception to §6's ordinary pads, because terracing a spur is what
  real fortification does. Pad edges blend over 1.5× pad size.
- **Water/ford:** the castle **commands the crossing** — its pad is sited so
  the nearest derived ford lies inside its field of view and within
  `CASTLE_FORD_COMMAND_DIST` = 250 m **(предложение — утвердить)**. It does
  not create or move the ford (fords stay derived, §7.1a).
- **Barrow:** `CASTLE_BARROW_DIST` = 40–80 m **(предложение — утвердить)**
  from the barrow entrance — close enough that both are in one frame from the
  approach. The seat literally stands over the grave.
- **Vaelmere:** ≈ 390 m as sited — deliberately **beyond** one
  `POI_TRAVEL_TIME` hop. The castle is not a neighbourhood building; you
  travel to it. Chain integrity is preserved by the composite-POI rule below.
- **Composite POI:** castle + barrow count as **one** POI ("the seat"),
  exactly as a hamlet counts as one regardless of building count (§6). This
  keeps the §7.2 chain valid without over-densifying that stretch.
- **Corridors:** a castle implies an approach. The existing watchpoint→barrow
  corridor becomes the castle approach; the gate faces it. **FUTURE:** an
  actual road along that corridor when roads exist.
- **Access invariant (story-mandated, binding on terrain):** the Ward must be
  enterable by an unarmoured commoner on foot, on ordinary business, in
  daylight. Therefore the terrace's **approach side carries a graded ramp that
  satisfies the §2.4 corridor rules end to end** — average slope ≤ 25°, no
  step > `PLAYER_STEP_HEIGHT` — from the corridor up to the gate threshold. A
  spur pad whose only approach is a scarp is a **failed placement**, not a
  detail to fix later: cut the ramp in the same terrace stamp. The remaining
  pad edges may stay steep (that is the fortification read).
- **Barrow sightline (story asset):** the line of sight from the Ward's yard
  and gate to the Backbarrow entrance must be **clear** — the terrace's own
  cut/fill may not occlude it. The beneficiary lives within sight of the
  evidence; that is a checkable raycast, not a mood note.
- **Gate orientation — settled, do not re-litigate:** the gate faces the
  valley/ford, **not** the barrow. It serves the access invariant (the gate
  sits on the walkable approach) and it is the truthful read: landlords
  watching the crossing they own, with the grave behind the house. Because the
  yard→barrow sightline above guarantees the barrow is visible from their own
  ground, they have not hidden it — they have declined to face it. Story
  confirmed this is canon (BIBLE §5.1).

**Both new invariants join the guarded set.** The approach ramp and the
yard/gate→barrow sightline are occludable by later passes — a pine retune, a
scatter change, a terrace edit — exactly as the L0 sightlines are. They are
re-validated by the same canopy-aware raycast machinery as C1 (§1.3), on every
worldgen run, not checked once at authoring time. A seed that terraces away
the ramp or grows a strip across the barrow sightline fails, like any other
C1 failure.

#### 6.1.3 Footprint, mass, readability

**It is a gentry hall-castle, not a royal fortress** (story: "Harrowward",
House Corvane's small stone hall). That is a mass ruling, not just flavour:
the silhouette is **horizontal-dominant** — a long hall block with one modest
vertical — not a tall square keep. This *helps* every constraint above (lower
top elevation = more C1 clearance headroom) and it distinguishes the Ward from
the crag's ward-tower, which owns the valley's tall vertical.

Minimal version = **curtain wall + gatehouse + hall + solar block**, enclosing
an open yard. Chapel, granary and outer works are later elaboration and must
fit inside the same pad. Blocks, not filigree — §1.5 forbids sub-pixel detail
at range.

| Element | Footprint | Height | Tris | Reads as |
|---|---|---|---|---|
| Curtain wall | 40×40 m enclosure | `CASTLE_WALL_HEIGHT` 6–8 m | 300–500 | the horizontal base band |
| Hall | 10×22 m, inside the wall | `CASTLE_HALL_HEIGHT` 7–9 m | 400–700 | the long roof mass above the band |
| Solar block | 8×8 m, on the hall's end | `CASTLE_SOLAR_HEIGHT` 10–12 m | 250–400 | the single vertical — the Ward's "head" |
| Gatehouse | 10×6 m | `CASTLE_GATE_HEIGHT` 9–11 m | 200–350 | the notch + entry marker in the base band |
| Yard / tithe-yard | ≈ 20×20 m open | — | — | the dark interior gap read through the gate |

All heights предложение — утвердить, and all are subordinate to R3 (pad
elevation + tallest element is the binding constraint, not the table). The
envelope must accommodate story's act-1 interior set (public hall, yard,
muniment room, solar) — interiors are **not** designed here, only the footprint
that leaves room for them.

**Value, not height** (story's ask, and already §1.5 doctrine): the Ward is the
valley's only large pale-grey built mass. Stone against the meadow greens is
what makes it state power beside Vaelmere's timber-and-thatch hamlet — and
because it shares the crag's rock value, the two read as **one composition**
rather than two competing objects, which is exactly R1's intent.

Readability per §1.5 (`≥ distance / 30` at 640×360): the castle is a
**mid-range landmark**, designed to read from its approach corridor at
150–250 m (where a 15 m keep is 2–3× threshold), *not* from town across the
valley. From Vaelmere it is deliberately sub-threshold and read against the
crag body — you learn of the seat by travelling toward it. This is
occlude-and-reveal (§1.4) doing the work, and it costs nothing: the crag was
already the thing that pulls you that way. Silhouette discipline: one solid
keep mass + two framing towers + a horizontal wall band = three value steps,
dark against the crag's rock. No thin battlement teeth at this budget (§1.5:
nothing structural thinner than ~0.5 m matters beyond 100 m).

#### 6.1.4 Testbed placement (seed 1)

Pad center target **(760, 330)** ± 20 m — the crag's south-west foot spur:
≈ 55 m from the barrow entrance (780, 290), commanding the watchpoint ford
approach, ≈ 390 m from Vaelmere, and inside the crag's angular footprint from
every western/southern valley standpoint. `CASTLE_COUNT_TESTBED` = 1. Pad
ground target ≈ 24 m so R3 holds with a 15 m keep. Core solves the exact
position against the C1 re-validation; the invariants above are the contract,
the coordinates are a starting stamp.

## 7. Testbed application (worldgen v2, что core реализует первым)

Canvas: 4×4 chunks, world XZ = 0…1024 m both axes, seed 1, current surface
16–26 m. All coordinates below are *generator parameters* (a testbed layout
table in the worldgen tool's data), not hand sculpting — each is the center
of a procedural stamp/scorer, tunable and deterministic. **Все координаты и
высоты — предложение — утвердить.**

### 7.1 The plan (feature list, in pass order)

| Feature | Where (x, z) | Parameters |
|---|---|---|
| **L0: Ravenscar Crag** + watchtower ruin | peak (830, 200), footprint r ≈ 180 m | ridged-noise stamp raises peak to ~52 m (needs `WORLDGEN_MAX_HEIGHT` = 64); rock splat above ~34 m on the stamp; tower ruin (§6) on peak |
| **River** | source (760, 300) → lake; exits lake → south edge ≈ (300, 1024) | §3.1 algorithm; width 4→7 m; sinuosity ≥ 1.15; **fords are derived, not tabled** — P2 places them where POI-chain corridors cross the *generated* trace (§3.1 step 6), plus the `FORD_SPACING` minimum |
| **Lake** | center (230, 520), ≈ 90×140 m target | basin stamp, water plane ≈ 15.0 m (`LAKE_LEVEL_TESTBED`); shore sand per §3.3 |
| **Town site (TESTBED_TOWNS = 1): hamlet "Vaelmere"** | (360, 500), east lake shore / river inflow bend | hamlet per §6: tavern head faces the lake; trader at corridor entry; pads flattened at ≈ 17–18 m |
| **Shrine knoll** | (560, 620) | knoll +6 m local bump stamp; shrine spire breaks skyline from town and from ford (430, 620) |
| **Dungeon 1: barrow in the crag** (TESTBED_DUNGEONS 1/3) | entrance (780, 290), south face of the crag | entrance pad + dark portal frame; visible from foothill watchpoint, not from town (occlude-and-reveal) |
| **Castle: House Corvane's seat** (§6.1) | pad center (760, 330) ± 20 m, crag SW foot spur, ground ≈ 24 m | terraced 60 m pad; keep ≤ 15 m (R3); composite POI with the barrow; commands the watchpoint ford; scored in C1 as occluder AND attractor |
| **Dungeon 2: forest ruin** | (620, 850), inside SE oak forest | in a clearing (r = 25 m); ruin walls = L2 from clearing edge |
| **Dungeon 3: lakeshore cave** | (180, 350), NW lake shore under a 10 m bluff stamp | reachable along the sand shore; visible across the water from town (water gap = curiosity) |
| **Foothill watchpoint (minor POI)** | (660, 430) | rock outcrop cluster + lone skyline pine + ford; bridges the town↔barrow gap in the POI chain |
| **Forest masses** | oak: S+SE band (roughly z > 700 plus x > 500, z > 600); pine: **radial ridge strips** on the crag foothills (4 sectors, duty 0.25 — layout knobs `pine_strip_count`/`pine_strip_duty`; a closed annulus can never pass canopy-C1, see §1.3) + N ridge strips | total coverage ≈ 0.30 of land; clearings per §2.2; birch lines along river and lake banks (derived from `dist_to_water`, never tabled) |
| **Meadows** | center and west | flower patches, outcrops, meadow clusters per §2.2–2.3 |

### 7.1a Plan vs generated truth (seed 1, stage-3b probes)

The layout table rows are *stamp centers and targets*; the generated world is
the truth, and validation runs against it (tour v3 already aims at generated
truth). Render's probes of the actual seed-1 build recorded this drift:

- River trace: (730, 320) → (560, 500); outflow leaves the south edge at
  x 300–335.
- The originally tabled ford coordinates did not land on the generated river
  (probe at (430, 620): grass, 60 m from water) — which is why fords are now
  derived (§7.1), never tabled.
- The "flooded bend" at x 320–480 / z ≈ 560 and the apparent oversized lake
  (x 188–274 / z 460–700) measured by the first probes were **pond-and-spill
  overflow sprawl**, not the basin: the §3.3 mud-cap rule drains pond water
  beyond max(`SHORE_SAND_DIST`, 2 × local width) of the trace, after which
  the true basin sits at its 90×140 m target. Total water settled at ≈ 2.3 %
  of the world (lake 0.96 + channel 0.6 + capped bend pools ≈ 0.75).

Resolution (same day, core): fords derived at corridor × trace intersections
+ `FORD_SPACING` gap fill; corridor water depth validated ≤ `FORD_DEPTH_MAX`;
Vaelmere ring and pads dry with > `BUILDING_WATER_MARGIN` clearance against
generated water; seed-1 canopy-aware C1 = 0.618 against
`LANDMARK_VISIBILITY_MIN` = 0.6 (headroom 0.018 — retunes go *down* in
density, there is no room up). Render re-probe of the western/southern town
vantages and one riverside bend confirms the fixes on the next tour.

**Rule (learned the hard way) — water-adjacent placements are derived-only.**
Hydrology drift makes any tabled coordinate that must sit on or near water a
trap. Everything keyed to water — fords, birch lines, shore sand, lakeshore
POI *approaches* — derives from the generated trace and the `dist_to_water`
field. Only stamp centers (basin, source, POI pads) may be tabled, and they
must tolerate the trace landing where it lands.

### 7.2 Why this layout satisfies the contracts

- **POI chain (C3, 180–270 m links):** town → shrine ≈ 230 m; shrine →
  watchpoint ≈ 215 m; watchpoint → barrow ≈ 185 m; shrine → forest ruin ≈
  240 m; town → lakeshore cave ≈ 230 m (along shore). Every POI has a
  neighbor in band; total walk town→barrow ≈ 3 links ≈ 3×70 s — the farthest
  destination is a journey, near ones are hops. POI positions are stamps, so
  these distances survive hydrology drift — but links that cross the
  *generated* river count as valid only once a derived ford (§7.1a) sits on
  them; the C3 validation must use generated water, not this table.
- **C1/C2:** the crag (peak +34 m over town ground, ~560 m away, angle
  ≈ 0.06 rad — clears the ≤ 26 m intervening hills) is visible from the
  meadows, lake shore, and both fords; the SE forest and crag shoulder
  occlude the barrow and forest ruin until approached. From the town: crag +
  shrine + (across water) cave bluff = 3 attractors, the rest hidden.
- **Skyline (§1.5):** shrine on knoll and tower on crag break the horizon
  from the main corridors; birch lines flag the water; pine strips lead the
  eye up the foothills.
- **Water gameplay (§3.4):** hamlet on the lake, 3 fords keep the river from
  severing the graph, one dungeon keyed to water.
- **Density check:** 7 POIs + continuous L2 fabric on 1 km² respects the
  testbed contract without approaching region spacing (Q46 kept separate).
- **Readability check (§1.5 math):** crag mass ≈ 180 m wide reads from
  anywhere; tower (12 m) reads within ≈ 360 m (8 px at 640×360) — i.e. from
  the watchpoint, exactly where the final approach starts. At 320×180 the
  tower reads from ≈ 180 m; the crag itself carries the far read — the layout
  survives the user's pending pixel-size decision.

### 7.3 Implementation order for core (highest impact first)

1. **P1 macro v2** — feature stamps (crag ridged noise, knoll, bluff, valley
   `pow` redistribution) + `WORLDGEN_MAX_HEIGHT` = 64 m + the testbed layout
   table. Deliverable: the tour shows a valley with one unmistakable landmark.
2. **P2 hydrology** — river trace/carve, lake basin, shore mask, fords;
   P3 splat inputs (slope/height/dist-to-water) for render's splat shader.
   Deliverable: water reads on screenshots; sand marks fords.
3. **P4+P5 sites & scatter** — building pads + hamlet/shrine/dungeon-entrance
   placeholder prisms (capsule-era stand-ins are fine, silhouettes per §6),
   forest masses with the three species as cone/ball placeholders, corridor
   mask + C1/C3 validation pass. Deliverable: the closed testbed loop (Q45)
   has its stage — town, 3 dungeons, guides between them.
   Micro (P6) comes last and is mostly render-side.

---

## 8. Sources

Consulted 09:08:2026. Engine-internal grounding: NUMBERS.md, DECISIONS.md
(Q12/Q41/Q45/Q46), `engine/world/sources/Worldgen.cpp` (octaves, quantization
contract), devlog sync №2; installed skills `level-design` (pacing, critical
path, readability, blockout checklist) and `procedural-gen` (fBm,
redistribution, biome lookup, blue-noise scatter, determinism checklist).

- Breath of the Wild triangle rule (Fujibayashi/Yonezu/Dohta, GDC/CEDEC 2017):
  [GamingBolt summary](https://gamingbolt.com/the-legend-of-zelda-breath-of-the-wilds-ingenious-world-design-owes-itself-to-triangles),
  [Nintendo Life report](https://www.nintendolife.com/news/2017/10/zelda_breath_of_the_wilds_ingenious_design_is_all_about_triangles_apparently)
- Robert Yang — [Open world level design: spatial composition and flow in
  Breath of the Wild](https://www.blog.radiator.debacle.us/2017/10/open-world-level-design-spatial.html)
  (scale hierarchy, occlusion/reveal, orbiting, curved paths)
- GMTK — [How Nintendo Solved Zelda's Open World](https://gmtk.substack.com/p/how-nintendo-solved-zeldas-open-world)
  (attraction distribution, progressive revelation, motivation-based pull)
- Joel Burgess — [Skyrim's Modular Level Design, GDC 2013 transcript](https://level-design.org/?p=1643)
  and [Motivating Players in Open World Games, GDC 2011](http://blog.joelburgess.com/2011/03/gdc-2011-transcript-motivating-players.html)
  (landmark-driven motivation, modular kits; the "weenie" lineage in Bethesda
  worlds)
- The "weenie" concept — [Theory of Theme Parks: Wayfinding in Themed Design](http://theoryofthemeparks.blogspot.com/2015/08/wayfinding-in-themed-design-weenie.html)
- Amit Patel / Red Blob Games — [Polygonal Map Generation for Games](http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/)
  (rivers downhill via descent, pond-and-spill, elevation redistribution,
  elevation+moisture biome lookup)
- Jaap van Muijden (Guerrilla Games) — [GPU-Based Run-Time Procedural
  Placement in Horizon Zero Dawn, GDC 2017](https://www.guerrilla-games.com/read/gpu-based-procedural-placement-in-horizon-zero-dawn)
  (layered density/exclusion maps, ecotope-driven scatter, water-proximity
  density)
- The Level Design Book — [Landscape](https://book.leveldesignbook.com/process/blockout/massing/landscape)
  (walkable slopes, bowls/ridges vocabulary, water curves, trees as walls,
  rain-shadow reasoning)
