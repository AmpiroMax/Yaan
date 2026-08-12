<!--
Created: 12:08:2026 - 22:57:02
Last updated: 12:08:2026 - 22:57:02
-->
<!--
UPD:
- 12:08:2026 - 22:57:02: Выделен из docs/design/LANDSCAPE.md (9786 строк против FILE_HARD_LIMIT 800): §10.6, §10.7, §10.8. Чистый перенос — ни одна строка тела не изменена, ни один номер секции не изменён; адреса вида «LANDSCAPE.md §X» продолжают действовать, таблица § → файл живёт в docs/design/LANDSCAPE.md.
-->

### 10.6 BUILD ORDER, AND WHY THIS ORDER

1. **B2 outcrops + B1 boulders, together, with B6 skirts as part of the same
   step.** Most change in the frame per triangle spent, and — decisively —
   **outcrops are the only natural class whose read distance covers 150–750 m**,
   which §10.4.1 identifies as where the flatness complaint actually lives.
   B6 is not a separate step because boulders and outcrops without skirts have a
   hard contact seam and read as props; the skirt is part of B1/B2's acceptance,
   not a later polish pass.
2. **B3 fences.** ~350 triangles per run, and it doubles as the acceptance
   instrument for §10.1: a straight fence top is a flat-ground report.
3. **B4 towers.** Highest value per object in the frame, but the most expensive
   to author, and its siting depends on B2 (towers stand on outcrops).

B7's lean is a one-line change to an already-approved class and can ride with any
of the three; B5, B8 and B9 are settlement and water work and follow their own
passes.

---

### 10.7 NUMBERS REQUESTED (Rule 35 — via lead, to `docs/NUMBERS.md`)

Every value below is **предложение — утвердить**. The "second zone" column names
who else must agree, which is what makes it a NUMBERS.md row rather than a
design-local figure.

| constant | proposed | unit | second zone |
|---|---|---|---|
| `GROUND_RELIEF_SIGMA_20M_MIN` | 0.35 | m | core (generator + the probe that measures it) |
| `GROUND_RELIEF_SIGMA_20M_MAX` | 1.20 | m | core |
| `MIDGROUND_OBJECT_COUNT_MIN` | 5 | silhouettes ≥ 8 px | render (it is counted on a frame at `INTERNAL_RES`) |
| `BOULDER_SIZE_MIN` / `_MAX` | 0.8 / 4.0 | m | core |
| `BOULDER_BURIAL_FRAC_MIN` / `_MAX` | 0.25 / 0.55 | fraction | core |
| `BOULDER_CLUSTER_SIZE_MIN` / `_MAX` | 3 / 9 | stones | core |
| `BOULDER_CLUSTER_SPAN_MIN` / `_MAX` | 6 / 20 | m | core |
| `BOULDER_CLUSTERED_FRAC_MIN` / `_MAX` | 0.60 / 0.75 | fraction | core |
| `BOULDER_SIZE_RATIO_MIN` (within a cluster) | 1.6 | ratio | core |
| `BOULDER_DENSITY_ANCHORED_MIN` / `_MAX` | 1.5 / 4.0 | per ha | core |
| `BOULDER_DENSITY_OPEN_MIN` / `_MAX` | 0.1 / 0.4 | per ha | core |
| `BOULDER_ERRATIC_DENSITY_MAX` | 0.05 | per ha | core |
| `BOULDER_SOURCE_RADIUS` | 60 | m | core |
| `OUTCROP_DENSITY_MIN` / `_MAX` | 0.4 / 1.2 | per ha | core |
| `OUTCROP_PROUD_SLAB_MIN` / `_MAX` | 0.1 / 0.6 | m | core |
| `OUTCROP_PROUD_BOSS_MIN` / `_MAX` | 2 / 8 | m | core |
| `OUTCROP_EXTENT_MIN` / `_MAX` | 3 / 25 | m | core |
| `OUTCROP_IN_VIEW_MIN` | 3 | count, open ground | render (frame-side check) |
| `BEDDING_DIP_MIN` / `_MAX` | 5 / 25 | ° | core; **reuses §4.1's absolute-height stratum field** |
| `BEDDING_AZIMUTH_COHERENCE` | 200 | m | core |
| `OUTCROP_TRI_BUDGET_NEAR` / `_FAR` | 600 / 60 | tris | render (LOD switch ≈ 150 m) |
| `FENCE_POST_HEIGHT_MIN` / `_MAX` | 0.9 / 1.5 | m | core |
| `FENCE_POST_SPACING_MIN` / `_MAX` | 1.8 / 3.0 | m | core |
| `FENCE_RUN_LENGTH_MIN` / `_MAX` | 15 / 80 | m | core |
| `FENCE_GAP_FRAC_MIN` / `_MAX` | 0.10 / 0.30 | fraction | core |
| `FENCE_ROAD_OFFSET_MIN` / `_MAX` | 2 / 5 | m | core |
| `FENCE_POST_LEAN_MIN` / `_MAX` | 3 / 15 | ° | core |
| `TOWER_MINOR_DIM_PER_DISTANCE` | 1/30 | ratio | render — **it is `SILHOUETTE_MIN_PX` restated as a siting rule** |
| `TOWER_CROWN_NOTCH_COUNT_MIN` | 3 | notches | core |
| `TOWER_CROWN_NOTCH_DEPTH_MIN` | 0.5 | m | core |
| `TOWER_CROWN_LINE_VARIATION_MIN` | 1.0 | m | core |
| `MASONRY_BLOCK_YAW_MAX` | 8 | ° | core |
| `MASONRY_COURSE_OFFSET_MAX` | 0.15 | m | core |
| `KERB_HEIGHT_MIN` / `_MAX` | 0.15 / 0.30 | m | core |
| `STEP_RISE_MIN` / `_MAX` | 0.15 / 0.20 | m | **core + movement — must agree with `PLAYER_STEP_HEIGHT`** |
| `STEP_TREAD_MIN` / `_MAX` | 0.30 / 0.45 | m | core |
| `RETAINING_WALL_HEIGHT_MIN` / `_MAX` | 0.8 / 2.5 | m | core |
| `RETAINING_WALL_BATTER_MIN` / `_MAX` | 3 / 8 | ° | core |
| `BUILT_EDGE_LEVEL_CHANGE_MIN` | 0.4 | m | core — above this, a built edge, never a grade |
| `KERB_STRAIGHT_RUN_MAX` | 12 | m | core |
| `SHRUB_SKIRT_FRAC_MIN` / `_MAX` | 0.50 / 0.80 | fraction | core + flora |
| `CLUMP_SPAN_MIN` / `_MAX` | 2 / 6 | m | flora |
| `SNAG_LEAN_MIN` / `_MAX` | 12 / 30 | ° | flora — **an addition to §5.9's existing class, not a new one** |
| `SNAG_LEAN_AZIMUTH_SPREAD` | 25 | ° about the wind azimuth | flora + render (`WIND_FIELD_*`) |
| `ARCH_OPENING_PER_DISTANCE` | 1/30 | ratio | render |
| `WINDMILL_SAIL_SPAN_MIN` / `_MAX` | 8 / 12 | m | core |
| `WINDMILL_SAIL_CROSS_ANGLE` | 45 ± 15 | ° from vertical | core |

