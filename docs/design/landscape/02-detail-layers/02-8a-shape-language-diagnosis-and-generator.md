
### 2.8 Massif shape language — the anti-dome ruling, second pass (stage-4)

The user has rejected the mountain a **third** time, and this time gave a
shape brief rather than a complaint: «гора — это всё ещё сиська… гора она
должна быть **острая**, иметь **рёбра**, надо её **из камней собирать
местами, где-то кубы на кубах**, высоту надо задавать **линиями уровня,
которые где-то ближе, где-то дальше**, **перепады не должны быть
постоянными».** Decoded into the five things a generator must do:

1. **Sharp, not domed** — the summit is a point, not a crown.
2. **Ribs (рёбра)** — hard arêtes: flat faces meeting along visible crest
   lines, not soft radial swells.
3. **Assembled from stone in places, cubes on cubes** — blocky stacked rock
   that reads as *rock*, not as terrain that happens to be steep.
4. **Height defined by contour lines whose spacing varies** — close where it
   is steep, far where it is gentle.
5. **The steps are not uniform** — no constant gradient, and no wedding cake
   either.

They are looking at **Ravenscar** (`L0_RELIEF` 115 m). The temple massif does
not exist yet. This ruling therefore governs **every massif** — L0, LR, and
the inner faces of the border ranges — with Ravenscar as the acceptance case.

#### 2.8.1 Diagnosis — why the first invariant did not bite (measured, core)

Asked before ruling, and the answer is unambiguous.

**The dome is in the spec, not in the mesher.** The L0 stamp is
`smoothstep(1 − d/R) · peak − 13 m · (1 − prof) · (1 − ridged_noise)`. That is
a **smooth radial falloff with a little noise on its sides**: no angular
modulation, no cliff bands, no asymmetry. Worse, the noise term is multiplied
by `(1 − prof)`, so it **vanishes at the summit by construction** — the top
third of Ravenscar is a pure smoothstep surface of revolution. A smoothstep
radial profile has **zero slope at the apex, maximum slope at mid-radius, and
zero slope again at the hem**. That is not "a bit round". That is the
mathematical definition of the shape the user keeps naming.

Measured on the built crag, seed 1:

| Measure | Value | Reading |
|---|---|---|
| Lobe ratio at ½ / ⅔ / ⅚ relief | **1.27** at ½ (⅔ and ⅚ pending re-measure) — the **0.80 / 0.81 / 0.80** first recorded here is **WITHDRAWN**, see the box below | near-circular, and **identical at every height** — a self-similar cone. The clause it truly fails is the *rise*, not the level |
| Surface above mid-height over 40° | 45.9 % | passes the old 60 %-ish intent only partly, and pointlessly |
| Surface over 55° | **0.0 %** | there is no cliff anywhere on this mountain |
| Surface over 70° | **0.0 %** | |
| Slope histogram, whole crag | **FOOTPRINT-WEIGHTED — SUPERSEDED** (was: 0–10°: 45.4 %, 30–40°: 12.4 %, **40–50°: 33.2 %**, 50–60°: 0.8 %) | the reading stands — two spikes, flat ground plus **one uniform ≈45° flank** — but the *figures* are in the weighting §2.8.3 replaced, and are not reconstructed. See the box below |
| Field max slope vs mesh | field 68.7° max; mesh's 40–50° bin *higher* than the field's | **surface nets is not losing slope** |
| Raw contour spacing (5 m), CV | mean 6.9 m, σ 6.5 m, **CV 0.935** | base fBm bleeding through; the *stamp* is perfectly regular |

