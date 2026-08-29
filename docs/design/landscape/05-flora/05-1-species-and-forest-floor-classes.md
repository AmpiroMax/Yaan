
### 5.1 Dale Oak (broadleaf — the valley tree)

- **Silhouette:** short thick trunk (1/3 of height), one wide rounded crown
  mass, wider than tall overall. Reads as "ball on a stump" at 8 px.
- **Size (stage-4 revision, §5.7):** **24–32 m** tall, crown 10–16 m wide.
  Crown base at 35–45 % of height (≈ 9–13 m of clear trunk) — the height is
  only half the effect, the *space underneath* is the other half.
- **Poly budget:** 300–500 tris (`TREE_TRI_BUDGET` family).
- **Palette:** mid-green crown, near-black trunk; value: darker than meadow
  grass, lighter than pines.
- **Placement:** valley floors and slopes < 20°; the default forest-mass tree
  (S/SE forest masses); Poisson spacing 5–8 m inside masses
  (`TREE_SPACING_FOREST`), clusters of 5–15 in meadows.
- **Clustering:** high — oaks define forest interiors and edges.

### 5.2 Highland Pine (conifer — the slope tree)

- **Silhouette:** narrow triangle, 2–3 stacked cone tiers, tip must survive
  quantization (make the top cone ≥ 1.5 m wide). Tall and pointed — the
  anti-oak.
- **Size (stage-4 revision, §5.7):** **28–38 m** tall, base 6–9 m wide —
  ×2.3, deliberately **not** ×4 (see §5.7 for why 4× breaks the valley).
- **Poly budget:** 150–300 tris (cones are cheap).
- **Palette:** dark blue-green, darkest flora value in the scene — pines are
  the *dark mass* tool for composition backdrops.
- **Placement:** slopes 10–35°, higher terrain, crag foothills and northern
  ridges; spacing 4–7 m; follows ridgelines in strips 20–60 m wide (great for
  leading lines toward the L0). Pines are the main C4 hazard: 12–18 m of
  canopy on foothills out-angles the L0 from valley standpoints — pine strips
  on landmark-facing shoulders are subject to the L0 sight-wedge filter (§1.3)
  before anything else.
- **Clustering:** strips and wedges, not blobs; a lone skyline pine is a
  legitimate L2 guide.

### 5.3 River Birch (accent — the water tree)

- **Silhouette:** slim pale trunk (readable!), small loose crown, slightly
  leaning. The *light-value* accent against dark water or pines.
- **Size (stage-4 revision, §5.7; crown restated stage-5):** **16–22 m** tall,
  crown **5.4–7.5 m — DESCRIPTIVE, not authored.** The authored quantity is
  `crown_width_frac` = 0.34 (crown diameter / height); the metre figures are
  what it realises across the height band. **«A range is two assertions» does
  not apply — there is one assertion and it is the fraction.** The old «5–7 m»
  understated the top by half a metre and was twice mistaken for a lever. Stays
  the smallest and slimmest of the three, keeping its accent role.
- **Poly budget:** 200–350 tris (trunk needs a few more sides for the pale
  read).
- **Palette:** near-white trunk (brightest flora value), light yellow-green
  crown.
- **Placement:** within 20 m of water only (`BIRCH_WATER_DIST` = 20 m),
  clusters of 3–7; marks rivers/lake at distance — a birch line = water line.
