
### 5.10a THE MOSS RULING — an anchored class is explained by its ANCHOR, not by the ground (ruling, stage-5)

Flora requested `GROUND_MOSS_FOREST_PER_M2` = **0.0263** (263/ha) against the
shipped **0.0040** (40/ha), so that `authored × E[clump]` would land the realised
count on 40/ha. **REFUSED. The row is unchanged at 0.0040.** Not because 263/ha
is a wrong number for ground moss — it is a perfectly good number for a
*different thing* — but because the two figures are not two values of one
quantity, **and because the requested row does not do what it was requested to
do.** That second half is measurable and it comes first, since it settles the
matter without anyone having to win the definitional argument.

#### 5.10a.1 The arithmetic, which closes it before the definitions are opened

`scatter_forest_ground` (WorldgenScatter.cpp) iterates **anchors**, not ground:
one candidate trunk per `TREE_SPACING_FOREST` lattice cell (mid-band 15 m ⇒
225 m²), and it places **at most one patch per trunk**, with per-anchor
probability `per_m2 × 225 × clump_field(p)`.

- At the shipped 0.0040 the per-anchor probability is **0.90** — which is
  flora's own «44 stems/ha × ~⅔ carrying a basal patch» read back correctly,
  so the row and the code agree about the *derivation*.
- `CLUMP_COVERAGE_MOSS` 0.22 and `CLUMP_CONTRAST_MOSS` 0.55 give, from
  `clump_field`'s closed form, **E[field] = 0.1614** (coverage × [½·edge +
  (1 − edge)], edge = 1 − 0.55·0.85 = 0.5325). That is *derived*, not fitted:
  it reproduces core's independently measured 0.152–0.163 without using it.
- Predicted realised = 0.90 × 0.1614 × 44.4/ha = **6.45/ha**, against core's
  **measured 6.09/ha**. The 5.6 % gap is the pass's own rejections (water, pads,
  entrance rings, the path margin). **A model that reproduces a measurement it
  did not consume is allowed to predict the next one.**

Now run the *requested* row through that same model. At 0.0263 the per-anchor
figure is **5.92 — and it is a probability.** It saturates:

> P(place) = E[min(1, 5.92 · field)] = **0.200**  ⇒ realised ≈ **8.4/ha**.

**The requested row buys 1.4×, where 6.6× was intended.** And it cannot be made
to buy more by pushing it further, because this pass places **one patch per
trunk at most**: its structural ceiling is one per lattice cell = **44.4/ha
before rejections, ≈ 41.9/ha after.** 263/ha is **5.9× a ceiling the row does
not raise.**

**Normalising by the field's own mean instead — the obvious counter-proposal —
is arithmetically the SAME operation and fails identically:** 0.90 / 0.1614 =
5.58, P = 0.199, realised **8.4/ha**. Both "fixes" are one fix wearing two
names, and the reason they fail is the next section.

Rule 44 warned that raising a row to compensate *works* and silently redefines
the row. **Here it does not even work**, which is the same disease one stage
worse: the linear model `realised = authored × E[field]` that justifies the new
number is false exactly at the value the new number puts it. Had the row landed,
the suite's `ratio < 0.60` band would have gone on reporting a shortfall while
everyone believed the shortfall had been paid.

#### 5.10a.2 The definitional answer, and why NO row value exists

Two authored numbers about moss contradict each other as statements about the
same moss:

- flora's derivation asserts **⅔ of stems carry a basal patch** — 0.667;
- `CLUMP_COVERAGE_MOSS` asserts moss is non-zero on **0.22 of the ground**, and
  that figure is exact by construction (the raw field is rank-equalised, and
  core asserts the realised coverage against it).

**0.667 > 0.22, so no row value, no composition and no normalisation can satisfy
both** — they disagree about the CAUSE, not about a magnitude. A 13 m drift
field says *moss grows where the ground is damp and shaded*. A basal patch says
*moss grows where THIS TRUNK is damp and shaded*. A trunk manufactures its own
microclimate; that is the entire content of §A7's associative grammar —
«каждый предмет объясним соседом». **If the ground field decides whether the
patch exists, the anchor is decoration and the association is a lie.** That is
the ruling, and the 6.6× was its symptom.