> ### ⚠ RE-STAMPED — the first lobe-ratio figures were produced by the
> ### digitiser this document forbids
>
> **The 0.80 / 0.81 / 0.80 above were measured by counting boundary cells.**
> §2.8.3 already bans that and requires the marching-squares contour polyline;
> the figures predate the rule they violate, so they were never legitimate
> readings of this terrain — they are readings of a digitisation choice.
> Re-measured on the **same** terrain under the binding rule: **1.27**.
>
> **The perimeter was short by ≈ 26 %, not the ≈ 10 % §2.8.3 estimated**, and
> the mechanism is worth stating so nobody re-derives it: counting boundary
> cells measures the *Chebyshev* length of an outline. A boundary running
> diagonally across the grid covers √2 of cell length per cell and is counted
> as 1, so a smooth, near-circular contour comes back short by up to
> 1/√2 ≈ 29 %. Our 26 % sits essentially at that theoretical floor — which is
> confirmation, not coincidence, since a near-circular contour is diagonal at
> most bearings. My predecessor's ≈ 10 % was itself an understatement; the
> rule was right for a **stronger** reason than the one it was written with.
>
> **Two consequences, and the second is the one that matters.**
>
> 1. **I8's headroom was never 69 %.** 1.27 against a 1.35 threshold is 6 %.
>    I8 is a far weaker test than the first pass believed, and its
>    load-bearing clause is therefore the **second** one —
>    `MASSIF_LOBE_RISE_MIN`, the requirement that lobing **grow with height**.
>    A self-similar cone can sit near 1.27 the whole way up; only the rise
>    clause can see that, which is exactly the blindness §2.8.1 diagnosed in
>    the single-slice test it replaced.
> 2. **The banded model REGRESSED lobing, and the contaminated baseline would
>    have disguised the regression as progress.** Post-band measurement:
>    **1.01, flat at every height.** Read against 0.80 that looks like a step
>    forward; read against the true 1.27 it is a step **backward**. The old
>    crenulation was base fBm bleeding through the smoothstep stamp — the same
>    bleed that scores contour-spacing CV 0.935 in the row above. It was
>    cosmetic, noise-derived and self-similar, but it was **real perimeter**,
>    and the band model replaced the slice outline with the authored `R_k(θ)`,
>    which is *cleaner* than the noise it displaced. **So the I7/I8 question is
>    not "how do we add lobing" but "does `R_k(θ)` REPLACE the fBm crenulation
>    or ride on top of it".** The mechanism is core's; the design requirement
>    is that structural lobing must exceed what noise gave us for free, at
>    every height, and must grow with height.
>
> **RESOLVED the same session — and half of point 2 was MY error, so it is
> corrected here rather than quietly left standing (core).** The re-stamp did
> its job: it stopped core accepting 1.01 and sent them looking properly. What
> they found was **a geometry bug in their own bearing-field helper, not a
> property of the band model.** They were sampling the bearing field on a
> circle sized so its circumference spanned `lobes` cells, which forces
> `radius = lobes · CELL / 2π` — at 3 arêtes on a 64 m cell that is a circle
> 61 m across sitting **inside a single 64 m cell**. The noise read it as one
> smooth patch, so contour radius varied ±4 % where `MASSIF_RADIAL_LOBE_AMP`
> asks for ±18–35 %. A circle cannot be simultaneously small enough to carry
> few lobes and large enough to cross cells: the construction was degenerate
> for **every** arête count this document specifies.
>
> - **What stands:** the re-stamp itself; that the 1.27 was fBm bleed rather
>   than structure (now *confirmed* rather than inferred); that I8's headroom
>   is 6 % and its load-bearing clause is the rise; and the sequencing call
>   that followed from all of it.
> - **WITHDRAWN — my mechanism.** I wrote that the band model "replaced the
>   slice outline with the authored `R_k(θ)`, which is *cleaner* than the noise
>   it displaced" — i.e. that authored outlines are inherently smoother than
>   fBm. That was a plausible story fitted to one number, and it was wrong:
>   `R_k(θ)` was not producing an outline at all. **There was never structural
>   lobing to erase.** The corrected sentence is narrower and duller — an
>   authored lobe term that does not actually vary *suppresses* the noise that
>   used to, and the result reads as a regression.
> - **The lesson, and it is the §1.3-withdrawal lesson wearing my own face.**
>   A directionally plausible mechanism gets less scrutiny than a surprising
>   one. "Clean authored geometry displaced dirty noise" is a satisfying
>   sentence, it explained the measurement, and it was fiction. The measured
>   number was right; my account of *why* was invented. **Ruling a number and
>   narrating its cause are two different acts, and only the first was mine to
>   make** — the mechanism should have been marked as a hypothesis for core to
>   confirm, which is exactly what this document demands of every finding it
>   receives. Recorded because the re-stamp is quoted approvingly above, and a
>   reader should see that the same box contains a correct ruling and a wrong
>   explanation attached to it.
>
> **The slope-histogram row: SUPERSEDED, not re-measured (ruled, core's
> proposal accepted).** The 33.2 % was **footprint-weighted**, measured by my
> predecessor on the old smoothstep dome — terrain that no longer exists in the
> tree. Any figure produced now would be *reconstructed*, not measured, which
> is the same class of act as the boundary-cell numbers this box withdraws. So
> the row reads **"footprint-weighted, superseded"** and carries no number in
> the weighting §2.8.3 has just replaced. `MASSIF_SLOPE_BIN_MAX` = 0.30 keeps
> its provenance stated honestly: it was chosen just under a *footprint*
> reading of 33.2 %, and under surface weighting that same flank would have
> read **higher** — so the threshold is conservative in the right direction and
> needs no revision. A constant whose provenance is "chosen against a number we
> can no longer take" is acceptable **only** when the direction of the error is
> known, which here it is.
>
> **The process point, which is the durable part.** The measurement rule was
> written before the measurement mattered, and it is what caught this. Cost:
> one paragraph in §2.8.3. Return: a baseline nobody will cite wrongly, and a
> regression that was wearing the costume of progress. Same lesson as the
> §1.3 withdrawal, arriving cheaply for once — this time the rule caught the
> number instead of the number surviving three sessions.

