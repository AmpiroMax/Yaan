<!--
Created: 11:08:2026 - 13:23:37
Last updated: 12:08:2026 - 22:47:42
-->
<!--
UPD:
- 11:08:2026 - 13:23:37: Created from 16 reference frames the user dropped into images_examples/render (Morrowind / Oblivion / Skyrim). Frame index, then the invariants extracted from them, split into render's claims and design's claims. Zones: render and design. Lead owns this file; each zone lands its own consequences in its own docs.
- 11:08:2026 - 13:38:58: Correction from design, §3 tower row: a 6 m drum reads as an object only to 180 m (Rule 33: readable size = distance/30 from SILHOUETTE_MIN_PX 8 px), so "mid-distance anchor" was wrong -- 500 m needs a 17 m minor plan dimension, and frame 06's readable unit is the two-drum assembly, not one drum. I asserted a distance without doing the arithmetic the project already owns.
- 12:08:2026 - 22:47:42: Вторая поправка к строке про башни, и снова не моя: читаемая единица кадра 06 — СРОСШАЯСЯ связка барабанов со сплошным габаритом 8–10 м, а не сборка из двух барабанов с пролётом. Балка на 240 м есть десятая доля порога силуэта, поэтому пролёт несёт композицию, а не массу. «Стройте группой» было половиной ответа. Одну строку этого файла пришлось править дважды за два дня, оба раза потому, что я утверждал про дистанцию, не сделав арифметику, которой проект уже владеет.
-->

# REFERENCE FRAMES — what the world must look like

Source: `images_examples/render/` — 16 screenshots supplied by the user on
11:08:2026, from **Morrowind**, **Oblivion** and **Skyrim**. They are the
target look. They are NOT ours and NOT ours to ship: third-party game
screenshots, kept out of git (see `.gitignore`), referenced by path only.
Nothing derived from them may be a copied asset — only a rule.

The user's words, verbatim:

> надо сделать атмосферу
> надо на нашей земле, что карта высот, множество воксельных объектов
> расставлять: башни, камни, бордюрчики, и тд. земля — конечно карта высот,
> но объекты на ней трехмерные
> облака, свет, цвет земли без повторяющихся крупных кусков, ничто не
> ощущается плоским, всё угловатое наклоненное, даже если равнина, она
> ухабистая, нет идеальноплоского мира как в майнкрафте

## 1. Frame index

Working copies (900 px, for reading) live in the session scratchpad; the
originals are the authority.

| # | file | game | what it is evidence of |
|---|---|---|---|
| 01 | image copy 10.png | Skyrim | third-person on a plateau; "flat" ground that is not flat |
| 02 | image copy 11.png | Skyrim | hamlet: windmill, timber hall, mist band on the mountain |
| 03 | image copy 12.png | Skyrim | forest floor at eye height; dappled light, rock outcrop |
| 04 | image copy 13.png | Skyrim | god rays through a layered sky; waterfall; stone bridge |
| 05 | image copy 14.png | Oblivion | dusk: pink sky, white tower, valley read purely by haze |
| 06 | image copy 15.png | Skyrim | two stone towers + timber span over water — object grammar |
| 07 | image copy 2.png  | Morrowind | Balmora street: stone retaining wall, kerbs, cobbles |
| 08 | image copy 3.png  | Morrowind | interior; warm point light, cluttered surfaces |
| 09 | image copy 4.png  | Morrowind | Ald'ruhn: giant organic silhouettes against a soft sky |
| 10 | image copy 5.png  | Morrowind | Vivec-style plaza: stairs, terraces, kerb lines, lantern |
| 11 | image copy 6.png  | Morrowind (remaster) | red sunrise, starfield still up, deep aerial haze |
| 12 | image copy 7.png  | Skyrim | storm sky over peaks — clouds with structure and depth |
| 13 | image copy 8.png  | Skyrim | night: aurora, stars, trees as flat silhouettes |
| 14 | image copy 9.png  | Skyrim | Whiterun market: cobbles, kerbs, awnings, props |
| 15 | image copy.png    | Morrowind | Ashlands road: leaning dead trees, boulders, fence line |
| 16 | image.png         | Morrowind | Bitter Coast: heavy warm fog, leaning canopy, boulders |

## 2. What every frame has and ours does not

These are stated as OUTCOMES (Rule 38), each with the frames that carry it.

### R1 — Distance is read from haze, not from geometry (01,02,04,05,11,12,16)
In every wide shot the far field loses contrast and shifts toward the sky
colour, and it does so CONTINUOUSLY, over the whole visible range, not as a
fog wall at the far plane. Frame 05 is the extreme case: the entire valley
is legible ONLY through haze layering — remove it and the picture is a flat
poster. Frame 16 does it warm and near (tens of metres); frame 12 does it
cool and far (kilometres). Claim to hit: a ridge at 2 km and a ridge at
500 m must differ in contrast, not only in size.

