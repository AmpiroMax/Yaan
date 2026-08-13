<!--
Created: 13:08:2026 - 19:52:00
Last updated: 13:08:2026 - 20:49:05
File: docs/acceptance/render-sky-cumulus-stars-moonlight.md

Responsibility:
- The evidence behind three sky changes shipped together: cumulus off the
  20 km ring and into a slab, the star field's sampling fix, and the moon's
  ground gain. Every number here has a named arm and a zero-dose control.

Dependencies:
- Uses: tools/archive_frame.py (frames), the CPU mirrors of the cloud field
  (engine/render CloudModel) for the arms a frame cannot hold.
- Used by: docs/specs/render.md, the lead's review.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 27: every claim here is about MOTION and none of it can be read off a
  single frame. The arms are pairs.
-->
<!--
UPD:
- 13:08:2026 - 19:52:00: Created with the three changes.
- 13:08:2026 - 20:43:42: Deck thickness (both lower decks), the end-to-end sweep, and the
  reproducibility finding that came out of it.
- 13:08:2026 - 20:49:05: the cure verified end to end after the app wired it, and the
  residual named.
-->

# The sky, three reports closed

Three user complaints, three sampling defects. All three were measured before
anything was changed, and in two of the three the measurement contradicted the
diagnosis I was handed.

## 1. «Я не приближаюсь к облакам» / «бело-серые кучки»

**The diagnosis I was given was that the cumulus ring RIDES WITH THE PLAYER and
therefore never approaches. The first half is true and the second does not
follow from it.** The band sampled the field at `eye.xz + dh * 20000`, so under
a walk of Δ the sample point moved by exactly Δ — which is the *correct*
parallax response for a thing 20 km away, the same response the three deck
planes have for theirs. Anchoring was never the defect.

The defect is DISTANCE. Every cumulus pixel in the frame was at exactly 20 km,
and the entire walkable testbed is about 1 km across:

| arm | range to cloud, p10 / median / p90 | bearing swing per 300 m walked |
|---|---|---|
| RING (20 km, eye-centred) | 20.0 / 20.0 / 20.0 km | 0.86 / 0.86 / 0.86 deg |
| SLAB (deck to deck) | 3.2 / 5.6 / 21.2 km | 5.45 / 3.07 / 0.81 deg |

One number three times is the whole report: with every mass at one range there
is **no differential parallax anywhere in the frame**, so the bank can only
translate rigidly, which is what "stands there like scenery" IS, stated as
geometry. No amount of re-anchoring a 20 km ring produces a near cloud.

So the masses moved to where clouds are: a slab between the low and the middle
deck, four taps along the view ray, Beer-Lambert accumulation. Cloud went from
**2.8 % of sky pixels to 26.8 %**, and the nearest tenth now swings 6.7x the
bearing of the farthest.

The look follows from the geometry rather than from new constants: a ray at
2 deg of elevation crosses ~28x more slab than a ray straight up, so the same
field at the same threshold gives a solid bank at the horizon and separate
masses overhead.

Arms and controls: both models were run in ONE process over the same field
(`cumulus_walk`), and the no-walk arm of each came out at exactly 0.00 % of
pixels changed — the instrument cannot pass without the subject.

Frames: `render-cumulus-RING-before.png`, `render-cumulus-SLAB-after.png`.

**Two of my own mistakes, both caught by a frame and both fixed rather than
shipped.** (a) The 3-D field has no LOD, and the first cut laid a *speckled
stripe* across the horizon where one pixel spans kilometres along the ray;
fixed by converging to the field's own area average (which for a uniform field
above threshold T is exactly `1-T`, so no second constant), the same cure R3.3
applied to the sheets. (b) The veil extinction was applied INSIDE the integral,
where path length saturates opacity faster than distance can fade it — a 40 km
bank came out as an opaque grey WALL on the horizon. Applied once to the
finished alpha, on the slab's own entry distance, it melts into the sky like
everything else at that range.

## 2. «Звёзды дёргаются»

**Measured first, and it moved the blame twice.** The star field is a function
of the view direction alone, so WALKING changes it by 0.00 % of pixels — the
handed-down suspicion that the clouds' LOD metric was dragging the stars about
is also refuted below. What moves the star field is TURNING, which is what a
first-person player does constantly: 30 deg/s at 120 fps is 1.2 px of image per
frame.

