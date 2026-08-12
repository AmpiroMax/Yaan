<!--
Created: 12:08:2026 - 22:57:02
Last updated: 12:08:2026 - 22:57:02
-->
<!--
UPD:
- 12:08:2026 - 22:57:02: Выделен из docs/design/LANDSCAPE.md (9786 строк против FILE_HARD_LIMIT 800): §5 (шапка: global rules, crown-aspect ceiling). Чистый перенос — ни одна строка тела не изменена, ни один номер секции не изменён; адреса вида «LANDSCAPE.md §X» продолжают действовать, таблица § → файл живёт в docs/design/LANDSCAPE.md.
-->

## 5. Flora catalog

Global rules: every species is low-poly with 2–3 flat colors and strong value
separation (readability §1.5). **Foliage is the one exception to
"hard-edged mesh" (stage-4, user direction via flora):** «кроны не шариками, а
с листвой… плоскими прозрачными большими плоскими наборами листочков… хочу
чтобы сквозь листву можно было смотреть» — tree foliage is **flat
alpha-cutout cards** carrying leaf clusters, and the canopy is **see-through**.
Trunks, branches, bushes, logs and snags stay solid hard-edged meshes. What
does **not** change: the crown *envelope* still governs, so oak/pine/birch
remain separable by outline at `SILHOUETTE_MIN_PX`, and every size band,
crown-base fraction, spacing and density in this section is unaffected —
cards are a surface treatment inside the same envelope, not a new silhouette
language.

**The exception survives, but NOT on the reason it was granted (measured,
flora — see §1.3).** It was granted on «хочу чтобы сквозь листву можно было
смотреть», read as *transparency*. Measured against the user's own reference
photographs, the tracery in those images is **not** visible because sky shows
through it: luminance is branch 50, leaf 135, sky 235, i.e.
**branch : leaf ≈ 2.54×**. The skeleton reads because it is **the darkest
thing in frame against a bright backlit leaf field** — which is §1.5's
value-contrast doctrine, not an alpha effect. So: **dark limbs plus bright
foliage reproduce the reference; see-through cards do not.** Cards remain the
right way to build a foliage surface, and **nobody may widen the exception on
the theory that more transparency buys more of the reference look — it buys
almost none of it.** This strengthens the envelope wording above rather than
straining it.

Two further consequences to keep straight: our C1 occlusion model needs **no
change at all** — measured crown interiors are 79–86 % leaf, so the
solid-crown effective width was already close to right (§1.3), and the
one-time physics-correction budget stays unspent; and **dappled light under a
canopy is a lighting problem, not a geometry one**, because a shadow caster
thinner than render's caster floor (§2.8.4) will read as solid no matter how
open the card looks. Placement is Poisson-disc /
jittered-lattice per §2 (never raw high-frequency noise threshold — it
clumps). Trees never spawn on rock or sand splat, never inside building pads,
corridors, or water. Slope limit for all trees: `TREE_SLOPE_MAX` = 0.61 rad
(35°) **(предложение — утвердить)**. All densities предложение — утвердить.

**CROWN ASPECT CEILING — a checkable invariant, and the crown-base fraction
becomes DERIVED (ruling, stage-4; flora's finding, and the defect was in my
numbers).** The River Birch crown failed to read as a mass **four times across
two sessions**; flora stopped under Rule 28 rather than attempt a fifth
arrangement, and measured instead of describing. Generated foliage bounding
boxes — **corrected figures, see the pooling note below**: oak **1.53**
tall-to-wide reads as a mass, willow **1.37** reads as a mass, birch **2.30**
reads as a *column*, pine 4.23 and correct because a cone is meant to be
narrow.

