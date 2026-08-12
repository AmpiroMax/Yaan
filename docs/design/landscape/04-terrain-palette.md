<!--
Created: 12:08:2026 - 22:57:02
Last updated: 12:08:2026 - 22:57:02
-->
<!--
UPD:
- 12:08:2026 - 22:57:02: Выделен из docs/design/LANDSCAPE.md (9786 строк против FILE_HARD_LIMIT 800): §4, §4.1–§4.3. Чистый перенос — ни одна строка тела не изменена, ни один номер секции не изменён; адреса вида «LANDSCAPE.md §X» продолжают действовать, таблица § → файл живёт в docs/design/LANDSCAPE.md.
-->

## 4. Terrain palette (splat rules a shader can implement)

Inputs per sample, all computable in P3 or in-shader from the heightfield:
`height` (m), `slope` (rad), `dist_to_water`, `height_above_water`.
Priority order (first match wins), thresholds **предложение — утвердить**:

| Priority | Material | Rule | Constant names |
|---|---|---|---|
| 1 | **Sand** | shore mask: `dist_to_water ≤ 6` and `height_above_water ≤ 0.6` | `SHORE_SAND_DIST`, `SHORE_SAND_HEIGHT` |
| 2 | **Rock** | `slope ≥ 0.70 rad (40°)` — hard rock, also all L0 crag stamp area above its rockline | `SLOPE_ROCK_MIN` |
| 3 | **Grass→rock blend** | `0.52–0.70 rad (30°–40°)`: dithered blend (fits the palette look better than smooth lerp at low res) | `SLOPE_GRASS_MAX` = 0.52 |
| 4 | **Grass** | everything else below the treeline band | — |
| 2a | **Pale rock stratum** | a **modulation of Rock**, not a fifth material — see §4.1 | `ROCK_STRATUM_*` |
| FUTURE | Dirt/path | road pass | — |
| FUTURE | Snow | region mountains above snowline | `SNOWLINE_HEIGHT` (region) |

### 4.1 THE PALE ROCK STRATUM — «белые скалы», and it is the material half of the banded massif

User-authorised (he wanted «белые скалы» in **both** senses — the spire groups
of §2.9 *and* a pale rock surface, which the world does not have at all).

**This is not decoration. It is the missing half of §2.8.** The user's original
massif brief was «высоту надо задавать **линиями уровня**», and §2.8.2 answered
it in *geometry* — bands, risers, benches. The frame that refuted the suite
complained of **«ONE material band, not a rhythm»**, and §4 has never had an
answer to that, because rock has been a single grey since it was written.
**A pale stratum makes the contour lines visible as MATERIAL, which is the
layer the complaint was actually about.**

- **RULE: pale rock is a STRATUM — a layer in the bedrock, exposed wherever
  terrain cuts through its elevation.** Selection is `slope ≥ SLOPE_ROCK_MIN`
  **and** sample height inside a stratum band. It needs no new input: `height`
  is already there.
- **STRATA ARE DEFINED IN ABSOLUTE WORLD HEIGHT, GLOBALLY — never as a fraction
  of each landform.** This is the whole ruling. The same layer must appear at
  the same elevation on the crag, on the lakeshore bluff, in the river's cut
  banks and on any future quarry face. **A band at a fixed height everywhere
  reads as geology; a band at a fraction of each landform's height reads as
  paint.** Third instance of the absolute-versus-relative lesson, after the
  couloir scale («a feature's size comes from the feature it cuts, never from
  the mountain it sits on») and the summit tor.
- **It is a MODULATION OF THE ROCK MATERIAL, not a fifth splat layer**, so §4's
  four-material budget is untouched — rock's albedo lerps between its grey
  stops and `ROCK_PALE` on the stratum mask. No new splat channel, no new
  memory.
- **~~And it survives quantisation by construction~~ — the ramp-change argument
  is SCOPED TO QUANTISER-ON and is no longer what makes this work (stage-5).**
  It was: grey rock on the **rock-greys** ramp, pale rock on the **neutrals**
  ramp, so a stratum boundary is a ramp change and the bands cannot merge at
  any quantiser setting. That remains true *with the quantiser on*. **At full
  colour there are no ramps, so the stratum has no separation argument at all
  until one is measured** — and this is the first place §1.5.1's «in the
  brights the criterion is weak, and nothing has ever been tested there» bites
  something real. The replacement is a derived value range, below.

**Sizing, derived (Rule 33 — the strata must be readable from the acceptance
distance, not merely present):**

- At Ravenscar's d_accept of 360 m the readable size is **12 m**, so **a
  stratum thinner than ≈ 12 m cannot read** and is stripe noise.
