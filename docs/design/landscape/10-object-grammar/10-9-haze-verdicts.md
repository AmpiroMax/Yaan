<!--
Created: 12:08:2026 - 22:57:02
Last updated: 12:08:2026 - 22:57:02
-->
<!--
UPD:
- 12:08:2026 - 22:57:02: Выделен из docs/design/LANDSCAPE.md (9786 строк против FILE_HARD_LIMIT 800): §10.9–§10.9.5. Чистый перенос — ни одна строка тела не изменена, ни один номер секции не изменён; адреса вида «LANDSCAPE.md §X» продолжают действовать, таблица § → файл живёт в docs/design/LANDSCAPE.md.
-->

### 10.9 HAZE — the two verdicts render is waiting on (stage-5)

Render fixed aerial perspective and found it had never run
(`smoothstep(0.30·CAMERA_FAR, 0.85·CAMERA_FAR, d)` began at 2400 m in a
1024 m world, so the haze multiplier was identically zero, and measured
contrast *rose* with distance by 54%). They then derived
`HAZE_SCALE_LENGTH` = 1400 m from a written design contract rather than
inventing it, which is the right procedure. **The contract they derived it from
was stale, and it was stale because of me.** This section is design's half.

#### 10.9.1 Verdict 1 — `LANDMARK_HAZE_ONSET` stays a SITING rule. CONFIRMED, and it stops being a tabled metre value

**Confirmed**, and the reasons escalate:

1. **The evidence.** All sixteen frames show contrast falling continuously over
   the whole visible range. R1 says so in its own words — «not as a fog wall at
   the far plane». A switch at 800 m *is* a fog wall, i.e. the precise defect
   R1 exists to forbid.
2. **The structural reason, which is stronger: `exp(-d/L)` HAS NO KNEE.** There
   is nothing at 800 m for a renderer to switch, at any scale length. A number
   that cannot be a draw parameter can only be a placement parameter, and that
   settles the question independently of taste.
3. **So its meaning is fixed:** the distance beyond which a landmark reads as
   *far goal* rather than as *here*. It is an input to **where the LR and the L0
   are sited** (§1.3a), never to how either is drawn.

**But it must stop being a tabled 800.** §1.6.1 already ruled, about acceptance
distances, that they are **derived, never tabled** — «a tabled distance is a
tabled coordinate wearing a different hat». The same applies here, and it
applies harder now that there is an exponential model where before there was
none: under `exp(-d/L)`, 800 m means a different amount of haze at every L, so a
tabled 800 cannot survive a change to `HAZE_SCALE_LENGTH` without silently
changing meaning.

> **RULING: the onset is a CONTRAST RATIO of 2× — «hazy» is half the contrast
> of «solid» — and the metre value is derived from it:**
> **`d_onset = d_accept(L0) + L · ln 2`.**

With Ravenscar's `d_accept` = 360 m (§1.6.1):

| `HAZE_SCALE_LENGTH` | derived `d_onset` |
|---|---|
| 1400 m | **1330 m** |
| 600 m | **776 m** |

**Recorded as evidence for the lead's choice, not as the choice:** the tabled
800 m is what a ~600 m scale length produces, to within 3%. That 800 was written
in §1.3a *before* there was an exponential model — it is an intuition about how
far away a thing has to be to read as a far goal, and that intuition turns out
to be consistent with the short scale length and not with the long one. It is
one data point and it arrives from a completely different direction than
§10.9.2, which is the only reason it is worth stating.

#### 10.9.2 Verdict 2, part one — THE PREMISE UNDER 1400 IS MINE, AND IT WAS WITHDRAWN BEFORE THIS CONVERSATION STARTED

Render derived 1400 from §7.1b: *«Ravenscar must read SOLID not hazy at
287–717 m, and haze on it is a bug per §1.3a.»* **That sentence is mine.**

§1.6.1 rules `d_accept = 3·R`, and for Ravenscar (R = 120 m) that is **360 m,
not 717 m**. The same section rules, in as many words, that **«a landmark
photographed beyond its own d_accept certifies nothing about its shape»**.
717 m has not been an acceptance distance for this landmark since §1.6.1
landed — and **I never propagated that correction into the haze clause.** The
600 m→360 m correction was written into the acceptance machinery and left
sitting next to a haze sentence that still quoted the old range.

**Render did nothing wrong. They read a contract instead of inventing a number,
which is what we ask, and the contract lied to them.** The failure mode is worth
naming because it will recur: **a correction that lands in one section and not
in the sections that cite it is a shadow copy** (Rule 39) — the same defect as a
duplicated constant, wearing the costume of a stale cross-reference.

**WITHDRAWN: «haze on Ravenscar at 287–717 m is a bug».** What survives, and it
is narrower and checkable: **haze that breaks a CLAUSE at that clause's own
distance is a bug.** Per §1.6.1 — «the binding distance is always the clause's,
never the frame's.»

Retention `exp(-d/L)`, recomputed against the corrected distances:

| | 287 m (rhythm clause) | **360 m (verdict clause)** | 717 m (no longer a test) |
|---|---|---|---|
| L = 1400 m | 0.82 | **0.77** | 0.60 |
| L = 600 m | 0.62 | **0.55** | 0.30 |

**The only cell where the short scale length looks alarming is the one that
stopped being a test.** At the distance that actually binds, 600 m leaves
Ravenscar at 55% contrast — more than half, which is what «solid» was ever
meant to mean.

#### 10.9.3 Verdict 2, part two — WHAT I AM OBLIGED TO PROTECT, as three checkable propositions

Not «haze breaks it». Thresholds are in units of `PALETTE_SHADE_STEP_REF` =
**0.0784**, the frozen ruler, so that design's thresholds and render's band
construction meet on one number (Rule 35 — that is what the constant is for).