> **The first table I ruled on was inflated ≈ 15 % and flora self-reported it
> (oak 1.65 → 1.53, willow 1.51 → 1.37, birch 2.65 → 2.30, pine 4.88 → 4.23).**
> They had pooled twelve size variants into one bounding box, so the
> **variant spread** — birches are 16–22 m tall — was being measured as if it
> were one crown's shape. **The diagnosis is untouched:** the birch was still a
> column, oak and willow still read, and the true band is "1.53 reads, 2.30
> does not", which still brackets the ceiling. **Measure per variant, never
> pooled** now sits beside *measure the artefact, not the intention* — flora
> got the second right and the first wrong in the same table, which is how
> closely related these two failures are.
>
> **`CROWN_ASPECT_MAX` IS 1.8 — and my paragraph "accepting 2.0 rather than
> re-litigating" is WITHDRAWN, because it was written against a stale read of
> NUMBERS.md.** The lead landed 2.0 at 21:12 and refined it to **1.8 at 21:13,
> one minute later, per my own verdict.** I read the file between those two
> edits, inferred a disagreement that had already been resolved, and then
> spent a ruling reconciling myself to a value nobody was holding. **Checking
> the registry would have cost one grep.** Nothing downstream broke — flora's
> trees measure ≤ 1.28 and clear either number — but the reasoning in that
> withdrawn paragraph was sound applied to a fact that was not true, which is
> the third time this session I have produced a well-argued conclusion on an
> unchecked premise. **The tightening trigger it proposed is moot: the ceiling
> is already at 1.8.** Three previous fixes all changed *what goes in the box*
while **the box was the defect** — the same diagnosis §2.8.1 reached about the
mountain, twice in one stage: **a shape failure that lives in the authored
container cannot be fixed by anything that fills it.**

- **Rule: a broadleaf crown's GENERATED foliage bounding box may not exceed
  `CROWN_ASPECT_MAX` = 1.8 tall-to-wide (предложение — утвердить).** Measured
  on the built geometry, never on the authored container — the birch's
  container is 1.8 : 1 and its *generated* box is 2.65 : 1, so a rule checked
  against the spec would have passed the tree that fails. Same discipline as
  §2.8.3's polyline perimeter: **measure the artefact, not the intention.**
- **Species whose silhouette brief is a cone or spire are exempt** — pine at
  4.88 is not a defect, it is the anti-oak. The exemption is a property of the
  written brief, not a free pass anyone may claim later.
- **1.8 is provisional and I am saying so rather than dressing it as
  measured.** It sits above every value that reads (1.65) and well below the
  one that does not (2.65); **the band between 1.8 and 2.65 is untested.** As
  with §7.1, the frame outranks the number: if something at 1.7 still reads as
  a column, the ceiling moves, not the tree.
- **`CROWN_BASE_FRACTION` stops being a universal cap and becomes a FLOOR plus
  a derivation.** This is the real fix, and it dissolves flora's per-species /
  principle question rather than answering it. The 0.35–0.45 band was carrying
  **two unrelated jobs**: clear trunk height (a walkability and feel goal —
  §1.3 measured that crown base is *visibility*-insensitive to three decimals)
  and, by accident of being a fraction of height, the crown's aspect ratio (a
  silhouette property). Split them: `CROWN_BASE_FRACTION_MIN` = 0.35 stays as
  the walkability **floor** — more clear trunk is never worse for walking
  under — and each species' crown base is then **derived as the smallest value
  ≥ that floor which satisfies `CROWN_ASPECT_MAX`**. Oak and willow land
  inside the old band untouched; the birch lands at ≈ 0.58–0.62, which is
  flora's recommended remedy **reached by principle instead of by exception**.
  `CROWN_BASE_FRACTION_MAX` = 0.45 survives as documentation of the typical
  outcome for broad crowns, not as a binding cap.
- **Why the birch is free to change:** the 5–7 m width band is untouched, so
  `TREE_SPACING_FOREST` — which was derived *from* crown width — does not
  move; the accent role ("smallest and slimmest of the three") is strengthened;
  and clear trunk rises 8.5 → ≈ 11 m, which is §5.7's own stated goal. See
  §5.3.

##### RULING — THE WIDTH BAND IS NOT THE LEVER AND NEVER WAS. THE CEILING MOVES. (stage-5)

Flora asked nothing and was right not to; the trade is mine. The question put to
me: the built birch's crown aspect is **1.78 against a ceiling of 1.8** — one
percent, which by this document's own standard is not a margin — and the lever
offered was the crown WIDTH band, 5–7 m → 5–8 m.

