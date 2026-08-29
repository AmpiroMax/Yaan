
# R6a — WARM KEY / COOL SHADE: measured on the reference frames

**Verdict: the change was refused. We are not missing a warm/cool light split —
we have one, and it is the largest in the comparison. The reference frames'
cast shadows are the SAME HUE as their sunlight, only darker.**

Instrument: `tools/measure_light_split.py` (`pair` and `scan` modes).
Frames: `images_examples/render/` (third-party, path-only, never copied here —
`docs/REFERENCE_FRAMES.md` §opening). Ours:
`docs/acceptance/render-mist-R2-360m-ON-3d37ef3+r2.png`, already archived, at
640x360 native.

## The quantity

`WARM_SPLIT` = the yellow-blue chromaticity, in percent of luma, of the
**per-channel ratio between a lit patch and a shaded patch of the SAME
material**. It is the hue of the light the key ADDS, over the light already
there.

- `0` — the shadow is the same colour as the sunlight and merely darker.
- `+` — warm key over cool fill. This is what R6 asserts.
- `-` — cool key over warm fill.

The ratio is what divides the albedo out. A chromaticity DIFFERENCE does not:
grass albedo (60,110,40) under one white light gives yellow-blue chroma 43 lit
and 16 shaded, a delta of +27 out of a scene whose light has no colour at all.

## Calibration — the instrument is checked against a known answer

Our renderer has **no tonemap and no gamma anywhere** (grepped: nothing in
`shaders/*.sc`, `*.sh` or `BgfxRenderer*.cpp`), so the 8-bit frame is the light
arithmetic itself and the answer is predictable from the source:

```
LOOKDEV_SUN_COLOR      1.00, 0.96, 0.88     (Materials.h:93)
LOOKDEV_AMBIENT_COLOR  0.34, 0.36, 0.40     (Materials.h:94, "cool skylight")
flat ground, NdotL = LOOKDEV_SUN_DIRECTION.y = 0.62
predicted WARM_SPLIT = +14.01
measured on the frame            = +14.02
```

Two decimal places on a real frame. Everything below is read by the instrument
that did that.

## The table

`pair` = two boxes on one material either side of a cast shadow edge, placed by
looking at the edge. `scan` = block means over one box, darkest 15% against
brightest 15%, block coarser than the material's texture.

| frame | mode | box(es) | luma range | **WARM_SPLIT** | red_split |
|---|---|---|---|---|---|
| **OURS** grass, obelisk shadow vs sun | pair | `85,338,132,358` / `0,336,58,358` | 1.79x | **+14.02** | +15.12 |
| **OURS** grass + obelisk shadow | scan 16 px | `0,240,400,358` | 1.66x | **+8.77** | +11.54 |
| ref 14 Whiterun cobbles, cast shadow vs sun | pair | `600,612,706,672` / `740,552,880,668` | 1.90x | **+0.32** | +1.76 |
| ref 14 Whiterun cobbles | scan 24 px | `560,330,1010,570` | 1.97x | **-1.13** | +2.10 |
| ref 03 forest floor, the dapple | scan 24 px | `540,430,1190,600` | 2.28x | **-8.55** | +5.96 |
| ref 01 plateau ground | scan 24 px | `700,400,1180,600` | 1.72x | **+0.52** | +5.25 |
| ref 10 Vivec plaza floor | scan 32 px | `20,430,900,760` | 4.09x | **-0.10** | -4.81 |

Reference frames: 14 = `image copy 9.png`, 03 = `image copy 12.png`,
01 = `image copy 10.png`, 10 = `image copy 5.png`.

**Four reference frames, five boxes, two independent modes: -8.55 to +0.52.
Ours: +8.77 to +14.02.**

## How much of a reference reading is its pipeline

Reference frames are tonemapped and sRGB-encoded and ours is neither, so the
reference column carries a bias our column does not. `selftest` bounds it by
pushing a ZERO-DOSE scene (white key, white ambient) through Reinhard + sRGB:

```
tonemap bound   WARM_SPLIT  +6.19 (saturated albedo)   -1.12 (pale albedo)
```

So a reference reading is worth about ±6. **Every reference number above is
inside that bound — they are all consistent with exactly zero light split.
Ours is not: +14.02 is more than twice the bound.**

## What the shadows in the reference actually are

Ref 14, the cleanest case (same cobbles, the player's own cast shadow, one
metre from sunlit stone):

```
shadow  68.8, 66.4, 51.6   warmth 24.40
sun    132.2,125.4, 98.0   warmth 24.73
added-light ratio  1.923  1.889  1.900     <- three equal channels
```

The shadow is 1.90x darker and 0.33 percentage points different in hue. Skyrim
is not painting a cool shadow there. What separates its shadow from its sunlight
is **depth and edge**, not colour.

## Why the first instrument said the opposite, and why that matters

The first version of `measure_light_split.py` binned each box's pixels into luma
deciles and compared the ends. It reported **-36.5** on ref 14's cobbles — a
large cool-KEY split, the opposite of both R6 and this document. The whole of it
was the material: within one cobble texture the dark pixels are mortar and
crevice and the bright pixels are stone tops, so the two arms were **different
albedos**, and the delta was a texture statistic wearing the light's clothes.

That is Rule 47 for the **sixth** time in this zone, and it survived the Rule 48
zero-dose control because that control was built on ONE albedo and the defect
needs two. Both are recorded inside the tool. The cure was Rule 47's own: hold
everything that is not the subject identical between the arms — here the
material — which on a photographed frame means a cast shadow edge, or blocks
coarse enough to average the texture away.

## Consequence

**Item 1 of the light brief is refused on its own evidence.** Warming the key
and cooling the fill would move `WARM_SPLIT` further above +14, i.e. further
from every reference frame in the set. If anything the lever points the other
way, and the honest reading of the table is that the hue of our light is not
where the difference between our picture and the reference lives.

Two things the same table says DO differ and are not this claim:

- **Our ground is far more chromatic than any reference ground** — box warmth
  46.8/51.8 against 24.4-38.5 everywhere in the reference set. That is R5's
  axis, not R6's, and it belongs to whoever owns the ground material.
- **Ref 03's forest floor holds a 2.28x luma range at 24 px block scale, from
  dapple alone.** Our 1.66x is one obelisk shadow across an otherwise flat
  field. That is R6's SECOND half and it is where the frame is actually lost.

Nothing in the render zone was changed for this document. The refusal is the
deliverable (Rule 45's stopping condition: refuse the quantity and write
nothing rather than fit a number through it).