- Ravenscar's banded zone is cliffline (38 m) to summit (115 m) = **77 m**.
  Two to three pale bands across it give rhythm without corduroy.
- Therefore **`ROCK_STRATUM_PERIOD` = 28–36 m** with **`ROCK_STRATUM_PALE_FRAC`
  = 0.35–0.45** (⇒ 10–16 m of pale per period) **(предложение — утвердить)**.
- **The period is SEEDED AND NON-UNIFORM, with the same coefficient-of-variation
  discipline as `MASSIF_BAND_SPACING_CV_MIN`** — «линии уровня, которые где-то
  ближе, где-то дальше» applies to the material bands for exactly the reason it
  applied to the geometric ones, and a fixed period would rebuild the wedding
  cake in paint.

**Value, stated as a constraint so render picks the triple:**

- **~~`ROCK_PALE` sits between the grey rock stops (≈ 0.37) and the spire white
  (≈ 0.87), at least one palette step below the spire white.~~ — REPLACED BY A
  DERIVED RANGE (stage-5), and the old wording had two defects.** It gave
  render an *open interval half the value axis wide* with no separation floor
  in it at all, so a pale stratum 0.02 above grey rock would have satisfied it;
  and «one palette step» is denominated in the unit §1.3b has just retired,
  and in the *loophole* wording §1.3b closed even before that («one step across
  a ramp change» was tightened to a plain two steps, and this line was never
  updated with it). A textbook stale citation of the kind §1.6.5 names.
- **DERIVED, from §1.3b's re-based criterion at 2 rulers = 0.157 weighted-RGB.**
  Both boundaries are near-neutral pairs, for which the weighted metric reduces
  exactly to the luminance difference, so the arithmetic is direct:

  > **`ROCK_PALE` albedo ∈ [0.53, 0.71] (предложение — утвердить).**
  > Lower bound 0.37 + 0.157 — pale must separate from GREY ROCK, which is the
  > band the feature exists to show. Upper bound 0.87 − 0.157 — pale must stay
  > separated below SPIRE WHITE, which is §2.9's brightest-thing-in-the-world
  > clause restated as a distance instead of as a wish.

  The interval is **0.18 wide, i.e. the two floors consume 63 % of the
  available range** — which is the useful thing this derivation reveals and the
  reason the open interval was dangerous: there was much less room here than
  the old wording implied.
- **AND THE STRATA FADE WITH THE LIGHT, PROPORTIONALLY, WHICH IS NOT A DEFECT
  BUT MUST NOT BE MISREAD AS ONE.** Both materials are the same rock under the
  same sun, so their *rendered* difference is the albedo difference times the
  local illumination: a pair clearing 2 rulers on a fully-lit face clears 0.6
  in shadow at a third of the light. **§4.1's acceptance check («the bands hold
  their ELEVATION across a lighting change») is therefore about where the bands
  are, never about how strong they are** — a stratum that dims on a shaded
  flank is behaving correctly, and a reviewer must not file that as the feature
  failing. This is §4.2's «all families converge at the dark end» arriving in a
  place where it is benign.
- **The spires must remain the brightest value in the world** (§2.9), or a
  cliff face of spire-white drowns the L1 formation the brightness was doing
  work for. **A material must never out-value the landmark whose legibility
  depends on being the brightest thing in its frame** — C4's hierarchy
  argument, applied to the palette instead of to height.
- Pale rock is **not snow** and must not read as it; the FUTURE snow material
  is a separate row and a separate ramp position.

**Where it appears follows from the rule rather than from a table** (§7.1a):
every rock face the strata pass through — the crag's risers, the lakeshore
bluff, the river's cut banks, dungeon portal cuts. That consistency is the
point: **a stratum you can trace from the mountain down into the river bank is
the cheapest possible statement that this world has bedrock.**

### 4.2 The display palette — the ramp budget (ruling, stage-4)

The 64-colour post is **8 ramps × 8 shades**, quantised by nearest colour over
all 64 entries. Render asked which family gives up a slot for the conifer ramp
§5.12 requires. **Neither answer they offered is the first thing to try.**

- **RULING: the budget is 64 ENTRIES, not eight families of eight.** Ramp depth
  should follow **the lighting range a family carries and the screen area it
  covers**, and those are wildly unequal. Grass, rock, neutrals and sky span
  deep shadow to full light across most of the screen and need their depth.
  Sand serves a shore mask; water serves a 90 × 140 m lake and a 4–7 m river.
  **Reclaiming two shades each from the small families funds a conifer ramp
  without deleting anything.**
- **This follows directly from §1.5's correction.** If a ramp change is the
  coarsest and most robust signal the palette can carry and a shade step the
  finest, then **trading shades for ramps is favourable by default** and the
  uniform 8 × 8 grid is the one thing in the palette nobody has justified.
