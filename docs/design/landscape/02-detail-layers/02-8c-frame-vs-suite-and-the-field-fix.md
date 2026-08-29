
#### 2.8.7 THE FRAME REFUTED THE SUITE — nine invariants, none of which can see

**Seven of eight invariants passed and the mountain is still a dome. I have
looked at the frame myself and I confirm it independently of render and the
lead: it is a low, smooth, convex arc with a grey cap and a green shoulder.
Zero crest lines, not the three §7.1 requires. One material band, not a
rhythm.** Per §7.1's standing clause the frames outrank the numbers, so **the
"7 of 8" status is WITHDRAWN as a description of the mountain.** It remains
true as a description of the tests, which is now the problem.

**The lighting excuse is dead, and I record that I reached for it first.** My
first hypothesis was that a backlit frame flattens every internal structure
into one dark mass and that the read was confounded. The frame refutes it:
there is ample illumination and clear grey-on-green material separation. The
geometry is a smooth hump. **Had I ruled from render's prose instead of
opening the image, I would have sent back a lighting question and cost the
project another round.** Look at the artefact — the same rule that governs
perimeter digitisers and foliage bounding boxes governs me.

**THE SYSTEMATIC DEFECT: all nine invariants measure the OBJECT; none measures
the VIEW.** Contours, slope histograms, aspect turns along arcs, radial
profiles, perimeter ratios — every one is computed on the heightfield from
above or around it. **Not one of them is evaluated from a camera at
`PLAYER_EYE_HEIGHT`.** Meanwhile §7.1's acceptance criterion has always been a
frame. So the suite and the acceptance test were written **in different
spaces**, and a shape can satisfy every member of one while failing the other.
That is not a bad threshold anywhere; it is a missing dimension everywhere,
and it explains the whole discrepancy without any individual invariant being
wrong.

**Two new invariants. The first is the one I most regret not having.**