At 640x360 the star disc was **0.55 px in radius** — smaller than the sample
spacing. A point that small is captured or missed depending on where the pixel
centre falls inside it, so one frame of that slow turn moved the whole field's
brightness by **-16.9 %**, with individual stars swinging 0.90 of full
brightness: appearing and vanishing between adjacent frames.

The cure for a sub-pixel point is not to antialias its edge — the whole star is
inside one sample. It is to spread the same ENERGY over at least one pixel. The
radius is now taken from the screen derivative and the brightness divided by the
area, so this is a change of shape and not of dose. The value is read off the
sweep, not chosen:

| radius | whole-field swing per 1.2 px turn | px at >=1 palette step | at >=2 |
|---|---|---|---|
| 0.55 px (shipped) | -16.93 % | 133 | 97 |
| 0.83 px | -1.34 % | 360 | 153 |
| 1.10 px | +0.86 % | 442 | 69 |
| 1.83 px | +0.49 % | 26 | 0 |

The swing falls 12.6x from 0.55 to 0.83 px and only 1.6x more by 1.10, while the
brightest stars start collapsing past 0.83 (153 -> 69 -> 0). Both curves are
still good at 0.83 px and one of them is not past it: that is the knee.

Note the third column — the field gets MORE visible, not less. The widening
recovers light the raster was throwing away (43.3 of an emitted 167 units), so
the stars are steadier AND there are more of them. The 3x3x3 cell read is part
of the same fix: the single-cell read CLIPPED any disc that crossed a cell
boundary, which alone cost 17 % of the field's light.

## 3. «Ночью свет от луны должен освещать слегка пространство»

**Measured, and the moon term is not the thing that was missing.** At the
player's eye, midnight, moon on the meridian at 32.5 deg, ground rows only, in
the quantiser's own metric (0.30/0.59/0.11):

| arm | ground mean | p10 | p90 |
|---|---|---|---|
| new moon | 7.01 | 6.18 | 7.66 |
| full moon, gain 0.30 (shipped) | 16.73 | 13.13 | 19.98 |
| full moon, gain 1.234 (now) | 47.00 | 35.26 | 58.15 |
| gain 0, full moon (zero dose) | 7.01 | 6.18 | 7.66 |

Two readings, and the second is the stronger one:

1. A full moon was **0.49 of one palette shade step** brighter than no moon at
   all (9.72 of the 19.99 luma a step is). This project's own rule — NUMBERS,
   `SUN_GLARE_LUMA_MAX` — is that one step IS the quantiser's cell and a
   one-step difference may round into the same entry, so a claim needs two. The
   moon was a factor of four under the threshold of being visible.
2. The moonlit ground spanned **0.34 of a step from its darkest tenth to its
   brightest**. Under the palette that is ONE ENTRY: the ground was a single
   flat colour and no amount of staring at it resolves a slope. That is
   «ничего не видно» exactly.

The gain is the requirement solved: two steps of separation need
`0.30 * 39.98/9.72 = 1.234`. Solving for it is legitimate because the response
was checked to be LINEAR first — the previous pass shipped 1.166 (derived in the
wrong metric) and predicted 37.8 luma of separation; the frame came back 37.79.
Measured after: **39.99 luma = 2.00 shade steps**, with p10/p90 now 1.09 steps
apart, i.e. the ground has shape.

**It is NOT the ambient fill, and the decomposition says so**: of the 16.73 a
full moon left, 9.72 was the moon and 7.01 the night ambient — the moon was
already the larger half. It is not missing, it is small, and so is everything
else at night. Raising the FLOOR would brighten moonless nights too, which is
the opposite of what a moon is for.

The zero-dose arm (`DFN_MOON_GROUND=0`) came back **identical to the new-moon
frame**, mean, p10 and p90: the term is zero at new moon by construction, so a
moonless night is untouched, and the two arms of the measurement came out of ONE
binary while seven other agents were editing this tree (Rule 47).

Frames: `render-moonlight-BEFORE-gain0.30.png`,
`render-moonlight-AFTER-gain1.234.png`,
`render-moonlight-NEWMOON-control.png`.

