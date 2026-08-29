
### 10.14 THE MAN-MADE MID-GROUND — B3–B9 released, and three of my own briefs corrected on the way

Step 1 landed (`artifacts/acceptance/core-object-grammar-step1.md`): outcrops,
boulders, skirts, and a mid-ground count of 8 unoccluded against a floor of 5.
§10.13.4 item 6 held B3–B9's constants pending that frame. **The frame exists,
so the hold is discharged and this section releases them.**

The build order the lead confirmed is the one §10.6 derived — **fences, then the
settlement floor, then towers** — and it survives everything below, but not for
all of the reasons I originally gave. Two of my own claims did not survive
contact with the arithmetic, and both corrections are in the direction that
makes core's job smaller rather than larger.

#### 10.14.1 What step 1 licenses, and the one thing it does not

Step 1 proves the *natural* mid field. Nothing in it says anything about
man-made objects, and this section must not borrow its pass. What step 1 does
license is the **method**: §10.11.3's rule produced a number (8, not 17) that
nobody had to argue about, and every count below is specified the same way —
**established in the generator, confirmed on the frame.**

---

#### 10.14.2 B3 — FENCE LINES. RELEASED, and my "it is also an INSTRUMENT" claim needed repair before it was true

**B3's original text says the fence «does not merely benefit from bumpy ground,
it PROVES bumpy ground», and names the failure statement as «the fence's TOP
line is straight in screen space». Checked, and the top line cannot carry
that.**

> **The post top is contaminated by the post's own approved lean.** §10.3.2
> gives fence posts 3–15° of independent decay tilt. A 1.2 m post leaning 15°
> drops its tip by 1.2·(1 − cos 15°) = **0.041 m** and displaces it laterally by
> 0.31 m; at 25° the vertical term is 0.11 m and the lateral term 0.51 m. Seen
> along the run, that lateral swing reads as vertical wander of the top line at
> the same order as the relief the line is supposed to report. **An instrument
> whose reading is set by its own decoration is Rule 47 in a prop's costume.**

**The repair is a construction rule, and it saves the instrument rather than
retiring it:**

> **`FENCE_RAIL_HEIGHT_FRAC` = 0.75 of post height, measured from the post's
> BASE, never from its tip.** A rail hung at a fixed height above the ground
> tracks the ground; a rail hung off the post tops tracks the leaning. With the
> attachment at 0.75 h the lean's vertical error is ≤ 0.08 m at 25°, against a
> relief signal of σ ≈ 0.35–0.7 m — **an order below the signal instead of level
> with it.**
>
> **The failure statement is therefore about the RAIL line, and the top line is
> explicitly not the instrument.**

Even repaired, the stronger claim has to come down one notch, and it is worth
stating precisely because the sentence will be quoted:

> **A fence does not measure relief. It PICTURES it.** §10.11.3 already ruled
> that the ground profile is a raycast fact established in the generator; the
> rail line adds no information the raycast does not already have. What it adds
> is that **a human being can see the answer** — which is exactly the job
> §10.11.3 assigns to the frame («the frame's job is to confirm that what the
> generator says is there can actually be seen»), and B3 is the cheapest way in
> this document to make that confirmation possible with an eye instead of a
> probe. **It keeps its place second in the build order; the ordering was
> argued on triangles per unit of frame, and that argument is untouched.**

##### The sizes, all of them re-derived rather than carried over