- **IF uniform ramp depth is structural in the shader, the sacrifice is DRY
  OLIVE, and the reason is not that it is the least pretty.** §4's material
  list is Sand / Rock / Grass-blend / Grass, plus two FUTURE rows. **There is
  no dry or upland grass material in this world, so dry olive is a ramp
  reserved for a biome that does not exist** — capacity held for an unbuilt
  thing while a built thing goes without, which is the LR's mistake in the
  colour space. Render's own argument (its dark end sits 0.046 from grass dark,
  closer than any other cross-ramp pair) is correct and I verified it; **but
  «serves nothing that exists» is the stronger reason and it is the one to
  record.**
- **The biome objection is answered in advance:** when biomes arrive they will
  need several new families and the palette is re-derived wholesale. Holding
  one ramp today does not meaningfully prepay that.

##### ⚠ THREE OF MY CLAIMS DID NOT SURVIVE MEASUREMENT — render measured, I reproduced

Render built a CPU mirror of the actual shader quantiser and re-ran every claim
against both palettes. **I reproduced their numbers independently before
accepting them and got the same figures to two decimals.** Recorded in full,
because the premise the whole change was ordered on is one of them.

| Claim of mine | Reality |
|---|---|
| «`PINE_DARK` must quantise into grass greens» | **It lands on WATER TEALS** — under the weighted *and* the unweighted metric |
| «the three needle tones are merged» | They land on **three adjacent water entries, cleanly resolved** |
| «separation goes from 0 → 3.1 shade steps» | **2.18 → 2.24** lit; **0.74 → 0.66** shadowed |

**Each failed for a different reason, and only the second is subtle.**

1. **The grass-greens claim I never computed at all.** I took it from a search
   report and made it load-bearing. It is wrong under *any* metric, so I cannot
   even plead the weighting. **This is the exact debt §1.6.5 names, incurred in
   the same document that names it.**
2. **The 3.1 figure used the wrong metric**, and that one is instructive: I
   measured Euclidean distance in RGB, and **the quantiser weights R/G/B at
   0.30 / 0.59 / 0.11.**
3. **The «0» was the shadowed case relabelled as the general case.** Lit rock
   already cleared the floor of 2 before any change.

**So the conclusion I drew — «nothing in the palette can fix the backlit
frame» — is CONFIRMED, and both numbers I used to reach it were wrong. Getting
the right answer for the wrong reasons is not being right**, and the only
reason it did not cost a build is that the ruling it supported (the apron
first) was load-bearing on other grounds.

##### THE METRIC IS A CONSTRAINT ON THE DESIGN VOCABULARY (render's amendment, ADOPTED)

**A separator must move RED or GREEN. Hue that lives in BLUE is invisible to
the quantiser.** At weights 0.30 / 0.59 / 0.11, a 0.2 difference in blue is
**0.9 shade steps** — under §1.3b's floor — while the same 0.2 in green is
**2.1 steps**. Green is 5.4× more effective per unit than blue, red 2.7×.

- **This is why needles and water collided**: blue-green water and green
  needles sit at nearly the same point in the (r, g) plane. Their measured
  separation is almost entirely in blue, which the metric all but discards.
  **Two colours that look completely different can be identical to the
  quantiser.**
- **It amends §1.5 rather than contradicting it.** «A ramp change is the
  coarsest signal the palette can carry» is a claim about **the eye**, and it
  stands. But **the quantiser decides which entry a colour reaches, and it runs
  first, and it does not use the eye's metric.** So a separator must pass two
  tests: *will the eye see it* (favours hue) and *will the quantiser preserve
  it* (favours R/G). **A blue-only difference passes the first and fails the
  second.**
- **General rule, and it is the transferable part: THE PIPELINE'S OWN METRIC IS
  PART OF THE DESIGN VOCABULARY AND BELONGS IN THIS DOCUMENT**, not discovered
  per-feature by whoever happens to implement next. Render's amendment is
  checkable arithmetic and it would have caught both of our proposals before
  either was written.
- **Consequence for §4.1, checked: pale rock vs grey rock is a VALUE change
  across near-neutral families, i.e. it moves R and G together.** It holds up
  under the metric. Same for the §2.9 spires.

##### ALL FAMILIES CONVERGE AT THE DARK END — doctrine, not defect

**Against rock in shadow, pine sits at ≈ 0.7 steps and still merges**, on both
palettes, because every family runs toward black and the darks are crowded by
construction.

- **NOTHING IN THE PALETTE CAN FIX THE BACKLIT VERDICT FRAME.** Confirmed by
  measurement rather than argued. **The apron is the fix; the palette is the
  hardening.** Nobody should spend a night in colour space on this — render has
  pinned it as an assertion that the shadowed case is *below* 2, so the limit is
  recorded rather than quietly hoped away.
