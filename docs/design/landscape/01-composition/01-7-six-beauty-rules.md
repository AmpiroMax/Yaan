<!--
Created: 12:08:2026 - 22:57:02
Last updated: 12:08:2026 - 22:57:02
-->
<!--
UPD:
- 12:08:2026 - 22:57:02: Выделен из docs/design/LANDSCAPE.md (9786 строк против FILE_HARD_LIMIT 800): §1.7. Чистый перенос — ни одна строка тела не изменена, ни один номер секции не изменён; адреса вида «LANDSCAPE.md §X» продолжают действовать, таблица § → файл живёт в docs/design/LANDSCAPE.md.
-->

### 1.7 The six beauty rules — acceptance conditions (user-ratified в19/в20, stage-5)

Six rules from LIVING_WORLD_RESEARCH.md, ratified whole by the user, entering
the bible as **acceptance conditions** — each stated testably, each with the
case it must reject and a case that can pass (Rule 30/30a), and **where a real
rejected instance exists, IT is the control and the threshold sits above it.**
They gate the stand maps (§8) before they gate anything else. Every constant
named here is a **requested NUMBERS row** (Rule 35: core generates against
these thresholds and design accepts against them — two zones, one number),
flagged **(предложение — утвердить)** until the lead lands it.

Labels BR-1…BR-6 map to грилл в19's (а)–(е) in order.

**BR-1 (а) — a path's curve hides its destination at least once.**
Hide-and-reveal (miegakure / BotW sightline occlusion): the bend is a
mechanism of curiosity, not an ornament.
- **Test:** at the path's 4 m stations, cast an eye-height (1.7 m) ray to the
  destination goal; PASS requires ≥ 1 contiguous occluded run
  ≥ `HIDE_REVEAL_MIN_RUN_M` (10 m proposed — ≈ 3 s of walking at
  `WALK_SPEED` 3.0 and ≥ 3 stations; shorter concealment reads as flicker,
  not as a reveal held back).
- **Must-fail control:** a straight path across the preserved plain (§2.7)
  between two mutually visible goals — zero occluded stations by
  construction.
- **Can-pass (30a):** a path bending around a single 2–5 m grive (§2.2); the
  smoothing the trace already gets produces this wherever meso-relief exists.
- **Scoping:** binds paths crossing the hill landform (§2.10 LF-2). On the
  deliberately preserved plain the rule may be waived **per path, in
  writing** — the waiver is authored, like the plain itself (в9).

**BR-2 (б) — a path connects real goals by a near-shortest route, or it
reads as painted.** Desire lines: a path is a record of repetition, and the
generator must fake the repetition honestly.
- **Test, two clauses:** (i) both endpoints are registered goals (POI or find
  — no path to nowhere); (ii) path length ≤ `DETOUR_MAX` (1.4 proposed) ×
  the cost-optimal route length, cost = distance weighted by slope and water
  penalties — the same cost field the generator routes with.
- **The Rule 30a trap, named:** the generator IS a cost search, so clause
  (ii) alone can never fail its raw output — the teeth are the endpoint
  clause and the bending passes: BR-1's hide-and-reveal detours and BR-2's
  ceiling FIGHT, and 1.4 is where the fight is settled. The ceiling must sit
  above the measured overhead of trace+smoothing+BR-1 bending (expected
  ~1.1–1.2 — measure it, Rule 30) and below the painted case.
- **Must-fail control:** an ornament path — one that ignores the cost field
  (a hand-drawn "scenic" S at ratio ≈ 2×) or ends nowhere.
- **Can-pass:** the desire-line trace with BR-1 bending applied.

**BR-3 (в) — the rich edge: moss, flowers, mushrooms live on the path
margin, not scattered uniformly.** The margin is the best real estate in the
frame — half-shade, moisture, nobody treads it (research A6).
- **Test:** decoration density as a function of `dist_to_path`: (i) on the
  trodden center ≈ 0; (ii) margin band (edge → 2 m out) ≥ `RICH_EDGE_RATIO`
  (3× proposed) × the density at 10–20 m; (iii) monotone decreasing beyond
  the margin peak. Band datum: **0 = the outer edge of the worn surface,
  measured outward** (never the centreline) — flora's naming, adopted; the
  trodden surface then sits at negative datum and clause (i) holds by
  construction.
