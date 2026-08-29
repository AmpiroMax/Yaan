
### 10.10 THE ARMS CAME BACK AND MOVED THREE OF MY OWN LINES (stage-5)

Render shot all three arms plus a **control with no air at all**, and the
control is what did the work. Arm C (`HAZE_SCALE_LENGTH` 600, `HAZE_HEIGHT_SCALE`
40, `HAZE_BASE_HEIGHT` 30) shipped. Of the three propositions I wrote in §10.9,
**one was measuring the wrong system, one had a threshold set without its
control, and one was never a proposition at all.** All three corrections below
are against my own lines.

*Recorded once and not dwelt on: §10.9.1 predicted that a ~600 m scale length is
what the tabled 800 m onset encodes, and 600 is the arm that shipped. It was one
data point offered as evidence; the arm was chosen on render's measurements, not
on my note, and the prediction is worth exactly what a prediction is worth.*

#### 10.10.1 H2 — WITHDRAWN from the haze question, ACCEPTED as a terrain defect, and it generalises to a rule

**Accepted without reservation.** H2 scored **0.61 against a required 1.00 with
zero atmosphere in the frame.** The lead's reasoning is correct and it is sharper
than a concession — it is a rule we did not have:

> **A CRITERION THAT FAILS ITS OWN ZERO-DOSE CONTROL IS MEASURING THE WRONG
> SYSTEM.** A threshold that cannot be met at dose zero cannot select a dose. It
> is not a strict criterion; it is a criterion pointed at the wrong subject, and
> every value it returns is a reading of something else.

That belongs next to F7 in §1.6, and it is why shooting the no-air control was
worth an arm. F7 says a frame must be *able* to fail; this says a criterion must
be *able* to pass. **The two together are the same discipline from both ends.**

##### The diagnosis, as a hypothesis with the probe that separates it

Two deaths are possible at the hem, and they are distinguishable by one
measurement that must be run **before anything is changed**:

- **(a) GEOMETRY** — bench/riser structure fades out before it reaches the hem,
  so there is nothing there to see.
- **(b) SPLAT** — the structure exists and is painted **one material**, so a
  riser and a bench are the same green and their value separation is zero
  whatever the geometry does.

**The probe:** sample terrain height along a line running from the hem up the
flank and look for the step signature; independently sample material ID along
the same line. Steps present + material constant ⇒ (b). Steps absent ⇒ (a).

##### My prior is (b), and it rests on two things already written down, not on speculation

1. **`ROCK_STRATUM_PERIOD/PALE_FRAC` are marked НЕ ПОСТРОЕНО in NUMBERS.md and
   have no consumer in the engine.** The material half of §4.1's banding has
   never existed anywhere in the world. Checked, not assumed.
2. **`MASSIF_ASPECT_MIN`'s own note already measured this failure and named it.**
   It records Ravenscar at 115 m of relief over 180 m of radius — **mean slope
   33°, below `SLOPE_ROCK_MIN` = 40°** — and concludes, in its own words, that
   «материал нарисует травяной холм, что бы ни делала геометрия… правило формы и
   правило раскраски обязаны сойтись, иначе гора проигрывает спор шейдеру».
   With `SLOPE_GRASS_MAX` = 30°, **the hem — the shallowest part of the massif —
   is the region most certainly painted pure grass.** The rhythm dies exactly
   where the slope rule says it must.

**This is the third occurrence of one lesson** (whole-massif aspect, then the
summit, now the hem): *a shape rule and a paint rule that disagree are settled by
the shader, always.* Recording it as recurrence rather than as news, because the
first two times it was written as a local finding and it clearly is not local.

##### The ruling, which holds under either diagnosis

> **A stratum that only appears above a slope threshold is not a stratum, it is a
> slope shader.** §4.1 defines the strata in **absolute world height, globally**,
> precisely so that they do not depend on local geometry. That contract is
> violated the moment the band is visible only where the ground happens to be
> steeper than 40°.
>
> **RULING: the stratum's value modulation applies to the ground ramp at the
> same absolute heights whatever material is painted there** — the band crosses
> the grass at the hem exactly as it crosses the rock above it.