| constant | was | **now** | why the number is what it is |
|---|---|---|---|
| `FENCE_POST_HEIGHT` | 0.9–1.5 m | **1.1–1.6 m** | **Floor from B6, ceiling from the walker.** B6 puts a shrub or tuft within 0.5 m of the contact on 50–80 % of all posts *by rule*, and a single shrub runs to 0.6 m. A 0.9 m post carries 0.3 m of clear timber above its own skirt — the rail line then runs *inside* the ground cover and stops being a line. 1.1 m leaves 0.5 m of clear air under the rail. The ceiling is `PLAYER_EYE_HEIGHT` = 1.7 m: above ~1.6 m a roadside fence stops being something you see over and becomes a wall, which is a different object with a different brief |
| `FENCE_POST_SPACING` | 1.8–3.0 m | **1.5–2.0 m** | **The ceiling is §10.2's own aliasing argument, turned on the fence.** The run samples the terrain at its post bases, and the shortest band it must report is `GROUND_MICRO_WAVELENGTH_MIN` = 8 m. At 3.0 m spacing that is 2.7 samples per period — §10.2's exact aliasing case, and the rail line would *swim* as the player walks rather than describe the ground. Four samples per period is the floor for a curve, so 8/4 = **2.0 m**. This is a tightening of my own band by an argument the document already owns |
| `FENCE_RUN_LENGTH` | 15–80 m | **20–80 m** | Floor: a run shorter than one full micro period (16 m max) cannot show an undulation, only a tilt. 20 m carries one period with margin. The ceiling is composition, not read: past 80 m a run must corner, change level or gate, for the same reason `KERB_STRAIGHT_RUN_MAX` exists |
| `FENCE_RAIL_HEIGHT_FRAC` | — | **0.75** | new, derived above; **from the base** |
| `FENCE_POST_YAW_MAX` | ± 10° | **± 10°** | unchanged |

##### Two forms, because sixteen frames show two fences and one band cannot be both

Frame 02's paddock beside the timber hall is nearly plumb and nearly complete.
Frame 15's run is derelict — posts at 20–25°, and well over half the bays have
lost their rail. **A single band drawn uniformly produces a fence that is
neither**, which is §10.3.1's failure ("noisy rather than weathered") in a new
place.

| | **kept** (frame 02) | **derelict** (frame 15) |
|---|---|---|
| `FENCE_GAP_FRAC` | **0.05–0.15** | **0.30–0.55** |
| `FENCE_POST_LEAN` | **1–6°** | **6–25°** |
| what carries the line | the rail | the **post tops**, as a dotted line |

**The form is chosen by the anchor and needs no constant:** a run that encloses
a field adjacent to a building pad is *kept*; a run on a corridor verge with no
pad attached is *derelict*. And the derelict form does not lose the picture —
§10.5 B4's own precedent applies, a regularly spaced dotted line reads as a line
long after the dots stop being objects.

##### Where they go — derived siting, no tabled distances (§1.6.1's doctrine)

> **`FENCE_ROAD_OFFSET` is DERIVED, not tabled. The run's near edge sits at or
> beyond `CORRIDOR_WIDTH` from the corridor centreline, and within
> `CORRIDOR_WIDTH` + 4 m of it.**

**The floor is a Rule 48 argument and it is the load-bearing part of this
brief.** §2.4's corridor relief mask (`WorldgenRelief.cpp`) is **zero** inside
`CORRIDOR_WIDTH`/2 and ramps to full only at `CORRIDOR_WIDTH` from the
centreline. **A fence closer than that stands on ground that is authored flat,
so its rail line is straight by construction and the frame can never fail** —
the criterion would be measuring the corridor's own flattening invariant, not
the terrain. At today's `CORRIDOR_WIDTH` = 10 m the band is 10–14 m from the
centreline, i.e. 5–9 m outside the travelled way; **if the corridor narrows, the
offset follows it without anyone re-arguing a metre value.**