**Was my predecessor's invariant insufficient? Both answers are true and both
matter.**

- **It was never binding.** `LR_LOBE_RATIO` exists as a number in NUMBERS.md
  and **nothing in the pipeline evaluates it**. It was written in §2.5, which
  governs a massif that has not been built, while the user judges one that has.
  A rule authored against object A while the user looks at object B is a
  process failure, and it is the primary cause here.
- **It was also genuinely insufficient**, and saying only the first thing
  would be dodging. A **single horizontal slice** cannot see that the shape is
  identical at every height — the measured 0.80/0.81/0.80 is exactly the
  signature it is blind to. And "≥ 60 % of the upper surface above 40°" is
  **satisfied by a perfect 45° cone**, which is precisely «перепады
  постоянные». Both clauses are plan-view or scalar statistics; **neither
  constrains the vertical profile**, and the vertical profile is the entirety
  of what the user described.

**One test I intended to write, killed by the measurement.** A floor on the
*coefficient of variation of contour spacing* is a bad invariant: the current
dome already scores **0.935**. Variance is free — fBm supplies it. What the
user is asking for is not variance, it is **alternation**: risers and benches
in a rhythm that is itself irregular. The invariants below test alternation
and are recorded here with the rejected version, so nobody re-proposes CV.

#### 2.8.2 The generator model — the BANDED CONTOUR MASSIF

The user handed us the authoring model in their own sentence: *«высоту надо
задавать линиями уровня»*. Take it literally. A massif is **no longer a
radial profile with noise on it**; it is a **stack of contours**, and height
is reconstructed between them. This matters because it is *structurally
incapable* of producing a dome — there is no smooth falloff anywhere in it.

Four seeded fields, all per-sample analytic, all deterministic, all pure
height-function work (so the voxel pipeline carries them unchanged):