This is not a hack to pass a test. It is what bedrock under thin soil looks
like, and it is in the reference set: frame 06's bedded shelves run down into the
water and stay bedded at low angles; frame 03's forest floor is *mostly* bedrock
with soil in the pockets. **A band that stops at a slope contour reads as paint;
a band that crosses materials at one elevation reads as geology** — which is
§4.1's own argument, applied to the axis it had not been applied to.

##### H2's fix and B2's brief are the same work

> **The hem of the massif is the largest rock-outcrop site in the world, and
> §10.5 B2 already specifies it.** B2's anchor rule places outcrops on convex
> curvature — ridge shoulders, spur noses, scarp lips — which is exactly the
> hem-to-flank transition. B2's slab sub-form *is* «bedrock with soil in
> pockets», and its hard-rim clause is what puts a shadow line back into a
> surface the splat rule had flattened.

**No new numbers requested.** `ROCK_STRATUM_*` exist and are unbuilt; B2's
constants are approved as of this stage. The gap here was never a missing value —
it was a rule about which materials the existing value applies to.

**New acceptance frame, added to §10.8:**

| # | ref | our standpoint | what would make it FAIL |
|---|---|---|---|
| **A6** | **06** | the massif hem at the range that puts the lowest three band pairs in frame, raking light | the lowest band pair reads as one value; the banding stops at a slope contour rather than at an elevation; the hem is one uninterrupted material |

#### 10.10.2 H1 — re-derived on p05, with the control known, and it becomes a BUDGET

**Two errors, both mine, and they are different errors.**

- **I set 2.00 without ever seeing its control.** The no-air frame reads 2.36, so
  I had left aerial perspective a budget of **0.36 shade steps for its entire
  existence.** This document's own standard — «a generator input must never equal
  the floor of the invariant that checks it» — applies to a threshold and its
  control just as much as to a generator and its test, and I broke it.
- **A hard minimum over 105 columns of a 640×360 frame is ONE PIXEL COLUMN.** A
  threshold evaluated at the instrument's own resolution has no slack by
  construction. Accepted, and generalised below rather than patched here.

##### The statistic changes first, and for every column-wise criterion, not just this one

> **`ACCEPTANCE_PERCENTILE` = 5.** Every column-wise or sample-wise acceptance
> statistic in this document is read at **p05**, never at the hard extremum. A
> hard extremum over N samples is a single sample, and a single sample at the
> instrument's resolution is noise wearing a threshold's clothes.

It is one row in NUMBERS.md rather than a convention in prose because design's
threshold and render's measurement have to meet on the same statistic (Rule 35) —
the same reason `PALETTE_SHADE_STEP_REF` was frozen.

##### H1 restated as two lines, because it was carrying two jobs

**Line 1 — the hard floor, and it is the quantiser, not a taste:**

> p05 of |body − adjacent sky| ≥ **one `PALETTE_SHADE_STEP_REF`**.

Below one step the outline and the sky can quantise into the same palette entry
and the silhouette is *gone*, not merely soft. No arm is near this; it is
recorded as the line that must never be approached. **Deliberately not a new
constant** — it is `PALETTE_SHADE_STEP_REF` × 1, and a row for it would be a
Rule 39 shadow copy, the same call as `TOWER_MINOR_DIM_PER_DISTANCE`.

**Line 2 — the budget, which is the line that actually binds:**

> **`HAZE_SILHOUETTE_RETENTION_MIN` = 2/3.**
> retention = p05(with air) / p05(no air) ≥ 0.667, at the landmark's own
> `d_accept`.

**Derived, then checked — in that order.** The derivation: at the distance where
we certify a landmark's *shape*, more of what the frame shows must be the subject
than is the atmosphere. Retention of 2/3 is exactly the point where surviving
contrast is 2× the contrast haze consumed. **The 2× is not a new constant** — it
is the same legibility unit §10.9.1 used for the onset ratio, which is the reason
to prefer it over any other round fraction.

