<!--
Created: 12:08:2026 - 22:57:02
Last updated: 12:08:2026 - 22:57:02
-->
<!--
UPD:
- 12:08:2026 - 22:57:02: Выделен из docs/design/LANDSCAPE.md (9786 строк против FILE_HARD_LIMIT 800): §10.5, брифы B1–B9. Чистый перенос — ни одна строка тела не изменена, ни один номер секции не изменён; адреса вида «LANDSCAPE.md §X» продолжают действовать, таблица § → файл живёт в docs/design/LANDSCAPE.md.
-->

### 10.5 THE PLACEMENT BRIEFS

One brief per class. Each states, in this order: **what it is for**, **the band
it serves** (Rule 33, §10.4), **size**, **anchor** — what in the world decides
where it goes, **rotation and tilt** (§10.3), **density**, **cost**, and
**failure statement** — the sentence an acceptance frame must be able to make
false (F7, §1.6).

The order below is the build order (§10.6), not an alphabet.

---

#### B1 — BOULDERS (валуны), 0.8–4 m

**Evidence:** 01 (scattered on the plateau), 05 (a whole field carrying the
foreground), 15 (a run of them walling the sunken road), 16 (half-buried domes
with moss on their crowns).

**For:** the near and near-mid field. This is the class that makes the ground
under the player's feet three-dimensional, and it is the cheapest thing in this
document per unit of frame.

**Band:** 15–120 m. A 1.5 m boulder expires at 45 m, a 4 m one at 120 m.
**It cannot help past 120 m and must not be asked to** (§10.4.2).

**Size:** `BOULDER_SIZE` 0.8–4.0 m, distribution weighted to the small end.
**Within one cluster, the largest and smallest must differ by ≥ 1.6×** — a
scatter of same-sized rocks reads as a tiling pattern, which is the failure mode
we are trying to leave, in a new costume.

**Burial — the single most important number in this brief.**
`BOULDER_BURIAL_FRAC` = **0.25–0.55** of the boulder's vertical extent sits
below the ground surface. An unburied boulder rests on the terrain with a
visible contact ellipse and reads instantly as *placed*; both frame 15 and
frame 16 show rock **emerging** from the soil, and frame 16's foreground dome
shows about half of an ellipsoid. Burial also solves the slope-contact problem
for free: a buried rock cannot float on a hillside.

**Grouping — boulders are NOT blue-noise.** `BOULDER_CLUSTER_SIZE` 3–9 stones,
cluster span 6–20 m, and `BOULDER_CLUSTERED_FRAC` = 0.60–0.75 of all boulders
belong to a cluster; the remainder are singletons. Frame 15's boulders run in a
line along the road bank; frame 05's carpet the bluff. A uniform sprinkle is the
signature of scatter code and reads as one.

**Anchor — A BOULDER COMES FROM SOMEWHERE.** A rock alone in open grass with no
source above it reads as a prop; every frame in the set puts its boulders below
something that could have shed them. Rule: **every cluster must have, within
60 m uphill, either a scarp (§2.7), a rock outcrop (B2), or ground at slope
≥ `SLOPE_ROCK_MIN`.** Preferred sites, in order: the toe of an outcrop, below a
scarp lip, stream banks and the outside of river bends, ridge shoulders.

*One deliberate exception:* the **erratic** — a single 3–4 m stone in open
ground with no source, rare enough to be an event
(`BOULDER_ERRATIC_DENSITY` ≤ 0.05 / ha). Because it is rare and large it reads
as a landmark rather than as debris, which is the opposite of the failure the
source rule exists to prevent. It is also a legitimate L2 guide.

**Tilt:** free uniform rotation on SO(3) — this is the one class that gets a free
azimuth (§10.3.1) — with the single constraint from the table: the long axis
stays within 40° of horizontal for ≥ 85% of instances.

**Density:** `BOULDER_DENSITY_ANCHORED` 1.5–4 / ha near an anchor,
`BOULDER_DENSITY_OPEN` 0.1–0.4 / ha elsewhere.

