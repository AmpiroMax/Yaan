
## 8. The stand maps — briefs (user-ratified в1/в2/в5/в6/в15, stage-5)

The stands are SEPARATE maps built from generation rules (в1) — not
hand-sculpts: everything on them comes out of the dictionary (§2.10) and the
same passes that later compose the big world. Order ruled by the user: tech
debt → forest → river+castle (character-agent parallel) → sea (в22) →
town + mirror map. **A brief states what must be TRUE and how it is
ACCEPTED — frames and conditions — never how it is built.**

**Scoping rule for all stands:** stand maps are exempt from the L0/weenie
wayfinding hierarchy (§1.3) — they test landforms and rules, not wayfinding;
§1.3 binds the testbed and the big world. Everything else in this bible
binds stands exactly as it binds the world, and the §1.6 frame doctrine
(declared vantages, each able to fail — F7) governs every acceptance below.

### 8.1 Stand 1 — FOREST: the walk-and-look map

**Purpose:** the first map where walking IS the content. It exists to prove
the six beauty rules (§1.7) and the forest floor (§5.10 — flagged unbuilt
twice by the lead; this stand is where it finally gets built), against the
user's founding complaint «земля плоская и мёртвая».

**Composition (§2.10 rule 4, declared):** LF-1 rolling plain (glades),
LF-2 ridge-and-swale, LF-5 crest/outcrop, LF-7 forest floor, LF-8 erosion
overlay. No massif, no sea, no L0 — deliberately: nothing tall rescues a
boring middle distance here, the meso tier and the floor must carry the
frame alone.

**What must be true:**

1. **All four path types as ONE system (в7):** мостовая (paved), dirt road,
   hint-path (тропинка-намёк), stone steps — one network, one
   `dist_to_path` field. Type changes along a route by rule: paved near the
   largest goal, dirt between goals, hint-paths to finds, steps where the
   slope demands them. Cross-section per research A6: worn center → pressed
   margins → rich edge (BR-3) — a GRADIENT, never a decal ribbon.
   **в7 binds the SYSTEM, not a per-stand instance count (core's report,
   stage-5):** this map realizes 59 cobble / 507 dirt / 76 hint-path
   stations and zero stone steps — honestly, at a re-derived 0.22 grade
   threshold (0.30 produced none), because this stand's routes contour and
   never climb a slope that demands them. That is accepted, not a defect:
   the class exists in the generator and is rule-selected like the other
   three, and forcing a climb into the landform solely to manufacture a
   steps frame would be the same "buy the number" move BR-4's grass class
   was already refused. A future stand (or a scarp-climbing spur added to
   this one) exercises the fourth class; nothing here requires it to be
   this stand.
2. **Real goals for the network** (BR-2 requires them): 4–6 small goals —
   e.g. clearing shrine, spring, woodcutter's hut, a pale-spire group
   against canopy (§2.9) — catalog design's, placement generator's.
3. **Finds at cadence (BR-6):** both regimes measurable — total path length
   ≥ 2 km so road and wild routes each yield ≥ 10 gaps (Rule 31 needs a
   distribution, not an anecdote).
4. **§5.10 floor classes built:** BigBush, both log classes, scarp-edge
   trees where scarps exist.
5. **Clump field authored (BR-4)** driving grass/flowers; rich edges (BR-3).
6. **The shared wind field exists here first:** grass and leaves read ONE
   vector. The sea stand's Gerstner waves (в21) later read the SAME field —
   this stand proves the field, the sea stand proves the waves.
7. **Walked in first person with the full body (в3/в11):** the acceptance
   tour is walked at eye height; the body itself is the character-agent's
   deliverable, but this map is its stage — step-feel (bob ↔ footstep sync,
   research D1) is accepted here when it lands.

**Acceptance:** all six §1.7 gates green on this map WITH their controls;
declared frames (fixed before the run): (a) down-a-path frame — worn
center, margins, rich edge in one image; (b) swale frame — the path bends
out of sight (BR-1 visible); (c) crest frame — a find revealed; (d) glade
frame — clumped flowers, not sprinkle; (e) floor frame — logs and bushes
breaking sightlines. Plus the two scripted walk routes (road / wild) with
gap statistics recorded.

