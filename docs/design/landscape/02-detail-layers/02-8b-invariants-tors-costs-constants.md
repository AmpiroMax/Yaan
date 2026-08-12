<!--
Created: 12:08:2026 - 22:57:02
Last updated: 12:08:2026 - 22:57:02
-->
<!--
UPD:
- 12:08:2026 - 22:57:02: Выделен из docs/design/LANDSCAPE.md (9786 строк против FILE_HARD_LIMIT 800): §2.8.3–§2.8.6. Чистый перенос — ни одна строка тела не изменена, ни один номер секции не изменён; адреса вида «LANDSCAPE.md §X» продолжают действовать, таблица § → файл живёт в docs/design/LANDSCAPE.md.
-->

#### 2.8.3 The invariants — nine tests the generator runs on itself

Scope: any landform whose relief ≥ `MASSIF_RULE_MIN_RELIEF` = 40 m. Knolls
(+6 m) and the lakeshore bluff (+10 m) are bumps and are exempt. Border
ranges (§2.6) inherit **I1, I3, I4, I5, I7** on their inner face; **I2 and I8
are massif-only** (a range has no single summit and no closed slice).

| # | Invariant | Test | Current crag |
|---|---|---|---|
| **I1** | **Concave profile** (the core anti-dome test) | mean slope over the **upper third of relief** exceeds mean slope over the **lower third** by ≥ `MASSIF_PROFILE_STEEPENING_MIN` = 12° | **FAILS** — one uniform 45° flank, difference ≈ 0 |
| **I2** | **Sharp summit** | mean slope within `MASSIF_SUMMIT_RADIUS_FRAC` = 0.12 of base radius of the summit ≥ `MASSIF_SUMMIT_SLOPE_MIN` = 40° | **FAILS** — smoothstep gives slope → 0 at the apex |
| **I3** | **Near-vertical rock exists** | ≥ `MASSIF_STEEP_FRACTION_MIN` = 0.12 of the surface above the cliffline exceeds `MASSIF_CLIFF_SLOPE_MIN` = 55° | **FAILS** — measured 0.0 % |
| **I4** | **No constant gradient** (the direct «перепады не постоянные» test) | above the cliffline, **no single 10° slope bin holds more than** `MASSIF_SLOPE_BIN_MAX` = 0.30 of the surface | **FAILS** — 40–50° holds 33.2 % of the *whole* crag, far more above the cliffline |
| **I5** | **Riser/bench alternation** | on each of `MASSIF_RADIAL_SAMPLES` = 64 radials, between ⅓ and full relief, ≥ `MASSIF_BAND_ALTERNATION_MIN` = 3 transitions between cliff class (≥ 55°) and bench class (≤ `MASSIF_BENCH_SLOPE_MAX` = 25°); required on ≥ `MASSIF_RADIAL_PASS_FRACTION` = 0.7 of radials | **FAILS** — no cliff class exists, so zero transitions |
| **I6** | **Band spacing is irregular** | CV of the **vertical spacing between successive cliff bands** along a radial ≥ `MASSIF_BAND_SPACING_CV_MIN` = 0.35. **Measured on band spacing, never on raw contour spacing** (see §2.8.1 — the dome scores 0.935 on the latter) | n/a — no bands |
| **I7** | **RE-SPECIFIED — see §2.8.8. I7 now measures RIDGE DESCENT DEPTH, not ridge count**, because a floor of 3 against a generator input of 3 was checking the detector rather than the mountain. Text below is superseded. ~~Arêtes exist, are sharp, and persist~~ | on contours at 0.4/0.55/0.7/0.85 relief, an arête is a point where surface **aspect turns ≥ `MASSIF_ARETE_TURN_MIN` = 50° within `MASSIF_ARETE_TURN_ARC_MAX` = 15 m of arc**, flanked on both sides by **facets turning ≤ `MASSIF_FACET_TURN_MAX` = 15° over ≥ `MASSIF_FACET_ARC_FRAC_MIN` = 0.08 of that contour's perimeter**. Require ≥ `MASSIF_ARETE_COUNT_MIN` = 3, each detected on ≥ `MASSIF_ARETE_PERSISTENCE_MIN` = 0.6 of the four levels within `MASSIF_ARETE_BEARING_TOL` = 15° of bearing | **FAILS** — no angular structure at all |
| **I8** | **Lobed AND increasingly articulated** | lobe ratio `P²/(4π·A)` at ½, ⅔ and ⅚ relief **each** ≥ `MASSIF_LOBE_RATIO` = 1.35, **and** `lobe(⅚) − lobe(½)` ≥ `MASSIF_LOBE_RISE_MIN` = 0.15 | **FAILS** — 0.80/0.81/0.80, flat as well as low |
| **I9** | **Blocky rock present** | placed rock assemblies cover `ROCK_OUTCROP_COVERAGE_MIN…MAX` = 0.10–0.20 of the surface above the rockline, **and the summit carries a tor** (§2.8.4) | **FAILS** — no such asset class exists |

