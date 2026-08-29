
ARCHIVED ARTEFACT of the lore session's worldmap pipeline. Copied verbatim
from the lore session's scratchpad; the body below is theirs and is not
edited here. See docs/design/WORLD_MAP.md 9.11.

==============================================================================

YAAN — the world map, six iterations, made in a local Azgaar FMG
==================================================================
Everything here is scratch. Nothing was written into the repository.

>>> THE FINAL MAP IS  iter6/final.png  (+ final.map, final-notes.txt). Read the
>>> ITERATION 6 section at the END of this file first; everything above it is the
>>> road to it, iteration by iteration, and each iteration's section says what the
>>> owner rejected, what was changed and what it cost to learn.
>>> index.html opens on iteration 6.

WHAT IS HERE
------------
  variant-{1,2,3}.png            political (states, borders, routes, burgs, labels)
  variant-{1,2,3}-cultures.png   culture regions = the namesbase belts
  variant-{1,2,3}-biomes.png     biome belts
  variant-{1,2,3}-relief.png     pure heightmap — the geography skeleton
  variant-{1,2,3}.map            FMG save files (4.1-4.7 MB), load in azgaar.github.io/Fantasy-Map-Generator
  variant-{1,2,3}-notes.txt      seed, settings, checklist 1-6 verdict, what failed
  variant-{1,2,3}-report.json    machine measurements (cultures, states, rivers, biome tallies, features)

  index.html     open this first — flips between the three variants and the four renders
                 (open with:  open .../scratchpad/worldmap/index.html ; the notes pane needs
                  a local http server, the images work from file:// directly)

  gen.mjs        the Playwright driver (node gen.mjs [--only N] [--probe])
  world.js       the authored geography, as an analytic FMG heightmap
  variants.js    the three parameter sets
  ../fmg/        FMG clone (v1.148.3) — `npm run dev -- --port 5199`

  Re-run:  cd ../fmg && npm run dev -- --port 5199 &
           cd ../worldmap && node gen.mjs

HOW IT WAS DRIVEN (for anyone repeating this)
---------------------------------------------
  * FMG is now a Vite/TypeScript app but the generation pipeline still lives in
    public/main.js as `generate()`. `?seed=X&width=&height=` boots it; `window.mapId`
    becoming defined is the "generation finished" signal (this is what FMG's own e2e
    tests wait on).
  * Options are locked by writing localStorage BEFORE the app boots: `randomizeOptions()`
    only randomises a setting when `stored(key)` is falsy. Playwright `addInitScript`
    is the right hook. Keys used: template, points, cultures, culturesSet, statesNumber,
    growthRate, manors, mapSize, latitude, longitude, prec, winds, temperature*,
    distanceScale, mapWidth, mapHeight.
  * `window.options` is NOT the app's `options` — main.js declares `let options`, which
    lives in the global LEXICAL scope, not on window. Inside page.evaluate use the bare
    identifier `options`. (window.options exists too and is a different, partial object —
    it has mapSize but no labels, which is a confusing way to fail.)
  * Generators are exposed on window (HeightmapGenerator, Cultures, States, Burgs, Names,
    Rivers, Lakes, Labels, Layers, Services). They are assigned at module load, so they can
    be intercepted with a `Object.defineProperty(window, name, {set})` hook installed in
    addInitScript before the app runs.
  * `Layers.set([...])` picks exactly the layers to draw; screenshot `#map`.
  * `await window.Services.Save.prepareMapData()` returns the .map file as a string —
    no download interception needed.
  * The heightmap is AUTHORED, not rolled. FMG's stock templates were NOT screened seed by
    seed — I read the template grammar (Hill/Pit/Range/Trough/Strait/Mask/Add/Multiply/Smooth,
    every op taking random count/height/x-range/y-range) and judged that steering it to
    "northern sea + enclosed southern sea with one strait + a wall with exactly one pass"
    is seed-hunting with no guarantee. Instead `HeightmapGenerator.generate` is replaced with
    an analytic function over grid.points: full control, still deterministic, and everything
    downstream (rivers, biomes, cultures, states, burgs, routes) is FMG's own machinery.
    Consequence to be aware of: these maps show what the DESIGN looks like when FMG dresses
    it, not what FMG proposes on its own. A "what do stock templates give" run is a separate,
    undone experiment.
  * Culture placement: `Cultures.getDefault()` can be overridden to supply exactly the
    cultures wanted, each with a `sort(cell)` scoring function — but `placeCenter` picks
    `sorted[biased(0, half, 5)]`, a biased RANDOM index, so the sort only nudges. The
    guarantee comes from two extra steps: relocate every `culture.center` onto its designed
    anchor between `Cultures.generate()` and `Cultures.expand()` (expand re-seeds from
    center), and then re-derive `culture.base` from where the centre actually landed and
    re-draw every burg/state/province/river name with `Names.getCulture`.
  * Label groups: a burg's label group is `burg.label.group` (set via
    `Labels.setGroup({type:"burg", entityId, group})`), NOT `burg.group`; and label groups
    have zoom.min/max, so at the whole-atlas zoom of 1 only the "capital" group is drawn
    until `options.labels.groups.find(g=>g.name==="city").zoom.min` is lowered.