- **Value and hue separation both vanish as luminance → 0. In deep shadow the
  ONLY thing that separates two shapes is silhouette.** Which is why §1.5's
  skyline rule exists, and why a landmark's read must never depend on its
  foreground being a different colour — it must depend on there being no
  foreground.
- **And the lit case is itself marginal: 2.18 steps against a floor of 2.** By
  this document's own standard that is a pass with 9 % of headroom. **If pine /
  rock separation ever needs to improve, the lever is `PINE_DARK`'s own R/G
  position, not the palette** — flora is rebuilding conifers now, which is the
  cheap moment to move it.

##### RULING — THE CONIFER FAMILY STAYS, AND THE REASON IS ENTIRELY DIFFERENT

It was ordered to fix the pine/rock merge. **It does not, and that merge was
never as broken as I said.** Asked whether it is still worth six entries:
**yes**, on a ground that survives measurement.

> **AUTHORSHIP OF APPEARANCE. Any element covering a significant fraction of
> the screen has its palette family chosen DELIBERATELY. A family arrived at by
> nearest-colour accident is not a decision: it moves whenever anything near it
> moves, and it couples two unrelated materials so that changing one drags the
> other.**

- **The forest was sharing a family with WATER.** A water look-dev change would
  have restyled every conifer in the world and nobody would have known why.
  That is a structural coupling defect, and it is not hypothetical here: **the
  river's source sits ≈ 122 m from the crag centre and the trace runs out
  through the pine foothills**, so pine against water is a present frame case in
  this testbed, not a future one.
- **The forest's colour was decided by an accident of the metric rather than by
  anyone.** That alone justifies the entries.
- **What it does NOT buy, stated so it is not re-claimed later:** conifer and
  broadleaf already separated (oak → grass greens on both palettes), and the
  three needle tones already resolved cleanly. Those are not gains.
- **DEPTH ALLOCATION — one measured amendment, render's call to take or leave.**
  Their split is sand 8→5, water 8→5. **Water is the worst place to spend it:
  it is the largest smooth gradient in the world, where banding is most
  visible, while sand is a thin dithered shore strip and dry olive serves only
  bright-grass highlights on already-dithered ground.** Measured per-shade
  spacing on the water family — smaller is smoother:

  | allocation | water step | pine vs lit rock |
  |---|---|---|
  | sand 5 / water 5 (as landed) | 0.105 | 2.19 |
  | sand 4 / water 6 | 0.084 | 2.14 |
  | **dry olive 5 / sand 6 / water 7** | **0.070** | **2.22** |

  The third is better on both axes at the same 64 entries. **Offered, not
  mandated** — banding visibility is a readability question and therefore mine,
  but ramp construction is render's craft.
- **AND I MUST WITHDRAW THE REASON I GAVE FOR PICKING DRY OLIVE.** I wrote that
  it «serves nothing that exists». **Measured: bright grass and dry grass both
  land on it.** No material *targets* it, but the quantiser is applied to the
  final image and pixels reach it — so it is functioning as the lit-grass
  extension. **That is my third unverified claim in one section**, and it is
  why the table above proposes *reducing* dry olive rather than deleting it.

##### RULING — TAKE CONIFER 8. FRAGILITY DEFEATS THE PURPOSE OF THE CHANGE

Render built my proposed allocation, found that **water 7 steals the lit needle
tone back into the water family**, searched the space rather than guessing, and
landed olive 5 / sand 5 / water 8 / conifer 6 — better than my proposal on both
of my own axes. **My principle held and my arithmetic did not**, which is the
correct division of labour and the second time tonight it has run that way.

They then recorded the part that matters: **water 7 fails, water 8 passes, and
nothing about that is robust.** Which family wins a given tone is decided by
where entries happen to fall. Ruling:

> **Take `conifer` = 8, paying for it with dry olive 4 and sand 4.**

1. **The fragile version does not deliver what the change was bought for.** The
   justification is AUTHORSHIP OF APPEARANCE — the forest's family must be
   chosen deliberately rather than fall out of a nearest-colour accident. **An
   allocation that holds only because water happens to be 8 is still leaving
   the forest's family to accident.** It is a different accident, not the
   absence of one.
2. **The input is about to move.** Flora is rebuilding conifers now and the
   atlas tone is the knob the ramp is derived from. A configuration that holds
   only for today's exact tones **breaks silently when they ship — and breaks
   toward «the forest quietly becomes water-coloured again», which is the
   original defect.** A silent regression into the bug a change was made to
   prevent is the worst available failure mode.