> **RULED: `CROWN_ASPECT_MAX` 1.8 → **2.0** (предложение — утвердить). The
> crown width band does not move, and the trade I was asked to weigh — a wider
> birch against its accent role — DOES NOT HAVE TO BE MADE.**
>
> **AND THE «BIRCH CROWN WIDTH 5–7 → 6–8 m, FORCED» RULING ABOVE IS
> WITHDRAWN**, together with the table that forced it.

**0. FIRST, THE MECHANISM, READ OUT OF THE LIVE GENERATOR** — because both the
question and my own first answer to it were built on a model of the birch that
the code does not implement (`ProcFlora.cpp:528-534`, `FloraSpecies.cpp:254-258`,
`FloraSpecies.h:101`):

```
crown_width_frac = 0.34          // crown DIAMETER / HEIGHT, not metres
crown_base_frac  = max( species_value , 1 - ceiling * 0.97 * crown_width_frac )
```

Three consequences, and each of them dissolves part of the question:

- **THE BIRCH'S CROWN ASPECT IS INDEPENDENT OF ITS HEIGHT.** Width is a
  *fraction* of height, so aspect = (1 − base) / 0.34 and the height cancels
  exactly. Computed at three heights: **16 m → 1.747, 19 m → 1.747, 22 m →
  1.747.** There is no worst corner of the height range, because there is no
  variation along it.