1. **Band elevations `e_0 … e_n`.** Non-uniform by construction:
   `e_{k+1} = e_k + Δ_k`, `Δ_k` drawn from a seeded distribution whose
   coefficient of variation is at least `MASSIF_BAND_SPACING_CV_MIN`. Bands
   start at the cliffline (`MASSIF_CLIFFLINE_FRAC` of relief) and run to the
   summit. **This is "линии уровня, которые где-то ближе, где-то дальше",
   authored rather than hoped for.**
2. **Radial extent `R_k(θ)`** — each band's outline as a function of bearing:
   `R_k(θ) = R_base(e_k) · (1 + ε(e_k) · ridged(θ))`, with
   `ε ∈ [MASSIF_RADIAL_LOBE_AMP_MIN, _MAX]` and **ε increasing with
   elevation**. Outward lobes are the **arêtes**; inward folds are the
   **couloirs**. Irregular by seeded phase — a symmetric star reads as
   artificial (§2.5.2, unchanged).
3. **Profile exponent `p(θ) ∈ [MASSIF_PROFILE_EXPONENT_MIN, _MAX]`** — the
   base falloff is `h = H·(1 − d/R)^p` with **p > 1**, which is the whole
   fix in one symbol. `p > 1` gives **steep at the summit, shallowing to the
   foot** — the concave profile every real mountain has, because talus fans
   out at the bottom. `smoothstep` gives the opposite and that is why it
   reads as a breast. Varying `p` with bearing **is** the §2.5.4 asymmetry
   rule: the low-`p` sector is the gentle flank that carries the ascent, the
   high-`p` sector is the scarp face.
4. **Riser class per (band, angular sector).** Each band's riser is either a
   **CLIFF** (≥ `MASSIF_CLIFF_SLOPE_MIN`) or a **RAMP**, chosen per sector by
   seeded noise. So a single band can be a cliff on the north side and a ramp
   on the south. This is what stops the wedding cake: the terracing is
   discontinuous *around* the mountain as well as irregular *up* it.

Between bands the surface is a **bench** (`MASSIF_BENCH_SLOPE_MAX`, width
`MASSIF_BENCH_WIDTH_MIN…MAX`). Riser heights come from
`MASSIF_CLIFF_BAND_MIN…MAX`.

**FACETS ARE PLANAR BY CONSTRUCTION, AND A COULOIR IS A PAIR OF FACETS RATHER
THAN A DENT IN ONE (ruling, stage-4 — this is the model answer to I11's
distance failure, and it is the same failure as I7).**

**I7 failing on every seed and I11 failing at 600 m are ONE failure, not two.**
A tangent break is scale-free: a genuine corner between two flat faces reads as
a corner at any distance, because the chord on either side lies along a
straight facet however wide the measurement window grows. **Breaks can only
wash out with distance if the facets themselves are curved** — then a wider
window swallows the corner into the surrounding curvature and the outline
becomes one smooth mass. That is exactly what 5/8/12/4 breaks at 300 m
collapsing to 1/1/0/0 at 600 m means, and it is exactly what I7 says directly:
`MASSIF_FACET_TURN_MAX` is the flatness test, and it has never passed. **The
facets are not flat.** Everything else follows.

- **Seeded variation perturbs the polygon's PARAMETERS, never the radius
  continuously across a facet.** Draw each facet's support distance `d_i` and
  bearing `α_i` once, per facet, from the seeded field. Do **not** modulate
  `R(θ)` with a continuous per-bearing term — that is precisely what bends a
  flat face into an arc, and it is why the support-function construction, which
  is polygonal by definition, has been producing curved facets anyway.
- **A couloir ADDS two facets; it does not dent one.** A notch with its own two
  planar walls preserves flatness and **adds corners**, where a smooth
  re-entrant subtracts them by curving the face it sits in. This also retires
  the tension core measured earlier — deepening couloirs dropped arêtes 4 → 0
  *because* they were dents. As facet pairs they raise I8 and I11 together.
  Everything on the massif is then flat faces meeting along lines at every
  scale, which is the user's «рёбра» and this document's own definition of an
  arête, finally built the way both are written.
