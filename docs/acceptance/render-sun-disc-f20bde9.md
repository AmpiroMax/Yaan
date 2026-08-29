
# Acceptance: the sun gained a body (W9)

User complaint answered: **"ярко есть, а солнца не видно"** — bright, but no
solar disc anywhere in the sky. Lead confirmed it on a capture before this work
started.

## Frames

| File | Build |
|---|---|
| `render-sun-disc-before-f20bde9.png` | `f20bde9` (the CONTROL — the old sky shader) |
| `render-sun-disc-after-f20bde9.png` | the commit that adds this file |

Both at native internal resolution 640x360. The 4x tour upscale adds pixels,
not information, so the archived frames are the pixels the shader actually
produced.

## Recipe — identical for both arms, one variable (the shader) different

```
DFN_MENU=0 DFN_STAND=0 DFN_RESTORE=<sidecar> \
  DFN_TIME=0.32 DFN_VISTIME=10 DFN_CLOUD=0 \
  DFN_CAPTURE_DIR=<fresh dir> DFN_CAPTURE_AFTER=4 ./dfn_app
```

Eye (128.0, 22.35, 127.9), yaw 1.570800, pitch 0.410000, seed 1. Sun pinned at
day fraction 0.32 — elevation 23.4°, azimuth mostly +X, so the eye looks
straight at it. Cloud cover pinned to zero and the visual clock pinned so the
only difference between the two frames is the sun itself.

**This vantage can fail** (Rule 27): the subject is in frame, at a size the
complaint is about, against plain sky with nothing else near it. A frame that
looked away from the sun would have measured the absence of a test.

## Measured, at that vantage, in the quantiser's own luma (0.30/0.59/0.11)

| | before (control) | after | spec |
|---|---|---|---|
| peak luma | 1.000, clipped | 0.988 | `SUN_DISC_LUMA` 1.00 |
| body at the half-contour | **21.59 px** | **9.24 px** | `SUN_ANGULAR_DIAMETER` = 9.0 px |
| edge fall, 90% -> 60% of peak | **16 px** | **7 px** | an edge, at all |
| red channel pegged at 255 | 15.51 px across | 8.06 px across | — |
| glare reach | — (no separate term) | ~36 px across | `SUN_GLARE_ANGULAR_DIAMETER` = 37.5 px |
| plain sky | 0.415 | 0.415 | unchanged, by construction |

**The 16 px edge fall is the defect, stated as a number.** The old sun was two
additive glow lobes, `pow(dot,900)*0.85 + pow(dot,24)*0.10`, and what a player
read as a disc was the RGBA8 clamp: 15.5 px of the red channel simply pegged at
255. Its apparent size was therefore whatever saturated that frame, and it
drifted through the day with `u_sunColor` and the sky ramp. Nothing in the code
named a radius.

**The disc measures 9.24 px against a 9.0 px row** — 2.7% over, which is inside
the one-pixel antialias the shader deliberately applies (from `fwidth`, so the
320x180 preset gets the right width without a second constant).

**And the bright FEATURE got BIGGER, not smaller, which is the half that is easy
to misread.** 9.24 < 21.59 looks like a shrink; it is not. The old 21.59 px was
a smear with no body, and the new disc sits inside a halo reaching ~36 px. The
user asked for the sun to be visible AND somewhat bigger, and those are the two
numbers that answer him separately.

**Why the halo has a ceiling, which is the actual fix.** The sky beside the sun
used to reach the top of the range, so a disc had nowhere above it to stand and
"make the sun brighter" was a no-op. `SUN_GLARE_LUMA_MAX` 0.843 holds the halo
two quantiser steps below the top — two and not one because one step IS the
quantisation cell, and a threshold equal to the instrument's resolution has no
margin (Rule 30a).

## Not covered by this pair

The two moons. Design's W9 brief derives their orbits, sizes and phases; the
model needs `apply_sky_time` to take absolute game days instead of a
(day_fraction, lunar_phase) pair that cannot be checked for agreement. Handed
over, not attempted.

## Second frame: the LOW SUN, because one vantage does not cover the range

`render-sun-disc-lowsun-f20bde9.png`. Same recipe, `DFN_TIME=0.272`
(elevation ~5°) and pitch 0.090000 so the disc sits just over the treeline.

Rule 27 asks for the subject ACROSS THE RANGE THE PROPERTY VARIES OVER, and
the sun's colour and brightness are functions of elevation: `apply_sky_time`
reddens `u_sunColor` toward `SUN_COLOR_LOW` (1.00, 0.55, 0.28) and scales it
down as the sun drops. A disc verified only at 23° would be a disc verified at
one colour.

Measured on that frame:

| | value |
|---|---|
| disc peak luma | **0.898** |
| disc RGB | (1.00, 0.90, 0.62) — reads warm, not white |
| sky beside it | 0.333 |
| disc − sky | **0.565 = 7.2 quantiser steps** |
| edge fall, 90% → 60% | 9 px |

**A STATED DEVIATION, measured rather than discovered later.** The disc does
not reach `SUN_DISC_LUMA` 1.00 at a low sun: it lands at 0.898. The cause is
arithmetic and not a bug — the shader carries hue at unit luma and multiplies
by the row, and a warm hue clips its red channel before its luma reaches 1.0
(the unit-luma form of `SUN_COLOR_LOW` is (1.53, 0.84, 0.43), and 1.53 does
not exist in an 8-bit channel). A saturated warm disc at luma 1.00 is not
representable, full stop.

It costs nothing that any rule cares about: the disc clears the sky by 7.2
quantiser steps against a 2-step requirement, and it clears it by HUE as well,
which is the property a low sun is supposed to have. The alternative — pushing
luma to 1.00 by desaturating — would make the sunrise sun white, which is the
opposite of what the frame should show.

The edge is 9 px of fall here against 7 px at 23°, because the halo is
relatively brighter next to a dimmer disc. Both are edges. The old shader had
16 px at 23°, which is not an edge at any elevation.
