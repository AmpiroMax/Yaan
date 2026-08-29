
### 1.5 Readability under the Daggerfall look (low-res, first person)

Internal resolution is `INTERNAL_RES` = 640×360 working default, with 320×180
still on the table (sync №2). With `CAMERA_FOV_Y` = 1.309 rad (16:9 →
horizontal ≈ 1.87 rad):

- Angular resolution: ≈ **275 px/rad vertical, ≈ 340 px/rad horizontal**
  at 640×360; exactly half at 320×180.
- A silhouette needs ≥ ~8 px to read as a shape. Therefore:
  **min readable feature size ≈ distance / 30** at 640×360
  (≈ distance / 15 at 320×180) **(предложение — утвердить:
  `SILHOUETTE_MIN_PX` = 8; design math, not a runtime constant)**.
  - At 250 m (mid POI spacing): features ≥ 8 m read. A house (5–8 m) is
    marginal → hamlets read as *clusters* + smoke (FUTURE), shrine spires
    (10–14 m) read individually.
  - At 550 m (across the testbed): only ≥ 18 m features read → the L0 must be
    a terrain-scale mass (crag), with its man-made topper (tower) becoming
    legible on approach.
- **Skyline rule:** silhouettes read best against sky. L0 and L1 verticals
  (spire, tower, lone pine) are placed on convex ground so that from the main
  corridors their top third breaks the horizon line. Implementable: candidate
  scoring by "sky behind top of bounding box" raycast.
- **No thin features at distance:** sub-pixel elements (masts, thin branches,
  fences) shimmer at low res. Distant-readable assets use thick silhouettes
  (§5, §6); nothing structural thinner than ~0.5 m matters beyond 100 m.
- **Value contrast over hue:** with the limited palette, tiers separate by
  *value* (dark crag vs light sky, pale birch vs dark pines). Every landmark
  brief in §5/§6 states its value contrast against its usual backdrop.
- **THE MISSING HALF OF THE SKYLINE RULE (added stage-4, and its absence cost
  this stage a session).** «Value against sky» governs the landmark's
  **OUTLINE**, where the competing surface is bright sky and value separation
  is enormous and free. It says nothing about the landmark's **BODY**, where
  the competing surface is **another dark mass in front of it** — and there the
  same doctrine inverts: **value is the weakest axis available, not the
  strongest.** Backlit dark against backlit dark is the weakest separation
  there is, which is precisely the hour §7.1b's verdict frame deliberately
  picks. **Rule: every landmark brief states its separation from its usual
  FOREGROUND as well as from its backdrop**, and where that foreground is
  vegetation the separation is carried by **hue and by silhouette scale**,
  never by value (§5.12, §1.3b).
- **AND THE LIMITED PALETTE ARGUES FOR HUE, NOT AGAINST IT — ~~a correction to
  the bullet above~~, NOW SCOPED TO QUANTISER-ON (stage-5).** It was written as
  a correction and it is not one: it is **true of the quantised configuration
  and inapplicable to the full-colour one**, where there are no ramps to change
  and therefore no ramp-change signal to be robust. **Both bullets are right,
  each in its own mode, and the bullet above governs the basis** (§1.5.1). From
  reading what the palette actually is: the 64-colour post is
  **8 ramps × 8 shades**. So the palette quantises *hue* into eight
  large, well-separated families and *value* into eight fine steps within each.
  **A ramp change is therefore the coarsest and most robust signal the palette
  can carry, and a step change is the finest and most fragile.** «Value
  contrast over hue» is sound as general low-resolution art direction and it is
  **backwards for this palette**: under quantisation, two things on different
  ramps can never merge, while two things on the same ramp merge as soon as
  they land within a step. **This is the whole argument for the conifer ramp
  (§5.12): a ramp is the strongest separation available, and the single most
  common dark mass in the world does not have one.**
  - **PRECONDITION, and without it the sentence above is a trap (render's
    amendment, measured, §4.2): A SEPARATOR MUST MOVE RED OR GREEN.** The
    quantiser weights R/G/B at **0.30 / 0.59 / 0.11**, so a hue difference
    living in blue moves almost nothing — 0.2 of blue is 0.9 shade steps, 0.2
    of green is 2.1. **A ramp change is the strongest signal to the EYE; it is
    only a signal at all to the QUANTISER if it moves R or G, and the quantiser
    runs first.** Two colours that look utterly different can be identical
    after the post. Every separation claim in this document is checked against
    the metric, not against the hue wheel.