### R2 — The mist band below the cloud layer (02,04,12)
Distinct from R1. There is a horizontal layer of cloud/mist that INTERSECTS
terrain partway up the mountains, so the mountain is cut into a lit crown
and a hazed base. This one band is the largest single contributor to
"объёмные облака" in the reference, and it is far cheaper than volumetric
clouds. Frame 04 additionally has god rays fanning from behind the cloud.

### R3 — Sky is layered and structured, never a vertical gradient (04,11,12,13)
Frame 12: at least three cloud strata, self-shadowed, with holes that show
brighter sky behind. Frame 11: a starfield still visible through a red dawn.
Frame 13: aurora plus stars. Our current sky reads as one smooth ramp; the
user called our clouds "квадратные и плоские".

### R4 — The skyline is fringed, never a clean curve (01,02,04,12,13,16)
Conifers break the horizon into a jagged fringe in every outdoor Skyrim
frame. The tree line is doing silhouette work, not foliage work.

### R5 — Ground colour is multi-hue at several scales, with no readable tile
(01,02,03,15)
Frame 02: grey-brown ground carrying rust-red shrub patches. Frame 01: pale
tan, grey-green scrub, dark rock, all within one screen. Nowhere in 16
frames can a repeating tile be located by eye. This is the user's "цвет
земли без повторяющихся крупных кусков".

### R6 — Light has a warm/cool split, and shadows are soft and stable
(01,02,03,14)
Key is warm, shade goes cool and blue. Frame 03's forest floor is entirely
made of soft dappled shadow. Nothing in any frame flickers or re-resolves —
the user's "тень рисуется каждый раз как пламя" has no counterpart here.

## 3. What design must place — the object grammar

The user's list ("башни, камни, бордюрчики, и тд") is exactly what the
frames contain. Named, with their evidence:

| Object | frames | what it does for the picture |
|---|---|---|
| Stone towers / ruins | 06, 05 | vertical anchor — but see the correction below: a tower is a NEAR-mid object unless it is big |
| Timber spans, bridges | 04, 06 | crosses water, gives the eye a route |
| Boulders (1-4 m, rounded) | 01,05,15,16 | scatter that makes "flat" ground unflat |
| Rock outcrops through soil | 01, 03, 06 | the heightmap's bones showing |
| Kerbs, steps, retaining walls | 07, 10, 14 | "бордюрчики" — settlement floor grammar |
| Fence lines | 15, 02 | leads the eye along a road; cheap, high value |
| Windmill / working structure | 02 | silhouette with an axis that is not vertical |
| Low shrub / scrub clumps | 01, 02, 14 | breaks the ground-to-object seam |
| Leaning dead trees | 15, 16 | "всё угловатое наклоненное" made literal |

Two rules the frames enforce about all of these:

**D1 — Nothing sits axis-aligned.** Trees lean (16 is dramatic — trunks 15-25°
off vertical), boulders rest at arbitrary rotations, roofs are pitched cones
and wedges (02), stairs and terraces cut diagonals (10). The only vertical
lines in the whole set are man-made walls, and even those are irregular
courses (06, 07).

**Correction to the tower row (design, from Rule 33 arithmetic).** I wrote
"mid-distance vertical anchor" without doing the sum. `SILHOUETTE_MIN_PX` = 8 px
at `INTERNAL_RES` 640x360 makes readable size = distance / 30, so frame 06's
6 m watchtower drum stops being an object at **180 m**, not at 500 m. To anchor
at 500 m a tower needs a minor plan dimension of 17 m or more. Frame 06 is not
a counter-example, it is the answer -- but not in the way I first wrote it.

**Second correction, also design's, also against me.** I said the readable
unit was the *assembly*: two drums plus the timber span. Reopening the frame
says otherwise. The unit is a **FUSED cluster of drums** sharing wall, solid
girth 8-10 m, readable to 240-300 m. The span is composition and route, not
silhouette mass: at 240 m a 0.4 m beam is a tenth of `SILHOUETTE_MIN_PX`.

> **A gap fuses two masses into one readable object only if it is filled with
> material of the same order.** A curtain wall fuses; a beam does not.

So "build it as a group" was half the answer, and 500 m still demands 17 m of
SOLID. The brief gets cheaper for it: two fused five-metre drums buy a 240 m
anchor for almost the geometry of one.

**D2 — A plain is bumpy, not flat.** Frame 01 is the direct answer to
"нет идеальноплоского мира как в майнкрафте": it is a plateau, i.e. the
flattest thing in the set, and it still has rolling metre-scale relief,
pebbles, scrub, and three rock outcrops in one view.

## 4. Ownership

- **render** owns R1-R6 (haze, mist band, sky strata, light, shadow stability)
  and lands the consequences in its own docs and in NUMBERS.md.
- **design** owns the object grammar and D1-D2, and lands them in
  `docs/design/LANDSCAPE.md`.
- Anything that needs a number both zones must agree on goes to NUMBERS.md
  under Rule 35.

Acceptance for every item here is Rule 27: a frame from OUR build, at the
same kind of viewpoint as the reference frame it answers, archived in
`docs/acceptance/` with its recipe. Not a measurement in a thread.