- **SCOPED BY MAINTENANCE, not applied flat to all four path types
  (flora's finding, ruled stage-5).** A rich margin is what grows where
  **nobody sweeps**; cobble through a settlement is swept, and a generator
  that gardens the gutters of a town street has made maintenance invisible.
  So the margin profile is authored per path class, and the fiction — who
  tends this ground — is the reason:
  | Path class | Margin | Why |
  |---|---|---|
  | Cobbled/paved (settlement) | **suppressed**, `RICH_EDGE_RATIO` does NOT apply; a *kept* verge instead | swept by people who live there |
  | Dirt road | moderate — ratio applies at reduced strength | used hard, tended never |
  | Hint-path (тропинка-намёк) | **maximum** — the BR-3 specimen class | nature reclaiming the edge |
  | Stone steps | **moss in the shaded joints**, no flowers | damp stone, trodden treads |
  **The rule, so this is not four invented multipliers: ONE threshold and
  an ORDERING.** `RICH_EDGE_RATIO` keeps its single value and is measured
  on the **hint-path** — the specimen class, the case the rule was written
  for. The other three are held to their ORDER against it, not to numbers
  of their own: `hint ≥ dirt > cobble`, with cobble at ≈ 1 (no margin peak
  at all) and the dirt road required only to show a peak, not to reach 3×.
  An ordering is what the fiction actually claims — *less tended means more
  overgrown* — and it needs no constant per class to be asserted, which is
  the whole point: four rows would be four things to tune, one ordering is
  a property. Steps are judged on their own clause (moss present in joints,
  flowers absent), not on the ratio.
  Acceptance therefore measures the ratio **on the unmaintained classes**;
  a cobbled street failing it is a PASS, and a test that reds there would
  be measuring the rule's scope rather than the world. Implementation: a
  per-class column on flora's edge table (it keys on habitat only today) —
  requirement, not a schema.
- **THE EDGE-GRADIENT FLOOR IS SCOPED BY THE SAME COLUMN.** flora's §3.12
  mechanism 2 floors the clump field near a path so a coverage gap can
  never bare a margin — that floor is exactly what would garden a cobbled
  gutter, i.e. the machinery installed to GUARANTEE BR-3 is what would
  break this ruling. The floor is zero on the maintained classes. Naming it
  here because it is invisible from the edge table alone.
- **A kept verge is not bare ground — §1.1 does not stop at the town gate.**
  Suppressing the margin must not re-make «земля плоская и мёртвая» inside
  the settlement, which would trade one complaint for the same complaint in
  a better neighbourhood. Maintenance reads by **where life survives a
  broom**, not by absence of life: moss and weeds in the joints, at wall
  bases, in the lee of steps and thresholds — the swept ground between them
  is what makes those pockets legible as spared rather than as leftover.
  Two consequences flora drew out and I confirm: the class weight scales the
  edge PEAK and never the base presence, so cobble at 0 means «ratio ≈ 1, no
  peak» and not «no plants» (asserted at a field ZERO, the case where the
  two readings would otherwise agree); and **moss alone keeps a small
  residual peak on cobble (0.25), the other species go to 0** — moss is the
  broom-survivor, so the fiction predicts it and the damp joint is the
  mechanism. Bounded, because a residual argued from fiction can grow:
  it stays strictly under the dirt weight (the ordering keeps its teeth) and
  it is moss only. **Its acceptance is a FRAME, not the ratio (Rule 27):
  pockets, not a ribbon.** If a cobbled street renders a continuous green
  stripe down both kerbs the residual is wrong regardless of what the test
  says — drop it to 0 and let the ShadeOfStone association carry the joints,
  which is the same fiction keyed to the place instead of the distance.
- **Must-fail control — the real rejected instance:** the current build's
  uniform scatter, the user's «земля плоская и мёртвая» said in numbers:
  uniform scatter measures ratio ≈ 1 and fails clause (ii) under any
  threshold above its noise. The threshold stands above it, as Rule 30
  requires.
- **Can-pass:** any density field keyed off `dist_to_path` with a margin
  peak — the field already needed to draw the path itself.

**BR-3 CLOSED — the ratio is DEMOTED to a floor, the ordering is the gate
(ruling, stage-5, on core's found gap and flora's authored fix).** Core
found the `ForestFloor` rows (MossPatch, Mushroom) carried no density at
all — `per_100m` is linear-per-path and a forest floor is not a linear
habitat, so the far-field side of BR-3's ratio was dividing by an
unauthored zero and returned ≈ 27 000, which is not a measurement (correctly
refused rather than shipped green). Ruled:

- **Densities blessed as proposed** (flora, `docs/specs/flora.md` §3.13,
  design blesses per Rule 25 — the numbers are flora's zone, the acceptance
  shape is mine): MossPatch 40/ha, Mushroom 20/ha, both DERIVED from
  existing anchor counts (stems/ha, log/deadfall counts) rather than picked
  fresh, and MossPatch explicitly excludes fallen logs (they carry moss in
  their own mesh, §5.10 `moss_cover`) so the figure is not double-dressing
  the same moss twice under two different meshes.
- **Denominator: SAME-SET** (the seven edge species measured on both sides
  of the ratio), not all-scatter. The numerator already counts edge
  species only; counting bush/snag/log/deadfall only on the far side would
  answer a different question (is the whole floor denser near the path)
  than the one BR-3 asks (are the EDGE species enriched near the path).
- **`RICH_EDGE_RATIO` (3) is DEMOTED from gate to floor, kept at its
  current value, never promoted back without a real intermediate rejected
  instance to derive against.** Same-set gives ≈ 30×, all-scatter gives
  ≈ 6× — either way the world clears 3 by a wide enough margin that, per
  this document's own Rule 30 language, the ratio at 3 certifies nothing
  among any authoring anyone would plausibly ship: it still correctly
  fails the uniform-scatter control (≈ 1), so it stays as a logged floor
  and a cheap total-collapse tripwire, but it stops being what BR-3's
  acceptance is measured against. **This is the same move already made for
  BR-5 the same day** (bare terrain kept as a permanent must-fail canary
  once it stopped being the gate) — an instrument that can only catch
  total failure, not grade real authoring, is demoted rather than
  discarded or arbitrarily re-tightened. Re-tightening to "where it bites"
  (flora's own estimate, ≈ 12–15×) is declined for now: nothing but the
  realized value itself would justify that number, and setting a threshold
  from the value it is meant to test is the 30a coincidence this document
  has already refused twice.
- **The ordering clause is BR-3's acceptance for clause (ii)'s intent,
  formally, not merely in practice:** `hint-path ≥ dirt > cobble ≈ 1`,
  measured with real separation between the classes, exactly as core
  already asserts it. It is falsifiable in both directions (a class out of
  order fails it; classes correctly ordered but numerically indistinct
  also fails it) and tracks the maintenance fiction directly, which the
  ratio never did.
- **The floor-vs-product composition question is CLOSED by core's mutation
  check, and the requirement is now written down rather than left
  implicit:** the composition must preserve the kept-verge floor via a
  MAX-with, never a plain product — multiplying by cobble's zero
  flower/pebble weights would zero the moss residual (0.25) too and
  rebuild «земля плоская и мёртвая» inside the settlement, exactly what
  the kept-verge ruling forbids. Core confirmed by mutation: swapping the
  shipped `max(...)` for a plain product drives cobble's margin to exactly
  zero and the suite correctly reds — the control exists and discriminates
  (Rule 30), and the floor is now proven load-bearing rather than merely
  argued.
- **NUMBERS.md forwarded to lead:** `RICH_EDGE_RATIO`'s row should record
  the same-set denominator and the floor-not-gate status, so a future
  reader does not re-litigate the ratio's role from the bare number.

**Sixth definitional question in three days, named because the pattern is
now load-bearing on its own:** dispersion denominator, per-class control,
ring aggregation, seed statistic, BR-5's instrument, now BR-3's ratio scope
— five of six were caught only once a measurement landed near a bar. The
forwarded ARCHITECTURE.md clause (name the aggregation and denominator
alongside the number) is doing exactly the job it was written for.

**BR-4 (г) — clumping of grass and flowers is an AUTHORED FIELD, not
randomness.** Tsushima's lesson: кучность is a parameter someone paints,
never a lucky accident of scatter.
- **Test, two claims:** (i) the scatter RESPONDS to the field — two runs
  identical except the clump-field value differ measurably in aggregation,
  measured as **NORMALISED Clark–Evans** `R_norm` ≤ `CLUMP_R_NORM_MAX`
  (**0.85** proposed) where the field says clumped, and `R_norm` within
  0.95–1.05 where it says even; (ii) the field itself passes Rule 31 — its
  distribution over the map is asserted, not just its bounds.
- **THE DENOMINATOR IS PART OF THE THRESHOLD, and my first wording had it
  wrong in BOTH directions (flora's measurement, stage-5).** Raw R is
  defined against a *Poisson* expectation, but R is a property of the
  PLACEMENT, not of the field: run the same machinery with the field held
  CONSTANT and it measures **1.134**, because a jittered lattice is more
  regular than Poisson. So ≈ 0.13 of every raw reading is machinery, not
  authorship. Hence:
  **`R_norm` = R(field on) / R(same placement, field constant)** — and the
  even-field case then lands at **exactly 1.0 by construction**, which is
  what proves the quantity rather than the verdict. My original clause
  demanded «R ≈ 1 where the field says even» on a machine that returns
  1.134 for precisely that case: **the correct pass case failed the test as
  written, which is Rule 30a — a test needs a case that CAN pass it — and
  it condemns the quantity independently of any class's result.** Whoever
  measures this next inherits the denominator with the row; it is never
  the measurer's choice.
- **THE CONTROL IS MEASURED PER CLASS, AT THAT CLASS'S DENSITY — one global
  1.134 is the same defect one level down.** A jittered lattice loses its
  regularity as you thin it: accept every candidate and you measure the
  lattice (R well above 1), accept one in ten and the survivors approach
  Poisson (R → 1). So the machinery's contribution is a function of
  COVERAGE, and our classes span 0.10 to 0.55 — a single control divides
  mushrooms by a number that was never theirs. The constant-field run is
  therefore taken **per class with the constant set to that class's own
  mean**, so numerator and denominator differ in one thing only: the field.
  **MEASURED (flora, same day), and the density dependence is now visible
  rather than argued** — the control climbs monotonically with the
  acceptance rate, 1.052 → 1.136 across the classes:
  | class | field mean | R (field on) | control | `R_norm` |
  |---|---|---|---|---|
  | Mushrooms | 0.087 | 0.383 | 1.052 | **0.364** |
  | Pebbles | 0.121 | 0.466 | 1.065 | **0.437** |
  | Flowers | 0.150 | 0.515 | 1.075 | **0.478** |
  | Moss | 0.163 | 0.529 | 1.080 | **0.490** |
  | GrassTufts | 0.352 | 0.776 | 1.136 | **0.683** (worst seed 0.714) |
  As predicted the correction ran one way and no verdict flipped: the
  over-divided low-coverage classes rose, and grass — at the top of the
  range, where the control is largest — got a bigger denominator and a
  smaller number. **The trap worth keeping:** the single-control table was
  wrong for four classes and right only for grass, by the accident that
  grass's mean sat nearest the one constant used — i.e. the error hid
  behind the very class we were arguing about, and a table can be correct
  exactly where you are looking while wrong everywhere else.
- **Must-fail control — the real rejected instance:** the current grass:
  pure jittered-lattice scatter, `R_norm` = **1.000 by construction**
  (it IS the denominator) and identical under any field value — fails
  claim (i) in both directions. The bar sits 0.15 below it, ≈ 3–4 seed-noise
  widths, and above the worst measured seed of the least-clumped authored
  class (grass, 0.714) — threshold above the passing cases, well clear of
  the rejected one. **0.85 is DERIVED on the new quantity, not translated
  from the old** (the lead's row makes the same point): translating 0.80
  through the control gives 0.705, under which grass's worst seed still
  falls — carrying a number across a change of quantity is the original
  error in a new suit, and the arithmetic would have hidden it as a
  conversion.
- **Can-pass:** two-level scatter (Poisson parents, clustered children)
  with parent density driven by the field. Measured `R_norm` against the
  per-class control, all five classes, no seed breaching: mushrooms 0.364,
  pebbles 0.437, flowers 0.478, moss 0.490, grass 0.683.
- **GRASS IS THE LEAST-CLUMPED CLASS ON PURPOSE, AND IT IS NOT TUNED TO
  PASS.** Coverage 0.55 puts grass on over half the ground, and a pattern
  covering half the ground *cannot* be strongly clumped — that is
  arithmetic, not a defect. Raising its contrast until a raw 0.8 passed
  would buy the number by putting **bare earth between the tufts**, i.e. a
  different meadow, which is a design change disguised as a tuning pass and
  is refused. **Known interaction, recorded rather than solved:** high
  coverage bounds achievable R from above. If a future broad-cover class
  bumps the bar, the answer is to record the coverage-R relationship — never
  to raise contrast until the class complies, and never to exempt broad-cover
  classes from BR-4, which would exempt exactly the class the user's
  complaint is loudest about.

**BR-5 (д) — the middle tier of hills exists to occlude: small finds are
visible only from crests.** BotW's middle triangle: a hill that hides
nothing is spending its budget on nothing — §2.2's meso-relief stops being
texture and becomes режиссура here.
- **Test, per find placed in the hill landform:** (i) from a ring of
  eye-height samples at 40–80 m, the find is occluded from
  ≥ `FIND_OCCLUSION_FRAC` (0.5 proposed) of bearings **— aggregated
  PER-DISTANCE, never pooled across the band (sharpened stage-5, flora's
  find, see the BR-5 note below): the frac must clear at each distance the
  ring is actually sampled at (the discrete rings core already measures,
  e.g. 40 m / 60 m / 80 m), because BR-5 models a walker CROSSING the band,
  and a strong far reading must never be allowed to buy cover for a weak
  near one — the same "a mean can hide a desert" reasoning BR-6 already
  states as a tail clause, applied here to distance instead of gap length**;
  (ii) the 30a clause — from the crest of the nearest grive it IS visible: a
  find nothing can reveal is a lost find, and a test without this clause
  measures the raycaster, not the composition.
- **Must-fail control:** a find on the preserved plain — occluded from ≈ 0
  of bearings. (Finds on the plain are legal; they are simply not BR-5
  specimens — the plain's emptiness is authored, в9.)
- **Can-pass:** a find in a swale between two grives: 2–5 m crests beat a
  1.7 m eye by arithmetic.

**BR-5 SCOPED — bare terrain is the wrong instrument for the FOREST STAND,
and the two carriers of this rule are declared, not accidental (ruling,
stage-5).** Core measured BR-5 on the forest stand at a median far under the 0.5 bar
(cited here as 0.03/0.06; **that pair was withdrawn 10.08.2026 and
re-measured at 0.0000/0.2083 — see the CONTROL paragraph below**)
occlusion (40/80 m rings) against the 0.5 bar, and flagged that LF-2's own
recipe cannot supply it alone: making the swale floor CONNECTED for W5's fog
(§2.10 LF-2, the percolation threshold at 0.593) is the same change that
opens the long sightlines BR-5 needs closed. **Ruling: option 3.** Bare
terrain is not the instrument BR-5 is measured against on this stand.
Grounds, checked rather than argued:

- Confirmed in source (core, not inferred): the current raycast
  (`WorldgenFinds.cpp:47-81`, fed by `Worldgen.cpp:255`) is terrain-only —
  macro relief + LF-8 erosion + path tread. No trunks, no canopy, no floor
  scatter. **0.03 was measured with essentially the whole forest missing**,
  not with the forest present and failing. That settles, by arithmetic, the
  branch core and flora both flagged as open (does the raycast count
  trunks — no): finds are not landing outside the forest mass (the oak rect
  covers the whole stand), so this was never a placement bug.
- §8.1's own purpose clause: *"no massif, no sea, no L0 ... nothing tall
  rescues a boring middle distance here, the meso tier **and the floor**
  must carry the frame alone."* The floor (LF-7) is declared, by this
  stand's own brief, as a co-equal composition carrier alongside the meso
  tier — not dressing added after the gate is decided. Measuring BR-5 with
  the floor deleted deletes exactly what §8.1 promises. This is Rule 36:
  bare terrain excludes the floor by convenience (LF-7 wasn't built yet
  when BR-5 was first measured), never by cause.
- **LF-2's own dictionary-level acceptance is UNCHANGED and stays
  bare-terrain** (cross-reference added at §2.10): that acceptance is for
  LF-2 stamped WITHOUT a forest (river-valley shoulders, big-world hills),
  where there is no floor to include. Scope split: landform-only contexts
  test BR-5 on terrain; the forest stand tests BR-5 on the composed scene.
  One rule, two contexts — not two rules.
- **LF-2's 55 % floor network never needed correcting.** The percolation
  requirement is core's to keep exactly as built; it was only ever in
  tension with BR-5 while BR-5 asked bare terrain to do a job §8.1 assigns
  jointly to terrain and floor. Candidates 1 (shorten hill wavelength to
  ≈ 55 m) and 2 (move the ring to 60–80 m) are declined — both patch the
  terrain side of a problem whose measured cause is that the terrain side
  was never meant to carry it alone on this stand.

**THE NEW GATE, STATED AS AN INSTRUMENT — "include the scatter" is not yet
a test:**

- **Scope:** the forest stand's BR-5 acceptance only (§8.1). Every other
  declared use of LF-2 (§2.10 `Used by`) keeps the bare-terrain instrument.
- **Occluder set — REAL PLACED instances for the seed under test, never a
  mean-density approximation:** (a) the existing terrain heightfield,
  unchanged; (b) tree trunks below `crown_base`, at the oak lattice core
  already generates for this stand (44.4 stems/ha, ≈ 15 m jittered spacing,
  `WorldgenScatter.cpp:288` — no new density, the one already shipping);
  (c) `Bush` and `BigBush` instances from the BR-4-driven floor scatter —
  flora's measurement makes these the two classes that matter (alone at
  60 m: bush 0.725, big bush 0.239, vs fallen-log 0.050 / snag 0.008 /
  deadfall 0.000). `FallenLog`/snag/deadfall MAY be included for
  completeness but the gate must never be made to depend on them — those
  classes are sized for the user's brief («поваленные деревья... кусты...
  сухие мертвые деревья»), never for a validator.
- **Mechanism:** a ray-vs-obstacle march along each ring bearing (same 4 m
  stepping core already uses), occluded when the terrain-chord test trips
  (unchanged) OR the segment intersects the disc/footprint of any real
  placed trunk, Bush, or BigBush instance. **This is NOT the C1/C4 canopy
  Beer–Lambert transmittance model** (§1) — that instrument is built for a
  different band (crown occlusion of a *distant* landmark) and would return
  0 blocked here, since an eye at 1.7 m and a find at ≈ 0.5 m both sit under
  `crown_base`. The right shape is the stem-level ray-vs-disc test flora
  already built for the floor classes — reuse it (Rule 32); this stand does
  not get a second, ad hoc occlusion model.
- **The bar itself does not move:** `FIND_OCCLUSION_FRAC` stays 0.5, the
  ring stays 40–80 m, eye height stays 1.7 m. Only the occluder set changes.
  Answering core's question directly, for the NUMBERS.md row (sent to
  lead): **the 0.5 is a bar on terrain+trunks+floor**, not on bare terrain
  and not on terrain+trunks alone.

**CONTROL (Rule 30) — the control stands, its NUMBER is withdrawn and
replaced (core, 10.08.2026, `c8e6a73`).** The terrain-only case is the
must-fail control permanently: it is the literal "forest with the forest
deleted", and it must keep reading far under 0.5. But the pair this
section cited for it — **0.03 / 0.06 at 40/80 m — does not reproduce and
is withdrawn.** Core re-measured the same terrain-only arm at
**0.0000 / 0.2083** (seed 1, 156 finds, 24 bearings), 3.5× the cited
figure at 80 m, and eliminated three candidate causes before reporting:
not their ray (pooled the old way it returns 0.1042, matching the
generator's own `Find::occluded_fraction` median to four decimals), not
bearing count (converged by 24; 8/16/24/48 never approach 0.06), and not
LF-8 erosion (disabling it raises the 80 m control to 0.2500, so erosion
slightly *reduces* terrain occlusion here — the opposite of the guess).

**The cited pair is refuted by arithmetic alone, with no re-run needed,
and design should have caught this without core spending a measurement.**
For two equal-sized groups, at least half of each lies at or below its own
median, so the pooled median can never exceed the larger of the two group
medians. Per-ring medians of 0.03 and 0.06 therefore force a pooled median
**≤ 0.06** — yet the generator itself recorded **0.1042** on the same
finds. **0.03 / 0.06 and 0.1042 cannot both describe one draw.** The
citation was inherited into this section as a per-distance pair without
anyone checking it against the pooled figure sitting beside it, and this
is Rule 34 landing on design: a number quoted from another zone is a
premise, and a premise gets checked before a ruling is built on it. It is
also the aggregation defect ARCHITECTURE.md's Rule 30 already records
using **this very rule** as its example — "a ring of samples at 40–80 m"
passes read as one ring and fails read per-distance. The number was
recorded on the wrong side of the ambiguity the rule itself was cited to
illustrate.

**What survives unharmed, and why this is a reconciliation and not an
alarm:** the control's JOB is to fail the bar, and 0.2083 fails it by
2.4×. Nothing in the BR-5 ruling rested on the control's magnitude — only
on its being far under 0.5, which it is. The rest of §1.7 stands.

**THE PINNED REGRESSION TEST STAYS, RECLASSIFIED — AND ITS FIRST CLAUSE
NOW HAS THE WRONG QUANTITY (amended 10.08.2026).** The "3–4×" clause is a
RATIO, and core's re-measurement shows its denominator can be **exactly
zero**: the terrain-only control reads 0.0000 at 40 m. A ratio against
zero is undefined, so at the near ring no threshold on that quantity
separates a working siting logic from a broken one — which is Rule 30's
own test for a wrong quantity rather than a wrong threshold. **Restate the
first clause as a DIFFERENCE, not a ratio:** composed minus terrain-only,
at each ring, aggregated per-distance and never pooled. On core's numbers
that reads 0.4167 at 40 m, 0.5833 at 60 m, 0.5000 at 80 m — all three
comfortably positive, all three defined, and the 40 m case (the one the
ratio cannot express at all) is where the siting logic does its *largest*
work. The below-0.5 tripwire clause is untouched and keeps its exact
wording.

Core's existing test — find siting beats a bare-ground/naive control, AND
the median stays below 0.5 on bare terrain — keeps BOTH assertions, the
first with the amended quantity above. It stops being read as the BR-5
acceptance gate (that role moves
to the terrain+trunks+floor instrument above) and becomes the **permanent
canary for this ruling's premise**: the 3–4× clause proves the siting logic
does real directional work even on an instrument too thin to reach the bar;
the below-0.5 clause is the tripwire — if bare terrain alone ever starts
clearing 0.5, the premise "landform cannot do this job alone on this stand"
has silently stopped being true, and per the standing instruction that must
FAIL the canary and force a rewrite, never a quiet pass.

**THE BR-4/BR-5 TENSION FLORA MEASURED IS REAL AND IS NOT CLOSED BY THE
INSTRUMENT CHANGE ALONE — ruled separately, same commit.** Flora measured
that BR-4's authored clumping costs 0.09–0.26 of occlusion at equal mean
Bush density against a naive even-scatter estimate, and that the ruled
density band's MIN end (0.339 at 40 m, 0.458 at 60 m) sits under the bar
even before trunks are added — the ruled band spans pass and fail (Rule
30's "a range is two assertions," its seventh appearance this stage). Two
levers are refused outright, flagged independently by flora and by the
lead: **do not retune BR-4's clump field to pass this gate** — the same
trap the grass class was already protected from in BR-4's own ruling, tuning
a meadow to a raycast; and **do not size dead wood up to compensate** — it
is the wrong class (flora measured it near-zero at 60 m) and it is a class
the user asked for by name, sized for the brief, never for a validator. The
lever that IS available, because it is already precedented in this rule and
retunes no authored field: **find placement becomes density-aware.** BR-5's
own can-pass clause already carves out "finds on the plain are legal,
simply not BR-5 specimens" (в9's authored emptiness); the same shape
extends here — a candidate find location whose local terrain+trunk+floor
instrument cannot plausibly clear 0.5 (checked at placement time, the same
instrument, no new one) remains a legal find location but is not claimed as
a BR-5 specimen there. This is a placement-generator lever, not a density
lever, and it is core's to build once the instrument above exists and is
re-measured — not sized today, because per Rule 34 nobody has yet measured
the real number the new instrument produces, only the terrain-only control (0.0000/0.2083 as re-measured) and
flora's trunk-less scatter estimate.

**Sequencing, for core:** (1) build the terrain+trunks+Bush/BigBush
ray-vs-disc instrument, reusing flora's floor-class shape; (2) re-measure
BR-5 on it, keeping the terrain-only arm as the permanent control at its
re-measured value; (3) only if the re-measured MIN end still fails, apply the
placement-density-awareness lever above — not before, since the numbers in
hand cannot say whether it is even needed once trunks (a real, uniform,
non-clumped 44/ha contribution no earlier estimate included) are in the sum.

**THE COMBINED FIGURE, MEASURED — flora closed the gap above rather than
leave it as a guess (§3.14, stage-5).** Trunks (the shipped 44.4/ha lattice,
UNIFORM — BR-4's clump field does not touch trees) plus the clumped floor
classes, one model:

| band | dist | trunks | floor | combined | vs 0.50 |
|---|---|---|---|---|---|
| ruled MIN | 40 m | 0.214 | 0.307 | **0.479** | FAIL by 0.021 |
| ruled MIN | 60 m | 0.300 | 0.501 | 0.662 | pass |
| ruled MIN | 80 m | 0.389 | 0.613 | 0.767 | pass |
| ruled MAX | 40 m | 0.214 | 0.575 | 0.649 | pass |
| ruled MAX | 60 m | 0.300 | 0.747 | 0.818 | pass |
| ruled MAX | 80 m | 0.389 | 0.881 | 0.924 | pass |

The trunk term alone measures 0.300 at 60 m against the 0.27 predicted
analytically earlier in this ruling — close, and wrong in the direction a
jittered lattice being more regular than the Poisson assumption predicts,
which is corroboration rather than coincidence and means the trunk term can
be trusted without a second re-derivation.

**Everything passes except ruled-MIN at 40 m, and whether that failure
EXISTS AT ALL turned on the aggregation clause just added above — flora
surfaced the ambiguity before either of us had to discover it by a
contradiction later.** Read pooled across the 40–80 m band the MIN end
averages ≈ 0.64 and passes comfortably; read per-distance (now ruled, see
above) it fails its near edge by 0.021. Per-distance stands, for the reason
stated there — this is the third time in two days the deciding fact was a
DEFINITION rather than a number (Clark–Evans' denominator, BR-4's per-class
control, now this), which is a pattern rather than bad luck: a rule states
its threshold precisely and its instrument loosely, so ambiguity collects in
the instrument and stays invisible until a measurement lands near the bar.
Flagged to the lead as a candidate standing clause for `docs/ARCHITECTURE.md`
(every acceptance rule names its aggregation and its denominator, not only
its number) — that file is the lead's zone, so it is a request, not a
ruling, here.

**0.021 against 0.5 is inside Rule 36's own "a few percent" caution — the
right next step is confirming the miss is real before building anything for
it, not building for a single seed.** Core re-measures ruled-MIN at 40 m
across a small seed spread (Rule 31: assert the distribution, do not act on
one draw) before the placement-density-awareness lever from the prior
paragraph is built. **If it holds:** the lever is scoped exactly where the
number says it is needed and nowhere else — sparse-floor (ruled-MIN)
locations, near ring (40 m) only; every other combination in the table
already clears the bar on the instrument as built, so there is no case for
building it any broader than that. **If it does not hold** (0.021 was
seed noise): the lever is not built at all, and BR-5 is fully closed by the
terrain+trunks+floor instrument alone, with no placement change required.
Either way this is core's next measurement, not a new design question.

**WHICH SEED-STATISTIC DECIDES THE VERDICT — the question DISSOLVES rather
than gets answered (flora's reconfirmation §3.14, the lead's reframing,
ruled here).** Flora re-ran ruled-MIN/40m across 40 seeds while core ran
their own instrument in parallel: mean 0.5038 (a pass), median 0.4778 (a
miss, −0.022), sd 0.164, min 0.2847, max 1.0000, 60 % of individual seeds
below 0.5, 95 % range [0.294, 0.764]. Not noise around a pass — a wide
spread whose centre sits on the fail side. Flora correctly declined to pick
the statistic herself (same category of move as picking a favourable
denominator) and named it a fourth instance of the same pattern: dispersion
denominator, per-class control, ring aggregation, now seed-statistic choice.

**The lead's reading is right and it is sharper than reaching for §2.8.3:
the spread IS the finding, not the centre.** Sd 0.164 against a 0.5 bar,
seeds ranging 0.28 to 1.00, does not describe a world that nearly passes —
it describes a PROPERTY THAT IS NOT RELIABLY PRODUCED: a third of seeds
hide the find well, a third leave it naked, and which one a given player
gets is close to a coin flip. Every summary statistic pulled from that
distribution is a way of not saying so — median would still be reporting a
population's central tendency for a question that was never about the
population.

**Because BR-5 is a PER-INSTANCE placement rule, not a per-seed structural
one, it dissolves rather than needing a §2.8.3-style answer.** §2.8.3 was
written for invariants with exactly one realised structure per seed (one
massif, one landmark) — there the only lever IS a population statistic
across seeds, because nothing about a single instance can be chosen. BR-5
governs MANY placed instances per seed, each individually siteable. The
property BR-5 actually wants is not "does the average patch of ground
happen to be covered" but **"is each find placed where cover exists."**
Density-aware siting — already scoped two paragraphs above as a candidate
lever — does not shift this distribution, it COLLAPSES it: a find sited
because its local terrain+trunk+floor instrument already clears 0.5 is no
longer a draw from the ambient population that produced sd 0.164 in the
first place. This is the general lesson underneath every "which
definition decides" instance this stage, one layer further down: sometimes
the fix to an unstable statistic is not a better statistic, it is removing
the randomness the statistic was trying to summarise.

**Ruling: the density-aware placement lever is CONFIRMED, not because a
chosen statistic fails, but because the population it would summarise is
itself the defect.** Scope unchanged from above: sparse-floor (ruled-MIN
density) locations at the near ring (40 m) only — every other cell in
flora's combined table clears comfortably under any reasonable statistic
and needs nothing. §2.8.3's min/median/max reporting still binds BR-5 for
every OTHER cell and for re-verifying this one after the lever lands (a
placed-and-sited population should read a tight distribution near 1.0, not
merely a passing median — that shape is itself evidence the fix worked
rather than papered over the number).

**Two gates before this is built, not after — the ruling PAUSES for the
first one:**

1. **Instrument reconciliation outranks this entire ruling.** Flora sent
   core the same 40 seeds as a cross-check target for core's independently
   built ray-vs-disc instrument. If core's reading on those seeds disagrees
   with flora's beyond seed noise, that is a bug in one of the two
   instruments, and everything above is provisional on the WRONG number
   until it is found — ten minutes of diffing now against a lever built on
   a bug later.
2. **The BR-6 interaction is measurable before anything is built, so
   measure it before, not after.** Sparse-floor locations are, by
   construction, where BR-6's finds would otherwise have landed too;
   steering BR-5 specimens away from them concentrates finds in the
   covered fraction, which can widen gaps in the sparse fraction — exactly
   what BR-6's `FIND_GAP_MAX_MULT` (3×) tail clause exists to catch on
   wilderness routes. Core checks the gap distribution under the
   density-aware lever, not merely BR-5's own numbers, before calling this
   closed.

**BR-6 (е) — the find rule: a walker meets a small find every ~60 s.**
The mailbox tier (Kyoto calibration, research A2) — the layer between POIs
that makes walking itself the content. Base is the USER'S CHOICE (в20):
- `FIND_SPACING_BASE_S` = **60 s** of walking ⇒ ≈ 180 m of route at
  `WALK_SPEED` 3.0 m/s. Near roads denser: spacing ×`FIND_NEAR_ROAD_MULT`
  (0.5 proposed ⇒ ~30 s / 90 m). Wilderness sparser: ×`FIND_WILD_MULT`
  (2.0 proposed ⇒ ~120 s / 360 m).
- **Test:** scripted walks per regime (road route / cross-country route);
  a find is "met" when the walker passes within `FIND_ENCOUNTER_RADIUS`
  (20 m proposed) with line of sight at some station. Median gap within the
  regime's band, **and the Rule 31 clause: assert the gap distribution —
  no gap on a road-adjacent route exceeds `FIND_GAP_MAX_MULT` (3× proposed)
  of the regime's spacing. A mean can hide a desert.**
- **Must-fail control — the real rejected instance:** the current world,
  which has no find layer at all: gap = ∞ on every route. This is the
  origin of the whole complaint, and it is the control.
- **Can-pass:** finds seeded along the path network at the derived linear
  density.
- **What a find is:** mushroom ring, abandoned cart, strange stone, spring,
  a pale-spire group (§2.9) — flora/render propose catalog entries, design
  accepts. Interaction with BR-5: near roads a find may sit visible from
  the road (the road is its reveal); in the hills it obeys BR-5.

---