- **Crest structure is sized against the ACCEPTANCE DISTANCE, not against the
  massif.** A facet's arc must exceed the readability window at the far
  acceptance range (≈ 20–24 m at 600–717 m) so a corner survives as a corner
  out to where the valley looks at it. **Second instance of this rule — the
  summit tor was the first**, and two is a pattern: **detail sized against the
  object shrinks out of legibility as the object recedes. Anything required to
  read at distance is sized against the distance.**
- **The corner count follows for free:** only limb-facing facets contribute
  breaks, so four arêtes alone put barely two corners on the outline. Couloirs
  as facet pairs raise that above I11's floor without touching
  `L0_ARETE_COUNT`, and without hitting §2.8.2's convexity cap, because a
  re-entrant notch is exactly what makes the section non-convex.

**A PER-BEARING FIELD MUST BE VERIFIED UNIFORM OVER ITS DECLARED RANGE (core's
proposal, adopted — it is the distribution-shaped version of the reject-case
control).** Core measured **0 % of samples below 0.4**, then lumps of 26 % at
0.6 and 30 % at 0.8, against a raw lattice uniform to a tenth of a percent.
Every "seeded spread" in this model was silently using the top 60 % of its
declared range, lumpily. What that cost, all of it invisible until measured:
the profile exponent never approached `MASSIF_PROFILE_EXPONENT_MIN`, **so the
gentle-flank half of §2.8.2's asymmetry rule never existed at all**; cliff
risers were never drawn near `MASSIF_CLIFF_SLOPE_MIN`, leaving 4 % in the
50–60° bin — a hole exactly where mass was expected; and the 0.5 cliff/ramp
split was arithmetic about a coin that was never fair. **A spread that is
secretly peaked passes every review, and no invariant in this document names
it.** Assert the distribution, not just the bounds.

**CLIFF RISERS ARE PLANAR BUT NOT IDENTICAL — THE ANGLE VARIES BETWEEN BANDS
(ruling, stage-4; this is my own I3 fix colliding with my own I4 rule).** After
the aspect cascade, I4 fails on eight seeds of twelve: a genuinely steep massif
concentrates surface in the 60–70° bin. The cause is not the steepening, it is
**planarity plus a single cliff angle.** I ruled risers planar so they would
stop spending their width on sub-cliff slope (I3) — and a planar face puts
*all* of its area at *one* angle, so if every cliff riser uses the same angle,
every riser's surface lands in the same 10° bin. My two rulings were fighting,
and I4 is the invariant that noticed.

**Fix, and it moves no threshold: the CLIFF class is a BAND OF ANGLES, not an
angle.** Each riser stays planar and stays above `MASSIF_CLIFF_SLOPE_MIN`, but
draws its angle from a seeded spread across the steep range. I3 is unaffected —
every riser is still a cliff — while the surface spreads across several bins
and I4 is satisfied by *variety* rather than by *shallowness*. This is also the
truer landform: a real banded scarp has faces at 55°, 70° and overhanging in
the same massif, not one repeated angle. It is the bench rule from below,
applied above: **`MASSIF_CLIFF_SLOPE_MIN` is a floor, and a floor was never a
target** — precisely what I said about `MASSIF_BENCH_SLOPE_MAX` being a ceiling.

**I4 is sound and is not the thing to relax.** Core asked whether I4 might now
be a texture rule being asked to constrain a legitimately uniform form. It is
not: «перепады не должны быть постоянными» is *exactly* the complaint that a
uniformly 65° massif re-creates at a steeper angle. A constant gradient is a
constant gradient whatever its value — which is the same conclusion §2.8.2
reached when dead-flat benches and ceiling-pinned benches each put most of the
mountain in one bin. **This is the third variant of that identical failure, and
each time the fix was variety rather than a different constant.**