3. **The cost lands where banding is least visible** — a thin dithered shore
   strip and a highlight extension on already-dithered grass — **and the gain
   lands on the largest dark mass in the world.** That is the same
   banding-visibility principle that produced my first amendment, applied
   consistently rather than only when it is cheap.

- **Come back only if it actually costs something visible** — if sand at 4
  bands on the shore, that is a readability regression and I would rather hear
  it than have it absorbed. Otherwise ship it.
- **Re-verify after flora's new needle tones land.** The tones are the input to
  the ramp; a derivation is only as current as what it was derived from.
- **I could NOT check their tone arithmetic and did not try to fake it.** My
  reconstruction of their ramp disagrees with their measurements in *both*
  directions, which tells me my reconstruction is unfaithful — not that theirs
  is wrong. What I could verify structurally holds in every allocation: pine
  lands on conifer, oak stays on grass. **Fragility tolerance and banding
  visibility are design calls; ramp arithmetic is render's, and the honest
  answer to «whose number is right» was that mine was not computable from
  here.**
  - **Render supplied the reason, and it is a failure mode this document has
    not yet named: THE ENDPOINTS MOVED UNDER ME.** I was reconstructing the
    cold blue-green pair I originally accepted; the landed family runs along
    the ray through flora's albedo. **Neither arithmetic had to be wrong for
    the results to disagree — the artefact changed between the claim and the
    check.** Distinct from a stale premise (which was never true) and from an
    unchecked one (which was never looked at): this one *was* true when taken.
    **The counter-measure is not more care, it is checking against the live
    artefact rather than a copy of it** — which is now possible, because the
    quantiser is CPU-side and GPU-free in `BgfxPalette` with
    `palette_separation_steps` exposed. **§1.3b's separation criterion is
    therefore mechanically checkable by design rather than by hand, and I
    should use it instead of rebuilding ramps in a scratch script.**

##### LANDED, AND THE PART OF MY OWN RULING THAT IS STILL UNSHOT

Final allocation: grass 8, **dry olive 4**, dirt 8, rock 8, **sand 4**, sky 8,
water 8, neutrals 8, **conifer 8**. Measured: pine vs lit rock **2.34** steps
(2.18 pre-conifer), pine vs shadowed rock 0.70 and asserted as under 2, needle
tones on three adjacent conifer entries — **and it now holds at water 7 as well
as 8, which was the whole point.**

**But the cut I ordered has been argued safe on two surfaces and observed on
neither, and that is UNSHOT** (§1.6.3) — my own status category, applied to my
own ruling. Render said so plainly rather than reporting «no banding seen» from
a frame containing neither a beach nor a dry-grass expanse. **Reporting the
absence of a test as a pass is the failure this document exists to prevent, and
they refused it while handing over.**

**The risk is quantified, and it is mine: the two families I cut are now the two
coarsest in the palette.**

| family | shades | step | vs grass |
|---|---|---|---|
| **sand** | **4** | **0.159** | **2.76×** |
| **dry olive** | **4** | **0.152** | **2.65×** |
| neutrals | 8 | 0.131 | 2.28× (inherent — it spans black to bone) |
| rock / sky / water / grass / dirt | 8 | 0.054–0.072 | ≈ 1× |
| conifer | 8 | 0.041 | 0.71× |

- **THE SHORE FRAME IS THE TEST, AND IT ANSWERS THREE OPEN QUESTIONS AT ONCE** —
  worth knowing for whoever schedules it: sand at 4 on a broad beach, dry olive
  at 4 on a large dry-grass expanse, **and the re-measured WaterBed coverage
  against §3.3's cap**, since it is the first shore frame taken against
  non-duplicated water.
- **THE VANTAGE IS DERIVED, NOT TABLED, AND THIS TIME THE REASON IS ACUTE:
  core's pond fix literally moved the shore.** A beach coordinate written down
  before that fix sits on a feature that no longer exists — §7.1a's trap with
  the ground shifting underneath it. Derive the standpoint from the regenerated
  waterline. Shot at `INTERNAL_RES` (F6).
- **Reversal condition, stated now so it is not a matter of taste later:** if
  sand at 4 bands visibly on the beach, **the first lever is dither coverage on
  the shore band, not re-allocation** — every remaining donor is either already
  the coarsest family in the palette or is the conifer depth we just bought the
  robustness with. Whether dither is available there is render's to judge.

##### ⚠ SHOT, AND IT FAILED — AND THE REMEDY I NAMED DOES NOT EXIST

**`screenshots/shore/02_river_ford.png`, 640×360, quantiser on: sand at 4
bands.** Hard-edged tonal plateaus following the ground's curvature rather than
any shadow silhouette. **The frame carries its own control** — water (8) fills
the right half and grass (8) the upper left, under the same sun and the same
dither pass, and neither plateaus. Render bounded the reading honestly (hard
shadows are hard by design, so only edges that track the ground contour count).