Two of these are **not new numbers at all** and are listed so nobody re-derives
them: `TOWER_MINOR_DIM_PER_DISTANCE` and `ARCH_OPENING_PER_DISTANCE` are both
`SILHOUETTE_MIN_PX` = 8 px at `INTERNAL_RES` restated as siting rules, and if
that constant ever moves, these move with it rather than being re-argued.

---

### 10.8 ACCEPTANCE — the frame pairs (Rule 27)

Each row is **one frame from our build beside the reference frame it answers**,
at the same *kind* of viewpoint, archived in `docs/acceptance/` with its recipe,
shot at or downsampled to `INTERNAL_RES` (F6). Each carries the sentence that
must be capable of being true, or it is a diagnostic and not a verdict (F7).

| # | ref | our standpoint | what would make it FAIL |
|---|---|---|---|
| **A1** | **01** | eye height on the **flattest legal ground** we have, looking level, sun at 25–35° elevation raking across the view | ground runs unbroken from the player's feet to the tree line; fewer than 3 ground crest-lines inside 60 m; fewer than `MIDGROUND_OBJECT_COUNT_MIN` mid-ground silhouettes; fewer than 3 outcrops in view |
| **A2** | **15** | on a road, looking **along** it, a fence run in frame, low sun | the fence's top line is straight in screen space; posts all plumb; boulders sitting on the surface rather than emerging |
| **A3** | **06** | 60–100 m from a tower group, low sun across the drums | the crown reads as a smooth arc; the drum's silhouette edge is one straight line; the tower stands on graded soil |
| **A4** | **03** | forest floor at eye height, dappled light | no bedrock visible through the soil within the near 20 m; every slab drawn as a splat patch with no rim shadow |
| **A5** | **16** | inside a stand with foreground boulders, warm near fog | snags and trunks plumb; neighbouring leans disagreeing about direction; no half-buried rock in the foreground |

**A1 is the one that answers the user's sentence** and it is the one to shoot
first, because it is the frame that is allowed to look bad: it is deliberately
taken on the flattest ground in the world, which is where «нет идеальноплоского
мира как в майнкрафте» either holds or does not.

**A note on what A1 cannot certify.** A1 proves the *near and mid* field. It says
nothing about R1 haze or R3 sky, which are render's and are certified on their
own frames — a plateau frame with a beautiful sky and a flat plateau still fails
A1, and a bumpy plateau under a bad sky still passes it. Keeping the two apart is
what stops a good frame from certifying a property it never tested.

---