**Three measurement rules, or the invariants measure the digitiser instead of
the world.**

- **Perimeter comes from the marching-squares contour polyline**, never from
  counting boundary cells. **Measured, not estimated:** boundary-cell
  perimeter came back short by **≈ 26 %** on our own crag (the ≈ 10 % first
  written here was itself an understatement — see the re-stamp box in §2.8.1),
  which moved the same terrain's lobe ratio from 0.80 to **1.27** against a
  1.35 threshold. A threshold that a digitisation choice can flip is not a
  threshold.
- **Slope is measured on the extracted mesh normals as well as on the field**,
  and both are reported. This session's whole diagnosis turned on that pair
  disagreeing; keep the ability to ask the question.
- **"Of the surface" means TRUE SURFACE AREA, never plan-view footprint
  (ruled — it decides I3's verdict).** The two measures answer different
  questions, and only one of them is the question I3 and I4 ask. Footprint is
  the **map projection** — what a bird, or a contour sheet, sees. Surface area
  is what the mountain **presents to a player standing on the valley floor**,
  which is the standpoint this entire section was written from: the user is
  looking at Ravenscar from below, side-on, and a near-vertical face fills
  that view while contributing almost nothing to a map.
  - **The decisive argument is that footprint weighting is anti-correlated
    with I3's own goal at the limit.** A 70° face carries ≈ 2.9× the surface
    of the ground beneath it; a 90° face carries **zero** footprint. So under
    footprint weighting the score of "near-vertical rock exists" falls toward
    zero *exactly as the rock becomes perfectly vertical*. A test that reports
    its own ideal as absence is not a test, and no threshold can repair it.
  - **The same weighting is right for I4, for a second and independent
    reason.** Surface weighting inflates steep bins by 1/cos, so the one
    failure mode I4 exists to catch — a single uniform ≈ 45° flank — is
    weighted **up** against the flat ground around it. Footprint does the
    opposite: it discounts that flank by 0.71 while counting flat benches at
    full weight, so the histogram fills with valley floor and the uniform
    flank hides inside it. An invariant must be **strictest** against the
    thing it was written to reject.
  - **Consistency is the third reason.** I3 and I4 use the same phrase over
    the same population in adjacent rows of one table. Two meanings for one
    phrase in one table is a trap laid for whoever reads this next.
  - **How to compute it, without a new constant.** The **binding** reading is
    **triangle area on the extracted mesh**: finite by construction, needs no
    clamp, and it is literally the surface the player looks at. The field-side
    reading (cell footprint ÷ cos slope) is **reported alongside** as the
    cross-check — it diverges at vertical and would need a clamp, i.e. a
    constant, which is precisely why it is the check and not the verdict. The
    two disagreeing is **information**: §2.8.1's whole diagnosis came out of a
    mesh/field pair disagreeing.
  - **Where footprint legitimately still rules: I8.** A horizontal slice's
    outline is a plan-view object *by definition* — `P` and `A` there are the
    perimeter and area of a 2D curve, not of a surface. Nothing about I8's
    measure changes (only its baseline did, §2.8.1). **I9's coverage
    denominator inherits the surface rule**, which makes I9 slightly harder on
    a steep massif; that is the correct direction, because a mountain with
    more cliff on it needs **more** rock, not the same rock spread thinner.
  - **THIS IS A SECTION-LEVEL CONVENTION, NOT A GLOSS ON ONE PHRASE (my
    error, corrected on core's second asking).** I first ruled this against
    the words "of the surface", which appear in I3 and I4 and nowhere else —
    so when the tor landed, core correctly refused to extend it by analogy to
    **I2**, whose row says only "mean slope". They were right to ask rather
    than assume, and the ambiguity was mine for stating a convention as a
    footnote to two rows. Restated properly: **every slope, area and coverage
    statistic in §2.8.3 is surface-area weighted.** I2, I3, I4 and I9 all take
    it. **I8 is the sole plan-view measure**, because a horizontal slice's
    outline is a 2D curve rather than a surface. No third reading exists.
  - **I2 in particular, because the argument is stronger there than for I3.**
    A tor is flat tops and near-vertical sides — the *pure* case of the limit
    argument above — so under plan weighting the flat tops carry all the
    footprint, the vertical sides carry almost none, and **a textbook tor
    scores as FLAT**. Core measured exactly that: adding the tor moved the
    footprint reading 32.7 → 32.5, i.e. it registered the ideal as a slight
    regression. But the decisive reason is not the analogy, it is that a
    plan-weighted I2 **would reject the landform §2.8.4 mandates** — the
    document would be testing for the absence of the thing it requires. When a
    test and a design ruling contradict, the ruling says what the mountain
    *is* and the test only says whether we got there; the test yields.
    **I2 therefore PASSES at 52.9°** against `MASSIF_SUMMIT_SLOPE_MIN` = 40°.
    The test is not thereby made vacuous: a smoothstep dome has slope → 0 at
    the apex, where cos ≈ 1 and the two weightings agree, so it still fails.
  - **Reporting doctrine, standing:** print **both** readings and label which
    one is the verdict. Same discipline as C2's raw unexempted count (§1.1)
    and the mesh/field slope pair above — an interpretation is stated in the
    open, and interpretations get audited. Core printing both rather than
    resolving the ambiguity in their own favour is the behaviour to keep; this
    ruling supplies the verdict, it does not delete the second column.

- **EVERY INVARIANT SHIPS WITH A CONTROL: THE SHAPE IT EXISTS TO REJECT MUST
  FAIL IT (core's practice, ruled into the document — and it is the strongest
  process rule of this whole stage).** Core ran the I11 detector against a
  smooth analytic cone before trusting it, "because I have now been burned
  twice by trusting a detector", and **the cone scored exactly 3 against a
  floor of 3.** This is the *third* time this section has produced a test its
  own reject-case passes: raw contour-spacing CV scored 0.935 on the dome,
  footprint-weighted I3 scores its ideal at zero, and now I11. Three
  instances is not bad luck, it is a missing step. **A new invariant is not
  believed until the dome fails it.** The control is cheap — an analytic cone,
  a uniform cone, a pancake — and it is the only thing that distinguishes "my
  test passes" from "my test discriminates".
- **A VIEW-SPACE TEST IS EVALUATED AT THE RESOLUTION OF THE EYE IT STANDS IN
  FOR (core's rule, adopted verbatim in intent).** Their first I11 returned
  17–31 breaks whose count **did not fall as the threshold rose** — the
  signature of sampling jitter rather than structure, and a diagnostic worth
  keeping on its own. The window is the readability scale: a readable feature
  is `distance/30` metres, so its **angular** size is 1/30 rad at every
  distance, and structure finer than that cannot be something the player sees.
  This generalises past I11 to every future camera-side check.

**Verdict on the open reading, stated so it is not left implicit: I3 PASSES at
16.5 %** against `MASSIF_STEEP_FRACTION_MIN` = 0.12. The 6.0 % footprint
reading is retained as a **diagnostic** — the ratio between the two readings is
a free measure of how much of the massif is steep — and is never a verdict.

**I7 IS RETRACTED AS EVER HAVING PASSED, AND ITS SAMPLING RANGE IS CORRECTED
(core, stage-4).** Every "4 persistent arêtes" reported to this document is
withdrawn, including the row in §7.1: the probe counted qualifying bearing
*samples* rather than ridges, and anchored its persistence scan on the lowest
slice. Corrected, **I7 fails on all 12 seeds** (max 2 against a floor of 3),
and core validated the detector against a couloir-free faceted polygon whose
corners exist by construction before believing the failure. **The lesson is
the sharpest one this stage: 42 arêtes was absurd on its face and was caught
instantly; 4 was not absurd and survived three rounds of rulings.** A wrong
number in the plausible range buys itself unlimited time — this is the third
instance this session, after the C1 self-occlusion figure and my own lobing
mechanism, and all three were *directionally reasonable*.

**Then the failure turned out to be partly mine: I7 was sampling where the
model never promised arêtes.** Raw detections rise with height — 2/3/5/5 at
0.4/0.55/0.7/0.85 — because §2.8.2's `ε` increases with elevation, which is
the mechanism that earns I8's rise clause. So **the mechanism satisfying I8's
rise is the mechanism preventing I7's persistence**, and two of I7's four
levels sit in the smooth apron below `MASSIF_CLIFFLINE_FRAC` that §2.8.2
explicitly describes as unbanded. **RULING: I7's persistence is measured over
four levels spanning the BANDED ZONE only — from the cliffline to the summit,
never the apron.** This is not a relaxation invented to make a red test green:
`ε` increasing with elevation was written before the conflict appeared, and
ribs dying into a talus apron is what real massifs do — it is the mirror of
the couloir-fade ruling above, which nobody objected to.

> **The guard that makes this legitimate rather than a loophole: I11.** A test
> whose sampling elevations I am free to choose is a test I can always make
> pass. I11 (§2.8.7) is measured from a camera against the sky and **cannot be
> gamed by choosing slice elevations at all.** This relaxation of I7 is
> therefore conditional on I11 existing — if I11 is not implemented, I7 keeps
> its original levels and stays red, because a proxy may only be loosened once
> the thing it was proxying for is being measured directly.

**I5 is measured on radials that carry no validated route (core's catch,
ruled).** A route breaches the cliff bands it crosses (§2.8.5), which locally
destroys the alternation I5 counts on that radial — so counting it there would
make **the ascent cause its own invariant to fail**, which is the §6.2
pad-scorer mistake in new clothes (judging a feature by a metric its own
purpose contradicts). Count alternation on non-route radials; assert the
existence of breaches **separately**, as their own check.

**Consequence of I3 that must be made loud, not discovered.** Cliff risers at
≥ 55° exceed `PLAYER_MAX_SLOPE` (~50°), so a compliant massif is genuinely
unclimbable off-route. That is the intent — it is what makes the breach
legible — but it means the crag's summit route becomes **the only** way up,
and a route-validation failure leaves the summit *unreachable* rather than
awkward, with act 1's climax attached to it. Route validation failure on a
banded massif is a **hard seed failure**, reported loudly, never a warning.

**I7 is the arête test and it is worth stating why it is shaped this way.**
"Ribs" is not "bumpy in plan". A rib is **flat faces meeting along a line**.
Aspect (the compass direction of downhill) is *constant across a face* and
*flips fast at a crest*, so the distribution of aspect-turn-per-arc is what
separates a ridged mountain from a lumpy one — and it is scale-free, which is
why the same test governs a 115 m crag and a 280 m massif. Persistence across
four elevations is what stops a single noise lump from scoring as a rib.

**I1 and I4 are the two that would have caught this a session earlier**, and
neither is expensive. If only two are implemented first, implement those.

**Fourth rule, added when the invariants started passing: A MARGINAL PASS ON
ONE SEED IS NOT COMPLIANCE.** I8 first passed at **1.36 against 1.35, with the
rise at exactly 0.15 against 0.15** — zero headroom on the clause §2.8.1
identifies as the load-bearing one. A shape parameterisation that lands *on*
its bound on seed 1 will land under it on roughly half of every other seed,
and every massif in the world (LR, border inner faces, future valley L0s) is
generated from the same rules with a different seed. So:

- **Invariants are reported as a distribution across seeds, not a verdict on
  seed 1.** Generate the massif under a handful of seeds and report min /
  median / max per invariant. This is cheap — headless generation plus the
  measurement code that already exists — and it is the only way to tell a
  parameterisation that is *right* from one that is *lucky*.
- **When the median sits at the bound, the SHAPE PARAMETERS move, not the
  threshold.** Widening a threshold to admit a marginal shape is the
  accommodation this document has refused twice already (§1.3's unspent
  physics-correction budget is the precedent).
- **Reason this is a design rule and not core's implementation detail:** it is
  the same failure class as §7.0a's — a coordinate stamped against one terrain
  state, mistaken for a property of the world. An invariant validated on one
  seed is a stamp against one terrain state.

#### 2.8.4 «Кубы на кубах» — the tor ruling, and what voxels can honestly do

**Say the achievability plainly, because the answer is not the one that was
assumed.** Surface nets rounds edges — that is recorded in
VOXEL_ARCHITECTURE.md §2 as a known cut of the crunch variant. It was
reasonable to suspect the mesher of the smoothness. **It is not guilty:**
measured field max slope 68.7°, and the mesh's 40–50° bin sits *above* the
field's. At `VOXEL_SIZE` 1.0 m the rounding radius is ≈ 1 m, which against an
8–15 m cliff band is a 7–12 % softening of the lip — a *weathered* cliff top,
which is what we want anyway.

So the brief splits cleanly by feature scale:

| Scale | Feature | Mechanism | Cost |
|---|---|---|---|
| ≥ 8 m | arêtes, couloirs, cliff bands, sharp summit, non-uniform contour spacing | **terrain SDF, per-sample math** | **free** — no new pass, no new storage, no mesher change |
| 3–8 m | slab benches, stepped shoulders | terrain SDF, ≈ 1 m lip rounding accepted | **free**, reads correctly |
| < 3 m | **«кубы на кубах»** — stacked slabs with crisp arrises | **placed rock meshes** | new asset class + placement pass + collision |

**RULING: the blocky read is PLACED MESHES, not dual contouring.** Four
reasons, and the first is decisive:

1. **Dual contouring would not deliver it.** DC sharpens edges the SDF already
   contains; it does not raise the sampling rate. At 1 m voxels a block under
   ≈ 3 m does not survive Nyquist at all, so DC buys us *crisper 3–5 m
   masses*, never "cubes". The thing the user asked for is below our voxel
   floor by construction.
2. **DC is a mesher change that touches every surface in the game** (core's
   words), with non-manifold and determinism care, for a benefit confined to
   one landform class.
3. **Placed rock is reusable everywhere else** — scarp faces (§2.7), outcrops
   (§2.2), the barrow lintel and standing stones (§6.2), the castle's spoil
   heap and never-laid dressed stone (§6.1.3), quarry cuts. It is not a
   one-mountain investment.
4. **It is the reference answer.** Bethesda puts placed rock meshes over
   heightfield terrain for exactly this problem, and the user has been told as
   much. Instanced, LODs trivially, arbitrarily crisp arrises.

**The tor rule — the single highest-value item in this whole ruling.**
**A massif's summit is a TOR, not a terrain vertex.** The top
`SUMMIT_TOR_HEIGHT` = 6–12 m of Ravenscar is a **stack of tilted slabs** over
a footprint of `SUMMIT_TOR_RADIUS` = 5–10 m, with the watchtower ruin (§6,
§7.1) standing **on** it rather than on smoothed ground. This is a real
landform (a granite tor), it is literally "кубы на кубах", and it converts the
one part of the mountain the eye always lands on from a rounded crown into a
broken rock crest. Ravenscar's silhouette against sky stops being an arc.

The LR's summit carries the temple; there the tor becomes the **plinth** the
temple stands on — same rule, same geometry, the building sits on rock.

**Built as a HEIGHT STAMP, and two rules came out of building it (core,
stage-4 — both found by measuring, both general).** The tor is terrain SDF,
not the placed-mesh class: the scale table above puts ≥ 3 m features in the
height function for free, and these slabs are metres thick over a 5–10 m
footprint. Slab count derives from the ≈ 3 m Nyquist floor implied by
`VOXEL_SIZE`, so it borrows nothing from the placed-rock constants.

- **"The tor REPLACES the top" must not be implemented as "TRUNCATE the
  top".** Capping the cone at the tor base and returning a flat platform
  outside the slabs builds a **mesa**, and truncation is precisely the
  silhouette I2 exists to reject: the summit-slope measure got **3.6° worse**
  when the feature meant to fix it was added. The cone is capped only *inside*
  the stack and left alone outside. Recorded because "replace the top" is a
  natural sentence with a wrong obvious implementation.
- **`L0_RELIEF` IS A CONTRACT — anything stamped on a summit is measured INTO
  the landmark's relief, never added ON TOP of it.** Core's tor initially
  overshot (its base was derived from `SUMMIT_TOR_HEIGHT_MIN` while its slab
  heights were drawn across `_MIN…_MAX` — reading one bound of a range as if
  it were the range, the same shape as the bugs in §2.8.2) and the peak drifted
  115.0 → 116.1. That is not a cosmetic 1 m: `CASTLE_SKYLINE_MARGIN`, R3, R4,
  C1 and the whole landmark hierarchy are **ratios and margins to the peak**,
  so a summit feature that quietly raises the peak edits every one of them
  from a zone that does not own them. Peak now measures exactly 115.0.

**Assembly grammar (generator rules, not hand placement):**

- A **stack** is `ROCK_STACK_BLOCKS` = 2–5 blocks, each
  `ROCK_BLOCK_SIZE` = 1.5–4.0 m, flat-topped, near-vertical sided, hard
  arrises, ≤ `ROCK_BLOCK_TRI_BUDGET_MAX` = 60 tris per block.
- Blocks are **offset laterally by up to `ROCK_STACK_OFFSET_MAX` = 0.8 m** and
  **tilted by up to `ROCK_STACK_TILT_MAX` = 0.21 rad (12°)**. A perfectly
  level, perfectly aligned stack reads as **masonry**, and the moment it does
  the player thinks *ruin*, not *mountain*. The offset is what makes the
  silhouette stepped; the tilt is what makes it geological.
- **Placement is derived from the terrain, never tabled** (§7.1a rule,
  extended once more): stacks sit on **arête crests** at
  `ROCK_STACK_SPACING` = 15–35 m of crest length, at **cliff-band lips and
  bases**, and on the summit. A crest that resolves, on approach, into stacked
  blocks is the payoff shot of this entire section.
- **Never on a bench a route crosses**, never inside a corridor mask, never
  where they would block a validated ascent (§2.8.5).
- **Budget:** `MASSIF_ROCK_TRI_BUDGET_MAX` = 60 000 tris for a whole massif at
  LOD0 — ≈ 1.5× one chunk of today's heightfield mesh. **Instancing and LOD
  are mandatory, not optional**: at 0.15 coverage Ravenscar wants ≈ 270 stacks,
  which is fine as instances and unaffordable as unique meshes. **Render has
  accepted these numbers as-is and the machinery already exists** — stacks
  bake into the same per-chunk world-space merged buffers the trees use, so
  "instanced" here means one buffer per chunk, not per-instance draws. No new
  batching work, and the coverage number does not need to move.

**The shadow-caster floor — a constraint I did not know, and it bounds all
future rock detail (render, measured).** At our shadow-map resolution
**anything under ≈ 0.31 m across casts no shadow at all**, and an unlit block
sitting on shadowed ground reads as pasted-on geometry rather than as rock.
Consequences, ruled:

- Our 1.5–4.0 m blocks clear it comfortably; nothing changes today.
- **The crisp read must come from the block's SILHOUETTE, never from arris
  detail.** A chamfer or a stepped edge under 0.31 m contributes no shading
  information at 640×360 and costs triangles for nothing — which is the same
  conclusion §6.1.3 reached about masonry coursing and §1.5 reached about
  battlement teeth, now with a measured number attached. Spend the 60-triangle
  budget on the block's *outline*, not on its corners.
- `ROCK_STACK_OFFSET_MAX` (0.8 m) is comfortably above the floor, which is
  part of why the stepped silhouette works: the offset is what casts.
- **This figure is NOT a NUMBERS.md constant.** It is derived from shadow-map
  resolution, which is a render setting like `INTERNAL_RES` (sync №3) — it
  moves when the setting moves. It is recorded here as a *published render
  figure that design rules against*, and any future prop class smaller than
  the current rock blocks must re-ask render for the number rather than cite
  0.31 m from this line.

**Dual contouring is explicitly NOT required by this ruling** and stays where
core deferred it. Its real customers are the castle terrace, quarry cuts and
cave mouths (VOXEL_ARCHITECTURE §2). Revisit it there, on their evidence, not
on the mountain's.

#### 2.8.5 What this costs the rules we already have

A landmark's shape is load-bearing for a dozen placements (§7.0a's durable
rule: *changing a landmark's relief invalidates every placement on its
slopes*). Applying that rule to my own change:

- **The validated ascents survive by being BREACHES, and that is an
  improvement, not a concession.** Ravenscar's summit route (§7.1), the LR's
  Steps (§2.5) and the castle scramble (§6.1.3, `SCRAMBLE_SLOPE` 30–45°) must
  each cross the cliff bands. Rule: **cliff bands are broken where a validated
  route crosses them**, breach width ≤ `MASSIF_ROUTE_BREACH_WIDTH` = 12 m,
  generated as part of the route stamp and reading as a gully or a gate. The
  gain is real: when everything else is cliff, **the one way up becomes
  legible from the valley floor**. The mountain teaches its own route. A
  breach is a feature of the shape language, not an exception to it.
- **Castle R1–R4 (§6.1.1) must be re-run, and I predict they get easier.**
  The base radius is unchanged (180 m), so the crag's angular footprint at the
  horizon — what R1 tests — does not move. `p > 1` makes the *mid-body*
  slimmer, so the crag occludes itself less and R2 (flank yes, crown no) gains
  headroom. **Predicted, not assumed:** re-validate. The failure mode to watch
  is the opposite of the one that looks obvious — a slimmer upper body could
  narrow the *upper* footprint enough to push a tall element outside R1.
- **§7.0a's barrow couloir search should now succeed.** Core's search for a
  low-terrain couloir in the 180°–240° arc failed against a stamp that has no
  couloirs — angular lobing creates them by construction, and core flagged
  this connection unprompted. **Re-run the couloir search after the reshape
  before touching the high-shoulder fallback**, which stays out of scope.
- **C1 / C4 (§1.3):** less self-occlusion should raise clearance; cliff bands
  add local flank occluders where the pine strips already sit. Re-measure;
  the floor does not move (§1.3's standing rule).
- **Flora — trees move onto the benches** (§5.10 already has the machinery).
  `TREE_SLOPE_MAX` (0.61 rad) excludes cliff faces automatically, so
  vegetation collects on benches, and the **cliff-edge setback measured from
  the outer edge of the root flare** now applies at *every band lip*, not just
  at scarps. Four rulings, three of them flora's and better than my drafts:
  - **My "one cluster per bench segment" is WITHDRAWN — right intent, wrong
    unit.** A count does not survive scale: one stand on a 200 m bench reads
    as a potted plant exactly as badly as a continuous line reads as
    landscaping, and both are the same failure — *the vegetation does not
    respond to the mountain*. Replaced by a duty cycle:
    `BENCH_VEG_DUTY_MAX` = 0.25 of a bench's running length,
    `BENCH_CLUSTER_LENGTH_MAX` = 25 m, `BENCH_CLUSTER_GAP_MIN` = 40 m, and
    **`BENCH_BARE_FRACTION_MIN` = 0.40 — at least 40 % of benches carry
    nothing at all.** That last one is the load-bearing one: what makes a
    stand on a ledge above a 12 m drop extraordinary is that the ledges above
    and below it are **bare**. A mountain where every bench has its one
    dutiful cluster is still landscaped, merely at lower density.
  - **Placement is biased to the LIP, not the riser base**
    (`BENCH_VEG_LIP_BIAS` = outer 0.40 of the legal band). Flora's addition
    and it matters more than the cap: a tree at the inner edge has a cliff
    face directly behind it — dark on dark, no silhouette, invisible at any
    range. The same tree near the lip has **sky** behind it and its crown
    overhangs the drop. This is §1.5's skyline rule applied at band scale, it
    costs nothing, and it is the entire reason to vegetate benches.
  - **6 m benches are wide enough; no terrain floor changes.** My worry
    double-counted the setback: only the **outer** lip is a drop. The inner
    side is the *base* of the riser going up — not a fall hazard, needing only
    the flare's own radius so the trunk is not embedded in rock, and a tree at
    a cliff base is a good thing. Legal axis band = `W − r_flare − (1.5 +
    r_flare)`; measured from the shipped meshes a pine (`r_flare` 0.84 m) gets
    2.81 m of lateral freedom on a 6 m bench. Vegetation floors:
    `BENCH_VEG_WIDTH_MIN` = 5.0 m (below it, bushes and grass only) and
    `BENCH_VEG_WIDTH_MIN_GIANT` = 7.0 m — **both below the 6 m terrain
    minimum**, so the two systems do not fight.
  - **The treeline SNAPS TO THE NEAREST BAND LIP.** A flat elevation cutting
    across banded terrain lands mid-riser and half-vegetates cliff faces,
    which reads as a **mowing line** rather than a limit. Snapped, the tree
    limit follows a geological feature — which is what real treelines do on
    banded rock and is far easier to look at. Same reasoning for the rockline.
    Cost is core's call; it should be small, since the lips are already known.
- **Render — RESOLVED, and the rule came back better than I asked for.** I
  requested a narrow exception for a hard splat edge at band lips; render
  reframed it as the general rule — *dither where the geometry is smooth, snap
  where the geometry has an edge* — which is not a carve-out from §4 but §4
  applied to a surface with creases, and it generalises free to quarry faces,
  cut terraces and cave mouths. Now written into §4 itself. Mechanism is a
  screen-space slope derivative: a couple of ALU instructions, no new data, no
  constant from design, threshold set by looking at a frame.
- **The splat coupling this section depends on is STRUCTURALLY protected, not
  merely agreed** (render, worth recording because it is the difference
  between a promise and a guarantee). Their zone carries a standing ruling
  from the "brown wash" incident: render must never re-derive material bands
  from raw height or distance fields — material comes from core's
  `surface_class`, and the shader only *augments* rock by slope between the
  two §4 thresholds. So the mechanism §2.8.2 relies on is the only one
  available to them, and decoupling slope from material on steep ground would
  require deliberately reintroducing a banned bug class. **The contour rhythm
  is safe by construction rather than by anyone remembering.**
- **Sim — cliff faces at 55° exceed `PLAYER_MAX_SLOPE` (50°)** and are
  therefore natural barriers, which is what we want. Watch the character
  controller against the ≈ 1 m rounded band lip.
- **Voxel pipeline — nothing to do.** All of §2.8.2 is height-function work,
  and per RIVER_RESEARCH §0.3 any change expressible as a height modification
  survives the voxel pipeline unchanged. Only the placed rock of §2.8.4 is new
  geometry, and it is ordinary instanced meshes with collision.

#### 2.8.6 Constants (for NUMBERS.md, Rule 14)

Renames first, because one of them is the lesson: **`LR_LOBE_RATIO` →
`MASSIF_LOBE_RATIO`**, **`LR_CLIFF_BAND_MIN/MAX` →
`MASSIF_CLIFF_BAND_MIN/MAX`**, **`LR_CLIFFLINE` → `MASSIF_CLIFFLINE_FRAC`**.
Nothing evaluates them today, so the rename is free; scoping a shape rule to
one landmark is exactly the failure that produced this session.
`LR_RIDGE_COUNT_MIN/MAX` (4–7) stays as the LR's generator input and is now
understood as its **arête count**; Ravenscar gets `L0_ARETE_COUNT` = 3–5.

**RULING (stage-4): Ravenscar's arête count is FOUR, and `L0_ARETE_COUNT` = 3–5
is retired as a range.** Core measured all three values across 12 seeds:

| Arêtes | I8 level | I8 rise | Reading |
|---|---|---|---|
| 3 | fails 3 seeds | fails 2 seeds | and the count *equals* the invariant floor |
| **4** | fails 1 seed | **fails none** | the only workable value |
| 5 | **fails all twelve** | — | the convex cap, measured |

- **Five is not a choice, it is arithmetically excluded.** `n·tan(π/n)/π` =
  1.16 for a pentagon against a 1.35 threshold, and couloirs cannot rescue it.
  The algebra in §2.8.2 predicted this and the measurement confirmed it, which
  is the first time that cap has been tested rather than derived. **So the
  authored range 3–5 contained a value that can never pass**, and a per-seed
  draw across it — which my own character-not-existence rule would otherwise
  invite — would ship guaranteed-failing worlds. The range narrows.
- **Three fails for a structural reason that is not about shape at all: THE
  GENERATOR INPUT AND THE INVARIANT FLOOR WERE THE SAME NUMBER.** The massif
  had exactly 3 corners and I7 requires ≥ 3 detected, so a single missed
  detection fails by construction. **General rule: a generator input must
  never equal the floor of the invariant that checks it** — that is not a
  margin, it is a coincidence, and every measurement error lands on the
  failing side of it.
- **Sixth instance of the range family.** `arete_count` was pinned at
  `L0_ARETE_COUNT_MIN` — one bound of a range read as the range, exactly as
  the tor derived its base from `SUMMIT_TOR_HEIGHT_MIN`. See §5's "a range is
  two assertions".

All values **предложение — утвердить**. Full table with units and
justifications is handed to the lead with this ruling.

