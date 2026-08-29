
## 7. Testbed application (worldgen v2, что core реализует первым)

Canvas: 4×4 chunks, world XZ = 0…1024 m both axes, seed 1, current surface
16–26 m. All coordinates below are *generator parameters* (a testbed layout
table in the worldgen tool's data), not hand sculpting — each is the center
of a procedural stamp/scorer, tunable and deterministic. **Все координаты и
высоты — предложение — утвердить.**

### 7.1 The plan (feature list, in pass order)

| Feature | Where (x, z) | Parameters |
|---|---|---|
| **L0: Ravenscar Crag** + watchtower ruin | peak (830, 200), footprint r ≈ 180 m | **banded contour massif per §2.8** (replaces the smoothstep radial stamp — the shape the user rejected three times); `L0_RELIEF` **115 m**; `L0_ARETE_COUNT` 3–5; cliff bands above `MASSIF_CLIFFLINE_FRAC`; **summit is a tor** (§2.8.4) carrying the tower ruin (§6); rock splat above the stamp's rockline; **validated summit route, breaching the bands — see below** |

**Ravenscar is the acceptance case for §2.8.** The nine invariants are not
abstract quality gates: the crag **fails all nine today**, measured, and the
user is looking at it. Acceptance is the tour frames from the valley floor and
from the western meadows showing a summit that is a broken rock crest rather
than an arc, visible horizontal stripe rhythm on the flanks, and at least
three crest lines readable at 640×360. If a frame still reads as a dome, the
invariant that let it through is the wrong invariant and gets fixed — the
frames outrank the numbers, because the numbers exist to predict the frames.

**Status — step 1 of §2.8 (the banded massif) is implemented, and five of the
eight currently applicable invariants pass** (core, stage-4; I9 is not
applicable until the placed-rock asset class exists):

| Invariant | Original crag | Step 1 (bands) | Step 2 (facets + couloirs) | Bound |
|---|---|---|---|---|
| I1 concave profile | −7.1° | +15.0° | **12.7°** | ≥ 12° |
| I2 sharp summit | fails | fails | **52.9°** surface / 32.5° footprint | ≥ 40° |
| I3 near-vertical rock | 0.0 % | 16.5 % | **27.4 %** surface / 13.1 % footprint | ≥ 12 % |
| I4 no constant gradient | (footprint, superseded) | 24.2 % | **18.5 %** fullest bin | ≤ 30 % |
| I5 riser/bench alternation | 0 | 100 % | **100 %** of radials | ≥ 70 % |
| I6 band-spacing CV | n/a | 0.518 | **0.464** | ≥ 0.35 |
| I7 arêtes | fails | fails | **4 persistent** | ≥ 3 |
| **I8** lobing | 1.27 (re-stamped) | 1.01, flat | 1.36 / 1.36 / 1.51 — levels pass, **rise 0.14 FAILS** | ≥ 1.35 each **and** rise ≥ 0.15 |

> ### ⚠ THIS TABLE IS NOT THE STATE OF THE MOUNTAIN — the frame is
>
> **Seven of eight pass and the mountain still reads as a dome**, confirmed
> independently by render, the lead and me from
> `screenshots/massif/02_massif_verdict_400m_diagnostic.png`: a smooth convex
> arc, **zero** crest lines against the three this section requires, one
> material band instead of a rhythm. Per the standing clause below, the frame
> governs. **Do not relay this table as progress without §2.8.7 attached** —
> the suite measures the object and never the view, which is how every row can
> be green while the acceptance criterion fails. I10 (massif aspect) and I11
> (silhouette breaks) are the answer; until they run, the honest status of
> Ravenscar is **"still a dome, better instrumented".**
>
> **Superseded by a 12-seed distribution, and SEED 1 WAS ONE OF OUR BEST
> WORLDS rather than a typical one — 8 of 8 on seed 1 is 5 of 8 on seed 2.**
> Every number this stage was measured on our luckiest draw. **I7 is retracted
> entirely and never passed on any seed** (§2.8.3).
>
> | Invariant | min | median | max | bound | failing seeds |
> |---|---|---|---|---|---|
> | I1 concave | 5.44° | 19.41° | 22.45° | ≥ 12° | 2 of 12 |
> | I2 summit | 53.0° | 56.4° | 60.3° | ≥ 40° | none |
> | I3 steep | 25.2 % | 44.5 % | 54.8 % | ≥ 12 % | none |
> | I4 fullest bin | 17.4 % | 21.6 % | 29.3 % | ≤ 30 % | none |
> | I5 radials | 93.8 % | 100 % | 100 % | ≥ 70 % | none |
> | I6 band CV | 0.22 | 0.48 | 0.80 | ≥ 0.35 | 1 of 12 |
> | **I7 arêtes** | 0 | 1 | 2 | ≥ 3 | **all twelve** |
> | I8 level | 1.24 | 1.42 | 1.99 | ≥ 1.35 | 3 of 12 |
> | I8 rise | 0.07 | 0.34 | 0.85 | ≥ 0.15 | 2 of 12 |
>
> > **AND THE MOUNTAIN IS 16 % SHORTER THAN THE CONSTANT THE USER APPROVED
> (core, stage-4).** `L0_RELIEF` = 115 m is documented in NUMBERS.md as
> «перепад» — relief above the foot — and **the code uses it as an absolute
> peak elevation.** With the valley floor at 18.8 m the peak sits at 115.0
> absolute, so the true relief is **96.2 m**. Consequences, and the first is
> the one that matters upward: the castle-dominance 0.285 and C1 0.903 that
> justified raising `L0_RELIEF` to 115 were measured on *this* build, so they
> stand as measurements — **but the user approved 115 m of relief and the world
> has been showing them 96.** Every rejection of this mountain has been a
> rejection of a shorter mountain than the one that was signed off. It also
> inflated the aspect failure, since relief is the numerator: fixing the
> meaning moves aspect 0.507 → 0.606 for free and lifts the required base
> radius from ≈ 99 m to ≈ 137 m. **The bug was costing us 20 m of footprint.**
> Seventh instance this stage of a constant's *meaning* being read wrong rather
> than its value — the range family's close cousin, and the reason NUMBERS.md
> prose is a contract and not a comment.

**I2, I3, I4 and I5 are robust on every seed and I am calling those
> genuinely done.** They are also — not coincidentally — the four that describe
> *local surface character* rather than *global form*: the suite is strong
> exactly where the frame agreed with it and weak exactly where it did not.

**Three cautions attached to this table, so it is not read as "done".**

1. **I8 fails by 0.01 on the clause §2.8.1 identifies as load-bearing**, and
   core stopped rather than close it against seed 1 — which is §2.8.3's
   marginal-pass rule working one turn after it was written. The fix ruled is
   a change of *unit* (§2.8.2: absolute couloir depth), not a nudge.
2. **The whole table is still one seed.** Per §2.8.3 nothing here counts as
   compliance until the min/median/max distribution exists. I1 at 12.7° against
   a 12° floor is the next-thinnest margin after I8 and would be the first to
   go on an unlucky seed.
3. **I2 and I3 print the field-side reading**; §2.8.3 makes mesh triangle area
   binding. Neither verdict turns on it (52.9 vs 40, 27.4 vs 12), but the
   binding reading is the one that belongs here once wired.

### 7.1b Acceptance vantages — the two frames (design, binding on the tour)

§2.8's acceptance was written as "tour frames from the valley floor and from
the western meadows", which names two standpoints that test **the same
thing**. Corrected on render's challenge, and their far/near split is the
better structure: the dome failure and the constant-gradient failure are
**invisible to each other's distance**. Two frames, one clause each, stamped
here so the acceptance test is reproducible across sessions.

| | **Frame 1 — the verdict frame** | **Frame 2 — the rhythm frame** |
|---|---|---|
| Eye | (120, 300), standing | (545, 165), standing |
| Aim | (830, 200) at y = 95 m | (830, 200) at y = 70 m |
| Range | ~~≈ 717 m~~ → **360 m, DERIVED (§1.6.1)** | ≈ 287 m |
| Light | low morning sun, **backlit** | low evening sun, **front-lit and raking** |
| Proves | the massif reads as a **sharp, ribbed, concave, lobed mass against sky** — not a dome — at the range the valley actually looks at it | the flank **alternates cliff and bench at irregular spacing** («перепады не должны быть постоянными»), with planar risers and hard splat lips |

- **Why frame 1 is backlit and frame 2 is not.** Each frame's light is chosen
  to make *its own* failure mode visible. Frame 1 tests an outline, and §1.5's
  doctrine is that landmarks read by **value against sky** — a dark mass
  against a bright sky is the purest possible form of the user's own
  complaint, which was a silhouette word. Frame 2 tests a surface, and a low
  sun behind the camera strikes near-vertical risers close to head-on while
  grazing the horizontal benches, which maximises the riser/bench value
  separation the band rhythm is made of. The same light would ruin the other
  frame: morning sun puts the whole west flank in shadow and no band reads.
- **Frame 1 does not test the bands, deliberately.** A riser+bench pair
  (≈ 28 m) subtends ≈ 11 px at 717 m and ≈ 27 px at 287 m. Bands survive to
  ≈ 960 m at 640×360 before dropping under `SILHOUETTE_MIN_PX`, so frame 1
  *could* carry them — but one frame, one clause.
- **No foot-of-cliff frame, and the reason is the invariant.** From directly
  beneath a riser you see one riser; I4/I5/I6 are about **rhythm**, which needs
  several bands stacked in one view. 287 m is the near end that still shows a
  rhythm.
- **Frame 2's bearing avoids the castle sector** (the castle and barrow sit
  ≈ 208° from the peak; frame 2 looks in from ≈ 280°). Inside 300 m §6.1.1
  explicitly *allows* the castle to fill the view, so a frame taken through it
  would be testing the castle rather than the mountain.