**Cost:** a convex blob at 40–80 tris (`ROCK_BLOCK_TRI_BUDGET_MAX` is already 60
for the massif stacks, and the same asset class serves). At 3 / ha over the full
120 m read disc — 4.5 ha, an upper bound since the frustum is a quarter of it —
that is ~14 boulders and **under a thousand triangles for the entire near-field
population.** This is why B1 and B2 are first: they are the largest change in
the frame per triangle spent.

**Failure statement:** the frame fails if boulders sit on the ground with a
visible contact seam, or if two neighbours in one cluster are the same size, or
if a cluster has no source above it.

---

#### B2 — ROCK OUTCROPS (выходы породы), 3–25 m

**Evidence:** 01 (three separate exposures in one plateau view — the lead's own
count, and it is right), 03 (a slab, left, plus a cliff mass filling the upper
right), 06 (bedded shelves stepping into the water and carrying both towers),
10 (natural rock deliberately left standing inside a built plaza).

**For:** THE MID FIELD, which §10.4.1 identifies as where the flatness complaint
actually lives. This is the class that literally is the user's sentence: the
heightmap's bone breaking through the soil.

**Band:** 90–750 m. A 3 m slab expires at 90 m, a 10 m boss at 300 m, a 25 m
mass at 750 m. **No other natural class covers 150–750 m.**

**Two sub-forms, and the distinction is load-bearing:**

- **Pavement / slab** (03, 01). Near-flat bedrock with soil in pockets, proud of
  the ground by 0.1–0.6 m, extent 3–15 m. Frame 03's forest floor is *mostly*
  this — bare rock with soil in the hollows, not a soil texture with rocks on it.
  **The rim must be geometry even if the face is splat**, because the shadow line
  under the lip is the entire read (§10.2, point 2). A slab drawn purely as a
  splat patch is a stain, not a rock.
- **Boss / tor** (01 far field, 03 upper right, 06). A mass 2–8 m proud and
  5–25 m across, with visible bedding steps and a broken top.

**Bedding — and this is a free consistency win.** §4.1 already rules that rock
strata are defined in **absolute world height, globally**, never as a fraction of
each landform. Outcrops inherit that rule unchanged: the same pale band that
crosses the massif crosses a 6 m boss on the plain at the same elevation. A
stratum that lines up across the whole world reads as geology; one that scales to
each rock reads as paint. It costs nothing because the field already exists.
Dip 5–25°, dip azimuth coherent over ≥ 200 m (§10.3.2).

**Anchor — outcrops appear where erosion STRIPS, never where it deposits.**
Implementable directly against the meso field: place where local mean curvature
is **convex** above a threshold — ridge shoulders, spur noses, scarp lips, the
outside of river bends. **Forbidden in concavities** (hollows collect soil),
in the floodplain, and inside building pads. This one rule is the difference
between rock that explains the terrain and rock sprinkled on it.

**Tilt:** bedding only. No free yaw — the outcrop's fabric *is* the bedding, and
a randomly spun boss breaks the shared-plane read that makes a group of outcrops
one bedrock (§10.3.1).

**Density:** `OUTCROP_DENSITY` 0.4–1.2 / ha in open and rocky ground, tapering to
zero in floodplain and pads. **Plus the frame-01 control: from a standpoint on
open ground, at least three outcrops in view.** Frame 01 has exactly three, on a
plateau, and that is the number the lead pointed at.

**Cost:** a boss at 300–600 tris. At 0.8 / ha over a 300 m read disc — 28 ha,
again an upper bound — that is ~23 bosses at ~9 000 tris, against
`MASSIF_ROCK_TRI_BUDGET_MAX` = 60 000 for a single massif. Affordable, **but it
needs LOD**: two levels, full geometry inside ~150 m and a ≤ 60-tri silhouette
blob beyond, since past 150 m a boss is a shape and not a surface.

