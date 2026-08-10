<!--
Created: 10:08:2026 - 22:00:42
Last updated: 10:08:2026 - 22:14:33
-->
<!--
UPD:
- 10:08:2026 - 22:00:42: Created the example world-map set the user asked for by name («примеры из N карт, я вообще напрямую хочу карту нарисовать»). Six maps as CONTENT, coordinated with mapdesign who owns the format: their four acceptance branches (M1 island, M2 home-valley continuity, M3 sea-vs-lake flood fill, M4 endorheic) are each carried by one map, plus two chosen for range of PLACE rather than branch coverage. Written against mapdesign's corrected elevation model — distance from DRAINAGE, not from coast — so local minima, lakes at altitude, coastless maps and sea cliffs are all legal, and the drawing lesson is «river spacing is elevation». Proposes LF-9 salt marsh, LF-10 maritime heath, LF-11 relict shoreline with recipe/acceptance/control per LANDSCAPE 2.10 rule 5; records five known gaps as requests (delta, incision, tides, rain shadow, drawn crest) rather than describing them as free; recommends Farness first with the Vaelmere sketch as its control.
- 10:08:2026 - 22:13:54: TOOL CHANGED — Azgaar's FMG supersedes the PNG sketch (design's WORLD_MAP.md §9, ddd34c0). Map CONTENT is unaffected and deliberately NOT rewritten; §0.1 and every "what is painted" paragraph are marked superseded pending design's importer schema. Ran design's requested Seremarch check from FMG's documented source rather than the browser: the endorheic map is PRODUCIBLE — `Mask` with a NEGATIVE fraction is the documented operation that lowers the map CENTRE instead of the edges, so land at every border is authorable, and FMG's lake type is computed from flux vs evaporation so a terminal salt lake is native. The risk is relocated, not removed: it moves from "can the tool draw it" to "do FMG's downstream modules assume an ocean exists", and Seremarch gains a NEW authoring constraint nobody had — the lake's salt is CLIMATE-DERIVED, not drawn, so the map must be sited hot and dry enough that evaporation beats inflow or LF-11's entire premise is lost. Also found: FMG's `dry lake` type is a native real rejected instance for LF-11, which answers design's synthetic-control flag on that entry.
- 10:08:2026 - 22:14:33: Исправление: запись 22:13:54 попала в блок Created/Last updated вместо блока UPD, и Last updated не был поднят. Обе ошибки — Rule 16: заголовок правился программой, которая нашла ПЕРВЫЙ `-->`, а не нужный, и header_check это пропустил, потому что он сверяет Last updated с самой новой записью UPD — а запись, положенная не в тот блок, для него просто не существует. Прибор мерил СОГЛАСОВАННОСТЬ двух полей, а дефект был в РАЗМЕЩЕНИИ третьего (Rule 41). Поймано перечитыванием файла после публикации, а не проверкой (Rule 34).
-->

# WORLD_MAPS.md — Six Example Worlds, as Places