- **~~MEASURE WITH THE QUANTISER ON, CERTIFY BOTH~~ — RETIRED AS A BASIS (user
  ruling, stage-5).** «давай цвета фигачить по полной, потом если что ужмем
  палитру.» **The look is developed at FULL COLOUR. The quantiser becomes a
  late pass fitted to a finished world, rather than the world being shaped to
  fit it.** Sixty-four entries stop being a design constraint. Not up for
  re-litigation; the substance of what it changes is §1.5.1 and §4.3.
  - **And the doctrine was also UNSOUND, which nobody had noticed, and I am
    recording it because the user's ruling is now resting on a claim I can no
    longer defend.** It rested on «the quantiser can only ever MERGE
    neighbouring colours and never split them», hence «measured with it on is a
    lower bound». **The second half is false, and the counter-example was
    already printed two sections below it.** A quantiser splits as readily as
    it merges: two colours either side of a Voronoi boundary land on entries a
    full step apart. That is not an edge case — **it is exactly what banding
    is**, and §4.2 had shot a banding frame in the same evening the doctrine
    was written. **Measured against the live artefact** (`BgfxPalette`,
    `palette_separation_steps`, swept along a lit rock flank and along the sand
    family):

    | family swept | largest separation the quantiser INVENTS | largest it DESTROYS |
    |---|---|---|
    | rock (8 shades) | **+0.83 steps** (from a true difference of 0.001) | 0.81 steps |
    | sand (4 shades) | **+1.98 steps** (from a true difference of 0.001) | 1.44 steps |

    **Against a threshold of 2, the instrument's own error is ±1 to ±2 steps —
    and it is worst on the coarsest family, which is where the decisions are
    hardest.** «One measurement certifies both» was never available. The right
    reading is not that the old doctrine was wrong-headed but that **an
    instrument which quantises its own inputs cannot measure a threshold of the
    same size as its lattice.**
  - **The conifer ramp is NOT a precondition for anything at full colour** —
    see §4.3, where I disagree with the framing I was handed and say why.

#### 1.5.1 WHAT FULL COLOUR COSTS THIS SECTION, RULE BY RULE (stage-5)

The user's ruling is a change of basis, not a note. Several rules here were
derived on a limited-palette premise and their **justifications** have to be
re-stated, not merely their status. Taken one at a time, and the honest answer
is different in each case.

**1. «Value contrast over hue» is RESTORED to governing, and the reason is not
that the correction was wrong.** The correction («a ramp change is the coarsest
and most robust signal the palette can carry») is a statement about a *lattice*.
Delete the lattice and it has no referent. What remains is the original
low-resolution argument, which never needed the palette: at 640×360 a silhouette
survives minification by its luminance step, and the pipeline's own metric
weights luminance at 0.30/0.59/0.11 precisely because that is where the eye's
sensitivity is. **Value governs. Hue is a second axis that is now free of the
quantiser's veto** — see 3.

**2. The separation criterion loses its unit but keeps its form** (§1.3b,
re-derived there). This is the one real casualty and it is handled rather than
noted.

**3. RENDER'S AMENDMENT IS DOWNGRADED FROM A CONSTRAINT TO A WEIGHTING, and
this is the largest thing full colour gives back.** «A separator must move RED
or GREEN» was a **hard gate**: a blue-only difference could be *annihilated*,
both colours landing on one entry, separation exactly zero. At full colour the
same difference is **attenuated but never destroyed.** Measured on the live
metric:

| separator | full colour | quantised |
|---|---|---|
| 0.2 in BLUE only | **0.85 rulers** | 1.31 |
| 0.2 in RED only | 1.40 rulers | 1.02 |
| 0.2 in GREEN only | 1.96 rulers | 2.01 |

**Blue buys 0.43× what green buys per unit, so a blue separator must be ~2.3×
larger — but it is no longer forbidden.** The design vocabulary regains its
blue axis at a stated exchange rate. Note also that the quantised column is
*not* consistently lower: it reads blue HIGHER than the truth and red LOWER,
which is the ±1-step lattice noise of §1.5 again.

**4. AND THE HEADLINE DEFECT OF STAGE 4 IS NOT A PALETTE ARTEFACT. Measured,
and it is the finding I most want travelling with this ruling.** Pine against
rock in shadow — the colour half of «the forest was eating the mountain», the
thing the user rejected in words:

> **full colour 0.632 rulers; quantised 0.700.** Full colour is **WORSE**.

