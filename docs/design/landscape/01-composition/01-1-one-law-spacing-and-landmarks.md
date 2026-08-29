
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

**AND THE CANOPY REALLY IS NEARLY OPAQUE — measured, and it refutes both
sides of the argument that was about to be had (flora, stage-4, from the
user's own reference photographs).** When foliage became alpha cards (§5) I
wrote down the worry that C1 would end up "measuring a wall that no longer
exists", and asked for a measured effective `w`. The answer is the opposite of
what either of us expected, so it is recorded here rather than left in a
thread:

| Depth into the crown | Transmittance |
|---|---|
| outer rim | 23–24 % |
| ≈ ¼ in | 10–14 % |
| ≈ ½ in | 3–4 % |
| interior | 0.5–4 % |

Clean Beer–Lambert decay, two exposures of the same tree agreeing within three
points, fitted extinction **k ≈ 0.84 per metre** (half-depth 0.83 m, T < 3 %
beyond ≈ 3 m of penetration). **A real crown is 79–86 % leaf at its core;
porosity is a RIM phenomenon, not a whole-crown one.** So the solid-crown `w`
is close to correct, no coefficient change is worth making yet, and **the
one-time physics-correction budget above stays unspent.**

**Both of us were wrong, in opposite directions, and that is the part worth
keeping.** I feared the model was now too opaque; flora expected foliage on
terminal twigs over a hollow interior, which is further from the truth than my
version was. Neither reading survived contact with the pixels. The lesson is
the §1.3-withdrawal lesson again in a cheaper form: **the disagreement was
settled by measuring, before either belief was written into a rule.** That is
the second finding this stage that arrived as a refusal to hand over a number
that would have moved a model, and it is the behaviour to keep.

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

> ### ⚠ C4 IS NOT A DOCTRINE GAP — IT IS AN UNENFORCED RULE WITH A STALE
> ### CONSTANT (regression, reported not patched, stage-4)
>
> In `screenshots/crag/06_crag_w_300m.png` the near pines stand **three to four
> times the L0's apparent height.** C4 says in as many words that *nothing
> which is not the L0 may exceed L0's apparent height from the main travel
> corridors — including canopy.* The rule is correct, it is implemented, and
> it is being violated grossly. **The reason is a stale constant.**
>
> The world's occlusion model — the sight-wedge filter that rejects trees, and
> the canopy height field that feeds the C1 raycast — hard-codes
> `OAK_MAX_H = 12`, `PINE_MAX_H = 18`, `BIRCH_MAX_H = 10`. The world is built
> with `OAK_HEIGHT_MAX` = 32, `PINE_HEIGHT_MAX` = **38**, `BIRCH_HEIGHT_MAX` =
> 22. **Every occluder is modelled at roughly half its drawn height — pine at
> 2.1× under.**
>
> - **§5.7's tall-tree ruling landed in render and never reached the world's
>   occlusion model.** That is Rule 32 exactly: a shared quantity was changed
>   for one consumer and left stale for the others.
> - **So C1 = 0.751 is not merely denominated in the wrong currency
>   (§1.3b) — it was computed on a world model half the height of the world.**
>   Two independent defects in one number, and the second is fixable tonight.
> - **Every tree currently standing inside an L0 sight wedge is there because
>   the filter thought it was 18 m tall.** The wedges did not fail; they were
>   lied to.
> - **Design does not ask for a new rule here and asks for no threshold change.
>   The heights are core's to source from the same constants render uses.**
>   Reported, not patched, and re-measure C1 and the wedge rejections
>   afterwards — do not assume the ratio scales.

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

> ### ⚠ EVERY RULE IN §1.3 AND §1.3a IS CURRENTLY UNSHOOTABLE — LOD IS THE
> ### PRECONDITION FOR THE ENTIRE LANDMARK DOCTRINE (render's finding, stage-4)
>
> `CHUNK_LOAD_RADIUS` is 2 chunks ≈ **512 m**, so **the world stops existing
> at about half a kilometre.** Render's attempt to shoot the §7.1b verdict
> frame at 717 m produced a picture with **no mountain in it at all** — the
> chunks were not resident — and they verified it by walking the same bearing
> in until the massif appeared. Consequences, ruled:
>
> - **`LANDMARK_MAX_DISTANCE` = 4 km and `CAMERA_FAR` = 8000 m are currently
>   fiction.** A landmark sited at 1.4–1.6 km is not hazy, it is *absent*.
>   §1.3a's whole depth-separation doctrine — hazy-and-huge far goal versus
>   solid-and-detailed near landmark — **cannot be observed in the engine
>   today**, and the LR that doctrine exists to place would be invisible from
>   the valley it is meant to pull the player toward.
> - **C1 has never been confirmed by a camera.** It is computed analytically
>   on the heightfield, which is legitimate and is not in question — but every
>   "the landmark reads from the valley floor" claim in this document is a
>   claim about a frame nobody can currently take. That is the §2.8.7 defect
>   in a different costume: **the rule and its verification live in different
>   spaces.**
> - **So LOD is not a performance nicety, it is the precondition for the
>   landmark doctrine**, and it should be scheduled as such rather than as
>   optimisation. Stated here, where the C1 rules live, so that whoever plans
>   that work finds the reason next to the rules it unblocks.
> - **Raising `CHUNK_LOAD_RADIUS` is NOT the answer and design does not ask
>   for it.** Core measured ≈ 72 ms per chunk and `CHUNK_LOAD_BUDGET` is
>   already 1 per update because of a user freeze complaint; a wider ring buys
>   one screenshot and costs seconds of hitching in play. The frame waits.
>
> **PARTLY WITHDRAWN — §1.6.4 corrects this box on a measurement.** Residency
> is **chunk-granular, not metric** (Chebyshev radius in chunk units, clipped
> to extent), so the 717 m vantage failed on its **bearing**, not its range.
> And §1.6.1 shows Ravenscar's acceptance distance is **360 m, not 717 m** —
> so **LOD is not the precondition for the L0's own frames**, which are
> shootable tonight. The box stays true for the LR and for anything beyond
> ≈ 800 m, and §1.6.3 adds the harder fact: **the LR does not exist in the
> generator at all.**

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

### 1.3b C1 MEASURES OCCLUSION, NOT LEGIBILITY — the two-number instrument (ruling, stage-4)

**A landmark can be 100 % unoccluded and invisible, and we now have the frame
that proves it.** `LANDMARK_VISIBILITY_MIN` is a raycast test: it asks whether
a line from the eye to the landmark is interrupted. Nothing in it can see that
two unoccluded masses have merged into one dark shape. So **C1 has been
certifying a property it cannot measure**, and every figure it has produced —
including the 0.751 I offered as spur budget one message ago — is denominated
in the wrong currency.

**C1 is NOT retired.** Occlusion is a real and necessary condition, and the
raycast measures it correctly. It is **re-scoped**: `LANDMARK_VISIBILITY_MIN` =
0.6 remains a floor on occlusion and **stops being cited as a legibility
figure.** Until the instrument below exists, every C1 number is treated the way
§1.6.3 treats UNSHOT rules — recorded, not certifying.

> **C1 RE-MEASURED ON HONEST TREE HEIGHTS (core, stage-4): 0.751 → 0.6429
> against a floor of 0.60.** Three readings of that number, and the third is
> the one that must travel with it:
>
> 1. **The fix landed.** The number moved *down*, which is the predicted
>    direction once every occluder doubled in modelled height. A figure that
>    moves as predicted is evidence; one that does not is a second bug.
> 2. **It passes with 0.043 — a 7 % margin — and that is MARGINAL by this
>    document's own standard.** «A marginal pass on one seed is not
>    compliance», and I do not know how many seeds this is. **Request: the
>    min/median/max across the twelve, as §2.8.3 requires of every other
>    invariant.** If the median sits near the bound, the forest moves, not the
>    threshold. The apron (§5.12) should raise it well clear regardless.
> 3. **IT IS STILL AN OCCLUSION NUMBER AND MUST NOT BE RELAYED AS «THE LANDMARK
>    READS».** It is now an honest measurement of the thing it always measured.
>    The legibility question is untouched and stays UNSHOT until C1-A/C1-B
>    below are built — and the frame that started all of this had C1 passing
>    comfortably.

**The instrument, approved by the lead and built jointly by render and core:
core's terrain-only horizon as the REFERENCE curve, the drawn frame's horizon
measured in VALUE. Two numbers, because occlusion and merging are demonstrably
different failures. The thresholds are mine and are below.**

##### C1-A — OUTLINE FIDELITY (does the drawn outline belong to the mountain?)

Per screen column across the landmark's angular span, compare the **drawn**
horizon to the **terrain-reference** horizon. A column is FAITHFUL when they
agree within one readability window (1/30 rad).

> **`LANDMARK_OUTLINE_FIDELITY_MIN` = 0.90 of columns faithful (предложение —
> утвердить), AND no contiguous unfaithful run longer than one readable unit.**

**The run-length clause is the load-bearing one and the fraction is the guard.**
A fraction alone is satisfiable by a tree wall that eats one whole flank while
90 % of the outline elsewhere stays clean — which is exactly the failure in
`screenshots/crag/06_crag_w_300m.png`. Derived from the feature budget of
§1.6.1: at d_accept the landmark spans ≈ 20 readable units and must carry six
silhouette features, so a contiguous loss of more than one unit can delete a
break together with its flanking run, and a break that is not flanked is not
detected.

##### C1-B — BODY EXPOSURE (is the mountain's *body* there, or only its cap?)

**The failure this exists to catch:** a mountain missing its bottom third loses
the bench and the flare of the base, and what survives is the upper cap — which
is convex on **any** mountain. **You do not need a domed mountain to get a
domed silhouette; you need a mountain with its base hidden.** No horizon test
can see this, because trees shorter than the crest do not touch the horizon at
all — they eat the body while C1-A reads clean.

> **The landmark's silhouette must be exposed — neither occluded NOR
> value-merged — continuously from `MASSIF_CLIFFLINE_FRAC` of its relief to its
> summit, across ≥ `LANDMARK_EXPOSURE_COLUMNS_MIN` = 0.90 of its columns
> (предложение — утвердить).** Below the cliffline, foreground is permitted.

**Derived from constants already in the document, not invented.** §2.8.7 ruled
that the body the eye reads as mountain **begins at the cliffline** and that the
apron below may flare; §2.8.8's I7 requires ribs to descend to that same line.
The cliffline is already this document's boundary between *mountain* and *hem*,
so it is the right place to put the exposure floor. Forest at the very hem is
legitimate — forest above the cliffline is not.

##### The value test — when are two masses "merged"?

> **~~Two adjacent regions are SEPARATE when the two colours they QUANTISE TO
> lie ≥ `LANDMARK_SEPARATION_STEPS_MIN` = 2 mean shade steps apart in the
> shipped palette.~~ — SUPERSEDED, stage-5. Replaced by the criterion below in
> this same section; a grep for the old wording lands here.**

##### RE-DERIVED FOR FULL COLOUR — the criterion, its unit, and what it costs

The user's ruling (§1.5) takes the quantiser out of the basis, and **the unit
this constant is denominated in was the quantisation floor.** At full colour
«one step» has no referent, so the constant cannot simply be carried over. It
is re-derived, and the answer separates cleanly into a FORM, a UNIT and a VALUE
— which is why the previous wording could not be patched.

> **THE FORM. Two adjacent regions are SEPARATE when the colours the player
> actually sees differ by ≥ `LANDMARK_SEPARATION_STEPS_MIN` = 2, where**
>
> > **separation(a, b) = √(0.30·Δr² + 0.59·Δg² + 0.11·Δb²) / `PALETTE_SHADE_STEP_REF`**
>
> **evaluated on the FRAME'S OWN PIXELS. This is `palette_separation_steps`
> with the quantise step deleted and the divisor frozen.**

**The single most important property, and it is what the old arrangement was
reaching for and could not have: ONE INSTRUMENT NOW READS BOTH CONFIGURATIONS,
because the quantisation has moved out of the instrument and into the input.**
Hand it pixels from a full-colour frame and it reports what a full-colour player
sees; hand it pixels from a quantiser-on frame and it reports what that player
sees. No doctrine is needed to relate the two, and the ±1–2 step lattice noise
measured in §1.5 — which the old instrument added to *every* reading, including
full-colour subjects it had no business quantising — is gone.

**THE UNIT. `PALETTE_SHADE_STEP_REF` = 0.0784 weighted-RGB, FROZEN AS A NUMBER
AND NOT AS A FUNCTION CALL** (measured from the live `palette_mean_shade_step()`
on the landed allocation, 0.078383). Two things change about it:

- **It stops being a floor and becomes a ruler.** A unit does not have to be a
  quantisation limit to be a unit; it has to be *stable*. Metres are not a floor
  on anything.
- **It must therefore be frozen, and this is a real hazard rather than
  tidiness.** `palette_mean_shade_step()` is computed from the current ramp
  allocation. **While the criterion divides by the live call, any re-allocation
  of the palette silently rescales every threshold in this document** — and the
  palette is scheduled to be re-derived wholesale (§4.3). A ruler that moves
  when the thing being measured moves is not a ruler. **Rule 35 applies by its
  own predictive form: this number gained a second consumer the moment design's
  thresholds and render's ramp construction both had to agree on it.** Requested
  as a NUMBERS.md row.

**THE VALUE — and here is where the lead's question lands: does 2 become a
luminance RATIO, a hue ANGLE, or something else? It stays a linear DIFFERENCE,
and the reason is where our failures actually are.**

- **Not a hue angle.** Hue angle is undefined as luminance → 0, and §4.2 has
  already established by measurement that **every merge this project has
  observed is in the darks.** A criterion that goes undefined exactly where the
  subject fails is not a criterion.
- **Not a luminance ratio, and this is the non-obvious one.** A ratio (Weber)
  criterion is the textbook-correct model of perception, and adopting it here
  would be **wrong in the only direction that matters**: at fixed ratio, the
  absolute difference required *shrinks* as luminance falls, so a ratio
  criterion is **most permissive in the darks** — the one region where we have a
  user-rejected merge on the record. A linear-difference criterion is strictest
  in the darks and laxest in the lights, which is strict where the failures are.
  **Chosen because of where the evidence sits, not because of which model is
  more sophisticated.**
- **The cost is stated rather than hidden: in the BRIGHTS the criterion is
  weak, and nothing has ever been tested there.** Bright pairs exist and matter
  — pale stratum against grey rock, spire white against pale rock (§4.1),
  anything against sky. §4.1 now carries a derived range because of this.
- **And the metric under-reads pure-hue separation** — it is a luminance-weighted
  RGB distance, not a colour-difference formula, so two colours of equal
  luminance and very different hue score lower than they look. **That error is
  in the safe direction** (it calls a separated pair merged, never the reverse),
  so the criterion is conservative for hue and accurate for value. Recorded so
  nobody re-derives it as a defect.

**STATUS OF THE NUMBER 2 — and I am not going to pretend it survived
untouched.** Its derivation is VOID: it was «one step is the quantisation floor,
and a threshold must sit above its own noise floor», and there is no
quantisation floor now. The value is **retained and re-based**, on Rule 30's
amendment:

- **The real rejected instance is the control.** Pine against shadowed rock —
  the merge the user rejected in words — measures **0.632** at full colour. The
  threshold sits **3.2× above it.** That clears it and clears the ≈1.6× margin
  this document takes elsewhere.
- **There is NO real accepted instance, and therefore no upper bracket.** We do
  not know whether 2 is conservative or merely inherited. Under §1.6.3's own
  status category the number is **calibrated below, UNSHOT above** — reported
  that way and not as «passing».
- **The experiment that would close it** is a frame containing two masses that
  measure between 1 and 2 and demonstrably read as separate. Cheap, and it
  belongs with the value-based silhouette instrument (C1-A/C1-B) rather than
  ahead of it.

**TIGHTENED from my original wording («2 steps, or 1 step across a ramp
change»), and render's question is what exposed the hole.** A ramp change is a
strong *heuristic* for separation, not a guarantee of one: **adjacent ramps
touch at their dark ends** — dry olive sits 0.046 from grass green there, which
is **less than a single shade step** — so «different ramp» can be a label
rather than a distance. Measuring between the **quantised entries** is what the
criterion always meant, it subsumes the ramp-change case (a genuine ramp change
clears two steps easily), and it closes the loophole. **Same constant, same
value, correct basis** — the identical act as I1's re-spec from surface mean to
envelope, and it cost nothing because the number was never the problem.

**Two, not one, and the reason is the same doctrine as I11's 20°:** one step is
the quantisation floor — two surfaces within one step are *literally the same
colour* after the post, so a one-step criterion measures the quantiser rather
than the image. A threshold must sit above its own noise floor.

- **~~The measured case:~~ `PINE_DARK` luminance 0.197 vs darkest rock stop
  0.192, «zero steps» — WITHDRAWN, and it was never the operative comparison**
  (§4.2, render's measurement). It compared a material to a *palette entry* on
  a luminance axis the metric does not use. **The correct measured case, live
  artefact, stage-5: pine vs shadowed rock = 0.632 at full colour, 0.700
  quantised.** The conclusion the wrong pair was used to reach — that pine and
  rock merge in shadow — is confirmed; the pair is not. The hue axis *does*
  separate them at source (pine saturation 0.45 against rock's 0.05) — see
  §5.12 for why that does not save the backlit frame.
- **~~THE TEST IS RUN WITH THE PALETTE ON, AND THAT CERTIFIES BOTH
  CONFIGURATIONS~~ — RETIRED, by the user's ruling AND by measurement.** The
  «lower bound» argument is false: a quantiser splits as readily as it merges,
  by up to a full step, which is what banding is. See §1.5. The re-derived
  criterion above needs no such argument, because it does not quantise
  anything.
- **~~Design's position: the quantiser should ship ON~~ — WITHDRAWN, and the
  user has ruled the other way** (§1.5). The argument was that §1.5's
  readability doctrine «has no premise» without a limited palette. **§1.5.1
  works through that claim rule by rule and it does not hold**: one bullet was
  scoped to the wrong mode, one is re-derived above, one was a constraint that
  full colour *relaxes*, and the headline defect measures worse at full colour
  than it does quantised. The lead's underlying correction survives and still
  matters: `settings.cfg` ships `palette=0`, **so the frames in this repository
  are what ships today** — they were never optimistic relative to the product.
- **Two shapes can occlude nothing and still merge.** That sentence is the
  reason there are two numbers and not one.

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