The check, run afterwards:

| arm | p05 | retention | budget |
|---|---|---|---|
| control, no air | 2.77 | 1.000 | — |
| A (L=1400) | 2.25 | 0.812 | pass |
| **C (L=600, shipped)** | **1.96** | **0.708** | **pass** |
| B | 1.69 | 0.610 | **fail** |

**Three things I will not paper over:**

- **C clears the budget by 6%, which is thin, and the thinness is information
  rather than an embarrassment.** It says the shipped arm sits near the edge of
  what the budget permits — worth knowing, and not a reason to move the budget
  to make it comfortable.
- **Fixing H2 will change H1's control, and H1 must then be re-measured.**
  §4.1's strata are global and absolute, so building them adds value structure to
  every rock face including the crown that H1 measures. The denominator moves,
  therefore the retention moves. **Stated now as a prediction so it is not
  discovered as a surprise.**
- **RULE 34 FLAG, and I am not ratifying past it: I do not know the range these
  p05 figures were shot at.** H1 is defined at Ravenscar's `d_accept` = 360 m.
  The lowland frames quoted elsewhere are 900 m. If these figures come from 900 m
  they are a **diagnostic** for H1 and not a verdict on it (§1.6), the budget
  above stands as written but has not yet been evaluated, and H1 at 360 m has
  materially more headroom than the table suggests. **One line from render closes
  this; nothing else depends on it.** I am flagging rather than assuming because
  the last time a distance travelled between sections unchecked it cost us the
  entire 1400/600 argument.

#### 10.10.3 H3 — RETIRED, and not as a demotion: it was §10.9.1 wearing a second hat

The lead is right that it gates nothing — no LR exists in the generator and no
`LR_` row is read — and that **a threshold nothing can fail is Rule 30's exact
defect.** But «keep it as intent» and «drop it» are both wrong, because the
content is neither aspirational nor disposable. It is **duplicated**.

§10.9.1 already rules that the LR is sited at or beyond
`d_onset = d_accept(L0) + L·ln 2`, defined as the distance at which the far
landmark retains **half** the near one's contrast. H3 asked for **1.7×**. Any
landmark sited beyond `d_onset` satisfies H3 **by construction**, with margin:

| | | |
|---|---|---|
| `d_onset` at L = 600 | 360 + 600·ln2 | **776 m** |
| LR nearest legal siting | §2.5 | 1400 m |
| ratio actually delivered there | 0.55 / 0.097 | **5.7×**, against H3's 1.7× |

> **RULING: H3 is retired as an acceptance proposition. Its content lives
> entirely inside `d_onset`, and keeping both is a Rule 39 shadow copy — two
> statements of one requirement that will drift the first time either is
> edited.**

**What replaces it is stronger, not weaker.** H3 needed a camera and an LR that
does not exist. `d_onset` needs neither: **«is the LR sited at or beyond
`d_onset`?» is a placement assertion checkable in the generator the moment the LR
lands**, with no frame and no measurement. A requirement that moved from
«unshootable» to «checkable at generation time» has been improved by being
deleted, which is the outcome I would want from every retirement.

#### 10.10.4 The frame-2 vantage — accepted, and the tabled coordinate has now rotted three times

**(581,344) accepted**, same 287 m, same hour. Render re-derived it correctly.

**But the point is not that the coordinate was wrong; it is that it was a
coordinate.** §7.1b's own rule is that acceptance vantages are **derived, never
tabled**, and (545,165) is a tabled coordinate that a later flora pass grew a
pine stand across. That is the third instance of one failure — the 717 m frame's
bearing, the water-adjacent placements, now this — and a rule broken three times
in its own document is a rule that needs its predicates written down so nobody
has to remember it:

