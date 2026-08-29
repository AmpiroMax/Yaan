
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

