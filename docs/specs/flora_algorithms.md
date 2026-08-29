
# Flora — tree generation algorithms (literature review and selection)

**Why this file exists.** The user rejected all three trees on 09.08.2026 and
asked, in as many words, for *proper tree generation algorithms, searched for
hard*. This is that search, the citations, and the decision of which algorithm
each species gets and why. `docs/specs/flora.md` remains the zone spec; this is
the record it points at, so that a successor never has to re-derive the choice.

**Rule 34 discipline applies to papers too.** Everything below that is stated as
a fact of the literature was read in the primary source (the PDF text is quoted
where the wording matters), not recalled.

---

## 0. The diagnosis, measured before any reading

Three complaints, one sentence each, and what the code actually does.

### 0.1 «дубы имеют случайно наложенные листья, не прикрепляющиеся к ветвям»

**Confirmed, and it is not an impression — it is what the code is written to
do.** `ProcFlora.cpp::scatter_envelope_clusters()` distributes the crown over
the species envelope *"independently of the skeleton"* (its own comment). It was
added deliberately, as the durable half of the fix for defect 3 in
`flora.md` §3.7 — the crown that did not exist — on the reasoning that foliage
is what reads at distance and must not depend on branches surviving the shadow
floor. That reasoning was right about the failure it was fixing and wrong about
the tree.

Measured on the built geometry (12 variants, `Full` LOD, distance from each
card's centroid to the **nearest wood vertex**, which *over*-states attachment
because wood vertices are only every 1–3 m along a limb):

| species | cards | mean gap | worst gap | worst gap / card reach |
|---|---|---|---|---|
| DaleOak | 404 | **2.60 m** | **6.91 m** | 1.39 |
| RiverBirch | 309 | 1.49 m | 2.94 m | 0.88 |
| ValeWillow | 399 | 1.61 m | 3.42 m | 1.01 |

**The average oak leaf cluster floats 2.6 m from the nearest wood and the worst
floats 6.9 m.** The user is describing the geometry correctly.

### 0.2 «елки просто юбки большие»

Confirmed. `HighlandPine` has `foliage = ConeShell` and its crown is
`build_cone_tiers()`: two to three solid cones swept as tapered tubes on the
trunk axis. There is no whorl, no branch bearing needles, no gap. The whorl code
path in `build_flora_mesh` exists (`sp.whorled`, three rings of six) but it emits
*wood only*; every needle in the world is on a cone shell. A skirt is the exact
name for a solid surface of revolution hanging from an axis.

### 0.3 «белое дерево выглядит как пальма… как острые пики»

Confirmed in `screenshots/crag/01_crag_sw_253m.png`, which is the frame that
shows it worst: pale, near-white, sharply tapering, curved 16–22 m poles in
groups of two and three, bare for the lower 58 % of their height
(`BIRCH_CROWN_BASE_FRACTION_MIN`), carrying a small tuft of cards at the top —
and, where the crown lands under the aspect derivation, carrying nothing at all.
Bare stem + terminal tuft **is** the palm silhouette; two or three of them from
one root **is** a spike cluster. Every individual rule that produced it was
defensible (see §3.3), which is why it took four attempts to see.

---

## 1. Runions, Lane & Prusinkiewicz 2007 — Space Colonization

> A. Runions, B. Lane, P. Prusinkiewicz, *Modeling Trees with a Space
> Colonization Algorithm*, Eurographics Workshop on Natural Phenomena (2007),
> pp. 63–70. <http://algorithmicbotany.org/papers/colonization.egwnp2007.html>
> PDF: <https://algorithmicbotany.org/papers/colonization.egwnp2007.pdf>
> A 3D extension of the authors' open leaf-venation model
> [Runions, Fuhrer, Lane et al., SIGGRAPH 2005].

### 1.1 The algorithm, from the paper

Start with N attraction points (*"usually hundreds or thousands"*) and one or
several tree nodes. Each iteration:

1. **Associate.** Every attraction point influences *the tree node closest to
   it*, provided that node is within the **radius of influence `di`**. The set
   of points influencing node `v` is `S(v)`.