- **I10 — MASSIF ASPECT (scale, which every other invariant is blind to).**
  Every existing invariant is **scale-free** — ratios, angles, distributions,
  normalised perimeters. All nine are satisfiable on a pancake, and Ravenscar
  is a pancake: **115 m of relief over a 180 m base radius**, a mean envelope
  slope of **≈ 33°**. Rule: **above `MASSIF_CLIFFLINE_FRAC`, the massif's mean
  envelope slope (relief over radial run) must reach `SLOPE_ROCK_MIN`.**
  - **The derivation, so this is not a number I invented:** §4 paints rock at
    ≥ 40° and grass below it. A massif whose envelope sits under that
    threshold **will be painted as a grassy hill by the material system no
    matter what its geometry does** — and that is precisely the frame: a green
    shoulder with one grey cap. The shape rule and the splat rule must agree,
    or the mountain loses the argument to the shader.
  - **RULED, because the constant and my text disagreed by 8° (core caught it
    before it became a third round): I10 is measured FROM THE CLIFFLINE
    CONTOUR TO THE SUMMIT, and it is an ENVELOPE measure, never a surface
    mean.** NUMBERS.md defines `MASSIF_ASPECT_MIN` over the whole cone; my text
    said above the cliffline; those are different mountains. **My text wins and
    the constant's definition is corrected to match**, because I10's whole
    derivation is §4's rock threshold — and a grassy *apron* is not a defect,
    it is what §2.8.2's `p > 1` profile is for. Talus fans out. What must reach
    the rock threshold is the body the eye reads as mountain, which begins at
    the cliffline. **Envelope, not surface mean, for the reason I1 has just
    taught us the hard way** (below): a surface average over benches and risers
    can be steep while the outline bulges.
  - **SCOPE — I10 constrains the massif ABOVE the cliffline, and the apron
    below it may still flare.** Stated explicitly because the naive reading
    (115 m of relief over a total base radius under ≈ 137 m) is both more
    destructive than intended and geologically wrong: §2.8.2's `p > 1` profile
    exists precisely because **talus fans out at the bottom**. The upper cone
    must reach the rock threshold; the hem may lie where it lies. This
    materially shrinks the cascade below.
  - **Consequence, predicted not assumed — core measures it.** The upper
    radius comes in; how far the hem follows is a measurement, not an
    inference. Cascades onto the barrow (radius 103 m), the castle spur, the
    pine strips and the ascent length, re-validated per §7.0a's rule.
  - **THE CASTLE IS THE PLACEMENT AT RISK, and my earlier prediction about it
    is void.** §2.8.5 reasoned that R1 was safe because "the base radius is
    unchanged (180 m)". It is changing now, so that reasoning expires with it.
    The Ward sits ≈ 148 m from the crag centre: **if the hem contracts far
    enough, the castle is no longer on a spur of the massif at all but on flat
    ground beside it**, which fails §6.1.2's siting rule outright and — story's
    condition, and they are right to make it one — shrinks the angular
    envelope that keeps the fortress reading **against rock rather than
    against sky**. That envelope is the mechanism protecting the tower's
    skyline monopoly, which is the arc's central image. **Re-measure R1–R4 and
    the castle's ground from the valley standpoints after the reshape; do not
    assume 115 m of relief still buys the margin it bought at 180 m of
    radius.**
  - **THE APRON FLARE IS THE RELEASE VALVE — spend it before moving the
    castle.** Since I10 constrains only the body above the cliffline, the hem
    is free to stay out where it is, and **preserving the Ward's spur is a
    legitimate reason to let it.** Order of preference, binding on the
    re-siting pass: (1) flare the apron so the spur survives; (2) stop the
    contraction; (3) move the castle — **and (3) requires a story consult
    BEFORE it lands, not after.**
  - **Why moving the castle is the expensive option, and it is an asymmetry
    worth understanding rather than a preference (story's catch).** When the
    *barrow* moves, its satellites follow for free: the ward gap and the
    barrow-facing tower are **defined relative to the barrow** (§7.0a). When
    the *castle* moves, nothing follows — the ≈ 55 m barrow proximity, the
    yard/gate→barrow sightline and the act-1 trespass route are each defined
    between the castle and something that is **not moving with it**, so all
    three break independently. **A landmark whose dependents are defined
    relative to it is cheap to move; a landmark that is itself the fixed end
    of other relations is expensive.** That distinction should be checked
    before relocating anything in this document, not only here.
  - **The LR is worse and is the cheap one to fix.** 280 m over a 600–700 m
    base radius is an envelope of **≈ 23°** — flatter than Ravenscar. It does
    not exist yet, so fixing `LR_BASE_RADIUS` now costs nothing, and building
    it first would have produced this same session a third time.
- **I11 — SILHOUETTE BREAKS (the eye's test, made executable).** §7.1 has
  always demanded "at least three crest lines readable at 640×360" and that
  criterion **was never implemented** — it sat in prose while nine other
  criteria ran in code. Rule: from standpoints on a ring at the acceptance
  distance, extract the massif's **horizon polyline against sky** and require
  at least three **tangent breaks** in it, each subtending at least
  `SILHOUETTE_MIN_PX`. This is the only invariant in the suite computed from a
  camera, and it is the one that would have failed on day one.
  - **I11 AS I FIRST WROTE IT WAS VACUOUS, AND I WROTE IT ONE MESSAGE AFTER
    DIAGNOSING THIS EXACT DEFECT TWICE (core's control, ruled).** A smooth
    analytic cone scores **exactly 3** at every standpoint and every distance —
    apex plus the two hem junctions — so `MASSIF_SILHOUETTE_BREAKS_MIN` = 3 was
    **satisfied by the dome the invariant exists to reject.** I had just
    finished withdrawing contour-CV and footprint-I3 for precisely this, and
    then built a third one. That is why the control rule above is now standing
    procedure rather than advice.
  - **RULING — count breaks on the INTERIOR of the horizon only**, excluding
    the apex and the two hem junctions: crest lines that meet sky *between* the
    outline's endpoints, which is what §7.1's "three readable crest lines"
    always meant. Under this reading the cone scores **0** and the reshaped
    massif scores **4–11** by standpoint. The reject case now fails at every
    threshold, which is the property the first version lacked.
  - **RULING — a break counts at ≥ 20° of tangent turn**, and the bracket is
    reasoned rather than picked off core's curve. Above: `MASSIF_ARETE_TURN_MIN`
    is 50° for a plan-view *aspect* turn, and a rib of given aspect turn always
    projects to a *smaller* tangent break in silhouette, so the silhouette
    threshold must sit below 50°. Below: core measured the noise floor around
    10–15°, where counts stop responding to the threshold. That brackets
    20–30°, and 20° takes the wider margin — 6 breaks against a floor of 3 at
    400 m, rather than 4 against 3 at 30°, which would violate the
    never-equal-the-floor spacing this document just adopted. **Provisional and
    the frames outrank it**, exactly as with `CROWN_ASPECT_MAX`.
  - **Why plan-view aspect turn (I7) does not imply a visible rib.** I7 finds
    four arêtes and the eye finds none, and both are correct. With only 3–5
    arêtes, a rib lies near the **limb** for a minority of bearings; from most
    viewpoints the outline is traced by a **facet**, whose profile is the
    smooth curve. So ribs read as **value structure on the body**, not as
    silhouette — except where a break is large enough to notch the outline.
    I7 measures a property the eye cannot see from the ground; I11 measures
    the property it can.
  - **BOTH OF MY SUSPECTED CAUSES ARE FALSIFIED, and I withdraw them as
    readily as I withdrew the lobing mechanism (core measured).** Radial
    excursion is **87.9 / 76.3 / 63.4 / 34.5 m** at the four levels against a
    13.3 m readable scale at 400 m — **the lobes are three to six times larger
    than they need to be**, so `MASSIF_RADIAL_LOBE_AMP` is delivering and the
    built surface is expressing it. Do not touch it. And the tightest corner
    radius of curvature is **2.8 m** against that same 13.3 m — the support
    polygon's corners are sharper than the eye can resolve. I7 is not passing
    on a rounded corner. **The ribs are big and sharp and still invisible**,
    which kills the two comfortable explanations and leaves the real one.
  - **Superseded — the original text of this bullet is kept below for the
    record.** Suspected second cause, to be measured before anything is tuned:
    §1.5 puts the readable feature size at ≈ distance/30, so an arête must
    stand **≈ 13 m proud at 400 m** and ≈ 24 m at 717 m. On paper
    `MASSIF_RADIAL_LOBE_AMP` should deliver that; whether the built surface
    does is unknown, because I8 reports a normalised perimeter ratio and
    **nobody has measured the raw radial excursion in metres.** A lobe ratio
    of 1.36 says nothing about whether the lobes are visible.

**I1 IS MEASURING THE WRONG THING, AND IT IS THE INVARIANT I CALLED "THE CORE
ANTI-DOME TEST" (core, measured — the single most important finding of the
stage).** The built outline's slope, summit outward, is
**20.1 / 18.2 / 20.9 / 20.9 / 31.8 / 23.9 / 30.0°** — **shallowest at the
summit, steepest at the foot**, the exact inverse of a concave profile and the
textbook dome signature. The silhouette bulges above a straight cone by up to
14.8 m at every radius. Meanwhile **I1 passes at 12.7–19.4°** on the same
mountain.

Both numbers are correct. I1 averages **surface** slope over the upper versus
lower third, and on a banded massif that average is set by **benches and
risers** — the sawtooth texture — not by the form the sawtooth sits on. So:

- **RULING: I1 is re-specified as an ENVELOPE measure.** It compares the
  *outline's* slope over the upper third against the lower third, not a mean
  of surface normals. Same threshold, same intent, correct basis. I10 is
  written the same way for the same reason.
- **The general rule, and it is core's sentence: AVERAGING OVER A SURFACE
  HIDES THE SHAPE OF ITS ENVELOPE.** A staircase of any overall form has the
  same mean tread-and-riser slope. Any invariant meant to constrain *form*
  must measure the envelope; surface means may only constrain *texture* —
  which is exactly what I3, I4 and I5 do, and exactly why those four are the
  ones robust across all twelve seeds.
- **And the sharpest version, because it is the one that will recur: A MODEL
  CHANGE CAN INVALIDATE AN INVARIANT'S MEASUREMENT BASIS WITHOUT CHANGING ITS
  NUMBER.** Before §2.8.2 the massif was smooth, so surface mean and envelope
  agreed and I1 was sound. Adding benches and risers decoupled them. **The
  feature that fixed I3, I4 and I5 silently broke I1's validity, and I1 kept
  reporting a healthy number throughout.** Nobody introduced a bug. When the
  model changes, every invariant's *basis* is re-opened, not just its value.

**THE SUMMIT TOR IS INVISIBLE, AND I CERTIFIED IT WITH AN INVARIANT I RULED
MYSELF.** Core disabled the tor entirely and re-measured the outline: identical
to the decimal, including the innermost band. At `SUMMIT_TOR_RADIUS` 5–10 m on
a ~190 m massif it hides inside the cone tip and cannot be resolved from any
acceptance distance. It passes I2 at 52.9° **only because I ruled I2
surface-area weighted**, and near-vertical slab sides dominate that average —
so we built an invariant that certifies a summit feature the camera cannot
see. Core calls this partly theirs for asking; it is mine for ruling it.

- **The weighting ruling still stands for I3 and I4** — the limit argument is
  untouched, and a plan-weighted I2 would still reject a tor outright.
- **RULING: the tor is SIZED AGAINST THE ACCEPTANCE DISTANCE, not against
  taste.** Its silhouette must clear `SILHOUETTE_MIN_PX` from §7.1b's frames —
  ≈ 13 m at 400 m, ≈ 24 m at 717 m — which makes `SUMMIT_TOR_RADIUS` a
  *derived* quantity, naturally landing near `MASSIF_SUMMIT_RADIUS_FRAC` of the
  base radius. **"The summit IS a tor" was always the ruling; a 5 m ornament
  on a 190 m mountain was never it.**
- **I2 means nothing until I11 runs.** Recorded as a dependency, not a
  criticism of I2: a summit-sharpness test with no camera in it can be
  satisfied by geometry no camera receives.

**THE SAME DEFECT EXISTS IN EVERY ZONE, AND STORY NAMED IT BETTER THAN I DID
— carried here in their words for the sync.** Their equivalent of nine
invariants that measure the object and never the view is **canon that is true
in the document and never checked from where the player stands**. Same defect,
two zones, and both of us found it the same way: *by someone finally looking
at the thing rather than at the numbers about the thing.*

The pair worth putting in front of the team, because each is the other's
proof:

1. **A test that measures the artefact instead of the experience.** Core
   disabled the summit tor entirely and the outline was identical to the
   decimal, while my own invariant scored that summit at 52.9° against a 40°
   floor. That is not a weak test — **it was measuring something the view
   cannot contain.**
2. **Canon that is true on the page and unverified from the ground.** Story's
   barrow-visibility condition is exactly that class, which is why they keep
   restating it as a refusal rather than a preference.

**Both pass right up until someone looks.** The counter-measure is not better
thresholds; it is that **every zone needs at least one criterion evaluated
from the player's position**, and that criterion outranks the rest of its
suite. I11 is terrain's. Story's is a raycast from Vaelmere.

**I11 WORKS, AND IT REPRODUCES THE USER'S COMPLAINT EXACTLY (core, measured on
the fully fixed build).** Interior breaks at 20°, floor 3, cone control reading
0 at every standpoint:

| Distance | Breaks by bearing | Verdict | Readable units (§1.6.1) |
|---|---|---|---|
| 253 m | 9, 9, 12, 10 | passes 3–4× over | 28 |
| 300 m | 5, 8, 12, 4 | passes from every bearing | 24 |
| **360 m — Ravenscar's DERIVED acceptance distance** | | | **20** |
| 400 m | 1, 4, 5, 4 | fails from one | 18 |
| **600 m** | **1, 1, 0, 0** | **fails from every bearing** | **12** |

> **THE 600 m ROW IS NOT A VERDICT ON RAVENSCAR — §1.6.1.** 600 m was written
> for the LR (base radius 260–310 m), which **does not exist in the generator**
> (§1.6.3). Applied to a 120 m crag it asks a six-feature test to fit in twelve
> readable units. The 300 → 400 → 600 decay everyone read as a shape failure is
> substantially an **angular-size curve**: a geometrically perfect mountain
> produces the same shape of curve. Ravenscar's acceptance distance is
> **360 m**, and the rows above it pass.
>
> **What that does NOT excuse, and it is the part to carry forward:** the decay
> is steeper than the budget alone predicts (12 → 2 breaks for a 2× range, once
> the fixed guard cost is taken out), and **the frame I ruled a dome from was
> shot at 400 m — inside the budget** (§2.8.8). The budget explains the
> *invariant*. It does not explain the *complaint*.

**~~The massif reads as broken rock up close and as a smooth mass from the
valley.~~ — THIS SENTENCE IS WITHDRAWN AS A FINDING ABOUT THE MOUNTAIN
(§1.6.1, §2.8.8).** It was produced by measuring a 120 m crag at a distance
written for a 280 m mountain that was never generated. It remains an accurate
description of *the user's complaint*, which is why it was so persuasive, and
that complaint is still open. Retained below as written, because the reasoning
it carried about spaces and cameras is sound and only its subject was wrong.

That was the sentence the user has been saying for four sessions,
produced by the only invariant computed from a camera — while nine object-space
invariants report a healthy mountain on the same build (I1 47.3°, I2 70.2°,
I3 67.9 %, I10 1.46, all robust across twelve seeds). **This is the §2.8.7
thesis measured rather than argued**, and it is worth stating that the frame
and the camera-side invariant agree with each other and disagree with
everything else. The model answer is in §2.8.2: the facets are not flat, which
is simultaneously why I7 has never passed and why breaks wash out with
distance.

**What this costs me, said plainly.** I wrote nine invariants, ruled on their
weightings twice, corrected their baseline, added a marginal-pass rule, and
none of that was worth as much as one screenshot. The invariants were not
useless — I1 through I8 each caught something real, and the mountain is
genuinely better than it was. But **they were a proxy for a judgement, and I
let the proxy accumulate authority it had not earned**, to the point where "7
of 8" was being relayed upward as the state of the world. The rule that saved
it was one I inherited and nearly did not honour: *the frames outrank the
numbers.* It only works if someone actually shoots the frame early, and this
suite ran for two sessions before anyone did.

#### 2.8.8 AFTER THE FIELD FIX — the frame attributed, two constants re-derived, I7 repaired

Everything in §2.8.7 above was reasoned on a build with a broken per-bearing
field and an inverted profile stamp. This section says which of it survives.

##### THE DOME FRAME IS ATTRIBUTED, AND IT INDICTED A BUG THAT IS NOW FIXED

The lead asked which landmark was in the frame I called a dome and at what
range. **It was recorded, and I did not have to reconstruct it:**

> `screenshots/massif/02_massif_verdict_400m_diagnostic.png`, shot
> 09:08:2026 21:14, **L0 Ravenscar at 400 m**, on frame 1's verdict bearing —
> render's parked vantage (120, 300) walked in along the same line, so the eye
> sits ≈ (434, 256), roughly due west of the peak and slightly south. Backlit,
> `DFN_TIME` 0.30. My verdict on it is the UPD entry at 21:21.

Three things follow, and the first two are mine to own:

1. **400 m is 18 readable units — inside Ravenscar's budget, not outside it**
   (§1.6.1 puts d_accept at 360 m; 400 m is one bearing's worth beyond it, and
   I11 duly failed from exactly one bearing at 400 m). **So the budget does not
   excuse that frame.** I looked at a mountain from very nearly its own
   acceptance distance and saw a dome. Recorded plainly because the comfortable
   reading — «we photographed it from too far away» — is available and is
   **false for this frame.**
2. **The frame predates the profile-clipping bug fix by roughly half an hour.**
   At 21:14 the stamp still computed `h = H·(1−t)^p`, decaying to zero at the
   rim rather than to the valley floor, so the entire concave tail sat below
   base terrain and was discarded by the `max()`. Core's own description of the
   consequence is «precisely why the envelope measured shallowest at the summit
   and steepest at the foot» — **which is the textbook dome signature.** The
   frame was right. It was a picture of a bug, and the bug was fixed at ≈ 21:41.
3. **Therefore MY dome verdict is CLOSED and the USER's complaint is NOT.** The
   frame is attributed to a defect that no longer exists; that closes the frame
   and nothing else. The user said «гора — это всё ещё сиська» while *playing*,
   from wherever he was standing, four sessions running, and no frame has been
   shot since **relief +19 m, base radius 180 → 120, the profile-clipping fix,
   faceted couloirs, arête count 4 → 3, and a noise field that had been
   returning a third of its range.** Nobody has looked at this mountain since
   any of that landed. **The complaint stays open until a frame closes it, and
   an explanation that dissolves the measurement while leaving the complaint
   standing is exactly the move this project keeps making.**

##### RULING — I7 MEASURES RIDGE PERSISTENCE, NOT RIDGE COUNT

I7 requires ≥ 3 detected arêtes and the massif now has exactly 3. My own rule
from §2.8.6 forbids that outright: *a generator input must never equal the
floor of the invariant that checks it.* Worse than a missing margin — **I7 was
measuring an input.** `L0_ARETE_COUNT` is a number design hands the generator;
counting it back out and comparing it to a floor checks the detector, not the
mountain. That is why it has never passed and could never have passed
informatively.

**RULING, and it is what «persistent arêtes» meant before the test turned it
into a count:**

- **I7's measured quantity is DESCENT DEPTH** — for each detected crest, the
  span of relief over which it survives continuously before the flank swallows
  it. Sampled over eight levels across the banded zone (cliffline → summit),
  not four.
- **`MASSIF_ARETE_DESCENT_MIN` = 0.50 of relief (предложение — утвердить),
  derived, not chosen.** A rib must structure the part of the outline the eye
  actually reads. The eye reads from the hem up; below `MASSIF_CLIFFLINE_FRAC`
  = 0.33 §2.8.7 explicitly permits a grassy apron and ribs are *supposed* to
  die into talus. So a rib must run from near the summit (≥ 0.85) down to the
  cliffline (0.33) — a span of 0.52, rounded to 0.50.
- **The count clause survives only as a guard against the coincidence, and it
  is a FRACTION: ≥ ⌈2/3 · `L0_ARETE_COUNT`⌉**, i.e. 2 of 3 today. Core measured
  that the detector reliably finds 2 of 3 corners, and this is the rare case
  where accepting a measured detector limit is not accommodation — **because
  the substance moved to persistence, the count is no longer what the test is
  about.** Had I lowered the floor and kept counting, that would have been the
  accommodation this document has refused twice.
- **Both controls, per Rule 30 and the lead's corollary.** *Must fail:* a
  smooth cone (zero detections at any level, descent depth undefined) — and,
  the case the old I7 could not reject, **a smooth cone with a faceted cap**,
  whose corners appear at 0.85 and are gone by 0.70, descent depth ≈ 0.15.
  That shape is exactly the dome-with-a-sharp-hat the frame kept showing, and
  the count-based I7 would have passed it. *Must be able to pass:* three ribs
  each running summit to cliffline — constructible, and it is what §2.8.2
  already builds when the facets stay flat.
- **Why this is the version that predicts I11.** A rib that dies above 0.55
  leaves the lower two-thirds of the silhouette smooth, and the lower
  two-thirds is most of what a standing eye sees. **Descent depth is the
  object-space quantity whose failure produces I11's failure**; ridge count is
  not. §2.8.2 already ruled that I7 and I11 are one failure — this is the
  version of I7 that makes that true rather than asserted.
- **The §2.8.3 guard from 21:29 is discharged.** I7's sampling levels were
  allowed to be re-scoped only «conditional on I11 existing», because a test
  whose slice elevations I may choose is a test I can always make pass. I11
  exists and is measured from a camera, so the condition is met.

##### THE CONVEXITY CAP PROTECTS NOTHING — THE BAR WAS ALWAYS ON ELONGATION

Asked what the cap protects, so that non-convexity can have a budget rather
than a veto. **The answer is that there is no veto to lift, and I should say so
before anyone spends a build on removing one.**

- **`n·tan(π/n)/π` is not a design rule. It is arithmetic** — the isoperimetric
  ratio of a regular n-gon — and it appeared in this document as a *consequence*
  of core's support-function construction, not as something design imposed. I
  have never ruled that a massif must be convex.
- **What I did bar is ELONGATION as a knob for passing I8**, and that bar has
  nothing to do with convexity: an elongated L0 is a **ridge, not a peak**. It
  stands, unchanged, and core's addition to it is the important part — no
  invariant we have would notice.
- **The model is already non-convex in plan** (couloirs are re-entrant notches
  cut into the hull). **What it cannot express is PROTRUSION**: nothing sticks
  out past the hull far enough to occlude the flank behind it. That is the gap,
  and it is a different word from concavity.

**So protrusion gets a BUDGET, denominated in things this document already
measures rather than in a new taste rule:**

1. **SINGLE SUMMIT — the binding one, and it is standard topography.** §2.8's
   first decoded requirement is «the summit is a point». A spur becomes a
   second mountain when its **prominence** — its height above the highest col
   connecting it to the main summit — gets large. Rule: **no spur's prominence
   may exceed `MASSIF_SPUR_PROMINENCE_MAX` = 0.20 of relief (предложение —
   утвердить)**; on Ravenscar that is 23 m, enough for a spur that occludes and
   far short of a subpeak. Above that the massif reads as a cluster of hills,
   which is precisely what C4's «one unmistakable mass» forbids and what a
   ribbed mountain is not.
2. **C1 VISIBILITY — already measured, no new constant.** Protrusion buys
   occlusion, and occlusion is what C1 counts. Spurs may grow until
   `LANDMARK_VISIBILITY_MIN` = 0.6 binds. Ravenscar currently measures 0.751
   with headroom to spend.
3. **ELONGATION — unchanged, and it is the one hard bar.** A spur programme
   must not become an axis. Corners clustering on a long axis is the failure
   mode; it is a design rule precisely because the suite is blind to it.
4. **The castle spur is a BENEFICIARY, not a casualty.** §2.8.7's release-valve
   ladder wanted the hem to flare so the Ward keeps its spur. Protrusion is
   that, done deliberately.

**No veto. «How much, and where.» And the answer to «where» is: on the flanks
the acceptance frames look at, which for Ravenscar is the south-west
three-quarters** — the crag sits 194 m from the east edge and 200 m from the
north edge of a 1024 m world, so it has no long sightlines from those bearings
at all (§1.6.2).

##### THE TWO TAINTED CONSTANTS, RE-DERIVED

**1. `MASSIF_PROFILE_EXPONENT_MIN` — 1.3 → 1.5, and the rule it belonged to was
wrong on the arithmetic.**

The mapping is `p = MIN + f·(MAX − MIN)` with `f` the per-bearing field, so the
broken field's `f ∈ [0.4, 1.0]` gave `p ∈ [1.66, 2.20]` lumped at 1.84 and
2.02. Nothing below 1.66 has ever been generated.

**First, the finding that matters more than the number: `p` CANNOT PRODUCE THE
ASYMMETRY §2.8.2 CLAIMS TO GET FREE FROM IT.** The profile is
`h = H·(1 − d/R)^p`, which runs from `H` at the centre to 0 at `R` **for every
value of p**. The mean envelope slope is `H/R` = 43.8° regardless. `p` does not
make a flank gentler or steeper; **it only moves where the steepness sits along
the radius** — high `p` gives a steep cap over a gentle apron, low `p` gives a
more uniform cone. §2.8.2's «the low-`p` sector is the gentle flank that
carries the ascent» is therefore false in both halves: the low-`p` sector is
the *most uniform* flank, not the gentlest, and it is the one closest to the
constant gradient I4 exists to reject. **The gentle flank has to come from
`R(θ)` — a longer run at that bearing — or from the benches, and §7.1b already
says the benches carry the ascent.** The broken field did not merely hide the
low-`p` half; it hid the fact that the low-`p` half was never going to do the
job it was assigned.

**So nothing pulls `p` downward, and the anti-dome argument pulls it up.**
Derived against I1's own floor (`MASSIF_PROFILE_STEEPENING_MIN` = 12°), with
the envelope basis I1 now uses, at H = 115 m and R = 120 m:

| `p` | upper-third envelope | lower-third envelope | I1 steepening | verdict |
|---|---|---|---|---|
| 1.0 (cone) | 43.8° | 43.8° | **0.0°** | control: a cone must fail, and does |
| 1.2 | 48.1° | 38.6° | 9.5° | **fails I1** |
| **1.3 (current)** | 50.0° | 36.6° | **13.4°** | passes by 1.4° — no margin |
| **1.5 (ruled)** | 53.4° | 33.6° | **19.8°** | 1.65× the floor |
| 1.8 | 57.7° | 30.5° | 27.2° | |
| 2.2 (MAX, unchanged) | 62.2° | 27.8° | 34.4° | |

**RULING: `MASSIF_PROFILE_EXPONENT_MIN` = 1.5.** The old 1.3 sat 1.4° above the
floor of the invariant that checks it — the never-equal-the-floor rule in its
marginal form, and it survived only because the field never generated it.
1.5 takes the same ≈ 1.6× margin §1.6.1 takes. `_MAX` = 2.2 is untainted (it
was always reachable) and unchanged.

**And the bounds are not enough — Rule 31 in full.** The fixed `bearing_field`
is a normalised sum of cosines, which is **bell-shaped about 0.5**, not
uniform: extremes require all harmonics to align. A range of [1.5, 2.2] drawn
from a peaked field concentrates `p` near 1.85 and delivers **little asymmetry
around the mountain**, which is a milder rerun of the same defect — «only the
top 60 %» replaced by «mostly the middle». Design does not get to specify the
noise function, so the requirement is stated on the **outcome**, where it is
checkable and field-agnostic:

> **`MASSIF_PROFILE_ASYMMETRY_MIN` = 10° (предложение — утвердить).** Across
> the 64 radial bearings, the spread between the steepest and shallowest
> per-bearing I1 steepening must reach 10°. A field returning only its middle
> half yields ≈ 7° and fails; the full [1.5, 2.2] range yields ≈ 14.6° and
> passes.

That is an asymmetry rule that is actually about asymmetry, it has a control (a
cone reads 0° of spread), and it cannot be satisfied by a peaked field
pretending to be a spread.

**2. The 20° silhouette break threshold — RE-CONFIRMED, on a different basis,
and it needs a name.**

The original bracket was: below 50° (`MASSIF_ARETE_TURN_MIN`, since a plan-view
aspect turn always projects to a *smaller* silhouette tangent break) and above
core's measured 10–15° noise floor. **The upper bracket is geometry and is
untouched. The lower bracket is withdrawn as a basis** — it was measured on the
broken field, and worse, «where counts stop responding to the threshold» is a
property of the terrain's sub-readable band structure, which is itself drawn
from the field that was broken. Re-deriving instead of re-measuring, so the
number stops depending on it:

- **Perceptual derivation.** At the readability window (1/30 rad ≈ 17.5 px of
  outline at 640×360), a break is a direction change between two ≈ 17.5 px
  segments. Orientation discrimination over segments that long is unambiguous
  well below 20°, so **20° is comfortably above the perceptual floor and below
  the 50° geometric ceiling** — the bracket holds without any measured curve
  in it.
- **20° also keeps the wider margin**, which was the original tie-break and
  still applies: it clears the floor of 3 by a factor rather than by a unit,
  where 30° gave 4-against-3.
- **CONTROL, standing, per Rule 30:** the analytic cone must read 0 at 20° —
  it does — **and the threshold sweep must show counts FALLING as the
  threshold rises.** Counts that do not fall are core's own jitter signature
  and mean the detector is measuring itself, not the mountain. That check is
  now part of the invariant, not a one-off diagnostic.
- **Rule 14 gap, and it is load-bearing: the 20° lives as a literal in the
  probe.** `MASSIF_SILHOUETTE_BREAKS_MIN` gives a count with no magnitude.
  Requested: **`MASSIF_SILHOUETTE_BREAK_TURN_MIN` = 0.35 rad (20°)**.

**3. `MASSIF_SLOPE_BIN_MAX` = 0.30 — STANDS, with its provenance replaced
rather than its value moved.** It was chosen just under the 33.2 % the old dome
scored, and that terrain no longer exists, so the provenance is dead even
though the number is untainted by the field. Re-derived from the landform
instead: a banded scarp puts its risers across three steep bins (≈ 22 % each,
surface-weighted) and its benches across two shallow ones (≈ 17 % each), so the
fullest bin of the intended mountain lands near 22 % — **0.30 rejects the
single uniform flank and is clear of the landform we are building.** Derived
from what the massif is, not from what the dome was. **I4 is not the thing to
relax, and its eight-seed failure is not to be diagnosed until it is
re-measured on the fixed field** — the band-of-angles spread that was supposed
to satisfy I4 was drawing from the broken field and has never actually been
exercised. That is a prediction, and it stays a prediction until core measures.

##### THE SUITE IS NOT A SCOREBOARD — the object-space invariants are NOT retired

Asked to consider seriously whether the object-space invariants measure the
wrong thing entirely, and to retire them if so. **They do not, and the honest
answer is more uncomfortable than retirement would be: they are nine correct
measurements of nine different things, seven of which the user never complained
about.**

- The complaint has two halves: «сиська» — a convex profile — and «рёбра» —
  angular structure. **I1 and I10 measure the first. I7 and I11 measure the
  second.** I3/I4/I5/I6 measure surface texture, which nobody disputed. I2 and
  I8 measure the summit and the plan outline.
- **A perfectly concave, perfectly smooth mountain passes I1, I3, I4, I5, I10
  and reads as a smooth mass.** Concave and smooth are not exclusive. There is
  no contradiction between the suite and the frame to resolve, because there
  never was one.
- **The two invariants that encode the actual complaint are exactly the two
  that fail on every seed.** I7 fails everywhere; I11 fails at the distance it
  was (wrongly) asked about. The suite has been agreeing with the frame all
  evening and being read wrong.
- **So the defect was in the REPORTING FORM, not in the tests.** «Seven of
  eight», «nine of eleven», «six of ten robust» — counting weights every
  invariant equally, when they were authored to protect different things and
  only some of them protect the thing under complaint. **RULING: a suite is
  reported as a list with its LOAD-BEARING member named for the complaint in
  hand, never as a score.** For «сиська» the load-bearing member is I1
  (envelope). For «рёбра» it is I7 (descent depth) and I11 (breaks). Everything
  else is context. This is the same act as §2.8.7's «every zone needs one
  criterion evaluated from the player's position», applied to how the result is
  written down rather than to how it is measured.
- **STANDING DEBT, and it is mine: nine of the ten invariants have no control.**
  Only I11 has ever been run against a shape it must reject. Rule 30 is
  retroactive or it is decoration. Requested of core as one cheap batch — run
  the whole suite against **a smooth analytic cone** and against **a
  `smoothstep` dome**, and publish the table. My predictions, recorded before
  the measurement so they can be wrong: I1, I3, I4, I5, I6, I8 and the repaired
  I7 all reject the cone; **I2 and I10 PASS it** — I2 because a cone's flank
  slope satisfies a summit-slope test that has no summit in it, I10 because it
  is a scale test whose proper reject case is a pancake, not a cone. If those
  two predictions hold, I2 needs a second control and probably needs retiring
  on the §2.8.7 grounds already recorded; if they do not hold, I was wrong
  about which ones are weak and that is worth more than being right.

##### THE I7 / I11 TRADE — no ruling was needed, and the reason generalises

Recorded because the shape of it will recur. Ruling 3 (crest structure sized
against the acceptance distance) was reverted on evidence: I11 read 1,1,0,0
without it and 2,1,2,0 with it — **failing from every bearing either way** —
while it dropped I7 from a first-ever pass to 1. There was never a trade to
adjudicate, because **one arm of it never crossed the bar.** The general form:

> **A trade between two invariants is only a trade if BOTH readings cross their
> thresholds. Two failing numbers moving in opposite directions is not a
> trade-off, it is one regression and one coincidence** — and the way to find
> out is to run the arm nobody ran, which is what the lead asked for and what
> settled it in one measurement.

Second, and it survives the revert: **§2.8.2 ruled that I7 and I11 are ONE
failure. A change that moves its two symptoms in opposite directions has
therefore not touched the mechanism** (Rule 32), whatever it does to either
number. That is the test to apply to the next candidate fix, before it is
measured rather than after.

**Ruling 3's PRINCIPLE is not withdrawn** — detail required to read at distance
is sized against the distance, which is now Rule 33 and is upstream of §1.6.1's
whole derivation. What is withdrawn is the *implementation* it bought, and the
distance it was sized against: **it was sizing crest structure for 600 m, a
range that belongs to a mountain that does not exist.**

