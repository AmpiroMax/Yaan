
### 5.12 THE FOREST WAS EATING THE MOUNTAIN — the apron ruling (stage-4)

Render re-shot the west 300 m frame with scatter suppressed and nothing else
changed. **The geometry is right**: a pointed tor with its tower nub, a concave
left flank carrying visible band lips at about two-thirds height, a long
straight right ridge with a distinct shoulder break, and the castle reading on
its spur. **With the trees on, that mountain is a low featureless hump.** I
opened both frames myself.

**Three candidate levers were put to me. I rule for the second, and the reason
is that it is the only one that addresses the mechanism that actually produces
the dome.**

##### THE MECHANISM, and why only one lever touches it

There are two failures, not one:

1. **THE FOOT IS EATEN.** A mountain missing its bottom third loses the bench
   and the flare of the base, and what survives is the upper cap — **which is
   convex on any mountain whatsoever.** This is the dome. It is not a shape
   defect and no shape change can fix it.
2. **VALUE MERGING.** Canopy and backlit rock land on the same value, so the
   eye does not see trees in front of a mountain; it sees one dark mass whose
   outline is the union of both (§1.3b).

**Failure 1 is the one that produces the user's word.** Hue separation makes
you able to *tell* tree from rock; it does not give you back the bottom third
of the mountain. **Only clearing the foot does.** So:

##### RULING — LEVER 2. AND IT IS NOT A CLEARING, IT IS A LANDFORM

**A massif's apron is talus, scree and scrub. Closed forest does not grow on
it.** The forest standing on Ravenscar's hem was never right; it is a placement
that no rule in this document ever asked for and no rule ever forbade.

- **Measured, so this is a mechanism and not a preference:** the pine annulus
  begins at **140 m** from the crag centre, which is *inside* the 120–162 m hem
  where the massif surface is still climbing. **Pines do not start at the foot;
  they start ON it.** Meanwhile the only treeless rule that exists,
  `on_crag_treeless`, fires **only** at `d < 120 m` **and** `h ≥ 57.5 m` — an
  elevation gate high on the mountain. **The band that is being eaten has no
  rule at all.** The one thing protecting the sightline is the strip duty
  cycle, which is an *angular* gap, not a radial standoff.
- **THE RULE IS DERIVED, NEVER A TABLED RADIUS** — §7.1a's trap, and I have
  fallen into it three times already. **No tree is placed where its canopy top
  would obscure the massif's silhouette below `MASSIF_CLIFFLINE_FRAC` from any
  acceptance standpoint.** That is C1-B (§1.3b) restated as a placement
  predicate, it uses the sight-wedge machinery that already exists, and it
  produces a radius per seed instead of a number in a table.
- **This ADDS content rather than deleting it, which is why it is the ruling
  the landscape actually wants.** The apron is exactly where §5.10's forest
  floor classes belong: scree and boulder fields, big bushes, snags, deadfall,
  and scattered stunted pines that are *below* the cliffline and therefore
  legal. A bare ring would be worse than the forest. **A talus apron with scrub
  and stone is a better landscape than closed pine to the hem, independently of
  any invariant** — real massifs look like that because that is what erosion
  puts there.
- **The ascent (§7.1b) benefits:** a worn watchmen's path across open scree
  reads as a path. Through closed pine it reads as nothing at all.

##### 5.12a SCOPE — the hole core measured in my own sentence (ruled 10.08.2026)

The predicate above says "no tree is placed where its canopy top would obscure
the massif's silhouette below `MASSIF_CLIFFLINE_FRAC` **from any acceptance
standpoint**" — and **it never says WHICH TREES it ranges over.** Core built the
apron (`3106051`, `687f152`), found the hole rather than papering over it, and
measured both readings instead of arguing them:

- **Read GLOBALLY it is a clearcut.** Measured: it excludes every tree within
  **~670 m** of a standpoint, because a tree in front of your face obscures a
  mountain too. **Refused.**
- **Read SCOPED to the massif's own stamp** — which is what LF-4 says ("a
  HEIGHT rule *at the massif foot*") and what core shipped — the apron reaches
  **162 m** at its tightest bearing as a derived output, against a pine annulus
  starting at 140 m. **RATIFIED AS SHIPPED.** It reproduces my measurement
  independently, from the other side, with no tabled radius: pines were
  starting ON the foot.

**But the counterfactual arm is what makes this rulable, and it says the apron
is not the whole answer:**