- **Clustering:** loose lines along banks; never deep forest.
- **Crown base ~~≈ 0.58–0.62~~ → 0.40–0.45 of height, DERIVED not authored
  (ruling, stage-4, re-derived twice since — this line SUPERSEDES the 0.58–0.62
  range wherever it still appears).** The live value is
  `BIRCH_CROWN_BASE_FRACTION_MIN` = 0.40, and at `CROWN_ASPECT_MAX` = 2.0 that
  authored value governs rather than being overridden by the ceiling (§5).
  The account below is the ORIGINAL stage-4 reasoning, kept because the diagnosis
  is what matters; its numbers were superseded when the aspect basis moved from
  the authored container to built geometry.
  The birch crown failed to read as a mass four times across two sessions and
  flora stopped it under Rule 28 rather than trying a fifth arrangement. They
  were right, and **the defect was in my numbers, not their geometry** — see
  the crown-aspect rule in §5. Two of my rules multiplied: a 5–7 m crown width
  and a crown base at `CROWN_BASE_FRACTION_MAX` = 0.45 of a 16–22 m height
  give a container **≈ 5.7 m wide by 10.5 m tall — 1.8 : 1 before a single
  leaf cluster is placed**, and the fill takes the measured foliage box to
  **2.65 : 1**. No arrangement of contents can make that read as a rounded
  mass. My own silhouette brief above says "small loose crown", and a crown
  occupying the top 55 % of the tree is not small: **the written intent and
  the numbers disagreed, and the numbers won.** Raising the crown base is free
  of everything I value here — the 5–7 m width band is untouched, so
  `TREE_SPACING_FOREST` (derived from crown width) does not move, the "smallest
  and slimmest of the three" accent role is *strengthened*, and clear trunk
  rises from ≈ 8.5 m to ≈ 11 m, which is §5.7's own goal.

### 5.4 Bush

- **Silhouette:** 1–1.5 m hemisphere lump; groups of 2–4 read better than
  singles. **Poly budget:** 60–120 tris. **Palette:** oak-green, slightly
  lighter.
- **Placement:** forest-mask edges (≤ 10 m outside) and clearing rims,
  0.01–0.03 /m² there (`BUSH_EDGE_DENSITY`); softens the tree/meadow
  boundary so forest masses do not read as walls of sticks.

### 5.5 Flowers

- **Silhouette:** 2-quad cross billboards, 8–16 tris, height ≤ 0.5 m.
- **Palette:** one accent hue per patch (white/red/blue family); the only
  saturated accents in open meadow — they read as L2 guides.
- **Placement:** patch system per §2.3 (`FLOWER_PATCH_*`); open grass splat
  only, slope < 15°, never under canopy, never in corridors' driving line
  (they may edge it — flowers *beside* the path pull the eye forward).

### 5.6 Grass

- **Silhouette:** simple card tufts, ≤ 0.4 m (`GRASS_HEIGHT_MAX`), 2 tris per
  card. **Palette:** exactly the underlying splat color family ±1 value step —
  grass is texture, not information.
- **Placement:** render-side within `GRASS_VIEW_DISTANCE` = 50 m,
  `GRASS_DENSITY` 0.5–1.5 /m² on grass splat only (§2.3); density fades to 0
  at the view distance edge (no popping line at low res — dither the fade).

---

### 5.7 Tall-tree revision — working the collisions through (stage-4)

User: trees ×4 taller, forests less dense, "как в Скайриме". Taken seriously,
that request collides with four existing rules at once. Working it through
rather than approving it:

**What the request is actually about.** Our 8 m oak has its crown starting at
≈ 2.7 m: the player pushes *through* foliage, which reads as scrub. What
makes Skyrim's forests feel tall is walking **under a canopy** — clear trunk
space overhead and light between stems. So the fix is three-part and the
height is only the first part: raise the trees, **raise the crown base**, and
**drop the density**. Height alone would have given us taller scrub.

**Ruling on the numbers.** Oak 8–12 → **24–32 m** (the requested ×4 at the
low end). Birch 6–10 → **16–22 m**. Pine 12–18 → **28–38 m**, which is ×2.3
and **declines the literal ×4**, because:

- A ×4 pine is 48–72 m. Standing on a 25 m foothill that is a 73–97 m crown
  top against Ravenscar's 52 m summit: **the forest would be taller than the
  landmark it frames.** Canopy-aware C1 (§1.3) would fail everywhere and no
  strip geometry could save it.