## What the same measurements REFUTED

The cloud sheets were suspected of a per-frame LOD churn large enough to read as
"the stars jitter" (58 % of sky pixels changing over a 3 m walk). Reproduced on
the CPU mirror with four arms in one process, at two pitches:

| arm | sky px changed | mean |d| | crossing a palette step |
|---|---|---|---|
| walk 3 m, shipped | 27.65 % | 0.692/255 | 3.41 % |
| walk 3 m, ceiling frozen | 26.12 % | 0.594/255 | 2.88 % |
| walk 3 m, **cells_px frozen** | **27.65 %** | **0.690/255** | 3.40 % |
| walk 3 m, + screen-space edge floor | 28.99 % | 0.683/255 | 3.31 % |
| no walk (zero dose) | 0.00 % | 0.000 | 0.00 % |

**Freezing the LOD metric changes the churn by nothing** (27.65 vs 27.65, max
identical to 0.01/255). The metric is translation-invariant by construction —
it is a screen derivative — and it is not the cause of anything. What the 27.65 %
is: the honest sub-pixel parallax of a plane at 1.6 km, worth 0.1-0.4 px of
image shift, mean 0.69 of 255 and only 3.4 % of pixels crossing a palette step
over a whole 3 m. Per 0.1 m — a frame of walking — it is 0.07 %.

A large *pixel count* at a tiny *amplitude* is what a fraction of a pixel of
translation always looks like in a difference image. It is not a defect and
there is nothing there to fix.

## 4. «Всё ещё плоские» — the decks stop being planes

Rule 52 in one line: a deck read at ONE plane intersection has a silhouette from
below and no vertical dimension, so it can only ever be a lid. Both lower decks
are SLABS now — three altitudes each, transmission `(1-a)^n` where n is the
number of field CELLS the chord crosses (`thickness/dir.y` over the cell width),
which is 1 overhead and 12 at the skyline.

| | thickness | why |
|---|---|---|
| middle (main sheet) | 300 m = half a coverage cell | a layer element is wider than it is deep; at the cell width the vertical structure would be as fine as the horizontal and read as noise |
| low (ragged, near) | 100 m = a third of that | its job in R3.2's ladder is to be SPARSE and show what is behind it, and a near layer as deep as the main sheet loses its gaps once the chord runs along it — the holes are the feature |
| high (cirrus veil) | none, on purpose | it is a veil at 4.4 km and cirrus have no depth to show. Rule 52 asks that a thing have a SHAPE, not that everything have a thickness |

Measured on the overcast sky probe (absolute eye, so the tour's resolved-height
drift cannot enter), sky showing through the deck:

| band | THICK | FLAT |
|---|---|---|
| near the horizon | 0.7 % | 5.5 % |
| overhead | 11.5 % | 15.6 % |

The first is the deck CLOSING along the layer, which is what an overcast sky
does and what a plane cannot do at any opacity. The second is the ray crossing a
third of a cell horizontally even straight up, so the three slices blend — that
is the change working too, it is simply not the headline, and the comment
claiming "it costs nothing overhead" was corrected in place rather than deleted.

**ZERO-DOSE ARM, and it earned its keep twice.** `DFN_DECK_THICK=0` reproduces
the shipped sheet at 0.002 % of pixels, max 1/255 — `pow(x, 1.0)` rounding — and
the low deck's own collapse measured 0.001 % / max 1. The first form of the path
wrote `clamp(1/dir.y, 1, 12)`, which does not contain the thickness at all, so
the dose-0 arm still closed the deck and came back 54 % different from the
shipped frame. **A dose that does not appear in the formula is not a dose.**

## 5. The end-to-end sweep, and what it found

Five things changed in the sky in one evening — cumulus volume, deck thickness,
the star field, the moon's gain, and the clock — and none had been seen with the
others. Swept together:

- **The pass's own control still holds.** `DFN_CLOUD=0` empties the WHOLE sky in
  one move: cumulus slab, both deck slabs and the high veil all vanish, leaving a
  clean gradient (`render-sky-endtoend-CLOUD0-control.png`). A cloud surviving
  cover 0 would be the two-copies defect made visible.
