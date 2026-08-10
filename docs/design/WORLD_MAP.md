<!--
Created: 10:08:2026 - 21:58:31
Last updated: 10:08:2026 - 22:17:38
-->
<!--
UPD:
- 10:08:2026 - 21:58:31: Created. The drawn world map: Amit Patel polygon map generation adapted to a HAND-DRAWN sketch as the primary input (user request, verbatim in the doc). Rulings: the polygon map is a REGIONAL structure anchored in absolute world metres (POLYGON_SPACING 600 m, derived from LANDMARK_HAZE_ONSET, LF2_HILL_WAVELENGTH and the region POI band); Amit elevation = distance-from-coast REJECTED and replaced by elevation = distance from the DRAWN DRAINAGE, which is what makes a hand-drawn river reproducible; sketch format is an indexed PNG mask (6 colours, 16 m/px = one hydrology cell) plus a JSON sidecar, compiled offline by tools/sketch_compile.py, no silent fallback ever; five Rule 39 shadow copies named before they exist; N = 4 example maps derived as the count of topologies the reader must be proven on; acceptance is a three-instrument cross-sketch matrix with the eye holding the headline claim (Rule 41).
- 10:08:2026 - 21:59:49: WORLDGEN_SEA_LEVEL corrected on two checked premises (Rule 34, publishing trigger): it is a per-map DEFAULT overridable by the sidecar, because FOREST_BASE_ELEV is 20 m and a single global sea level would drown an existing stand; and its derivation is anchored on MASSIF_CLIFF_BAND_MAX, whose own NUMBERS row is marked pending re-derivation against a broken field — recorded at the anchor rather than left to be rediscovered (Rule 37's second half).
- 10:08:2026 - 22:08:45: AMENDED — the user moved to Azgaar's Fantasy Map Generator, so FMG becomes the AUTHORING TOOL and we consume its export (§9). Measured from source, not assumed: licence is MIT with an explicit commercial-derivatives expansion (blocker cleared); height is (h-18)**heightExponent metres, so our whole 0-400 m range is FMG h in [20,38] — NINETEEN steps, quantum 5 m at the coast and 41 m at the ceiling — and the ruling is that h is NEVER our elevation, only a land/water mask and a mountain region, which is the same ruling that rejected Amit's elevation model; cell spacing is sqrt(map_area / cellsDesired) and therefore SCALE-FREE, giving the configuration instruction 30x30 to 69x69 km at 10k cells, against a continental default of ~40 km cells at which our entire target world is 1/16 of ONE CELL. Crop chosen by him via a DFN_ORIGIN marker, missing or duplicated is a hard error. Three NEW Rule 39 shadow copies from FMG's settlement/route/biome layers, each ruled FMG-owns-the-graph / we-own-the-geometry. §2.1-2.5 and §2.8's sketch compiler SUPERSEDED as the authoring path and kept with a note, because their reasoning is the importer's specification — §9.6 re-targets every ruling line by line, including the two-tier split-by-cause rule, whose heir (an unmapped biome id is an ERROR, never a nearest match) is the most likely place for a silently-nearby world under FMG. §1, §2.6 and §5 survive untouched. WORLDGEN_SEA_LEVEL stays and does NOT become an FMG mapping constant (Rule 43).
- 10:08:2026 - 22:17:38: §9.8 revised on story's Seremarch findings, §9.9 and §9.10 added. CLOSED: FMG CAN hold land at every border — Mask with a NEGATIVE fraction inverts and lowers the map centre, so border ocean is a DEFAULT, not a constraint. Story's evidence argument accepted and recorded rather than waived: a mechanism settles the general case and one seed cannot, though the grade is documentation-derived, neither source-derived nor frame-verified. REPLACED BY THE SHARPER QUESTION: do FMG's downstream modules assume an ocean exists — ports, the Marine biome, and the temperature/precipitation passes that key off distance-to-water? The risk relocated from the map to the import. NEW FORMAT REQUIREMENT (§9.9): lake type is CLIMATE-DERIVED via Penman evaporation against river flux, so latitude is a load-bearing authoring input and any map whose fiction leans on a salt or dry lake must state it — generalised to the rule that FMG derives more than it draws, so the settings that produce a fact belong in the map's requirements. LF-11's synthetic-control flag LIFTED: FMG's native `dry lake` type is the real rejected instance Rule 45 asked for. §9.10: the water half of h is 19 hyperbolic steps to -950 m, ample and irrelevant for the same reason as the land half — h is not our elevation on either side.
-->

# WORLD_MAP.md — the drawn world map

**The user's request, verbatim, because the framing is the specification:**

> «для генерации карты надо использовать вот этот код [Amit Patel's polygon map
> generation]. у меня на ноуте в одной из игр он уже частично есть, какая-то
> версия. хочу чтобы дизайнеры истории предоставили мне примеры из N карт, **я
> вообще напрямую хочу карту нарисовать**, пока эту систему надо подкапот
> занести, чтобы **когда я принес эскиз карты, было просто воспроизвести**.
> дополнительно, может это обогатит генерацию земли»

**The headline is not "add a random island generator". It is: HE DRAWS A MAP AND
THE WORLD COMES OUT OF IT.** Random generation is the fallback and the test
harness. The sketch-to-world path is the deliverable, and every ruling below is
made in its favour when the two conflict.

Source: Amit Patel, *Polygon Map Generation for Games*
(http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/,
mirror https://xenon.stanford.edu/~amitp/...). We take his **graph**, his
**island-shape hook**, his **moisture-from-fresh-water**, his **two-axis biome
lookup** and his **flow accumulation**. We reject his **elevation model** and
we do not implement his **noisy edges** as a second mechanism. Reasons below,
each one at the stage it applies to.

> ### ⚠ AMENDED 10.08.2026 — THE AUTHORING TOOL IS NOW AZGAAR'S FMG. READ §9 FIRST.
> The user moved to https://azgaar.github.io/Fantasy-Map-Generator/. **§2.1–§2.5 and
> §2.8's `sketch_compile.py` are SUPERSEDED as the authoring path** and re-targeted to
> an importer; they are kept because their reasoning IS the importer's specification
> (§9.6 maps every ruling across). **§1 (where it sits), §1.6 (the shadow copies),
> §2.6 (a drawn river generates elevation) and §5 (the whole acceptance) survive
> untouched** — §2.6 in particular is the answer whichever tool produced the polyline.

---

## 0. What Amit's pipeline can and cannot be steered by a drawing

This table is the whole design in miniature. Every stage is marked with whether
a drawing can steer it, and where it cannot, what replaces it.

| Amit's stage | Steerable by a drawing? | Our ruling |
|---|---|---|
| Random points → Lloyd ×2 | **No, and it must not be** | The polygon lattice is an implementation detail. The drawing steers CLASSES, never the mesh. This becomes an acceptance (§5.4): the same sketch at three point-seeds must give the same verdict. |
| Voronoi/Delaunay dual (Center/Corner/Edge) | n/a — structure | Taken whole. New structure; nothing in our tree duplicates it. |
| **Island shape function** | **YES — this is the hook the request hangs on** | Amit says it may be *"a user-drawn boundary"*. Replaced entirely by the sketch reader. Land/water assigned to **corners**, polygons classified by their corners' water fraction, flood-fill from the image border separates ocean from lake. Taken whole. |
| Elevation = distance from coast | **No — and it is the wrong model for us** | **REJECTED.** Amit is candid that it suits volcanic islands and not continents; and it would overwrite the massif, LF-2 and LF-8 we have already built and accepted. Replaced by **elevation = distance from the DRAWN DRAINAGE** (§2.6). |
| Redistribution `y = 1 − (1−x)²` | n/a — a curve | **Kept, applied to a different distance field.** Steep near the water, flattening at the divide, is a valley cross-profile as much as it is a coastal one. |
| Rivers: steepest descent on the corner graph | **Partly** | The drawn line *is* the river. Amit's descent is not run at regional scale; §3.1's existing grid trace still produces the walking-scale polyline, seeded by the drawn line instead of by an argmax (§1.4). |
| Flow accumulation, width ∝ √flow | **No — derived, and that is correct** | Taken whole. Width comes from accumulated drainage, clamped into our authored width bands. A width the sidecar declares but the drainage cannot support is a **loud rejection**, not a silent clamp (§2.7). |
| Moisture = inverse distance to fresh water | **Indirectly** | Taken. The user steers it by where he draws water. FOREST paint is an override on the consequence, not on the field. |
| Biomes from a Whittaker table | **Only through the declared list** | Mechanism taken (2D lookup), his ~20 names rejected. Cells resolve to **our landform dictionary (§2.10 of LANDSCAPE.md)**, and §2.10 rule 4 — composition is *declared*, not emergent — is preserved by making the map declare which entries it admits. An entry the map did not declare is a hard error (§2.4). |
| Noisy edges (quad subdivision) | No | **Not implemented as a second mechanism.** LANDSCAPE §2.10 rule 2 already requires domain-warped landform boundaries. Amit's subdivision is one implementation of a requirement we already have one implementation of; shipping both is Rule 39 by construction. |

---

## 1. WHERE THIS SITS IN OUR STACK

### 1.1 It is a REGIONAL structure, and it is larger than the world we have

Amit targets ~1000 polygons over a whole island. Our world today is 2×2 km
(`WORLD_EXTENT_CHUNKS` 8 × `CHUNK_SIZE` 256). Those two facts do not describe
the same object, and pretending they do is the first way this feature could go
wrong.

**Ruling: the polygon map covers the WHOLE WORLD AT ITS TARGET SIZE, and today's
2×2 km is a crop of it.** It is not a separate "region layer" wrapped around the
current world, and it is not sized to the current world.

The justification is already in the tree and is not a matter of taste:

- `WORLD_EXTENT_CHUNKS`'s own NUMBERS row states the target is **10×10 km**, that
  the LOD ladder is already computed for it, and that node numbering is bound to
  an **immobile world grid rooted at world zero, so growing the world renumbers
  nothing** (confirmed in `CoarseTerrain.h`: `coarse_node_key()` is on a fixed
  world grid).
- LANDSCAPE §1.3a rules that at 10×10 km the world becomes **multi-region: one
  LR per ≈ 4×4 km region cell**. That is a regional structure with no
  implementation. This is it.

**Consequence that must be stated once and obeyed everywhere: the sketch is
anchored in ABSOLUTE WORLD METRES, never normalised to the current extent.** The
sidecar carries `origin_x_m` / `origin_z_m` for pixel (0,0) and a metres-per-pixel
scale; the engine crops to `WORLD_EXTENT_CHUNKS`. If the sketch were normalised
to `[0,1]²` of the current extent, then raising `WORLD_EXTENT_CHUNKS` from 8 to
40 would silently move every polygon, every coastline and every drawn river — a
textbook Rule 37 latent defect, created by a registry edit nowhere near this
code. Absolute anchoring makes growing the world a **reveal**, not a rescale:
the same sketch, more of it resident.

### 1.2 What one polygon is in metres — derived, not chosen

Rule 33 says detail is sized against the viewing distance. A polygon is not
detail on an object, it is a **place**, so the question is: how big is a piece of
land a player perceives as one place? Three independent bounds, all from
constants already in NUMBERS.md, and the answer must satisfy all three.

**Upper bound — `LANDMARK_HAZE_ONSET` = 800 m.** That constant is the distance at
which our own doctrine says a thing stops reading as *here* and starts reading as
*a far goal* (§1.3a's depth separation). A polygon whose far rim is beyond the
haze onset from its near rim is not one place, it is two. For a hexagonal cell of
centre spacing *s*, the widest chord is `2s/√3 = 1.155 s`, so **s ≤ 800 / 1.155 =
693 m**.

**Lower bound — LF-2's own wavelength, `LF2_HILL_WAVELENGTH` = 100 m.** A polygon
that is assigned the landform "ridge-and-swale hills" must be big enough to
*contain* ridge-and-swale; one ridge is not a rhythm. Three cross-axis
wavelengths is the minimum at which a pattern reads as a pattern rather than as
an incident, so **s ≥ 300 m**.

**Sanity band — the region POI spacing, 540–900 m** (from `POI_TRAVEL_TIME`, per
§1.3a). This is not a third derivation, it is a *check*, and it carries the
honest one-sentence definition of what a polygon is: **a polygon is the catchment
of one POI.** If polygons were much smaller than POI spacing they would be
substructure with no name; much larger and a polygon would hold several places.

**`POLYGON_SPACING` = 600 m.** Inside all three, with margin at both ends
(1.16× under the haze ceiling, 2× over the LF-2 floor, mid-band on POI spacing).

| At `POLYGON_SPACING` = 600 m | |
|---|---|
| Mean cell area, `(√3/2)s²` | 0.312 km² |
| Polygons over today's 2×2 km | **≈ 13** |
| Polygons over the 10×10 km target | **≈ 321** |
| The island size at which we reach Amit's ~1000 | ≈ 17.7 × 17.7 km |

**The 13 is the number to look at, and it is the argument, not an embarrassment.**
Thirteen polygons over the current world says plainly that this structure has
almost nothing to do at 2 km, that its payoff arrives with the 10 km world, and
that anyone who evaluates it on today's map is evaluating one polygon at a time.
Building it now is right because it is the thing the 10 km world is *made of* —
but its acceptance frames are overhead frames at map scale (§5), not valley
frames, and no one should expect the valley to look different on the day it
lands.

### 1.3 What it REPLACES

| Replaced | Where it lives today | By what |
|---|---|---|
| **`ForestRegions`** — `oak_rects[2]`, `pine_annulus_r0/r1`, `pine_strip_count/duty`, `forced_clearing_center` | `engine/world/sources/TestbedLayout.h:274` | The moisture × elevation lookup. These are hand-typed rectangles and annuli standing in for a climate field; they are the single most obviously provisional thing in the generator, and they are exactly what Amit's chapter exists to remove. |
| **"forest masses from a coarse moisture-like noise field"** | LANDSCAPE §2.1 | A real moisture field. See the Rule 44 warning in §4.2 — `FOREST_COVERAGE` must be **re-derived**, not carried across. |
| **River source selection** — coarse argmax near `layout.river.source` | `WorldgenHydrology.cpp:339` | The drawn river's source pixel. §3.1 step 1 only. |
| **`StandId { Testbed, Forest }`** as the map identity | `TestbedLayout.h:72`, branched in `macro_height()` and `compose_passes()` | A compiled map file. A stand becomes a map that ships with the game rather than an enumerator in a header; the enum survives only as long as the two existing stands do. |
| **`testbed_layout.json`** as the only authored map | `assets/world/` | Superseded in role, not deleted. Its loader is the precedent this format copies (unknown keys are a hard error). |

### 1.4 What it FEEDS

- **`macro_height()`** gains a regional base term (§2.6). It does **not** gain a
  fourth open-coded copy of the pass stack — see §1.6.
- **Hydrology** gains its source, its mouth and its flow accumulation; the
  `trace_descent` / Chaikin / resample / monotone / carve / ford chain is
  untouched.
- **`classify_surface()`** gains nothing directly. Biome does not become a sixth
  `SurfaceClass`; it selects *which* landform recipe and *which* flora tables run
  in a polygon, and the five splat classes stay a material vocabulary.
- **`build_scatter()`** gains moisture and elevation zone as inputs to species
  selection, replacing the region rectangles.
- **Site placement (P4)** gains a geography to be scored against: real
  confluences, real bends, a real coast.

### 1.5 What it LEAVES ALONE — stated so nobody widens the blast radius

The massif's shape (§2.8, the anti-dome ruling and its `MASSIF_LOBE_RATIO` test);
LF-2's grive construction and its CDF equalisation; LF-8's droplet erosion; the
§3.1 river carve, trapezoid section and fords; the path network; the flora
catalog's species geometry; every acceptance frame taken at eye height. **The
polygon map decides WHERE things are, at 600 m granularity. It decides nothing
about what they look like from twenty metres away.** Amit's elevation model is
rejected precisely because it would have violated this line.

### 1.6 Rule 39 — the shadow copies this feature would create, named before they exist

This project unified three copies of the height pipeline into `compose_passes()`
one day before this document was written, at a measured cost of −1.50…+1.50 m of
drift that every suite stayed green through. The following are the copies **this
feature would create by default**, each with its ruling.

1. **Two river generators.** The polygon graph has a river algorithm; §3.1 has a
   river algorithm. *Ruling: there is one river tracer,
   `WorldgenHydrology::trace_descent`.* The polygon layer contributes three
   values — source cell, mouth cell, accumulated flow — and produces no polyline
   of its own. If a regional polyline is ever wanted for the map screen, it is
   read back from `HydrologyData::stations`, never traced again.

2. **Two distance-to-fresh-water fields.** `WaterSample::dist_to_water` (per
   sample, via `water_at`) and `HydrologyData::coarse_dist[]` (16 m grid) exist;
   Amit's moisture is a third. Rule 35 fires — the second consumer has appeared.
   *Ruling: moisture is DERIVED from the existing field, aggregated per polygon.
   There is one distance transform. Its saturation range is a parameter of the
   CALL, not a property of the function* — §3.3's `DIST_TO_WATER_RANGE` (≥150 m
   for settlement scoring) and moisture's multi-kilometre range are two calls,
   not two fields. A comment saying "this is the same distance" is not a
   mechanism that makes it the same distance; calling one function is.

3. **Two land/water authorities.** The sketch says water; the heightfield says
   "below sea level". These *will* disagree — LF-8 erosion cuts, the massif
   stamps, LF-2 rolls. *Ruling: the sketch is the authority for ocean and lake
   EXTENT. The height construction is clamped to agree at the point where the
   drainage floor is established (§2.6), before any relief is added, and the
   residual disagreement after relief is a MEASURED acceptance (§5.1), not an
   assumption.* Naming it as measured rather than asserted is the whole
   difference between this being checkable and being a comment.

4. **Two boundary-softening mechanisms.** Amit's noisy edges and our domain-warped
   landform boundaries (§2.10 rule 2). *Ruling: one mechanism, ours. Amit's
   subdivision is not implemented.*

5. **Two "which landform is here" decisions.** The Whittaker lookup and the
   declared composition list. *Ruling: the lookup SELECTS AMONG DECLARED ENTRIES.
   A cell resolving to an undeclared entry is a hard error at compile time
   (§2.4), which preserves §2.10 rule 4 verbatim and turns "composition is
   declared" from a doctrine into a check.*

---

## 2. THE SKETCH FORMAT

> **§2.1–§2.5 and §2.8 are SUPERSEDED as the authoring path (§9). They are kept
> because the importer's requirements are derived from them, one for one, in §9.6.
> §2.6 and §2.7 are NOT superseded and are load-bearing under FMG.**

### 2.1 What he draws, and why a PNG mask

**Decision: an indexed PNG colour mask plus a JSON sidecar.** The alternatives
were considered rather than skipped:

- **SVG / vector paths.** More precise, diffs as text, and a river genuinely *is*
  a polyline. Rejected as the authoring format because it fails the request's own
  test — «я вообще напрямую хочу карту нарисовать» means any tool, including a
  tablet and a finger. Painting a coastline blob is how a coastline gets drawn;
  drawing one as a Bézier path is drafting, not sketching.
- **Greyscale heightmap.** Rejected twice over: it forces him to think in
  elevation when the whole point of §2.6 is that he should not have to, and one
  channel cannot say "forest" or "this is a river".
- **Hand-written JSON polygons / GeoJSON.** Precise and agent-friendly, human-
  hostile. Rejected as authoring, **accepted as the compiled intermediate** the
  engine actually consumes (§2.8).
- **PNG mask.** Draws in anything, **diffs visually**, needs no tooling and no
  new engine dependency. Accepted.

The one real objection to a PNG is **antialiasing** — a soft brush produces
boundary pixels that are blends of two key colours and mean nothing, and a
reader that quietly snaps them to the nearest colour is exactly the silent-nearby-
world failure this document exists to prevent. It is answered in §2.5, causally.

### 2.2 Scale — `SKETCH_METRES_PER_PIXEL` = 16 m

Derived from `WORLDGEN_HYDRO_GRID_STEP` = 16 m, the coarse grid on which
`trace_descent` already runs. At 16 m/px **one sketch pixel is exactly one
hydrology cell**, so a drawn river becomes a trace seed with no resampling and
nothing to round, and `CHUNK_SIZE` is exactly 16 px.

| | at 16 m/px |
|---|---|
| Canvas for the 10×10 km target | **640 × 640 px** |
| Today's 2×2 km world | 128 × 128 px — a corner of it |
| One chunk (256 m) | 16 px |
| One polygon (600 m) | 37.5 px |
| Navigable river, 25–35 m (в15 / LF-3) | **2 px** — main channels are drawn 2 px wide |
| Tributary, 3–5 m | sub-pixel — **generated, never drawn** |

That last row is the scale's real justification: it draws the line between what
is authored and what is derived, and it puts it somewhere defensible. The finest
thing a person draws is a main river. Everything finer is a consequence.

**Rule 37 note, because this row is anchored on another row's name.** If
`WORLDGEN_HYDRO_GRID_STEP` ever moves, every drawn map silently rescales. The
remedy is at the join, not in anyone's memory: **the compiler asserts
`WORLDGEN_HYDRO_GRID_STEP == SKETCH_METRES_PER_PIXEL` and refuses to emit if
they have diverged**, naming both values. Two rows, one checked relationship.

### 2.3 The colour key

Six colours. Deliberately ugly and maximally separated — a sketch is an
instrument, not art. Minimum pairwise channel separation is 127, which is what
makes §2.5's antialiasing resolution unambiguous.

| Colour | Hex | Class | Semantics |
|---|---|---|---|
| blue | `#0000FF` | **WATER** | Ocean vs lake is **not drawn**: flood-fill from the image border decides it (Amit's own trick, and it saves a colour and a class of authoring error). A lake touching the border is therefore ocean — reported by name in the compile log, so it is a fact he is told, not one he discovers. |
| green | `#00FF00` | **LAND** | The base. The shipped template is pre-filled with it, so there is no "unpainted" state and a forgotten region cannot read as a default. |
| dark green | `#008000` | **FOREST** | An **override**. Unpainted land takes forest from the moisture field; painting forces a forest mass. |
| grey | `#808080` | **MOUNTAIN** | An override. High ground he insists on; §2.8's massif construction runs inside it. |
| red | `#FF0000` | **RIVER** | 1–2 px line on land. |
| yellow | `#FFFF00` | **SITE** | ≤ 3 px blob. Must have exactly one matching sidecar entry. |

**Why FOREST and MOUNTAIN are overrides rather than data.** Amit's economy is
that only land/water is drawn and everything else is derived; ours cannot be that
pure, because the user named forest and mountain explicitly. The compromise is to
keep them derivable and let the drawing *win where it is drawn*. This is the
honest arrangement — it keeps the derived pipeline exercised on every map instead
of letting it rot behind hand-painted overrides, and the compile log reports what
fraction of forest was painted versus derived, so silent drift toward "everything
is painted" is visible.

A seventh colour for a drawn ridge line was considered and **not added**: grey
regions already control where massifs go, and their internal shape is authored by
§2.8 (lobes, cliff bands, buttress ridges, the `MASSIF_LOBE_RATIO` anti-dome
test) which does not want a hand-drawn spine competing with it. If a map turns up
that genuinely needs one, it enters as a request with a frame that would settle
it.

### 2.4 The sidecar — `<name>.map.json`

JSON, because `engine/core/serialization` supports the binary section container
and JSON and nothing else, and because `load_layout_file()` already establishes
the behaviour this format needs: **unknown keys are a hard error.**

```json
{
  "map": {
    "name": "Vale of Aln",
    "seed": 1,
    "origin_x_m": 0, "origin_z_m": 0,
    "metres_per_px": 16,
    "sea_level_m": 50
  },
  "landforms": ["LF-1","LF-2","LF-3","LF-5","LF-6","LF-7","LF-8"],
  "sites": [
    { "px": [143, 88], "name": "Harrowward", "kind": "castle", "note": "..." }
  ],
  "rivers": [
    { "name": "Aln", "mouth_px": [201, 340], "navigable": true }
  ]
}
```

- `origin_*_m` are **absolute world metres** (§1.1).
- `landforms` is §2.10 rule 4 made machine-readable. The biome lookup may only
  return entries from this list.
- `sites[].px` must land on a yellow blob; the mapping is a **bijection** and both
  directions are checked (a blob with no entry, and an entry with no blob, are
  each an error). This is where story's fiction attaches.
- `rivers[].mouth_px` disambiguates which drawn line is which — the picture knows
  there is a river, only the sidecar knows it is the Aln.
- `navigable` is a **claim that gets checked** against accumulated drainage
  (§2.7), not a switch.

### 2.5 Ambiguity and contradiction — how a malformed sketch fails

**No sketch ever produces a nearby world. If the sketch does not compile, no
world is generated at all.** The random island generator is a separately selected
mode (`--island random --seed N`), never a fallback. This project has shipped four
silent-zero defects in two days; a silent fallback here would be the most
expensive one, because the output would look like a world and merely not be *his*
world.

**Colour resolution — two tiers, split by CAUSE (Rule 36), not by magnitude.**

1. A pixel exactly equal to a key colour takes that class.
2. A pixel that is not, but whose 8-neighbourhood contains exactly two distinct
   exact-match classes, is an **antialiasing artifact on a boundary** and takes
   the majority of its exact-match neighbours. Its count is reported.
3. Anything else — a stray colour, a soft-brush interior, an unresolvable
   3-class junction — is an **ERROR**. The compiler writes
   `<name>.map.errors.png` with every offending pixel flagged in magenta over a
   dimmed copy of the sketch, prints the count and the bounding boxes, and exits
   non-zero.

Tier 2 exists because boundary antialiasing has a *cause* and a signature; a
tolerance on colour distance would have been a magnitude filter, and a magnitude
filter is how a filter becomes the result. Ergonomics are handled outside the
reader: a shipped palette swatch (`.gpl`) and a pre-filled template PNG, so the
exact colours are one click away in any tool.

**The hard-error list, in full.** Each exits non-zero, names the offending
pixels, and emits the error overlay.

| # | Error | Why it cannot be guessed past |
|---|---|---|
| E1 | Unresolvable pixel colour | §2.5 tier 3 |
| E2 | A river polyline that does not terminate at WATER at exactly one end | A river ending in a field is not a river; guessing where it goes is inventing geography |
| E3 | A river network that is not a forest of trees — a cycle, or a channel that **splits** downstream | Many inflows to one channel is a confluence; one channel becoming two is a distributary, which neither Amit's model nor ours can express (§6, unsolved) |
| E4 | A river source that is not in the interior of land | A river starting in the sea has no direction |
| E5 | Water that gains height downstream after the monotone pass | §3.1's central invariant. A lake at 300 m draining into one at 400 m is unconstructible, not merely wrong |
| E6 | Yellow blob without a sidecar site, or sidecar site not on a blob | Bijection; either direction means the picture and the text disagree about how many places exist |
| E7 | Biome lookup resolves to a landform not in `landforms[]` | §2.10 rule 4; the map got a form it did not ask for |
| E8 | `navigable: true` on a river whose drainage cannot reach the 25–35 m band | §2.7 |
| E9 | `metres_per_px` ≠ `SKETCH_METRES_PER_PIXEL`, or the hydro-grid assertion fails | §2.2's Rule 37 join |
| E10 | Gorge depth over `GORGE_DEPTH_MAX` where a drawn river crosses grey | §2.6 |
| E11 | Sketch bounds do not cover the resident world extent at the given origin | The world would have unauthored holes; the crop must be a crop |

**Warnings** (reported, non-fatal, and every one of them is a fact he would
otherwise discover by walking): a blue region touching the border classified as
ocean rather than lake; a drawn river crossing a grey region (a gorge will be
generated, with its depth); a declared landform that no cell resolved to (the map
asked for something it did not use); the painted-versus-derived forest fraction;
each river's accumulated drainage and resulting width class.

### 2.6 A DRAWN RIVER THAT DOES NOT RUN DOWNHILL — the hard part, solved

> **NOT SUPERSEDED — this section is the reason the FMG pivot costs us nothing.**
> FMG derives its rivers from its own heightmap, but the moment he edits one in FMG's
> river editor the conflict returns identically. The construction below is the answer
> whichever tool produced the polyline.

This is the single hardest thing in the request. Amit's rivers are a
*consequence* of elevation: elevation is distance from the coast, it is monotone
inland, therefore it has no local minima, therefore steepest descent from any
corner reaches the sea and river generation is trivial. **A drawn river has no
such guarantee.** Someone will draw a river across a ridge, or from a low place to
a high one, or through a basin.

The three available answers, and why the third is right:

- **(a) Elevation wins; the drawn line is snapped to the nearest valid descent
  path.** Rejected outright: the map he gets is not the map he drew, which
  forfeits the entire request.
- **(b) Reject any drawing that disagrees with a pre-existing elevation field.**
  Rejected as the primary: it makes the user solve a topography puzzle before he
  is allowed to draw. Retained only as the backstop for the genuinely
  unconstructible cases, which is the E2–E5 list above — and note those are all
  detectable **on the drawing alone**, before any elevation exists.
- **(c) THE DRAWN RIVER IS NOT STEERED BY ELEVATION — IT GENERATES ELEVATION.**
  Adopted.

**The construction.** The drawn coast, lakes and rivers form one **drainage
network**. Then:

```
1.  Monotone pass on each drawn polyline, source → mouth: assign a water surface
    height that never increases downstream and is CONSTANT across any standing
    body it passes through.  This is §3.1 step 4 verbatim, run on HIS polyline
    instead of on a traced one.  Sea meets it at sea_level_m.
2.  h_drainage(p) = the water height of the nearest drainage station to p.
    d(p)          = horizontal distance to that station.
3.  h_base(p) = h_drainage(p) + DIVIDE_RELIEF(p) * f(d(p) / d_divide(p))
    where f is Amit's own redistribution y = 1 - (1-x)^2  and d_divide is the
    distance from the drainage to the polygon's divide.
4.  h(p) = h_base(p)  + massif stamps (grey regions)
                      + LF-2 grives
                      + LF-8 erosion
                      + ground micro-relief
    — i.e. compose_passes() as it stands, with h_base replacing the fBm roll as
    the macro term.
```

**Why this works, stated as the property rather than as the mechanism (Rule 38):**
the base term is monotone *increasing away from the drainage*, so downhill from
anywhere leads to the drainage; the drainage itself is monotone by step 1; so
**every drawn river is the valley floor of the valley it drew, by
construction.** §3.1's tracer, run afterwards on the composed field, finds the
drawn river because the drawn river is the lowest path there is. Nothing has to
agree by coincidence.

**Amit's own limitation is what this fixes.** His elevation is distance from the
coast; ours is distance from the coast *and every inland channel and lake*. That
is both the reason a hand-drawn river reproduces and the reason our continents
will not look like volcanic islands — the two problems have one answer.

**What relief re-introduces, and why that is correct.** LF-2 (2–5 m) and LF-8
gullies do create small local minima on top of the base. Those become ponds,
which §3.1's pond-and-spill already handles and which LANDSCAPE §3.1 step 4 has
already ruled the semantics of (a pond is a flat reach, with no level of its
own). This is a feature: a landscape with no closed depressions anywhere is the
tell of a generated world.

**The gorge case, which is the one real residual.** A river drawn across a grey
MOUNTAIN region is not an error — it is a gorge, and gorges are good. But a river
drawn straight over a summit is a contradiction wearing a gorge's clothes. The
discriminator is depth, and it has a control: **report `max(massif height − river
height)` per crossing; over `GORGE_DEPTH_MAX` it is E10.** Control (Rule 30): the
sketch with a river drawn across the massif's summit must fail it; the sketch
with a river drawn through a saddle must pass. Both go in the control set (§3).

### 2.7 River width — accumulation, clamped to authored bands, and the claim is checked

Width comes from flow accumulation on the drawn river tree (Amit: width ∝ √flow),
where flow at a station is the upstream drainage area taken from the polygon
graph's watershed assignment. It is then **clamped into our authored bands**:
3–5 m tributaries (LF-3), `RIVER_WIDTH` 4–8 m ordinary channels, 25–35 m
navigable (в15).

The interesting case is the one that must not be silent. If the sidecar declares
`"navigable": true` and the drainage yields 7 m, the reader does **not** widen the
river to satisfy the flag and does **not** ignore the flag. It fails with E8 and a
sentence the user can act on: *"the Aln is declared navigable but drains 41 cells
(≈ 7 m). Draw more tributaries into it, move its mouth downstream, or drop the
flag."* That is the shape every error in this format should have — it names the
contradiction and lists the ways out.

### 2.8 The pipeline, end to end

```
   <name>.map.png   (drawn, any tool)
   <name>.map.json  (sidecar; story writes the names)
            |
            |  tools/sketch_compile.py        <- ALL validation lives here
            |    colour resolution (§2.5)
            |    flood fill: ocean vs lake
            |    connected components: landmasses, lakes
            |    river skeletonisation -> polylines; tree/topology check (E2-E4)
            |    site bijection (E6)
            |    emits <name>.map.errors.png and exits non-zero on ANY error
            v
   <name>.dfnm      (compiled: binary section container, Rule 7 -
                     magic + version, INFO / MASK (RLE) / RIVR / SITE,
                     explicit little-endian, unknown sections skipped)
            |
            v
   engine/world:  polygon graph (points -> Lloyd x2 -> Voronoi/Delaunay dual)
                  land/water on CORNERS from MASK; polygons by corner fraction
                  drainage heights (§2.6 step 1)  -> h_base (steps 2-3)
                  moisture (from the ONE distance field, §1.6.2)
                  biome lookup -> declared landform entries (E7)
                  -> macro_height() -> compose_passes()  [the one sampler]
                  -> hydrology (source/mouth/flow seeded; trace unchanged)
                  -> classify_surface(), build_scatter()  [unchanged callers]
```

**Why the compiler is offline Python and not C++.** Four reasons, and the first
is decisive:

1. **There is no PNG decoder available to `engine/world`.** `bimg` vendors
   `stb_image` but `engine/platform/render/CMakeLists.txt` links **encode only**,
   and it is PRIVATE to `dfn_platform_render` by Rule 1 — `world` depends on
   `core` alone and cannot reach it. Writing a PNG decoder into `core` means
   inflate plus PNG filtering for a feature whose input is authored once per map.
2. **Validation belongs at authoring time.** He gets the error in a terminal with
   an overlay image, at the moment he saves the drawing — not at game startup.
3. **Rule 6 is satisfied**: compiling a sketch is not compiling C++.
4. **The messy work is image processing** — skeletonisation, flood fill,
   connected components — which is short in Python and long in C++. `tools/` has
   precedent (`pngdiff.py`, `measure_ground_junction.py`), and PNG decoding for
   our subset (8-bit, non-interlaced) is `zlib` plus `struct`.

**All three files are checked in** — PNG and JSON because they are the reviewable
source, `.dfnm` because a clean clone must run without invoking Python (Rule 24).
CI regenerates the `.dfnm` and fails on any difference, which costs nothing and
doubles as a Rule 13 determinism control on the compiler.

---

## 3. THE N EXAMPLE MAPS

`mapstory` owns the content and is briefed. **Design owns the format and the
acceptance.** Content lands in `docs/story/WORLD_MAPS.md`; the compiled maps land
in `games/daggerfall_n/assets/world/maps/`.

**N = 4, and the number is derived the same way a threshold is (Rule 30).** Four
is the count of distinct **topologies the reader must be proven on** — each map
exists because it is the only one that can fail a particular branch. It is not a
round number and it is not "enough to look at".

| Map | The branch only it can fail |
|---|---|
| **M1 — island, one river to the sea** | Amit's canonical case. The smoke test: one landmass, one mouth, one massif. If M1 does not work nothing does. |
| **M2 — the home valley, re-expressed** | **Continuity.** Our existing 2×2 km testbed — Ravenscar, the river, Harrowward — drawn as a sketch. It is the only map that can fail the claim *"this pipeline can reproduce a world we already have and accepted"*. Constrained rather than free; design co-owns it with story. |
| **M3 — coast with a bay, plus an inland lake with an outflow** | Flood-fill (which blue is sea, which is lake) and the river-through-lake flat reach. Fails if ocean/lake separation or §3.1's reach rule is wrong. |
| **M4 — endorheic interior, no ocean at all** | **The branch Amit's pipeline cannot do.** Under distance-from-coast it is not expressible; under distance-from-drainage it is ordinary. Also the shape most of Daggerfall's own map has. Fails if §2.6 was not actually adopted. |

**What makes a map good** — the acceptance story writes against:

1. It **declares its landforms and uses all of them.** A declared form no cell
   resolved to is a warning; it means the map asked for something it did not need.
2. **Every river terminates at water**, and the river graph is a tree.
3. **Every site sits somewhere geography explains** — a ford, a bend's outer
   bank, a spill point, a headland, a confluence — rather than somewhere the name
   sounded good. This is the test of whether the map is a *map* or a list.
4. **Its fiction survives the terrain between the named places being generated.**
   If the story depends on what is at 4.2 km east, the map is over-specified for
   what this format promises.
6. **It states the SETTINGS that produce what its fiction leans on**, not just
   the place — latitude above all, because lake type is derived from climate and
   not authored (§9.9). *(Added by the FMG amendment; the point generalises to
   biome, port status and population.)*

5. It is drawable in twenty minutes. A map that takes a day to paint will not be
   iterated on, and iteration is the entire value of the drawn-map path.

### 3.1 The control set — design's, not story's

Six deliberately malformed sketches, each broken in **exactly one** way, each
paired with the near-identical valid sketch it differs from. They are not a map;
they are the instrument's controls (Rule 30), and they are the thing that proves
the error list is a check rather than a description.

`C-E2` river ending in a field · `C-E3` a channel splitting downstream ·
`C-E5` a lake draining uphill into a higher lake · `C-E6` a yellow blob with no
sidecar entry · `C-E10a` a river drawn over the massif summit (must fail) paired
with `C-E10b` the same river through a saddle (must pass) · `C-AA` the same
sketch saved with a soft brush (tier-2 resolution must absorb the boundary and
tier 3 must catch the interior).

---

## 4. WHAT IT BUYS THE EXISTING TERRAIN

«может это обогатит генерацию земли» — yes, and specifically:

### 4.1 Open items this CLOSES

| Item | Where it is open | How |
|---|---|---|
| **"region lake density is FUTURE — needs the moisture field"** | LANDSCAPE §3.2 | The moisture field arrives. This is the named blocker, closed by name. |
| **Forest masses come from "a coarse moisture-like noise field"** | §2.1 | Replaced by a derived field. See §4.2 — with a warning attached. |
| **`ForestRegions` hand-typed rectangles and annuli** | `TestbedLayout.h:274` | Replaced by elevation × moisture. Species stops being "which rectangle am I in". |
| **Flora density and species have no climate input** | `build_scatter()`; species is slope/altitude/water-band only | Moisture and elevation zone give the §5 catalog a real 2D key. This is the largest single "обогатит" item. |
| **There is no sea in the generator at all** | §8's sea stand (в22), LF-6 coastal cliffs | A drawn coastline gives the shore stand a geography instead of a stamp, and LF-6 its first host. |
| **The river+castle stand's siting** | §8.2 | Real confluences, a real main channel, and a castle sited by the P4 scorer against real geography rather than by an authored anchor. |
| **"one LR per ≈4×4 km region cell", unimplemented** | §1.3a; `LR_*` NUMBERS rows marked **НЕ ПОСТРОЕНО** | Elevation zones over the polygon graph give each region cell a principled place to put its LR. Does not build the LR; gives it somewhere to be. |
| **"composition is declared, not emergent" has no machine form** | §2.10 rule 4 | The sidecar's `landforms[]` plus E7. |

### 4.2 Rule 44 warning — `FOREST_COVERAGE` must be RE-DERIVED, not carried

`FOREST_COVERAGE` (0.25–0.40, testbed target 0.30) was fitted while forest masses
came from a *noise field standing in for moisture*. When a derived moisture field
replaces it, the path between the constant and the outcome changes gain — and
this project has already paid for exactly this once, in the PathMargin /
ForestFloor composition that realised 2.5–2.7× over and 3.3–6.6× under its
authored densities in opposite directions.

**So: measure the realised coverage under the new field before touching the row,
and if the row has to move, say in its NUMBERS note what it now means.** The same
caution applies to anything else fitted through the old field — `BR-4`'s clump
coverage figures are the ones to check first.

### 4.3 What it does NOT close — stated so nobody expects it

- **The massif's shape.** Polygon elevation is 600 m granular; §2.8's banded
  contours, cliff bands and lobes are unaffected and unhelped. Amit's elevation
  model is rejected *because* it would have reached in here.
- **LOD and residency.** `CHUNK_LOAD_RADIUS` = 2 means the world stops at ~512 m.
  A regional map makes this **more** visible, not less: 13 polygons over the
  current world and the player can see about one. This feature does not unblock
  the §1.3/§1.3a landmark doctrine; LOD still does.
- **Rivers at walking scale.** The carve, the trapezoid section, the fords,
  `NAVIGABLE_DRAFT` — untouched.
- **The palette.** Biomes *add* material families, and §4.2 of LANDSCAPE already
  records that 64 shades are short by 22 with rock and sky at 52% coverage. This
  makes that worse. Flagged, not solved, and not urgent while full colour is the
  basis.
- **Anything at eye height.** Nothing about the frames this project shoots at
  360 m changes on the day this lands.

---

## 5. ACCEPTANCE

### 5.0 The honest split, up front

**"The world reproduces the sketch" is a claim about a PICTURE, and Rule 41 says
an instrument that measures the object cannot settle it.** The proposal on the
table — render the coastline back to a mask and compare per-pixel — is *nearly*
right and fails Rule 41 in its usual costume: a coastline displaced by one polygon
across a whole island barely moves an IoU, while a coastline that preserves IoU
and drops a peninsula is a different map. **Identical number, opposite verdicts.**

So the split is stated deliberately rather than papered over:

> **The user's eye accepts the headline claim. The three instruments below exist
> so that once he has said yes, we can tell the day it stops being true.**

The eye's evidence is two archived frames (§5.5). The numbers are regression
instruments. Presenting the numbers as the acceptance of "it looks like my
drawing" would be the exact error Rule 41 was written for.

### 5.1 A — realised shoreline agreement

- **Quantity:** the generated heightfield thresholded at `sea_level_m`,
  downsampled to the sketch grid, XOR'd against the sketch's water mask.
- **Aggregation:** fraction of pixels disagreeing.
- **Denominator:** **sketch pixels inside the resident world extent** — not the
  whole canvas, or a map larger than the world scores well for free.
- **Threshold, derived not chosen:** the only legitimate disagreement is the
  domain-warp band along the coast. For warp amplitude *A* metres and coast
  length *L* pixels, allowed disagreement = `L · (A / 16) / N`. Quote the derived
  figure per map; do not carry one map's number to another (their coast lengths
  differ, which is the same defect as one control taken across classes).
- **Can it fail?** Yes: a massif stamp flooding a bay, LF-8 cutting a channel to
  the sea, the base term overshooting near a mouth. That is what it is for.

### 5.2 B — coastline displacement, as a distribution

- **Quantity:** for each sketch coast pixel, distance in metres to the nearest
  realised coast pixel.
- **Aggregation:** **p95 and max**, reported together. Not the mean — the mean is
  where cancelling errors hide, and it cannot see a lost peninsula.
- **Denominator:** sketch coast pixels inside the resident extent.
- **Thresholds:** p95 ≤ the domain-warp amplitude; **max ≤ `POLYGON_SPACING`
  (600 m)** — a coast displaced by more than one polygon has changed *which
  polygon is coastal*, which is a change of place, not of shape.
- **Rule 45 check, and it is mandatory before this quantity is trusted.** Is 600 m
  a floor or a separator? Its derivation mentions polygon size, not a rejected
  instance — which is the tell of a floor. **Stopping condition: measure the
  off-diagonal pairs (§5.4) FIRST. If a different sketch does not score at least
  3× the diagonal on B, refuse quantity B and rely on A and C.** Finding the
  quantity wrong is a legitimate outcome and beats a fitted number.

### 5.3 C — topology, which is what pixels cannot see

- **Quantity:** counts and identities — number of connected landmasses, number of
  lakes, number of river mouths, the river tree's edge list, and for each site
  the **identity of the polygon it lands in**.
- **Aggregation:** **exact equality**. No tolerance; these are integers and names.
- **Denominator:** the counts in the sketch.
- **Its control, which is the point:** a sketch differing from another *only* by
  one island splitting in two must **fail C while passing A and B**. If it does
  not, C is not measuring topology and should be rebuilt. This is the instrument
  that expresses "same shape" in the sense a person means it, and it is cheap.

### 5.4 The cross-sketch matrix — the control the whole feature rests on

Generate all four maps. Score **every generated world against every sketch**: a
4×4 matrix, 16 cells.

- **The 4 diagonal cells must pass all three instruments.**
- **All 12 off-diagonal cells must fail**, on C at minimum.
- Report it as a matrix, and report the *margin* — the ratio between the worst
  diagonal and the best off-diagonal on each instrument. That ratio, not the
  thresholds, is the honest statement of whether these numbers discriminate.

**And the seed-invariance arm, which tests the thing §0 says cannot be steered.**
Run M1 at three polygon point-seeds. All three must pass all three instruments,
**and** pairwise realised coastlines must agree within the same tolerance. This is
what makes "the drawing steers classes, never the mesh" a checked property rather
than an intention.

### 5.5 The frames (Rule 27)

Two, both archived at native resolution into `docs/acceptance/` with their
recipes, per Rule 27's archiving clause.

1. **The overhead pair** — the sketch and a top-down render of the generated world
   at the sketch's scale, side by side, same crop. **This is the frame the user
   accepts or rejects the headline claim from.** Its vantage can fail: a
   generation that ignored the sketch produces an obviously different picture.
2. **An eye-height frame at a place the sketch names** — standing on the headland
   the sketch draws, looking across the bay it draws. Because an overhead match
   can be perfect while the world at eye height is unrecognisable as anywhere,
   and this project's whole §1.7 doctrine is about the second thing.

### 5.6 Compiler acceptance

The `.dfnm` for each of the four maps regenerates **byte-identically** from its
PNG + JSON (Rule 13.1, and it is free). The control set (§3.1) exits non-zero on
all six, each with the expected error code and each paired against its
near-identical valid sketch, which must exit zero.

---

## 6. UNSOLVED, AND NAMED AS SUCH

- **Deltas and distributaries.** A channel splitting downstream is not expressible
  and is rejected (E3). Amit cannot do it either. A real limitation on any map
  wanting a river mouth that fans.
- **Braided and anastomosing channels.** Same reason.
- **A drawn ridge line.** Deliberately not added (§2.3). If a map needs a crest
  the grey region cannot express, it comes back as a request with a frame.
- **Coast at two scales.** The drawn coast is 16 m/px; the walked coast is a
  metre. What happens between them is the domain warp, and its amplitude is
  currently an unauthored consequence of the warp field rather than a designed
  quantity. It is the input to §5.1's and §5.2's thresholds, so it has to become
  authored before those thresholds mean anything.
- **`DIVIDE_RELIEF` has no frame yet.** §7 proposes both ends with derivations;
  neither has been shot.
- **The 13-polygon problem.** At 2×2 km this structure is nearly inert. There is
  no way to make it otherwise, and the honest answer is that its acceptance is an
  overhead acceptance until the world is 10 km.

---

## 7. NUMBERS REQUESTS — for `main`, with derivations

`docs/NUMBERS.md` is lead-owned. Rows requested, each with the derivation that
produced it, both ends of every band derived separately (Rule 30):

| Row | Value | Unit | Derivation |
|---|---|---|---|
| `SKETCH_METRES_PER_PIXEL` | 16 | m | Equals `WORLDGEN_HYDRO_GRID_STEP`, so one sketch pixel is one hydrology coarse cell and a drawn river seeds `trace_descent` with no resampling; also makes `CHUNK_SIZE` exactly 16 px. **The compiler asserts the equality and refuses to emit on divergence** (Rule 37 join). |
| `POLYGON_SPACING` | 600 | m | Three bounds. Upper: widest chord `1.155·s` ≤ `LANDMARK_HAZE_ONSET` (800) → s ≤ 693. Lower: ≥ 3 × `LF2_HILL_WAVELENGTH` (100) so a ridge-and-swale polygon contains a rhythm → s ≥ 300. Check: inside the region POI band 540–900, which is the definition — a polygon is one POI's catchment. Yields 0.312 km²/cell; ≈13 cells at 2 km, ≈321 at 10 km. |
| `LLOYD_ITERATIONS` | 2 | — | Amit's own figure; he states more iterations trade interest for uniformity. Not a tuning knob — §5.4's seed-invariance arm is what would detect it mattering. |
| `WORLDGEN_SEA_LEVEL` | 50 | m | **A per-map DEFAULT, overridden by the sidecar's `sea_level_m`** — the forest stand sits at `FOREST_BASE_ELEV` 20 m and has no sea at all, so a single global sea level would put an existing stand underwater. Derivation: ≥ 3 × `MASSIF_CLIFF_BAND_MAX` (15) = 45 m, so LF-6 can cut a full banded cliff face without hitting the 0 floor of the shared quantisation range. Upper bound `WORLDGEN_MAX_HEIGHT` (400) − `LR_RELIEF` (280) − divide relief, which 50 clears. **⚠ THE DERIVATION IS ANCHORED ON A CONSTANT NUMBERS MARKS AS PENDING RE-DERIVATION** — `MASSIF_CLIFF_BAND_MAX`'s own row says «ПОДОГНАНО ПОД СЛОМАННОЕ ПОЛЕ, ЖДЁТ ПЕРЕВЫВОДА» (the `bearing_field` returning only its top 60%). So 50 is a placeholder that must be re-derived when that row is, and this is written here rather than discovered later because Rule 37's second half is exactly this: what is anchored on that name? |
| `DIVIDE_RELIEF_MIN` | 20 | m | Must dominate LF-2, or the regional structure is invisible under the local one: 4 × `LF2_HILL_RELIEF_MAX` (5). **Proposal — the frame that would settle it is the cross-valley overhead at 10 km.** |
| `DIVIDE_RELIEF_MAX` | 60 | m | Must not out-compete `L0_RELIEF` (115) in its own valley, or the §1.3 dominance hierarchy inverts; half of it. Also keeps the mean grade over a half-polygon (300 m) at 20%, under the 25° corridor ceiling. **Proposal.** |
| `GORGE_DEPTH_MAX` | 120 | m | The depth at which a drawn river crossing grey stops being a gorge and becomes a contradiction. Derived as `L0_RELIEF` (115) rounded up — a cut deeper than the valley landmark's whole relief is not a gorge through a mountain, it is a mountain that should not have been painted there. **Control exists** (§3.1 `C-E10a`/`C-E10b`), which is what makes this a separating threshold rather than a floor. **Proposal.** |
| `SKETCH_COAST_DISPLACEMENT_MAX` | = `POLYGON_SPACING` | m | §5.2. **Held pending §5.2's Rule 45 stopping condition** — if the off-diagonal pairs do not separate by 3×, this row is refused rather than fitted. |

Not NUMBERS rows, and deliberately: **the colour key and the biome table are
content** (Rule 5) and live in data files. A colour is not a simulation constant.

---

## 8. WHAT `core` WOULD BUILD, IN ORDER

Sequencing note for the lead, highest-value-first, each step shippable:

1. `tools/sketch_compile.py` + the template PNG + the palette swatch + the control
   set (§3.1). **Nothing in C++.** This alone gives the user the loop he asked
   for — draw, compile, see the errors — and it is where every validation lives.
2. The `.dfnm` reader in `engine/world` (binary section container; the
   `load_layout_file` precedent for hard-erroring on anything unexpected).
3. The polygon graph: points → Lloyd ×2 → Voronoi/Delaunay dual, land/water on
   corners, flood fill, components. Independently testable against the mask with
   no terrain involved.
4. §2.6's drainage heights and `h_base`, entering through `macro_height()`.
   **The first step that changes a pixel of the game**, and the one that needs
   §5.1/§5.2 standing before it lands.
5. Moisture from the existing distance field; the biome lookup; `landforms[]`
   enforcement.
6. Hydrology seeding from the drawn rivers; flow accumulation and width bands.
7. `build_scatter()` reading moisture and elevation zone; `ForestRegions` deleted
   in the same change (Rule 32 — the mechanism, not the instance).

---

## 9. AMENDMENT — AZGAAR'S FMG BECOMES THE AUTHORING TOOL

> «так, нашел другой генератор в 1000 лучше —
> https://azgaar.github.io/Fantasy-Map-Generator/ — тут столько всего
> конфигурировать и сделать можно»

**The seam moves and it moves in our favour.** FMG is a browser application, not a
library: there is no algorithm to lift and porting is off the table. So FMG
becomes the **authoring tool** and we consume its export. That is a closer answer
to the original request than anything we would have built — he configures without
touching our code, and FMG's own image converter means a drawing on paper still
becomes a world.

**What is superseded:** §2.1–§2.5 (the PNG colour key, the two-tier antialiasing
resolution, the ambiguity list) and §2.8's `tools/sketch_compile.py` **as the
authoring path**, and §3.1's control set as painted PNGs. They are kept, not
deleted, because **the reasoning in them is what justifies the importer's
requirements** — every ruling below is a re-targeting of one of them, and §9.6
maps each one across.

**What survives untouched:** §1 (where it sits — regional structure over the
10×10 km target, today's 2 km a crop, anchored in absolute world metres); §1.6
(the five shadow copies, now MORE urgent — see §9.5); **§2.6 (a drawn river
GENERATES elevation), which is the best thing in this document and is the answer
whichever tool produced the polyline**; §5 entire (the three instruments, the
cross-map matrix, the seed-invariance arm, Rule 45's stopping condition); and the
no-silent-fallback doctrine.

### 9.1 Licence — CHECKED, not assumed

**MIT**, © Max Haniyeu (Azgaar), read from the repository's own `LICENSE` file.
It carries a non-standard *expansion* rather than a restriction: *"You can produce,
without restrictions, any derivative works from the original software and even
reap commercial benefits from the sale of the secondary product"*, extended
explicitly to *"created maps, map images, screenshots, videos, and other
materials"*. No attribution burden beyond shipping the licence text. **We may
depend on it and we may ship worlds made with it.** Blocker cleared.

### 9.2 HEIGHT RESOLUTION — measured, and the answer is that `h` never becomes our elevation

**The conversion, verbatim from `src/utils/unitUtils.ts:61–74`:**

```js
let height = -990;
if (h >= 20)          height = (h - 18) ** +heightExponentInput.value;
else if (h < 20 && h > 0) height = ((h - 20) / h) * 50;
```

Metres when the unit is `m` (the base expression is metric; `unitRatio` 3.281
converts to the default feet). **`heightExponent` is carried in the JSON export**
(`src/services/io/export-json.ts:113`), so **the importer reads it and never
assumes a default** — this document deliberately does not quote one.

**The measurement, at exponent 2.** The conversion is a power law, so the step is
not uniform, and that changes the answer completely:

| FMG `h` | our metres | step to next `h` |
|---|---|---|
| 20 (sea level) | 4 m | **5 m** |
| 25 | 49 m | 15 m |
| 30 | 144 m | 25 m |
| 38 | **400 m** = `WORLDGEN_MAX_HEIGHT` | 41 m |
| 100 (FMG max) | 6724 m | 165 m |

**So "eighty steps across the land range" is true of FMG's range and false of
ours.** Our entire world height budget, 0–400 m, is FMG `h ∈ [20, 38]` — **19
integer values** — and the quantum is **finest exactly where our world lives**: 5 m
at the coast, 41 m at the ceiling.

**The ruling, and it is the same ruling that rejected Amit's elevation model, which
is the strongest argument for it: FMG's `h` is NOT our elevation.** It supplies
exactly two things — **(a) the land/water mask at the threshold `h ≥ 20`**, and
**(b) the MOUNTAIN region, the successor to §2.3's grey paint, at a threshold read
from the export.** Elevation comes from §2.6's drainage construction, unchanged.

**The number that says why, which is what `main` asked for.** If anyone did use
`h` as elevation, the terrace to be smoothed is the quantum — **41 m at the top of
our range** — and the term available to smooth it is `DIVIDE_RELIEF`, whose band
is 20–60 m. **The filler and the artefact are the same size**, so the filler
cannot hide the artefact; it can only move it. That is the quantitative form of
the ruling, and it is the number to quote at anyone who proposes reading `h` as
metres. Under §2.6 the question does not arise: `h_base = h_drainage +
DIVIDE_RELIEF · f(d/d_divide)` is continuous by construction and takes nothing
from `h` but a mask.

### 9.3 CELL SIZE — the question re-derived as `main` asked, and the answer is a configuration instruction

`POLYGON_SPACING` was ours to choose; FMG's cell size is set by its own controls.
So the question becomes **"which FMG map settings give a cell we can accept"**.

**The formula, verbatim from `src/utils/graphUtils.ts:82`**, on a jittered square
grid:

```js
const spacing = rn(Math.sqrt((graphWidth * graphHeight) / cellsDesired), 2);
```

Multiply through by the km-per-pixel setting and the pixels cancel:

> **cell spacing = √( map area in km² / `cellsDesired` )**

**This is scale-free** — the "1 pixel = X km" slider does not enter. Only the map's
physical area and the cell count do. That is a much better situation than it
looked, because it means one instruction covers every zoom he might use.

`cellsDesired` defaults to 10,000, range 1,000–100,000. §1.2 derived the
acceptable spacing interval as **[300 m, 693 m]** (≥ 3 × `LF2_HILL_WAVELENGTH`;
widest chord under `LANDMARK_HAZE_ONSET`). Inverting:

| `cellsDesired` | map area for 600 m cells | acceptable map area, [300, 693] m |
|---|---|---|
| 1,000 (min) | 360 km² ≈ 19 × 19 km | 90 – 480 km² |
| **10,000 (default)** | **3,600 km² ≈ 60 × 60 km** | **900 – 4,800 km² (30×30 to 69×69 km)** |
| 100,000 (max) | 36,000 km² ≈ 190 × 190 km | 9,000 – 48,000 km² |

**The configuration instruction, derived rather than chosen: at FMG's default
10,000 cells, make the map between 30×30 km and 69×69 km, and 60×60 km puts the
cell exactly on `POLYGON_SPACING`.** Our 10×10 km target world is then 1/36 of it
and holds **≈ 278 cells**; today's 2×2 km holds **≈ 11**, which is the same
answer §1.2 gave and is the reconciliation that matters.

**And the number that decides the architecture — the counter-case.** At FMG's
*continental* defaults (1 px = 3 km on a ≈1900×950 canvas → ≈5700 × 2850 km,
16.2 M km², 10,000 cells) the cell spacing is **≈ 40 km**. At that setting **our
entire 10×10 km target world is one sixteenth of ONE CELL.** There is no
interpolation that recovers a coastline from that; the map simply does not contain
one.

**So there are two regimes and he must pick per export. They compose, and
conflating them is the failure mode:**

- **ATLAS regime** (continental, ~40 km cells). FMG supplies **fiction and
  context** — which state, which culture, which biome band, nearest burg, is this
  coast — and **no geometry whatsoever.** Our generator supplies every metre. This
  is what the map screen shows and what story writes against.
- **REGION regime** (30–69 km at 10k cells, ~600 m cells). FMG cells **are** our
  polygons: its coastline, rivers, biomes and burgs become geometry. **This is the
  regime that realises the original request.**

**The importer computes `√(area / cellsDesired)` from the export's own metadata
and hard-fails outside [300 m, 693 m]** (error **E9′**), naming the computed
spacing and the map area that would fix it. An ATLAS export handed to the importer
is rejected in one line rather than producing a world at 40 km granularity — which
would be precisely a silently-nearby world, and a spectacular one.

### 9.4 WHICH REGION — the crop, and who chooses it

**He chooses it, in FMG, using an object that exists in his map so it survives
regeneration and round-trips: a MARKER.**

He places a marker named `DFN_ORIGIN`. FMG exports markers to CSV with lon/lat
(and height). The importer takes the `WORLD_EXTENT_CHUNKS` square centred on it.

- **Missing marker → hard error.** There is no default crop and no centre-of-map
  guess. A crop guessed is a world in the wrong place, and it would look fine.
- **Two markers with that name → hard error**, naming both.
- The resolved lon/lat and world origin are **recorded** into the compiled
  artifact's `INFO` section — recorded, not re-decided. One authority (Rule 39).

Why a marker rather than a field in our sidecar: the crop is a geographic decision
and it belongs where the geography is, visible on the map at the moment it is
made. A number typed into a JSON file beside a map he keeps regenerating is a
second copy waiting to drift.

**In REGION regime the crop is 1/36 of the map** (10 km of 60 km), so choosing it
is a real decision and worth a marker. In ATLAS regime there is no crop to choose,
because there is no geometry to crop.

### 9.5 RULE 39 UNDER FMG — three new shadow copies, because FMG generates far more than terrain

§1.6's five rulings stand unchanged. FMG ships cultures, states, provinces, burgs
with population and port status, religions, trade routes and military — layers we
do not have and will need. **Every one of them is a place for a second authority
to appear, and the pattern of the ruling is the same each time: FMG owns the
GRAPH, we own the GEOMETRY.**

6. **Two authorities for "where is the castle."** FMG's burgs versus our P4 site
   scorer and `TestbedLayout`'s sites. *Ruling: FMG's burgs are authoritative for
   WHICH places exist and for their names, population and port status. The P4
   scorer is authoritative for the exact metre-scale pad inside the cell, and it
   is not consulted about whether the place exists.* Not "the scorer proposes and
   we compare" — that is two copies with a comment holding them together.

7. **Two authorities for routes.** FMG's trade routes versus §8.1's four-type path
   network and its single `dist_to_path` field. *Ruling: FMG routes are
   authoritative for the route GRAPH — which burg connects to which, and by land
   or water. Our generator is authoritative for the polyline, the type selection
   and the cross-section.* Same shape as the river ruling (§1.6.1), deliberately.

8. **Two authorities for biome.** FMG's `biome` id versus the elevation × moisture
   lookup this document specified. *Ruling: in REGION regime FMG's biome id is the
   input and our lookup is retired for cells FMG covers; the moisture field is
   still built, because §3.2's region-lake blocker and the flora tables need a
   CONTINUOUS field and a biome id is categorical.* Two things that sound like one
   thing: **which recipe runs here** (FMG) and **how wet is it here, exactly**
   (ours, from the one distance field of §1.6.2).

### 9.6 WHAT THE SUPERSEDED SECTIONS BECOME — each ruling re-targeted

The PNG format is gone; **the reasoning that produced it is the importer's
specification.** Line by line:

| §2 ruling | Its heir in the importer |
|---|---|
| **Two-tier colour resolution, split by CAUSE not magnitude (§2.5)** | FMG's fields are categorical integers, so there is no antialiasing — **but the analogous defect is exactly present**: an FMG biome id, lake type, or route type with no entry in our mapping table. **Same ruling, same reason: a mapped id resolves; an unmapped id is an ERROR, never a nearest-match.** This is the single most likely place for a silently-nearby world under FMG, because a nearest-biome fallback is the obvious thing to write and it would never go red. |
| **E1 unresolvable pixel colour** | **E1′ unmapped attribute id** (biome, lake type, route type, burg type), naming the id and the cells. |
| **E2/E3/E4 river topology** | **Unchanged in substance.** FMG's rivers are derived from its own heightmap and are well-formed — but the moment he edits one in FMG's river editor we are back in the same conflict, which is why §2.6 survives. Check them, do not trust them. |
| **E5 water gains height downstream** | **Unchanged, and now the most valuable check we have**, because the heights come from a tool whose datum is not ours. |
| **E6 site bijection** | **E6′ burg ↔ site bijection** between the CSV export and the compiled artifact. |
| **E7 undeclared landform** | **Unchanged.** §2.10 rule 4 survives the tool change intact: the map declares its composition and a cell resolving outside it is an error. |
| **E9 metres-per-pixel** | **E9′ cell spacing outside [300, 693] m** (§9.3) — the ATLAS/REGION guard. |
| **E10 gorge depth, E11 crop coverage** | **Unchanged.** |
| **No silent fallback, ever (§2.5)** | **Unchanged and more important**, because an import has more ways to half-succeed than a paint file does. |
| **`tools/sketch_compile.py`** | **`tools/fmg_import.py`.** Same architecture and the same first reason: it is offline, in Python, and emits the same `.dfnm` binary section container. FMG exports GeoJSON and CSV, which are text, so the PNG-decoder argument is moot — but the other three reasons stand, and the compiled artifact, its byte-identical regeneration check (§5.6) and the whole downstream C++ path are **unchanged**. |
| **§3.1's six painted control sketches** | **Six FMG exports, hand-edited to be malformed in exactly one way each**, plus their near-identical valid twins. The `C-AA` antialiasing control is replaced by **`C-BIOME`: an export carrying one biome id absent from the mapping table**, which must fail rather than snap. |

### 9.7 `WORLDGEN_SEA_LEVEL` — it stays, and it does NOT become an FMG mapping constant

`main` asked whether the row becomes a mapping constant or disappears. **Neither:
it stays, with its meaning unchanged, as the per-map default it was already
amended to be.**

FMG's `20` is a **dimensionless mask threshold** on a Uint8 field. Our
`WORLDGEN_SEA_LEVEL` is **the water-surface height of the ocean drainage in our
datum, in metres** — the value the §2.6 monotone pass starts from, and the number
LF-6 needs 45 m of room below. Identifying the two would be **Rule 43 exactly**: a
bound written on one quantity and enforced on another, holding only while a
coincidence holds.

The mapping between them is not a constant at all — it is the whole of §2.6. And
the row's existing flag stands: its derivation is anchored on
`MASSIF_CLIFF_BAND_MAX`, which NUMBERS marks as pending re-derivation.

### 9.8 WHAT MUST BE MEASURED IN THE TOOL — one closed, one replaced, two open

Claims this amendment does **not** make on its own authority, because they are
properties of a running application. Each is graded by what kind of evidence
settles it, because "measured" has covered three different standards in this
section alone.

**1. CLOSED — FMG CAN produce a map with land at every border. Mechanism, not a
frame (story's finding).** FMG's `Mask` operation is exactly what manufactures
border ocean (it grades edge cells toward 0; fraction 2 leaves them at 50), and
per FMG's template-editor documentation **a NEGATIVE fraction inverts it, lowering
cells in the map CENTRE instead** — an inland basin as a single template op, high
rim and low middle, land to the frame. The heightmap brush and the image converter
reach the same place by hand. So *"FMG floods the borders"* is a **default, not a
constraint**, which is the distinction §9.8 could not settle from source.

**And the evidence argument is right, so it is recorded rather than waived:** a
browser run tells you what one seed did and cannot distinguish *"this template
happens not to flood the border"* from *"the border can be kept dry on purpose"*.
**Naming the mechanism settles the general case; a seed cannot.** Two things a
frame would still add, neither of which blocks anything: it would show what the
*default* templates do, which is a different question and is the one a user will
hit first; and it would catch a documented operation not behaving as documented.
Grade of this evidence, stated plainly: **documentation-derived, not
source-derived and not frame-verified.**

**2. ITS REPLACEMENT, and it is the sharper question (story's) — DO FMG'S
DOWNSTREAM MODULES ASSUME AN OCEAN EXISTS?** Ports and harbours, the `Marine`
biome, and above all the **temperature and precipitation passes, which key off
distance-to-water**, all run after the heightmap on a map that now has none. The
risk did not disappear when (1) closed, it **relocated from the map to the
import**, and it is what the first endorheic export must be inspected for. This is
an importer question, not a map question.

**3. OPEN — what does a REGION-regime export actually contain?** At 30–69 km, does
FMG still assign states, cultures and burgs sensibly, or one state and three
burgs? Decides how much of the fiction layer is usable at playable scale.

**4. OPEN — does the GeoJSON export carry river `discharge` and `length`**, and
are lake types (fresh / salt / dry) in it, or only in the `.map` format? §2.7's
width-from-accumulation and E8's navigability check are both written against
discharge.

### 9.9 LAKE TYPE IS CLIMATE-DERIVED, WHICH MAKES LATITUDE AN AUTHORING INPUT

**The finding is story's and it is a format requirement, not a map note.** FMG
classifies a lake fresh / salt / **dry** dynamically, from river flux against
evaporation (a simplified Penman using surface, temperature and elevation). So a
terminal salt lake is **native** — three rivers into one sink with no outlet is
salt by FMG's own hydrology, and we do not draw it.

**But the salt is derived, not authored.** Site the same province somewhere cool
and wet and FMG returns a large *fresh* lake, probably with an outlet — and every
consequence the fiction was built on (the shrinking, the salt, the relict
shorelines, the stranded harbours) is gone, leaving a lake district.

> **Requirement, added to §3's list of what a map must specify: any map whose
> fiction depends on a lake being salt or dry must state its LATITUDE as an export
> requirement.** It is a load-bearing authoring decision and it must not be left to
> whoever happens to configure the tool.

This generalises past lakes and is worth holding on to: **FMG derives more than it
draws.** Anything the fiction leans on that FMG *computes* — lake type, biome,
port status, population — is a consequence of settings, so the settings that
produce it belong in the map's requirements, not in a sentence about the place.

**LF-11's control is closed by the same mechanism.** I flagged LF-11 (relict
shoreline) for having only a synthetic control. **FMG has a native `dry lake`
type** — a real relict basin the tool produces on its own — which is precisely the
real rejected instance Rule 45 asks for, supplied by the tool rather than
invented. The flag is lifted.

### 9.10 THE WATER HALF OF `h` — the same ruling, applied once more

Confirmed: FMG's land threshold is **`h >= 20` on a 0–100 `Uint8Array`, not
runtime-configurable**, so the whole water range is 20 units. The reasonable worry
is that this bounds what a `Mask -N` inland basin can carry. **It does not**, and
the reason is §9.2's ruling applied to the other side of the threshold.

The depth branch is `((h - 20) / h) * 50` metres, which is hyperbolic, not linear:

| `h` | depth |
|---|---|
| 19 | −2.6 m |
| 10 | −50 m |
| 5 | −150 m |
| 1 | −950 m |

Nineteen steps spanning nearly a kilometre, finest at the shoreline — ample for
any basin, and **irrelevant for the same reason the land half is: `h` is not our
elevation on the water side either.** Lake beds are cut by §3.1's carve from the
reach level, which is the one authority for where water is. FMG's water `h` gives
us the mask and nothing else.