> **`FENCE_APPROACH_LENGTH` is derived too: fence runs line a corridor over the
> last 30 × (the site's tallest built mass) of its approach** — the distance at
> which the site is already an object under Rule 33. For a hamlet of 7 m
> buildings that is ~210 m. A fence leads the eye *to a thing the eye can
> already see*; a fence leading toward nothing legible is an arrow with no
> target. Fences are otherwise **sited, not scattered** — same call as B4's, and
> for the same reason (a per-hectare number here would create a second placement
> authority).

##### Cost, corrected honestly

A 60 m run at 2.0 m spacing is 31 posts and 30 bays: **31 × 10 + 30 × 4 ≈ 430
triangles.** The brief and the lead's instruction both quote ~350; that figure
was taken at 2.4 m spacing, and the anti-aliasing tightening above costs about
80 triangles per run. **Still the cheapest thing in this document per unit of
frame, and the number should travel corrected rather than round.**

##### The one number B3 asks for that is not a size

> **`FENCE_RAIL_DEVIATION_MIN` = 0.10 m**, RMS deviation of the rail attachment
> height from the best-fit straight line over a run, **computed in the generator**
> from post base heights, read at `ACCEPTANCE_PERCENTILE` = 5 over runs.

**Constructed exactly like §10.1.2's original σ floor and deliberately below
what the approved octaves predict** — the micro octave alone (0.3–0.6 m over
8–16 m) leaves ~0.15–0.30 m of residual over a 20 m run, so a correct build
passes with margin and a run sited on flattened ground fails. **And it does not
repeat σ's mistake** (§10.12): σ was a proxy for slope; this is the line's own
straightness, which *is* the property the failure statement names. Its zero-dose
control is well behaved — a fence on the corridor pad reads exactly 0.000.

---

#### 10.14.3 B5 — KERBS, STEPS, RETAINING WALLS. RELEASED, and one requested row deleted before it was approved

The ruling B5 exists to produce stands unchanged. **Its constant does not.**

> **`BUILT_EDGE_LEVEL_CHANGE_MIN` = 0.4 m is WITHDRAWN as a row before it lands.
> The rule is `PLAYER_STEP_HEIGHT`.**
>
> A level change the player can walk over needs no architecture; a level change
> the player cannot walk over is exactly the one a built place resolves with a
> kerb, a step or a wall. `PLAYER_STEP_HEIGHT` = 0.35 m already *is* that
> boundary, and 0.4 m was a rounded copy of it — Rule 39, the same call as
> `TOWER_MINOR_DIM_PER_DISTANCE` and the one-shade-step floor of §10.10.2. **A
> constant that a second constant defines to within 14 % is a shadow copy that
> will drift the first time either moves.**

##### Sizes, with the derivations attached

| constant | proposed | why |
|---|---|---|
| `KERB_HEIGHT` | **0.15–0.30 m** | The ceiling is derived: **a kerb the player must jump is a wall.** `PLAYER_STEP_HEIGHT` = 0.35 m, less a 0.05 m margin so a kerb on a 5° pad (`BUILDING_PAD_SLOPE_MAX` = 0.09 rad) is still steppable at its high end |
| `STEP_RISE` | **0.15–0.20 m** | Agrees with `PLAYER_STEP_HEIGHT` with 1.75× to spare, which is what §10.5 flagged for Rule 35 and what movement has to co-sign. Frame 10's flights read ~0.19 m rise |
| `STEP_TREAD` | **0.30–0.45 m** | Frame 10 reads ~0.30 m. Kept wide at the top of the band deliberately: 2R + T lands at 0.70–0.75 m against a real-world comfort figure of ~0.63, i.e. **our stairs are shallower than real ones**, which is the right side to err on for a walker with no stair animation yet (в24) |
| `RETAINING_WALL_HEIGHT` | **0.8–2.5 m** | Frame 07's dry-stone wall reads ~1.5–2 m at the street |
| `RETAINING_WALL_BATTER` | **3–8°**, into the bank | unchanged |
| `KERB_STRAIGHT_RUN_MAX` | **12 m** | **Provenance stated because it is measured, not derived:** scaled against frame 10's doorways (~2.1 m), the longest unbroken straight terrace edge in that plaza is 5–6 door-heights, ≈ 11–13 m. The reference frames are the authority for a composition rule (§7.1's oldest clause), and a measured figure off the reference is stronger than a figure derived from an unrelated constant |

##### The consequence core actually needs, and it is a count rather than a density