> **Frame 2's standpoint is re-derived on every worldgen run from four
> predicates, and stored nowhere:**
> 1. **Range** = the clause's distance, not the frame's (§1.6.1). The band-pair
>    clause is metric — a 28 m pair reads to ~840 m — so 287 m is generous and
>    the range is not the binding predicate.
> 2. **Canopy** — transmittance along the ray ≥ `CANOPY_VISIBILITY_MIN` = 0.25
>    (§1.3's Beer-Lambert rule). This is the predicate (545,165) failed, and it
>    failed it *later*, which is why the check has to run per-worldgen.
> 3. **Bearing** — outside the castle sector (§7.1b, unchanged: inside 300 m
>    §6.1.1 lets the castle fill the view and the frame would be testing the
>    castle).
> 4. **NEW — the frame must contain the LOWEST band pair.** §10.10.1 makes the
>    hem the subject, and §10.9.4 established that the property varies with
>    elevation. A frame showing only mid-flank bands would report a pass with the
>    failure sitting below the bottom edge. **F7 in the vertical axis, and it is
>    now this frame's binding predicate.**

Predicate 4 may well disqualify (581,344) too — I do not know whether it sees the
hem. **That is render's measurement to take, and the point of writing predicates
instead of a coordinate is that the answer no longer requires me.**

#### 10.10.5 A1 ALREADY HAS ITS «BEFORE» FRAME, AND IT WAS SHOT FOR ANOTHER QUESTION

The lead looked at `render-haze-lowland-900m-A` and `-C` and described the ground
as a flat green plane with a visible repeating smoothing pattern, sprinkled with
identical pebbles, a palisade of identical trees on the horizon, and **nothing at
all between the near grass and the lone conical mountain.**

> **Those two frames ARE §10.8 A1's before-state.** They are a counterfactual arm
> that already exists, taken by another zone for another question, which makes
> them better evidence than a before-frame shot on purpose — nobody composed them
> to make the after look good. **Archive them into `docs/acceptance/` labelled as
> A1's before-state**, and A1's pairing under Rule 27 is satisfied without
> re-shooting anything.

**They fail three separately approved criteria, and naming which ones is what
turns an impression into an acceptance record:**

| what the lead saw | criterion it fails | owner |
|---|---|---|
| nothing between near grass and the mountain | **§10.4.1 `MIDGROUND_OBJECT_COUNT_MIN` = 5** | design → core, step 1 |
| flat green plane, visible repeating smoothing pattern | **R5 — no readable tile at any scale**, and §10.1's σ floor | render (colour) + core (relief) |
| identical pebbles | **B1 `BOULDER_SIZE_RATIO_MIN` = 1.6** — no two neighbours in a cluster the same size | core, step 1 |
| palisade of identical trees | flora's variation problem, not mine — **named so it is not silently absorbed into step 1's scope** | flora |

**§10.4.1 was a claim about a frame nobody had taken. It has now been seen in a
frame taken for an unrelated purpose, by a zone that was not looking for it.**
That is the strongest form the confirmation could have arrived in, and it is why
step 1 goes to outcrops and boulders rather than to more octaves: the missing
thing is object silhouettes in the mid field, and the frame says so directly.

**A1 is ready to shoot the moment core's first placement pass lands**, on the
same flat ground, from the same standpoint if render can reproduce it — same
standpoint turns A1 from a pair of frames into a controlled comparison, and the
before-frame already exists.

##### Numbers this section asks for (Rule 35, via lead)

| constant | proposed | unit | second zone |
|---|---|---|---|
| `HAZE_SILHOUETTE_RETENTION_MIN` | 0.667 | fraction of no-air p05 | render (it measures; design sets the floor) |
| `ACCEPTANCE_PERCENTILE` | 5 | percentile | render — governs **every** column-wise acceptance statistic, not only H1 |

Retired rather than added: **H3** (subsumed by `d_onset`, §10.10.3), and the
one-shade-step hard floor of §10.10.2, which is `PALETTE_SHADE_STEP_REF` × 1 and
must not become a row of its own.

---