**Needs, for the lead to sequence:** core — path-network generator
(cost-field desire lines, four types, `dist_to_path`), landform composition
per §2.10, clump-field sampling, find placement, erosion pass; render —
path splat cross-section, floor-class and find meshes, steps geometry,
wind-driven grass; flora — edge population tables, understory clumping,
find catalog entries; sim — collision for logs/bushes (§5.10 table).
в24 binds the split: **core generates, render draws, flora populates the
edge, design accepts.**

### 8.2 Stand 2 — RIVER + CASTLE: the 25–35 m river and the walled city

**Purpose:** water at real scale (в6: «не как лужица что сейчас») and the
seat of state power at its final size — a NEW castle (в5) that, once
polished, replaces Harrowward in the big world. The replacement's fiction is
story's; this stand only has to earn it.

**Composition:** LF-3 river valley with terraces (the spine), LF-1 on
terrace tops, LF-2 on valley shoulders, LF-5 outcrops at shoulder breaks,
LF-7 in the valley-side woods, LF-8 erosion. Structures: castle + walled
city (§6.1 applies, scaled), stone bridge, wharf, posad.

**What must be true:**

1. **The river:** 25–35 m wide, **navigable edge to edge** — continuous
   channel ≥ `NAVIGABLE_DRAFT` (1.2 m proposed); obeys §3.1 whole,
   including flat reaches; **current on the surface, not waves** (в21);
   tributaries 3–5 m join with visible confluences (LF-8 fans where they
   cut the terraces).
2. **Crossings:** per LF-3, **the ford rule is superseded by bridges on the
   navigable channel** — ≥ 1 stone bridge carries the main road, with
   `BRIDGE_CLEARANCE` (3 m above reach level proposed) so navigability
   survives the crossing. Fords remain legal on tributaries only.
3. **The castle: LARGE (в2 — the user's emphasis).** §6.1 hierarchy, siting
   and footprint rules apply at the new scale; sited commanding the river
   (bend outer bank / terrace edge, §3.4 scoring logic). **Walled city
   INSIDE the walls (в6), posad outside the land gate, wharf on the
   water.**
4. **City anatomy minimum:** wall with ≥ 2 gates; keep + bailey (§6.1);
   streets connecting gates ↔ keep ↔ wharf; posad = unwalled cluster
   outside the land gate; wharf = quay wall + landing on the navigable
   channel, reachable from a gate.
5. **The path system continues (в7):** paved inside the walls, dirt on the
   approaches, the bridge carries the main road; BR rules bind the
   approaches.
6. **Terraces:** 2–3 per side reading as horizontal lines; the city stands
   on a terrace, not on a slope (§6.1 pads).

**Acceptance:** declared frames: (a) water-level up-river frame — valley,
terraces, castle over the water (the user's sentence «на реке должен стоять
ЗАМОК БОЛЬШИХ РАЗМЕРОВ» as an image, from a vantage that could fail it);
(b) bridge frame — width and current readable; (c) far-terrace frame — the
whole hierarchy wall/keep/posad/wharf readable at 640×360 (§6.1.3);
(d) wharf frame at eye height. Conditions: width in band at every station;
navigability trace green; §3.1 invariants green; **no ford on the main
channel — control: run the ford generator against the navigable river and
the acceptance must reject the result;** §6.1 checks re-run at scale.
**Control for LARGE:** Harrowward as built is the real comparative
instance — proposed `CASTLE2_FOOTPRINT_MULT` ≥ 2× its footprint
(предложение — утвердить).

**Needs, for the lead to sequence:** core — wide-river carve + terrace
operator + navigability trace, bridge/wharf pads, tributaries, and the
**city wall + street generator, which does not exist and is the long pole —
flagged**; render — water-current shading, bridge/quay/wall meshes
(placeholder prisms legal per §7.3 precedent), castle at scale; sim —
collision (boats are FUTURE: navigability is accepted by trace, not by
sailing); flora — riverbank species (birch bank lines at
`BIRCH_BANKLINE_SPACING`), terrace-edge planting; story — **consult before
the city lands** (it will acquire a name and canon, §2.9.5 precedent, and
it eventually replaces Harrowward — that transition's fiction is story's).

---

