<!--
Created: 12:08:2026 - 22:57:02
Last updated: 12:08:2026 - 22:57:02
-->
<!--
UPD:
- 12:08:2026 - 22:57:02: Выделен из docs/design/LANDSCAPE.md (9786 строк против FILE_HARD_LIMIT 800): §10 (шапка), §10.1–§10.4.2. Чистый перенос — ни одна строка тела не изменена, ни один номер секции не изменён; адреса вида «LANDSCAPE.md §X» продолжают действовать, таблица § → файл живёт в docs/design/LANDSCAPE.md.
-->

## 10. THE OBJECT GRAMMAR — what stands on the heightmap (stage-5, from the 16 reference frames)

Source: `docs/REFERENCE_FRAMES.md` and the 16 frames in `images_examples/render/`.
Ownership per that document's §4: render owns R1–R6, **design owns the object
grammar and D1–D2**, and this section is where they land. Frame numbers below
are that document's index. Nothing here is a copied asset; every clause is a
rule derived by looking.

The two sentences this section exists to satisfy, verbatim:

> надо на нашей земле, что карта высот, множество воксельных объектов
> расставлять: башни, камни, бордюрчики, и тд. земля — конечно карта высот,
> **но объекты на ней трехмерные**
>
> ничто не ощущается плоским, всё угловатое наклоненное, **даже если равнина,
> она ухабистая**, нет идеальноплоского мира как в майнкрафте

Read them as one sentence and they already contain the ruling this section
derives independently in §10.2: **the ground is a heightmap and the thing that
makes it not flat is not the heightmap.**

---

### 10.1 D2 — «равнина ухабистая» is a DISPERSION, and it is measured DETRENDED

Frame 01 is the load-bearing frame. It is a **plateau** — the flattest thing in
sixteen frames, the frame that ought to look like our plain — and inside one
screen it carries rolling metre-scale relief, gravel, scrub clumps and three
separate rock exposures. It is the direct answer to «нет идеальноплоского мира
как в майнкрафте», and it answers it *on the flat ground*, which is the only
place the answer counts.

**This is a different band from `HILL_ANISOTROPY`.** That constant (2.5, §2.1)
stretches the 128 m octave into ridgelets; it is a SHAPE rule for the hill
band and it says nothing about the 10–40 m band. A world can satisfy
`HILL_ANISOTROPY` perfectly and still be a set of smooth elongated ramps —
which is a Minecraft superflat with a tilt applied, and it fails frame 01
exactly as hard.

#### 10.1.1 The hole in the current contract

§2.7 already sizes the plain: «flat to ±1.5 m overall» over `PLAIN_EXTENT`
400–600 m. **That is a bound on the TREND, and a perfectly smooth 1.5 m dome
600 m across satisfies it.** Nothing in the document currently forbids the
billiard table; §2.7 asserts micro-relief is «retained», but an assertion with
no instrument is Rule 30's exact failure, and §2.7 itself records that the
global micro octave **was backed out and never re-landed** (built on massif
benches only). So the plain today is defended by a sentence, not by a check.

#### 10.1.2 The instrument: `GROUND_RELIEF_SIGMA_20M`

> **Sample terrain height inside a disc of radius 20 m centred on the
> standpoint, fit and SUBTRACT a least-squares plane, and take the standard
> deviation of the residual. That residual σ is the bumpiness.**

Three properties, and each is why a simpler instrument fails:

- **Detrended, so a slope cannot pass as bumpiness.** Peak-to-trough over a
  window rewards a tilted plane; a tilted plane is the thing we are trying to
  forbid. Removing the plane is what makes the number mean «ухабистая» rather
  than «наклонная».
- **Fixed 20 m radius, so it names a BAND.** The window sets what the number can
  see: wavelengths well above 40 m are eaten by the plane fit, wavelengths below
  ~4 m are below the sampling floor (§10.2). The number is therefore about
  precisely the band that reads as «rolling» at eye height — 8–40 m.