**A CLIFF riser is a PLANAR face, and a bench is neither dead flat nor pinned
at its ceiling (core, found by measuring — four bugs with one lesson under
them).** Each of these is written down as a rule because each produced a
*measured* invariant failure on the first implementation of this model, and
none of them was visible by looking at the shape:

- **A smoothstepped riser spends its width on sub-cliff slope.** A riser eased
  in and out reaches `MASSIF_CLIFF_SLOPE_MIN` only at its midpoint, so most of
  its area lands in the 30–50° bins and **I3 measures no cliff on a mountain
  that visibly has them**. Cliff risers are therefore **planar faces** — full
  angle from lip to base. This is also what §4's snap rule requires: a crease
  is drawn as a crease only where there is one, and an eased riser has no lip
  for the screen-space slope derivative to find. My predecessor's promise of a
  **hard splat edge at band lips** was made assuming a planar riser; it is now
  a *requirement* rather than an assumption, and the two rules corroborate.
- **A dead-flat bench is a constant gradient too** — flat benches put **62 %**
  of the massif in a single slope bin, an I4 failure produced by the fix for a
  different I4 failure. And **pinning benches at `MASSIF_BENCH_SLOPE_MAX` is
  the same mistake wearing a different constant** (**75 %** in another bin).
  A bench is *ground*, and §2.7's rule is general and already binding:
  **terrain never flattens.** Benches carry the `GROUND_MICRO_*` octave
  (0.3–0.6 m over 8–16 m) and take a seeded slope spread **within**
  `MASSIF_BENCH_SLOPE_MAX`. That constant is a **ceiling** — the angle at
  which a bench stops being able to carry a road — and it was never a target.
- **A RAMP band must still be a band.** A sector whose riser class is RAMP
  cannot fall back to the bare underlying cone; that left half the massif
  unbanded and reads as the old dome wearing stripes down one side. The
  cliff/ramp choice varies the riser's **angle**, never whether the mountain
  has contours at that bearing.

**The lesson under all four, and it is why §2.8.3 says implement I1 and I4
first:** three of them are cases where the obvious fix for one invariant broke
another, and **constant-gradient failures move rather than disappear.** I4 is
the invariant that follows them around. All four were caught by measuring the
invariant, not by looking at the mountain — which is the whole argument for
having written the invariants down before building the shape.

**The cross-section is a FACETED POLYGON WITH RE-ENTRANT COULOIRS, and the
couloirs are load-bearing rather than decorative (core's construction, ruled
in).** `R_k(θ)` is built as an irregular rounded polygon by support function,
`r(θ) = min_i d_i / cos(θ − α_i)` — a boundary of **flat facets meeting at
corners**, which is this document's own definition of an arête («плоские
грани, сходящиеся по линии») rather than a proxy for it. Three consequences
worth having in the doc, because they constrain any future massif and not just
this one:

- **A support function is convex by construction, and a near-regular convex
  cross-section is CAPPED at `n·tan(π/n)/π`** — 1.65 for 3 facets, **1.27 for
  4, 1.16 for 5**. Against `MASSIF_LOBE_RATIO` = 1.35 that means **a convex
  massif with 4 or 5 arêtes cannot pass I8 at any amplitude**, and rounding
  the corners only lowers it further. Since `L0_ARETE_COUNT` is 3–5 and the
  LR's is 4–7, **couloirs are what make I8 satisfiable at most of the arête
  counts this document allows.** §2.8.2 asked for outward lobes *and* inward
  folds; this is the proof it needed both.