**RULING M-1 — MossPatch/ForestFloor is ANCHORED.** `GROUND_MOSS_FOREST_PER_M2`
**stays 0.0040 (40/ha)**; the request for 0.0263 is refused; and
`clump_applies` on that row goes to **FALSE in the same commit as this text**.
The column already exists and `FlowerJewel` already uses it, so this is a
one-bool change and no new mechanism. Predicted realised afterwards:
0.90 × 44.4 × 0.944 = **37.7/ha = 0.94× authored**, and *that* residual is
honest — it is the placement's stated exclusions, each of which has a cause.
**Where the patchiness then comes from, since it must come from somewhere:**
trees are already clumped, ~10 % of trunks carry nothing, and M-4 below
scatters the dressing. Moss inherits the stand's own structure, which is what
an anchored class is supposed to do.

**RULING M-2 — Mushroom/ForestFloor keeps `clump_applies` TRUE and its 20/ha
row unchanged.** Flora authored it «BEFORE clumping» in as many words: the
rings-and-clusters look IS the intent and the field IS that look, so the
authored number correctly sits upstream of it. Realised 1.61/ha against 20 is
the design, not a defect. The two rows share a habitat and a loop and must
still differ — which is exactly why M-3 exists.

**RULING M-3 — the DISCRIMINATOR, so this is a mechanism and not two
exceptions.** Every density row declares which of two things its number counts,
and the schema carries it as a column rather than as prose:

| basis | meaning | composition | rows |
|---|---|---|---|
| **BASE** | density *before* the field; the field is the intended look | `authored × field` | Mushroom/ForestFloor, and every row whose derivation says "before clumping" |
| **REALISED** | a count of things that *exist on the ground*; the field may SHAPE but never SCALE | `authored`, field normalised by its own integral | MossPatch/ForestFloor, and **every `per_100m` row by definition** — that field is documented as "a TOTAL COUNT, not a density" |

**The tell that tells you which one you are holding: read the derivation and
count the verbs.** «44 stems × ⅔ carrying a patch» counts patches — there is no
field anywhere in that sentence, it is already the answer. «Fungi fruit on
rotting wood, before clumping» names an upstream quantity explicitly. Flora
supplied both sentences correctly; nothing in the schema could receive the
difference, so one rule got applied to both and one of them was wrong.
**Requested of the lead and of core: `FloraEdgeRule` gains a `DensityBasis`
column, and `clump_applies` stops being asked to mean two things.** Until it
lands, the two ForestFloor rows carry the distinction in their comments.

**RULING M-4 — §A7's association changes in the same commit, and it is not the
change that was anticipated.** The count does not rise, so the feared failure
(6.6× the patches piling into rings at the trunk bases) never arises. But
raising the *hit rate* to 0.90 exposes a different §A7 breach that 6/ha was
hiding: **every patch is placed at `trunk + (0, −0.6)` — one constant offset, one
azimuth, one distance, for every trunk in the world.** At 6/ha that is
invisible; at 37/ha it is 37 identical dressings per hectare, and §A7's own
sentence forbids it — «равномерная сыпь запрещается как штамп». So, binding on
the same commit:

- the offset **bearing** is the shade azimuth **jittered**, not a compass
  constant, and −Z is a placeholder for a shade direction nobody has computed
  yet (a sun the world already owns; this is a Rule 35 second consumer waiting
  to happen, and it should be named as such before it is);
- the offset **distance** scales with the trunk it touches rather than being
  0.6 m everywhere — a patch at the base of a 1.5× giant oak standing the same
  0.6 m out is standing *inside* the trunk;
- **acceptance:** the distribution of patch bearings around their anchors is
  not concentrated — *aggregation:* circular variance of the bearing over all
  placed MossPatch/ForestFloor instances in the stand; *denominator:* the
  uniform-bearing control on the same anchor set. A single-azimuth build reads
  0 and is the must-fail arm; it is what ships today.

**RULING M-5 — ground moss, if it is wanted, is a SEPARATE ROW.** 263/ha of
patches in leaf litter, on rocks and in hollows is a real and good thing and
this ruling does not forbid it — it forbids *spending an anchored row's number
on it*. Such a row is `EdgeAssociation::Nothing`, `DensityBasis::Base`, carries
its own derivation that must not mention the stem count (or it is the same moss
twice), and must state its relationship to §5.10's `moss_cover` on fallen logs
for the same reason flora already excluded logs from the anchored figure.
**Filed as open, unauthored, and it is flora's to author or to decline.** It is
NOT a blocker for the two zones waiting on this ruling: they are waiting on
M-1, which is settled.

#### 5.10a.3 The PathMargin overshoot is the SAME defect, not a second one

