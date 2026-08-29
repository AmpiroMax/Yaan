
## 1.9 THE BACKWARD SWEEP — every pre-existing acceptance rule against the aggregation/denominator clause (audit, stage-5)

**This was handed back twice and it should not have been.** It ran now because
three independent things forced it, and **all three were found by other zones
auditing me rather than by me auditing the corpus** — §1.7's terrain-only
control, withdrawn after core found it contradicted a figure in its own report;
§5.12's fraction, which was the wrong quantity outright; and `docs/audits/CODE_AUDIT.md`
finding the same disease in the test suites, in three shapes: *a share of X among
Y where Y is pre-selected by X*, *a ratio whose ideal value is achieved by a
fully flat world*, and *a headline whose two halves come from two different
denominators.* **All three shapes are present in this document.** Sixty-one
acceptance rules across LANDSCAPE.md and WEATHER.md were read against the clause.

##### 1.9.0 THE INSTRUMENT, because it is the transmissible part and it is one question

Reviewing a rule when you write it asks *"does this measure the thing?"* — and
every rule below passed that, which is why they shipped. The denominator defect
is invisible to that question and visible to a different one:

> **What world MAXIMISES this number, and would I ship that world?**

It is mechanical, it costs seconds per rule, it needs no measurement, and it
found S-1 and S-5 below immediately. **Every acceptance rule in this document
gets that question asked of it at authoring time from now on**, and the answer
goes in the rule's own text where the aggregation and denominator already go.

##### S-1 ⚠ C1's DENOMINATOR IS CHOSEN BY THE EFFECT C1 MEASURES — and more forest RAISES the score

**Verified in source, not inferred** (`WorldgenValidation.cpp:104`):

```cpp
// "Open walkable ground": dry, walkable slope, not inside forest
// masses (trees occlude) and not on the landmark itself.
if (in_forest_mass(layout, p)) continue;
```

**The numerator asks "is the landmark occluded?" and the denominator has already
removed the places where the dominant occluder lives — and the comment gives the
reason as `trees occlude`.** That is the audit's first shape exactly, and it is
Rule 36 inverted: an exclusion is supposed to be chosen by cause and this one is
chosen by the effect under test.

Three consequences, all arithmetic rather than opinion:

- **Planting forest can RAISE C1.** Every standpoint that becomes forest leaves
  the denominator, and standpoints inside a wood are overwhelmingly the blocked
  ones. The rule improves as the world acquires the occluder it exists to police.
- **The excluded set is larger than the margin.** `FOREST_COVERAGE` is
  0.25–0.40; C1 measured **0.6429** against a floor of 0.6, a margin of 4.3
  points. A filter covering a quarter to two fifths of the ground is deciding a
  four-point verdict.
- **C1 could never have caught §5.12.** The forest eating the mountain is exactly
  a canopy-occlusion defect, and the standpoints where it is worst are the ones
  C1 does not look at. That is not a hypothetical: it took a screenshot and a
  user's word, and this is why.

**RULING S-1. The exclusion is RETAINED and stops being invisible.** It has a
real defence — "the player standing inside a wood sees nothing, and that is not
a landscape failure" — but a defence makes it a **scope**, and a scope has to be
written on the rule instead of living in a comment. Binding:

- **C1 is restated as a rule about open ground**, in its own text, with the
  excluded fraction of walkable ground **reported beside every C1 figure.** A
  number quoted without it is not quotable.
- **A second figure is computed over ALL walkable ground including forest
  interiors, and it is REPORTED, NEVER ASSERTED.** *Aggregation:* same standpoint
  grid. *Denominator:* every walkable, dry, non-landmark standpoint. If the two
  figures diverge by more than the margin, **the verdict is being carried by the
  denominator** and the rule is not usable until that is resolved.