**Failure statement:** the frame fails if the mid field contains no rock; if
neighbouring outcrops disagree about their bedding direction; or if an outcrop
sits in a hollow.

---

#### B3 — FENCE LINES (изгороди) — the cheapest thing in this document, and it is also an INSTRUMENT

**Evidence:** 15 (posts on both banks of the sunken road, a rail run spanning the
gap, and the whole thing derelict), 02 (a paddock fence right of the timber hall).

**For:** leading the eye along a road — and, more importantly than that:

> **A fence line is a CONTOUR GAUGE laid on the land.** Post bases sit on the
> terrain, so the rail line draws the ground's own profile in the air where the
> eye can see it against the sky. It converts D2's relief from something you must
> infer out of shading into a visible line.

That is why it ranks second in the build order despite being a prop: it does not
merely benefit from bumpy ground, it **proves** bumpy ground. It is simultaneously
set dressing and the acceptance device for §10.1.

**Band:** Rule 33's fourth case in this document. A 1.2 m post expires at 36 m —
**but the readable unit is the LINE, not the post.** A 40–80 m run of regularly
spaced posts reads as a dotted line to roughly 300 m, exactly as
`SPIRE_GROUP_SPAN` argues that the readable unit is the group and not the spire.

**Size:** `FENCE_POST_HEIGHT` 0.9–1.5 m, `FENCE_POST_SPACING` 1.8–3.0 m,
`FENCE_RUN_LENGTH` 15–80 m.

**Broken by rule:** `FENCE_GAP_FRAC` = 0.10–0.30 of a run has posts or rails
missing. Frame 15's fence is half gone, and that is what makes it read as an old
world with a history rather than as a level-designer's arrow. A complete fence is
a fence; a broken fence is a place.

**Anchor:** parallel to a road or corridor at `FENCE_ROAD_OFFSET` 2–5 m, or
enclosing a field beside a settlement. Follows the corridor's plan curve, never a
surveyed straight line.

**Tilt:** each post independent, 3–15° (§10.3.2 — genuine decay, genuinely
uncorrelated), yaw ± 10° about the run. **The rail sags between posts.** A
perfectly straight rail is a hairline and is the single tell that gives the asset
away.

**Cost:** post 8–12 tris, rail 4 tris per bay. A 60 m run at 2.4 m spacing is
25 posts and 24 bays ≈ **350 triangles.** Essentially free.

**Failure statement — and it is the sharp one:** the frame fails if the fence's
top line is **straight in screen space** over its whole run. A straight fence top
means flat ground under it, which means D2 failed and the fence has just reported
it.

---

#### B4 — TOWERS AND RUINS

**Evidence:** 06 (two stone drums flanking a timber span, standing on an
outcrop), 05 (a distant white civic spire, the only true vertical in a whole
valley at dusk).

**For:** a vertical anchor — but Rule 33 says something uncomfortable about how
far that works, and it corrects a phrase in REFERENCE_FRAMES.md.

**Band — the arithmetic, because it changes the brief:** the readable dimension
of a vertical mass is its **minor plan dimension**, and it must be ≥ d/30 at the
distance it is meant to anchor.

| tower | minor plan dim. | anchors out to |
|---|---|---|
| watchtower drum (frame 06) | 5–6 m | **150–180 m** |
| to anchor at 500 m | **≥ 17 m** | — a keep or a group, not a tower |
| civic spire (frame 05, at ~900 m) | **≥ 30 m** | consistent with what that frame shows |

> **A lone 6 m tower on a distant ridge is a wasted asset.** It is a 180 m
> object. Either site it within ~180 m of a route the player walks, or build a
> **group** — frame 06 is two drums plus the span between them, and the readable
> unit is the whole assembly, gap included. Fifth Rule-33 case in this document.

**Silhouette — the read is the CROWN.** Frame 06's drums are unmistakable at a
distance because their tops are broken and uneven. A smooth cylinder top is a
chess piece. Rule: the crown must break the vertical in **at least 3 places**,
notch depth ≥ 0.5 m, and the crown line must vary by ≥ 1 m across the drum.