Core measured PathMargin at **2.5–2.7× OVER** authored while ForestFloor ran
3.3–6.6× under, and reported the two compositions — `max(clump, edge×rich)` vs a
pure product — as the difference. **The compositions are not the defect. The
missing normalisation is, in both, and in opposite directions.** The shipped
formula is its own indictment:

```
rho(p) = (per_100m / 100 m) * field(p) / INTEGRAL(edge over the band)
                              ^^^^^^^^   ^^^^^^^^^^^^^^^^^^^^^^^^^^^
                              max(clump, edge*rich)      edge alone
```

**The numerator's weight and the denominator's normaliser are different
functions.** `per_100m`'s own contract says the magnitude is "normalised by the
ramp's own integral … so the placed total is `per_100m` by construction whatever
shape the ramp has" — and it is normalised by the integral of a ramp that is not
the weight being placed with. The overshoot is then not a mystery but a
quotient: `∫max(clump, edge×rich) / ∫edge ≈ 2.5–2.7`, which is what core
measured.

**RULING M-6 — the `max()` STAYS; normalise by the integral of the weight
actually used.** The floor is load-bearing and proven so: core's mutation check
shows a plain product drives cobble's margin to exactly zero and correctly reds
the suite, which is the kept-verge ruling (§1.7 BR-3) doing its job. What must
change is that the normaliser is computed over `max(clump, edge×rich)` — the
same function, sampled on the same lateral grid the placement already walks —
rather than over `edge`. **No row moves, in either habitat.** That is the whole
point: Rule 44's trap is avoided not by choosing a composition but by noticing
that both habitats were dividing by the wrong thing, which is Rule 30's
denominator clause showing up inside an implementation instead of inside a test.

*(Housekeeping for whoever touches the file: `WorldgenScatter.cpp`'s ground-cover
banner cites "§5.11" for the forest floor. §5.11 is seasonal foliage; the forest
floor is §5.10 and this ruling is §5.10a.)*

### 5.11 Seasonal foliage — the palette contract (ruling, stage-4)

The user wants summer, autumn and winter. The seasons themselves are a future
game-design decision; **the palette SHAPE is a catalog decision and it is
cheaper to fix before there are entries in it than after** (flora's flag, and
they were right to raise it early). Ruling on the shape, adopting flora's
proposal because it adds no mechanism:

- **Foliage colour is a per-species palette indexed by SEASON.** The mesh
  stores an **index, not a colour**. A season change is then a data swap, not
  a re-export.
- **A palette entry is a RAMP, not a single value**, so "autumn varies more
  within a crown than summer does" is expressible as a wider ramp with no new
  machinery.
- **Winter costs one boolean, `has_foliage`**, in the same table: false for
  deciduous, true for conifers. **Conifers are season-stable apart from snow**
  (measured: pines stay green in every reference frame), and snow is render's
  and core's, not the catalog's.

**Two design constraints flora cannot see from their side, and they are the
reason this needed a ruling rather than an ack:**

1. **Value separation must hold in EVERY season, not just summer.** §1.5
   separates our species by *value*, not hue — pale birch, mid oak, dark pine.
   An autumn palette that turns oak and birch into two similar warm mid-values
   destroys that separation at `SILHOUETTE_MIN_PX`, and the forest stops being
   readable at distance in exactly one season. **Each season's entry must
   preserve the species' value ORDER.** That is a checkable property of the
   palette table and it is the acceptance test for any season anyone proposes.
2. **Winter opens sightlines, and C1 must not be validated against it.** With
   `has_foliage = false` a deciduous forest's effective width collapses toward
   trunk diameter, so transmittance rises sharply and landmarks become visible
   through woods that hide them in summer. That is a *lovely* seasonal read
   and we should keep it — but it makes visibility season-dependent.
   **RULING: C1, C4 and the sight wedges validate against the WORST case,
   full summer canopy.** Winter may only improve on a passing seed, never
   rescue a failing one. Decided now, in one line, so that no future seed
   passes in February and fails in July.

**Palette sourcing rule, binding:** **never calibrate a palette from
photographs** (flora's finding — the same tree in two frames gave leaf/dark
splits of 76/10 and 53/40 purely on exposure). Colour taken off a reference
photo is colour taken off the camera's metering. Reference photographs are
evidence about **structure** — density, value ratios, silhouette — and are not
evidence about hue. The same caution applies to every reference image this
project uses, not only to foliage.