- Skyrim's own tall conifers are ≈ 25–30 m. The literal ×4 overshoots the
  reference the request cites.

**Collision 1 — the landmark must out-top its own forest.** A 35 m tree on a
25 m foothill tops out at 60 m against a 52 m summit. Two consequences, both
binding:

- **Ravenscar must grow with its forest:** `L0_RELIEF` 52 → **110–120 m**
  — **APPROVED by the user**. The argument is composition, not validation:
  a valley heart that its own trees overtop is not a landmark. (The C1 case
  once made for this raise rested on contaminated numbers; the corrected
  measurement shows the raise *improves* C1 to ≈ 0.865 rather than costing
  anything — see the withdrawn finding in §1.3. Right answer, and now for a
  reason that survives.) All castle rules are ratios to the peak
  (`CASTLE_SKYLINE_MARGIN` etc.) and re-derive automatically — story's ~55 m
  castle/barrow geometry is unaffected.
- **L0/LR sight wedges — revised (flora's pushback accepted, with its
  reasoning corrected).** My first draft banned *all* trees in a wedge. Flora
  is right that at 12–18 m spacing this carves mown lanes radiating from every
  POI — authored-looking, the exact effect standing stones exist to create
  deliberately. The rule instead:
  - **RE-RULED after the C1 correction (flora asked for the re-decision, and
    was right to).** Both wedge constraints were justified partly by a
    headroom crisis that turned out to be a validation bug (§1.3). Rather than
    inherit rules whose premise evaporated, the wedge now uses **one test,
    the same one already governing the castle**: *no tree may occlude the
    L0/LR's **crown** (top third) from any corridor standpoint; occluding its
    **flank** is permitted.* One notion of acceptable occlusion across
    architecture and vegetation, and it needs no near/far half-split.
  - **Giants are ALLOWED in sight wedges** — reversing my exclusion. Flora's
    compositional argument is the stronger one: a single elder standing in the
    middle distance, off the axis, **gives the landmark scale**, which is the
    thing a distant landmark most needs and most rarely gets. That is
    repoussoir, and removing it deletes our best depth cue exactly where depth
    matters most. Conditions: subject to the crown rule above, and **at most
    one giant per wedge** — one elder frames, three elders screen.
  - **Sharpening of C4 that this exposed:** C4 governs **masses and built
    structures**, not individual near vegetation. A 48 m crown 100 m away
    subtends more than the mountain behind it and that is *fine* — nobody
    mistakes a nearby tree for a distant massif; the eye reads the depth cue
    correctly. C4's real target was always the foothill pine *wall*, a mass.
    Apply it to masses.
  - Bushes, saplings and anything under ≈ 8 m were never restricted and still
    are not — the wedge keeps its ground texture and reads as young growth
    under an old canopy, never as a mown lane.
  - **Fallback if per-tree crown testing proves expensive:** the previous
    conservative rule (tall three banned in the near half) is an acceptable
    approximation — but it is the fallback, not the intent.

### 5.8 Maturity tiers — restoring fullness without restoring canopy

