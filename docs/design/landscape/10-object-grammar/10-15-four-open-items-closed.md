
### 10.15 THE FOUR OPEN ITEMS — closed, and three of the four close against my own lines

The lead is right that these block other zones. Each is ruled here; **none is
ruled by moving a threshold.**

#### 10.15.1 REJECTION 3 — the span floor is RETIRED today, and the replacement is a ratio, not a new dimension

flora did this correctly: measured, reported, refused to assert. The state is
that **foliage span no longer separates** (accepted oak 0.32–0.44 against a
rejected birch bounded at ≤ 0.42), **foliage base does not either** (rejected
0.58 against an accepted pine at 0.51–0.60), and **limb spread does not either**
(accepted oak 0.166 *below* the rejected birch's 0.17–0.19).

> **RULING 1: `FOLIAGE_SPAN_MIN` = 0.28 is RETIRED, not left standing.** It
> rejects a synthetic rosette and nothing else, and the pole-ratio clause below
> rejects that rosette too. **A weakened threshold that still looks like an
> invariant is worse than an admitted gap** — flora's own sentence, and it is
> right. Nothing is lost by the retirement: `LIMB_SPREAD_MIN` = 0.15 still
> rejects the synthetic palm at 0.06.

##### Why no single dimension can work, stated so nobody tries a fourth

**The accepted birch and the rejected birch differ in exactly one authored
input** — the crown base, 0.40 against 0.58. Crown *width* was never touched. So
**no purely horizontal quantity can separate that pair**, and the vertical one
that does separate it (span, 1.29×) fails only because the oak's widening moved
the oak. The separating quantity has to be one in which **the oak's small span
is paid for by its large width** — that is, a quantity carrying both.

##### The quantity, and it is the palm's own definition

> **`CROWN_POLE_RATIO` = (height of the lowest foliage) / (crown width).**
> Both measured on **built geometry**, never on the authored container.
>
> **A tree is a pole with a tuft on top when its bare bole is longer than its
> crown is wide.** That is the sentence the user's rejection was describing, and
> it has been sitting in plain language the whole time.

Predicted from approved constants, and these are **predictions, not
measurements** — the arithmetic is mine, the geometry is flora's:

| | bole / crown width | source |
|---|---|---|
| Dale Oak | **0.50–0.60** | base 0.35–0.42, crown/height 0.70 (`TREE_WIDTH_SCALE`) |
| River Birch, accepted | **≈ 1.1–1.2** | base 0.40–0.45, width 6–8 m on 16–22 m |
| **River Birch, REJECTED** | **≈ 1.8** | base 0.58, width 5–7 m on 16–22 m |
| synthetic palm | ≫ 3 | rosette at 0.90 |

> **`CROWN_POLE_RATIO_MAX` = 1.4 — PROPOSED, and it is NOT a gate until flora
> measures it.** 1.4 sits ~25 % above the highest predicted accepted value and
> ~25 % below the rejected artefact, which is the same construction that placed
> `CROWN_ASPECT_MAX` at 2.0 in a 1.29× interval.

**The measurement that must happen before it becomes a gate**, and the acceptance
condition for the constant itself (Rule 30's sharpening, which is design's own):

1. Measure on built geometry, **per variant, never pooled** (§5's own lesson).
2. **The rejected birch is rebuilt as the control** — `BIRCH_CROWN_BASE_FRACTION`
   at 0.58 — because a synthetic control is the easy reject and the artefact the
   user actually turned down is the hard one.
3. The threshold must sit **strictly between the highest accepted and the
   rejected artefact.** If no value does, **the quantity is wrong and it is
   reported as wrong** rather than shipped with a floor under everything.
4. **Watch item, flagged because my own arithmetic finds it:** the birch's width
   band (6–8 m) and height band (16–22 m) drawn *independently* can produce a
   legal birch at ratio 1.65, over the proposed ceiling. `TREE_WIDTH_SCALE`'s
   note says width is drawn **with allometry**, i.e. correlated with height, and
   under that correlation the worst legal birch lands near 1.22. **If width and
   height are in fact drawn independently, this constant fails on the accepted
   species and the finding is about the draw, not about the threshold.**

##### The conifer, exempted with the gap named rather than papered

`CROWN_POLE_RATIO` puts our pine at ~2.4–3.3 — **above the rejected birch** —
and the pine is accepted, because a mature conifer genuinely *is* a long bare
bole under a crown. **The pine is exempt, by the same door `CROWN_ASPECT_MAX`
already uses: cone/spire species are exempt by their written brief.**

**But an exemption without a replacement is a hole, so it gets one:**

> **REJECTION 2's control set must gain a «pine pole» — a conifer with whorls
> only in the top 15 % of its height — and REJECTION 2 must REJECT it.** My
> reading of its instrument says it will (roughness is mean absolute change in
> row fill, and a stick with one tuft has almost none), **but that is a
> prediction and it must be run.** If REJECTION 2 passes the pine pole, **the
> conifer has no palm sentinel at all**, and that is the finding, not a reason
> to widen `CROWN_POLE_RATIO` until it fits a cone.

#### 10.15.2 The 229 m² floor — RETIRED as a gate, re-denominated, and the 2.5× margin is probably hiding a LOSS

`FLORA_PRESENTED_AREA_FLOOR_M2` = 229 is an **accepted-sample floor**: the worst
azimuth of the build the user called «листва прикольная». Its control is «half
density must fail». **After the crown widening the control passes**, so the
floor permits a crown with half its leaf material. **A floor whose control can
no longer fail is a description** (Rule 30), and this one has become one.

**The diagnosis is structural, not a matter of re-baselining:**

> **Presented area conflates EXTENT and DENSITY, and the widening moved extent.**
> A crown 45 % wider presents more area **for free**, without being any fuller.
> The quantity therefore responds to a lever that is not the one it exists to
> guard — which is §10.12's σ failure in a different zone: *a sound,
> falsifiable, well-controlled criterion aimed one quantity to the left of the
> target* (Rule 41).

> **RULING: `FLORA_PRESENTED_AREA_FLOOR_M2` is RETIRED as a gate and stays
> REPORTED as a diagnostic** — the same disposal as `GROUND_RELIEF_SIGMA_20M_MIN`,
> and for the same reason. **Do not re-baseline it to 2.5× its value:** that
> fits a threshold to a proxy that is structurally incapable of gating the
> property (Rule 45's distinction).
>
> **The replacement divides the extent out.** `FLORA_CROWN_OPTICAL_DEPTH` =
> **(presented card area) / (crown's own presented silhouette area)**, both at
> the **worst of 36 azimuths**, both on built geometry. It is a dimensionless
> "layers of leaf" number.

Four properties, each earning its place:

- **The control fails by construction.** Halving the cards halves the numerator
  and leaves the denominator alone, so the half-density arm drops by exactly 2×.
  **Rule 48's positive form is satisfied by the shape of the quantity, not by a
  lucky threshold.**
- **It cannot be bought with width.** Widening raises numerator and denominator
  together, which is what a fullness measure should do and what the absolute
  floor did not.
- **It serves both existing consumers unchanged** (Rule 35): flora checks
  against it, and render's «cards buy angular coverage, ≥ 3 planes» is still
  expressible in it, because the worst-azimuth aggregation is unchanged.
- **It makes the open LOD gap legible instead of maskable.** Reduced detail
  measured 208 m² against 229 — a 9.2 % miss that a wider crown could have
  erased without adding a leaf. Under a ratio it cannot.

> **PREDICTION, recorded before the measurement so it is not discovered as a
> surprise (§10.10.2's discipline): the ratio has probably FALLEN.** Presented
> area rose ~2.5×; crown silhouette area rose by at least the width factor and
> the depth factor together. **If the ratio is below the pre-widening build's,
> the 2.5 × "margin growth" has been reporting a fullness loss as a safety
> margin.** The anchor for the new floor is that pre-widening build — the one
> the user approved by eye — measured under the new denominator.

**Value deliberately not assigned.** Three measurements set it and none of them
is mine: the pre-widening accepted build (the anchor), today's shipped build,
and the half-density control. **The floor goes between the control and the
anchor**, and if today's build is under the anchor that is the finding.

#### 10.15.3 `CROWN_BASE_FRACTION_MIN` — the fraction goes, and the absolute that replaces it is NOT 2.2 m

The lead's diagnosis is right and it is my own lesson pointed at me: 0.35 was
derived on an **8 m** oak (2.8 m of clear trunk) and is applied to a **24–32 m**
one, where it yields 8.4–11.2 m against a stated requirement of 2.2 m.

> **RULING: `CROWN_BASE_FRACTION_MIN` = 0.35 is RETIRED as the walkability gate.
> A fraction of height was always a proxy for an absolute clearance, and the
> absolute exists.** Fifth instance of the family §5 already named — *a model
> change can invalidate a constant's derivation without changing its number* —
> and here the model change was the tree tripling in height.
>
> **`CROWN_BASE_FRACTION_MAX` = 0.45 survives**, for the same reason
> `GROUND_RELIEF_SIGMA_20M_MAX` survived: **the ceiling's job genuinely is a
> fraction.** A crown starting above 45 % of height is a proportion failure at
> any size, and that is scale-free. Only the floor falls.

##### And the absolute cannot be 2.2 m, because 2.2 passes the artefact the user rejected

**`CANOPY_CLEARANCE_MIN` = 2.2 m sits BELOW the real rejection.** Its own
NUMBERS row records the complaint: «кроны начинались на **2.7 м**, игрок шёл
СКВОЗЬ листву». **2.7 > 2.2.** The gate that is supposed to guarantee walking
*under* the canopy would pass the build in which the player walked *through* it.

> **This is design's own sharpening of Rule 30, and it lands on design's own
> constant: a floor placed below every real failure is a description, not a
> test.** I recommended exactly this to flora about the synthetic palm control,
> and did not run it across my own rows.

> **PROPOSED: `CANOPY_CLEARANCE_MIN` 2.2 → 3.6 m.** Derived, then checked
> against the rejected sample — in that order. **The derivation:
> 2 × `PLAYER_CAPSULE_HEIGHT` (1.8 m)** is the point at which a ceiling reads as
> a *hall* rather than as a *doorway*, and «ходить ПОД пологом» is a request for
> the first. **The check:** 3.6 m sits **33 % above the rejected 2.7 m**, so the
> gate now catches the artefact it exists to catch.

**The one measurement that could move it, and it must be taken before approval:**
the **lowest clear trunk across the whole accepted catalog on built geometry**,
including the riparian willow (base 0.30–0.35, and its height is not in
NUMBERS.md — I did not have it and did not guess it). **If any accepted species
measures below 3.6 m, the floor moves to the symmetric point between it and
2.7 m; if any accepted species measures below 2.7 m, the quantity is wrong and
that is the finding.** Predicted safe: birch 0.40 × 16 m = 6.4 m, oak
0.35 × 24 m = 8.4 m — both clear with 1.8–2.3× to spare.

**Nothing about the giant oak is touched:** its steps and platforms are §5's and
GIANT_OAKS.md's, and a 3.6 m clearance is a fifth of its bole.

#### 10.15.4 The clearing в9 — the exemption is WITHDRAWN, because I misread the contract I was exempting

Core measured the clearing at **80 m against `AUTHORED_FLAT_RADIUS_MAX` = 50 m**
and correctly refused to bend either side. **Neither side has to bend, because
the exemption should never have been granted.**

I read «спокойная» as «ровная». **They are different bands, and the code says
so:**

- `glade_factor()` multiplies the **meso** tier only — `forest_grive_component`
  (LF-2's 2–5 m ridge-and-swale field) and `meso_scale` into `ground_relief`.
- **§2.7's micro octave passes through the clearing untouched**, and
  `WorldgenForest.cpp`'s own note records the change that made it so.
- **в9's own dictionary entry demands the opposite of flatness.** §2.10 LF-1:
  «base field + §2.7 micro-relief (0.3–0.6 m waves), **NO meso hills**»;
  acceptance «the eye-height horizon frame shows **waves, not a billiard
  table**»; control «the pre-§2.7 flat plane — the real rejected instance
  («земля плоская») **fails**».

> **RULING: §10.12.6's exemption for в9 is WITHDRAWN. в9 is bound by
> `GROUND_OCCLUSION_COUNT_MIN` = 3 like any other legal ground.**
> `AUTHORED_FLAT_RADIUS_MAX` = 50 m **does not move and does not need to**: it
> continues to bound the places that genuinely are authored flat — corridors,
> building pads, the castle terrace, the shore band — and the 80 m clearing was
> never one of them. **The conflict was mine, and it dissolves rather than being
> waived or bent.**

**And withdrawing it exposes what the exemption would have hidden**, which is
why this matters beyond the bookkeeping:

> **в9 measured σ = 0.119 m.** Its micro octave alone, at the approved
> `GROUND_MICRO_AMPLITUDE` 0.3–0.6 m, predicts a detrended σ near
> **0.19–0.38 m** — the clearing is delivering **under half of the amplitude its
> own approved octave specifies.** The exemption would have certified that as
> authored calm. **This is exactly the failure §10.1.2 was built to catch: «a
> floor that catches a MISSING octave rather than a re-litigation of an approved
> one.»**

**Handed to core as a measurement, not as a claim** — the amplitude may be drawn
from a distribution and 0.119 may be one sample of it. What core should report
is the micro octave's realised amplitude distribution inside в9 against its own
approved band.

**And the arithmetic says the clearing can pass on micro relief alone**, so this
is not a request for an exemption in a new costume. For a wave of length L, the
crest hides the trough at ranges of order L, where the grazing angle is
atan(1.7/L):

| L | RMS slope at σ = 0.30 m | grazing angle at d = L | occludes? |
|---|---|---|---|
| 8 m | **13.5°** | 12.0° | yes |
| 12 m | 9.0° | 8.1° | yes |
| 16 m | **6.8°** | 6.1° | yes, thinly |

**At the approved amplitude the micro octave clears its own grazing angle across
the whole 8–16 m band, by 12–20 %.** Thin, and thin in the same direction
§10.12.3 found for the meso band — **the short end of the wavelength range is
where the margin lives, at both tiers.** That is now two independent bands
saying the same thing, which is worth more than either alone.

##### Numbers (Rule 35, via lead)

| constant | action | note |
|---|---|---|
| `CROWN_POLE_RATIO_MAX` | **new, = 1.4 — PROPOSED, not a gate** | flora measures first; the threshold must sit strictly between the highest accepted and the rebuilt rejected birch, or the quantity is reported wrong (§10.15.1) |
| `FOLIAGE_SPAN_MIN` = 0.28 | **RETIRE** | separates nothing; the rosette is caught by `LIMB_SPREAD_MIN` and by the pole ratio |
| `FLORA_PRESENTED_AREA_FLOOR_M2` = 229 | **RETIRE as a gate**, keep reported | its control can no longer fail; do **not** re-baseline it |
| `FLORA_CROWN_OPTICAL_DEPTH_MIN` | **new, value TBD by three measurements** | presented card area / crown silhouette area, worst of 36 azimuths; floor between the half-density control and the pre-widening accepted build |
| `CROWN_BASE_FRACTION_MIN` = 0.35 | **RETIRE as the walkability gate** | a fraction proxying an absolute that already exists |
| `CROWN_BASE_FRACTION_MAX` = 0.45 | **unchanged** | the ceiling's job genuinely is a fraction |
| `CANOPY_CLEARANCE_MIN` | **2.2 → 3.6** | m; 2 × `PLAYER_CAPSULE_HEIGHT`, and 33 % above the **rejected** 2.7 m the old value passed |
| `AUTHORED_FLAT_RADIUS_MAX` = 50 m | **unchanged** | в9 is no longer claimed under it (§10.15.4) |
| `BUILT_EDGE_LEVEL_CHANGE_MIN` = 0.4 m | **WITHDRAW before approval** | it is `PLAYER_STEP_HEIGHT` rounded (Rule 39) |
| `TOWER_CROWN_LINE_VARIATION_MIN` = 1.0 m | **WITHDRAW before approval** | it is `SILHOUETTE_MIN_PX` restated: d_site / 30 |
