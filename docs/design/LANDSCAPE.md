<!--
Created: 09:08:2026 - 10:45:06
Last updated: 09:08:2026 - 19:44:49
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
- 09:08:2026 - 17:28:51: New §6.2 — dungeon entrance archetypes after a live player read a mis-sited entrance as a bug: relief-selected adit vs sunken barrow; flat-ground answer is a stamped mound + cut forecourt + lintel (generator makes the relief it needs) with four findability layers; marker/facing derived from the carve mouth (derived-only rule extended to carve-adjacent placements); attractor status ruled (assembly counts, hole never does, short-range L1 only).
- 09:08:2026 - 18:55:22: Stage-4, five user decisions ruled. §1.3a — world to 2x2 km: testbed and region contracts coexist SPATIALLY (home valley / transition band / open region), new top landmark tier LR, LANDMARK_VISIBILITY_MIN measured over each landmark's own domain, LR and L0 separated by atmospheric depth rather than angular size. §2.5 — temple mountain: 280 m relief, massif ratio, ridged noise + irregular buttress ridges + cliff bands + asymmetry, checkable anti-dome invariant (lobed slice + 60% rock slope), mandatory validated ascent. §2.6 — border mountains replace invisible walls: varied crest, lobed spurs, impassable by slope AND a traversability flood-fill, never counted as attractors. §2.7 — micro-relief octave everywhere + one plain sited to frame the LR reveal. §5.7 — tall trees worked through: oak 24-32 m, birch 16-22 m, pine 28-38 m (declines literal x4, would overtop Ravenscar), L0_RELIEF 52 -> 110-120 m proposed, sight wedges become tree-free, crown base 35-45%, TREE_SPACING_FOREST 12-18 m (~80% density cut), tri budget 700 + mandatory LOD.
- 09:08:2026 - 19:01:50: Flora's findings ruled. §1.3 — canopy occlusion is now a BAND (crown_base, crown_top) with transparent trunks up to CANOPY_TRUNK_PATH_MAX 250 m; the old solid-column model was pessimistic and was failing sightlines that exist. §1.3 C4 — recorded a boundary on the "raising the peak is a dead end" finding: dead for stamp-scaled flanks, live for fixed-height canopy occluders; one measurement requested. §5.7 — sight-wedge ban narrowed to the tall three in the NEAR half only (flora's bald-lane pushback accepted, their near/far reasoning corrected: near trees steal dominance, far trees steal sight). §5.8 — maturity tiers 15/60/25 with Elder Oak folded in as the giant tier. §5.9 — standing snag (only flora allowed at full height in a wedge) and riparian willow approved, with birch=moving water / willow=still water as the readable split.
- 09:08:2026 - 19:06:29: C1 blocking ruling. §1.3 — binary forest opacity RETIRED in favour of Beer-Lambert attenuation over expected canopy hits (T = exp(-sum n_local*w(h)*d), visible at T >= CANOPY_VISIBILITY_MIN 0.25; permits ~225 m of trunk-level or ~24 m of crown-level forest), which subsumes and retires the ad-hoc CANOPY_TRUNK_PATH_MAX; same transmittance governs C4. Stated as a physics correction allowed ONCE — if seed 1 still fails, the world changes, not the floor. Recorded core's measurements: heights cost -0.048, band recovers only +0.011, crown-base fraction is visibility-insensitive. §1.3 C4 — my "live for canopy" boundary REFUTED by measurement in 52-64 m with the mechanism now stated (the crag's own flanks dominate and it hides itself faster than it gains height); above 64 m untested due to WORLDGEN_MAX_HEIGHT saturation, so L0_RELIEF stays open pending the re-run at the raised ceiling.
- 09:08:2026 - 19:14:17: STOP-THE-LINE correction + stage-4 batch. §1.3 — "raising the peak lowers clearance" WITHDRAWN entirely: core's C1 raycast was counting the crag as an occluder of itself, so every C1 number this stage was contaminated; corrected, clearance RISES with peak (52->0.751 ... 200->0.915) and the taller canopy never broke C1 (0.751 vs 0.60 floor). Recorded why it survived — directionally plausible findings get less scrutiny than surprising ones. Beer-Lambert attenuation kept as the better model but explicitly NOT load-bearing: the one-time physics-correction budget was never spent. §1.3a — LANDMARK_MAX_DISTANCE 4 km bounds the far plane (CAMERA_FAR 1000 -> >=4000), beyond it is backdrop; 10x10 km gets one LR per 4x4 km cell. §2.5 — "7000 steps" ruled as a staged climb (1200-1800 m path, 5-7 landings), literal reading flagged to the user. §2.7 — meso relief band 25-60 m / 1.5-4 m as GENERAL terrain (forest-only would seam the forest edge) + scarps 2-5 m. §5.8 — maturity re-weighted 25/60/12/3 (sapling rare per user; sub-mature retained because young trees do mid-canopy layering, not ground fill). §5.9 — snag density split by material: 1.5-3/ha weathered grey inside forest, 0.25-0.5/ha pale bone in the open, preserving the false-L2-guide rule. §5.10 — BigBush, FallenLog (big/small, across the fall line), trees on scarp edges approved. §6.1 — castle REVISED to a real fortress per user: 80x80 curtain + 4 towers + twin-tower gatehouse + hall + keep, scalable by terraced wards A/B/C, pad 60->120 m; safe because Ravenscar's growth gives ~2x architectural headroom under the same siting mechanism. §6.3 — NEW: true-darkness places, graded AMBIENT_FLOOR, qualification by enclosure + 25 m depth, three anti-surprise layers, torch floor 4 m, C1 exempt with a findable-way-out guarantee.
- 09:08:2026 - 19:15:39: §5.7 sight wedges RE-RULED after the C1 correction (flora asked for the re-decision): near/far half-split replaced by the single crown-vs-flank test already used for the castle; giants ALLOWED in wedges (one per wedge) because an off-axis elder gives the landmark scale — repoussoir is our best depth cue and the exclusion deleted it; C4 sharpened to govern masses and built structures rather than individual near vegetation.
- 09:08:2026 - 19:19:07: Flora's root-flare finding. §5.10 — cliff-edge setback datum corrected: >=1.5 m measured from the OUTER EDGE OF THE ROOT FLARE, not the trunk axis (a 1.6x flare left only ~0.5 m of ground and the tree would still have floated); cliff lean recorded as a separate, larger parameter than the crowding lean. §2.7 — added the standing rule that terrain never flattens under vegetation: the plant absorbs the ground via root flare, because a smoothed disc under every trunk is the pool-table flatness this section exists to remove; future "floating trees" bugs are flora/render fixes, not terrain ones.
- 09:08:2026 - 19:20:45: §6.1.3 — story picked fork (a) (the Corvanes fortified because they feared what they buried), so two terrain asks folded in: ward masonry phasing carried by BLOCK SIZE AND VALUE rather than texture (invisible at 640x360), which costs nothing because the terrace order already puts the oldest ward uphill nearest the barrow; and a binding sightline from the barrow-facing corner tower's top to the barrow entrance, validated and occlusion-protected like the yard/gate sightline.
- 09:08:2026 - 19:23:49: §6.1.3 — three named masonry phases from story (panic / treaty money / fear returning), and resolved an ambiguity I created: my A/B/C were BUILD stages, story's are IN-WORLD construction generations, all present when the player arrives — the doc now means the in-world axis, with implementation minimum A+B. Phase C ruled UNFINISHED as generator rules: 0.4-0.6 partial arc with the completed arc covering the APPROACH and the gap on a flank (a gap on the approach would make the gatehouse decorative and kill the petitioner ritual), raw stepped unfaced ends, 0.6-0.75 height, no parapet, spoil heap and never-laid dressed stone. Nearly free against R3 and the silhouette budget.
- 09:08:2026 - 19:27:13: §6.1.3 — C's gap placed on the BARROW-FACING flank (story's ask; bearings checked: barrow 27 deg, peak 28 deg, approach 225 deg, so grave and road are opposite sides and the approach stays walled). Forced one refinement: since the barrow side is uphill where ward A sits, C is a contour-following perimeter wrapping A and B rather than simply the lowest terrace — also the more authentic form, since uphill outer works matter most on a hillside. Noted that the two story asks reinforce: the barrow-facing tower watches the grave THROUGH the unbuilt stretch, so sightline clearance is guaranteed by absence rather than by a height check. Gap reachable off-corridor only — a back way, never an alternative front door.
- 09:08:2026 - 19:30:26: §2.5 — "7000 steps" CLOSED by user decision: it is a name, not a step count; 1200-1800 m / 5-7 landings / ~8 min stands, fiction keeps the name. §6.1.3 — gap reachability promoted from observation to VALIDATED INVARIANT, since story's act-1 trespass route now depends on it alongside the act-3 muster: continuous traversable route from barrow ground up the NNE spur (validated like the castle ramp and summit ascent), deliberately non-corridor-grade at SCRAMBLE_SLOPE 30-45 deg, passing within 40 m of the barrow entrance so the act-1/act-3 rhyme is geometric rather than lucky, and the completion fraction recorded as having two dependents.
- 09:08:2026 - 19:34:15: Story's near-miss (they nearly attached the Steps to act 1's climax, which is a different mountain) produced three fixes. §2.5 — added a boxed TWO DIFFERENT CLIMBS warning (the Steps are the regional massif, act 2; Ravenscar's climb is the local L0, act 1); specified the Steps as a BUILT stair in four generations of disrepair, with disrepair strictly visual/routing and never impassable; landings are now STATIONS with built markers, and LR_ASCENT_LANDINGS gains narrative dependents (5-7 landings = 5-7 rite beats), so it is no longer a free pacing knob. §7.1 — filled a real gap in my own doc: Ravenscar had no validated summit route despite act 1 climaxing there; now required and validated like the temple ascent and castle ramp, and specified as an informal worn PATH rather than a stair so the two climbs never read as the same place.
- 09:08:2026 - 19:38:02: §2.5 — LR_ASCENT_LANDINGS pinned at 7 (was a 5-7 band); story ruled the count since it now carries narrative dependents, one landing per station of the naming rite, which anchors the stair's name diegetically while the user's never-count-steps rule stays intact. Verified rhythmically before pinning: 7 landings over 1200-1800 m give 57-86 s segments at WALK_SPEED, inside the testbed POI_TRAVEL_TIME band across the whole range, so the climb's cadence matches the valley's exploration cadence; ~40 m of relief per station keeps the view changing.
- 09:08:2026 - 19:44:49: New §7.0a — barrow re-sited after the L0 raise buried it (a cascade from my own proposal, owned). RULING: swing the bearing into a couloir, do NOT move the castle — core's outward-move options were rejected because moving the castle cascades into ford command, approach corridor, trespass route, ward count and the R1 check to buy what a bearing change buys outright. Measured a ~60 deg feasible arc (bearings 180-240 at radius 90-110 keep CASTLE_BARROW_DIST legal with the castle unmoved); core searches it for low terrain nearest the current 209 deg. Fallback is a high shoulder entrance, pre-cleared with story because it inverts "the seat stands over the grave". Durable rule extracted: §7.1 coordinates are stamps against a terrain state, so changing a landmark's relief invalidates every placement on its slopes and re-validation is part of the change, not a follow-up.
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
field is terrain **plus canopy** — terrain-only raycasts pass seeds where a
pine wall buries the L0 (render's stage-3b probes).

**Canopy is a BAND, not a column (stage-4 correction, flora's finding).**
Modelling canopy as solid ground-to-treetop was correct only while crowns
started at 2.7 m. With crown base at 35–45 % of height (§5.7) the canopy is a
**slab** from ≈ 9–13 m to ≈ 24–38 m with open trunk space beneath it, and a
ray from a 1.7 m eye rises with distance: through forest 30 m out the ray sits
at ≈ 6 m and passes cleanly *under* the crowns. The column model blocked that
ray and was **pessimistic** — it was failing sightlines that exist. Rules:

- The occlusion query returns a band `(crown_base, crown_top)` per sample.

**Forest attenuates, it does not switch (stage-4 ruling — the binary model is
retired).** First-hit-opaque models a wall we have deliberately stopped
generating: after the 80 % density cut a ray crossing 150 m of forest at trunk
level expects ≈ 0.92 trunk hits. Vegetation occlusion is therefore
**probabilistic**, and the rule is Beer–Lambert over expected hits:

```
T = exp( − Σ_segments  n_local · w(h) · d_segment )
```

- `n_local` — local stem density (stems/m², from the placement lattice, so the
  maturity mix and the young sub-lattice are accounted for automatically).
- `w(h)` — mean horizontal width the ray meets at its current height: **trunk
  diameter below `crown_base`, crown diameter inside the band, 0 above**.
- The landmark counts as **visible when `T ≥ CANOPY_VISIBILITY_MIN` = 0.25**
  **(предложение — утвердить)**.

Worked from our own numbers (44 stems/ha, 1.4 m trunks, ≈ 13 m crowns):
λ_trunk = 0.0062/m, λ_crown = 0.0572/m, so the 0.25 threshold permits
**≈ 225 m of trunk-level forest or ≈ 24 m of crown-level forest**. That is the
right shape: you can see a landmark down a long colonnade, and you cannot see
it through more than a thin screen of crown. It also **subsumes and retires**
the ad-hoc `CANOPY_TRUNK_PATH_MAX` = 250 m from the previous revision — that
guess and this derivation agree to within 10 %, which is why one mechanism now
replaces both rules.

- **Same transmittance governs C4:** an occluder the ray passes with
  `T ≥ CANOPY_VISIBILITY_MIN` is not counted as an occluder for the clearance
  test either. One notion of "blocks", used consistently.
- **This is a physics correction, not an accommodation.** If a seed fails C1
  after attenuation, the floor does **not** move: we change the world — thin,
  shorten, or relocate the foothill strips. Attenuation is allowed once, as
  the fix for a model that was wrong; it is not the first of a series.
- **That budget was NOT spent.** The C1 emergency this rule was written under
  turned out to be a validation bug (see the withdrawn finding in §1.3):
  corrected, seed 1 with the full §5.7 canopy measures **0.751** against a
  0.60 floor, so attenuation was never load-bearing for the floor. It is
  implemented because it is the better model — the maturity mix falls out of
  `n_local` for free, and one consistent notion of "blocks" across C1 and C4
  is worth having on its own merits. **The contingency was never called; a
  future reader should not think the budget is gone.**

**Measured facts to build against (core, stage-4, post-correction), so nobody
re-derives them:** crown base 35 % vs 40 % gives **identical C1 to three
decimals**, because the ray almost never threads that 9.8–11.2 m window at the
crossing points. **Do not spend tuning effort on the crown-base fraction** —
it is a walkability and feel parameter (§5.7), not a visibility one. Likewise
all three flank parameterisations (fraction-of-peak, fixed metres,
sqrt-scaled) give identical C1 to three decimals; core kept absolute metres
anyway, because coupling an occluder's size to the thing it occludes is a bad
idea even when it is not the bug.

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
  clearance factor. One knob is a genuine dead end: the treeline is useless
  here (foothill terrain sits below any sane treeline).

  > ### ⚠ WITHDRAWN — "raising the peak lowers clearance"
  >
  > **This finding was WRONG and is withdrawn entirely** (09:08:2026, core).
  > It was never a property of the world: the C1 raycast was **counting the
  > crag itself as an occluder of the crag**. The aim point is peak + 8 m
  > while `LANDMARK_CLEARANCE_FACTOR` (1.2) multiplies against terrain that is
  > essentially at peak height, so near-summit ground "out-angles" the summit
  > as soon as `0.2 × (peak − eye) > 8 m` — above a ~60 m peak the test
  > returned 0.000 for *every* standpoint regardless of the world, and below
  > it the measure was already dragged down.
  >
  > **Corrected (landmark excluded from its own occlusion):** clearance
  > **RISES** with peak height, which was the intuitive relationship all
  > along — peak 52 → C1 0.751, 70 → 0.783, 90 → 0.849, 115 → 0.865,
  > 150 → 0.895, 200 → 0.915.
  >
  > **Why it survived, recorded so the next one does not:** it pointed the
  > direction we half-expected, and **nobody asked why a landmark would become
  > less visible for being taller.** A finding that is directionally plausible
  > gets less scrutiny than a surprising one, which is exactly backwards. The
  > review chain is what caught it — flora challenged the boundary and read
  > the code, core re-measured and self-reported. Flora's specific hypothesis
  > (`ridge_amp_frac`) was itself refuted; the challenge was still what
  > produced the fix.
  >
  > **Consequences:** `L0_RELIEF` 110–120 m does not cost C1, it **improves**
  > it (≈ 0.865). The taller canopy never broke C1 either — seed 1 with the
  > §5.7 heights measures **0.751** against a 0.60 floor. Every C1 number
  > reported before this correction is contaminated; do not cite them.

  The effective lever on landmark visibility remains forest *shape*: a closed
  canopy annulus around an L0 can never pass canopy-C1 from valley ground —
  landmark-skirting forest must be broken into radial/ridge strips with gaps
  (see §7.1). That finding was measured independently and stands.

### 1.3a World scale, zones, and the fourth landmark tier (stage-4 ruling)

The world grows to 2×2 km now, 10×10 km once LOD exists. The valley becomes a
corner of it.

**Zones, not epochs — the two density contracts coexist SPATIALLY.** Q46 says
never cross-apply testbed and region numbers; it does not say the world may
only have one of them. Ruling: the testbed contract is a *contract*, not a
size. The original 1×1 km valley keeps it; everything beyond runs the region
contract. Neither is corrected toward the other.

| Zone | Extent | POI spacing (from `POI_TRAVEL_TIME`) |
|---|---|---|
| **Home valley** | the original 1×1 km corner | 180–270 m (testbed) |
| **Open region** | the rest of the 2×2 km | 540–900 m (region) |
| **Transition band** | `ZONE_TRANSITION_WIDTH` = 300 m ring around the valley **(предложение — утвердить)** | interpolate between the two |

The transition is a designed feeling, not a seam: leaving home should *feel*
like the world opening up. A hard density cliff would read as a bug; a
gradient reads as journey.

**Landmark tiers gain a top level.** The map now needs a landmark above the
valley's:

| Tier | Name | Count | Domain |
|---|---|---|---|
| **LR** | **Regional landmark** — the temple mountain (§2.5) | exactly 1 per world | the whole map |
| L0 | Valley-dominant — Ravenscar | 1 per valley | its valley |
| L1 / L2 | as before (§1.3) | | |

**Scale-aware visibility — the fix that keeps C1 meaningful at 2 km.**
`LANDMARK_VISIBILITY_MIN` (0.6) is measured over the landmark's **own
domain**, never the whole map: the valley L0 over the valley, the LR over the
world. This was implicit while domain == map; at 2 km it must be explicit, or
Ravenscar fails a test it was never meant to take.

**C1/C2/C3 at the new scale:** C1 holds unchanged as a floor (the LR satisfies
it across most of the region by design — that is intended, not a loophole).
C2-testbed is already scale-free (it compares angular ratios, not counts of
metres) — keep as written. C3 uses the per-zone spacing above. Border
mountains (§2.6) **never count as attractors** in any of these tests.

**The two big landmarks coexist by DEPTH, not by size.** This is the
crag-vs-castle problem again, but the map-scale answer is different: do *not*
require the near landmark to out-subtend the far one — at 2 km their
subtended heights are comparable and forcing a margin would mean deforming
one of them. Instead they separate by **atmospheric depth**: the LR always
renders beyond the haze onset from valley standpoints, the valley L0 always
inside it. Hazy-and-huge versus solid-and-detailed reads instantly as "far
goal" versus "here"; it is how Skyrim keeps the Throat of the World from
eating every local landmark. Contract for render: `LANDMARK_HAZE_ONSET` =
800 m **(предложение — утвердить)**; the LR is never sited closer than that
to the valley's main corridors, and the valley L0 never further.

**The LR is never fully occluded** from the valley's main corridors — it is
the far goal, and a goal you cannot see is not a goal. Same wedge machinery
as §1.3, applied at map scale.

**Maximum landmark siting distance — the rule that bounds the far plane
(render's blocker, ruled).** `LANDMARK_MAX_DISTANCE` = **4 km**
**(предложение — утвердить)**. No navigational landmark is ever sited further
from reachable ground than this, at any world size. Consequences, decided once
so render and core need not revisit:

- The 2×2 km world's temple (1.4–1.6 km) sits well inside it; `CAMERA_FAR`
  must rise from 1000 m to **≥ 4000 m** or the LR is not hazy, it is *clipped*.
- At 10×10 km, "one LR visible from everywhere" does **not** scale — 14 km of
  diagonal is scenery, not navigation. The world becomes multi-region:
  **one LR per ≈ 4×4 km region cell**, each dominating its own domain
  (§1.3a's own-domain visibility rule already carries this).
- **Beyond 4 km is BACKDROP, not geometry.** Distant ranges may be drawn by
  whatever cheap mechanism render prefers (impostor layer, sky-dome
  silhouette) and need not be depth-correct. One design constraint: a backdrop
  must be *consistent* with the real terrain behind it — the ridge you see at
  6 km must be the ridge you walk into at 3 km. A backdrop that lies is worse
  than no backdrop.

This bounds depth precision at ~4 km rather than 8+, which is the difference
between a far-plane change and a depth-buffer restructure.

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

### 2.5 The regional landmark massif — the temple mountain (LR)

The far goal: high, cliffy, uneven, with a walkable ascent and a temple on
top. Ravenscar keeps the valley and the story; this keeps the horizon.

**Scale (предложение — утвердить).** `LR_RELIEF` = 280 m above the
surrounding plain (proposed band 250–350). `LR_BASE_RADIUS` = 600–700 m, i.e.
mean flank slope ≈ 25° — that ratio is what makes it read as a **massif**
rather than a spike; a cone steep enough to be dramatic at the summit must be
broad enough at the foot to look like it belongs to the ground. Sited ≥
`LANDMARK_HAZE_ONSET` (800 m) from the valley, in practice the far corner
(≈ 1.4–1.6 km out). Readability (§1.5): at 1500 m anything ≥ 50 m reads, so
the massif is unmistakable while its temple (15–20 m) only resolves inside
≈ 600 m — the temple is the reward for approaching, exactly like the castle.
This requires `WORLDGEN_MAX_HEIGHT` 64 → **400 m**.

**"Cliffy and uneven" as generator rules — the user explicitly rejects smooth
domes, so these are invariants, not suggestions:**

1. **Ridged noise, not fBm**, for the massif field: `r = 1 − |2n − 1|`
   summed over octaves. fBm makes domes; ridged noise makes spines.
2. **Radial buttress ridges:** `LR_RIDGE_COUNT` = 4–7 ridges descending from
   the summit with couloirs between, as an angular modulation
   `h *= 1 + A·cos(k·θ + φ(θ))` with `φ` from noise so the ridges are
   **irregular, never symmetric** (a symmetric star reads as artificial).
3. **Cliff bands:** above `LR_CLIFFLINE` (⅓ height), quantize elevation into
   bands of `LR_CLIFF_BAND` = 8–15 m spaced 30–60 m vertically, blended just
   enough to avoid stair-stepping artefacts. This is the "cliffy" read and it
   feeds §4's splat directly (rock above 40°).
4. **Asymmetry:** one flank biased steep (a scarp face), the opposite gentler
   — the gentle side carries the ascent. Real mountains are not radially
   uniform and neither is this one.
5. **THE ANTI-DOME INVARIANT (checkable, not editorial):** a horizontal slice
   at ⅔ height must be **lobed, not circular** — perimeter² / (4π·area) ≥
   `LR_LOBE_RATIO` = 1.35 **(предложение — утвердить)** — and ≥ 60 % of the
   surface above mid-height must exceed `SLOPE_ROCK_MIN` (40°). A seed that
   produces a smooth dome **fails**. This is the user's "not a boob" stated
   as a test the generator can run on itself.

**The ascent is mandatory and validated.** A continuous walkable route from
the foot to the summit must exist: average slope ≤ 25°, nowhere exceeding
`PLAYER_MAX_SLOPE`, no step > `PLAYER_STEP_HEIGHT`. Same class of invariant
as the castle ramp (§6.1.2) — a summit temple you cannot reach on foot is a
failed placement, not a later problem. Derived from the generated massif,
never tabled.

**"7000 steps" — a staged climb, not a switchback (user requirement).** The
ascent is a *sequence*, not a ramp: `LR_ASCENT_LENGTH` = 1200–1800 m of path
(4–6× the direct horizontal distance, so the route wraps the massif rather
than attacking it) with `LR_ASCENT_LANDINGS` = **7** staged rests — a shrine,
a vista, a wind-scoured shoulder — each a place to stop and look back at how
far the valley has fallen away **(предложение — утвердить; pinned from the
former 5–7 band by story, one landing per station of the naming rite)**.

**Seven verifies rhythmically, which is why it is pinned rather than merely
accepted.** Over the 1200–1800 m path, seven landings give segments of
171–257 m, i.e. **57–86 s of walking at `WALK_SPEED`** — inside the testbed's
`POI_TRAVEL_TIME` band (60–90 s) across the whole range. The climb's internal
rhythm therefore matches the valley's exploration rhythm: the player already
knows, in their legs, how long "one stretch to the next thing" takes, and the
ascent speaks the same cadence. Each station also gains ≈ 40 m of relief, so
the view genuinely changes between them rather than repeating. Landings are what
make a climb read as long; raw distance just makes it tiring. At 1500 m that
is ≈ 8 min of walking one way, which is a journey.

**The Steps are BUILT and UNREPAIRED (story canon, and it costs nothing).**
The ascent is a **stair**, not a bare path: cut treads and revetted edges
following the route, in four generations of disrepair — worn and dished
treads, sections slumped or collapsed with the path detouring around them,
vegetation encroaching at the margins, revetment shed downslope as rubble.
Hard constraint: **disrepair is visual and routing, never impassable.** No gap
exceeds `PLAYER_STEP_HEIGHT`, every collapsed section has a walkable detour
within the ascent's slope band, and the summit stays reachable — the crown
kept the order poor, but nobody ever forbade the climb.

**Each landing is a STATION** (story: a pilgrim speaks a name at each). So
each carries a small built marker — a station stone, a niche, a lintel — sized
as an L2 guide, and the landing is a *place*, not merely a flat spot on a
path. Consequence to respect: `LR_ASCENT_LANDINGS` now has **narrative
dependents** — seven landings is seven recitation beats, and story's folk
etymology hangs the stair's name on the seven stations rather than on any step
count (which is what lets the name have a source in the world while the user's
"never count steps" rule stays intact). Changing the count changes a rite; it
is no longer a free pacing knob, and moves through story the way the castle's
completion fraction does.
> **⚠ TWO DIFFERENT CLIMBS — do not conflate them.** This world has two
> ascended landmarks and story nearly attached the wrong beat to the wrong
> mountain. **The Steps are HERE, on the regional temple massif (§2.5)** — a
> distant act-2 destination. **Ravenscar's climb is a different, local
> ascent** to the ward-tower ruin on the valley L0 (§7.1), which is act 1's
> climax. Same verb, different mountains, ~1.4 km apart. Whenever a beat says
> "the climb", check which landmark it means.

**DECIDED — user, 09:08:2026: "7000 steps" is a NAME, not a step count.**
(«НЕ буквально, 8 минут — кайф, название оставляем».) The numbers above stand
as written — 1200–1800 m, 5–7 landings, ≈ 8 min one way — and the climb keeps
its name in the fiction, which is story's to use in canon. Closed; do not
reopen on the arithmetic.

### 2.6 Border mountains — the world edge

Replaces the invisible walls. The world ends in geography, Skyrim-style.

- **Band:** `BORDER_BAND_WIDTH` = 200–300 m of mountain, preceded by
  `BORDER_FOOTHILL_WIDTH` = 100–200 m of rising ridgelets so the player
  climbs *into* it rather than meeting it **(предложение — утвердить)**.
- **Height:** crest at `BORDER_CREST_HEIGHT` = 150–250 m above local terrain,
  **varied ±30 % along its length** at a long wavelength (600–1200 m).
- **Not a wall — three shape rules:** (1) crest height varies as above, so it
  reads as a *range*; (2) spurs push inward irregularly by 100–250 m so the
  boundary is lobed, never straight — a straight edge is the tell that gives
  away a box; (3) the inner face uses the same ridged/cliff-band rules as
  §2.5, never a uniform slope.
- **Impassability: slope first, validation second.** The inner face averages
  ≥ 55° over ≥ 40 m of climb, which exceeds `PLAYER_MAX_SLOPE` (~50°) — but
  noise *will* occasionally produce a walkable saddle, so slope alone is not
  trusted. A traversability flood-fill from inside the playable area must
  fail to reach the outer edge; where it succeeds the generator raises the
  offending saddle and re-runs. Geometry plus a test, not a promise. Keep a
  hard clamp far outside the band as engineering safety — but it is a
  backstop nobody should ever touch, not a design element.
- **Border mountains are NOT attractors** (§1.3a): they are the frame. They
  never satisfy C1's "something to see" test — a wall of rock is not content,
  and letting it count would license genuinely empty ground.

### 2.7 Ground micro-relief and the plain

**Everything is slightly uneven.** The complaint is that the land reads flat;
§2.1's anisotropy gave us hill-scale ridgelets, and this is the layer below
it. Add a fourth octave, `GROUND_MICRO_WAVELENGTH` = 8–16 m at
`GROUND_MICRO_AMPLITUDE` = 0.3–0.6 m, plus an optional fifth at 2–4 m /
0.1–0.2 m for surface tooth **(предложение — утвердить)**. At 0.5 m over a
12 m wavelength the local slope is ≈ 5°, so this is free: it never threatens
`PLAYER_STEP_HEIGHT`, corridors, or building pads, and it kills the
billiard-table read at eye level. Micro-relief is **suppressed inside
building pads and the castle terrace** (they are cut flat on purpose) and
**retained everywhere else, including the plain**.

**Meso relief — the missing middle band (stage-4).** Between the hill octave
(128 m / 6 m, §2.1) and the micro octave above there was a gap, and it is
exactly the scale at which walking through a forest felt like a flat traverse.
Add `GROUND_MESO_WAVELENGTH` = 25–60 m at `GROUND_MESO_AMPLITUDE` = 1.5–4 m
**(предложение — утвердить)** — dips, rises and hollows you walk into and out
of. Max local slope ≈ 18°, so corridors and pads are unaffected.

**Terrain does NOT flatten under vegetation — vegetation absorbs the terrain.**
Stated because the opposite fix is the tempting one and it would undo this
whole section: micro and meso relief deliberately make the ground under a tree
uneven (a 1.2 m trunk on `TREE_SLOPE_MAX` spans ≈ 0.84 m of drop across its
own base, before roughness), and the answer is **geometry on the plant** — a
root flare that buries its own skirt (§5.10) — never a flattened disc of lawn
beneath every trunk. A forest floor smoothed under each stem is a pool table
with trees on it, which is precisely the flatness complaint that produced this
section. Any future "trees are floating" bug is a flora/render fix, not a
terrain one.

**Ruling: this is GENERAL terrain, not a forest-specific stamp.** Forests
merely sit on it. Three reasons: a forest-only stamp makes the forest edge a
seam where terrain character visibly changes — the classic tell of generated
ground; meadows want the same relief (it is the same "too flat" complaint);
and the *perception* that forests have more relief comes free, because trunks
and canopy give the eye something to measure height against, while open meadow
reads flatter at identical amplitude.

**Scarps (обрывы).** Small cliff steps `SCARP_HEIGHT` = 2–5 m, placed where
the meso field's local slope already exceeds a threshold, by a low-probability
terracing transform — `SCARP_DENSITY` = 0.5–1.5 per hectare inside forest
masses, rarer in open ground **(предложение — утвердить)**. Constraints: never
inside a corridor mask (§2.4); never enclosing a walkable region (a scarp is
an obstacle to go around, never a trap — the traversability check of §2.6
applies locally); and always with a walkable way around within 40 m, so a
scarp costs the player a decision, not a reload.

**One small plain, and it earns its flatness.** A flat area is only valuable
as contrast, so it is placed where flatness *does something*:
`PLAIN_EXTENT` = 400–600 m across, on the route from the valley toward the
LR, positioned so that **the massif is fully revealed and unobstructed from
it**. The enclosed valley opens onto the plain, and the mountain is suddenly
the whole horizon — that is the reveal beat (§1.4), and it is the reason this
is a composition and not a bald patch. Rules inside it: flat to ±1.5 m
overall, micro-relief retained (flat, not sterile), no forest mass, only
sparse L2 (standing stones, a lone skyline tree) so the openness reads as
intentional. The plain is also the natural site for a future FUTURE road and
for the act-scale muster/travel beats story may want.

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
- **Size (stage-4 revision, §5.7):** **24–32 m** tall, crown 10–16 m wide.
  Crown base at 35–45 % of height (≈ 9–13 m of clear trunk) — the height is
  only half the effect, the *space underneath* is the other half.
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
- **Size (stage-4 revision, §5.7):** **28–38 m** tall, base 6–9 m wide —
  ×2.3, deliberately **not** ×4 (see §5.7 for why 4× breaks the valley).
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
- **Size (stage-4 revision, §5.7):** **16–22 m** tall, crown 5–7 m — stays
  the smallest and slimmest of the three, keeping its accent role.
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

### 5.7 Tall-tree revision — working the collisions through (stage-4)

User: trees ×4 taller, forests less dense, "как в Скайриме". Taken seriously,
that request collides with four existing rules at once. Working it through
rather than approving it:

**What the request is actually about.** Our 8 m oak has its crown starting at
≈ 2.7 m: the player pushes *through* foliage, which reads as scrub. What
makes Skyrim's forests feel tall is walking **under a canopy** — clear trunk
space overhead and light between stems. So the fix is three-part and the
height is only the first part: raise the trees, **raise the crown base**, and
**drop the density**. Height alone would have given us taller scrub.

**Ruling on the numbers.** Oak 8–12 → **24–32 m** (the requested ×4 at the
low end). Birch 6–10 → **16–22 m**. Pine 12–18 → **28–38 m**, which is ×2.3
and **declines the literal ×4**, because:

- A ×4 pine is 48–72 m. Standing on a 25 m foothill that is a 73–97 m crown
  top against Ravenscar's 52 m summit: **the forest would be taller than the
  landmark it frames.** Canopy-aware C1 (§1.3) would fail everywhere and no
  strip geometry could save it.
- Skyrim's own tall conifers are ≈ 25–30 m. The literal ×4 overshoots the
  reference the request cites.

**Collision 1 — the landmark must out-top its own forest.** A 35 m tree on a
25 m foothill tops out at 60 m against a 52 m summit. Two consequences, both
binding:

- **Ravenscar must grow with its forest:** `L0_RELIEF` 52 → **110–120 m**
  — **APPROVED by the user**. The argument is composition, not validation:
  a valley heart that its own trees overtop is not a landmark. (The C1 case
  once made for this raise rested on contaminated numbers; the corrected
  measurement shows the raise *improves* C1 to ≈ 0.865 rather than costing
  anything — see the withdrawn finding in §1.3. Right answer, and now for a
  reason that survives.) All castle rules are ratios to the peak
  (`CASTLE_SKYLINE_MARGIN` etc.) and re-derive automatically — story's ~55 m
  castle/barrow geometry is unaffected.
- **L0/LR sight wedges — revised (flora's pushback accepted, with its
  reasoning corrected).** My first draft banned *all* trees in a wedge. Flora
  is right that at 12–18 m spacing this carves mown lanes radiating from every
  POI — authored-looking, the exact effect standing stones exist to create
  deliberately. The rule instead:
  - **RE-RULED after the C1 correction (flora asked for the re-decision, and
    was right to).** Both wedge constraints were justified partly by a
    headroom crisis that turned out to be a validation bug (§1.3). Rather than
    inherit rules whose premise evaporated, the wedge now uses **one test,
    the same one already governing the castle**: *no tree may occlude the
    L0/LR's **crown** (top third) from any corridor standpoint; occluding its
    **flank** is permitted.* One notion of acceptable occlusion across
    architecture and vegetation, and it needs no near/far half-split.
  - **Giants are ALLOWED in sight wedges** — reversing my exclusion. Flora's
    compositional argument is the stronger one: a single elder standing in the
    middle distance, off the axis, **gives the landmark scale**, which is the
    thing a distant landmark most needs and most rarely gets. That is
    repoussoir, and removing it deletes our best depth cue exactly where depth
    matters most. Conditions: subject to the crown rule above, and **at most
    one giant per wedge** — one elder frames, three elders screen.
  - **Sharpening of C4 that this exposed:** C4 governs **masses and built
    structures**, not individual near vegetation. A 48 m crown 100 m away
    subtends more than the mountain behind it and that is *fine* — nobody
    mistakes a nearby tree for a distant massif; the eye reads the depth cue
    correctly. C4's real target was always the foothill pine *wall*, a mass.
    Apply it to masses.
  - Bushes, saplings and anything under ≈ 8 m were never restricted and still
    are not — the wedge keeps its ground texture and reads as young growth
    under an old canopy, never as a mown lane.
  - **Fallback if per-tree crown testing proves expensive:** the previous
    conservative rule (tall three banned in the near half) is an acceptable
    approximation — but it is the fallback, not the intent.

### 5.8 Maturity tiers — restoring fullness without restoring canopy

Approved (flora's proposal): every instance carries a maturity scalar rather
than standing at species nominal size, because 44 stems/ha all identical reads
as a **plantation**, and a plantation is a different failure from an empty
field but a failure all the same.

**Revised (user: «гигантским и могучим… мелкие деревья очень редкие»):**

| Tier | Share | Size | Placement |
|---|---|---|---|
| Giant / **Elder** | **25 %** | ×1.5 nominal | the "могучий" read; also L2 guides (§1.3) — a forest with internal hierarchy is legible from inside |
| Mature | **60 %** | nominal | the main lattice |
| Sub-mature | **12 %** | ×0.7–0.85 | mid-canopy layering on a half-spacing sub-lattice |
| Sapling | **3 %** | ×0.4–0.6 | deliberately rare — a nursery is what the user rejected |

Shares предложение — утвердить. **Why not flora's 25/67/8:** their reasoning
was right that the young tier's *ground-level* fill job is better done by the
new bushes, logs and snags. But young trees at ×0.5–0.7 of a 28 m tree are
14–20 m — their crowns sit **above** eye level and **below** the main canopy,
so they were never doing ground fill; they were doing **mid-canopy layering**,
which is what makes a wood feel deep rather than like a colonnade with debris
on the floor. Deleting them wholesale would flatten the vertical section. So
the tier splits: the genuinely small (sapling, ×0.4–0.6) drops to 3 % —
satisfying «очень редкие» — while a sub-mature band survives at 12 % to keep
the middle of the section populated. Ground fill moves to §5.9's classes,
exactly as flora argued.

**The "Elder Oak" proposed as a separate species IS the giant tier** — one
system, not two: a maturity scalar on the existing parameter set, not a
catalog row. Giants are excluded from sight wedges entirely (§5.7).

### 5.9 Additional species (approved from flora's proposals)

**Standing snag (dead trunk).** Approved, and the sharpest idea in the batch:
a barkless broken column with no crown is **the only flora that may legally
stand at full height inside a sight wedge** — nothing to out-angle with.
30–60 tris, the cheapest asset in the project.

**Density revision (user endorsed dead trees; flora flagged the tension
honestly rather than letting it be overwritten).** My original rarity existed
because pale dead wood is the highest flora value in the scene and a common
snag becomes a false L2 guide. That reasoning does not stop being true because
more were requested — so the fix is to **split the material, which turns the
tension into a feature**:

| Where | Density | Value | Reads as |
|---|---|---|---|
| Inside forest masses | `SNAG_DENSITY_FOREST` = 1.5–3 / ha | weathered grey-brown | texture and atmosphere; one every ~30–80 m |
| Open ground | `SNAG_DENSITY_OPEN` = 0.25–0.5 / ha (unchanged) | pale bone | a deliberate vertical accent — a legitimate L2 guide |

**(предложение — утвердить.)** A pale snag standing alone in a meadow is a
landmark; a grey snag in a wood is weather. Same asset, two jobs, and the
composition rule survives contact with the user's request instead of being
quietly traded away. Prefer old stands, edges, clearing rims, scarp tops, and
ground near barrows and ruins where the atmosphere pays double.

**Vale willow / alder (riparian).** Approved, and it fixes a real flaw I had
left: *every* water body in the world is currently flagged by pale birch, so
all water reads the same. Two riparian species let water say two things —
ruling on the split: **birch marks moving, clear water** (river banks, fords),
**willow marks still or slow water** (lake shores, pond rims, slack bends).
Dark low mass, the value opposite of birch, 10–14 m, within
`RIPARIAN_WATER_DIST` (reuse the birch 20 m band). A player who learns that
dark drooping mass means still water has learned to read the landscape, which
is the whole point of a palette this small.

**Elder oak** — folded into §5.8's giant tier, not a separate species.

### 5.10 Forest floor classes (user-specified, stage-4)

The floor is what makes a wood feel *walked* rather than crossed, and at the
new density it is also what replaces the fill the young tier used to carry
(§5.8). All approved as distinct catalog classes.

**BigBush** — a class, **not a scaled Bush**, and flora is right about why: a
1.2 m bush is ground texture, a 3.5 m bush is an **obstacle that breaks a
sightline and makes the player pick a side**. That is forest-floor navigation.
2.5–4 m, denser silhouette, 120–180 tris. `BIGBUSH_DENSITY` = 8–15 / ha inside
masses, plus clearing rims, scarp bases and stream banks. **Never inside a
corridor mask** (they exist to be gone around, and the critical path is not
the place for that); may partially occlude but at 4 m they cannot threaten a
POI sightline at any meaningful range.

**FallenLog**, two classes as specified:

| Class | Size | Tris | Density | Rules |
|---|---|---|---|---|
| Big | 8–14 m long, ⌀ 0.8–1.4 m, half-sunk | 80–120 | `LOG_DENSITY_BIG` = 3–8 / ha | **excluded from corridors** until vaulting exists (⌀ exceeds `PLAYER_STEP_HEIGHT`, so it would be a wall on the critical path); collision on (sim) |
| Small deadfall | 2–4 m, ⌀ 0.35–0.5 m, half-sunk | ~40 | `LOG_DENSITY_SMALL` = 15–30 / ha | allowed anywhere including corridors — half-sunk it sits under step height |

**(предложение — утвердить.)** **Logs lie ACROSS the fall line, never along
it** — flora's geometric ask, approved with its reasoning made binding: a log
pointing downhill reads as a stick, a log lying across a slope reads as a
fallen tree and as something to climb over. Big logs are nearly free
geometrically (the trunk mesh, rotated and sunk) and are the cheapest "this
forest is old" signal in the medium. **FUTURE:** when sim adds vaulting, big
logs become legal in corridors and turn into pacing furniture.

**Trees on scarp edges — flora's question, ruled: YES, deliberately.** A tree
leaning out over a drop is a superb silhouette and a genuine L2 guide, and it
is exactly the kind of detail that still reads at 640×360. Constraints:
mature or giant tiers only (a sapling on a cliff reads as an accident, not a
statement); lean 10–20° outward, away from the mass; scarps ≥ 3 m only, since
below that there is no drama; **never inside a sight wedge** (a tall occluder
on a high edge is the worst case we have). Rare by design — one per scarp
segment, never a row: a row reads as planted.

**Setback corrected (flora's root-flare finding).** The ≥ 1.5 m root-plate
setback is measured **from the outer edge of the root flare, not the trunk
axis** — with a 1.6× flare on a 1.2 m trunk the flare radius is ≈ 0.96 m, so
the original wording left barely 0.5 m of ground beyond it and the tree would
still have floated the first time the scarp was voxelised. The rule was right;
the datum was wrong.

**The lean here is a SEPARATE, larger parameter than the crowding lean** —
flora's note, adopted: crowding lean caps near 0.12 rad, and reusing it on a
cliff edge produces a limp tree instead of a statement.

**Collision 2 — under-canopy walkability.** `CANOPY_CLEARANCE_MIN` = 2.2 m is
a floor, satisfied ~4× over by a crown base at 35–45 % of height. Stated
explicitly so no future species regresses: **every tree species must carry
≥ 2.2 m of clear trunk**, and the tall species must target the 35–45 % band
rather than merely clearing the floor.

**Collision 3 — density, numerically.** Crown width scales ~×1.6, not ×4 —
tall *and* slender is what makes a forest feel tall. Spacing must open up
accordingly: `TREE_SPACING_FOREST` 5–8 m → **12–18 m**
**(предложение — утвердить)**. In trees per hectare that is ≈ 240 → ≈ 44, an
**≈ 80 % density cut** — which is the user's "less dense" expressed as a
number core can implement. `FOREST_COVERAGE` (0.25–0.40) is **unchanged**:
coverage is how much land is forest, density is how many trees stand in it,
and only the second was wrong.

**Collision 4 — budgets.** A 4× taller tree does not need 4× the triangles;
it needs a silhouette that survives being large on screen. `TREE_TRI_BUDGET`
500 → **700 max** for the tall species, but the real answer is LOD, not base
budget: each species ships LOD1 at ≈ 40 % tris and a billboard beyond
`TREE_BILLBOARD_DIST` **(предложение — утвердить, value is render's call)**.
Shadow-map coverage is render's zone; design's constraint is only that a
35 m canopy must not black out the ground it shades — dappled, not sealed,
which the 80 % density cut largely delivers on its own.

**Net effect on composition:** fewer, taller, slimmer trees with light
between them. Forests stop being walls (§2.2's "trees are walls you can walk
through" was written for the dense version) and become colonnades — you see
*through* a forest now, which makes occlude-and-reveal work at a distance it
never could before.

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

**REVISED — it is a real fortress (user decision, stage-4).** The gentry
hall-castle was too small: the user wants "крепость вокруг с башнями и
каменными стенами с воротами входными и замком внутри крепости", explicitly
scalable. So the hall no longer *is* the castle — it stands **inside** a
walled and towered enclosure.

**Why this is now safe, when I originally sized down to protect the
hierarchy:** Ravenscar grows to 110–120 m (§5.7). Every constraint in §6.1.1
is a **ratio or a margin to the peak**, so a 2.2× taller landmark grants
roughly 2× the architectural headroom for free. With a spur pad near 45 m and
`CASTLE_SKYLINE_MARGIN` 12 m, the ceiling is ≈ 98–108 m absolute — some 50 m
of building height available where the old design used 12. The fortress fits
*inside the same siting mechanism*; the pad does not need to move. This is the
first time the landmark has had real headroom over its own architecture.

| Element | Footprint | Height | Tris | Reads as |
|---|---|---|---|---|
| Curtain wall | **80×80 m** enclosure | `CASTLE_WALL_HEIGHT` **8–10 m** | 600–900 | the long horizontal stone band — the fortress's base read |
| Corner towers ×4 | 8×8 m | `CASTLE_TOWER_HEIGHT` **12–15 m** | 200–300 ea. | the rhythm along the wall; four verticals say "fortified" at a glance |
| Gatehouse (twin towers) | 14×8 m | `CASTLE_GATE_HEIGHT` **14–16 m** | 400–600 | the entry mass — the one asymmetry in the wall line |
| Hall | 10×22 m, inside the ward | `CASTLE_HALL_HEIGHT` 8–10 m | 400–700 | the long roof seen *over* the wall |
| Solar / keep | 10×10 m | `CASTLE_SOLAR_HEIGHT` **16–20 m** | 350–550 | the tallest element — the Ward's head, still below R3 |
| Yard / tithe-yard | ≈ 35×35 m open | — | — | the dark interior read through the gate arch |

All heights предложение — утвердить and all remain subordinate to R3 (pad
elevation + tallest element is the binding constraint, not the table).

**Scalable by TERRACED WARDS, not by a bigger box** — this is the ruling that
makes "расширяемая" real. The pad grows 60 → `CASTLE_PAD_SIZE` **120 m**, and
because a 120 m terrace cannot be cut flat into a spur within
`CASTLE_PAD_CUT_MAX`, the enclosure **steps down the slope in levels**, each
level flat within `BUILDING_PAD_SLOPE_MAX` and the wall running along the
terrace edges. This is what real hillside fortresses do, it solves the cut
budget, and it makes growth additive:

**Two different A/B/C axes — do not conflate them (ambiguity I created,
resolved).** My first table read as *build* stages (what we implement now
versus later). Story's phases are *in-world construction generations* — all
three already exist when the player arrives. They are different axes and the
doc means the **in-world** one from here on:

| Phase | In-world | Contains | Terrace | Masonry |
|---|---|---|---|---|
| **A — the panic** | first Corvane lord, on the crown's grant; built fastest, closest to the grave | curtain + 4 towers, the redoubt | upper ward (uphill, nearest crag and barrow) | largest irregular blocks, darkest weathered value |
| **B — the treaty money** | ~2 generations later; the family made respectable, a seat rather than a redoubt | hall, keep-solar, gatehouse, tithe-yard, granary | lower bailey, one terrace down | smaller regular coursing, lighter and cleaner |
| **C — the fear returning** | the dowager's time; begun, never finished | outer works, partial | contour-following perimeter wrapping A and B, stepping with the slope | B's stone, stopped mid-sentence |

**Implementation minimum is A + B** (the act-1 interior set lives in B), and C
comes along free because C is mostly *absence*.

Each stage is a ring, not a rebuild — and every stage re-runs the §6.1.1
checks, because a lower bailey extends the silhouette downhill toward the
valley where it is most visible.

**Phase C is UNFINISHED — as generator rules, not an adjective.** Story's
choice, and the cheapest of the three to build because most of it is what is
*not* there. Rules:

- **Partial arc:** the ring is built through `CASTLE_WARD_C_COMPLETION` =
  0.4–0.6 of its perimeter **(предложение — утвердить)**.
- **The completed arc covers the APPROACH; the gap faces away.** This is both
  how anyone builds (wall the threatened side first) and a necessary design
  guard: if the gap fell on the approach it would become the de-facto way in,
  the gatehouse would be decorative, and the petitioner ritual §6.1.2 exists
  to protect would quietly die. The gate stays the way in; the gap is a flank.
- **WHICH flank: the barrow-facing one** (story's ask, granted — geometry
  checked and it works). From the pad the barrow bears 27°, the crag peak 28°,
  and the approach 225°: the grave and the road are on **opposite** sides, so
  the completed arc covers approach and valley (S/W/SW) and the unbuilt stretch
  faces NNE, uphill toward the barrow. Story's reason is the same lie-in-stone
  logic that chose the fortified reading: the dowager walled the side a
  frightened family can explain — the road, brigands — and stopped before
  closing the side that faces the grave, because finishing *that* stretch
  would have admitted what the whole wall was for.
- **Consequence for C's terrace, and the one refinement this forces:** the
  barrow side is *uphill*, where ward A sits, so C is **not** simply "the
  lowest terrace". C is a **contour-following perimeter that wraps the
  complex**, stepping where the slope demands — one step outside A and B on
  every side, including above them on the crag side. That is also the more
  authentic form: on a hillside the uphill outer works are the ones that
  matter most, since an attacker holds the high ground.
- **The two story asks reinforce each other rather than compete.** The
  barrow-facing corner tower must see the barrow entrance (§6.1.3); the
  barrow-facing stretch of C is exactly the part that was never built. So the
  tower watches the grave **through the gap that shame left open**, and no
  wall-versus-sightline conflict can arise — the clearance is guaranteed by
  the absence, not by a height check.
- **Reachability — now a VALIDATED INVARIANT, not an observation.** Story's
  act-1 MQ4 uses this gap as the trespass route into the muniment room, so it
  is load-bearing in two acts and can no longer rest on "the spur is probably
  climbable". Rules:
  - **A continuous traversable route must exist** from the barrow ground up
    the NNE spur to the gap — validated like the castle ramp (§6.1.2) and the
    summit ascent (§2.5). A seed that walls it off with a 55° scarp breaks
    act 1, silently.
  - **It must NOT be corridor-grade, deliberately.** Average slope in the band
    `SCRAMBLE_SLOPE` = 30–45° **(предложение — утвердить)** — above the 25°
    corridor maximum, below `PLAYER_MAX_SLOPE`. No corridor mask, no width
    guarantee, scarps and outcrops permitted along it. A scramble, not a
    stroll: the difficulty is what makes it read as a back way rather than a
    second front door.
  - **The route passes within 40 m of the barrow entrance**, so story's rhyme
    (the trespasser takes the path the dead will take) is guaranteed by
    geometry rather than by luck.
  - **The completion fraction now has TWO dependents** — the act-1 trespass
    route and the act-3 muster gap. Moving it moves both; never tune it for
    one beat without checking the other.
- **Raw ends:** the wall terminates in a stepped, unfaced core — a ragged
  vertical break, never a clean end. At range this is the whole tell.
- **Lower and unparapeted:** `CASTLE_WARD_C_HEIGHT_FRAC` = 0.6–0.75 of the B
  wall height, flat-topped, no crenellation **(предложение — утвердить)**.
- **Unfaced core** on the last stretch: lighter, rougher value than B's facing
  — inside the same block-size-and-value grammar, so it reads at 640×360.
- **Stopped work on the ground:** a spoil heap and stacks of dressed,
  never-laid stone at the raw end. Two props that say "stopped" better than
  any amount of wall detail, and they date the stoppage to a person rather
  than to history.

Bonus the fiction did not have to pay for: at 0.6–0.75 height and partial
extent, C adds almost nothing to the silhouette budget — it is nearly free
against R3 and the §6.1.1 checks.
**FUTURE (act 3):** the gap is a hole a muster must hold. Whenever that beat
needs a specific footprint, the completion fraction is the knob — story comes
to me with the beat, not with metres.

**The phasing is legible in stone (story's ask, and the two systems already
agree).** Each ward carries a masonry generation: **phase A is the oldest and
roughest**, later wards progressively more regular. At 640×360 this must be
carried by **block size and value, never surface texture** — fine coursing
detail is invisible at our resolution, so: older = larger irregular blocks in
a darker weathered value; newer = smaller regular coursing in a lighter,
cleaner value. Read at distance it becomes a tonal difference *between wards*,
which is exactly the point.

This costs nothing because the terracing rule already puts stage A uphill,
nearest the crag and nearest the barrow, with later wards stepping down toward
the valley. So the oldest, roughest, most hurried masonry is the ring closest
to the grave — story's "phases of dread" and my terrace order are the same
geometry, arrived at independently. Free coherence; keep it.

**One corner tower overlooks the barrow (story's ask, ruled binding).** The
corner tower nearest the Backbarrow must have **clear line of sight from its
top to the barrow entrance**, validated by the same raycast as the yard/gate
sightline (§6.1.2) and equally protected from later occlusion. A garrison
posted on a grave should be visibly posted *on the grave* — and unlike most
narrative asks this one is free: the tower exists anyway, the requirement only
fixes which corner it is and forbids the terrace cut from blinding it.

**Readability changes, and that is intended.** At 80–120 m across, the
fortress *will* now read from Vaelmere (≈ 390 m) where the old hall was
deliberately sub-threshold. R1 still holds — it reads against the crag's body,
never sky — so it does not steal the skyline; what changes is the nature of
the reveal. It shifts from *"you did not know it was there"* to **"you did not
know how big it was"**: at distance a grey horizontal band at the crag's foot,
and only on approach do the gate, the four towers and the hall over the wall
resolve into a fortress. That is still occlude-and-reveal, and arguably a
better version of it.

The envelope must accommodate story's act-1 interior set (public hall, yard,
muniment room, solar) — interiors are **not** designed here, only the footprint
that leaves room for them. The access invariant (§6.1.2) is unchanged and now
applies to the **gatehouse** specifically: the graded ramp runs to the gate
threshold, and a fortress that a petitioner cannot walk into fails the same
way a scarp-only pad did.

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

### 6.2 Dungeon entrances — archetypes (ruling, stage-3)

An entrance is a **terrain feature first and a prop second**. The generic
building-pad scorer (flat + dry) is the wrong tool: a cave mouth needs a
hillside to face out of, and flat dry ground is precisely where it cannot
exist. Two archetypes, selected by measured relief.

**Selection rule.** Measure relief within 25 m of the candidate site. Relief
≥ `DUNGEON_ENTRANCE_MIN_RELIEF` = 6 m → **adit** (horizontal). Below that →
**sunken barrow** (descending). **(предложение — утвердить)**

**Marker and facing are DERIVED, never tabled** — from the carve mouth
position and its outward normal. This is the §7.1a derived-only rule, which
now explicitly covers **carve-adjacent** placements as well as water-adjacent
ones: any prop whose meaning depends on generated geometry is derived from
that geometry. A marker 10 m from its mouth is not a cosmetic defect, it is
the same class of bug as a ford that isn't on the river.

#### 6.2.1 Adit (sloped ground, ≥ 6 m relief)

Mouth cut into the hillside, facing out along the slope normal; stub passage
15–20 m. Frame the mouth with a dark lintel and a 2–4 m rubble apron. The
hill itself supplies the silhouette, so no extra marking is required beyond
the scatter exclusion below.

#### 6.2.2 Sunken barrow (flat ground) — the flat-ground answer

Do **not** place a bare hole. A hole in flat ground has no silhouette, cannot
be found, and reads as a bug — which is exactly what happened. Instead, the
generator **makes** the relief it needs, in the shape the fiction already
wants: a **mound with a cut forecourt leading to a lintel in its flank**.
This is a real chambered-barrow form (mound + forecourt + portal), it is one
radial stamp plus one linear trench stamp, and it solves geometry and
readability with the same gesture:

| Element | Value (предложение — утвердить) | Purpose |
|---|---|---|
| Mound | `BARROW_MOUND_HEIGHT` 3.0 m, `BARROW_MOUND_RADIUS` 15 m | the silhouette — restores the "hillside to face out of" |
| Forecourt trench | `BARROW_FORECOURT_LENGTH` 8 m, `BARROW_FORECOURT_WIDTH` 3 m, descending to `BARROW_FORECOURT_DEPTH` 2.5 m at the portal | the descent, walkable |
| Trench slope | ≤ `BARROW_FORECOURT_SLOPE` 0.35 rad (20°) | under `PLAYER_MAX_SLOPE` with margin; no step > `PLAYER_STEP_HEIGHT` |
| Standing stones | `STANDING_STONE_COUNT` 2–4, `STANDING_STONE_HEIGHT` 2.0–2.5 m | vertical accents flanking the approach — a leading line pointing in |

Mound crest to trench floor gives ≈ 5.5 m of working relief — enough for a
portal at human scale. It is walked down, never fallen into.

**Findability layers** (a descending entrance must be *earned* by the
approach, §1.4): (1) the mound's silhouette; (2) standing stones as the
directional cue — they read as *intentional* at distance, which nothing
natural does; (3) a scatter exclusion ring, `ENTRANCE_SCATTER_EXCLUSION` =
mound radius + 5 m, with no trees or bushes and shortened grass — in a forest
clearing a bald ring reads as made ground; (4) value: the trench interior is
the darkest value in the frame, and the eye goes to the dark hole. At the
forest-ruin site this stacks with the ruin walls §7.1 already specifies —
ruin above ground, barrow beneath it, one coherent site.

#### 6.2.3 Attractor status (C1/C2)

- **The assembly counts, the hole never does.** Mound + stones + ruin are the
  attractor; the forecourt and portal contribute no silhouette and are never
  counted.
- It credits C1 **only within its readable range** — a 3 m mound at 15 m
  radius clears `SILHOUETTE_MIN_PX` out to roughly 90–110 m at 640×360, so it
  is a short-range L1. Beyond that the clearing's approach rests on the forest
  edge and corridor guides, not on the barrow.
- It counts **once** as a composite POI (§6.1.2), and being threshold-scale it
  will rarely join a coequal crowd (§1.1 C2-testbed).
- Trivially compliant with C4 at 3 m; a sunken barrow can never contest the
  L0.

### 6.3 True-darkness places (stage-4 ruling)

The user keeps night **playable** — moonlit and navigable — but wants specific
places to be pitch black: «чёрную пустоту, где даже факел освещает лишь мелкий
клочок света». So darkness becomes a property of a **place**, never of the
clock.

**Graded, not binary.** Each enclosed volume carries an `AMBIENT_FLOOR` in
[0, 1]: the minimum light that exists regardless of sources. Exterior night
sits at the moonlit floor; a crypt with light shafts sits between; true
darkness is `AMBIENT_FLOOR = 0`. A binary flag would force every dark place to
be *equally* dark and would waste the most atmospheric range we have.

**What qualifies — a rule, not a list.** True darkness is available to any
volume that is (i) **fully enclosed**, no sky access, **and** (ii) beyond
`DARKNESS_DEPTH_MIN` = 25 m of path from its nearest entrance
**(предложение — утвердить)**. Consequences that make this the right rule
rather than a naming exercise: a barrow's forecourt and first chamber are dim,
its inner chamber is black; darkness is **earned by depth**, so the player
learns the language and can predict it; and no designer has to remember to tag
a location. Story may of course *want* a specific place dark — under this rule
they get it by making it deep, which is the same thing the fiction already
says.

**No unfair surprises — three required layers:**

1. **A lit threshold.** The last space before true dark must itself be
   visibly lit, so the boundary is seen from outside as a wall of black you
   choose to enter.
2. **A gradient, never a plane.** Ambient falls off over
   `DARKNESS_FALLOFF` = 8–12 m **(предложение — утвердить)**. Walking into
   darkness is a slope, not a switch.
3. **An audible cue** at the threshold (change in reverb/ambience) — sim and
   audio own the implementation; design's requirement is that it exists.

**The torch must still work.** In true darkness the torch's usable radius
stays ≥ `TORCH_RADIUS_DARK` = 4 m **(предложение — утвердить)** — enough to
show the floor and a couple of steps ahead. "Lights only a small patch" is
atmosphere; "cannot see your own feet" is a control problem wearing
atmosphere's clothes.

**C1 does not apply inside true darkness, and something else must.** The
no-dead-horizon rule is meaningless where nothing is visible, so true-dark
volumes are **exempt from C1** — and inherit a replacement guarantee in its
place: **the way out must always be findable.** Either the passage is
unbranching within the dark zone, or the entrance direction is discoverable by
a non-visual cue (draft, sound, a faint glow at the threshold). Getting lost
in the dark is a designed feeling; being *stranded* in it is a bug. Interior
layout belongs to whoever owns dungeon interiors — this is the contract they
must satisfy, and the reason `AMBIENT_FLOOR` belongs in world data rather than
in a renderer setting.

## 7. Testbed application (worldgen v2, что core реализует первым)

Canvas: 4×4 chunks, world XZ = 0…1024 m both axes, seed 1, current surface
16–26 m. All coordinates below are *generator parameters* (a testbed layout
table in the worldgen tool's data), not hand sculpting — each is the center
of a procedural stamp/scorer, tunable and deterministic. **Все координаты и
высоты — предложение — утвердить.**

### 7.1 The plan (feature list, in pass order)

| Feature | Where (x, z) | Parameters |
|---|---|---|
| **L0: Ravenscar Crag** + watchtower ruin | peak (830, 200), footprint r ≈ 180 m | ridged-noise stamp; peak raised to `L0_RELIEF` **110–120 m** (approved, §5.7 — needs the raised `WORLDGEN_MAX_HEIGHT`); rock splat above the stamp's rockline; tower ruin (§6) on peak; **validated summit route — see below** |

**Ravenscar's ascent is a required invariant too (gap exposed by story's
near-miss).** Act 1's climax is the climb to the ward-tower, and I had
specified a validated route for the temple massif (§2.5) and for the castle
ramp (§6.1.2) but never for the crag itself — the landmark whose summit the
story actually uses first. Rule: a **continuous walkable route from valley
ground to the tower ruin** must exist and be validated every worldgen run,
average slope ≤ 25°, never exceeding `PLAYER_MAX_SLOPE`, no step >
`PLAYER_STEP_HEIGHT`. It is a *path*, not a stair — unbuilt, informal, the
line four generations of watchmen wore into the spur — which also keeps it
visually distinct from the Steps (§2.5), so the two climbs never read as the
same place. At 110–120 m of relief this is a real climb; the L0 sight-wedge
rules (§5.7) already keep its approaches clear of canopy.
| **River** | source (760, 300) → lake; exits lake → south edge ≈ (300, 1024) | §3.1 algorithm; width 4→7 m; sinuosity ≥ 1.15; **fords are derived, not tabled** — P2 places them where POI-chain corridors cross the *generated* trace (§3.1 step 6), plus the `FORD_SPACING` minimum |
| **Lake** | center (230, 520), ≈ 90×140 m target | basin stamp, water plane ≈ 15.0 m (`LAKE_LEVEL_TESTBED`); shore sand per §3.3 |
| **Town site (TESTBED_TOWNS = 1): hamlet "Vaelmere"** | (360, 500), east lake shore / river inflow bend | hamlet per §6: tavern head faces the lake; trader at corridor entry; pads flattened at ≈ 17–18 m |
| **Shrine knoll** | (560, 620) | knoll +6 m local bump stamp; shrine spire breaks skyline from town and from ford (430, 620) |
| **Dungeon 1: barrow in the crag** (TESTBED_DUNGEONS 1/3) | entrance (780, 290), south face of the crag | entrance pad + dark portal frame; visible from foothill watchpoint, not from town (occlude-and-reveal) |
| **Castle: House Corvane's seat** (§6.1) | pad center (760, 330) ± 20 m, crag SW foot spur, ground ≈ 24 m | terraced 60 m pad; keep ≤ 15 m (R3); composite POI with the barrow; commands the watchpoint ford; scored in C1 as occluder AND attractor |
| **Dungeon 2: forest ruin** | (620, 850), inside SE oak forest | in a clearing (r = 25 m); ruin walls = L2 from clearing edge; ground is flat here, so the entrance is the **sunken barrow** archetype (§6.2.2) — stamped mound + forecourt under the ruin |
| **Dungeon 3: lakeshore cave** | (180, 350), NW lake shore — mouth at the **foot** of the 10 m bluff, never its crown | adit (§6.2.1), 15–20 m stub; mouth ≥ 2 m above the lake plane; reachable along the sand shore; visible across the water from town (water gap = curiosity) |
| **Foothill watchpoint (minor POI)** | (660, 430) | rock outcrop cluster + lone skyline pine + ford; bridges the town↔barrow gap in the POI chain |
| **Forest masses** | oak: S+SE band (roughly z > 700 plus x > 500, z > 600); pine: **radial ridge strips** on the crag foothills (4 sectors, duty 0.25 — layout knobs `pine_strip_count`/`pine_strip_duty`; a closed annulus can never pass canopy-C1, see §1.3) + N ridge strips | total coverage ≈ 0.30 of land; clearings per §2.2; birch lines along river and lake banks (derived from `dist_to_water`, never tabled) |
| **Meadows** | center and west | flower patches, outcrops, meadow clusters per §2.2–2.3 |

### 7.0a Re-siting the barrow after the L0 raise (stage-4 ruling)

Raising Ravenscar 52 → 115 m buried the Backbarrow: at 81–105 m from the crag
centre the terrain is now 40–64 m, so there is no hillside there to open a
mouth in. **This cascade is mine** — I proposed the raise and did not check
what was anchored to the old surface.

**The durable rule it produces, worth more than the fix:**
**§7.1 coordinates are stamps against a specific terrain state.** Anything
sited *on a landmark's slopes* — entrances, pads, routes — holds an implicit
dependency on that landmark's relief. **Changing a landmark's height
invalidates every placement on it and re-validation is part of the change, not
a follow-up.** Same seam class as the missing Ravenscar ascent: the fact lived
in one zone, the dependency in another.

**RULING: swing the bearing, do not move the castle.** Rejecting core's (a)
and (b) — both move the castle, which cascades into the ford-command
distance, the approach corridor, the trespass route, the terrace/ward count
and the R1 footprint check, to buy something a cheaper change buys outright.
The barrow does not need to move *outward*; it needs to move *around*, into a
**couloir** — one of the re-entrant folds between the ridged stamp's buttress
ridges, where terrain at the same radius is still near valley level.

Measured feasible window (castle unmoved at (760, 330); barrow currently
radius 103 m, bearing 209° from the crag centre):

| Bearing from crag centre | Radii keeping `CASTLE_BARROW_DIST` 40–80 m |
|---|---|
| 180° | 100–110 m |
| 190°–230° | 90–110 m (the whole band) |
| 240° | 110 m |

So there is a **≈ 60° arc** of legal placement. Core's test: within bearings
180°–240° at radius 90–110 m, find samples where terrain ≤ ≈ 28 m (valley
level ≈ 20.4 m plus a working margin), pick the one **nearest the current
209°**, and site the mouth there by the §6.2.1 adit rules. A ridged stamp
produces couloirs by construction, so this should exist; it is a search, not a
carve.

**Fiction cost: almost none.** Proximity, relative elevation, and "the seat
stands over the grave" all survive; the gap and the barrow-facing tower are
defined *relative to the barrow*, so they follow the new bearing
automatically. The only change story absorbs is a compass direction moving up
to ~30°. A grave hidden in a fold of the mountain is also, if anything, the
better image.

**Fallback if no couloir clears:** core's (c) — a **high entrance on the
shoulder**, mouth 20–44 m above the valley, castle unmoved. It keeps
everything geometric but **inverts one line of story's canon**: the grave then
stands over the seat rather than under it. That inversion is arguably stronger
(the Corvanes cannot escape being overlooked by what they did) but it is
story's sentence, not mine — pre-cleared with them rather than assumed.
Options (a) and (b) remain last resorts.

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