> ## ⚠ STATUS — THE AUTHORING TOOL CHANGED; THE PLACES DID NOT
>
> The user found **Azgaar's Fantasy Map Generator** and it replaces the
> six-colour PNG sketch as the authoring tool (design's `WORLD_MAP.md` §9).
> What that supersedes here is **§0.1 (the format paragraph) and every
> "what is painted" section** — those become "what is configured, and what
> the export must contain", and they are **deliberately not rewritten yet**:
> design holds the importer schema, and writing against a schema that has not
> landed is the exact parallel-schema failure this document avoided once
> already.
>
> **Everything else stands.** The six maps are content — geography, names,
> why anyone lives there, what the walk is like — and no part of that
> depended on which tool draws the boundary. So does §0.2 (our elevation
> model is still distance-from-drainage; FMG's own `h` never becomes our
> elevation, per §9.2), §9 (the three landforms, all accepted), §10 (the
> gaps), and §11 (build Farness first, Vaelmere beside it as its control —
> that reason never mentioned the tool).
>
> **Seremarch survives, and moved from "at risk" to "constrained"** — see
> §7's closing note.

**What this is.** Six worked example maps, written as CONTENT — places, not
algorithms. The user asked for these by name («хочу чтобы дизайнеры истории
предоставили мне примеры из N карт, я вообще напрямую хочу карту нарисовать»)
because he intends to **draw a map himself**. These are the examples he looks
at to decide what he wants, and the set the sketch pipeline gets tested
against.

**Ownership.** `mapdesign` owns the FORMAT — the sketch files, the scale, the
acceptance. Story owns the CONTENT — what places exist, why anyone lives
there, and what the walk is like. Every format fact below is theirs, cited,
not invented here; their doc (`docs/design/WORLD_MAP.md`) is the authority and
this document follows it rather than running beside it.

**Companion docs:** `BIBLE.md` (canon — Ealdmarch, the Feast, the naming
rites), `docs/design/LANDSCAPE.md` (§2.10 landform dictionary, §3 water, §5
flora, §8 stand briefs), `ACT1_VALLEY.md` (the valley we already have).

---

## 0. If you are about to draw a map, read this page and nothing else

### 0.1 The format, in one paragraph (mapdesign's) — ⚠ SUPERSEDED BY FMG, see the status note above

You draw a **PNG, 640 × 640 pixels = a 10 × 10 km world** at
`SKETCH_METRES_PER_PIXEL` = **16 m/px** (one pixel is exactly one hydrology
cell, so a drawn river needs no resampling). Six exact colours, and no others:
**blue** water, **green** land, **dark green** forest (an override), **grey**
mountain (an override), **red** river — draw main channels **2 px wide**,
which is the real 25–35 m navigable width — and **yellow** for a site, a blob
of 3 px or less. Whether a blue region is sea or lake is **not drawn**: water
touching the image border is sea, water that does not is a lake. Beside the
PNG sits a **JSON** sidecar naming the map, its seed, its world origin, its
metres-per-pixel, its **declared landforms**, and one entry per yellow site
and per red river. Unknown keys are a hard error. That is the whole format.

For scale while you draw: one chunk is 16 px, one Voronoi polygon is about
**37 px**, our entire existing world is the **128 × 128 px corner**, and a
named region — a forest, a massif, a marsh — is roughly **40–100 px** across.

### 0.2 The one thing that decides everything you can draw

Our pipeline is **not** Amit Patel's elevation model. He derives elevation
from distance to the coast; mapdesign rejected that for us, because it makes
volcanic islands rather than countries and because it would overwrite the
terrain we already have. What we use instead:

> ### **THE WATER YOU DRAW IS THE LOW GROUND. EVERYTHING ELSE RISES AWAY FROM IT.**
> Your rivers, lakes and coast form one drainage network. Each river is given
> heights that only ever decrease from source to mouth, and then **land
> elevation = the nearby drainage's height + a rise with distance from the
> nearest water**, with the massif stamps, ridge-and-swale relief and erosion
> gullies laid on top.

The single practical consequence, and the sentence to keep in your head while
your pen is moving:

> ### **RIVER SPACING IS ELEVATION.**
> Two rivers drawn close together leave a low ridge between them. Two rivers
> drawn far apart leave a high divide between them. **You do not draw
> mountains by drawing mountains; you draw them by leaving space.** And
> because moisture also comes from water, the same spacing decides the
> forests: near the rivers is wooded, the wide divides are bare. One stroke
> of the pen sets both the height and the vegetation of everything between.

Everything else follows:

| What you draw | What you get |
|---|---|
| two rivers close together | a **low** wooded ridge between them |
| two rivers far apart | a **high, dry** divide between them |
| a region with no water in it at all | high ground, whether you meant it or not |
| water on both sides of a strip of land | that strip is **low along its whole length**, and a mountain there is impossible |
| a coast cut into deep fingers | a **flat** country — nothing is ever far from water |
| grey paint | a massif, with its own authored internal shape (LANDSCAPE §2.8) |
| grey paint touching blue | a **sea cliff** — this is free, because elevation is not distance from coast |
| a lake with a river out of it | legal, at any altitude, at the height the descent gives it |
| a lake **above** the lake it drains into | **rejected loudly.** Water never gains height downstream |
| a river splitting downstream (a delta) | **not expressible.** Rivers merge going down; they never fork |
| a river that stops in the middle of the land | **rejected.** A river ends at water at exactly one end |
| no ocean anywhere | legal, and it is the branch nothing else exercises (map 5) |

The last three rows are the ones that will actually catch you out. The rest
is freedom, and there is more of it than Amit's model would have given us:
local minima are allowed, lakes at altitude are allowed, a coastless map is
allowed, and a mountain standing in the sea is allowed.

---

## 1. Why six

Two different requirements are being served, and they are not the same
measurement of the same set.

**mapdesign chose four**, and chose them well: each of their four is the only
map that can fail one branch of the pipeline — the canonical island, the
continuity case, the flood-fill case (which blue is sea?), and the endorheic
case Amit's model cannot do at all. That is Rule 30 applied to an example
set, and it is the right basis for **acceptance**.

**But the user did not ask for branch coverage.** He asked to see examples so
he can decide what he wants to draw, and "does the parser handle it" and "is
it a place worth walking for an hour" are different questions about the same
picture. A set that covers every branch can still show only one kind of
country.

So: **six — mapdesign's four branches, each carried by one map, plus two that
exist for range rather than for coverage.** The mapping is exact and nothing
of theirs is dropped:

| mapdesign's branch | carried by |
|---|---|
| **M1** island, one massif, one river to the sea | **Aldfell** (§4) — and it degrades to their exact smoke test by erasing seven rivers, which is itself a useful run |
| **M2** the home valley, re-expressed (continuity) | **The Vaelmere Sketch** (§3) — constrained, coordinated with mapdesign |
| **M3** coast with a bay, plus an inland lake with an outflow | **Thornsound** (§8) — the freshwater lake above the sound's head |
| **M4** endorheic, no ocean | **Seremarch** (§7) — three rivers into a dying salt lake |
| *(range)* the flat wet country | **Sedgewend** (§5) |
| *(range)* the country with no fresh water at all | **Farness** (§6) |

The two extras are not decoration. Sedgewend and Farness are the two maps in
the set with **no drawn river at all** — their whole terrain comes from the
painted shape and the sea — which makes them the purest possible test of the
raster mask, and it raises a format question mapdesign should answer before
the reader is written (§11, question 1).

Seven was considered and cut: a delta. Rivers in this pipeline merge and never
fork, so a delta is a known gap, recorded in §10 and not written up as if it
were available.

---

## 2. What each map states, and four additions to the brief

Each map below gives: **identity** in one line a player would repeat;
**what is painted** (so it could be drawn from the prose); **what the drawing
does to the land**, traced explicitly, so the map teaches while it describes;
**rivers**; **landforms and biome**, named from LANDSCAPE §2.10 and §5;
**settlement with a "why here" for every place**; **what the player does**;
**the one image**; **scale**; and **how you would know it came out wrong**.

Four things the brief's minimum list did not ask for, adopted throughout,
each with its reason:

1. **What the sky does here.** The user's stated pleasure is walking and
   looking at weather. A map that does not say how weather arrives, how far
   off you see it coming, and what the light does to the ground is missing
   the thing he plays for. Sedgewend and Seremarch exist very largely for
   their skies, and no other field on the list would have caught that.
2. **Walking time, not kilometres.** A map is experienced in hours. At
   `WALK_SPEED` 1.8 m/s a kilometre is **9.3 minutes**, so the 10 km canvas
   is about **93 minutes** corner to corner at a stroll. Both numbers are
   given everywhere, because "9 km" and "an hour and a half" are different
   design objects and only the second is a player experience.
3. **How you leave.** A map is a cell in a bigger world and its edges are
   content: a pass, a firth, a road that keeps going.
4. **How you would know it came out wrong.** One sentence per map naming what
   must be false. Rule 30 moved into a map brief: a description nothing can
   fail is a description, not a specification. It costs a line, and it is the
   line design will actually use.

---

## 3. Map 1 — THE VAELMERE SKETCH *(continuity; mapdesign's M2)*

> **Identity:** home — a lake in a bowl of hills, one river out of it, and one
> black crag standing over the water with a grave in its side.

**Why it exists.** This is the world we have already built, drawn as a sketch,
and it is the control for the whole adoption. If the pipeline cannot
reproduce a world we can already walk, we should know exactly what it costs
before adopting it and not after. Everything in `ACT1_VALLEY.md` happens here
and nothing about it is invented in this document — the geography is
LANDSCAPE §7's and the fiction is BIBLE's. **Constrained, not free:
coordinate any change with mapdesign and design before it moves.**

**What is painted.** A **128 × 128 px** patch — our whole 2 × 2 km world is
the corner of one canvas, which is the plainest possible statement of how
much bigger this structure is than what we have. Green everywhere; **grey**
for Ravenscar, filling its authored footprint; **blue** for the Mere, a lake
that does not touch the border and therefore flood-fills as fresh; **red**,
2 px, for the Vael — in at the head of the valley, through the Mere, out at
the frame edge; **dark green** for the SE oak mass that swallows Harrowmere
Hall; **yellow** for Vaelmere, Harrowward, the shrine knoll, the Hiding, the
Hall, the Backbarrow and the ward-tower.

**What the drawing does to the land.** The Vael sets the low line, and the
ground rises away from it to the valley shoulders. The Mere is a **flat
reach** of the river at the height the descent gives it (LANDSCAPE §3.1.4) —
which is what makes it legal, and the fact worth checking on the first run.
Ravenscar's height is the grey stamp's, not the distance field's, so it keeps
the angular banded four-ribbed character §2.8 already rules for it rather
than being a swelling in a gradient.

**Landforms declared:** LF-1 rolling plain, LF-2 ridge-and-swale (the valley
shoulders), LF-3 river valley with terraces, LF-4 scree apron (Ravenscar's
foot), LF-5 crest and outcrop, LF-7 forest floor, LF-8 erosion.

**Biome.** Ours as built: Dale Oak along the water and through the SE mass,
Highland Pine on the shoulders, River Birch on the bank lines, scree and bare
rock on the crag.

**Settlement — the why-here that already exists:**

| Place | Why here |
|---|---|
| **Vaelmere** | at the Mere's **outlet** — the one point where you can net still water and take moving water in the same morning; the last calm before the current. |
| **Harrowward** | at the **ford**, under the crag. The house owns the crossing, and its gate faces the crossing it owns (BIBLE §5.1). |
| **The shrine knoll** | the first dry rise above the flood line, in sight of both hamlet and water: a place for the rites that is never underwater. |
| **The Backbarrow / the Hall / the Hiding / the ward-tower** | canon (BIBLE §2). Each is a place the terrain hides or reveals, and the act-1 climb walks up that order: water, forest, grave, crag. |

**What the player does.** Act 1 as written. Worth noting what the sketch
format makes visible that prose never did: the province is legible as **one
line** — everything is either up the river or down it — and a player who
simply follows the water uphill because it is pretty is walking the main
quest without being told to.

**What the sky does.** The crag catches cloud the valley floor never gets;
the Mere is the weather instrument, and flat grey on it means it is already
raining upstream.

**How you leave.** Down the Vael, out of the frame. Up over the shoulder,
toward the temple mountain — act 2, and off this canvas.

**Scale.** 128 × 128 px = 2 × 2 km. Corner to corner ≈ **19 minutes**. On a
full 640 canvas this is one sixteenth of the area, which is the honest
picture of how much world is not yet drawn.

**How you would know it came out wrong.** If the Mere's surface stands higher
than the river leaving it (the 7.98 m defect, LANDSCAPE §3.1) — or if
Ravenscar comes out of the grey stamp as a dome, which is the complaint the
project has already spent a stage on.

---

## 4. Map 2 — ALDFELL, the Wheel of Waters *(mapdesign's M1, extended)*

> **Identity:** one island, one mountain, eight valleys — and you say where
> you are from by naming your river.

**What is painted.** Blue to the border on all sides. A single rough-edged
island filling most of the canvas, ~560 px (9 km) across, with a **grey**
massif of about 190 px (3 km) at its centre. **Eight red rivers**, 2 px, from
the grey out to the blue like spokes. Dark green in the valleys between them
if you want to force it; leave it green and the moisture field will put the
forest there anyway. Yellow at each river mouth, one at each valley's head of
cultivation, and one on the summit.

**What the drawing does to the land.** The eight rivers cut the massif's skirt
into eight valleys, and the ground between each pair of rivers rises into a
**radial ridge** — for free, because the ridge is simply the place furthest
from either river. The elevation ladder is complete on one walk: strand,
meadow, oak, upper conifer, heath, scree, rock, in about 4 km horizontally,
which is **37 minutes uphill**.

**Rivers: eight, radial, and they never meet.** This is the only map in the
set whose drainage is a **wheel** rather than a tree, and it does the
political work by itself: eight valleys, identical climate, no shared water,
and no reason on earth to be friends.

**The degradation that is worth running.** Erase seven of the eight rivers and
Aldfell becomes exactly mapdesign's M1 smoke test — one landmass, one massif,
one mouth. The pair is more useful than either alone: the same island with
one river and with eight is a clean before/after on what river count alone
does to a country, run on identical geometry.

**Landforms declared:** LF-1 (the strand meadows), LF-2 (the radial ridges),
LF-4 scree apron at the massif foot, LF-5 crest and outcrop, LF-6 coastal
cliffs on the two windward lobes — **grey painted to the water's edge, which
mapdesign confirms is free** — LF-7 forest floor, LF-8 erosion.

**Biome.** The full ladder by height, crossed with a moisture stripe: the
valleys are river-adjacent and wooded (Dale Oak low, Highland Pine above),
the radial ridges between them are dry and open. So the island's forest is a
set of **stripes radiating from the mountain**, and a walk around the island
at mid-height goes woods, open, woods, open, eight times. That rhythm is the
map's signature and nobody places a tree to get it.

**Settlement, with why-here:**

| Place | Why here |
|---|---|
| **Eight river-mouth towns** | a mouth is where the boat meets the river — the only point at which sea trade and inland produce can change hands. The island's entire politics is eight of these and nothing else. |
| **Eight head-of-cultivation hamlets** | one per valley, at the last flat ground before the river turns steep. Above it grazing, below it fields. The gradient puts the line in a different place in each valley, and the valleys are proud of the difference. |
| **The beacon on the summit** | the only object all eight can see. It is the one reason they ever have to speak to each other, and whoever lights it has addressed the whole island at once — a good empty hook for canon to fill later. |

**What the player does.** This is the climbing map and the **reveal** map:
every ridge crossing drops you into a complete world with the same weather
and different people, eight times over. And it is the only map here where you
can stand somewhere and see the whole of your own journey — from the summit,
all eight valleys and sea on every side.

**What the sky does.** An island this shape makes its own weather: cloud piles
on the massif and the summit is in it more often than not, so the mountain
reports the day to you from every valley.

**The one image.** From the strand at a river mouth, looking upriver: the
valley opening away and the whole massif standing at the end of it, the river
a bright line running out of its foot toward your boots.

**How you leave.** By sea, from any of the eight. The island is a closed
system on purpose — that is what makes the eight valleys matter.

**Scale.** 9 km island on the 10 km canvas. Mouth to summit ≈ 4 km ≈
**37 minutes** up; right round the coast ≈ 29 km, most of a day, and worth it
once.

**How you would know it came out wrong.** If any two of the eight rivers meet
before the sea, the spokes are not radial and the massif is off-centre; if
the summit reads as a dome from the strand frame, §2.8's anti-dome rules have
failed on the grey stamp.

---

## 5. Map 3 — SEDGEWEND, the Hundred Arms *(range: the flat wet country)*

> **Identity:** a country with no hill in it — the sea comes in a hundred
> fingers, and wherever you stand there is water on two sides of you.

**What is painted.** A comb. Blue reaches in from one border in long parallel
inlets, 125–190 px deep (2–3 km) and 12–30 px wide (200–500 m), with ribs of
green 37–75 px wide (600–1200 m) between them, all running roughly the same
way and shortening toward the landward side. **No red at all** — see below.
No grey, anywhere, on purpose. Yellow at the head of five inlets and at one
neck.

**What the drawing does to the land.** This is the map that proves the rule in
the negative. **No point on it is more than about 600 m from water**, so no
point on it can be high: the crown of the widest rib reaches perhaps 40–60 m
and most of the map is under 25. There is no mountain here and there
**cannot** be one, and that is not a limitation worked around — it is the
map's entire identity.

**Rivers: none drawn, and that is correct.** A rib's spine drains both ways
into the inlets beside it, so the map has dozens of streams and no river at
all — nothing is long enough or high enough to gather one. Every stream here
is 3–5 m wide, which mapdesign's scale puts **below one pixel**, so they are
generated, never drawn. **Consequence for play:** there is no ford worth the
name and no bridge worth building on the main inlets. Every crossing is a
ferry, or a two-hour walk to the head of the inlet and back down the other
side. That one fact generates the entire society.

**Landforms declared:** LF-1 rolling plain (the rib crowns), LF-7 forest floor
(the alder and willow carr in the hollows), LF-8 erosion — and **LF-9 salt
marsh, proposed** (§9), because nothing in today's dictionary describes the
intertidal sedge that is most of this map's ground.

**Biome.** Wet everywhere: nothing is far from fresh water and the sea is
always adjacent. Reed and sedge at the water, wet meadow above it, alder and
willow in the hollows, and every tree low and wind-flagged because there is
no shelter anywhere. The one dry thing on the map is the crest of each rib,
which carries a thin heath — **and that is where every road goes**, so the
roads read from a distance as pale lines on green.

**Settlement, with why-here:**

| Place | Why here |
|---|---|
| **Every inlet head** (five of them are towns) | where the tide stops and the water turns fresh: the last place a sea boat can reach, the first place you can drink, and the only place the two shores are joined by land. Three reasons stacked on one point. |
| **Crestway villages** | where a rib road must come down off its dry crest to a crossing. A village exists at each descent because that is where travellers stop, not because anyone chose the spot. |
| **Thwaitneck** (the capital) | at the **portage** — the one place where two inlets come within 250 m of each other, so a boat is hauled a quarter-kilometre across land instead of sailing thirty around. Everything worth money on this map crosses that neck, and the town charges for it. |

**Why this settlement pattern is worth having:** the rule repeats. After the
second inlet head the player has learned it, and from then on he can look at
an unexplored arm of water and **predict** the town at the end of it. A map
whose logic can be learned and then used is a map that respects the player,
and this is the only one in the set whose logic is that simple.

**What the player does.** Walking here is bearing-keeping and patience. The
rib roads run straight, the view is symmetric — water left, water right, the
far shore 300 m away and always out of reach — and every few kilometres the
same decision: walk to the head (forty minutes) or wait for a ferry
(unknown).

**What the sky does — and this is why the map exists.** You can see weather
coming for **ten minutes before it arrives**. Squalls come off the sea and
walk up the inlets one at a time, so you routinely stand in sunlight watching
rain fall on the next rib over. Nothing else in the set can do that, because
nothing else in the set is flat enough.

**The one image.** From the crest road at low sun: three parallel inlets
stepping away into haze, each a different shade of grey-green, and one rain
squall standing over the middle one only, with its own light beneath it.

**How you leave.** Landward, where the inlets shorten and close and the
country lifts into ordinary hills. Seaward, by boat.

**Scale.** The full 10 × 10 km canvas. Along the ribs ≈ **93 minutes**;
across the grain is the joke of the map and takes half a day.

**How you would know it came out wrong.** If there is a hill on it, or if any
watercourse on it exceeds about 2 m wide, the mask is not being read — a comb
cannot produce either.

**What it would want.** Tides (§10, G3). Sedgewend is good without them and
would be the best map in the set with them: an intertidal band you can walk
at low water and cannot at high turns every crossing decision into a clock.

---

## 6. Map 4 — FARNESS, the Long Walk *(range: no fresh water at all)*

> **Identity:** one road, nine kilometres long, with the sea on both hands,
> going nowhere except the end of the land.

**What is painted.** Blue to the border. One thick root of green at the
landward edge, and a single arm reaching out of it: **560 px** (9 km) long,
30–95 px (500–1500 m) wide, pinched to **necks** of 12–22 px (200–350 m) in
three places, ending in a blunt head. **No red. No grey. No dark green.**
Four yellow blobs. That is the entire painting — and it is the reason this is
the map I would build first (§11).

**What the drawing does to the land.** Width sets the ceiling, so the
elevation is decided the moment the arm is drawn: the root reaches ~180 m,
the first segment ~110, the second ~70, the third ~35, and the head is barely
above the water. **Elevation descends as you walk out**, and it does so
because of the shape and nothing else. That has a lovely consequence for
play: you always know how far out you are by how low you are, without a map,
a compass or a marker.

**Rivers: none, and the arm has no fresh water.** Nothing rises far enough to
gather one. That is correct rather than impoverished — it is a real
constraint real people live under. Springs at the necks where the land
pinches, cisterns everywhere else, and one well that the third neck's town
exists because of.

**Landforms declared:** LF-1 rolling plain, LF-2 ridge-and-swale on the root,
LF-5 outcrop at the necks, LF-6 coastal cliffs along the weather side of the
arm, LF-8 erosion — and **LF-10 maritime heath, proposed** (§9).

**Biome.** Grass, heath, gorse, thrift; no trees at all except in the lee
hollows, and everything wind-flagged the same way, so the whole province
leans in one direction. That is the visual signature of the map. The moisture
field gets this right by accident and for the wrong reason — no rivers means
low moisture means open country — but the **name** will be wrong: our lookup
will call low-and-dry something like grassland or worse, and this is neither.
Hence LF-10.

**Settlement, with why-here:**

| Place | Why here |
|---|---|
| **Three neck towns** | the strongest why-here in the set: **at a neck the two shores are within 300 m, so there is a lee harbour in every wind.** A boat can always land out of the weather. Each neck therefore has two harbours and one street between them, and they get smaller going out — the first a port, the second a village, the third four houses and a well. |
| **Rootmarket** | where the arm's wool and fish meet the mainland's grain. The only place on the map with a market day. |
| **The head** | a light, and the ground behind it where the drowned are buried because there is nowhere else. Nothing else, deliberately. |

**What the player does.** The map is a corridor and that is the point. One
road, no navigation problem at all, which frees the player's whole attention
for **looking** — which is exactly the stated pleasure. What changes across
the walk is the ground under him: farmland at the root, grazing by the first
neck, rock and grass by the second, bare by the third. And the weather
crosses him **sideways** rather than arriving head-on, so he gets the
maritime thing of watching rain walk across the sea, hit him, pass, and be
gone in ten minutes, three or four times an hour. A player who does not enjoy
walking will not enjoy Farness. A player who does will remember it first.

**The one image.** From the last high ground at the root, looking out: the arm
narrowing away through four depths of haze, the road along its spine catching
the light, sea on both sides at once, and the third neck just visible as the
place where the land almost stops.

**How you leave.** Landward through the root. Seaward there is nothing — the
head is the end, and that is the map's point.

**Scale.** 9 km of arm plus 2 km of root. Root to head ≈ **84 minutes** one
way. You feel the walk back, and you should.

**How you would know it came out wrong.** If the head is higher than the root,
or if any watercourse appears on the arm. Both are impossible under a correct
distance-from-drainage field on this mask, which is exactly why they are the
test.

---

## 7. Map 5 — SEREMARCH, the Long Thirst *(mapdesign's M4: endorheic)*

> **Identity:** three rivers that never reach the sea, running down into a
> salt lake that is smaller every year, with the marks of where it used to be
> written on the hills above it.

**What is painted.** **No blue touches the border anywhere** — this map has no
ocean at all, which is the branch nothing else in the set exercises and the
one Amit's model cannot do. In the middle-low third of the canvas, a blue
**terminal lake** about 125 px (2 km) across. Three **red** rivers, 2 px,
running into it from three sides, their sources 150–190 px (2.5–3 km) apart
at the canvas edges. **Grey** along two of the borders as the high rim.
Everything else green, and left green: no dark green anywhere on this map.
Yellow along the rivers, two out on the crossings, one at the lake.

**What the drawing does to the land.** This map is `RIVER SPACING IS
ELEVATION` stated as loudly as it can be stated. The three rivers are far
apart, so the divides between them are **high** — and because moisture comes
from the same water, the divides are also **dry**. One decision about pen
spacing produces the height, the barrenness and the danger of the crossings
all at once. The land tilts inward from the rim to the lake, and everything
drains to a place with no outlet, which is why the water is salt.

**Rivers: three, and that is the whole design.** River count is a knob, and
this map exists to show what turning it down does. Each carries the only
fresh water for a day's walk in either direction. All three descend to the
terminal lake, so the invariant that water never gains height downstream is
satisfied by construction — but it is the invariant to watch on this map,
because three sources feeding one sink is the arrangement most likely to
expose a bug in the monotone pass.

**Landforms declared:** LF-1 rolling plain, LF-2 ridge-and-swale on the
divides, LF-3 river valley with terraces (the three corridors), LF-5 crest and
outcrop, LF-8 erosion — and **LF-11 relict shoreline, proposed** (§9), which
is the map's whole soul.

**Biome.** The divides are dry: bare stone, dust, thin scrub, the occasional
thorn. The river corridors are a strip of green 300–400 m wide with real trees
in them — Dale Oak on the lower reaches, River Birch on the banks. From above
the map is a stone-coloured field with three green threads laid across it and
a white rim around the lake. From the ground it is the most **legible** map in
the set: the colour of the land tells you exactly how far you are from water.

**The relict shorelines, which are the reason to build this map.** The lake
has been shrinking for a long time, and every level it paused at left a
terrace on the hills above it. So the slopes around the lake are **stepped**,
and each step is an old waterline with the old harbours stranded on it: a
stone quay four hundred metres from any water, a boat-shed with a floor of
dry cracked mud, a village on the third terrace that was a port in somebody's
great-grandfather's time and is now a day's walk from the shore. The player
can read the history of the whole province by counting steps down the
hillside, and nobody has to tell him a word of it. This is the single best
thing in this document and it comes entirely out of geography.

**Settlement, with why-here:**

| Place | Why here |
|---|---|
| **Everything on the rivers, in a line** | there is no such thing as an inland village here, and that absence is the loudest thing on the map. |
| **Serewell** and **Middlethirst** | on the two dry routes **between** rivers, each at the halfway point, each existing solely because there is one hole in the ground there. A place that exists because of a single well is a place with a built-in ending, and everyone living there knows it. |
| **Saltstair** | on the lake's old shore, and it has moved **downhill three times**. The town is on the third terrace; the second terrace below it is empty stone houses nobody bothered to pull down; the first is under salt crust. The living town is the top step of a staircase of its own corpses. |

**What the player does.** This is the map about **planning**, and its pleasure
is unlike anything else here. The crossing between two rivers takes most of a
day; you carry water for it; the payoff is cresting the last rise and seeing
the green line below with trees in it — a reveal that only works because of
the hours spent without it. Occlude-and-reveal (LANDSCAPE §1.4) usually
operates over a few hundred metres; here it operates over three kilometres,
and it is the same trick played very slowly.

**What the sky does.** Visibility is enormous — you can watch a storm at 20 km
and watch it not come to you, which is a genuinely different relationship
with weather than any other map on this list. Nothing blocks anything, and
light on stone at either end of the day is the whole palette.

**The one image.** From the dry divide at evening: the green line of the far
river laid across the entire width of the view with the sun on the water in
three places, the white rim of the lake beyond it, and nothing else in the
frame but stone.

**How you leave.** Up any river to its source and over the grey rim, into a
country that drains somewhere else.

**Scale.** The full 10 × 10 km canvas. River to river ≈ 3 km ≈ **28 minutes**
on the direct line and rather more in practice, because there is no direct
line.

**How you would know it came out wrong.** If a tree appears anywhere off the
river corridors, moisture is not keyed to fresh water. If any of the three
rivers runs uphill into the lake, the monotone pass has failed on the case
most likely to break it. If the lake has an outlet, it is not endorheic and
the branch has not been tested.

**What it would want.** River incision (§10, G2) and rain shadow (§10, G4) —
both improvements, neither blocking.

**FMG check — the map is producible, and it gained a constraint nobody had.**
Design asked for ten minutes in the browser on the one thing they could not
settle from source: can FMG make a map with **no ocean at all**? Answered from
FMG's own documentation instead, which is stronger than a single browser run
because it names the mechanism rather than one seed's outcome.

- **Land at every border is authorable.** FMG's `Mask` operation is what
  creates border ocean — it grades the edge cells down toward 0 — and
  **a negative fraction inverts it, lowering the map CENTRE instead.** That is
  an inland basin stated as a template operation: high rim, low middle, land
  all the way to the frame. The heightmap brush and the image converter reach
  the same result by hand.
- **The terminal salt lake is native.** FMG classifies a lake as fresh, salt or
  **dry** from river flux against evaporation, so a basin with inflow and no
  outlet becomes salt by its own hydrology — we do not draw it.
- **But the salt is CLIMATE-DERIVED, not authored, and that is the new
  constraint.** Seremarch must be sited hot and dry enough that evaporation
  beats inflow. Put this province somewhere cool and wet and FMG hands back a
  large *fresh* lake with an outlet — at which point the shrinking, the salt,
  the relict shorelines and the three stranded harbours of Saltstair are all
  gone, and the map is just a lake district. **Latitude is now a load-bearing
  authoring decision on this map**, which it is on no other map in the set.
- **A gift for LF-11.** FMG's `dry lake` type is a **real** relict basin the
  tool produces on its own — which supplies the real rejected instance design
  flagged LF-11 as lacking (§9), instead of the synthetic smooth slope.

The residual risk is real but it is a **different** risk from the one design
named: not "can the tool draw it", but "do FMG's downstream modules assume an
ocean exists" — ports and harbours, the Marine biome, and the temperature and
precipitation passes that key off distance to water. That is what the first
export should be inspected for, and it is an importer question rather than a
map question.

---

## 8. Map 6 — THORNSOUND, the Two Shores *(mapdesign's M3: sea, bay, lake with outflow)*

> **Identity:** one arm of the sea cut eight kilometres into the mountains,
> with a freshwater lake standing above its head — and everything anyone does
> here is either crossing it or going along it.

**What is painted.** Blue to the border, entering as a **sound**: 500 px (8 km)
long, 25–62 px (400–1000 m) wide, driven inland between two thick lobes of
green. Above its head, a **second blue body that does not touch the border** —
a lake, and the pipeline knows it is fresh because of that alone — with one
**red** river 2 px wide running out of it down into the sound, and two more
red rivers coming down the flanks. **Grey** over both flanks, painted right to
the water's edge in two places. Yellow at each fan, at the ferry, at the
sound's head and at the lake.

**Why this map is mapdesign's flood-fill branch.** Two blue regions, one
touching the border and one not, and the pipeline must call the first salt and
the second fresh with nobody drawing the difference. If it gets that wrong,
the sound becomes a lake or the lake becomes a bay, and either failure is
visible in one frame.

**What the drawing does to the land.** The flanks are wide, so they are high:
the ground behind each shore rises to 400–500 m within 2 km. And where the
grey is painted to the water, the rise is a **wall** rather than a ramp —
mapdesign confirms sea cliffs are free under distance-from-drainage, which is
what makes this map the one it looks like in your head rather than an
approximation of it. LF-6's pale strata, its undercuts at the waterline, and
the pale spires at the cliff foot (never on the skyline, §2.9) all finally
have a coast to live on; this is the first map in the project that gives them
one.

**Rivers: three, all short and steep.** Two off the flanks into the sound, one
out of the lake down to the head, each building an **alluvial fan** where it
arrives. The fans are the only flat ground on either shore, and that single
fact places every town on the map.

**Landforms declared:** LF-2 ridge-and-swale on the upper flanks, LF-3 river
valley with terraces (the lake's outflow), LF-4 scree apron under the cliff
bands, LF-5 crest and outcrop, LF-6 **coastal cliffs — this map's centrepiece**,
LF-7 forest floor, LF-8 erosion.

**Biome.** Northern: Highland Pine from the shore to a treeline at ~350 m,
then heath, then rock; a narrow strip of wet green along the water itself
where moisture is highest; River Birch on the fans. Snow on the tops for part
of the year (LANDSCAPE §5.11).

**Settlement, with why-here:**

| Place | Why here |
|---|---|
| **A chain of fan-towns on both shores** | each on an alluvial fan at a river mouth — the only flat land, and the only route up out of the sound. |
| **The pairing, which is the map's engine** | each fan-town **faces** one on the other shore that it can see clearly and cannot reach in under two days by land. Your nearest neighbour is the one you wave at and never visit. That is a political engine, a rumour engine and a grief engine, and it is a free consequence of the geography. |
| **Thornferry** | at the narrowest point, 400 m. It is the single most important object in the province and everyone's plans route through it — which makes it the best possible thing for a story to take away. |
| **Soundhead** | where the salt ends and the river out of the lake begins: the market both shores must come to, because it is the only place they are joined by land. |
| **Cauldmere** (the lake) | a fishing hamlet on fresh water, forty minutes above a town on salt water, and the two do not intermarry. |

**What the player does.** A shore walk with the far shore always in view and
always unreachable, which makes every hour legible: you measure your progress
against the landmarks opposite, and you watch a place you passed yesterday
shrink behind you across the water. The sound is a continuous weather
instrument — mirror-flat at dawn, white and streaked in wind, full of mist to
the head for hours after a cold night.

**The one image.** From the shore road at first light: the far shore's
mountains standing upside down in still water, one sail crossing, and the head
of the sound eight kilometres up and lost in mist.

**How you leave.** Seaward out of the mouth; landward over either flank's
shoulder, a real climb, and the reason the ferry exists.

**Scale.** The full 10 × 10 km canvas; the sound 8 km long. One shore end to
end ≈ **75 minutes**. From a fan-town to the one opposite ≈ two days, and the
player should be allowed to find that out for himself.

**How you would know it came out wrong.** If the lake above the head comes out
salt, or the sound comes out fresh, the flood-fill is wrong. If you can cross
the sound on foot anywhere but the head, it is not deep enough. If the grey
painted to the waterline produces a ramp instead of a cliff, the elevation
model has quietly reverted to distance-from-coast.

---

## 9. Three landforms these maps need, proposed by name

LANDSCAPE §2.10 rule 5: a new entry needs a recipe, acceptance conditions, and
a map that wants it. mapdesign invited these explicitly. Recipes are stated as
requirements, never as implementations; design owns whether they land.

**LF-9 — SALT MARSH.** *Wants it:* Sedgewend, and any future coast with a
sheltered shore. *Recipe:* on near-flat ground within the influence of salt
water, a sedge-and-reed surface cut by a dendritic network of narrow tidal
creeks that are too small to be drawn and must be generated; bare mud in the
creek bottoms, low turf on the flats between; no trees at all, and a hard
inland edge where the ground lifts clear of the salt. *Acceptance:* the creek
network is dendritic and connected to the open water — every creek drains
somewhere; the marsh's inland boundary follows a contour, not a field edge
(§2.10 rule 2); no tree stands on it. *Control:* the same ground rendered as
ordinary wet meadow — flat green with grass on it, which is the thing every
generator ships by default and the thing this entry exists to stop being.

**LF-10 — MARITIME HEATH.** *Wants it:* Farness, and the tops of any exposed
coast. *Recipe:* low ground that is far from fresh water but soaked in sea
air: continuous dwarf-shrub cover (gorse, heather, thrift) with grass between,
no canopy anywhere, trees permitted **only** in lee hollows and always
wind-flagged, plus bare wind-scoured patches on the crowns. The key property
is that it is **dry by the moisture field and wet by the weather** — the
lookup will otherwise resolve it to grassland or desert, both wrong.
*Acceptance:* zero trees outside declared lee hollows; every tree that does
exist leans the same way as the shared wind field (§8.1 rule 6); shrub cover
is continuous rather than scattered. *Control:* the same cell rendered as the
existing dry-grassland treatment — an open green plain, which is what we would
get today and which reads as farmland, not coast.

**LF-11 — RELICT SHORELINE.** *Wants it:* Seremarch, and any shrinking or
managed water body later. *Recipe:* a shrinking lake leaves a **staircase**:
3–5 near-horizontal terraces above the current water level, each a former
stand, each with its own narrow beach-gravel band, salt crust, and the
vegetation appropriate to how long it has been dry — youngest step bare,
oldest step indistinguishable from the hillside. Terrace spacing is uneven,
because the lake did not fall at a constant rate. *Acceptance:* each terrace
reads as a **horizontal line** across the cross-lake frame (the same test
LF-3's terraces already pass); the steps are unevenly spaced; the youngest
step carries salt and no plants. *Control:* a single smooth slope from the
current shore to the hillside — the shape a lake with no history would leave,
and the one the frame must be able to reject.

---

## 10. Known gaps — what these maps want that the pipeline does not do

Written as requests, not described as free. mapdesign has already granted
what earlier drafts of this document asked for: hand-drawn boundaries,
archipelagos, coastless maps, lakes at altitude and sea cliffs are all in.
What remains:

| # | Gap | Wanted by | Blocking? |
|---|---|---|---|
| **G1** | **Distributaries** — a river that forks downstream, for a delta. Rivers here form trees and only merge. | nothing in this set; recorded so the gap is not rediscovered | no — the delta map was cut for this reason |
| **G2** | **River incision** — a carve deep enough to make a gorge (tens of metres, not §3.1.5's 1.5 m trapezoid). | Seremarch | no; it converts a good moment into the best one in the set |
| **G3** | **Tides** — an intertidal band, walkable at low water and not at high. | Sedgewend (would become the best map here), Thornsound | no |
| **G4** | **Rain shadow** — moisture driven by a prevailing wind over terrain, not only by distance from water. | Seremarch | no. Endorheic drainage already explains the salt honestly, which is why this dropped from "needed" to "wanted" |
| **G5** | **A drawn crest line** (a 7th colour). | nobody, yet | no — mapdesign asked us not to request this until a map genuinely needs it, and none of the six does: grey regions do the work |

G5 is recorded specifically as **not asked for**. Grey painted regions carry
every ridge in this document, and asking for a seventh colour before a map
needs one would be adding a knob nobody turned.

---

## 11. The set at a glance, what to build first, and what mapdesign still owes

| Map | Coast | Elevation | Rivers drawn | Biome |
|---|---|---|---|---|
| 1 **Vaelmere Sketch** | none (inland) | one valley, one grey crag | 1 through a lake | our temperate valley |
| 2 **Aldfell** | rough closed circle | one central massif, eight radial ridges | **8, radial, never meeting** | full ladder + forest stripes |
| 3 **Sedgewend** | a comb of deep inlets | **flat — nothing over 60 m** | **none** (all sub-pixel) | wet everywhere; marsh + carr |
| 4 **Farness** | one thin arm | **descends outward, by shape alone** | **none, and no fresh water** | maritime heath; no trees |
| 5 **Seremarch** | **none at all — endorheic** | high dry divides, tilting to a sink | 3, all inward to a salt lake | stone with three green threads |
| 6 **Thornsound** | one deep sound + an inland lake | **cliffs at the water; 500 m flanks** | 3, short and steep | northern conifer to treeline |

No two share a row, which was the requirement.

### Build FARNESS first — and ship the VAELMERE SKETCH beside it as its control.

Four reasons, in order of weight:

1. **It is the cleanest possible test of the thing the user actually asked
   for.** Farness paints no river, no grey, and no forest override. Its
   entire terrain comes from **one boundary and nothing else**, so if a
   hand-drawn mask works at all, Farness proves it — and if it does not, this
   is the cheapest place to find out, because there is only one variable in
   the picture.
2. **It arrives with its own control, built into the geography.** A thin arm
   between two seas **cannot** be high and **cannot** carry a river. So the
   map ships with two conditions a correct pipeline must satisfy and a broken
   one must visibly fail — a hill on the arm, or a watercourse on the arm.
   Rule 30 satisfied without inventing an instrument.
3. **Nothing in it is blocked.** Its only want is a biome name (LF-10). Every
   other map here waits on something, and Aldfell in particular waits on the
   anti-dome question, which is a reason to build it **second** rather than
   first: after the mask is proven, a bad summit is known to be the massif's
   fault and not the boundary's. One variable at a time.
4. **It is the map most purely about the stated pleasure.** One road, no
   navigation problem, sea on both hands, weather crossing sideways, and the
   ground changing under you the whole way out. If the user does not enjoy
   walking Farness, the walking-and-looking hypothesis is wrong, and we should
   learn that on the cheapest map rather than the most expensive one.

**And beside it, the Vaelmere Sketch**, because a first result with no
counterfactual is not a result: Farness answers "can it express a drawing?",
Vaelmere answers "does it lose what we already have?", and neither question
is worth much without the other. They are cheap together — Vaelmere is 128 px
square and every one of its facts already exists.

**Then Aldfell** (the first map worth living in), **then Seremarch** (the
branch mapdesign most wants, and the map with the best single idea in this
document), **then Thornsound**, **then Sedgewend**.

### Open with mapdesign

1. **Two of the six draw no river at all.** Sedgewend and Farness paint only a
   boundary; every watercourse on them is sub-pixel and therefore generated.
   Does the format accept an **empty rivers array**, and does the reader
   accept a map with **no red pixels**? If it does not, these are the two maps
   that catch it, and they catch it on the first run rather than the tenth.
2. **An endorheic map has no border-touching water at all**, so the
   sea-versus-lake flood-fill classifies **everything** as lake. Confirm that
   is legal and that `sea_level_m` is simply unused there — Seremarch's base
   level is its terminal lake's, which the descent decides.
3. **LF-9, LF-10 and LF-11** (§9) are proposed by name with recipe,
   acceptance and control, per §2.10 rule 5. Design owns whether they enter
   the dictionary; three of the six maps declare them.
4. **The JSON sidecars are not written here.** Each map's painted description
   above is complete enough to write one, but the field names are
   mapdesign's and their doc is the authority; the sidecars should be
   authored against the published schema rather than guessed at from a
   message, which is exactly the parallel-schema failure this coordination
   avoided in the first place.