Nobody may read «develop at full colour» as «the merge was a quantiser
problem». It was not. **§5.12's apron stands entirely**, §4.2's «in deep shadow
the only separator left is silhouette» stands entirely, and both are now
established on the configuration we actually design in rather than on the one
we do not.

**5. What is genuinely no longer decided here: sixty-four.** Every allocation
argument in §4.2 — which family gives up shades, whether water is 7 or 8 — is a
question about a *late pass fitted to a finished world*, and answering it now
fits the pass to a world that does not exist yet. §4.3 records what survives of
it as input to that pass.

### 1.6 THE ACCEPTANCE FRAME — what a frame certifies, and from how far (doctrine, stage-4)

§7.1's oldest clause is **the frames outrank the numbers.** That clause is only
safe while a frame is a *verdict*. This stage shot four frames and **not one of
them was**: the 717 m verdict frame had no mountain in it, the 287 m rhythm
frame was owned by a pine stand, the frame-2 hour lit the subject backwards,
and the 400 m frame I ruled a dome from turned out to be attributable to a
generator bug (§2.8.8). Four for four is not bad luck; it is a missing
definition. This section supplies it, so the clause keeps the authority it has
earned.

**A frame is a VERDICT on its subject only if all five conditions hold. A frame
that fails any of them is a DIAGNOSTIC — useful, often decisive, but labelled,
and never relayed upward as the state of the world.** Render labelled the 400 m
shot a diagnostic correctly and I then reasoned from it as a verdict anyway;
the label only works if the reader honours it.

| # | Condition | Fails when |
|---|---|---|
| **F1** | **RESIDENCY** — the subject exists in the world and in memory at the standpoint | the 717 m frame: chunks not loaded |
| **F2** | **BUDGET** — the subject subtends enough readable units to *afford* the features under test | the 600 m I11 row: twelve units for a six-feature test |
| **F3** | **AUTHORSHIP** — everything the frame credits or blames is authored | any frame whose composition clause rests on unauthored fBm |
| **F4** | **STANDPOINT** — derived, C1-credited, on reachable ground, at eye height | the tabled (545, 165) inside a pine stand |
| **F5** | **LIGHT** — chosen to expose *this frame's own* failure mode | frame 2's first hour |
| **F6** | **RESOLUTION** — judged at the resolution the player plays at | every frame this stage, judged at 2560×1440 |
| **F7** | **THE FRAME MUST BE ABLE TO FAIL** — it contains the subject across the range the property under test varies over | the lake-bluff frame: sand at one luminance cannot show banding however bad the ramp |

F4 and F5 were ruled in §7.1b and are unchanged. F1–F3 and F6 are new.

##### F6 — AN ACCEPTANCE FRAME IS JUDGED AT `INTERNAL_RES`, NOT AT WINDOW SIZE

`INTERNAL_RES` is **640×360**, and every readability judgement in this document
is angular and calibrated to it (`SILHOUETTE_MIN_PX` = 8 px ⇒ readable size =
distance/30). A frame judged at a higher resolution than the game draws credits
the subject with structure the player's screen cannot resolve.

- **RULING: an acceptance frame is captured at, or downsampled to,
  `INTERNAL_RES` before any acceptance judgement is made**, and the frame
  records the internal resolution it was shot at. A frame that does not state
  its resolution is a diagnostic.
- **SATISFIED by this stage's sweep — checked by the lead, not assumed.**
  `settings.cfg` carries `internal_resolution=640x360`, `tools/run_tour.sh`
  shoots at `DFN_INTERNAL_RES=640x360`, and the PNGs are 2560×1440 = **exactly
  4× in both axes**, i.e. an integer framebuffer upscale. **The files contain
  no detail the player does not have — a one-pixel band lip is four file
  pixels, magnified rather than invented.** The frames stand and the sweep is
  not re-run.
- **MY «CUTS TOWARD FLATTERY» READING IS WITHDRAWN, and the way I got it wrong
  is worth more than the claim was.** I correctly refused to assert the premise
  — I wrote that the capture path was render's to state and that I had not read
  it — **and then built a conclusion on it in the same breath.** Flagging a
  premise as unchecked does not make it checked. That is Rule 34 in its
  subtlest costume: not reasoning from a premise I believed, but reasoning from
  one I had explicitly labelled unknown, as though labelling it discharged it.
  The rule is *check it or draw nothing from it.*