2. **Grow.** For every `v` with `S(v)` non-empty, create `v'` at distance `D`
   from `v` along the normalized sum of the *normalized* directions to its
   sources:

   > `v' = v + D·n̂`, where `n̂ = n⃗/‖n⃗‖` and `n⃗ = Σ_{s∈S(v)} (s−v)/‖s−v‖`   (eq. 2)

   Optionally biased by a vector `g⃗` carrying tropism and branch weight:

   > `ñ = (n̂ + g⃗)/‖n̂ + g⃗‖`   (eq. 3)
3. **Kill.** An attraction point `s` is removed once some node is closer to `s`
   than the **kill distance `dk`**.

Note the asymmetry that makes the algorithm work: **one point influences exactly
one node** (its closest), but one node may be influenced by many points. That is
what makes two branch tips near each other diverge instead of merging, and it is
why the paper can state that *"branch intersections are prevented by the nature
of the algorithm."*

### 1.2 The parameters, with the paper's own values

`dk` and `di` are both expressed as multiples of `D`, the node step.

| parameter | paper's values | visual effect (paper's own wording) |
|---|---|---|
| `N` | 375, 1 500, 12 000 (fig. 3) | *"Decreasing N and increasing dk yields crowns that are increasingly sparse."* Small N also gives **irregular** branches, because adding or removing one point can swing a tip. |
| `dk` | `2D` and `20D` (fig. 3) | Large `dk` → the point set per tip is larger, individual points matter less, **smoothly curved branches**. Small `dk` → the tip must reach each point, finer and denser. |
| `di` | `∞`, `8D` (trees, fig. 5), `17D` (shrubs, fig. 4) | *"As its value decreases, branch tips tend to meander between attraction points, coming into, then leaving their zones of influence; this results in a wiggly or gnarly appearance."* |
| `D` | the unit of length; *"we do not have an algorithmic criterion for choosing the optimal value"* | dominates run time |

### 1.3 The five findings that decide our design

1. **Foliage cannot be detached from a branch.** A point survives only until a
   node comes within `dk` of it. So every *consumed* attractor has a node within
   `dk`, and `dk` is a small multiple of the node step. Put the leaves on
   consumed attractors (or on the tips that consumed them) and §0.1 is not fixed
   — **it is unrepresentable**. This is the whole reason to adopt the algorithm.

2. **Crown shape is the attractor envelope, so our silhouette guarantee
   survives intact.** Fig. 6 generates a columnar crown and a conical crown from
   nothing but the envelope the points fill. `flora.md` §3.1 stage D calls the
   envelope *"the single most important design decision in the zone"* because an
   emergent silhouette is unaffordable at 640×360. Space colonization does not
   weaken that: it changes the envelope from a **clip applied after growth** to
   the **cause of growth**. Strictly better, and `CROWN_ASPECT_MAX` still gets
   measured on the built tree.

3. **Excurrent vs decurrent form is emergent from crown width.** *"Narrower
   trees have a clearly delineated trunk, whereas in widely spread trees even the
   main limbs are highly ramified."* We get the oak's forking limbs and a
   conifer-ish leader from one mechanism with two envelopes — although see §2 for
   why the conifer still does not get this algorithm.

4. **Points near the ENVELOPE SURFACE give an open crown with twigs only on the
   crown shell** (fig. 7). *"In many trees and shrubs the density of branches
   increases near the crown surface due to better access to light. We generate
   the resulting forms by increasing the density of attraction points near the
   envelope."* This is the same object §3.10 of `flora.md` measured in the
   user's photographs from the other side: dark limbs in the interior, leaf mass
   on the outside, porosity a rim effect. **A radial density ramp on the
   attractor cloud is one line and buys the reference look.**

5. **Weeping is the algorithm's admitted failure.** *"although we were not able
   to generate strongly pendulous, 'weeping' trees with this approach"*. So the
   willow keeps an explicit droop applied to its tips; we do not chase it with
   `g⃗`. Recorded because a successor will otherwise try.