- **THE 5–7 m BAND IS NOT AN AUTHORED QUANTITY. It is a DESCRIPTION of what
  0.34 realises**, and 0.34 was calibrated *to* it (flora's own comment: «0.30
  built a 3.6–4.5 m crown against design's 5–7 m band»). Widening the band does
  not widen a birch; **only moving the fraction does**, and the fraction is
  flora's.
- **THE CEILING IS NOT A GUARD RAIL ON THIS SPECIES — IT IS THE SPECIES'
  DRIVING INPUT**, which is exactly what NUMBERS.md says it must not be
  («сторож, а не движущая сила»). At 1.8, `from_aspect` = **0.4064**, which is
  *above* the species' authored and frame-tested 0.40, so **the `max` overrides
  flora's value and the generator derives the crown base from the ceiling.**

**0b. AND THAT GIVES THE REAL DIAGNOSIS OF THE ONE PERCENT, which is not a
shortage of width.** The generator derives at **0.97 of the ceiling on
purpose** — flora's comment: «derive just inside the ceiling, so the assertion
on the BUILT tree has somewhere to fail if the geometry ever drifts outward
again.» So the nominal tree sits at 1.746, **exactly 3 % under, by design.** The
1 % that reached flora is what survives after the known nominal→built overshoot
(cards reach with their *corner*, §5's recorded effect) has eaten two thirds of
that guard.

> **The margin is not thin — it is PRE-SPENT. A 3 % guard against geometry
> drifting outward is being consumed by a structural overshoot that is always
> present, so there is no guard left for the drift it exists to catch.** That is
> a defect in where the ceiling sits, not in how wide the tree is, and no amount
> of crown width would have fixed it.

**0c. WHY MY PREDECESSOR'S TABLE FORCED THE WRONG ANSWER, and it is this
document's own most expensive error committed by the author of the rule against
it.** That table varies H against w **independently** — it asks what a 22 m
birch with a 5 m crown would measure — and reads off «2.64 ✗, the existing band
is ALREADY ILLEGAL». **The generator never builds that tree.** A 22 m birch gets
0.34 × 22 = 7.48 m. The illegal corner is unreachable by construction, and
applying the ceiling to a corner of the authored band is **measuring the
container** — the precise act §5's own rule forbids in the sentence that defines
it («measured on the built geometry, never on the authored container»). **Third
time the number 1.8 and its consequences have come from a container rather than
a tree**, after the ceiling's own value (point 1) and the original 2.65 : 1.

**1. THE CEILING'S VALUE IS THE AUTHORED CONTAINER'S RATIO, AND THE RULE IT
GATES FORBIDS MEASURING THE CONTAINER.** Read §5's own derivation back: «crown
width 5–7 m and `CROWN_BASE_FRACTION_MAX` 0.45 of a 16–22 m height give a
container **1.8 : 1** before a single cluster, and the generated foliage box
measures 2.65 : 1». **1.8 is literally the container number.** The rule then
says, correctly and in the same breath, *measured on the built geometry, never
on the authored container — the container passes and the tree fails.* **The
threshold is a container figure wearing a generated-geometry hat**, and it has
been binding on built geometry ever since the basis was corrected under it.
**Fifth instance of the family this document has already named: a model change
can invalidate a constant's derivation without changing its number** (after
I1's surface-mean → envelope re-spec, `MASSIF_SLOPE_BIN_MAX`, the profile
exponent, and `BIRCH_CROWN_BASE_FRACTION` itself).

**2. THE INSTRUMENT IS ANTI-CORRELATED WITH THE JUDGEMENT ON THIS SPECIES, AND
THAT IS DECISIVE.** Put the two real instances side by side:

| birch | crown aspect | verdict |
|---|---|---|
| base 0.58 — «pale pole with a tuft», the palm | **1.02–1.27** | **REJECTED** |
| base 0.40 — the current tree | **1.78** | **ACCEPTED (frame)** |

**The rejected birch scores BETTER on the ceiling than the accepted one.** A
threshold that ranks the artefact we turned down above the artefact we kept is
not measuring this failure. §5 already contains the diagnosis and I am only
applying it: *«a palm and a birch can have identical crown aspect — what
separates them is STRUCTURAL, which is why flora's limb-spread invariant is the
right instrument and the aspect ceiling never was.»* **`CROWN_ASPECT_MAX` is a
guard rail against the 2.30 : 1 column-box. It must never become the thing that
shapes a species** — NUMBERS.md already records it as «сторож, а не движущая
сила», and widening a birch to satisfy it would be exactly that inversion.

**3. AND THE CEILING STRUCTURALLY PENALISES THE ONE PROPERTY §5.3 DEMANDS. I
OPENED THE FRAME** (`screenshots/flora_grown/01_birch_at_040_EXPERIMENT.png`)
**rather than relaying my predecessor's reading.** The two pale-trunked trees
read as slim, light-crowned water-margin trees with visible branch structure —
§5.3's brief, which asks for a *«small loose crown»*, not a rounded mass. **But
a LOOSE crown inflates its own bounding box**: the aspect is measured on the
generated foliage box, which spans from the lowest cluster to the highest, while
the mass between them is deliberately sparse and see-through. **So the looser
the crown — the more it obeys its brief — the worse it scores.** The ceiling and
§5.3 are pulling in opposite directions on this species, and the ceiling is the
younger and the more provisional of the two.

**4. WHERE 2.0 COMES FROM, and it is a bracket, not a preference.** The
evidence band is now narrower than §5's «1.53 reads, 2.30 does not»:

> **1.78 reads (design-accepted, frame). 2.30 does not (the real rejected
> column, flora's per-variant corrected measurement).**

**2.0 sits 12 % above the highest accepted value and 13 % below the lowest
rejected one.** Stated plainly rather than dressed up: **the interval is only
1.29× wide, so ±12 % is the maximum symmetric margin obtainable** — the ceiling
is now *tightly bracketed by evidence* rather than generously clear of it, and
that is the better of the two conditions. It also lands back on the value the
lead first chose, which my predecessor talked down to 1.8 on the container
measurement that has since been superseded.

- **Superseded explicitly, so a grep finds it:** §5's line «it sits above every
  value that reads (1.65) and well below the one that does not (2.65)» is
  replaced by 1.78 / 2.30. Both of its numbers were the pooled-variant figures
  flora withdrew.
- **The tightening trigger is unchanged and still live**, in the direction it
  was written for: *if something below the ceiling reads as a column, the
  ceiling moves, not the tree.* This ruling is the same principle — the frame
  outranks the number — applied in the other direction.

**5. WHAT 2.0 ACTUALLY DOES TO THE TREE — computed, not estimated, and it is
almost nothing.** At 2.0 the derived `from_aspect` falls to **0.3404**, below
the species' authored 0.40, so the `max` **hands control back to flora's
frame-tested value**:

| ceiling | derived floor | crown base used | nominal aspect | margin |
|---|---|---|---|---|
| **1.8 (today)** | **0.4064** | **0.4064 — ceiling overrides flora** | 1.746 | **3.0 %** |
| **2.0 (ruled)** | 0.3404 | **0.4000 — species value governs** | 1.765 | **11.8 %** |

- **The tree moves by 0.006 of its height.** Crown base 0.4064 → 0.4000; the
  built birch is the one already in the frame.
- **There is no runaway, and this is the fear worth killing explicitly: the
  ceiling enters as a `std::max`, so it can only ever RAISE the crown base.**
  Raising the ceiling cannot make the birch bushier than flora authored it, and
  `CROWN_BASE_FRACTION_MIN` = 0.35 is never approached.
- **The structural gain is the point:** at 2.0 the ceiling stops deriving this
  species and goes back to guarding it, which is what the registry already
  describes it as and what §5 says a ceiling is for.

**6. THE COST I WAS ASKED TO PRICE DOES NOT ARISE — and one cost cited for the
opposite ruling was illusory anyway.** «A wider birch weakens its accent role»
was the real trade, and **no birch gets wider**, so it is not spent. Separately:
§5 above justifies changes by saying a wider band would move
`TREE_SPACING_FOREST` «which was derived *from* crown width». **Checked in
NUMBERS.md: `TREE_SPACING_FOREST` is 12–18 m** — half again as wide as an 8 m
crown — **and §5.3 places birches in loose bank lines, never deep forest.** That
cost has been cited twice in both directions and does not exist in either.

- **STILL STANDING, and it does not depend on the withdrawn ruling: the birch
  lattice is hard-coded at 8.0 m in `WorldgenScatter.cpp` while oak and pine
  read `TREE_SPACING_FOREST`.** That remains a real Rule 32 defect for core —
  one species' spacing pinned where the others are derived — and it is now the
  *only* live item from the withdrawn block. It was never contingent on the
  width band moving.
- **AND THE DOCUMENTED BAND IS WRONG AT ITS TOP, which is worth fixing as
  documentation rather than as a lever:** 0.34 × 16…22 m realises **5.4–7.5 m**,
  not 5–7. §5.3's band is **descriptive of a fraction**, so «a range is two
  assertions» does not apply to it — there is only one assertion, and it is
  `crown_width_frac`. Restated in §5.3.

- **And the frame I have cannot price the real cost, which is a further reason
  to move the ceiling instead.** A species line against flat sky is fit for the
  question under test — crown aspect is a silhouette property and this frame
  varies it across seven trees — but it says **nothing** about whether a wider
  birch still reads as an accent against dark water and pines at distance
  (F7: the frame must vary the dimension the property lives in, and accent role
  lives in *distance and backdrop*). **Moving the ceiling needs no frame we do
  not have; moving the width does.**

**6. STATUS, AND IT IS NOT «PASSING».** The 1.78 birch is **design-accepted and
USER-UNSHOT** — the user rejected the previous trees in words and has not seen
the rebuilt ones. §1.6.3's category applies to my own ruling: the upper bracket
rests on a frame I opened, not on the user's verdict. **If the rebuilt birch is
rejected on the tour, this bracket re-opens and the width band comes back into
play with it.**

> ### RULING — `BIRCH_CROWN_BASE_FRACTION` 0.58–0.62 → **0.40–0.45**, and this
> ### is not a change of ruling but the FIRST APPLICATION OF THE RULE ABOVE
>
> **The rule says «the SMALLEST value ≥ the floor which satisfies
> `CROWN_ASPECT_MAX`». 0.58 has never been that value.** It was derived when
> the aspect was measured on the authored **container** (2.30:1). Flora then
> corrected the basis to **generated geometry**, where the birch measures
> **1.02–1.27 against a ceiling of 1.8** — and the derived value was never
> recomputed against the corrected basis. My own NUMBERS note records the gap
> without my noticing what it implied: *«берёза 0.58 при выведенных 0.09»*, and
> the ceiling would only bind at a crown base near 1.2.
>
> **Fourth instance of the family: A MODEL CHANGE CAN INVALIDATE A CONSTANT'S
> DERIVATION WITHOUT CHANGING ITS NUMBER** — after I1's surface-mean → envelope
> re-spec, `MASSIF_SLOPE_BIN_MAX`'s dead provenance, and the profile exponent.
> The measurement basis moved and the constant fitted to the old basis stayed.
>
> **Flora's sentence is the one to keep: THE MARGIN IS WHERE THE PALM LIVES.**
> A value chosen «с огромным запасом» over its derivation is not safe, it is
> *unexamined* — the surplus does work nobody specified, and here the surplus
> confined the foliage to the top 42 % of the tree and built a palm.
>
> **I looked at both frames before ruling** (`screenshots/flora_grown/`). At
> 0.58 the birch is a pale pole with a tuft on top; at 0.40 it is a slender
> light-crowned tree with visible branch structure inside the crown. The
> difference is not subtle and the aspect ceiling cannot see it, because **a
> palm and a birch can have identical crown aspect** — what separates them is
> structural, which is why flora's limb-spread invariant is the right
> instrument and the aspect ceiling was never going to be.
>
> - **MAX drops to 0.45 too — a range is two assertions.** 0.40–0.45 is a
>   0.05-wide band whose lower end is the tested value and whose upper end is
>   `CROWN_BASE_FRACTION_MAX`, so **the birch exception very nearly dissolves**:
>   it is now simply the top of the general 0.35–0.45 band, which is what a
>   slender water-margin tree should be. **Both ends are measured before it
>   ships**, per the rule flora's own pine just demonstrated.
> - **Walkability is untouched:** 0.40 × 16 m (shortest birch) = 6.4 m of clear
>   trunk against `CANOPY_CLEARANCE_MIN` = 2.2 m — nearly 3× over.
> - **Correcting one figure in flora's case, because it will be quoted:** 0.40
>   gives ≈ 7.6 m of clear trunk on a 19 m birch, which is **less** than the
>   8.5 m the old 0.45 gave, not more. It does not change the ruling — 8.5 → 11
>   was a bonus I claimed, never a requirement — but the argument should not
>   travel with an arithmetic slip in it.
> - **And the principle flora offered if I refused is right, so it is recorded
>   even though it does not apply: a species nobody will defend by eye should
>   not have a catalog slot.** That is «an invariant nothing fails is not an
>   invariant» pointed at content instead of at tests.

> ### ~~RULING — BIRCH CROWN WIDTH 5–7 m → 6–8 m, and it is FORCED~~
> ### ⚠ WITHDRAWN (stage-5) — THE TABLE BELOW DESCRIBES A GENERATOR WE DO NOT HAVE
>
> **`crown_width_frac` = 0.34 is crown DIAMETER / HEIGHT, so H and w are not
> independent and the height cancels out of the aspect entirely.** Every row
> below asks what a 22 m birch with a 5 m crown would measure; the generator
> builds that birch with a 7.48 m crown. **The «already illegal» corner is
> unreachable by construction, and reading the ceiling off a corner of the
> authored band is measuring the CONTAINER — the one act this rule's own
> definition forbids.** Replaced by the ceiling ruling in §5 above
> (`CROWN_ASPECT_MAX` 1.8 → 2.0); the width band does not move. The only clause
> here that survives is the hard-coded 8.0 m birch lattice, which was never
> contingent on the band.
>

> Flora reports the birch at **aspect 1.78 against a 1.8 ceiling** — 1 % of
> margin, and only after spending `card_aspect` 0.95 → 0.76. They declined to
> ask for the width band, because a wider birch weakens the «smallest and
> slimmest of the three» accent role. **The arithmetic takes the decision out of
> both our hands.**
>
> | H | base | crown height | w = 5 | 6 | 7 | 8 |
> |---|---|---|---|---|---|---|
> | 16 m | 0.40 | 9.6 m | **1.92 ✗** | 1.60 | 1.37 | 1.20 |
> | **22 m** | **0.40** | **13.2 m** | **2.64 ✗** | **2.20 ✗** | **1.89 ✗** | **1.65** |
>
> **THE EXISTING BAND IS ALREADY ILLEGAL.** At the top of the height range a
> 22 m birch needs **≥ 7.33 m of crown** merely to reach the ceiling, and the
> band's maximum is 7. Flora's built 6.9 m is not a tight pass; it is **outside
> the band's own worst case**, and the only reason nothing has failed is that
> `crown_width_frac` never realises the corner the band permits. **A range is
> two assertions and this range's assertions were never re-checked against the
> new crown base** — sixth instance, and the first where the illegal end is
> mine rather than an implementation's.
>
> - **So the band moves for the derivation, not for the margin, and both ends
>   move: 6–8 m.** A crown that begins at 0.40 instead of 0.58 is **43 % taller**
>   and a real birch's lower limbs are correspondingly longer. **This is the
>   same act as the crown-base re-derivation itself, one level along: a constant
>   fitted under a condition that has since changed.** Worst realised aspect
>   becomes **1.65**, an 8 % margin, which covers the crown-base band's own 8 %
>   span.
> - **THE ACCENT ROLE SURVIVES, CHECKED RATHER THAN ASSERTED.** Oak crowns are
>   11.5–15.4 m and pine 9.2–12.5 m. **A birch at 8 m is still 13 % slimmer than
>   the narrowest pine and half the oak** — it remains the smallest and slimmest
>   of the three by a comfortable margin, and flora was right to raise the
>   concern and right that it is mine to weigh.
> - **⚠ CONSEQUENCE FOR CORE, VERIFIED IN SOURCE: the birch lattice is
>   hard-coded at 8.0 m** (`WorldgenScatter.cpp`, 45 % keep) **and does not
>   derive from crown width, while oak and pine both read
>   `TREE_SPACING_FOREST`.** At an 8 m crown on an 8 m lattice, adjacent kept
>   birches touch, and **a line of L2 guides becomes a hedge** — §1.3 lists «lone
>   birch» as a guide; a thicket is not one. **The defect is that one species'
>   spacing is pinned where the others are derived** (Rule 32's shape: a derived
>   quantity computed by one consumer and hard-coded by another). Reported, not
>   patched — the fix is core's and the birch lattice should follow crown width
>   as the other two do.
>
> ### RULE 30, SHARPENED TWICE — and flora's version is better than mine
>
> I ruled that the control should be **the real rejected artefact**. Flora
> applied it and found it **could not be satisfied on the clause I aimed it at**:
> the repaired birch measures limb-spread 0.399–0.442, but **the oak's smallest
> variant sits at 0.166 — below the rejected birch's 0.17–0.19.** A compact
> crown on a short tree and a tuft on a tall pole give the same number from
> different objects, so **no floor on that quantity separates accepted from
> rejected without failing an accepted species.** They moved the floor to
> foliage *span*, where a 0.58 crown base caps span at 0.42 by construction and
> every accepted species measures 0.49–0.76 — rejecting **the whole class**
> rather than one instance.
>
> > **WHICH CLAUSE A FLOOR BELONGS ON IS ITSELF A MEASUREMENT (flora's, adopted
> > verbatim). And the test for it: if NO value on a quantity separates the
> > accepted cases from the rejected ones, the quantity is wrong — not the
> > threshold.**
>
> **That is the discriminating-power test, and it is the mechanical form of
> §2.8.7's whole thesis.** Nine invariants measured the object and none the
> view; the way to have caught that in an afternoon was to ask of each one *«is
> there any threshold on this quantity that separates the mountain the user
> rejected from one he would accept?»* For most of them the answer is no.
> **«Measuring the wrong thing» stops being a judgement and becomes a
> computation.**
>
> ### TWO RULES FROM DEFECTS ONLY A MOVING FRAME COULD FIND
>
> - **`cards_per_cluster` = 2 IS NOT A CHEAPER 3, IT IS A DIFFERENT OBJECT.**
>   Cards are fixed-orientation, so two crossed planes have azimuths where both
>   present edge-on — and there the birch was «a line of bare white poles with a
>   few flecks», **the rejected silhouette surviving a rewrite that had genuinely
>   fixed the shape, purely as a viewing-angle artefact.** Three planes cannot
>   all be edge-on. **Rule: any card-based foliage species uses ≥ 3 planes per
>   cluster.** This is F7's corollary in geometry: a property that varies with
>   azimuth is invisible to any test that does not vary azimuth.
> - **§1.5's SEPARATION REQUIREMENT APPLIES WITHIN AN OBJECT, NOT ONLY BETWEEN
>   OBJECTS.** All wood drew in one colour, so the birch's near-white limbs
>   matched its own foliage and the crown read as scaffolding rather than
>   tracery. **A tree whose limbs and foliage share a value reads as one mass —
>   the same defect as pine-against-rock, two scales down.** Fixed with dark
>   twigs on a white bole, which is both what the photographs measure and what a
>   birch is.
>
> ### RULE 30, SHARPENED — THE CONTROL SHOULD BE THE REAL REJECTED ARTEFACT
>
> Flora's limb-spread invariant ships with a control (a synthetic palm scoring
> 0.06 against a 0.15 floor), which is Rule 30 done correctly. **And it still
> passed the tree the user rejected: the birch measured 0.17–0.19.**
>
> **A synthetic worst case is the EASY reject. The hard one — the one that
> matters — is the artefact that was actually turned down.** When a real
> rejected instance exists, it is the control, and the floor must sit above it.
> Recommendation to flora, not a ruling in their zone: **re-measure limb spread
> on the repaired birch; if it lands at or above ≈ 0.22 (the lowest accepted
> species), raise the floor to sit between the rejected version and the
> accepted ones**, so the invariant would have caught what the eye caught.
> A floor placed below every real failure is a description, not a test.
- **A RANGE IS TWO ASSERTIONS, AND A SUITE THAT TESTS ONE BOUND IS GREEN WHILE
  THE WORLD DRIFTS OUT THE OTHER SIDE (general rule, from flora's second
  finding).** The birch's crown had drifted to **3.6–4.5 m against this
  document's 5–7 m brief** — a third narrower than specified — with a fully
  green suite the whole time, because only the *ceiling* of the width band was
  ever asserted. That was the other half of why it read as a column, and it
  means the rule was not wrong, it was **half-implemented**. This is now the
  fourth appearance of a range-handling failure this stage (core's tor derived
  a base from `_MIN` while drawing heights across `_MIN…_MAX`, and they report
  four earlier instances in their own zone), so it is written down as a family
  rather than as four anecdotes: **every `_MIN`/`_MAX` pair in this document is
  two separate claims about the world, and a validation that checks one of them
  is not checking the range.**
- **Widths are calibrated against the BUILT tree, not the envelope** (flora,
  accepted): foliage never reaches the envelope's widest point, because
  containment holds a *card's corner* inside and the widest ring sits where a
  card would overshoot the crown top, so achieved width is ≈ 0.7–0.9 of
  nominal. Same discipline as the aspect ceiling, one level down.
- **The aspect ceiling is asserted at NOMINAL size only, deliberately, and
  flora was right to draw that line.** §5.8's maturity tiers scale trees
  ×0.4–1.5, which takes crowns outside any absolute band *by construction and
  on purpose*; a per-instance width assertion would be a rule forbidding this
  document's own tiers. The aspect *ratio* is scale-invariant and therefore
  still meaningful per instance — the width *band* is not.
- **Where each half lives:** the invariant is a design acceptance criterion
  and lives here, exactly as §2.8.3's do. How the generator satisfies it —
  cluster placement, card layout, the derivation itself — is flora's spec.
  Same split as the massif: design owns the test, the zone owns the mechanism.