- **The standard seven-vantage tour** renders unbroken at every stop, with cloud
  shadows crossing the meadow from the same field the sky draws
  (`render-sky-endtoend-eye-level.png`).
- **Night, everything on**: the moonlight separation holds at 2.07 shade steps,
  the star field sits in the gaps between moonlit clouds, and the ground is
  navigable.
- **Frame cost**: the whole cloud apparatus measures 0.93 ms of median against a
  `DFN_CLOUD=0` arm from ONE binary (16.385 vs 15.457 ms). HONEST: the instrument
  is vsync-limited (median 16.4 ms = 60 Hz, p10 1.4 / p90 24), so 0.93 ms is NOT
  resolvable. What IS resolvable: neither arm misses the 16.7 ms deadline, and at
  night the two arms have the same median to 0.01 ms.

**AND THE SWEEP FOUND SOMETHING THE PARTS COULD NOT.** Reproducibility of a
restored recipe collapsed during the evening: two runs of one recipe that had
matched bit for bit at 19:30 differed by 1.79 % of the frame at 20:05, all of it
in the sky rows. Measured with the arm that separates the two candidates:

| two runs of ONE binary, one recipe | pixels differing | max |
|---|---|---|
| visual clock free | 67.466 % | 137/255 |
| visual clock pinned (`DFN_VISTIME`+`DFN_WIND_FREEZE`) | **0.000 %** | **0** |

So the geometry is exactly reproducible and the CLOCK is the whole of it. **The
sky had two clocks**: the sun and moon run off the app's `game_seconds_`, which
a tour advances by a fixed step per frame precisely so the world is a pure
function of the frame index, while the cloud drift and the wind envelope ran off
a `steady_clock` read inside `render()`. One of the two had been fixed and the
other had not.

**The defect is as old as the field and this evening only made it visible** —
cumulus went from 2.8 % of the sky to 26.8 % and the decks gained structure, so
the same few metres of drift now move an order of magnitude more pixels. It is
worth saying plainly: cloud volume did not break the acceptance method, it
exposed a hole in it. `RenderSystem::set_visual_time` is the cure (additive and
latched: until a caller tells the time, the wall-clock path is bit-identical),
and the app wires it from the same clock the sky is drawn from.

### The cure, verified after the wiring

With `set_visual_time(game_seconds_)` called by the app, two UNPINNED runs of
the same recipe:

| | pixels differing | max channel |
|---|---|---|
| before the wiring | 67.466 % | 137/255 |
| after the wiring | 1.97 - 12.79 % | **17/255** |

and the residual is named rather than left as noise. The frame log says it
exactly: run 1 rendered **252** frames before the shutter and run 2 rendered
**253**, so the deterministic clock stood at 868.200 s against 868.217 s — ONE
SIM step apart. The remaining difference is not the clock and not the sky: it is
that the tour's settle fires at a different FRAME INDEX between runs, because
streaming latency varies. That is a different defect with a different owner.

The number that matters for everyone's method: max channel 17/255 is **under one
PALETTE_SHADE_STEP_REF** (19.99), so after the wiring no pixel of a repeated run
can cross a shade — the pair measures a dose again.

## Owed

- `CLOUD_CEILING_MIN_M` / `CLOUD_CEILING_MAX_M` / the moon's ground gain want
  NUMBERS rows (Rule 14). Derivations are at their definitions.
- The R3.4 place term moves the ceiling by 0.33 m per 3 m walked, which is 14 %
  of the walk's whole sky churn — small, but its own header claims walking does
  not change it, and at 0.33 m per 3 m that claim is false as written.
- The slab has NO LOD on the 3-D field; it converges to the area mean instead.
  That is honest at the horizon and it is not the same thing as a mip chain.
- THE PLACE HALF OF THE CEILING STAYS AS IT IS, by the lead's decision, and the
  reasoning is worth keeping because it is a refusal to fit a threshold: 1.17x
  is under the 1.30x discrimination bar, but shortening the place wavelength to
  clear the bar would tell the player that five hundred paces changed his
  CLIMATE. The world is 1024 m across and climate moves on tens of kilometres.
  The row says "the line describes a possibility, the behaviour comes from
  weather alone" and that sentence points at the real cure — a bigger world.