- **The convex escape route exists and is the wrong one — record it so nobody
  takes it.** The cap above is for a *near-regular* polygon; an **elongated**
  convex cross-section beats it easily (a 4:1 rectangle scores 1.99). So a
  future implementation could pass I8 convexly by stretching the massif. It
  must not: an elongated L0 is a **ridge, not a peak**, it breaks §1.5's
  skyline read and C4's "one unmistakable mass with a summit", and it would
  satisfy the invariant while destroying the thing the invariant protects.
  **Elongation is a landform choice (border ranges, §2.6, are legitimately
  elongated), never a knob for making a lobe test pass.** Core's addition, and
  it is the part worth having written down: **no invariant we have would
  notice.** An elongated support polygon puts its corners on the long axis, so
  arête bearings *cluster* — and I7's persistence check would keep passing
  while the mountain became a ridge. The bar must be a design rule precisely
  because the suite is blind to it.
- **I7 and I8 pull in opposite directions, and the resolution is that couloirs
  FADE TOWARD THE SUMMIT.** Measured by core: deepening and widening couloirs
  raised I8 and dropped persistent arêtes **4 → 0**, because a couloir spread
  across a facet *curves* that facet and I7 requires it flat. The fix is
  structural, not a tuning compromise: **couloirs are flank features that
  merge into the arêtes as they rise.** Summit contours stay clean facets (I7
  reads them); flanks keep re-entrant perimeter (I8 reads that). This is also
  what erosion actually does — a couloir is cut by what runs down it, and
  nothing runs down a crest. Second, independent reason it is correct: an
  angularly-constant couloir shrinks to ≈ 1 m of arc at summit radius, far
  under `MASSIF_ARETE_TURN_ARC_MAX` = 15 m, so near the top it could only ever
  be **noise to the arête detector**.

**COULOIR DEPTH IS ABSOLUTE (metres); COULOIR ANGULAR WIDTH IS RELATIVE (a
fixed fraction of its facet). This is the fix for I8's two clauses fighting
each other, and it is a change of UNIT rather than of value (ruling,
stage-4).** Core mapped the parameter space and found every axis they had
traded I8's level clause against its rise clause: couloirs carried to the
summit hold the level and kill the rise; couloirs that fade buy rise and lose
level; curving the blend trades them one-for-one and at `k³` collapses I7 to
zero arêtes. **They were all the same experiment**, because every one of them
varied *how fast the couloir fades* while leaving the couloir's depth
expressed as a fraction of local radius. That is the actual defect:

- **A quantity held as a fraction of local radius is self-similar by
  construction, and self-similarity is precisely what the rise clause exists
  to detect.** Lobe ratio responds to *relative* inset ε = depth / R. Hold ε
  fixed and the ratio is identical at every height — the 0.80/0.81/0.80
  signature §2.8.1 diagnosed, reproduced by a different mechanism.
- **Hold the depth in METRES and the rise appears for free.** R shrinks toward
  the summit, so a constant absolute inset is a *growing* fraction of a
  shrinking radius: ε = depth / R(h) rises on its own. The mountain becomes
  more articulated near the top not because its features grow but **because
  the mountain gets smaller around them**, which is what actually happens to
  real massifs and is exactly the read the rise clause was written to reward.
- **Angular width stays relative, and that is what protects I7.** Holding
  *width* absolute would make a couloir occupy an ever-larger angular slice of
  an ever-smaller circumference, curve the facets, and kill the arêtes again.
  Constant angular width keeps each couloir the same fraction of its facet at
  every height, and core's own arc-length check confirms it stays safe: ≈ 1 m
  of arc at summit radius, far under `MASSIF_ARETE_TURN_ARC_MAX` = 15 m, so up
  there it reads to the detector as a corner rather than as a curve.
- **Precedent, so this is recognisable rather than novel:**
  `MASSIF_ARETE_TURN_ARC_MAX` is already absolute on purpose — «гребень,
  который поворачивает 60 м, — это плечо, какой бы горы он ни был». Same
  reasoning, applied to the couloir instead of the crest.