- **σ of a residual, not a max, so one lucky bump cannot buy a pass.** A single
  boulder-sized spike in a smooth field barely moves σ; a genuinely rolling
  surface moves it a lot.

**Proposed value — and it is deliberately set BELOW what our own approved
octaves already predict, so that it is a floor that catches a MISSING octave
rather than a re-litigation of an approved one.** Reconciling against
NUMBERS.md rather than inventing (Rule 34): `GROUND_MESO` is 25–60 m at
1.5–4 m and `GROUND_MICRO` is 8–16 m at 0.3–0.6 m. Taking mid-range values as
sinusoids, the meso octave contributes a semi-amplitude near 1.4 m (σ ≈ 0.97 m
undetrended, and a 40 m window against a ~42 m wavelength loses roughly a third
to half of that to the plane fit, so ≈ 0.5–0.7 m), the micro octave contributes
σ ≈ 0.16 m, and in quadrature the design as approved should land near
**0.55–0.7 m**.

| | value | meaning |
|---|---|---|
| `GROUND_RELIEF_SIGMA_20M_MIN` | **0.35 m** | floor. Roughly half of what the approved octaves predict — so a correct build passes with 2× margin, and a build that has silently dropped either octave (which is the state §2.7 records) fails |
| `GROUND_RELIEF_SIGMA_20M_MAX` | **1.20 m** | ceiling, so the answer to «flat» never becomes unwalkable churn. Non-binding where §2.4 corridors and building pads already flatten by rule |

Where it is measured: **on the flattest legal ground in the world, including
inside `PLAIN_EXTENT`.** Measuring it on a hillside proves nothing — the
complaint is about the flat places, so the floor binds where the terrain is
otherwise most entitled to be smooth. Exempt: corridor masks, building pads,
the castle terrace, and the shore taper band of §2.7 (water flattens its own
margins, and that clause stands).

#### 10.1.3 The picture-side control: THE GROUND MUST CUT ITSELF

σ is a probe, and a probe is not a frame (Rule 27). The frame-side test, which
is what frame 01 actually shows:

> **From eye height on the flattest legal ground, looking level, at least
> THREE distinct ground crest-lines must cut across the frame between roughly
> 5 m and 60 m — places where near ground hides ground behind it.**

The geometry behind the threshold, so the number is derived and not chosen:
from eye height 1.7 m, the line of sight to ground at 40 m depresses by
atan(1.7/40) ≈ **2.4°**. Any local downslope steeper than that opens a pocket
of hidden ground with a shadowed lee, and a pocket edge is a crest-line the eye
reads as an edge. The approved octaves reach 5–18° of local slope, so they clear
this by a wide margin **if they are running**. Frame 01 shows at least three such
edges in the near field alone.

**Failure statement (F7).** The frame fails D2 if the ground runs unbroken
from the player's feet to the tree line — one continuous shaded surface with no
edge on it. That is what a superflat world looks like from eye height, and it is
what «плоско как в майнкрафте» describes. A frame that cannot show that
sentence being true or false is not an acceptance frame for D2.

---

### 10.2 D2b — THE SUB-4-METRE BAND IS NOT THE HEIGHTMAP'S JOB, AND NO NUMBER OF OCTAVES WILL CHANGE THAT

The tempting answer to «flat» is another octave. It is the wrong answer below
about 4 m, and the reason is arithmetic we have already approved.

**The sampling floor is `LOD_VOXEL_SIZE_L0` = 1.0 m.** The nearest detail level
carries one height sample per metre. An octave of wavelength 2–4 m is therefore
sampled 2–4 times per period: it does not render as relief, it renders as
aliasing that swims when the camera moves — the same class of defect as the
shimmer render just spent a commit killing. **Anything the eye must read as
structure below ~4 m of wavelength has to be an object, because the heightmap
cannot carry it at the resolution we ship.** That is the seam, and it is derived
from an approved constant rather than asserted:

> **The heightmap owns 4 m and up. Objects own 0.1–4 m.**

And even with infinite resolution the heightmap would still lose, because three
things visible in the frames are not functions of (x, y):