- **Ravenscar must read SOLID, never hazy.** At 287–717 m it is far inside
  `LANDMARK_HAZE_ONSET` = 800 m, and §1.3a's depth separation requires the
  valley L0 always inside the onset and the LR always beyond it. Haze on
  Ravenscar in frame 1 is a bug, not atmosphere.
- **The hard splat edge at band lips is INTENDED — do not smooth it** (§4,
  §2.8.2's planar risers). The thing to check is not whether the edge is hard
  but whether it is hard **in the right place**: the snap must track the
  geological lip, not wander across a bench along a slope-threshold contour. A
  hard edge in the wrong place is worse than a soft one.
- **These frames are shot with the palette post off**, per the user's standing
  instruction, so they are not a test of §1.5's shipped value separation.

**Three corrections after the first shoot, two of them mine.**

1. **FRAME 1 IS UNSHOOTABLE AND THAT IS THE BIGGEST FINDING OF THE SHOOT — see
   §1.3a.** At 717 m the massif's chunks are not resident and the mountain is
   simply **absent** from the frame. Not a design problem and not a camera
   problem; the world stops existing at ≈ 512 m. Frame 1 waits for LOD, and
   render has parked the vantage unchanged behind `DFN_MASSIF_PROBE` so the
   re-shoot is one command. **The 400 m shot is a DIAGNOSTIC, never the
   acceptance frame** — render labelled it so, correctly.
2. **ACCEPTANCE VANTAGES ARE DERIVED, NEVER TABLED (my error, and it is the
   third instance of this exact trap).** I tabled (545, 165) by geometry —
   right distance, bearing clear of the castle — **without checking it against
   the generated forest**, and a dense pine stand owns the frame. §7.1a's rule
   already covers this and I broke it: *any tabled coordinate that must sit on
   or near generated features is a trap.* A camera position aimed through a
   generated forest is exactly such a coordinate. **Rule: an acceptance
   vantage is derived as the nearest standpoint on the required bearing and
   range that C1 already credits with seeing the L0.** A vantage the
   composition rules do not protect can fail for reasons that have nothing to
   do with the subject — and `LANDMARK_VISIBILITY_MIN` = 0.6 explicitly allows
   40 % of the ground not to see the landmark, so picking a point blind draws
   from that 40 % two times in five.
3. **The hour on frame 2 was wrong and the sun geometry is render's to
   state.** I reasoned that a low western sun would front-light an
   east-looking camera; the frame came back backlit under a dusk sky. My
   reasoning was sound and my premise about the sun's azimuth was not, so the
   fix is not to argue it: **render publishes the sun azimuth as a function of
   `DFN_TIME`, it is recorded here, and every future frame request picks its
   hour from that table** rather than from anyone's mental model of a sunset.
   The requirement is unchanged and is stated in geometry instead of clock
   time: **frame 2 needs the sun roughly perpendicular to the view axis and
   low**, so risers and benches separate by value.

The acceptance test does not move and does not become easier for the table
being green: it is still the tour frames from the valley floor and the western
meadows, and **the frames outrank the numbers**, because the numbers exist to
predict the frames. Seven of eight invariants and a summit that still reads as
a dome would mean the invariants are wrong, not that the mountain is right.

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

**UNBUILT IS BINDING, NOT DECORATIVE — escalate before laying a single step
(story's condition on clearing the I10 reshape, and it survives them in
ACT1_VALLEY §2).** If steepening ever makes the ≤ 25° natural line hard to
find, **the answer is a longer traverse or more switchback, never masonry.**
The reason is not fussiness about materials: a built stair up Ravenscar
collapses §2.5's TWO DIFFERENT CLIMBS distinction, both mountains become
stairs, and **act 2's Seven Thousand Steps stop being singular.** The whole
point of specifying this route as a worn watchmen's line was that no frame
could confuse the two. A graded ramp is masonry by another name here; the
castle's approach ramp (§6.1.2) is a *pad* feature and is unaffected.

**And the banded model already supplies the ascent's structure, which is why
this condition is cheap rather than a constraint we have to fight.** On a
massif whose upper cone reaches the rock threshold, a ≤ 25° line **cannot go
straight up** — 115 m of climb needs ≈ 247 m of horizontal run, more than a
contracted radius provides on any radial. So the route must wrap. That is
exactly what §2.8.2 already builds: **the benches ARE the traverses and the
band breaches (§2.8.5) ARE the risers between them.** A path zig-zagging bench
to bench through breaches is a worn line by construction, it is what real
mountain paths do on banded rock, and it makes the ascent *more* legible from
the valley rather than less. The steeper mountain does not threaten the route;
it supplies it.

**One measurement to watch as the route wraps further:** I5 counts alternation
on non-route radials (§2.8.3), so a longer wrapping route shrinks that sample.
The pass *fraction* is unaffected — the denominator is the non-route set — but
if the route consumes most bearings there is too little left to measure
honestly. Report the non-route radial count alongside I5.
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

**Status after the §2.8 reshape, and a SEQUENCING RULING (stage-4).** The
crag-tunnel and Backbarrow carve tests went **red** on the reshape and core
reported them rather than papering over them — correct, and worth saying
plainly: those two tests going red **is the durable rule above working, not
breaking.** Reshaping the massif invalidated the placements on its slopes,
exactly as stated, and the tests are the mechanism that says so out loud.

**Ruling: re-run the couloir search AFTER I7/I8, not now.** §2.8.5 said to
re-run it "after the reshape", on the reasoning that angular lobing creates
couloirs by construction. That reasoning is sound but its precondition is not
met yet: the reshape's measured lobe ratio is **1.01 against the smoothstep
stamp's true 1.27** (§2.8.1), i.e. the current massif has **fewer and
shallower re-entrants than the shape the search already failed on.** Searching
now would fail for the same reason it failed the first time, and — this is why
it matters — a second failure would wrongly promote the **high-shoulder
fallback**, which inverts a line of story's canon. Do not spend that
inversion on a measurement taken at the wrong moment. The barrow stays where
it is, with its test red, until arête/lobe work lands.

**The override was vindicated harder than I expected (core, same session).**
Their first faceted variant made couloir *presence* a seeded per-facet draw,
and **on seed 1 all three facets missed** — a bare convex polygon with zero
couloirs, on a massif that looked thoroughly reshaped. Re-running the search at
that moment would have failed a third time for exactly the predicted reason,
against a shape whose appearance argued it should have succeeded, and the
third failure is what would have spent the high-shoulder inversion. **The
precondition is now met rather than assumed:** couloirs exist on the flanks by
construction (§2.8.2's no-coin-flip rule came out of this near miss), so the
search may run with the next step. Also recorded: **the crag-tunnel carve test
went GREEN on its own** when the couloirs and deeper facet insets opened the
flank the switchback exits through — a placement that was invalidated by the
reshape and then repaired by it, which is the §7.0a dependency working in both
directions. Only the barrow mouth is still red, where it was told to stay.

**Fallback if no couloir clears:** core's (c) — a **high entrance on the
shoulder**, mouth 20–44 m above the valley, castle unmoved. It keeps
everything geometric but **inverts one line of story's canon**: the grave then
stands over the seat rather than under it. That inversion is arguably stronger
(the Corvanes cannot escape being overlooked by what they did) but it is
story's sentence, not mine — pre-cleared with them rather than assumed.
Options (a) and (b) remain last resorts.

**FINAL RULING ON THE RED TEST (stage-5): register EXPECTED-FAIL, comment
naming this section — with a TRIGGER for expiry, because a red without an
owner and an expected-fail without an expiry are the same lie at different
volumes.** Core's latest measurement supersedes the «couloirs exist by
construction» note above (Rule 34: the owner of the massif code was asked and
answered): the current massif is a **self-similar cone with no couloirs at
any height**, so re-running the §7.0a search now would fail a third time for
the already-predicted reason. And the high-shoulder fallback is **measured to
break story's not-visible-from-Vaelmere constraint** — so it is no longer
merely story's sentence to pass, it is currently illegal. That leaves no
green placement to site today, and a permanently red test trains people to
ignore red — which is worse than the debt it advertises. Therefore:

- The barrow-mouth carve case is registered **expected-fail**, with a comment
  naming §7.0a. The suite goes green. Re-siting becomes a stage task.
- **The expiry is a trigger, not a date:** when arête/couloir work lands (the
  §2.8.2 absolute-couloir-depth unit fix that I8's 0.14-vs-0.15 failure
  already demands), the couloir search runs **as part of that change** —
  §7.0a's window: bearings 180°–240°, radius 90–110 m, terrain ≤ ≈ 28 m,
  nearest to 209° — and the registration flips. An expected-fail that
  unexpectedly PASSES is precisely the mechanism that announces the couloir
  now exists; that is why expected-fail is chosen over deletion.
- Owner of the trigger: design (this document). Core owns only the
  registration and the comment.

**The half-buried cutting on the tunnel's lower legs (core's stage-5
report) — ruled: PARKED ON THE SAME TRIGGER, with its acceptance named
now.** The §2.8 reshape left survey legs 1→3 with their corridor top proud
of the terrain by ~1–2 m for ~50 m (cover −1.0 to −2.2 m where the old
massif buried them): geometrically walkable, visually a trench scar on the
flank. Deliberately NOT patched today: those legs sit on exactly the flank
the §2.8.2 couloir/arête work will move again, and this section's durable
rule already makes re-validation of slope placements part of that change —
burying them now is spending the work twice on terrain that is about to be
wrong. What they must meet when the trigger fires: **every tunnel leg is
either BURIED (cover ≥ `TUNNEL_COVER_MIN`, 1 m proposed — предложение —
утвердить) or an AUTHORED OPEN CUTTING — a deliberate sunken-road stretch
with visible revetment that reads as built, not as eroded. The accidental
in-between — a bare corridor top poking through the grass — is the rejected
case.** Whether the lower approach goes back underground or becomes an
honest cutting is decided then, from the reshaped terrain — not now, from
terrain that will not survive the change.

**The control, corrected (core's challenge, upheld — my first wording named
«core's frame» as the control when NO FRAME EXISTED: the finding was
measured, not shot, and none of the seven tour vantages contains the flank
readably. That is the Rule 27 trap this document itself defines — naming
evidence that cannot show the defect — caught by core in me.** The control
is two halves, both reproducible from the repo:

1. *Quantitative:* the measured cover table — legs 1→3 at **−1.0…−2.2 m**
   over ~50 m are the must-fail against `TUNNEL_COVER_MIN` = 1 m, and legs
   3→7 at **+1.6…+18 m** are the passing neighbor: both Rule 30 cases from
   the same instrument.
2. *Visual:* a vantage RECIPE, never a file path — `screenshots/` is
   gitignored, so pixels die with a clean clone and only the recipe is
   durable: binary `build_render/dfn_app`, seed 1, `DFN_MASSIF_PROBE=1`,
   `DFN_MASSIF_EYE="660,300"`, `DFN_TIME=0.72` (front-lit SW flank). At
   that vantage the cutting reads as a faint diagonal seam on the lower
   right flank at ~130–160 m — verified by design against the produced
   frame. **Subtle at valley range is expected and is why this vantage is
   the CONTROL, not the acceptance: the acceptance that fires with the
   trigger needs one closer authored vantage that CAN fail loudly (F7),
   spec'd by design at that time.** Durable pixel archiving, if wanted, is
   a lead call (tracked frames dir or artifact store); the recipe carries
   the control either way.
Core's two real fixes in the same area (derived daylight portals; the 6 m
switchback clearance) are accepted as reported — both are the §7.0a
dependency rule working, and neither waits on the trigger.

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