**And the lever I named is arithmetically unavailable.** The palette dither is a
single global expression spanning **0.047** per channel. It breaks a band only
when its span is comparable to one quantisation step:

| family | shades | max step | dither covers | shades for ≥ 60 % |
|---|---|---|---|---|
| **sand** | 4 | 0.195 | **24 %** | **9** |
| **dry olive** | 4 | 0.207 | **23 %** | **10** |
| **neutrals** | 8 | 0.163 | **29 %** | **16** |
| **rock** | 8 | 0.090 | **52 %** | **10** |
| **sky** | 8 | 0.090 | **52 %** | **10** |
| dirt / water | 8 | 0.077 | 61 % | 8 |
| grass | 8 | 0.074 | 64 % | 8 |
| conifer | 8 | 0.056 | 84 % | 7 |

Raising the amplitude to cover sand would be a **4× global increase applied to
every family**, noising up the whole image to fix one band.

##### THE REAL FINDING: 64 ENTRIES CANNOT CARRY NINE FAMILIES AT THESE SPANS

**Every family at ≥ 60 % coverage needs 86 entries. We have 64.** The palette is
**a third too small**, and no allocation fixes that — restoring sand to 5 or 6
still leaves it at 32–39 %.

**So my §4.2 ruling was right about the principle and wrong about the
sufficiency.** «The budget is 64 entries, not eight families of eight» remains
correct — the uniform grid was never justified. But I then **reallocated inside
a budget I had never checked was adequate at all**, and ordered a cut on the two
families that could least afford it. **Checking whether the constraint is
satisfiable at all comes before optimising within it**, and I did not.

**Two families are already under the line that nobody has looked at: ROCK and
SKY, both at 52 %, and both carry large smooth surfaces.** Prediction, flagged
for measurement rather than asserted: **a quantiser-on frame of the massif may
band on its flanks.** Nobody has shot one.

> **AND THAT COLLIDES WITH §4.1.** A deliberate pale stratum and an accidental
> quantisation band are **the same visual event** — tonal steps across a rock
> flank. They are distinguishable by one property and it is already in §4.1's
> design: **strata track ABSOLUTE WORLD HEIGHT; quantisation bands track
> LUMINANCE**, so they diverge wherever the flank turns away from the sun.
> **§4.1's acceptance check is therefore that its bands hold their elevation
> across a lighting change** — not merely that bands are visible. Stated before
> the feature is built, for once.

##### RULING — THREE LEVERS, RANKED, AND THE FIRST IS A MEASUREMENT

1. **NARROW THE SPANS to the range each material actually occupies.** Costs
   nothing and is the only lever that could make 64 sufficient. Sand runs
   0.35 → 0.84 and neutrals 0.02 → 0.95; **a ramp should span what its material
   actually uses, not a decorative full range**, and every unused tone at the
   ends is resolution stolen from the middle where the surfaces live. **This is
   a per-material histogram of the pre-quantised frame — measurable, and I am
   not guessing at it after three wrong colour numbers tonight.**
2. **STEP-AWARE DITHER.** Fixes every family at once, costs no entries, and is
   structurally the right shape: **the present dither is one fixed amplitude
   applied to a palette whose steps are not uniform — the identical defect as
   the uniform 8 × 8 grid I already ruled against, one level down.** That
   recurrence is the strongest argument that it is correct. Render is right
   that it is a feature rather than a knob, and the local step is available
   where the nearest entry is found.
3. **MORE ENTRIES.** Last resort. The palette is a **user graphics setting**
   (sync №3), so this is not mine to spend alone, and it trades away the look
   the quantiser exists to produce.

**Nothing is reverted tonight.** Reverting sand to 5 buys 32 % coverage — still
banding — so it would be motion without a fix, and it would spend the conifer
depth that is holding a real property. **The palette stays as landed, with the
defect recorded and the frame in the repository**, which is the honest state.

Design rationale, binding for render:

- **Visual = gameplay truth.** `PLAYER_MAX_SLOPE` is 0.87 rad (~50°). The
  rock material starts at 40° so that by the time ground *looks* fully rock,
  it is nearly unwalkable; grass is always walkable. The player learns the
  material language instead of testing every slope. Never let grass render on
  slopes above `PLAYER_MAX_SLOPE`.