**The hamlet common already contains a level change it must resolve.** At
`BUILDING_PAD_SLOPE_MAX` = 0.09 rad across `HAMLET_COMMON_RADIUS` = 15–25 m, the
common falls **2.7–4.5 m corner to corner**. At `PLAYER_STEP_HEIGHT` = 0.35 m
per built edge that is **eight to thirteen kerb, step or wall lines across one
hamlet common** — not a target, a consequence of two constants that were already
approved.

That is the number to hand core: **a hamlet floor is not "add some kerbs", it is
about ten built edges, and a hamlet with two is failing an arithmetic
requirement rather than a taste one.** Cost: a kerb run is ~2 triangles per
metre, so ten runs of 15 m ≈ **300 triangles for the entire settlement floor.**

---

#### 10.14.4 B4 — TOWERS. RELEASED, and I have to correct my own correction to `REFERENCE_FRAMES.md`

§10.5 B4 and the correction I filed against `REFERENCE_FRAMES.md` both say frame
06's readable unit is «the whole assembly, gap included». **I opened the frame
again and that is not what it shows.**

> **CONJOINED, NOT ADJACENT.** Frame 06 contains **two tower masses ~25 m
> apart**, and each mass is itself **two or three drums sharing wall** — a
> contiguous plan extent of roughly 8–10 m, not a single 5–6 m drum. *That* is
> the assembly that reads, and it anchors to **240–300 m**, not 180.
>
> The timber span between the two masses is **composition and route, not
> silhouette mass.** At 240 m a 0.4 m beam is a tenth of `SILHOUETTE_MIN_PX`; it
> contributes nothing to the outline and cannot join two masses into one.
>
> **RULING: a gap joins two masses into one readable mass only if the gap is
> filled with material of the same order as the masses.** A curtain wall does
> it, a shared plinth does it, **a beam does not.** So «build a group» was half
> an answer: to anchor at 500 m you still need 17 m of *contiguous* plan
> dimension, and a group delivers that only when the group is contiguous — a
> keep, a gatehouse block, a walled bailey.

**This makes the brief cheaper, not more expensive:** two conjoined 5 m drums
buy 240 m of anchor for barely more geometry than one, and that is the whole
trick frame 06 is doing.

| constant | proposed | why |
|---|---|---|
| `TOWER_DRUM_DIAMETER` | **4.5–7.0 m** | frame 06, read against the figure standing on the right mass's platform |
| `TOWER_DRUM_HEIGHT` | **10–16 m** | same frame |
| `TOWER_DRUMS_PER_CLUSTER` | **2–3, conjoined** | walls interpenetrating, not tangent — a tangent pair has a re-entrant notch at the join and reads as two |
| `TOWER_CLUSTER_PLAN_MIN` | **8.0 m** | the contiguous minor plan dimension that makes the cluster a 240 m object. Below it the cluster is a 180 m object wearing a group's costume |
| `TOWER_CROWN_NOTCH_DEPTH_MIN` | **0.5 m** | **re-scoped: this is a WALK-PAST rule, not a silhouette rule.** 0.5 m clears `SILHOUETTE_MIN_PX` only inside 15 m, and saying so stops it being cited as a distance guarantee |
| `TOWER_CROWN_NOTCH_COUNT_MIN` | **3** | unchanged |
| `MASONRY_BLOCK_YAW_MAX` / `MASONRY_COURSE_OFFSET_MAX` | **8° / 0.15 m** | unchanged (§10.3.3) |

##### `TOWER_CROWN_LINE_VARIATION_MIN` becomes DERIVED, and it moves A3's standpoint from taste to arithmetic

The tabled 1.0 m does not survive its own Rule 33 check: **at A3's 60–100 m
standoff, 1.0 m of crown variation is 0.5–0.8 pixels.** The clause that says
«the crown must not read as a smooth arc» was untestable at the distance I chose
to test it.

> **`TOWER_CROWN_LINE_VARIATION_MIN` = d_site / 30**, the same
> `SILHOUETTE_MIN_PX` unit as everything else — **so it is not a new constant,
> it is that one restated, exactly like `TOWER_MINOR_DIM_PER_DISTANCE`.**