- **What survives, and it is a different point:** the band lips on the
  trees-off flank are ≈ 1 internal pixel, far under `SILHOUETTE_MIN_PX` = 8.
  They are genuinely **visible as value texture on the body** and they are
  **not readable structure**, which is exactly the distinction §2.8.7 drew when
  it explained why ribs read as value on the body rather than as silhouette.
  Visible ≠ readable, and only the second one satisfies a criterion.
- **Keep the condition.** It would have caught a native-resolution capture, and
  the failure it guards against is the same one as §1.6.1 in the other axis:
  there I measured at a distance nobody derived; here the risk was judging at a
  resolution nobody declared.

##### F7 — A VANTAGE THAT CANNOT FAIL IS NOT EVIDENCE (render's formulation, adopted)

**Rule 30 in a frame instead of in a test.** An acceptance frame must be capable
of showing the failure it is taken to exclude; otherwise «I see no defect»
reports the absence of a test.

Render nearly filed a clean result off the lake-bluff frame before catching it:
it *did* contain sand, **flat and at essentially one luminance — and a strip at
one value cannot show banding across a 4-shade ramp however bad that ramp is.**
The frame that did fail contained a large bank across a real lighting gradient.

- **Generalised: the frame must contain the subject across the RANGE the
  property under test varies over.** F2 is that condition in angular size; this
  is the same condition in whatever dimension the property lives — luminance
  for a tonal test, bearing for a silhouette test, distance for a legibility
  test.
- **Corollary, and it is why this is not merely F2 restated: a property that
  varies with viewing AZIMUTH needs a frame set that varies azimuth.** Flora's
  birch cards read correctly from most bearings and as bare poles from the
  edge-on one; a single standpoint certifies a single azimuth. Render's crag
  sweep already does four bearings — **that is now a requirement rather than
  thoroughness.**
- **State, per frame, what would have to appear in it for the test to fail.** If
  that sentence cannot be written, the frame is not an acceptance frame.

#### 1.6.1 F2 — AN ACCEPTANCE DISTANCE IS A PROPERTY OF THE LANDMARK, NEVER OF THE PROJECT

**This is the ruling the stage was missing, and it invalidates the number the
whole evening was measured against.**

§1.5 fixes the readable feature size at `distance / 30`. Define one **readable
unit** as that size. A landmark of base radius `R` seen from `d` subtends
`2R/d` radians, which is

> **U(d) = 60·R / d readable units.**

A view-space invariant does not demand a *shape*, it demands a **feature
count**, and every feature costs readable units — its own, plus the flanking
run on each side that the detector needs to establish a tangent before and
after it. I11 demands three interior breaks, and its outline also carries an
apex and two hem junctions that consume budget without being counted: **six
features.** At roughly two units apiece that is **twelve units with zero
slack**, and this document has twice refused zero slack («a marginal pass on
one seed is not compliance», «a generator input must never equal the floor of
the invariant that checks it»). Taking the same ≈ 1.6× margin those rulings
took:

> **`LANDMARK_ACCEPTANCE_UNITS_MIN` = 20 readable units (предложение —
> утвердить)**, whence the acceptance distance for a silhouette-structure
> frame is **d_accept = 60·R / 20 = 3·R.**

**Derived first, then checked — not fitted.** The measurement was run
afterwards and agrees: I11 fails from one bearing at 400 m (18 units), passes
everywhere at 300 m (24 units) and reads 9–12 against a floor of 3 at 253 m
(28 units). The break-even sits between 18 and 24 units and 20 is inside it.

| Landmark | R | d_accept = 3R | The number that was actually used |
|---|---|---|---|
| **L0 Ravenscar** | 120 m | **360 m** | **600 m** |
| **LR temple massif** | 260–310 m | **780–930 m** | 600 m |

**So the 600 m acceptance distance was written for the LR and then applied to
the crag, and the LR does not exist** (§1.6.3). Read straight down that table:
600 m is *conservative* for the mountain it was written for and *impossible*
for the one it was used on. The evening's headline — «the massif reads as
broken rock up close and as a smooth mass from the valley» — is that mismatch,
measured. It is not a finding about Ravenscar's shape.

- **Corollary, and it is the honest cost of this ruling: RAVENSCAR IS TOO SMALL
  TO HAVE A FAR FRAME DISTINGUISHABLE FROM ITS NEAR FRAME.** §7.1b's two frames
  were split 717 m / 287 m on the premise that «the dome failure and the
  constant-gradient failure are invisible to each other's distance». At 3R the
  verdict frame lands at 360 m and the rhythm frame stays at 287 m, so the two
  frames now differ by **their clause and their light, not by their range.**
  That is not a defect in the frames; it is what a 120 m landmark is. A far
  frame is a thing only a large landmark has.
