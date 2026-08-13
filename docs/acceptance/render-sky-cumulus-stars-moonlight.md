<!--
Created: 13:08:2026 - 19:52:00
Last updated: 13:08:2026 - 19:52:00
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

## Owed

- `CLOUD_CEILING_MIN_M` / `CLOUD_CEILING_MAX_M` / the moon's ground gain want
  NUMBERS rows (Rule 14). Derivations are at their definitions.
- The R3.4 place term moves the ceiling by 0.33 m per 3 m walked, which is 14 %
  of the walk's whole sky churn — small, but its own header claims walking does
  not change it, and at 0.33 m per 3 m that claim is false as written.
- The slab has NO LOD on the 3-D field; it converges to the area mean instead.
  That is honest at the horizon and it is not the same thing as a mip chain.