And it produces a real bound: a 12–14 m ruin can carry about 4 m of crown
variation before it stops being a tower and becomes a stump, so **its crown
reads to ~120 m while its mass reads to ~240 m.** Two distances, and the smaller
binds the frame that tests the crown.

> **A3's 60–100 m standoff is CONFIRMED, and now for a reason instead of by
> feel.** It was right; it had no derivation until this paragraph.

**Siting stays under §1.3/§1.3a (no density), and B4 keeps its attachment to
B2**: on rock, never on graded soil, with a talus skirt of B1 boulders that
satisfies B1's source rule by construction.

---

#### 10.14.5 B8 and B9 — released with **no new numbers for B8 at all**

**B8 (spans and bridges).** Frame 04 at ~80 m is a dark bar with two bright
apertures, and that is the entire recognition. The rule stands as written and
its constant is `ARCH_OPENING_PER_DISTANCE` = 1/30, which §10.7 already records
as `SILHOUETTE_MIN_PX` restated. **So B8 asks for nothing.** The prop at 15–35°
is already in §10.3.2's table. Siting belongs to §3 and to core's corridor pass.

**B9 (windmill).** `SiteKind::Hamlet` exists in the generator, so this brief has
a consumer today.

| constant | proposed | why |
|---|---|---|
| `WINDMILL_SAIL_SPAN` | **8–12 m** | reads to 240–360 m, the best silhouette a hamlet can buy for its cost |
| `WINDMILL_SAIL_CROSS_ANGLE` | **45 ± 15°** from vertical | the whole point of the object: four diagonals where everything else built throws verticals |

**One observation from the frame that changes the asset and not the numbers:**
frame 02's sails are an **open lattice**, not solid vanes. The read is four
lines, so the cross must be built as frames rather than as quads — a solid sail
at 300 m is a blob and loses the diagonals the class exists for.

---

#### 10.14.6 What is deliberately NOT requested

**B7's `SNAG_LEAN_*` and `SNAG_LEAN_AZIMUTH_SPREAD` are withheld.** §5.9's snag
constants are marked **НЕ ПОСТРОЕНО with no consumer**, and a lean angle for an
object nobody places is a number that will drift silently until the day someone
builds it. B7's content is unchanged and stays in §10.5; **it enters NUMBERS.md
on the day the snag has a consumer, not before.** Same call for
`ROCK_STRATUM_*` (§10.13.5).

---

#### 10.14.7 NUMBERS REQUESTED (Rule 35 — via lead)

Rows superseding §10.7 are marked; **§10.7's superseded values must be replaced,
not shadowed** (Rule 39).