- **THE ABSOLUTE SCALE IS THE BAND HEIGHT, NOT THE MASSIF'S RADIUS (core's
  correction, and it repairs a hole I put in my own ruling).** "Absolute"
  needs a unit, and the obvious choice is wrong: taking
  `MASSIF_RADIAL_LOBE_AMP` off the 180 m base radius gives 32–63 m insets —
  wider than the entire upper mountain — so the clamp below binds at *every*
  height and **silently restores the fraction-of-local-radius behaviour the
  unit change exists to remove.** Measured in that state: levels
  1.50/1.50/1.60 but rise 0.10 and I7 gone. My clamp was not a safety net, it
  was **a re-entry point for the bug it was guarding against**, and it is
  withdrawn as written. The scale is `MASSIF_CLIFF_BAND_MIN…MAX` (8–15 m), on
  core's reasoning and it is the right reasoning: **a couloir is a gully
  incising the cliff bands, so it is the same landform at the same scale and
  it should carry the same units.** A feature's size comes from the feature it
  cuts, never from the mountain it sits on — which is the same principle as
  `MASSIF_ARETE_TURN_ARC_MAX` being absolute.
- **A clamp against the apex is still needed but must not be able to bind
  below the summit region.** If it engages at ordinary heights it is
  re-introducing self-similarity, and that failure is silent — the invariants
  keep passing at the level while the rise quietly dies. Any implementation
  must report whether the clamp bound, and where.

**Varying arête COUNT with elevation is the other candidate and it is NOT the
first lever** (core raised it; ruled). Ribs merging as they rise is
geologically true and may earn a place later, but it fights I7's persistence
check by construction — an arête that stops existing above 0.55 relief cannot
be detected on 0.6 of four levels. Change the unit first: it is cheaper, it
touches nothing else, and it is the only axis core's sweep did not vary.

**A structural feature the invariants depend on is NEVER a per-instance coin
flip (ruled, from a near miss).** Core's first variant made couloir *presence*
a seeded per-facet draw; on seed 1 all three facets missed and the massif came
out a bare convex polygon with zero couloirs — a shape that *looks* reshaped
and satisfies nothing. Rule: **the seed varies a feature's character — depth,
asymmetry, bearing, spacing — never its existence.** Anything an invariant is
counting must be guaranteed by construction, because a seeded absence produces
a world that fails silently and plausibly, which is the most expensive kind of
failure we have. Same rule already applies to arêtes, cliff bands and benches;
it is written down here because a coin flip is such a natural way to author
variety.

**Both per-bearing fields must be PERIODIC in θ (core's catch, binding).**
`R_k(θ)`, `p(θ)` and the riser-class sector index all wrap: sampling noise on
the *angle value* puts a branch cut at ±π and produces **a vertical seam from
summit to foot** — a scar exactly where nothing should be. Sample instead on
the unit vector `(cos θ, sin θ)`, i.e. noise over a circle embedded in the
plane, which is periodic by construction and costs nothing; and let sector `0`
and sector `n−1` be neighbours so the cliff/ramp alternation has no
discontinuity at the same bearing. Recorded here rather than left in a message
so the LR massif does not rediscover it.

**Why this is cheap:** every term above is arithmetic on `(d, θ, h)` at a
sample that is already being evaluated. Core's own ranking agrees — angular
ridge modulation, non-uniform falloff, cliff-band quantisation and asymmetry
are all *free* per-sample math on a position function. No new pass, no new
storage, no mesher change.

**The bonus nobody had to pay for.** Cliff risers exceed `SLOPE_ROCK_MIN`
(40°) and benches sit under `SLOPE_GRASS_MAX` (30°), so the existing §4 splat
paints **risers as rock and benches as grass/blend automatically**. The
contour rhythm becomes a **visible horizontal stripe rhythm** at 640×360, with
zero shader work. The user asked to define height by contour lines; this is
the mechanism by which they will actually *see* them.