**H1 — SILHOUETTE.** Frame 1, 360 m, backlit low morning sun.
> Along Ravenscar's outline, |body luminance − adjacent sky luminance| ≥
> **2 × `PALETTE_SHADE_STEP_REF` = 0.157**, at every point of the outline.

Two steps and not one: §1.5 makes a shade step the finest signal the palette
carries, so a one-step margin is a margin *equal to* the floor, and this
document has twice refused zero slack. **Fails when** the crown quantises into
the sky ramp anywhere along the outline — which is the failure a landmark
doctrine cannot survive, because §1.5 says landmarks read by value against sky.

**H2 — RHYTHM.** Frame 2, 287 m, raking evening sun behind camera.
> Riser-to-bench luminance separation ≥ **1 × `PALETTE_SHADE_STEP_REF` =
> 0.0784**, measured **at the LOWEST band pair visible in the frame, never on
> the flank mean** (see §10.9.4 for why that clause is load-bearing).

One step suffices here because a rhythm is a *repeated* signal that the eye
integrates across several bands, and because §7.1b already chooses this frame's
light to maximise riser/bench separation. **Fails when** the lowest two bands
read as one value.

**H3 — DEPTH SEPARATION.** §1.3a's actual requirement, and the one everybody
assumes is the binding constraint.
> contrast(L0 at 360 m) ≥ **1.7 ×** contrast(LR at its nearest legal siting,
> 1400 m).

| L | L0 @ 360 | LR @ 1400 | ratio | H3 |
|---|---|---|---|---|
| 1400 m | 0.77 | 0.37 | **2.1×** | pass |
| 600 m | 0.55 | 0.097 | **5.7×** | pass, by 2.7× more |

> **The short scale length satisfies my own §1.3a requirement BETTER, not
> worse** — because separation is a RATIO, and a shorter scale length is
> precisely what makes ratios large. §1.3a asked for *depth separation between
> two landmarks*. It never asked for an absolute contrast floor on Ravenscar.
> **The absolute floor entered the contract only through the 717 m number, and
> that number is withdrawn.**

So H3 does not choose between 1400 and 600. Only H1 and H2 can, and both are
measured on frames nobody has taken yet — which is the correct place for this
decision to sit.

#### 10.9.4 What the height lever changes — and the one refinement it forces

The lead is right that `HAZE_HEIGHT_SCALE` protects what I protect and relaxes
what R1 wants, and that frames 02 and 12 show literally that: a hazed base under
a clean crown. It is also the more physical model — haze is aerosol, aerosol
settles, and a density that falls with altitude is why real mountains do this.

The three propositions move in **opposite directions** under that lever, which is
why they had to be separated before it could be discussed at all:

- **H1 gets EASIER.** A mountain's outline *is* its crown — the highest geometry
  in the frame — and altitude-scaled density protects exactly that.
- **H3 is unchanged or easier.** The ratio is carried by the valley air between
  the two landmarks, which is where the density stays high.
- **H2 gets HARDER at the bottom of the flank and easier at the top**, and this
  is the refinement the lever forces. With `HAZE_HEIGHT_SCALE` = 250 m and
  Ravenscar's `L0_RELIEF` = 115 m, the crown sees `exp(-115/250)` = **0.63** of
  the density at the hem. The band rhythm therefore **survives at the summit and
  dies at the hem first.**

> **Hence H2's «lowest visible band pair» clause, and it is not fussiness.** A
> separation averaged over the flank would report a comfortable pass while the
> failure sat at the bottom of the frame — the property under test varies with
> elevation, so the measurement must be taken where it is worst. That is F7 in
> the vertical axis, and it is the same error as measuring banding on a strip at
> one luminance.

**My position, stated so the lead can close this without another round:**

- **If the height-scale pair comes back and H1, H2 and H3 all hold at the
  shorter scale length, I withdraw and I will not manufacture a new objection.**
  My contract asked for depth separation and for two clauses to stay readable;
  it never asked for a contrast floor on Ravenscar, and the number that looked
  like one has been withdrawn above.
- **The one thing I will not trade is H2 measured at the hem.** If the height
  lever's entire benefit is bought by hazing the base of the mountain, then the
  base is where the cost lands, and §7.1b's rhythm clause is measured there. That
  is the check that can still say no, and it is the only one.
- **And the reference frames get the last word on ties.** Render measured that
  the far ridge stood out 54% *more* than the near one — our frame asserted the
  opposite of all sixteen references. Between a rule of mine and sixteen frames
  the user chose, the frames win; §7.1's oldest clause says so. My job here was
  to make sure the rule that gets overruled is a real one, and on inspection the
  strongest-sounding part of it was a stale citation.

#### 10.9.5 Propagation — the correction §1.6.1 owed and never paid

Recorded so this class of failure is closed rather than noted:

- **§7.1b's haze sentence** («287–717 m … haze on it is a bug») is superseded by
  §10.9.2 and §10.9.3. The frames themselves are unchanged — §1.6.1 already
  ruled that they now differ by clause and light rather than by range.
- **§1.3a's `LANDMARK_HAZE_ONSET` = 800 m** is superseded by §10.9.1: it remains
  a siting rule, its meaning becomes a 2× contrast ratio, and its metre value is
  derived from `HAZE_SCALE_LENGTH` rather than tabled.
- **General:** a distance quoted from another section is a **premise, not a
  fact** (Rule 34). Every acceptance distance in this document is now derived by
  §1.6.1's `d_accept = 3R`; any section still quoting a metre figure for one is
  quoting a shadow copy and is wrong by construction.

---