- **The falsifiable clause, and it is the one that makes this a rule rather than
  a caveat: C1's denominator must not shrink when the world gains occluders.**
  Counterfactual arm (Rule 30b), one run: raise `FOREST_COVERAGE` and confirm
  C1 does not **improve**. **If it improves, the rule is inverted** and no
  threshold on it means anything. Nobody has run this and it is cheap.

##### S-2 ⚠ I4 HAS NO VALID CONTROL — its must-fail arm and its passing values sit on DIFFERENT denominators

I4 is *"above the cliffline, no single 10° slope bin holds more than 0.30 of the
surface"*, surface-area weighted. Its recorded control — the old dome at **33.2 %**
— was measured **footprint-weighted, over the whole crag**, and this document
already records that reading as *superseded, not reconstructed*. The passing
values (24.2 %, 18.5 %) are surface-weighted above the cliffline.

**So the number that is supposed to fail I4 and the numbers that pass it have
never been on the same instrument.** This is the audit's third shape — a headline
assembled from two denominators — inside an invariant that is currently cited as
holding. 33.2 % vs 30 % is a 10 % margin, and switching from plan-view footprint
to true surface area systematically *lowers* the fullest bin's share on a steep
body, so **the control may well pass I4 once measured correctly**, which would
leave I4 with no rejected instance at all.

**RULING S-2. Until the dome is re-measured on I4's own denominator, I4 is
REPORTED, NEVER ASSERTED**, and it may not be counted among the invariants a
seed "passes". Re-measuring it is one run of an instrument that already exists.
**This is Rule 30 at its plainest and it has been sitting in the numbers table
looking green.**

##### S-3 §2.9's spire siting is a luminance RATIO, and §1.3b ruled that quantity is a linear DIFFERENCE

§1.3b settled it explicitly — *"Not a luminance ratio… a ratio criterion is most
permissive in the darks"* — and §2.9's backdrop table predates it and still
divides. Two rules about value separation, two denominators, one document.
Converted to §1.3b's own ruler (`PALETTE_SHADE_STEP_REF` 0.0784), using §2.9's
own measured luminances:

| backdrop | §2.9's ratio | difference | **steps** | verdict |
|---|---|---|---|---|
| bright sky 0.790 | 1.10× "unusable" | 0.079 | **1.01** | **fails the 2-step floor outright** |
| mid rock 0.371 | 2.3× "strong" | 0.498 | **6.35** | passes |
| `PINE_DARK` 0.197 | 4.4× "maximal" | 0.672 | **8.58** | passes |

**No verdict flips, and the sky case gets STRONGER** — "1.01 steps against a
floor of 2" is a rejection by the project's own separation rule, where "1.10×
unusable" was an adjective. **RULING S-3: §2.9's siting rule is restated in
steps; the ratio table is retained as provenance and marked superseded.** Cheap,
no re-measurement, and it removes a second denominator from the corpus.

##### S-4 THE RULES WHOSE AGGREGATION OR DENOMINATOR IS STILL UNSTATED — reported, never asserted, until named

**RULING S-4: a rule in this list may be measured and reported but may not carry
a verdict until its missing half is written.** Not deleted — most are good rules
missing one sentence — but a rule that cannot say what it divides by cannot fail
anything, and quoting one as a pass is the exact move this sweep exists to stop.