1. **Overhang and undercut.** Frame 16's foreground boulder dome and frame 15's
   right-hand boulder wall both go vertical and then tuck back under. A
   heightfield has exactly one z per column; it can approach vertical and never
   pass it.
2. **A hard rim with a shadow line under it.** Frame 03's bedrock slab, left of
   frame, is read entirely by the dark line beneath its lip. Noise has gradients
   and no edges — you can make a steep ramp, you cannot make an edge. That
   shadow line is the single strongest «this is a solid object standing on
   ground» cue in the whole set, and it is the one thing a splat map can never
   fake.
3. **Material change AT the silhouette.** The boulder is grey where the ground
   is brown, and the change happens across one pixel at the object's own
   outline. Splat blending changes over metres by construction (§4).

**Therefore the user's sentence is the ruling, and this section only supplies
its reason:** «земля — конечно карта высот, но объекты на ней трехмерные».
D2 is satisfied by TWO things that must both be present — the octaves of §10.1
above 4 m, and the object grammar of §10.5 below it. Either alone fails, and
they fail in visibly different ways: octaves without objects give smooth dunes
(a lava lamp), objects without octaves give a flat table with props on it
(a diorama).

---

### 10.3 D1 — nothing stands on an axis, and TILT IS NOT JITTER

Across sixteen frames the only true vertical lines are man-made stone: the tower
drums of 06, the retaining wall of 07, the terrace faces of 10 — and even those
are irregular course by course. Everything else tilts: trunks 15–25° off
vertical in 16 and further in 15, boulders resting at arbitrary rotations in
01/05/15/16, roofs as cones and wedges in 02/07, stairs and terraces cutting
diagonals in 10, the windmill's sail cross sitting at 45° rather than at 0/90
in 02.

**The composition argument, which is why the vertical exception list must stay
short: a vertical is only a signal in a world where nothing else is vertical.**
Frame 05's white spire reads across a whole valley at dusk because it is the
only straight line in the picture. If our fences, trunks and rocks all stand to
attention, the tower has nothing to be different from.

#### 10.3.1 The rule that most implementations get wrong

A uniform random ±X° per instance is **not** what the frames show, and shipping
it produces a world that reads as noisy rather than as weathered.

> **Every tilt has an AZIMUTH SOURCE, and only boulders may use a free one.**

In frame 16 the leaning canopy leans *coherently* — the trees agree about which
way the wind blows. In frames 03 and 06 the rock slabs share a bedding dip, and
that shared dip is the entire reason they read as one bedrock instead of as
scattered props. A field of independently tilted objects reads as debris; a
field of objects that agree about a direction reads as a place with a history.
The exception is genuine decay: fence posts in 15 rot and lean independently,
and *there* independence is the truth.

#### 10.3.2 The tilt table

Tilt is measured from the object's own natural axis (vertical for standing
things, the bedding plane for rock, level for built horizontals).

| class | tilt | azimuth source | yaw |
|---|---|---|---|
| Boulder 0.8–4 m | free, uniform on SO(3) — **but** long axis within 40° of horizontal for ≥ 85% of instances (a rock on end is a menhir, and we already have standing stones as an L2 class) | free | free |
| Rock slab / outcrop | bedding dip **5–25°** | **regional plane; dip azimuth coherent over ≥ 200 m** | locked to bedding, not free |
| Standing tree, open ground | 2–6° | wind field (`WIND_FIELD_*`) | free |
| Standing tree, on slope | 4–12° | downslope + crowding (§5.10) | free |
| Standing snag / dead tree (§5.9) | **12–30°** | wind azimuth ± 25° | free |
| Fallen log | lying; long axis across the fall line (§5.10, unchanged) | fall line | free |
| Fence post | **3–15°, independent per post** | none — this one is decay | ± 10° about the run |
| Timber prop / brace | 15–35° from vertical, **into** the load | geometry | — |
| Kerb, step tread | top face ≤ 2° from level | — | plan line follows ground; no straight run > 12 m |
| Retaining wall face | **3–8° batter, leaning into the bank** | — | — |
| Tower drum axis | ≤ 1.5° | — | per-block yaw ± 8°, course offset ± 0.15 m |
| Roof | pitch 35–50°; cones on drums | — | ridge line need not be level: ± 3° sag |
| Windmill sail cross | axle horizontal ± 5°; cross at **45° ± 15°** from vertical | — | — |

