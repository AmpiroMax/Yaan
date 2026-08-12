<!--
Created: 12:08:2026 - 22:57:02
Last updated: 12:08:2026 - 22:57:02
-->
<!--
UPD:
- 12:08:2026 - 22:57:02: Выделен из docs/design/LANDSCAPE.md (9786 строк против FILE_HARD_LIMIT 800): §10.11–§10.11.3. Чистый перенос — ни одна строка тела не изменена, ни один номер секции не изменён; адреса вида «LANDSCAPE.md §X» продолжают действовать, таблица § → файл живёт в docs/design/LANDSCAPE.md.
-->

### 10.11 CLOSING THE HAZE LOOP — and running Rule 47 across my own criteria (stage-5)

#### 10.11.1 The Rule 34 flag is closed, and H1 is RATIFIED

The p05 figures were shot at **360 m**, not 900 m — recipe verified by the lead
in `docs/acceptance/README.md`: frames `render-haze-H1-360m-{Z,A,B,C}`, eye at
`DFN_MASSIF_EYE` = (518,380), measured over box (245,100)–(350,215). The 900 m
lowland frames are a separate set carrying a separate quantity (`lowland ΔL`) and
contribute no columns to H1.

> **RATIFIED. `HAZE_SILHOUETTE_RETENTION_MIN` = 0.667 is evaluated at H1's own
> `d_accept`, arm C sits at 0.708, and the 6% margin is a real margin at the
> binding distance rather than an artefact of the wrong range.** §10.10.2's
> conditional is discharged; the conclusion about C stands on what it appeared to
> stand on.

**And the recipe contains the thing that must not be tidied away later:** the
silhouette edges are taken **from the control arm** and every arm is read on the
same columns. That is Rule 47 satisfied by a specific deliberate choice, not by
luck. Anyone who later "simplifies" H1 by re-finding the outline per arm will
reintroduce the exact defect Rule 47 was written from, and the frames will report
that heavy haze has the least effect.

#### 10.11.2 Rule 48 has a POSITIVE form, and it is what licenses H1

The lead's addition to Rule 48 — that the dose response was **real and
monotone** (0.61 → 0.45 → 0.25 → 0.17), so a monotone response does not prove the
lever is yours — has a consequence worth making explicit, because it is what
tells a future reader when *not* to invoke the rule.

**H1 responds monotonically to the same lever:** 2.77 → 2.25 → 1.96 → 1.69.
Monotone response is common to both criteria, so it cannot be what separates
them. What separates them is one thing only:

| | zero-dose control | responds to dose | verdict |
|---|---|---|---|
| **H1** | **passes** (2.77, far above the one-step floor) | yes | genuinely measures haze |
| **H2** | **fails** (0.61 of 1.00) | yes | measures something else |

> **Rule 48 read forward: a criterion measures its dose only if BOTH its
> zero-dose control passes AND it responds to dose.** The negative form tells you
> when to throw a criterion out; this tells you when you are entitled to keep
> one, and it is the half a reader needs when their control passes and they want
> to know whether they are done.

#### 10.11.3 RULE 47 RUN ACROSS MY OWN CRITERIA — three are exposed, and core is measuring two of them this week

Rule 47 is render's finding about render's instruments. **I own criteria too, and
three of mine have the same defect.** This is urgent rather than academic: core
is building B1/B2/B6 now and A1 is next.

| criterion | how it finds its subject | Rule 47 |
|---|---|---|
| **H1** | outline columns fixed on the control arm | **safe, by deliberate choice** (§10.11.1) |
| **H2** | band rows found in the image | **exposed** — Rule 47's own text names this instrument: «третий нашёл дизеринг сплаттинга вместо полок породы» |
| **`MIDGROUND_OBJECT_COUNT_MIN` = 5** | counts silhouettes ≥ 8 px in the frame | **exposed** |
| **`OUTCROP_IN_VIEW_MIN` = 3** | same | **exposed** |
| **A1's ≥ 3 ground crest-lines** | counts occlusion edges in the frame | **exposed** |

**H2.** Rule 47's third instrument is H2's instrument, which means **0.61 may
itself be an artefact** — the true zero-dose value could be lower, or the failure
differently located. *This does not reopen §10.10.1's withdrawal:* under either
reading the criterion failed at zero dose, so Rule 48 removes it from the haze
question regardless. But it binds the **diagnostic probe**, which has not been
run yet and must not be run on the image.

> **The fix is stronger than Rule 47 asks for, and §4.1 already paid for it.**
> Rule 47's remedy is to fix the band rows once on the control arm. But if the
> control arm is itself the frame where the bands are broken, that remedy still
> reads positions out of a picture that has none. **§4.1 defines the strata in
> ABSOLUTE WORLD HEIGHT, so their screen rows are computable by projection with
> no image involved at all.** The absolute-height ruling makes H2 Rule-47-proof
> **by construction** — an unplanned second reason to like a decision made for
> geological consistency.

**The three counts.** All of `MIDGROUND_OBJECT_COUNT_MIN`,
`OUTCROP_IN_VIEW_MIN` and A1's crest-line count locate their subjects by
segmenting the frame. Each therefore drops when anything lowers contrast — haze,
flat light, a palette revision — **with no change whatever in placement.** The
metric would then attribute a render change to core's scatter pass, and it would
do it in the direction Rule 47 warns about: *systematic bias toward «the objects
are not there»*, exactly when they are hardest to see. Core would be sent to
place more objects to fix a lighting change.

> **RULING, and it covers every count in §10 at once: a count is established in
> the GENERATOR and verified on the FRAME. It is never counted on the frame.**
>
> - `MIDGROUND_OBJECT_COUNT_MIN` and `OUTCROP_IN_VIEW_MIN`: count from the
>   placement list projected into the frustum — which objects are in view, at
>   what apparent size — and apply the 8 px filter to that *computed* apparent
>   size, never to measured pixel contrast.
> - A1's crest-lines: «ground occludes ground» is a **raycast fact** from the
>   standpoint, computable with no shading at all.
> - The frame's job in all three cases is to confirm that what the generator says
>   is there can actually be seen. **If the two disagree, that disagreement is
>   the finding** — and it is a finding about render or about light, which is
>   precisely the information a frame-side count destroys by folding it into the
>   number.

This is a correction to the measurement recipe of constants that were approved
three messages ago, and I would rather surface it now than after someone measures
step 1 the wrong way and core is sent to fix a defect it does not have.

**No new numbers.** The values are unchanged; only where they are read from
changes.

---