THE SCALE PROBLEM — STATED, NOT SOLVED
--------------------------------------
These are ATLAS maps, deliberately, and they are NOT importable by the pipeline that
docs/design/WORLD_MAP.md specifies. The numbers:

  canvas                 1920 x 1080 px, distanceScale 3.5 km/px
  map extent             6720 x 3780 km  =  25.4 million km^2
  cellsDesired           10 000
  cell spacing           sqrt(25.4e6 / 10 000) = 50.4 km          <- measured, per §9.3's formula
  §9.3 REGION regime     cell spacing must be in [300 m, 693 m]   -> 50.4 km is 73-168x too coarse
  §9.3 configuration     at 10k cells the map must be 30x30 .. 69x69 km; 60x60 puts the cell
                         exactly on POLYGON_SPACING = 600 m
  the game world         10 x 10 km = 100 km^2. One FMG cell here is 2540 km^2, so the WHOLE
                         target world is about 1/25 of ONE CELL — 2.9 px on these images.
                         (§9.3 quotes 1/16 of one cell at FMG's own continental defaults; same
                         order, and the same conclusion.)

So per §9.3 an importer MUST reject these files with E9', and that is correct behaviour,
not a bug in the maps. The design question that is OPEN and that I am not deciding:

  the world the owner described (an empire, two seas, an overseas power, a Bosphorus)
  is a CONTINENT. The engine's world is 10x10 km. Those are not the same object, and
  §9.3 already names the two regimes without saying how they compose.

  Three ways it could be closed, none of them chosen here:
   (a) TWO EXPORTS, ONE FICTION. This atlas stays ATLAS-regime: it supplies fiction and
       context only (which state, which culture, which biome, nearest burg) and no geometry;
       a SECOND FMG map at 60x60 km is authored for the province the game is actually set in,
       and the DFN_ORIGIN marker lives on THAT one. The atlas would then need a stated rule
       for where the 60x60 km window sits on it — which is exactly the thing a marker on the
       atlas cannot express, because 60 km is 17 px here.
   (b) THE ATLAS IS THE ONLY MAP and the engine's world grows by orders of magnitude. Not
       plausible: §1.1's ruling is 10x10 km with the LOD ladder already computed for it.
   (c) THE ATLAS IS NOT AN FMG MAP AT ALL — it is a drawn picture for the map screen, and
       FMG is used only in REGION regime. Cheapest, and it makes the atlas non-authoritative
       for anything the importer reads.

  I did NOT place a DFN_ORIGIN marker on any of the three. §9.4 says the marker is how the
  crop is chosen and that a missing one is a hard error — but §9.4 also says "in ATLAS regime
  there is no crop to choose, because there is no geometry to crop." Putting a marker here
  would make an atlas export look importable. That is the silently-nearby-world failure the
  whole document is written against, so the marker is deliberately absent and this paragraph
  is the reason.