| rule | missing | note |
|---|---|---|
| **R4 `CASTLE_SILHOUETTE_RATIO` 0.6** | aggregation; **and the denominator is self-selecting** | "standpoints ≥ 300 m **where both are visible**" — a castle that occludes the L0 deletes the standpoints where it dominates most. **S-1's disease, in the rule that protects the landmark hierarchy from the castle.** Highest priority in this table. |
| **W3 wind-field invariant** | both — *"agree… to within their stated lag"* and **the lag is never stated** | unfalsifiable as written; the must-fail control is good and cannot be run against nothing |
| `CANOPY_VISIBILITY_MIN` 0.25 | both | a per-ray transmittance with no rule for combining rays |
| occlude-and-reveal 30–80 % | aggregation, **and it has never been measured** | see S-5 — this is the load-bearing one |
| `LANDMARK_SEPARATION_STEPS_MIN` | aggregation | the threshold and its ruler are exemplary; what is aggregated over the two masses is not stated |
| BR-2 `DETOUR_MAX` 1.4 | aggregation | denominator is exemplary (the generator's own cost field); per-path vs worst-path unstated, and passing overhead is still unmeasured |
| BR-3 ratio, BR-4 | aggregation / seed statistic | both already demoted or normalised, so the exposure is low |
| I6 CV 0.35 | cross-radial aggregation | per-radial CV is defined; how radials combine is not |
| §4.1 `ROCK_PALE`, elevation-holding check | aggregation | interval is derived from §1.3b's rulers and is sound |
| W4, W5 | both | frame-condition rules, never numeric |

##### S-5 ⚠ A FLAT, TREELESS WORLD SCORES THE MAXIMUM ON C1 — and the only rule that would reject it has never been measured

Asked of the corpus's headline rule, §1.9.0's question answers itself: **C1 is
maximised at 1.000 by a bare plain with one crag on it** — «земля плоская и
мёртвая», the exact world §1.1 exists to forbid, scoring perfectly on the rule
§1.1 leads with. C2 was meant to be the counterweight and does not bind: its
testbed form counts *coequal attractors*, which a bare plain with one landmark
trivially satisfies, and §2.1's concealment clause is a **ceiling** (≤ 40 % hidden),
pointing the same way.

**Searched the whole corpus for a LOWER bound on concealment. There are two.**
BR-1's occluded run is per-path and waivable in writing. **§1.4's
occlude-and-reveal — "visibility from the two nearest POIs is between 30 % and
80 % of the approach path" — is the only global one, and its lower bound of 30 %
is the single clause in this document that a flat world fails.** It is recorded
as *"already validated"* with **no number anywhere.**

**RULING S-5.**
- **C1 and occlude-and-reveal are read as a PAIR and neither is quoted alone.**
  C1 without its partner certifies a landmark visible from a world with nothing
  in it.
- **Occlude-and-reveal is promoted to a first-class acceptance and gets the
  missing half:** *Aggregation:* fraction of sampled stations along the approach,
  **plus** a longest-visible-run clause, since 55 % visible in one continuous
  block and 55 % alternating are the reveal and its absence at the same number —
  §5.12's lesson, one rule over. *Denominator:* stations on the approach path
  between the two nearest POIs, the path being BR-2's own cost-optimal route so
  the two rules cannot disagree about which path they mean.
- **It must be measured, and the flat plain is its control** — a real rejected
  instance this project has already shipped and been told about in words. It will
  read ≈ 100 % visible and must fail.

##### 1.9.6 WHAT THE SWEEP DID NOT FIND, which is most of it

Stated because a sweep that only reports damage misrepresents the corpus and
tempts the next reader to discount it. **The large majority of these sixty-one
rules name both halves, and a dozen name them better than the clause requires:**
BR-4's normalised Clark–Evans (denominator re-derived per class, after the single
global control was found wrong for four of five); BR-5, whose ratio was
*demoted to a difference* precisely because its denominator can be zero, and
whose aggregation is pinned per-distance against pooling; BR-6's median-plus-tail
("a mean can hide a desert"); I3 and I5's true-surface-area rule with footprint
kept as a diagnostic; I8's isoperimetric denominator; §4.3's banding criterion,
which replaced a ratio-shaped instrument with a max-interior-step against a named
ruler and shipped with both arms from one frame; and **WEATHER.md's A1–A7 and
W10.4's C1–C3, every one of which names aggregation, denominator and a control —
four of them a real shipped rejected instance, one of them another studio's
shipped game.** The clause works. It was the pre-existing corpus that had never
been passed under it, and that is now done.

---