### 1.4 The post-process, which we want and which is nearly free

The paper's pipeline is (a–c) colonize → (d) **decimate** the skeleton → (e)
**move each remaining node halfway toward its more basal neighbour**, which
*"reduces the branching angles"* → (f) curve subdivision → (g) generalized
cylinders → (h) organs.

Step (e) is the cheap cure for the algorithm's one ugly habit — raw space
colonization forks at close to a right angle, which reads as a candelabra. It
costs one pass over the node array. Step (d) is how we hit the triangle budget.

### 1.5 Branch radius — the pipe model, and it is what the reference photo shows

Radii are computed **basipetally** (tips → base), all tips starting at `r0`:

> `rⁿ = r₁ⁿ + r₂ⁿ`   (eq. 1), *"where n is a parameter of the method (usually
> between 2 and 3)"* [Shinozaki et al. 1964; Macdonald 1983 pp. 131–135].

This is the property that makes a tree read as a tree at low resolution: limb
thickness is a *record of how much crown is above it*, so the eye gets the
hierarchy for free. Our current generator instead multiplies a fixed
`radius_ratio` per generation, which produces limbs whose thickness carries no
information.

**And it interacts with our shadow floor in exactly the useful direction.**
`flora.md` §3.5 forbids anything under 0.35 m diameter. Under the pipe model the
main limbs get *thicker* than the current per-generation ratio gives them
(defect 3 measured birch primaries at 0.168 m — under the floor, which is how
the entire birch crown once vanished), because their radius is driven by the
count of tips they support rather than by a decay constant.

### 1.6 Cost, which is the reason this is affordable at all

The tree is baked, not simulated: `FLORA_VARIANTS` (12) × species × LOD, built
once. The paper reports *"between a few seconds and a few minutes"* per tree
using an exact 3D Voronoi/Delaunay association (CGAL). **We do not need that.**
Association is "nearest node to each point", which over ≤ 600 points and ≤ 400
nodes is 240 k distance tests per iteration — a brute-force pass in
microseconds, and the whole crown converges in a few dozen iterations. The
paper's cost is a consequence of tens of thousands of points and an exact
geometric predicate, neither of which a 700-triangle tree wants.

---

## 2. Why the conifer does NOT get space colonization

Space colonization models **competition**, which is the dominant force in a
mature *decurrent* broadleaf — the paper says so plainly: *"the competition for
space appears to play the dominant role… in determining the overall branching
structure of temperate-climate trees and shrubs."*

A spruce or a pine is the other case. It is **monopolial and rhythmic**: one
leader extends each year and produces a *whorl* — a ring of lateral branches at
one node — and the tree's structure is a stack of those rings. That is a
*deterministic developmental pattern*, not an emergent competitive one, and
feeding it to a competition algorithm throws away the one property that makes a
conifer recognisable. (See §3.2 for the botany and the numbers.)

So: **conifers get an explicit whorl generator.** That is also precisely the
diagnosis of «юбки»: a skirt is what you get when the tiers are a surface of
revolution instead of a ring of individual branches with air between them.

---

## 3. Species decisions

| species | algorithm | why |
|---|---|---|
| DaleOak | space colonization, wide sphere envelope, surface-weighted points | decurrent, ramified limbs, foliage on the crown shell — §1.3.3/§1.3.4 |
| ValeWillow | space colonization + explicit tip droop | §1.3.5 — the paper cannot do pendulous, our `droop` already can |
| RiverBirch | space colonization, single stem, narrow ovoid envelope high on the trunk, pendulous tips | §1.3.3 gives the delineated trunk a narrow crown implies |
| HighlandPine | explicit whorl generator (§2) | rhythmic monopodial growth is not competition |
| Snag / logs / bushes | unchanged | no crown to get wrong |

### 3.1 Sources for the botany

- Whorl = a year, whorl counting, "a complete whorl is one with at least three
  branches, and an average whorl contains 2-7":
  <https://openoregon.pressbooks.pub/forestmeasurements/chapter/4-4-field-technique-tips-for-counting-whorls/>