| vantage | hidden, apron OFF | hidden, apron ON |
|---|---|---|
| 300 m west | 39.5 % | 38.4 % |
| 350 m west | 45.6 % | 38.1 % |
| 500 m west | 42.0 % | 31.0 % |

##### 5.12b THE ACCEPTANCE QUANTITY IS WRONG, AND THAT IS WHY THE MIDDLE LOOKED UNRULABLE

Core offered three candidate middles and declined to invent one. **None of the
three is the answer, because the disagreement is not about where to put a
threshold — it is about what to measure.** Rule 30's mechanical test: if no
value on a quantity separates the accepted cases from the rejected ones, the
QUANTITY is wrong, not the threshold. Apply it here.

**At 300 m the apron moves the number by 1.1 points (39.5 → 38.4).** If
"fraction of the sub-cliffline surface hidden" were the right quantity, the
apron — which demonstrably fixed the real defect, pines standing on the hem —
would have moved it. It did not, at the nearest vantage. And **core's own third
option is correct on its own terms: a third hidden IS what a forested valley
looks like.** Both of those are true at once, and together they convict the
quantity rather than either answer.

**Here is the case the fraction cannot see.** Sixty-nine per cent visible
spread evenly reads as a mountain standing behind a wood. Sixty-nine per cent
visible with the *bottom* uniformly curtained reads as a painted backdrop —
the mountain no longer stands on the same ground the player is standing on. The
fraction is identical in both. **That is the defect the user's word "eating"
names, and it is the same family as §2.8.7: the instrument measures the object
while the acceptance is about the view.**

**RULING — the acceptance moves to the GROUND JUNCTION.**

> Along the massif's angular extent in the valley frame, there must exist a
> contiguous run of **≥ 20 px at 640×360** (= 4.885° at `SKY_ANGULAR_PIXEL`
> 0.0042629 rad/px) over which the lowest visible massif pixel is
> **massif-meeting-ground**, not a canopy edge.
> *Aggregation:* longest contiguous run. *Denominator:* the massif's total
> angular width in that frame, reported alongside so the run can be read as a
> fraction as well as an absolute.

One visible junction is qualitatively different from zero — it is what tells
the eye the mountain rises from this valley rather than hanging behind it — and
20 px is the width at which a run survives the palette quantiser instead of
reading as a gap between two trees.

**THE THRESHOLD IS NOT YET PLACED, AND I AM NOT PLACING IT TODAY.** Both arms
must be measured on the NEW quantity first: apron-OFF is the real rejected
instance (Rule 30 — when a real rejected instance exists, IT is the control),
and 20 px is my derivation of legibility, not a measured separation. If
apron-OFF already clears 20 px, the threshold is too low and the quantity needs
its run-count or its position tightened. **Sizing a threshold before both arms
are measured is the error I ruled against on BR-5 six hours ago; I am not
committing it here.**