FMG CONVENTIONS ALREADY IN THE REPO THAT A CLEAN MAP MUST HONOUR
-----------------------------------------------------------------
All from docs/design/WORLD_MAP.md §9 (the FMG amendment) unless noted.
There is NO tools/fmg_import.py yet — §9.6 names it as the heir of tools/sketch_compile.py,
and nothing in tools/ touches FMG today. So every convention below is currently paper.

 1. §9.3  TWO REGIMES, and conflating them is the named failure mode. ATLAS = fiction and
          context, zero geometry. REGION = 30-69 km at 10k cells, FMG cells ARE our polygons.
          The importer computes sqrt(area/cellsDesired) from the export's own metadata and
          hard-fails outside [300, 693] m — error E9'.
 2. §9.4  The crop is chosen IN FMG by a marker named DFN_ORIGIN. Exactly one. Missing = hard
          error, duplicated = hard error naming both. No centre-of-map guess, ever. The
          resolved lon/lat is RECORDED into the compiled artifact's INFO section, not re-decided.
 3. §9.2  FMG's `h` is NEVER our elevation. It gives exactly two things: the land/water mask at
          h >= 20, and the mountain region at a threshold read from the export. `heightExponent`
          is carried in the JSON export and must be READ, never assumed. Elevation comes from
          §2.6's drainage construction. (Our 0-400 m range is h in [20,38] — nineteen steps.)
 4. §9.6  An unmapped attribute id — biome, lake type, route type, burg type — is an ERROR
          (E1'), never a nearest match. This is called out as the single most likely place for
          a silently-nearby world, because a nearest-biome fallback is the obvious thing to
          write and would never go red.
 5. §9.6  Burg <-> site bijection is checked BOTH ways (E6'); river topology is checked, not
          trusted (E2-E4); water gaining height downstream is E5 and is called the most
          valuable check we have because FMG's datum is not ours.
 6. §9.5  FMG OWNS THE GRAPH, WE OWN THE GEOMETRY. Burgs are authoritative for which places
          exist and their names/population/port status; the P4 scorer only chooses the pad
          inside the cell. Routes are authoritative for which burg connects to which; our
          generator owns the polyline and cross-section. Biome id selects the recipe; the
          continuous moisture field stays ours.
 7. §9.9  LATITUDE IS AN AUTHORING INPUT, and generalised: FMG DERIVES MORE THAN IT DRAWS.
          Lake type (fresh/salt/dry), biome, port status and population are all computed from
          settings, so any map whose fiction leans on one must STATE THE SETTINGS that produce
          it. Hence every notes file here records mapSize/latitude/longitude -> latN/latS,
          temperatures, prec and winds. (These three maps have ZERO lakes, so no salt/dry-lake
          claim is being made; but all three do lean on port status, which is derived —
          see §5.3a.)
 8. §5.3a Instrument D: every DECLARED derived assertion is checked against the export, exact,
          and ZERO DECLARED ASSERTIONS IS A FAILURE. Two failure modes kept separate:
          contradiction, and UNEXPRESSIBLE. A clean Yaan map must therefore declare, in its
          sidecar, the things it is claiming — "Crownhaven is a port", "Saltcliff is a port",
          "the inner sea has exactly one outlet" — and each must be readable back.
 9. §2.4  The sidecar carries origin_x_m / origin_z_m in ABSOLUTE WORLD METRES (§1.1), never
          normalised to the current extent; metres_per_px; a declared `landforms[]` list that a
          cell resolving outside is E7; `rivers[].mouth_px` + a `navigable` CLAIM that is
          checked against drainage (E8), not a switch.
10. §2.5  NO SILENT FALLBACK, EVER. If the map does not compile, no world is generated at all.
          §9.6 marks this as "unchanged and more important" under FMG.
11. §9.1  Licence checked: MIT with an explicit commercial-derivatives expansion covering
          "created maps, map images, screenshots". We may ship worlds made with it. This clone
          is v1.148.3 and its LICENSE is unchanged.
12. §9.8  Two things still OPEN in the doc that these runs happen to answer in part:
          - "does a REGION-regime export assign states/cultures/burgs sensibly?" — not answered
            here, these are ATLAS.
          - "do FMG's downstream modules assume an ocean exists?" — partially: all three maps
            have oceans, so the endorheic question is still untested.
          These runs DO add one data point the doc did not have, stated with its caveat:
          an enclosed sea is classified as a LAKE, not an ocean, until its outlet actually
          connects — and on the first attempt it did not. One FMG cell here is 14.4 px
          (sqrt(1920*1080/10000)); the first strait was ~2 cells wide and the sea came back
          as a freshwater lake with 0 ocean cells in it. Widening the channel to ~4-5 cells
          fixed it. HONEST CAVEAT: a second bug was fixed in the same edit (the channel's
          taper did not reach the sea's own shore), so "2 cells is too narrow" is NOT
          isolated — what IS established is that the lake/ocean classification is a hard,
          silent switch that changes lake type, port eligibility and salinity, and that at
          atlas resolution a real Bosphorus (~1 km) is far below one cell. Anything the
          fiction hangs on "the strait is narrow" is a REGION-regime fact, not an atlas fact.

NAMING / OWNERSHIP FLAG
-----------------------
docs/story/WORLD_MAPS.md already owns six named example worlds (Farness, Vaelmere,
Seremarch, Sedgewend and two more), and §11 there recommends building Farness first.
"Yaan" is a new, larger object and does not collide with any of them, but the relationship
between "the world" (this atlas) and "the six example maps" (regional, and the acceptance
set for the importer) is undecided and belongs to mapstory + mapdesign, not to this scratch.

==========================================================================
ITERATION 2 — iter2/ ; iteration 1 (everything above) was REJECTED
==========================================================================
Owner's verdict on the three maps above: "too rectangular, too unnatural."
He was right; see the political render of variant 1 — the Solrech/Yaan border is a
ruled vertical line at u=0.155 and the eastern frontier another at u=0.86, because
iter1's driver assigned cells to powers with a RECTANGULAR region() test.

  iter2/world2.js      the rebuilt analytic heightmap (shear + warp + fractal coasts +
                       spline ridges + island clusters + named site terrain)
  iter2/variants2.js   the three parameter sets, the 13 site anchors, the 12 city specs
  iter2/gen2.mjs       the driver: node gen2.mjs [--only N] [--probe]
  iter2/preview.mjs    rasterises the field WITHOUT FMG (2 s per variant instead of 3 min)
                       and MEASURES the acceptance criterion: the longest coastal run that
                       stays within `tol` of its own chord. Writes preview-N.png with the
                       six worst straights painted red.
  iter2/baseline1.mjs  the same instrument pointed at iteration 1, for the comparison
  iter2/mknotes.mjs    writes the notes files from the reports, so the numbers cannot drift

THE THREE THINGS THAT COST THE MOST TO LEARN
--------------------------------------------
 1. A 2-D DOMAIN WARP CANNOT CARRY THE BIG IRREGULARITY. Its shear is about a1*f1*2;
    at the amplitude the brief asks for (5-10% of map width) and any useful frequency
    that is O(1), and the inner sea comes apart into an archipelago. Even a gentle one
    displaces the north coast and the sea's shore by different amounts at the same x,
    so it changes BAND WIDTHS — and a 255 px sea between two coasts cannot absorb that.
 2. A SHEAR CAN. y -= B(x) and x += C(y) are triangular maps with unit Jacobian:
    invertible at any frequency, so they bend the whole band stack coherently and can
    never close a strait. All the high-frequency bending now lives there.
 3. A BAND MUST BE ~2.5x THE SUM OF ITS TWO COASTS' NOISE BUDGETS or it fragments.
    The southland was 173 px against two coasts of ~0.10 units each and came back as
    three islands. It is 244 px now.

WHAT IS STILL NOT MET
---------------------
 * the ~200 km straight-line target, on the analytic field: 390-407 km at 7 km
   tolerance (iteration 1: 1481-2268 km). By eye on the FMG render, where the coast
   follows 61 km Voronoi cells, no ruled line survives — but the number is the number.
 * the 15-25 degree tilt is carried by the mountain axis only. An 18 degree tilt of a
   full-width east-west feature on a 1920x1600 canvas eats 45% of the map height and
   evicts the climate bands, which FMG computes from v and which are not ours to move.

==========================================================================
ITERATION 3 — iter3/ ; iteration 2 was APPROVED ON STYLE, this is its heir
==========================================================================
Owner's verdict on iteration 2: "выглядит супер лучше", favourite Yaan III (seed yaan2-9158).
He then asked for six STRUCTURAL edits. Iteration 3 is Yaan III's skeleton plus those six,
and nothing else was touched: the shear still carries the large-scale irregularity, the
coasts are still fractal with ridged fjords north and limans south, the culture belts are
the same four, the strait still leaves the inner sea at its EASTERN end, the twelve city
passports and the Harrowmere marker are unchanged.

  iter3/world3.js      the heightmap: iter2's world2.js plus the polar slab, the ice-cap
                       elevation model, a second spline ridge (the Skyrim massif), LONE
                       MOUNTAINS, closed lake BASINS, and disputed-landform LOBES
  iter3/variants3.js   three parameter sets, 16 site anchors, the 9 powers, the 6 disputes
  iter3/gen3.mjs       the driver:  node gen3.mjs [--only N] [--probe] [--port 5200]
  iter3/preview3.mjs   the field WITHOUT FMG (5 s), now painted by PREDICTED CLIMATE CLASS
                       and reporting water topology as a BAND CENSUS
  iter3/mknotes3.mjs   writes the notes files from the reports, so no number can drift

  Re-run:  cd ../fmg && npm run dev -- --port 5200 &
           cd ../worldmap/iter3 && node preview3.mjs && node gen3.mjs && node mknotes3.mjs

THE SIX EDITS, AND WHAT EACH COST
---------------------------------
1. SQUARE CANVAS 1600 x 1600 (A = 1.0). Not cosmetic: FMG derives temperature from v alone,
   so vertical room IS latitude, and the 0.167 units of map width gained is the entire
   budget for edit 2. The empire's own band is still 0.233 units — the part the owner liked
   did not shrink.
2. THE GREAT ICE is a SECOND SLAB in `landness`, unioned with the continent by a smooth MAX,
   with the cold gulf between them and ONE land bridge welding them (a gaussian on the ice
   coast that pushes it past the continent's north coast). The bridge is what keeps the gulf
   an honest gulf of the world ocean: FMG calls a water body an OCEAN iff it touches a map
   border and a freshwater LAKE otherwise, so a fully enclosed northern sea would have been
   the iteration-1 strait failure all over again. Variant 3 puts the bridge in the MIDDLE and
   gets TWIN gulfs, one reaching the west border and one the east — both still oceans, and
   preview3.mjs checks that before FMG is started.
3. HIGH MOUNTAINS. A lone mountain is an ellipse with profile (1-r)^p at p ~ 1.9: the top
   15% of the radius carries half the height, which is the silhouette of one mountain out of
   a plain rather than of a hill field. At heightExponent 1.8, h 100 is ~2790 m, and every
   peak here is authored to clear h 88. All three come out GLACIER-capped, which is FMG's own
   arithmetic (6.5 C per km of altitude) and not a setting.
4. NINE POWERS, and the two things that cost the most to learn:
   a. FMG's `createStates()` writes `{i: burg.i}` while `expandStates()` reads `states[s]`.
      Those agree ONLY because every capital burg is created before every town, so capitals
      occupy ids 1..N contiguously. Adding a burg and flagging it capital makes states[s]
      undefined and the generator throws. The fix is to RELOCATE one of FMG's own capitals
      to each power's seat — id order untouched.
   b. A STATE IS ROUGHLY THE SIZE OF ITS CAPITAL'S CULTURE REGION. expandStates charges -9
      inside the capital's culture and +100 outside it, so a khanate seeded on a Ruthenian
      cell becomes a Ruthenian state and eats the empire's centre. It did, twice. Every seat
      is now required to stand in its own cultural belt.
   Sizes are set BEFORE growth (expansionism and movement type written onto each FMG state),
   not by reassigning cells afterwards — so every border on the political map is still a
   border FMG grew along the terrain.
5. DISPUTED TERRITORIES are AUTHORED AS GEOMETRY (lobes of land grafted onto a coast, one
   deliberately large island) and then MEASURED, never drawn: the driver counts how many
   powers hold land inside each named landform and prints the answer whichever way it came
   out, plus a blind sweep of every island feature that ended up shared.
6. LAKES. Iteration 2 got ZERO. The reason: FMG's addLakesInDeepDepressions floods OUT from a
   local minimum through every cell below floor + lakeElevationLimit and cancels the lake the
   moment it reaches the sea; iteration 2 authored a bowl inside a plateau whose outer slope
   ran straight down to the steppe, so such a path always existed. The new site kind `basin`
   builds the RIM explicitly — a closed annulus at floor+rim — and overrides the terrain
   noise inside it. Also: a basin must not sit within ~0.03 map widths of a coast, or the
   coastline cuts the rim.
   STATE LABEL CLIPPING is also solved, and the cause was ours: iteration 2's driver scaled
   every label group's font-size x2.1 to make burg names readable, and the state group went
   with it. FMG fits a state label to a textPath, and SVG silently drops the glyphs that fall
   off a textPath's ends — that is the whole of "rngeng ingdo". The state group is now
   excluded from the scaling and every state label is measured against its own path length
   and fitted to ~58% of it.

WHAT ITERATION 3 STILL DOES NOT DO
----------------------------------
 * the ~200 km straight-line target is still not met on the analytic field (see each notes
   file for the number). Iterations 1 and 2 were RE-MEASURED at iteration 3's raster scale so
   the three columns in index.html are the same measurement, not three different ones.
 * the 15-25 degree tilt is still carried by the mountain axes only, for the same reason as
   iteration 2 and now a stronger one: on a SQUARE canvas an 18 degree tilt of a full-width
   east-west feature eats 32% of the map height and evicts the climate bands.
 * these are still ATLAS-regime maps and an importer must still reject them (§9.3). No
   DFN_ORIGIN marker is placed. Everything the iteration-1 README argues about the two
   regimes stands unchanged.
 * a NEW open question this iteration raises: the Great Ice is ~32% of the land and belongs
   to nobody. Whether an atlas should show a third of itself as uninhabited is a design
   decision for mapstory, not one this scratch should make.

==========================================================================
ITERATION 4 — iter4/ ; iteration 3 was ACCEPTED WITH ONE CORRECTION
==========================================================================
Owner's verdict on iteration 3:
  "зимняя часть слишком всё занимает сверху; она не должна быть бесконечным куском —
   только частью материков, а дальше море; по краям ВСЕХ материков — море, не горы и
   не льды."

Iteration 4 is iteration 3 with exactly two things changed and nothing else touched.
Its base skeleton is iteration 3's VARIANT 1 (seed yaan3-3141), the one that was
recommended; variant 2 of this iteration is the HYBRID the owner was offered —
iteration 3 variant 3's coast geometry (yaan3-1618) carrying variant 1's politics.

  iter4/world4.js      the heightmap: iter3's world3.js minus the polar slab, plus the
                       OCEAN FRAME, the horizontal squeeze, `northCoast.horns` (the ice
                       capes) and `iceCaps` (the domes on them)
  iter4/variants4.js   three parameter sets; the ONE place the design->screen squeeze is
                       applied to power anchors, disputes and culture centres
  iter4/gen4.mjs       the driver:  node gen4.mjs [--only N] [--probe] [--port 5201]
  iter4/preview4.mjs   the field WITHOUT FMG (6 s), now also reporting EDGE CLEARANCE per
                       border, the LANDMASS CENSUS and the glacier share
  iter4/mknotes4.mjs   writes the notes files from the reports, so no number can drift

  Re-run:  cd ../fmg && npm run dev -- --port 5201 &
           cd ../worldmap/iter4 && node preview4.mjs && node gen4.mjs && node mknotes4.mjs

RESULT, IN THE THREE NUMBERS THE CORRECTION WAS ABOUT
------------------------------------------------------
                              iter 3            iter 4 v1 / v2 / v3
  glacier, % of the land      32.3 / 31.6 / 33.3   8.6 /  9.8 / 12.3
  nearest land to a border    0.000 (touching)     0.118 / 0.091 / 0.114 map widths
  landmasses >= 2% of land    1                    1 / 2 / 4

WHAT THE TWO EDITS COST TO LEARN
---------------------------------
1. A MARGIN MEASURED IN DESIGN SPACE IS NOT A MARGIN. The shear and the warp move a
   design point by up to 11% of the map width, so the frame has to be subtracted LAST
   and in SCREEN coordinates. It is written  m0 + amp*u + detail  with u in [0,1] and
   detail >= 0 — the noise may only ADD water — so m0 is a hard floor no seed can break.
   Consequence: the frame has BAYS but no CAPES; a cape would be a hole in the floor.
2. AND A CLIP IS NOT A DESIGN. Clipping the iteration-3 world at 10% would have cut the
   German march down to a splinter and merged the inner sea with the border ocean. The
   world is SQUEEZED horizontally into [0.075, 0.925] instead (y is never squeezed: FMG
   derives temperature from v alone, so vertical room is latitude). Everything the
   driver compares against cells.p/W — power anchors, dispute windows, culture centres —
   is therefore in SCREEN units while the geography is in DESIGN units, and variants4.js
   converts in exactly one place.
3. THE ICE HAD TO CHANGE MECHANISM, NOT SIZE. Iteration 3 made ice out of LATITUDE.
   At the latitude a peninsula can reach, FMG gives TUNDRA:
       seaLevelTemp(v) = -11.64 + 32.42 v, and the biome test is trunc(temp) < -5,
       i.e. temp < -6 C  ->  glacier by latitude alone only north of v = 0.174.
   trunc(-5.3) is -5, so "-5.3 C" is not a glacier cell — the first pass of iteration 4
   lost a whole degree to that and came back with 5.2% glacier. The ice is now made of
   ALTITUDE: explicit elliptical DOMES (6.5 C per km) on gaussian peninsulas. Each dome
   is zero at its own margin and grows inward, which is what leaves the TUNDRA RING under
   it — and that ring is what Rimehold's passport stands on.
4. A GAUSSIAN FLANK IS A STRAIGHT LINE TO THE INSTRUMENT. The first ice capes put the
   longest straight run of the whole map (406 km) on a cape's own flank, and the first
   ocean frame put a 1170 km ruled line down the western shore because its margin noise
   was a plain 0.5+0.5*sfbm, which almost never leaves [0.35, 0.65] — 1.7% of the map
   width of variation over the whole edge. Both now carry a gain and their own
   high-frequency fractal. preview4.mjs is what caught both.
5. THE STRAIT MUST BE CARVED AFTER THE ISLANDS. In iteration 3 it was cut before them,
   and an island blob of the inner sea's archipelago landed on the strait's northern
   mouth and overwrote the channel: variant 3 came back with a 41 745 px freshwater LAKE
   where the inner sea should be. A strait has to be able to cut through an island chain.
6. WITH THE FRAME, THE WHOLE WORLD OCEAN IS ONE FMG FEATURE. Iteration 3 could name "the
   northern sea" and "the inner sea" by ocean feature id, and ranking features by mean
   latitude answered the question. Iteration 4 cannot: the driver now asks a harbour's
   HAVEN CELL for its own design latitude. Left as a feature test both Crownhaven and
   Saltcliff fall silently through to "any harbour at all" — tier 3 — and the report
   would say tier 3 without saying why.
7. THE EMPIRE'S ASSIGNMENT WEIGHT WAS PAID FOR BY THE ICE. Iteration 3 could give Yaan a
   weight of 4.2 because a third of the land was unclaimable glacier and the konungdoms
   owned a polar continent. With the ice cut to caps there is no such reserve: at 4.2 the
   empire took 45-57% of the land and both konungdoms came out under 2%. The weight is
   2.0 in iteration 4, the expansionism is unchanged, and no border was moved by hand.
8. FMG'S STATE EXPANSION IS CHAOTIC IN THE HEIGHTMAP. Raising the Barrowstep pan by six
   height units and tightening its rim — a change 0.05 map widths across — moved every
   political border on all three variants (the empire went 31.9% -> 46.4% on variant 1).
   Anything tuned in the politics has to be re-measured after ANY terrain edit.

WHAT ITERATION 4 STILL DOES NOT DO
-----------------------------------
 * THE BARROWSTEP PAN IS FRESHWATER on all three variants; the passport asks for a SALT
   lake. FMG's rule is src/generators/features.ts:361 —
       no outlet AND evaporation > flux -> "salt"   (evaporation > 4*flux -> "dry")
   evaporation rises with the lake's own altitude, flux is what the catchment delivers.
   A higher, tighter pan was tried, stayed fresh, and cost the political balance (see 8),
   so iteration 3's basin geometry is kept and the lever is written down instead of spent.
   Iteration 3 got a salt pan on ONE of its three variants: this is a coin the pipeline
   flips, not a mechanism it lacks.
 * VARIANT 3 has no Barrowstep lake at all (nearest lake 0.132 map widths away).
 * THE SOUTHLAND HANGS OFF THE MAIN CONTINENT BY THE WESTERN ISTHMUS, deliberately. The
   brief asks both for an overseas southern continent AND for the inner sea to keep its
   single eastern strait; a sea with land on its west and one mouth in the east is what
   an isthmus produces. Cutting it gives a true island continent and a second mouth. One
   line moves it either way: geo.seaWest.x in variants4.js.
 * the ~200 km straight-line target is still not met on the analytic field, and iteration
   4 is slightly WORSE than its own parents at the tightest tolerance (327/418/376 km
   against iteration 3's 290/347/256), because the frame's shore and the capes' flanks
   are two new coastlines that are smooth by construction. Every notes file carries the
   comparison against its OWN iteration-3 parent, measured by the same code at the same
   raster scale.
 * the 15-25 degree tilt is still carried by the mountain axes only, for iteration 3's
   reason: on a square canvas an 18 degree tilt of a full-width east-west feature eats
   32% of the map height and evicts the climate bands.
 * these are still ATLAS-regime maps and an importer must still reject them (§9.3). No
   DFN_ORIGIN marker is placed. Everything the iteration-1 README argues about the two
   regimes stands unchanged.

==========================================================================
ITERATION 5 — iter5/ ; THE FINAL MAP. Iteration 4 variant 2 was APPROVED
==========================================================================
The owner approved iteration 4's VARIANT 2 — "Yaan VIII, the Sundered Ice", the hybrid
(coasts of iteration 3's variant 3, seed yaan3-1618; politics of variant 1) — and asked
for exactly two things, with the geometry explicitly frozen: NAMES and SETTLEMENTS.

  iter5/gen5.mjs       the driver. It IMPORTS iter4/world4.js and iter4/variants4.js
                       unchanged and runs ONE variant. It cannot move a coast: it
                       replaces the NAME FIELDS of the nine powers and edits burgs after
                       every generator has already run.
  iter5/loadtest.mjs   boots a clean FMG, loads final.map through the app's own upload
                       path and reads the names back off the loaded map
  iter5/mknotes5.py    writes final-notes.txt out of final-report.json
  iter5/final*.png     political / relief / biomes / cultures
  iter5/final.map      the FMG save
  iter5/final-notes.txt  every rename as a was->now table, and the settlement decisions

  Re-run:  cd ../fmg && npm run dev -- --port 5202 &
           cd ../worldmap/iter5 && node gen5.mjs --port 5202 && node loadtest.mjs --port 5202
           python3 mknotes5.py

THE PROOF THE GEOMETRY DID NOT MOVE, AND WHY IT IS THE RIGHT ONE
----------------------------------------------------------------
final-relief.png is BYTE-IDENTICAL to iter4/variant-2-relief.png (same md5). The relief
render is the heightmap, the coastline, every river and every lake with no politics and
no labels on it, so one hash covers the whole of "the geometry did not change" — better
than any eyeball comparison of two political maps. On top of that every power holds
exactly the same cell count as in iteration 4, so no border moved either.
The mechanism that makes this cheap: postPass runs AFTER every FMG generator, and the
renaming block runs after postPass's own city placement. Nothing iteration 5 does feeds
back into a generator.

WHAT THE FIVE THINGS COST TO LEARN
-----------------------------------
1. A RENAME CAN MOVE THE MAP, THROUGH THE RNG. Names.getCulture() draws from FMG's own
   seeded RNG, and Burgs.add / Burgs.defineFeatures in postPass section 3 draw from the
   same stream. A name sweep placed before section 3 silently changes the twelve cities'
   walls, citadels and temples. The whole iteration-5 block therefore sits after every
   generative step. (This is the same class of bug as iteration 4's "any terrain edit
   re-rolls the politics", one layer down.)
2. "THREE CONSONANTS IN A ROW" IS NOT A GIBBERISH TEST. The first cut of the sweep
   re-rolled 28 burgs and 29 rivers and was wrong: it flagged Lubutsk, Snerensk,
   Hotvinsk, Serniversk (the ordinary Russian -sk/-tsk ending) and Dornstalden (ordinary
   German), and replaced them with other rolls from the SAME namesbase — churn, not
   improvement. The rule that survived asks whether a cluster is pronounceable: digraphs
   collapse, h/w/y are glides (or "Crownhaven" itself trips it), word-initial clusters
   and known word-final endings are exempt, and what is left must have a liquid or nasal
   to lean on. It catches 4 rivers and 1 burg out of 212 rivers and 238 burgs.
3. FMG HAS NO SEA. LABEL_TYPES is state/province/burg/river/route/added: an ocean is a
   `feature` and features carry no label, and there is no culture label either. Both the
   four sea names and (for the culture render only) the twelve people names are ADDED
   LABELS — free-standing map objects whose only content is their text. The sea labels
   are anchored by FMG's own distance-from-coast field cells.t so they land in open
   water; the culture labels are added for one render and removed again, because a
   people is the `cultures` array and not a piece of type.
4. STYLE LIVES IN TWO PLACES AND ONLY ONE OF THEM SURVIVES A SAVE. Burg ICON groups are
   re-read from the DOM into `style` on every redraw (createIconGroups does it), so
   writing icon sizes onto the DOM after a draw sticks — and writing them into `style`
   BEFORE a draw does not, because the draw overwrites them from the DOM's defaults.
   LABEL groups are the other way round: renderLabelGroups builds them from
   style.labels.groups every time, so a file saved with label sizes only on the DOM
   opens with the twelve cities' names back at FMG's default 5-6%. Found by loading the
   file back, not by reading the code. Both are written now, and the file is saved with
   the political layers on so it opens as the map that was approved.
5. BIGGER TYPE MEANS COLLISIONS, AND COLLISIONS MUST BE MEASURED. Doubling the city type
   put Highbarrow across "Yaan", Fenholm across "Westmark" and Barrowstep across
   "Kaisak". Every city label's box is now tested against every state label, every sea
   label and every city label already placed, and a colliding one is moved by the
   smallest offset that clears everything; the offset is written onto burg.label.dx/dy
   so it lives in the .map too. 5 of 12 move. This is the same failure as iteration 2's
   clipped state labels wearing a different hat: type that is drawn but not legible.

WHAT ITERATION 5 DOES NOT DO
-----------------------------
 * the twelve cities keep their ENGLISH names — the Russian renaming is a separate
   decision the owner has not made;
 * the Barrowstep pan is still FRESHWATER (iteration 4's open item);
 * the southland still hangs off the main continent by the western isthmus, deliberately;
 * the Aigerots came out of FMG as a TWO-CELL culture, so the Archonate of Aigeros is
   pinned to that culture by the driver rather than being grown into the archipelago —
   growing it would be a change to the map, not to its names;
 * this is still an ATLAS-regime map: ~56 km cells against the REGION regime's 300-693 m,
   so per WORLD_MAP.md §9.3 an importer must reject it, and no DFN_ORIGIN marker is
   placed. Everything iteration 1's README argues about the two regimes stands.

==========================================================================
ITERATION 6 — iter6/ ; THE FINAL MAP. One geometric edit: THE VOLKHONA
==========================================================================
The owner asked for a GREAT RIVER joining Crownhaven (the capital, on the northern sea) and
Saltcliff (the southern shore), rising at one northern node with a short sister running north
into the capital's bay, and a PORTAGE labelled between the two heads — the classical answer to
"rivers do not cross a watershed". Everything else of iteration 5 was to be kept, and THE
COASTLINE WAS NOT TO MOVE.

  iter6/world6.js      iteration 4's world4.js plus ONE thing: `channelCarve`, which cuts a
                       river valley from a polyline carrying an explicit thalweg profile
  iter6/variants6.js   the four channels — the route itself, and the one politics correction
  iter6/gen6.mjs       the driver:  node gen6.mjs [--port 5203]
  iter6/preview6.mjs   BOTH fields rasterised without FMG (12 s) and the four checks:
                       coastline diff, prec-weighted D8 drainage, mouth ranking, the portage
  iter6/flow.mjs       the drainage instrument: FMG's discharge is the SUM OF PRECIPITATION
                       over the catchment, not its area, so the D8 accumulation is weighted by
                       the real prec field read out of iter5/final.map
  iter6/coastcheck.py  the coastline constraint checked on FMG's OWN grid, iter5 vs iter6
  iter6/grid.mjs / grid6.mjs / corridor.mjs / probe.mjs / render.mjs
                       the survey instruments the route was chosen with — height dumps, the
                       land corridor between the divide and the sea, and where a drop goes
  iter6/final*         the map, the save, the notes, the machine-readable report

  Re-run:  cd ../fmg && npm run dev -- --port 5203 &
           cd ../worldmap/iter6 && node preview6.mjs --png && node gen6.mjs --port 5203
           node loadtest.mjs --port 5203 && python3 mknotes6.py

WHAT CAME OUT
-------------
  Volkhona     discharge 2406, length 1028, width at the mouth 1.13 km — THE LARGEST AND THE
               LONGEST RIVER ON THE MAP; the next is Gutosch 1077. Mouth 1.6 cells (92 km) from
               Saltcliff. Source: the Tollgard pass, u 0.229 v 0.452.
  Yaren        discharge 85, 3 cells, ON CROWNHAVEN'S OWN CELL — iteration 5's known defect
               ("the capital has no river, and Yaren hangs off somebody else's mouth 291 km
               away") is fixed, and Crownhaven's passport goes tier 2 -> tier 1.
  The Tsar's Portage / Государев волок — an added label and a marker at u 0.592 v 0.459:
               Yaren's bed h 28, the saddle h 30, Volkhona's bed h 24, carry 247 km.
  COASTLINE    0 of 10 000 grid cells changed side, 0 raised, 259 lowered. The pack graph came
               back identical: 6036 cells, 4319 of them land, same ocean frame to three
               decimals. See iter6/final-notes.txt for how that is guaranteed and measured.

THE FIVE THINGS THAT COST THE MOST TO LEARN
-------------------------------------------
1. A CARVE THAT CAN ONLY LOWER LAND CANNOT MOVE A COAST, AND THAT IS THE WHOLE DESIGN.
   h_new = min(h, max(target, 21)), inside the land branch only. FMG's land threshold is 20,
   so the set {h >= 20} is untouched by construction — no seed, no route and no depth can
   break it. Everything else in this iteration is free to be tuned because of that one line.
   It also makes the check cheap: both maps save the SAME 100x100 grid, so grid.cells.h is
   comparable cell for cell, and "no cell crossed 20" is a stronger statement than any
   comparison of two coastline pictures.

2. DISCHARGE IS RAINFALL, NOT AREA — AND THAT DECIDED THE ROUTE.
   drainWater adds grid.cells.prec to every land cell and pours it downhill, so a river's size
   is decided by WHICH cells it drains. The Yaan interior is arid: prec 0-5 over the whole
   Venedian centre against 19-31 in the western marches. The first honest route — down the
   northern plain into Crownhaven's own gulf, the largest catchment on the map — came back
   SECOND (1676 against 1720) because it drained the dry half of the continent. What made the
   Volkhona first was reaching one channel west through the Tollgard pass, the single gap in
   the western wall, into the wet marches: the runner-up fell from 1720 to 1077 and the
   Volkhona rose to 2406 in the same edit.

3. THE BANK NOISE WAS ON THE FLOOR, AND AN INTEGER HEIGHTMAP HAS NO GENTLE GRADIENTS.
   Two bugs of the same family, both invisible in a picture and both caught by the drainage
   trace. The roughness that keeps a trench from reading as a ruled line was weighted (1-w),
   which is 1 ON THE THALWEG: heights are integers and the floor only falls 0.2 per vertex, so
   +-1.1 of noise turned a monotonic thalweg into a chain of puddles. And even with a clean
   floor, six height units spread evenly over 0.45 map widths ROUND to flat runs ten cells
   long — and on a flat run FMG's own tiebreak (alterHeights adds distance-from-coast/100)
   sends the water to the NEAREST coast, which is the one the river was being routed away
   from. Floors are now chosen so the ROUNDED sequence steps down, with the steps placed where
   they are needed rather than evenly.

4. A LAKE IS A DEPRESSION THAT CANNOT POUR OUT THROUGH CELLS BELOW ITS FLOOR PLUS SIX.
   main.js:534. The Barrowstep salt pan sits at h 30-31 with a western rim of only 37-40, so
   the steppe tributary — whose head was then four cells away at u .728, cut to h 25 — opened
   a sub-36 path out of it and the lake simply never formed. Three grid cells came back land
   instead of lake, and coastcheck.py named them. Pulling the head back to u .706 and
   narrowing the channel put it back. THE LESSON IS THE RADIUS: a trench does not have to
   touch a basin to drain it, it only has to come within its spill threshold.

5. WHERE A RIVER "STARTS" IS NOT WHERE ITS VALLEY STARTS, AND THE PORTAGE PAID FOR IT.
   FMG draws a river only from the cell where flux first reaches 30 (MIN_FLUX_TO_FORM_RIVER),
   and the Yaren drains five arid cells — so its drawn source is three cells from its own
   mouth, out on the rim of the fjord. Measuring the carry from that point sent it across the
   gulf's shoulder: 322 km over h 48, seven hundred metres, which is a mountain crossing and
   not a portage. The driver now searches over the two CARVED VALLEYS — every pair of points
   within 0.09 map widths, the line walked cell by cell, the pair whose highest cell is lowest
   wins — and the answer is 247 km over a saddle two units above one bed and six above the
   other. Same geometry, different question.

WHAT ITERATION 6 DOES NOT DO
-----------------------------
 * STONEFORD IS NOT ON THE VOLKHONA. Its passport is "the bend of a big navigable river" and
   the owner asked for the river to be taken through it if it could be. The Venedian arm IS
   carved through Stoneford and Stoneford does stand on a big river (fl 249, passport tier 0),
   but that river is the Slavitsa. The reason is measured: the Venedian plain lies at h 26-28
   and the Midday Sea is TWO CELLS south of it, so its outlet stands at h 20-22 within two
   cells of any axis one could carve there, and a channel that captured the whole plain would
   have to run below the sea it is avoiding. Half of it (531 of 867) was captured anyway.
 * THE BORDERS MOVED. Iteration 4 already wrote down that FMG's state expansion is chaotic in
   the heightmap; iteration 6 lowers 259 grid cells. The first run gave the empire 42% of the
   land, so its expansionism was turned back down (4.2 -> 2.6, the same lever, applied BEFORE
   FMG grows anything — not one cell reassigned by hand) and it landed at 31.3% against
   iteration 5's 24.5%. Yaan is still the first power and its seat is inside it. SEVEN of the
   nine powers are outside two percentage points — Westmark, the Kaisak Horde and the Seferid
   Sultanate lost, Oberfels, Aigeros, Vestrskjold and the Empire gained. THIS IS THE ONE PLACE
   THE ITERATION FAILS THE LETTER OF THE BRIEF, which asked for borders within a couple of
   cells. Full table in iter6/final-notes.txt.
 * The Barrowstep pan is still FRESHWATER — iteration 4's open item, untouched by decision.
 * Crownhaven is passport tier 1, not tier 0: it has its river now, but the mouth cell is a
   flat h-20 shore cell and the passport also asks for local relief >= 6.
 * The twelve cities keep their ENGLISH names — still the owner's undecided question.
 * This is still an ATLAS-regime map and an importer must still reject it (WORLD_MAP.md 9.3);
   no DFN_ORIGIN marker is placed. Everything iteration 1's README argues about the two
   regimes stands unchanged.
 * Nothing here was written into the repository.