**Anchor:** **on rock, not on soil.** Frame 06's towers stand on the bedded
outcrop, and that is not decoration — outcrop plus tower is one composite mass,
so it reads further than either alone, and it explains why anyone built there.
Attach B4 siting to B2 by rule.

**Tilt:** axis ≤ 1.5°, per-block yaw ± 8°, course offset ± 0.15 m (§10.3.3).
A ruin additionally gets up to 4° of lean on the surviving stub, and a **talus
skirt of B1 boulders at its foot** — which satisfies B1's source rule by
construction, since the tower *is* the source.

**Density:** none given deliberately. Towers are L1/L2 siting under §1.3's
hierarchy and §1.3a's tiers, not scatter, and inventing a per-hectare number here
would create a second placement authority for the same objects.

**Failure statement:** the frame fails if the crown reads as a smooth arc; if the
drum's silhouette edge is a single straight line from base to crown; or if the
tower stands on graded soil with no rock under it.

---

#### B5 — KERBS, STEPS, RETAINING WALLS (бордюрчики)

**Evidence:** 07 (a dry-stone retaining wall holding a level change beside the
street, cobbles, a two-course brick step at the door), 10 (stairs and terraces
cutting diagonals across the whole plaza), 14 (a kerb edging a planted bed, a
low well parapet, steps into the market).

**For:** making a settlement floor read as **built** rather than as a painted
patch of a natural surface. They do it by putting **horizontal lines at known
heights** into a frame — and a settlement is the only place in the world where a
horizontal line is permitted (§10.3.3).

**The ruling this brief exists to produce:**

> **Inside a settlement pad, a level change of ≥ 0.4 m must be resolved by a
> BUILT EDGE — kerb, step, or retaining wall — and never by a graded slope.**

That single rule is most of the difference between frames 07/10/14 and a village
dropped onto a heightmap. Graded ground inside a built place says nobody
built it.

**Size:** `KERB_HEIGHT` 0.15–0.30 m; `STEP_RISE` 0.15–0.20 m with tread
0.30–0.45 m; `RETAINING_WALL_HEIGHT` 0.8–2.5 m with 3–8° batter into the bank.
**`STEP_RISE` must agree with `PLAYER_STEP_HEIGHT`** — a step the player cannot
walk up is a bug that looks like architecture. That is a Rule 35 number: it is
flagged for NUMBERS.md in §10.7 rather than settled here.

**Band — Rule 33's sixth case.** A 0.25 m kerb expires at 7.5 m as a
*silhouette*: it is a first-person, walk-past object and it earns nothing in a
vista. **But the LINE reads far**, because a kerb run is a value edge rather than
a silhouette — a 20 m run reads to roughly 150 m. This is why settlements in
frames 10 and 14 read from a distance as a pattern of lines, and it is why kerbs
are worth building despite the size arithmetic.

**Plan line:** follows the ground contour or the building line. **No straight run
longer than 12 m without a jog or a change of level** — frame 10 does this
constantly, and it is what stops a plaza reading as a floor tile.

**Failure statement:** the frame fails if any level change inside the pad is a
grass ramp; if a kerb runs dead straight for more than 12 m; or if the built floor
meets natural ground with no edge between them.

---

#### B6 — SHRUB AND SCRUB CLUMPS (куртины кустарника)

**Evidence:** 01 (grey-green clumps on pale tan), 02 (a rust-red mass filling the
foreground against grey-brown ground — the strongest single colour move in all
sixteen frames), 14 (yellow-green beds inside the market).

**For:** two jobs, and the first is the one everyone skips.

1. **Breaking the ground-to-object seam.** A boulder standing on bare ground has
   a hard contact line and reads as placed. A tuft at its foot removes the line.
   Rule: `SHRUB_SKIRT_FRAC` — **50–80% of all boulders, outcrop rims, posts and
   trunks carry at least one shrub or grass tuft within 0.5 m of the contact.**
   This is what separates 15 and 16 from a prop scatter, and it is nearly free.