| constant | proposed | unit | second zone |
|---|---|---|---|
| `FENCE_POST_HEIGHT_MIN` / `_MAX` | **1.1 / 1.6** | m | core — *supersedes 0.9 / 1.5* |
| `FENCE_POST_SPACING_MIN` / `_MAX` | **1.5 / 2.0** | m | core — *supersedes 1.8 / 3.0* |
| `FENCE_RUN_LENGTH_MIN` / `_MAX` | **20 / 80** | m | core — *supersedes 15 / 80* |
| `FENCE_RAIL_HEIGHT_FRAC` | **0.75** | fraction of post height, **from the base** | core |
| `FENCE_GAP_FRAC_KEPT_MIN` / `_MAX` | **0.05 / 0.15** | fraction | core — *supersedes the single 0.10 / 0.30* |
| `FENCE_GAP_FRAC_DERELICT_MIN` / `_MAX` | **0.30 / 0.55** | fraction | core |
| `FENCE_POST_LEAN_KEPT_MIN` / `_MAX` | **1 / 6** | ° | core — *supersedes the single 3 / 15* |
| `FENCE_POST_LEAN_DERELICT_MIN` / `_MAX` | **6 / 25** | ° | core |
| `FENCE_POST_YAW_MAX` | 10 | ° about the run | core |
| `FENCE_RAIL_DEVIATION_MIN` | **0.10** | m RMS, generator-side, p05 | core (computes) + render (frame confirms) |
| `KERB_HEIGHT_MIN` / `_MAX` | 0.15 / 0.30 | m | core |
| `STEP_RISE_MIN` / `_MAX` | 0.15 / 0.20 | m | **core + movement** (`PLAYER_STEP_HEIGHT`) |
| `STEP_TREAD_MIN` / `_MAX` | 0.30 / 0.45 | m | core |
| `RETAINING_WALL_HEIGHT_MIN` / `_MAX` | 0.8 / 2.5 | m | core |
| `RETAINING_WALL_BATTER_MIN` / `_MAX` | 3 / 8 | ° | core |
| `KERB_STRAIGHT_RUN_MAX` | 12 | m | core |
| `TOWER_DRUM_DIAMETER_MIN` / `_MAX` | **4.5 / 7.0** | m | core |
| `TOWER_DRUM_HEIGHT_MIN` / `_MAX` | **10 / 16** | m | core |
| `TOWER_DRUMS_PER_CLUSTER_MIN` / `_MAX` | **2 / 3** | drums, conjoined | core |
| `TOWER_CLUSTER_PLAN_MIN` | **8.0** | m, contiguous | core |
| `TOWER_CROWN_NOTCH_COUNT_MIN` | 3 | notches | core |
| `TOWER_CROWN_NOTCH_DEPTH_MIN` | 0.5 | m (**walk-past rule**) | core |
| `MASONRY_BLOCK_YAW_MAX` | 8 | ° | core |
| `MASONRY_COURSE_OFFSET_MAX` | 0.15 | m | core |
| `WINDMILL_SAIL_SPAN_MIN` / `_MAX` | 8 / 12 | m | core |
| `WINDMILL_SAIL_CROSS_ANGLE` | 45 ± 15 | ° from vertical | core |

**Withdrawn before approval, and each is a Rule 39 shadow copy:**

| withdrawn | because |
|---|---|
| `BUILT_EDGE_LEVEL_CHANGE_MIN` = 0.4 m | it is `PLAYER_STEP_HEIGHT` = 0.35 m, rounded |
| `TOWER_CROWN_LINE_VARIATION_MIN` = 1.0 m | it is `SILHOUETTE_MIN_PX` restated: **d_site / 30** |
| `FENCE_ROAD_OFFSET_MIN` / `_MAX` = 2 / 5 m | derived from `CORRIDOR_WIDTH`, not tabled (§10.14.2) |
| `FENCE_RAIL_SAG` (never tabled, and now never will be) | 0.05 m of sag is under a pixel past ~10 m at `INTERNAL_RES`. The readable thing is the **kink at every post**, which a per-bay straight rail gives for free |
| `SNAG_LEAN_*`, `SNAG_LEAN_AZIMUTH_SPREAD` | **no consumer** (§10.14.6) |

#### 10.14.8 ACCEPTANCE — A2 corrected, A7 added (Rule 27)

| # | ref | our standpoint | what would make it FAIL |
|---|---|---|---|
| **A2** *(corrected)* | **15** | on a corridor, looking **along** it, with a fence run in frame **whose posts stand outside `CORRIDOR_WIDTH` from the centreline**, low sun | the fence's **RAIL** line is straight in screen space over the whole run; every post plumb; boulders sitting on the surface rather than emerging |
| **A7** *(new)* | **07 + 14** | standing on a hamlet common, looking across it, low sun raking the floor | any level change ≥ `PLAYER_STEP_HEIGHT` inside the pad is a grass ramp; a kerb runs dead straight for more than 12 m; the built floor meets natural ground with no edge between them; **fewer built edges than the pad's own slope requires** |

**Two things A2 must not be allowed to certify.** First: the top line is not the
instrument (§10.14.2) — a frame read on post tips is reading lean. Second, and
it is the one that would quietly hollow the test out: **a fence sited on the
corridor pad passes A2 by construction**, which is why the standpoint predicate
names the offset rather than leaving it to whoever shoots the frame.

---