Approved (flora's proposal): every instance carries a maturity scalar rather
than standing at species nominal size, because 44 stems/ha all identical reads
as a **plantation**, and a plantation is a different failure from an empty
field but a failure all the same.

**Revised (user: «гигантским и могучим… мелкие деревья очень редкие»):**

| Tier | Share | Size | Placement |
|---|---|---|---|
| Giant / **Elder** | **25 %** | ×1.5 nominal | the "могучий" read; also L2 guides (§1.3) — a forest with internal hierarchy is legible from inside |
| Mature | **60 %** | nominal | the main lattice |
| Sub-mature | **12 %** | ×0.7–0.85 | mid-canopy layering on a half-spacing sub-lattice |
| Sapling | **3 %** | ×0.4–0.6 | deliberately rare — a nursery is what the user rejected |

Shares предложение — утвердить. **Why not flora's 25/67/8:** their reasoning
was right that the young tier's *ground-level* fill job is better done by the
new bushes, logs and snags. But young trees at ×0.5–0.7 of a 28 m tree are
14–20 m — their crowns sit **above** eye level and **below** the main canopy,
so they were never doing ground fill; they were doing **mid-canopy layering**,
which is what makes a wood feel deep rather than like a colonnade with debris
on the floor. Deleting them wholesale would flatten the vertical section. So
the tier splits: the genuinely small (sapling, ×0.4–0.6) drops to 3 % —
satisfying «очень редкие» — while a sub-mature band survives at 12 % to keep
the middle of the section populated. Ground fill moves to §5.9's classes,
exactly as flora argued.

**The "Elder Oak" proposed as a separate species IS the giant tier** — one
system, not two: a maturity scalar on the existing parameter set, not a
catalog row. Giants are excluded from sight wedges entirely (§5.7).

### 5.9 Additional species (approved from flora's proposals)

**Standing snag (dead trunk).** Approved, and the sharpest idea in the batch:
a barkless broken column with no crown is **the only flora that may legally
stand at full height inside a sight wedge** — nothing to out-angle with.
30–60 tris, the cheapest asset in the project.

**Density revision (user endorsed dead trees; flora flagged the tension
honestly rather than letting it be overwritten).** My original rarity existed
because pale dead wood is the highest flora value in the scene and a common
snag becomes a false L2 guide. That reasoning does not stop being true because
more were requested — so the fix is to **split the material, which turns the
tension into a feature**:

| Where | Density | Value | Reads as |
|---|---|---|---|
| Inside forest masses | `SNAG_DENSITY_FOREST` = 1.5–3 / ha | weathered grey-brown | texture and atmosphere; one every ~30–80 m |
| Open ground | `SNAG_DENSITY_OPEN` = 0.25–0.5 / ha (unchanged) | pale bone | a deliberate vertical accent — a legitimate L2 guide |

**(предложение — утвердить.)** A pale snag standing alone in a meadow is a
landmark; a grey snag in a wood is weather. Same asset, two jobs, and the
composition rule survives contact with the user's request instead of being
quietly traded away. Prefer old stands, edges, clearing rims, scarp tops, and
ground near barrows and ruins where the atmosphere pays double.

**Vale willow / alder (riparian).** Approved, and it fixes a real flaw I had
left: *every* water body in the world is currently flagged by pale birch, so
all water reads the same. Two riparian species let water say two things —
ruling on the split: **birch marks moving, clear water** (river banks, fords),
**willow marks still or slow water** (lake shores, pond rims, slack bends).
Dark low mass, the value opposite of birch, 10–14 m, within
`RIPARIAN_WATER_DIST` (reuse the birch 20 m band). A player who learns that
dark drooping mass means still water has learned to read the landscape, which
is the whole point of a palette this small.

**Elder oak** — folded into §5.8's giant tier, not a separate species.

### 5.10 Forest floor classes (user-specified, stage-4)

The floor is what makes a wood feel *walked* rather than crossed, and at the
new density it is also what replaces the fill the young tier used to carry
(§5.8). All approved as distinct catalog classes.

**BigBush** — a class, **not a scaled Bush**, and flora is right about why: a
1.2 m bush is ground texture, a 3.5 m bush is an **obstacle that breaks a
sightline and makes the player pick a side**. That is forest-floor navigation.
2.5–4 m, denser silhouette, 120–180 tris. `BIGBUSH_DENSITY` = 8–15 / ha inside
masses, plus clearing rims, scarp bases and stream banks. **Never inside a
corridor mask** (they exist to be gone around, and the critical path is not
the place for that); may partially occlude but at 4 m they cannot threaten a
POI sightline at any meaningful range.

**FallenLog**, two classes as specified:

| Class | Size | Tris | Density | Rules |
|---|---|---|---|---|
| Big | 8–14 m long, ⌀ 0.8–1.4 m, half-sunk | 80–120 | `LOG_DENSITY_BIG` = 3–8 / ha | **excluded from corridors** until vaulting exists (⌀ exceeds `PLAYER_STEP_HEIGHT`, so it would be a wall on the critical path); collision on (sim) |
| Small deadfall | 2–4 m, ⌀ 0.35–0.5 m, half-sunk | ~40 | `LOG_DENSITY_SMALL` = 15–30 / ha | allowed anywhere including corridors — half-sunk it sits under step height |

**(предложение — утвердить.)** **Logs lie ACROSS the fall line, never along
it** — flora's geometric ask, approved with its reasoning made binding: a log
pointing downhill reads as a stick, a log lying across a slope reads as a
fallen tree and as something to climb over. Big logs are nearly free
geometrically (the trunk mesh, rotated and sunk) and are the cheapest "this
forest is old" signal in the medium. **FUTURE:** when sim adds vaulting, big
logs become legal in corridors and turn into pacing furniture.

**Trees on scarp edges — flora's question, ruled: YES, deliberately.** A tree
leaning out over a drop is a superb silhouette and a genuine L2 guide, and it
is exactly the kind of detail that still reads at 640×360. Constraints:
mature or giant tiers only (a sapling on a cliff reads as an accident, not a
statement); lean 10–20° outward, away from the mass; scarps ≥ 3 m only, since
below that there is no drama; **never inside a sight wedge** (a tall occluder
on a high edge is the worst case we have). Rare by design — one per scarp
segment, never a row: a row reads as planted.

**Setback corrected (flora's root-flare finding).** The ≥ 1.5 m root-plate
setback is measured **from the outer edge of the root flare, not the trunk
axis** — with a 1.6× flare on a 1.2 m trunk the flare radius is ≈ 0.96 m, so
the original wording left barely 0.5 m of ground beyond it and the tree would
still have floated the first time the scarp was voxelised. The rule was right;
the datum was wrong.

**The lean here is a SEPARATE, larger parameter than the crowding lean** —
flora's note, adopted: crowding lean caps near 0.12 rad, and reusing it on a
cliff edge produces a limp tree instead of a statement.

**Collision 2 — under-canopy walkability.** `CANOPY_CLEARANCE_MIN` = 2.2 m is
a floor, satisfied ~4× over by a crown base at 35–45 % of height. Stated
explicitly so no future species regresses: **every tree species must carry
≥ 2.2 m of clear trunk**, and the tall species must target the 35–45 % band
rather than merely clearing the floor.

**Collision 3 — density, numerically.** Crown width scales ~×1.6, not ×4 —
tall *and* slender is what makes a forest feel tall. Spacing must open up
accordingly: `TREE_SPACING_FOREST` 5–8 m → **12–18 m**
**(предложение — утвердить)**. In trees per hectare that is ≈ 240 → ≈ 44, an
**≈ 80 % density cut** — which is the user's "less dense" expressed as a
number core can implement. `FOREST_COVERAGE` (0.25–0.40) is **unchanged**:
coverage is how much land is forest, density is how many trees stand in it,
and only the second was wrong.

**Collision 4 — budgets.** A 4× taller tree does not need 4× the triangles;
it needs a silhouette that survives being large on screen. `TREE_TRI_BUDGET`
500 → **700 max** for the tall species, but the real answer is LOD, not base
budget: each species ships LOD1 at ≈ 40 % tris and a billboard beyond
`TREE_BILLBOARD_DIST` **(предложение — утвердить, value is render's call)**.
Shadow-map coverage is render's zone; design's constraint is only that a
35 m canopy must not black out the ground it shades — dappled, not sealed,
which the 80 % density cut largely delivers on its own.

**Net effect on composition:** fewer, taller, slimmer trees with light
between them. Forests stop being walls (§2.2's "trees are walls you can walk
through" was written for the dense version) and become colonnades — you see
*through* a forest now, which makes occlude-and-reveal work at a distance it
never could before.