- **Metric structure and angular structure have DIFFERENT acceptance
  distances, and the frames must not be given a shared one.** A band pair
  (riser + bench ≈ 28 m) is sized in metres and is readable to 28 × 30 ≈ 840 m;
  a silhouette break is sized in units of the landmark and is readable to 3R.
  Frame 2's clause therefore survives far past frame 1's. **The binding
  distance is always the clause's, never the frame's.**
- **This does NOT retire Rule 33; it completes it.** Rule 33 says detail is
  sized against the viewing distance. F2 says the viewing distance is itself
  derived — from the landmark and from the invariant's feature count. Sizing
  detail against a distance nobody derived is how a 5 m tor came to sit on a
  190 m mountain.
- **A landmark photographed beyond its own d_accept certifies nothing about its
  shape.** It certifies that the engine can draw it. That is a render result
  and it is reported as one.
- **Acceptance distances are DERIVED, NEVER TABLED** — the same rule §7.1b
  already imposed on vantages, for the same reason, now extended to the range.
  A tabled distance is a tabled coordinate wearing a different hat.

#### 1.6.2 F3 — what a frame of unauthored backdrop actually certifies

**Measured in the generator, not assumed** (`WorldgenMacro.cpp`,
`TestbedLayout.h`, `App.cpp`): the world is 4 × 4 chunks of 256 m = **1024 m
square**, closed by physics walls at its edge. Inside it, authored influence is
**a set of stamps with hard finite footprints** — the massif reaches 162 m from
its centre and returns exactly zero beyond that; the troughs reach 96–128 m;
the bumps and the lake basin end at their own radii. **Everywhere else, inside
the box as much as outside it, the terrain is three octaves of value noise with
a valley redistribution, 0–31.5 m of relief, and nothing else.** «Authored» is
not a region of the map. It is a union of footprints, and it is small.

Rulings:

- **Unauthored FOREGROUND does not invalidate a frame.** Ground is allowed to
  be ground; §2.7 governs it and it is not the subject. A verdict frame on
  Ravenscar crosses hundreds of metres of fBm and is unharmed by it.
- **Unauthored BACKDROP invalidates every COMPOSITION clause.** Hierarchy
  contrast (C4), dominance (C2), depth separation (§1.3a) and «reads against
  rock rather than against sky» (§6.1) are all claims about the *relationship
  between two authored masses*. A frame containing one authored mass and a
  field of noise cannot carry any of them, whatever it looks like.
- **Unauthored terrain touching the subject's OUTLINE contaminates the
  silhouette clause specifically**, because a noise knoll behind the crag
  enters the horizon polyline and is read as the crag's own crest. I11 already
  guards this by limiting its azimuth sweep to the subject's own angular extent
  × 1.35; **that guard is hereby general — every view-space test states the
  angular window within which it attributes structure to its subject.**
- **A frame whose subject is unauthored is a RENDER test, not a design test.**
  It certifies draw distance, streaming, fog and palette. Legitimate, valuable,
  and not evidence about any rule in this document.
- **Therefore: extending the world does not extend the authored world.** A
  2 × 2 km map with the same five stamps in it has exactly as much design in it
  as the 1 km map does, and four times as much backdrop. Growth of the map is a
  render and streaming milestone; growth of the *authored* world is a placement
  pass, and only the second one moves any rule in this bible.

#### 1.6.3 F3, second half — a landmark rule written for 4 km, in a world 1 km across

**Ruled plainly, because the alternative is to keep quoting numbers that mean
nothing: NO RULE IN §1.3 OR §1.3a IS CURRENTLY EITHER PASSING OR FAILING BEYOND
THE AUTHORED EXTENT. IT IS UNSHOT.**

- **`LANDMARK_MAX_DISTANCE` = 4000 m is a SITING CEILING and a depth-precision
  bound. It is not, and never was, a legibility specification.** Its derivation
  — beyond this a landmark is backdrop, and this bounds what the depth buffer
  must resolve — is untouched and stands. What it cannot do is certify
  anything, because nothing has ever been sited past 1 km: **there is nowhere
  to put it.**
- **Every C1 / C2 / C3 / C4 figure in this document is a statement about a
  1024 m world** and is to be reported with that extent attached. They are not
  wrong. They are narrower than they read.
