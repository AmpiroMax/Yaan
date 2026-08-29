
### 10.13 STATE AT WIND-DOWN — handoff, open items, and what would reopen each ruling

Written on the lead's wind-down instruction. **Nothing about today's work lives
outside this file.** This section exists so the next reader does not have to
reconstruct anything from a thread.

**Procedural note, stated plainly:** the lead asked that §10.12's three
questions be left OPEN with variants and costs. **They had already been ruled in
§10.12 when that instruction arrived.** I have not torn up the reasoning — but a
ruling made on the last day of a stage deserves its alternative written next to
it, so this section records what each ruling COST and what would REOPEN it. The
lead may reopen any of the three by reading this section alone.

#### 10.13.1 The D2 problem statement — recorded standalone, because it is worth a day to whoever reads it next

Even if every ruling below is discarded, **this paragraph should survive**:

> **A1 passed the probe and failed the picture in the same frame.** σ measured
> **0.353 against a floor of 0.35**; F7 failed — the ground still ran unbroken
> from the player's feet to the tree line. The cause: for `h = A·sin(2πx/L)`,
> σ = A/√2 and **RMS slope = 2πσ/L**. **σ bounds AMPLITUDE. Ground-occludes-
> ground is a property of SLOPE. The two are joined only through WAVELENGTH, and
> wavelength was never in the contract.** A field can hold σ arbitrarily above
> any floor and remain a shallow swell that hides nothing. The 2.4° grazing angle
> was derived correctly from eye geometry and then demanded of a quantity that
> does not constrain it. Rule 41, not Rule 48 — σ's zero-dose control is well
> behaved (0.000 on flat ground), so the criterion can pass and can fail; it is
> simply pointed one quantity to the left of the target.

Full working, the verified arithmetic table, and the replacement instrument are
in §10.12. The single most useful derived number there, if nothing else is kept:
**at σ = 0.353 the field clears the 40 m grazing angle only below L ≈ 52 m, and
`GROUND_MESO_WAVELENGTH` is approved at 25–60 m** — the top third of our own
approved band cannot produce occlusion at the amplitude we are producing.

#### 10.13.2 The three rulings, with their alternatives and costs

| # | Ruled in §10.12 | The alternative, and what it costs | What would reopen it |
|---|---|---|---|
| **D2 instrument** | Retire σ as a gate; gate on `GROUND_OCCLUSION_COUNT` (raycast, floor 3, p05, terrain-only) | **(a)** Keep σ and add a wavelength constant — costs a second constant that can be traded against the first behind the gate's back. **(b)** The lead's slope-area-fraction in 5–60 m — cheaper to compute, but must pick ONE grazing angle and is wrong at both ends of the band (4.86° at 20 m, 1.62° at 60 m). **(c)** Do nothing — σ keeps certifying frames that fail | Raycast cost turning out to be non-trivial over standpoints × bearings. Then (b) is the fallback and its known error is documented above |
| **LF-8** | Rebuild to locate gullies by CONNECTIVITY to the drainage (reuse §3.1's descent field), then measure depth. Stays RED until rebuilt | **The alternative I rejected: admit LF-8 has no instrument on bumpy ground and retire it.** Cost of retiring: we lose the only check on washouts, and §2.10's landform dictionary keeps an entry nothing verifies. Cost of my rebuild: it assumes §3.1's descent field is queryable at LF-8's scale — **I did not verify that**, and if it is not, the rebuild is more work than it looks | §3.1's field not being usable at this scale. Then retirement becomes the honest option, and it should be a retirement, not a loosened threshold |
| **Clearing в9** | Exempt, bounded by `AUTHORED_FLAT_RADIUS_MAX` = 50 m (derived so non-exempt ground stays inside the standpoint's own 5–60 m band) | **(a)** Exempt with no bound — costs the rule: an unbounded exemption eventually swallows the plain. **(b)** No exemption, shrink the clearing's calm core — costs в9's authored contract, which is the user's. **(c)** No exemption, lower the floor — costs everything, it is fitting to the achieved | **в9's actual extent exceeding 50 m, which I did not check.** If it does, the exemption as written does not cover it and (b) is the next option — a design question, not a number to bend |

#### 10.13.3 §2.7's fifth octave — done, and done harder than asked

The lead asked for the 2–4 m octave to be marked unapproved and contradicting
§10.2. **It was instead WITHDRAWN and reassigned in the §2.7 text itself**
(edit landed this session), naming §10.2's aliasing argument
(`LOD_VOXEL_SIZE_L0` = 1.0 m samples a 2–4 m period 2–4 times) and pointing the
work at B1's small end and B6's tufts. A marked-but-present line is still a line
someone applies; a withdrawn one with its replacement named is not. **This item
is closed, not open.**

#### 10.13.4 Open items carried forward — the register

Nothing here is a decision. These are things that are TRUE and UNFINISHED:

1. **`GROUND_MESO_WAVELENGTH_MAX` = 60 m cannot occlude at the achieved σ.**
   Flagged, deliberately not changed — the gate decides, not me (Rule 38). First
   knob for core: shorten L toward 25–40 m, which buys slope without buying
   amplitude and so costs nothing against the ceiling, corridors or
   `PLAYER_STEP_HEIGHT`.
2. **H1 must be RE-MEASURED after H2's banding is fixed.** §4.1's strata are
   global and absolute, so building them adds value structure to the crown H1
   measures; the retention denominator moves. Predicted in §10.10.2, still
   pending.
3. **H2's diagnostic probe has not been run**, and when it is it must read band
   rows from §4.1's absolute world heights by projection — never from the image
   (§10.11.3, and Rule 47's own text names this instrument).
4. **The frame-2 vantage (581,344) may fail its new fourth predicate** — the
   frame must contain the lowest band pair. Render's measurement, not mine.
5. **Three of my counts still need their measurement recipe migrated** to the
   generator side per §10.11.3: `MIDGROUND_OBJECT_COUNT_MIN` is done (0 → 8
   unoccluded vs floor 5), `OUTCROP_IN_VIEW_MIN` and A1's crest-line count are
   not.
6. **B3–B9 briefs are written but their constants are deliberately unapproved**,
   waiting on a frame from step 1 — the lead's НЕ ПОСТРОЕНО reasoning, which I
   agree with and which is my own argument applied to me.
7. **A1's before-state exists** (`render-haze-lowland-900m-A`/`-C`) and should be
   archived into `docs/acceptance/` labelled as such. Not yet done.

#### 10.13.5 Not mine, recorded so it is not lost

- **Identical trees on the horizon** — flora's variation problem, explicitly kept
  out of core's step-1 scope at the lead's request. No owner has picked it up.
- **`ROCK_STRATUM_*` is НЕ ПОСТРОЕНО with no consumer**, which is half of H2's
  likely cause (§10.10.1).

**Nothing else is held anywhere.** Every finding, every number, every open
question from this session is in §10 of this file.

---