- Scots pine forms one whorl per year at the top of the leader:
  <https://www.silvafennica.fi/article/5347>
- Branch count per whorl tracks that year's (or the previous year's) shoot
  length — Makinen on *Pinus sylvestris* and *Picea abies*:
  <https://ouci.dntb.gov.ua/en/works/4kWwbp04/> ,
  <https://academic.oup.com/forestry/article-abstract/76/5/525/517568>
- Crown ratio, measured: forest-grown Norway spruce **0.44-0.49**, forest-grown
  Scots pine **0.30**; open-grown **0.91-0.94** and **~0.86**:
  <https://pmc.ncbi.nlm.nih.gov/articles/PMC2987550/>
- Branch insertion angles 40-70 deg from the stem, left-skewed; the
  ascent-to-horizontal transition is fast in the upper crown and then plateaus:
  <https://www.sciencedirect.com/science/article/abs/pii/S0378112707000357> ,
  <https://pmc.ncbi.nlm.nih.gov/articles/PMC8959813/>
- *Picea abies* — strong central leader, horizontal side branches, **vertically
  pendulous branchlets**, "the upper level ascending, the lower drooping":
  <https://research.fs.usda.gov/feis/species-reviews/picabi> ,
  <https://www.conifers.org/pi/Picea_abies.php>