- **THE LR DOES NOT EXIST IN THE GENERATOR.** Core established by search that
  no code path reads any `LR_` constant; the world contains three raised
  landforms — the crag, a +6 m knoll and a +10 m bluff. Every ruling I have
  made about the temple massif — its relief, its base radius, its ascent, its
  seven landings, its haze separation from the L0 — is **unvalidated by
  construction.** §1.3a's whole depth-separation doctrine needs two authored
  landmarks and there is one.
- **Design stops refining LR numbers until an LR stamp exists.** Tuning a
  constant for an absent object is the purest instance of the defect this whole
  stage is about, and I have been doing it all evening. `LR_BASE_RADIUS` landed
  tonight against a shape nobody has generated. The §2.8.7 line «the LR is
  worse and is the cheap one to fix» was right that it is cheap and wrong about
  what it was: not a fix, a specification.
- **NEW STATUS CATEGORY, and it is the reporting half of Rule 30.** An
  invariant or rule is **PASSING**, **FAILING**, or **UNSHOT** — and **UNSHOT
  never enters a count.** «Seven of eight» and «nine of eleven» were both
  produced by counting rules of unequal standing and unequal shootability as
  interchangeable tallies. A suite is reported as a list with its load-bearing
  member named (§2.8.8), never as a score.

#### 1.6.4 F1 — residency is chunk-granular, and it does NOT block Ravenscar

**Correcting §1.3a's box on a measurement rather than leaving it to stand.**
`ChunkManager` loads chunks within `CHUNK_LOAD_RADIUS` = 2 measured as
**Chebyshev distance in chunk units** around the focus, and clips everything
outside the 4 × 4 extent. So «the world stops existing at 512 m» is the right
order of magnitude and **the wrong shape**: residency is a square in chunk
space, not a radius in metres.

- For a subject in chunk (3, 0) — Ravenscar — the residency-legal standpoints
  are **x ≥ 256 m and z < 768 m**, and the greatest legal distance to it is
  ≈ 808 m, only from the south-west.
- **Frame 1's tabled vantage (120, 300) is illegal on its BEARING, not on its
  RANGE** (chunk x = 0 is three chunks from the subject). I record this as a
  **prediction from reading `ChunkManager.cpp`, to be checked by render before
  anyone relies on it** — I have not read how a probe sets its streaming focus,
  and whether the focus follows the camera or the player is render's to state,
  not mine to assume.
- **LOD is NOT the precondition for Ravenscar's acceptance.** At the derived
  d_accept of 360 m the verdict frame is shootable tonight, and render's
  253 m / 300 m acceptance sweep already is. §1.3a's box stays true for the
  LR and for anything beyond ≈ 800 m, and is **withdrawn as a blocker on the
  L0's own frames** — it was blocking them at a distance that was never theirs.
- **`CHUNK_LOAD_RADIUS` still must not rise for a screenshot**, and design
  still does not ask for it. That part of the box was right.

#### 1.6.5 Two conduct rules this stage earned, in transmissible form

Both were learned by being caught, and «I caught it in myself» does not
transmit. These are the greppable versions.

**A HEDGE IS A DEBT, NOT A LICENCE.** Naming a premise as unverified creates an
obligation to verify it or to drop every conclusion resting on it. **A message
containing both «I have not checked X» and a conclusion that depends on X is
self-refuting**, and that is mechanically detectable in one's own draft before
sending. This is Rule 34's subtlest form: not reasoning from a premise one
believes, but from one already labelled unknown, as though the label discharged
it. It cost this document one wrong finding (F6) which the lead settled in
about a minute by *reading the file I had declined to read*.

**A CITATION IS A CLAIM ABOUT A DOCUMENT AT A MOMENT, AND IT GOES STALE IN
SILENCE.** The stale tree heights in the occlusion model carried **correct
citations of a superseded ruling** — the code said «§5.1: 8–12 m» and §5.1 had
said exactly that, before §5.7 moved it to 24–32 m. **The code documented its
provenance faithfully and was wrong anyway**, and the citation made it *harder*
to spot: a bare literal invites suspicion, a cited literal buys trust it has not
earned. Design's share of the fix, since this document is what gets cited:

- **When a ruling supersedes a numeric range that has appeared in this
  document, the ruling says so explicitly and names the section it replaces**,
  so a grep for the old section number finds the correction.
- **A number in this document is never the source of truth for code — NUMBERS.md
  is** (Rule 14). Where a §5 or §2 brief quotes a value, it is quoting, and a
  reader who finds it in code has found a shadow, not a reference.

---