- **Dither where the geometry is smooth, SNAP where the geometry has an
  edge.** At 640×360 a soft blend band reads as smear, so ordered dither is
  the default and it matches the retro look while keeping the palette small
  (sync №2's palette flag). But **dithering across a real discontinuity smears
  the one line that discontinuity exists to produce** — a 55° cliff riser
  meeting a 20° bench (§2.8) is a genuine crease, not a gradient, and must be
  drawn as one. **Stated as the general rule rather than as a massif
  exception (render's reframing, and it is the better statement):** this is
  not a carve-out from the dither rule, it is the dither rule applied to a
  surface that has creases in it. It therefore generalises for free to
  everything else with an edge — quarry faces, cut terraces, the castle pad's
  cut, cave mouths.
  - **Mechanism (render, measured cheap):** screen-space derivative of slope.
    Where `fwidth(slope)` is small the ordered dither runs unchanged; where it
    spikes — 35° of slope change inside a pixel or two — the material boundary
    snaps. A couple of ALU instructions, no new data from core, no memory, and
    **no constant from design**.
  - **The threshold is render's and is set by looking, not by arithmetic.**
    Its natural unit is degrees of slope change *per pixel*, which is
    resolution-dependent — and `INTERNAL_RES` is a user graphics setting
    (sync №3), not a constant. A number derived here would be wrong at
    320×180. Tune it against a 640×360 frame of a band lip.
- **Treeline (region, FUTURE for testbed):** trees stop at
  `TREELINE_HEIGHT` (region-scale, ~180 m proposal) and the grass→rock blend
  shifts 5° earlier above it, giving bald summits — the classic
  mountain-meets-sky read. The testbed's 64 m ceiling has no treeline; the
  crag gets bald via its rock stamp instead.
- Max 4 materials in the splat at once (render budget + palette discipline).

### 4.3 FULL COLOUR IS THE BASIS — what §4.2 leaves behind, and what it hands forward (user ruling, stage-5)

**«давай цвета фигачить по полной, потом если что ужмем палитру.»** The look is
developed at full colour; the quantiser is a late pass fitted to a finished
world. §4.2 above is preserved whole under Rule 17 and is **read from here**,
because the ruling changes what most of it means.

##### 1. THE SAND AND DRY-OLIVE BANDING LEAVES THE URGENT LIST

`screenshots/shore/02_river_ford.png` **stays in the repository and the finding
stays true.** It is a quantiser-only defect, and there is no quantiser in the
basis. **No re-allocation, no step-aware dither, not now.** The three ranked
levers of §4.2 are not cancelled — they are **re-dated** to the late pass, and
lever 1 (narrow each family's span to what the material actually occupies) is
*better* placed there, because it wants a histogram of a finished world and we
do not have one.

##### 2. «86 NEEDED AGAINST 64 AVAILABLE» IS WITHDRAWN — AND IT WAS THE WRONG INSTRUMENT, NOT THE WRONG ARITHMETIC

This was the headline of my last message and I am taking it back with reasons.

**What 86 actually measured:** the entry count at which a *fixed* dither span of
0.047 covers ≥ 60 % of every family's step. **That is a property of today's
dither implementation, converted into an entry count and then reported as a
property of the world.** «60 % coverage» has never had a control; it entered
this document as a rule of thumb about when ordered dither breaks a band.

**The criterion this document already owns gives a different answer.** A band is
an edge the quantiser MANUFACTURES. §1.3b defines exactly when two adjacent
regions are different colours to the player. Put those together:

> **A family BANDS when its own largest interior step reaches
> `LANDMARK_SEPARATION_STEPS_MIN`** — i.e. when the quantiser manufactures an
> edge that clears the threshold at which this document declares two regions to
> be separate shapes. One constant governs merging *between* materials and
> banding *within* one.

**Derived first, then checked against the live `BgfxPalette` — and it has both
controls, from the one frame that was shot** (§4.2's shore frame carries its own
control, which is why that frame is worth so much):

| family | shades | max step | in rulers | predicted | observed |
|---|---|---|---|---|---|
| **sand** | 4 | 0.189 | **2.41** | BANDS | **bands** ✓ |
| dry olive | 4 | 0.181 | **2.31** | BANDS | not shot |
| **neutrals** | 8 | 0.161 | **2.05** | BANDS | not shot — **and nobody has ever flagged neutrals** |
| rock | 8 | 0.088 | 1.12 | clean | not shot |
| sky | 8 | 0.085 | 1.08 | clean | not shot |
| **water** | 8 | 0.073 | **0.94** | clean | **clean** ✓ |
| **grass** | 8 | 0.070 | **0.90** | clean | **clean** ✓ |
| dirt | 8 | 0.066 | 0.84 | clean | not shot |
| conifer | 8 | 0.049 | 0.63 | clean | not shot |

**Positive control sand, negative controls water and grass, same frame, same sun,
same dither — and the criterion separates them.** Rule 30 satisfied.

**The sizing that follows, and it inverts the finding:**

| banding floor | entries needed | |
|---|---|---|
| 2.50 rulers | 40 | |
| **2.00 rulers (= `LANDMARK_SEPARATION_STEPS_MIN`)** | **47** | **fits in 64 with 17 spare** |
| 1.50 rulers | 58 | fits |
| **1.40 (approx.)** | **64** | **break-even** |
| 1.25 rulers | 68 | over |
| 1.00 rulers | 80 | over |
| 0.75 rulers | 103 | over |

**64 IS NOT A THIRD TOO SMALL. It is roughly a third larger than this
document's own criterion needs, and the defect is ALLOCATION, not size.** The
86 figure sits at an implied floor of ≈ 0.9 rulers — **it was demanding that no
manufactured edge exceed half the difference at which this document says two
things are different colours at all.** That is why it produced an impossible
number.

- **What survives of the finding, and it is the useful part:** the palette
  question is now **one measurable quantity — the banding floor** — and 64
  suffices down to ≈ 1.4 rulers. Whoever runs the late pass gets a curve and a
  single experiment instead of a verdict.
- **HONEST CAVEAT, and it pushes back toward the old number: 2 rulers was
  derived for two LARGE MASSES, and a band is a THIN CONTOUR.** The eye is more
  sensitive to contour than to large-field difference (Mach bands), so the
  banding floor is plausibly *below* the merging floor, and the table above
  shows how fast that costs: at 1.25 we are already over budget. **47 is a
  floor on the requirement, not a certificate.**
- **THE DISCRIMINATING EXPERIMENT, and it is the same frame §4.2 already
  wanted, now worth more.** The two criteria agree on everything shot and
  disagree on three families: **the dither-coverage criterion predicts ROCK and
  SKY band; the separation criterion predicts they do not (1.12, 1.08) and that
  NEUTRALS does (2.05).** A quantiser-on frame carrying a large rock flank and
  a large neutral surface settles which instrument the late pass should use.
  **Per the user's ruling this is NOT urgent** — it is the first item of the
  late pass, and it is now a test between two instruments rather than a check
  of one prediction.
- **§4.2's prediction «the massif may band on its flanks» is WITHDRAWN as
  stated** (it was made on the coverage instrument). It becomes the negative
  arm of the experiment above.

##### 3. AUTHORSHIP OF APPEARANCE SURVIVES; THE CONIFER RAMP'S JUSTIFICATION DOES NOT SURVIVE UNCHANGED — I disagree with the framing I was handed, and here is why

I was told the conifer argument «never depended on the quantiser». **Read
literally it did, entirely**, and saying so is worth more than nodding:

- The recorded harm was **coupling**: «a water look-dev change would have
  restyled every conifer in the world.» That coupling exists **because the
  needles quantised into the water ramp.** At full colour a conifer's colour is
  flora's albedo and is coupled to nothing. **The mechanism of the defect is
  absent from the basis, so the six-then-eight entries buy nothing there.**
- **What genuinely survives is the PRINCIPLE, and it survives stronger:**

  > **Any element covering a significant fraction of the screen has its
  > appearance CHOSEN. An appearance arrived at by nearest-colour accident is
  > not a decision — it moves whenever anything near it moves.**

  At full colour that principle is **satisfied by construction**: the forest's
  colour is chosen directly, by flora, in albedo. The conifer ramp was the
  *mechanism* for satisfying it under quantisation. **Principle: durable and
  mode-independent. Mechanism: mode-specific.**

**RULING: conifer 8 stays landed. Do not churn it.** It costs nothing to keep,
it is correct for the quantised mode, and re-allocating a palette that is about
to be re-derived wholesale is motion without a fix — the same reasoning §4.2
used to refuse reverting sand. But:

- **It stops being cited as a precondition for any full-colour decision**
  (§1.5, §1.3b both amended).
- **It is provisional by its own terms anyway** — §4.2 already requires
  re-verification when flora's needle tones land, and the late pass re-derives
  every allocation against a finished world.

##### 4. WHAT THIS SECTION HANDS FORWARD TO THE LATE PASS

1. **The banding criterion** (max interior step vs `LANDMARK_SEPARATION_STEPS_MIN`),
   with its positive and negative controls named.
2. **The sizing curve**, and the one experiment that picks the floor on it.
3. **Lever 1 unchanged and better placed** — narrow each family's span to the
   histogram of a finished world. Neutrals spans 0.92 against conifer's 0.28
   and is the obvious first cut.
4. **`PALETTE_SHADE_STEP_REF` must be frozen before the palette moves**
   (§1.3b), or the re-derivation silently rescales every threshold in this
   document.
5. **Nothing in the palette fixes a shadow merge.** Confirmed at full colour:
   pine vs shadowed rock is **0.632**, worse than the 0.700 it measures
   quantised. **The apron is the fix** (§5.12) and always was.

---