- Self-pruning is predicted by branch age and by size **relative to its own
  whorl**, and a dead-stub band sits below the live crown (the "self-pruning
  ratio"): the two Makinen papers above.
- *Pinus sylvestris* is **uninodal** — one whorl a year, no interwhorl branches;
  needles persist 2-6 years so foliage exists only at the branch ends:
  <https://www.conifers.org/pi/Pinus_sylvestris.php>
- Phyllotaxis: golden angle 137.5 deg; oak 2/5 = 144 deg, willow 5/13, birch
  spiral: <https://pmc.ncbi.nlm.nih.gov/articles/PMC6408360/> ,
  <https://en.wikipedia.org/wiki/Phyllotaxis>
- The crown is a shell of moderate thickness, not a filled volume and not a
  hollow membrane — long (peripheral) shoots carry ~26 % of leaf area, short
  (interior) shoots ~74 %:
  <https://pmc.ncbi.nlm.nih.gov/articles/PMC10107860/>
- Oak habit — few large sinuous limbs, "ample, irregular and never dense"
  foliage "densely grouped at the end of the twigs":
  <https://www.monaconatureencyclopedia.com/quercus-robur/?lang=en>
- *Betula pendula* — "main branches are upright but outer branchlets on older
  trees becoming thin, drooping and flexible":
  <https://www.treesandshrubsonline.org/articles/betula/betula-pendula/>
- Weber & Penn 1995, for the parameter vocabulary and two ideas kept:
  <https://courses.cs.duke.edu/cps124/fall01/resources/p119-weber.pdf>
- Blender's Sapling add-on is the only mainstream tool with an explicit whorl
  parameter (`nrings`); Arbaro and Weber & Penn have none:
  <https://github.com/blender/blender-addons/blob/main/add_curve_sapling/utils.py>

### 3.2 Conifer architecture — the four facts that produced the fix

1. **A WHORL IS A YEAR.** The leader extends one internode and flushes a ring of
   laterals at the top of it. So **whorl spacing is that year's height
   increment** — short at the apex (this year), longest through the vigorous
   middle years, short again at the base — and the number of whorls is the
   crown's age. Evenly stacked rings integrate into a solid; unevenly stacked
   ones do not. *Implemented as a hump-shaped internode ladder built from the
   apex down and scaled onto the crown span.*
2. **BRANCH COUNT TRACKS THE SAME VIGOUR AS THE INTERNODE.** Long internode
   years make fat whorls. *Implemented by drawing both from one per-year
   variable, which is what makes the result irregular rather than merely noisy.*
3. **SELF-PRUNING, and the crown-ratio numbers are the headline.** A forest
   Scots pine carries live foliage on only ~0.30 of its height; a forest spruce
   0.44-0.49. **Open-grown is 0.86-0.94 — and 0.86-0.94 IS a skirt.** Our old
   pine had its crown over the top 62 %, i.e. an open-grown paddock spruce
   standing in a wood. *Implemented as a crown base at 0.45 plus a miss-chance
   per whorl position that rises downward, plus a band of dead stubs below the
   live crown.*
4. **THE PENDULOUS SECOND-ORDER SHOOTS ARE THE SILHOUETTE.** On *Picea abies*
   the first-order branch is near-horizontal and its second-order shoots hang
   vertically off it. *Implemented as foliage anchors hanging under each
   primary — as CARDS, not tubes, because a shoot is one pixel wide at any
   gameplay distance (Rule 33).*

Plus one design rule the old cone was quietly breaking: **§5.2 requires the top
of the cone to stay >= 1.5 m wide** so the tip survives quantization. The
envelope tapered to a point, which also starved the top whorls (any branch under
0.4 m of reach is dropped) and left a bare leader standing above the foliage —
the «острые пики» silhouette arriving at the pine by a different route. The cone
envelope now floors at 0.18 of the crown radius, which is 1.58 m on a 4.4 m
crown: design's rule, satisfied by construction.

### 3.3 Phyllotaxis and where foliage actually lives

The measured answer settles an argument this zone has had with itself twice.
**The crown is a shell of moderate thickness — not a filled volume, and not a
hollow membrane.** Long peripheral shoots carry ~26 % of leaf area; short
interior shoots carry ~74 % but cost 36 % less biomass per unit area. That is
the same object §3.10 measured in the user's photographs from the other side
(porosity is a RIM effect over a 79-86 % opaque core), and it is the same object
Runions' fig. 7 produces by concentrating attractors near the envelope.
**Three independent methods, one answer.** `surface_bias` is the parameter.

The atomic unit is the **leaf-bearing shoot**, not the leaf. That is exactly
what a card cluster is, and it is why cards were the right medium even though
the reason originally given for them (transparency) turned out to be wrong.

---

## 4. What changed in the code

| Deleted | Replaced by |
|---|---|
| `scatter_envelope_clusters()` — the crown distributed over the envelope *"independently of the skeleton"* | foliage anchored to the node that grew to reach it |
| `grow_branch()` — recursive, fixed `branch_count`/`branch_angle`/`length_decay`/`radius_ratio` per generation | `colonize()` — space colonization into the crown volume |
| `build_cone_tiers()` — 2-3 solid cones swept on the trunk axis | `whorl_skeleton()` — leader, whorls, pendulous shoots, dead stubs |
| per-generation `radius_ratio` decay | the pipe model over trunk and crown as one structure |
| branch TERMINATION at the shadow floor (which took the foliage with it) | radii CLAMPED UP to the floor — nothing can be detached by a clamp |

New files, all flora's: `FloraSkeleton.{h,cpp}` (the growers), `FloraBuild.{h,cpp}`
and `FloraNeighbours.cpp` (Rule 21 splits of a `ProcFlora.cpp` that was already
931 lines when this session inherited it). Registered in `dfn_render` by render
at flora's request; flora did not edit their CMake.

**Three defects the rewrite exposed in code that was already there**, all worth
naming because all three had been "fixed" before:

1. **The drill-bit slide was still live.** §3.7.5's cure — shrink a cluster to
   fit, never slide it to the axis — was implemented as `radius = min(radius,
   env)` followed by a slide of `(env - radius)/len`, which is **exactly zero
   when the clamp bites**. So every over-sized cluster still landed on the trunk
   axis. Measured on a shy oak: five clusters at (0, y, 0) carrying 6.2 m of
   card. An axis-centred cluster also escapes crown shyness entirely, because it
   has no azimuth to be shy on. Now: `radius = min(radius, max(env - len,
   0.30 env))` — take the size, never the position.
2. **Containment ran against the proposed height, not the final one.** The
   vertical clamps MOVE a cluster after `emit_cluster` has already contained it,
   and in a cone moving up shrinks the envelope out from under a card that was
   legal a moment earlier. Pine cards reached 6.7 m where the envelope allowed
   2.6. Fourth instance of "enforce on the thing that actually reaches".
3. **Shyness was applied to the foliage but not to the growth.** Shrinking the
   attractor cloud is not enough on its own: a branch can still overshoot toward
   a point and be clipped at the full envelope, so the shy side quietly got its
   width back. The invariant that exists to reject this caught it.

**And one modelling error that was worth more than every parameter in the
table.** Branch tips were grown *to* the species envelope, so the foliage that
hangs off a tip had nowhere to go — containment allows a cluster only the space
between its centre and the envelope, and a tip sitting exactly ON the envelope
got a cluster of 0.30 x env. Crowns came out as a big skeleton wearing small
tufts. **THE ENVELOPE IS WHERE THE FOLIAGE ENDS, NOT WHERE THE WOOD ENDS.** The
growth envelope is now inset from the silhouette envelope by the cluster reach.
Real crowns work this way: the twigs stop short and the leaves make the surface.

---

## 5. The measured result

Max across 12 variants, `Full` LOD, nominal maturity.

| Species | tris F / R / S | height (m) | width (m) | crown aspect | gap mean | gap max |
|---|---|---|---|---|---|---|
| DaleOak | 360 / 308 / 54 | 23.1-30.8 | 10.2-13.8 | 1.27 | 0.22 | 0.68 |
| HighlandPine | 632 / 552 / 36 | 28.0-37.6 | 6.1-8.5 | 2.28 (exempt) | 0.17 | 1.07 |
| RiverBirch | 300 / 266 / 54 | 17.0-21.3 | 5.5-6.9 | 1.78 | 0.24 | 0.85 |
| ValeWillow | 388 / 338 / 54 | 13.9-19.4 | 9.1-12.1 | 1.12 | 0.32 | 1.11 |
| Snag | 60 / 60 / 30 | 10.2-19.1 | — | — | — | — |
| Bush / BigBush | 78 / 114 | 1.0-1.5 / 2.8-3.9 | — | — | — | — |
| FallenLog / Deadfall | 48 / 30 | ⌀0.6-1.0 / 0.2-0.3 | 8.4-13.7 / 2.0-3.7 long | — | — | — |

**Gap is in units of the card's own corner reach**, so 0.23 means the average oak
leaf cluster's centre sits less than a quarter of a card-width from wood — it
overlaps its branch on screen. **The rejected oak measured 2.60 m absolute, worst
6.91 m.** Every species is inside `TREE_TRI_BUDGET_MAX` = 700, and the budget is
now *derived* rather than tuned: the crown is decimated to what is left after the
trunk and the cards are paid for, so it holds across the maturity tiers by
construction.

Two width bands were **calibrated against the built tree** and both had drifted
under their brief with a green suite, which is design's «a range is two
assertions» defect for the fifth time: the pine's envelope was 6.7-9.0 m and its
built crown 4.6-6.6 m against a 6-9 m brief.

**The arithmetic that closes on its own, and the lead is right that it is worth
recording as such.** With tip diameter at the 0.35 m shadow floor and a pipe
exponent of 2.5, a trunk of diameter d supports (d/0.35)^2.5 tips — 22 for an oak
at ⌀1.2 m. That is almost exactly the 12-22 leaf clusters the card budget wants
and the number of foliage masses the botany describes. **The shadow floor, the
triangle budget and the pipe model are three independent derivations landing on
one number.** Nobody should "optimise" it later without knowing that.

---

## 6. The three invariants, and the two that did not work

`tests/render/ProcFloraTests.cpp`, each shipped with the case it must reject
(Rule 30).

1. **Every leaf cluster hangs off a branch that exists.** Per-card ceiling of
   1.5 card-reaches plus a mean of 0.60. *Control: a bare bole with clusters
   distributed over the crown envelope on the golden angle — which is what
   three of the four species actually were, since their primaries fell under the
   shadow floor and terminated. It fails on the mean.*
2. **The conifer is a stack of whorls, not a skirt.** Vertical ROUGHNESS of the
   row-fill profile >= 0.10. *Controls: a tapering cone (0.037) and a solid of
   revolution following this generator's own envelope (0.034), against a real
   pine's 0.150-0.232.*
3. **No canopy tree is a bare pole with a tuft on top.** Limb spread >= 0.15 of
   height AND foliage span >= 0.20 of height. *Control: a tapered stem with all
   its limbs from one node near the tip — 0.06 on both clauses.*

**Two earlier versions of invariant 2 passed and measured nothing, and only the
control exposed them.** First it counted rows whose fill was low against *the
row's own occupied span* — under which a bare stick between two whorls scores a
perfect 1.0 for being a stick. Then it measured against the envelope but
restated the cone profile locally, and went stale within the hour when the
envelope gained its apex floor; the smooth control then scored **gappier than the
real pine**. Three lessons, all of them Rule 30 or Rule 35 in miniature:

- a test that keeps its own copy of a shape measures the copy;
- a control that differs from the thing under test in some *other* way as well is
  not a control, it is a second experiment;
- and the sampler must be denser than the grid it samples into. A fixed 6x6
  barycentric lattice put a cone's tall thin side triangles on 7 distinct
  heights out of 24, so the *solid* control measured as four empty rows in five.

---

## 7. The birch — improved, still the weakest, and the remaining lever is not mine

**What was fixed in this zone:** the clump is gone (2-3 pale poles from one root
is half of the palm read, and §1.5 outranks the field guide here — a real river
birch is multi-stemmed and a legible one is not); the crown is grown rather than
tufted; and branches now leave the bole **below** the foliage line, which is what
`ColonizeParams::grow_from` exists for. The foliage line is untouched by that
change — attractors still fill only the crown volume — so `CANOPY_CLEARANCE_MIN`
and design's crown-base rule hold exactly as written.

**What is left, measured.** The birch's limb spread is 0.17-0.19 of its height
against 0.22-0.35 for the others; it clears the invariant's 0.15 floor but it is
the only species near it. The cause is `BIRCH_CROWN_BASE_FRACTION_MIN` = 0.58:
the attractor cloud can only fill the top 42 % of the tree, so no arrangement of
growth can put crown lower down.

**Tested rather than argued.** A build with the crown base at 0.40 — everything
else identical — is in
`screenshots/flora_grown/01_birch_at_040_EXPERIMENT.png` beside the shipped
0.58 in `00_species_line_44m.png`. The 0.40 birch reads as a slender
light-crowned tree; the 0.58 birch reads as a pale pole with a crown on top.
0.40 is still well above `CROWN_BASE_FRACTION_MIN` = 0.35 (the walkability
floor) and leaves ~7.6 m of clear trunk on a 19 m birch.

**Not landed, because it is not flora's number** (Rule 14/35). The registry row
was derived by design to fix a crown ASPECT problem, and it did — aspect is 1.27
against a ceiling of 1.8. It then created a silhouette problem one level down.
Escalated with the two frames; design and the lead rule.

---

## 8. What only the tour frame showed (Rule 27, discharged 10.08.2026)

The world came back (render's 4096-handle exhaustion: 17 336 lake planes ate the
pool at startup, so every terrain mesh after the water silently failed to
upload — water and a castle drawn, no ground). Three defects were then visible
in one frame that **no isolated render and no invariant had caught**, and all
three are the same kind: a property that is only wrong from certain viewpoints
or against certain neighbours.

1. **TWO CROSSED CARDS HAVE AN EDGE-ON AZIMUTH.** Cards are fixed-orientation by
   design (a billboard shimmers at 640x360 and casts a rotating shadow). The
   birch carried `cards_per_cluster = 2` with a comment saying "a narrow crown
   does not need a third plane". **It does.** Two planes have azimuths where
   both present nearly edge-on and the cluster all but vanishes; three cannot.
   Oak and willow never showed it because they had three. In the frame the
   birches were a line of bare white poles with a few flecks — the rejected
   silhouette surviving a rewrite that had genuinely fixed the shape, purely as
   a viewing-angle artefact. The pine had the same exposure at one card per
   spray and survived only statistically, on 46 sprays; it now has two.
2. **A PALE TREE NEEDS DARK TWIGS OR IT IS A WIRE FRAME.** §3.10 measured that
   the tracery in the reference photographs reads by VALUE CONTRAST — branch 50,
   leaf 135, a 2.54x ratio — and not by transparency. Every species drew all its
   wood in one colour, so the birch's near-white limbs sat at the same value as
   its foliage and the crown read as white scaffolding with leaves stuck on it.
   `twig_color` plus a per-species `twig_radius_frac` fixes it, and it is what a
   real birch is: white bole, dark limbs. **The measurement was already in this
   spec; it had simply never been applied to wood.**
3. **THE ATTACHMENT METRIC WAS OVER-STATING AND FINALLY PRODUCED A FALSE
   FAILURE.** `gap_to_wood` measured to the nearest wood VERTEX, documented here
   as pessimistic. A conifer spray sitting ON the leader at 94 % of tree height
   measured 1.73 m away, because the trunk is 7 segments over 31 m and its
   vertex rings are 4.4 m apart. The card was touching wood; the ruler had no
   marks there. Now sampled over the triangle surface. **The threshold did not
   move** — a better instrument, not a relaxed rule.

### 8.1 Design's sharpening of Rule 30, and why it landed on a different clause

Design's rule: *a synthetic control is the easy reject; when a real rejected
artefact exists, IT is the control, and the floor must sit above it.* My
limb-spread floor was 0.15 with a synthetic palm at 0.06 — and the **rejected
birch measured 0.17-0.19**, so the invariant would have passed the tree the user
turned down.

Applying it took a measurement, and the measurement refused the obvious move:

| species | limb spread | foliage span |
|---|---|---|
| DaleOak | **0.166**-0.341 | 0.549-0.602 |
| HighlandPine | 0.240-0.353 | 0.493-0.505 |
| RiverBirch (repaired) | 0.399-0.442 | 0.555-0.595 |
| ValeWillow | 0.586-0.679 | 0.711-0.758 |
| *rejected birch* | *0.17-0.19* | *< 0.42 by construction* |
| *synthetic palm* | *0.06* | *0.06* |

**The oak's smallest variant sits at 0.166 — below the rejected birch.** A
compact crown on a short tree and a tuft on a tall pole produce the same
limb-spread number from different objects, so no floor on that clause separates
accepted from rejected without failing an accepted species.

**Foliage span does separate them, and by construction.** The rejected birch had
its crown base at 0.58, so its foliage could not span more than 0.42 of the tree
*whatever went in it*; every accepted species measures 0.49-0.76. The floor moved
0.20 -> **0.45**, which rejects the whole CLASS — any tree whose crown starts
above ~0.55 — rather than one instance of it.

Design's rule is right and it is now satisfied. The lesson underneath is that
*which* clause a floor belongs on is itself a measurement.

### 8.2 The birch's remaining margin, stated rather than hidden

At the landed 0.40 base the birch measures **aspect 1.78 against a ceiling of
1.8** — about one per cent of margin, and only after `card_aspect` went 0.95 ->
0.76 to spend vertical reach on width. The lever both the lead and design named
is crown WIDTH, and it is genuinely the right one — a crown starting lower has
longer lower limbs. **But the width band is 5-7 m and the built crown is already
5.5-6.9 m, so there is nothing left to spend.** The aspect is now a structural
consequence of two design bands multiplying: at 16-22 m height with a 0.40-0.44
base and a 5-7 m width, aspect lands at 1.65-1.78 and cannot go lower.

Reported to design rather than resolved here. My own sentence applies to me now:
**the margin is where the palm lives**, and one per cent is not margin.