The windmill row is not decoration: frame 02's whole silhouette contribution is
four diagonals against a mountain, and it gets them by refusing to put a sail at
twelve o'clock.

#### 10.3.3 The verticals that survive, and what they owe

Man-made stone keeps its axis (≤ 1.5°) and pays for it in irregularity instead:
per-block yaw ± 8°, course offset ± 0.15 m, and a crown that is never a smooth
arc (§10.5, B4). Frame 06's drums are plumb and still not one straight edge long
enough to read as a chess piece. **Plumb axis, ragged surface** is the formula.

---

### 10.4 RULE 33 APPLIED TO SCATTER — the read-distance ladder, and the diagnosis it produces

§1.5/§1.6 fix the instrument: `SILHOUETTE_MIN_PX` = 8 px at `INTERNAL_RES`
640×360, so **readable size = distance / 30**. Invert it and every scatter class
gets a hard expiry:

> **An object of size S stops being an OBJECT at d = 30·S. Past that it is
> texture, and it contributes nothing to «not flat».**

| object | size | reads as an object out to |
|---|---|---|
| pebble / cobble | 0.2 m | 6 m |
| shrub, single | 0.6 m | 18 m |
| fence post | 1.2 m | 36 m |
| boulder, small | 1.5 m | 45 m |
| boulder, large | 4 m | 120 m |
| shrub clump | 5 m | 150 m |
| watchtower drum (minor plan dim.) | 6 m | 180 m |
| rock outcrop, boss | 10 m | 300 m |
| rock outcrop, large | 25 m | 750 m |
| civic tower (frame 05) | 30 m | 900 m |

#### 10.4.1 THE FLATNESS COMPLAINT IS A MID-FIELD COMPLAINT

This is the finding the ladder produces, and it is the one I would defend
hardest.

A world with grass, trees and mountains populates **0–30 m** (grass, near
trunks) and **1 km +** (the massif) and has *nothing in between*. In the
60–400 m band the ground is then a smooth shaded ramp with no object silhouette
standing on it — no edge, no cast shadow, no occlusion of ground by ground. **A
smooth shaded ramp with nothing on it is exactly what «плоско как в майнкрафте»
describes**, and it describes the mid field, because the near field always looks
fine (grass hides everything) and the far field always looks fine (mountains
carry it).

Count what populates the mid field in frame 01: two or three rock exposures, a
conifer group, a second conifer group, the plateau lip itself, several scrub
patches. Five to eight distinct silhouettes, none of them a tree in the near
field and none of them the mountain.

> **`MIDGROUND_OBJECT_COUNT_MIN` = 5.** From any standpoint on open ground, an
> acceptance frame must contain at least five distinct object silhouettes that
> are (a) at least `SILHOUETTE_MIN_PX` across, (b) neither near-field vegetation
> nor the far massif, and (c) sitting between the near ground and the horizon.

It is deliberately defined by what is *countable on a frame* rather than by a
distance estimate, because distance in a screenshot is a guess and a count is
not. Frame 01 scores 5–8 depending on whether the scrub patches clear 8 px; our
build's count is the number to go and get.

#### 10.4.2 The corollary: A CLASS SERVES ONE BAND ONLY

Boulders at 1–4 m expire at 120 m. They cannot fix the mid field past that
distance however many you scatter, and scattering more of them is the obvious
wrong response to a mid-field failure — it costs draw calls and moves nothing in
the frame. **Each band must be populated by a class sized for it**, which is why
§10.5 opens with outcrops rather than with boulders: outcrops at 5–25 m are the
only natural class whose expiry lands in the 150–750 m band.


---