2. **Carrying R5's second hue.** Frame 02's rust-red on grey-brown is the extreme
   case. Flagged to render and flora as the ground-colour partner of R1/R5 —
   design's part is only that the clumps exist and that their colour is
   *different from the ground*, not a darker version of it.

**Band:** a 0.6 m shrub expires at 18 m; a 5 m clump reads to 150 m. **The CLUMP
is the readable unit** (seventh Rule-33 case). `CLUMP_SPAN` 2–6 m, 4–12 plants per
clump. Consequence handed to render: individual shrubs drawn beyond ~20 m are
wasted draws and should collapse into the clump's own representation.

**Failure statement:** the frame fails if any placed object meets the ground with
a visible hard contact line, or if the clumps are the same hue as the ground they
stand on.

---

#### B7 — LEANING DEAD TREES (наклонённые сухие деревья)

**Evidence:** 15 (a whole stand of them, trunks 20–40° with real curvature —
this frame is D1's poster), 16 (living canopy leaning 15–25° and leaning
*together*).

**This is NOT a new class.** §5.9 already approves the **standing snag** with
densities `SNAG_DENSITY_FOREST` 1.5–3 / ha and `SNAG_DENSITY_OPEN` 0.25–0.5 / ha,
a material split, and a 30–60 tri asset. Inventing a competing "dead tree" class
here would create two authorities for one object. **B7 supplies only the property
§5.9 was missing: the lean.**

- `SNAG_LEAN` = **12–30°** from vertical.
- Azimuth = the wind field azimuth ± 25° (§10.3.1) — snags in one locality lean
  *together*, which is what frame 16 shows and what a per-instance random tilt
  would destroy.
- Heights and densities: unchanged, §5.9 governs.

**Why it is worth doing early even so:** a bare trunk is 30–60 triangles and it
draws a diagonal across the sky. Per triangle it is the loudest possible
statement of «всё угловатое наклоненное», and NUMBERS.md currently records the
snag constants as **НЕ ПОСТРОЕНО with no consumer at all** — so the asset exists,
the rule exists, and the world has none.

**Failure statement:** the frame fails if snags stand plumb, or if neighbouring
snags lean in unrelated directions.

---

#### B8 — TIMBER SPANS AND BRIDGES

**Evidence:** 04 (a low twin-arch stone bridge at ~80 m), 06 (a timber span
carried on a prop that leans into the water).

Siting belongs to §3 (water) and to core's corridor pass; this brief supplies only
the two rules the frames enforce.

- **The read of an arch is the HOLES, not the mass.** At 80 m frame 04's bridge
  is a dark bar with two bright apertures in it, and that is the whole
  recognition. Rule: **an arch's clear opening must be ≥ d/30 at the distance the
  bridge is meant to be recognised from** — a 2.5 m opening reads to 75 m. Below
  that the bridge reads as a wall and stops being a bridge.
- **The prop is the D1 element.** Frame 06's span rests on a brace 15–35° off
  vertical, leaning into its load. It is the piece that stops a bridge being two
  rectangles.

---

#### B9 — WINDMILL / WORKING STRUCTURE

**Evidence:** 02 — a stone drum, timber upper, conical shingle cap, and a sail
cross at 45°.

At most one per hamlet. Its entire value is that it owns **an axis that is not
vertical**: the axle is horizontal and the cross sits at 45°, so it throws four
diagonals against the sky where every other man-made thing throws verticals and
horizontals. `WINDMILL_SAIL_SPAN` 8–12 m reads to **240–360 m**, which makes it
the best silhouette a hamlet can buy for its cost. The conical cap is the D1
element on the roof line (§10.3.2).

**Failure statement:** the frame fails if the sail cross sits at 0/90°, or if the
mill's cap is a flat disc.

---