**5.12c THE NAMED GAP SURVIVES, RECLASSIFIED.** Core's >15 % / <50 % band is a
good instrument held the right way (two assertions, per §5.11's habit), but it
must stop being called a gap in the *acceptance*, because under 5.12b the
fraction is no longer the acceptance. **It becomes a canary on the apron's own
machinery**: under 15 % the apron has started clearcutting and someone has
widened its scope; over 50 % it has stopped working. Both ends are mechanism
failures, which is what a canary is for and what an acceptance is not.

**5.12d NO MECHANISM IS PICKED TODAY, AND THE WEDGE HAS A COST NOBODY NAMED.**
The foreground corridor along standpoint→massif bearings is the obvious lever
and the machinery exists — but a corridor pointed at a landmark is **a bald
lane through a forest**, one of the most reliable tells of a generated world,
and it would be carved along exactly the bearing the player walks. If it is
ever built it is built as a *thinning with a soft edge*, never a carve-out, and
it is sized only after 5.12b's two arms are measured. **Trigger for revisiting:
the junction-run measurement on both arms, nothing earlier.**

**5.12e THE FRAME IS NOW THE INSTRUMENT, NOT ONLY THE PROOF.** Core reported
honestly that they could not produce the acceptance frame — restore placed them
at 0.000 m error, but both captures came out at night. That was a Rule 27 debt;
under 5.12b **it is now a blocker**, because the junction run is defined ON the
frame and cannot be measured without one. Priority accordingly.
The clock convention they were missing, since it is mine: time-of-day is a
FRACTION of `DAY_LENGTH_SECONDS` (2880 s) — **0.25 sunrise, 0.5 noon, 0.75
sunset**, and `START_TIME_OF_DAY` 0.30 is the early-morning default a fresh
world opens on. For a midday massif frame set 0.5; for the raking light that
made the band lips legible in the original diagnostic, 0.30–0.35.

##### 5.12f THE JUNCTION'S THRESHOLD IS WITHDRAWN — Rule 41 has now fired TWICE on the same acceptance, and the second time it fired on the quantity Rule 41 itself installed

Core produced the frames and measured both arms (`3506f0b` line of work), and
**they reported rather than tuned, which is the whole of what 5.12b asked for
and the only reason this is rulable at all.** A zone that had quietly moved the
threshold to fit would have handed me a green number and no information.

| quantity | apron OFF (the real rejected instance) | apron ON | movement |
|---|---|---|---|
| ground-junction run | **106 px** | **108 px** | +2 px, **+1.9 %** |
| massif visible angular extent | 328 px | 357 px | +29 px, **+8.8 %** |

**The 20 px is dead on Rule 30's plain reading: a threshold must sit above the
real rejected instance, and the rejected instance clears mine by 5.3×.** I do
not get to keep it, and I said in 5.12b that if apron-OFF already cleared 20 px
the quantity needed tightening. It does. This is that debt being paid.

**But the interesting failure is the second one, and it is Rule 41 word for
word:** *when an acceptance number moves by almost nothing while everyone agrees
the thing got better, do not widen the threshold — ask whether the quantity can
express the difference at all.* The junction moved 1.9 %. The hidden-fraction it
replaced moved 1.1 points. **Two different quantities, the same non-movement,
the same file, one day apart** — and the second one is the replacement I wrote
*because* the first failed that exact test. Recorded plainly, because a rule that
catches everyone else and not its author is not yet a rule.

**And no threshold can be placed between 106 and 108.** Below both, it certifies
nothing (Rule 30: a threshold below every real failure is a description). Between
them, it is derived from the values it is meant to test — corollary 30a, refused
three times in this document already, and I am not making it four.

##### 5.12g WHAT WENT WRONG BOTH TIMES, since it is the same mistake and it is transmissible

The first quantity I inherited from core's instrument. The second I derived
myself — and I derived it from **legibility** («20 px is the width at which a run
survives the palette quantiser instead of reading as a gap between two trees»)
and then put that number in the slot where a **separation** belongs.

**A legibility floor and a separating threshold are different objects.** A
legibility floor answers *"below what value can the eye not read this at all?"*
and is derived from the display. A separating threshold answers *"what value
puts the rejected picture on one side and the accepted picture on the other?"*
and can only be derived from **two measured arms**. Mine was correctly computed
and correctly cited and answered the wrong question, so it landed 5.3× below the
thing it was supposed to reject. **The tell is available before any measurement:
a threshold whose derivation never mentions the rejected instance is a floor, and
a floor put in an acceptance's slot will pass everything.** Forwarded to `main`
for ARCHITECTURE as a sibling of Rules 30/41 rather than written into it here —
`docs/` is the lead's zone.

##### 5.12h RULING — the junction SURVIVES, its AGGREGATION does not, and angular extent is REFUSED

**Angular extent is refused as the replacement, and the reason is that it is
blind in exactly the direction the defect lies.** It moved, which is seductive
after two quantities that did not — but movement is not discrimination:

- **The defect is VERTICAL and extent is HORIZONTAL.** §5.12's mechanism is «a
  mountain missing its bottom third». A massif whose full angular *width* is
  visible while its bottom third is canopy scores 100 % extent and **is the
  rejected picture**. That is the identical failure that convicted the hidden
  fraction — identical number, opposite verdicts — one axis over.
- **It is not monotone in the defect.** Clearing trees near the massif raises
  extent whether or not the base is freed, so extent will keep improving as the
  apron widens even in the limit where the apron becomes the bald clearcut
  5.12a refused.
- **It has no control.** 328 is the rejected arm; nothing has ever measured an
  *accepted* one. Adopting it would put us one measurement later in exactly the
  position we are in now, which is the argument for spending that measurement on
  a quantity that can lose.

**The junction quantity is retained. What is withdrawn with the 20 px is its
AGGREGATION, and the aggregation is where the 106 px comes from.** «Longest
contiguous run anywhere along the massif's angular extent» scores a run at the
extent's outer margin — where the massif is nearly at valley level anyway and
meeting the ground is unremarkable — identically to a run through its centre,
which is the only place the base flare and the bench can be read. **A quantity
whose aggregation lets an unremarkable region answer for the remarkable one
cannot discriminate, however well it is measured.** So:

> **§5.12h ACCEPTANCE (replaces 5.12b's run-length clause; the quantity is
> unchanged, the aggregation and the denominator are not).**
>
> **Primary — THE CURTAIN HEIGHT.** For each column of the massif's angular
> extent in the valley frame, take the elevation of the **lowest visible massif
> pixel**, expressed as a fraction of that column's **full unoccluded massif
> extent** (base to summit, which the generator knows and the scatter-suppressed
> frame shows).
> *Aggregation:* the **median over the central half** of the massif's angular
> extent — median because one open column must not answer for the picture, and
> central half because the flare and the bench are read at the body, not at the
> hem. The outer quarters are **reported, never asserted.**
> *Denominator:* the same column's unoccluded extent in the **scatter-suppressed
> frame** — not the massif's modelled height, so occlusion by other terrain is
> divided out rather than counted as canopy.
> *Direction:* lower is better. 0 = the massif stands on the ground it is
> drawn on.
>
> **Secondary, retained and demoted — the junction run**, reported with its
> aggregation and denominator as written in 5.12b, as a canary alongside 5.12c's
> 15/50 band. It failed as a gate; it remains a cheap tripwire for the massif
> having lost its footing entirely.

**THE THRESHOLD IS STILL NOT PLACED, AND THIS TIME THE PROCEDURE THAT PLACES IT
IS WRITTEN DOWN WITH A STOPPING CONDITION**, so this cannot come back a third
time as a threshold argument:

1. **Three arms, on the same frame and vantage** (300 m west, midday 0.5, and
   also at 350 and 500 m where core already has the other two quantities):
   **scatter-suppressed** (the accepted extreme — and this frame *already
   exists*, it is the one that opened §5.12 and showed the tor, the band lips
   and the shoulder break), **apron ON**, **apron OFF** (the rejected instance).
2. **The stopping condition, checked BEFORE any number is proposed:** if
   apron-OFF and scatter-suppressed do not separate by **more than the
   frame-to-frame noise of the measure itself** (re-shoot one arm twice and
   read it), **the quantity is refused too and no threshold is written** —
   report that and stop. Rule 41 a third time is a possible outcome and it is
   better than a fitted number.
3. Only if they separate: the threshold goes **between the apron-OFF value and
   the apron-ON value**, and if apron-ON does not itself land materially closer
   to scatter-suppressed than apron-OFF does, **the apron is not the whole fix**
   — which 5.12a already suspects — and 5.12d's thinning is back on the table
   with its bald-lane cost still standing.
4. Report all three arms and the noise figure. **A single arm is not a
   measurement of a threshold, it is a measurement of a world.**

##### THE OTHER TWO LEVERS, ruled rather than surveyed

**LEVER 1 — density near sightlines: NOT the lever, because it is already
built and the frame still fails.** `TREE_SPACING_FOREST` is 12–18 m and the
scatter consumes it: oak on a 15 m lattice, pine on 14 m. Against the previous
5–8 m that is **5.3× sparser by area**, more than the «не менее чем в трое» the
user ordered. **The user's ruling landed and it was not enough near a
landmark**, which is worth saying plainly so nobody spends the fix twice.
Density is not the binding constraint; *proximity to the massif* is.

**LEVER 3 — hue separation: NECESSARY, NOT SUFFICIENT, and re-scoped.** The
source colours already differ strongly in hue — `PINE_DARK` is a teal-green at
saturation 0.45, every rock tone is neutral at 0.05. The lever is therefore not
«add hue separation», it is two different defects:

- **Chroma discrimination collapses at low luminance.** At the backlit hour
  both surfaces are lit by ambient alone, and at those levels the hue
  difference that exists on paper is not available to the eye. **Lever 3 is
  weakest exactly where the problem is.**
- **The 64-colour palette has no conifer ramp.** Design requirement handed to
  render: **the shipped palette carries a conifer family**, and §1.3b's
  separation test is run with the palette ON.

  > **⚠ MY STATED CAUSE WAS FALSE AND THE MEASUREMENT IS IN §4.2.** I wrote
  > that `PINE_DARK` «must quantise into *grass greens*, whose dark end is a
  > yellow-green». **It quantises into WATER TEALS**, and it does so under both
  > the weighted and the unweighted metric — so this was never a subtlety I
  > missed, it was **a claim I never computed at all.** I took it from a search
  > report and made it load-bearing. The conifer family is still right; the
  > reason it is right changed completely (§4.2).

**Ranked, so implementation order is not a judgement call: (1) the apron, which
restores the mountain; (2) the conifer family, for the reason in §4.2 — which is
NOT that it fixes the pine/rock merge, because measurement shows that merge was
never as broken as I claimed and the family does not move it; (3) nothing
further on density.**

##### THE APRON IS NOT BARE BY CONSTRUCTION — it is a HEIGHT rule, not a clearing

**Sequencing was raised on the reasonable fear that an apron shipped before
§5.10 exists would be the bare ring I warned against. Worked through, that fear
does not survive the arithmetic, and the reason is worth having: the derived
rule never removes vegetation, it caps its HEIGHT.**

C1-B (§1.3b) requires the silhouette exposed above `MASSIF_CLIFFLINE_FRAC`. A
tree is illegal only if **its canopy top subtends more than the cliffline
does** from an acceptance standpoint — and because a near tree sits much closer
to the eye than the mountain behind it, that is a real constraint rather than a
formality. Worked at Ravenscar's d_accept, as an illustration of the derivation
and **not as a tabled number**:

- cliffline elevation = 0.33 × 115 m ≈ **38 m** above the crag's base;
- from 360 m, the cliffline subtends 38/360 ≈ **0.106 rad**;
- a tree on the near hem (≈ 162 m from the crag centre) stands ≈ **198 m** from
  the eye;
- so its legal canopy top is 0.106 × 198 ≈ **21 m**.

**Pine is 28–38 m and is excluded. Everything at or under ≈ 21 m is admitted.**
Big bushes, scrub, stunted and young pines, and stone all pass. **So the apron
is populated in its very first version**, and the §5.10 classes enrich it
rather than being the precondition for it.

- **Consequently the apron does NOT have to wait for §5.10**, and blocking the
  one fix that addresses the user's complaint behind an entirely unbuilt
  feature set would be the wrong trade. Revised order: **tree heights fixed in
  the occlusion model → apron (interim, with the classes that already exist) →
  §5.10 floor classes enriching it → conifer ramp → quantiser decision.**
- **One requirement this places on core, stated because I checked and it is not
  free:** `Bush` and `Stone` are both currently barred from inside a forest
  mass, and the apron band lies inside the pine annulus. **The apron is a
  distinct ground-cover class, not an absence of forest**, so bush and stone
  placement must be admitted there. Whether that is a small change is core's to
  say, not mine.
- **The interim apron is honest, not a stopgap:** a hem of scrub, stone and
  young pine under a cleared skyline is what a talus apron looks like. Nothing
  about it has to be undone when snags and deadfall arrive.

##### §5.10 IS UNBUILT — the LR's position, a second time

Checked in source rather than assumed, and this is the answer to «check which,
because §5 has been in the same position the LR was»:

| §5.10 / §5.8 item | Constants | Mesh | Placed in world |
|---|---|---|---|
| Snags | `SNAG_DENSITY_*` (6 rows) | exists | **never** |
| Big bushes | `BIGBUSH_DENSITY_*` | exists | **never** |
| Fallen logs / deadfall | `LOG_DENSITY_*` (4 rows) | exists | **never** |
| Floor scarps / elevation change | `SCARP_*` (4 rows) | — | **no generator at all** |
| Maturity tiers 25/60/12/3 | `TREE_MATURITY_*_PCT` | — | **never — scale is a uniform 0.8–1.2** |
| Boulders on the forest floor | — | exists | **explicitly excluded from forest masses** |

**The scatter alphabet has five members — oak, pine, birch, bush, stone — and
bushes and stones are both barred from inside a forest mass. So the forest
floor today is bare terrain splat and nothing else.** Every constant above has
**zero consumers**, and unlike the `LR_*` rows **none of them carries a
«НЕ ПОСТРОЕНО» marker**, so the registry currently reads as though this is
built. Requested of the lead: mark them, exactly as the LR rows are marked. A
numbers table that overstates what exists is how a zone spends three sessions
tuning an absent object — §1.6.3, in a different zone, on the same evening.

