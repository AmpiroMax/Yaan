
# LANDSCAPE.md — Landscape & World Design Bible

Owner: `design` (Rule 25). This document drives worldgen v2 and the placement
passes. Every rule here is written to be executable by a noise-based generator
or a deterministic placement pass (Q13: procedural base, hand-editing arrives
later via the editor). No rule requires hand sculpting.

Conventions used below:

- **(предложение — утвердить)** — a number invented in this document. It is a
  candidate for NUMBERS.md; the lead adds it after approval. Code must
  reference the future constant name, never the literal.
- **FUTURE** — depends on a system that does not exist yet (roads, quests,
  day cycle, swimming, region-scale biomes). Design intent recorded now so
  worldgen v2 does not paint itself into a corner.
- Existing constants are cited by NUMBERS.md name (`WALK_SPEED`, `CHUNK_SIZE`,
  `POI_TRAVEL_TIME`, ...). Units per Rule 14: meters, seconds, radians.
- **Section order note:** §9 is Sources but it is not the end. **§10 — the
  object grammar (D1/D2, the placement briefs B1–B9, their acceptance
  frames, and §10.9's haze verdicts) follows it**, appended stage-5 from the
  reference frames.

---

## Как читать этот файл

**Этот файл — оглавление. Тело живёт в `docs/design/landscape/`.**

Документ дорос до 9786 строк при нашем собственном `FILE_HARD_LIMIT` в
800 (`docs/NUMBERS.md`), то есть в двенадцать раз выше предела, который мы
требуем от кода. Он разбит как книга в LaTeX: здесь — шапка, соглашения и
таблица секций, там — секции. Правило деления: **папка на крупную секцию**
(§1, §2, §5, §10 — каждая больше предела сама по себе), **один файл на
мелкую**; имя файла несёт номер секции, поэтому связь адреса с файлом видна
из `ls`, а не только отсюда.

**Номера секций не менялись и меняться не будут при таких переносах.**
Ссылка вида «LANDSCAPE.md §7.1» из `docs/NUMBERS.md`, `docs/BOARD.md`, спек
других зон, комментариев в коде и брифов агентов ведёт сюда и находит адрес
в таблице ниже. Разбиение меняет ФАЙЛЫ, а не АДРЕСА. Если вы добавляете
секцию — добавьте строку в таблицу; если секция перерастает 800 строк —
делите её на файлы по под-подсекциям, но номер оставляйте на месте.

## Таблица секций — § → файл

Пути относительно `docs/design/`. Порядок — как в документе, а не
отсортированный: §7.0a стоит между §7.1b и §7.1a, потому что он там и стоял.
Адреса мельче заголовка (§3.1.4, §8.1.6 — нумерованные пункты внутри
секции) ищите в файле ближайшего охватывающего заголовка (§3.1, §8.1).

| § | Заголовок | Файл |
|---|-----------|------|
| `1` | 1. Composition principles | `landscape/01-composition/01-1-one-law-spacing-and-landmarks.md` |
| `1.1` | 1.1 The one law: пустота — наш враг, but readable emptiness | `landscape/01-composition/01-1-one-law-spacing-and-landmarks.md` |
| `1.2` | 1.2 Spacing derived from our metrics | `landscape/01-composition/01-1-one-law-spacing-and-landmarks.md` |
| `1.3` | 1.3 Landmark hierarchy (weenies, three tiers) | `landscape/01-composition/01-1-one-law-spacing-and-landmarks.md` |
| `1.3a` | 1.3a World scale, zones, and the fourth landmark tier (stage-4 ruling) | `landscape/01-composition/01-1-one-law-spacing-and-landmarks.md` |
| `1.3b` | 1.3b C1 MEASURES OCCLUSION, NOT LEGIBILITY — the two-number instrument (ruling, stage-4) | `landscape/01-composition/01-1-one-law-spacing-and-landmarks.md` |
| `1.4` | 1.4 Draw-the-player rules | `landscape/01-composition/01-1-one-law-spacing-and-landmarks.md` |
| `1.5` | 1.5 Readability under the Daggerfall look (low-res, first person) | `landscape/01-composition/01-5-readability-and-the-acceptance-frame.md` |
| `1.5.1` | 1.5.1 WHAT FULL COLOUR COSTS THIS SECTION, RULE BY RULE (stage-5) | `landscape/01-composition/01-5-readability-and-the-acceptance-frame.md` |
| `1.6` | 1.6 THE ACCEPTANCE FRAME — what a frame certifies, and from how far (doctrine, stage-4) | `landscape/01-composition/01-5-readability-and-the-acceptance-frame.md` |
| `1.6.1` | 1.6.1 F2 — AN ACCEPTANCE DISTANCE IS A PROPERTY OF THE LANDMARK, NEVER OF THE PROJECT | `landscape/01-composition/01-5-readability-and-the-acceptance-frame.md` |
| `1.6.2` | 1.6.2 F3 — what a frame of unauthored backdrop actually certifies | `landscape/01-composition/01-5-readability-and-the-acceptance-frame.md` |
| `1.6.3` | 1.6.3 F3, second half — a landmark rule written for 4 km, in a world 1 km across | `landscape/01-composition/01-5-readability-and-the-acceptance-frame.md` |
| `1.6.4` | 1.6.4 F1 — residency is chunk-granular, and it does NOT block Ravenscar | `landscape/01-composition/01-5-readability-and-the-acceptance-frame.md` |
| `1.6.5` | 1.6.5 Two conduct rules this stage earned, in transmissible form | `landscape/01-composition/01-5-readability-and-the-acceptance-frame.md` |
| `1.7` | 1.7 The six beauty rules — acceptance conditions (user-ratified в19/в20, stage-5) | `landscape/01-composition/01-7-six-beauty-rules.md` |
| `1.9` | 1.9 THE BACKWARD SWEEP — every pre-existing acceptance rule against the aggregation/denominator clause (audit, stage-5) | `landscape/01-composition/01-9-backward-sweep.md` |
| `1.9.0` | 1.9.0 THE INSTRUMENT, because it is the transmissible part and it is one question | `landscape/01-composition/01-9-backward-sweep.md` |
| `1.9.6` | 1.9.6 WHAT THE SWEEP DID NOT FIND, which is most of it | `landscape/01-composition/01-9-backward-sweep.md` |
| `2` | 2. Detail layers and worldgen pass order | `landscape/02-detail-layers/02-1-macro-meso-micro-and-massifs.md` |
| `2.1` | 2.1 Macro (mountains, ridgelines, water bodies, forest masses) | `landscape/02-detail-layers/02-1-macro-meso-micro-and-massifs.md` |
| `2.2` | 2.2 Meso (hills, clearings, river bends, outcrops, tree clusters) | `landscape/02-detail-layers/02-1-macro-meso-micro-and-massifs.md` |
| `2.3` | 2.3 Micro (grass, flowers, bushes, stones, sand patches) | `landscape/02-detail-layers/02-1-macro-meso-micro-and-massifs.md` |
| `2.4` | 2.4 Critical-path protection (applies to every layer) | `landscape/02-detail-layers/02-1-macro-meso-micro-and-massifs.md` |
| `2.5` | 2.5 The regional landmark massif — the temple mountain (LR) | `landscape/02-detail-layers/02-1-macro-meso-micro-and-massifs.md` |
| `2.6` | 2.6 Border mountains — the world edge | `landscape/02-detail-layers/02-1-macro-meso-micro-and-massifs.md` |
| `2.7` | 2.7 Ground micro-relief and the plain | `landscape/02-detail-layers/02-1-macro-meso-micro-and-massifs.md` |
| `2.8` | 2.8 Massif shape language — the anti-dome ruling, second pass (stage-4) | `landscape/02-detail-layers/02-8a-shape-language-diagnosis-and-generator.md` |
| `2.8.1` | 2.8.1 Diagnosis — why the first invariant did not bite (measured, core) | `landscape/02-detail-layers/02-8a-shape-language-diagnosis-and-generator.md` |
| `2.8.2` | 2.8.2 The generator model — the BANDED CONTOUR MASSIF | `landscape/02-detail-layers/02-8a-shape-language-diagnosis-and-generator.md` |
| `2.8.3` | 2.8.3 The invariants — nine tests the generator runs on itself | `landscape/02-detail-layers/02-8b-invariants-tors-costs-constants.md` |
| `2.8.4` | 2.8.4 «Кубы на кубах» — the tor ruling, and what voxels can honestly do | `landscape/02-detail-layers/02-8b-invariants-tors-costs-constants.md` |
| `2.8.5` | 2.8.5 What this costs the rules we already have | `landscape/02-detail-layers/02-8b-invariants-tors-costs-constants.md` |
| `2.8.6` | 2.8.6 Constants (for NUMBERS.md, Rule 14) | `landscape/02-detail-layers/02-8b-invariants-tors-costs-constants.md` |
| `2.8.7` | 2.8.7 THE FRAME REFUTED THE SUITE — nine invariants, none of which can see | `landscape/02-detail-layers/02-8c-frame-vs-suite-and-the-field-fix.md` |
| `2.8.8` | 2.8.8 AFTER THE FIELD FIX — the frame attributed, two constants re-derived, I7 repaired | `landscape/02-detail-layers/02-8c-frame-vs-suite-and-the-field-fix.md` |
| `2.9` | 2.9 Pale spires — the white rock formation (ruling, stage-4) | `landscape/02-detail-layers/02-9-pale-spires.md` |
| `2.9.1` | 2.9.1 The two rejections, because both are instances of rules this document already has | `landscape/02-detail-layers/02-9-pale-spires.md` |
| `2.9.2` | 2.9.2 THE RULE THAT SETTLES BOTH OF FLORA'S CONSTRAINTS AT ONCE | `landscape/02-detail-layers/02-9-pale-spires.md` |
| `2.9.3` | 2.9.3 The class, as a brief | `landscape/02-detail-layers/02-9-pale-spires.md` |
| `2.9.4` | 2.9.4 It is STONE, and three properties must not follow it across | `landscape/02-detail-layers/02-9-pale-spires.md` |
| `2.9.5` | 2.9.5 Two flags | `landscape/02-detail-layers/02-9-pale-spires.md` |
| `2.10` | 2.10 The landform dictionary (user-ratified, в18, stage-5) | `landscape/02-detail-layers/02-10-landform-dictionary.md` |
| `3` | 3. Water | `landscape/03-water.md` |
| `3.1` | 3.1 Rivers — must flow downhill | `landscape/03-water.md` |
| `3.2` | 3.2 Lakes and ponds | `landscape/03-water.md` |
| `3.3` | 3.3 Shoreline treatment | `landscape/03-water.md` |
| `3.4` | 3.4 Water and gameplay placement | `landscape/03-water.md` |
| `4` | 4. Terrain palette (splat rules a shader can implement) | `landscape/04-terrain-palette.md` |
| `4.1` | 4.1 THE PALE ROCK STRATUM — «белые скалы», and it is the material half of the banded massif | `landscape/04-terrain-palette.md` |
| `4.2` | 4.2 The display palette — the ramp budget (ruling, stage-4) | `landscape/04-terrain-palette.md` |
| `4.3` | 4.3 FULL COLOUR IS THE BASIS — what §4.2 leaves behind, and what it hands forward (user ruling, stage-5) | `landscape/04-terrain-palette.md` |
| `5` | 5. Flora catalog | `landscape/05-flora/05-0-global-rules-and-crown-aspect-ceiling.md` |
| `5.1` | 5.1 Dale Oak (broadleaf — the valley tree) | `landscape/05-flora/05-1-species-and-forest-floor-classes.md` |
| `5.2` | 5.2 Highland Pine (conifer — the slope tree) | `landscape/05-flora/05-1-species-and-forest-floor-classes.md` |
| `5.3` | 5.3 River Birch (accent — the water tree) | `landscape/05-flora/05-1-species-and-forest-floor-classes.md` |
| `5.4` | 5.4 Bush | `landscape/05-flora/05-1-species-and-forest-floor-classes.md` |
| `5.5` | 5.5 Flowers | `landscape/05-flora/05-1-species-and-forest-floor-classes.md` |
| `5.6` | 5.6 Grass | `landscape/05-flora/05-1-species-and-forest-floor-classes.md` |
| `5.7` | 5.7 Tall-tree revision — working the collisions through (stage-4) | `landscape/05-flora/05-1-species-and-forest-floor-classes.md` |
| `5.8` | 5.8 Maturity tiers — restoring fullness without restoring canopy | `landscape/05-flora/05-1-species-and-forest-floor-classes.md` |
| `5.9` | 5.9 Additional species (approved from flora's proposals) | `landscape/05-flora/05-1-species-and-forest-floor-classes.md` |
| `5.10` | 5.10 Forest floor classes (user-specified, stage-4) | `landscape/05-flora/05-1-species-and-forest-floor-classes.md` |
| `5.10a` | 5.10a THE MOSS RULING — an anchored class is explained by its ANCHOR, not by the ground (ruling, stage-5) | `landscape/05-flora/05-10a-moss-ruling-and-seasonal-palette.md` |
| `5.10a.1` | 5.10a.1 The arithmetic, which closes it before the definitions are opened | `landscape/05-flora/05-10a-moss-ruling-and-seasonal-palette.md` |
| `5.10a.2` | 5.10a.2 The definitional answer, and why NO row value exists | `landscape/05-flora/05-10a-moss-ruling-and-seasonal-palette.md` |
| `5.10a.3` | 5.10a.3 The PathMargin overshoot is the SAME defect, not a second one | `landscape/05-flora/05-10a-moss-ruling-and-seasonal-palette.md` |
| `5.11` | 5.11 Seasonal foliage — the palette contract (ruling, stage-4) | `landscape/05-flora/05-10a-moss-ruling-and-seasonal-palette.md` |
| `5.12` | 5.12 THE FOREST WAS EATING THE MOUNTAIN — the apron ruling (stage-4) | `landscape/05-flora/05-12-forest-apron-ruling.md` |
| `5.12a` | 5.12a SCOPE — the hole core measured in my own sentence (ruled 10.08.2026) | `landscape/05-flora/05-12-forest-apron-ruling.md` |
| `5.12b` | 5.12b THE ACCEPTANCE QUANTITY IS WRONG, AND THAT IS WHY THE MIDDLE LOOKED UNRULABLE | `landscape/05-flora/05-12-forest-apron-ruling.md` |
| `5.12f` | 5.12f THE JUNCTION'S THRESHOLD IS WITHDRAWN — Rule 41 has now fired TWICE on the same acceptance, and the second time it fired on the quantity Rule 41 itself installed | `landscape/05-flora/05-12-forest-apron-ruling.md` |
| `5.12g` | 5.12g WHAT WENT WRONG BOTH TIMES, since it is the same mistake and it is transmissible | `landscape/05-flora/05-12-forest-apron-ruling.md` |
| `5.12h` | 5.12h RULING — the junction SURVIVES, its AGGREGATION does not, and angular extent is REFUSED | `landscape/05-flora/05-12-forest-apron-ruling.md` |
| `6` | 6. Structures catalog (домики под разные задачи) | `landscape/06-structures.md` |
| `6.1` | 6.1 Castle — the seat of state power (ruling, stage-3) | `landscape/06-structures.md` |
| `6.1.1` | 6.1.1 The hierarchy ruling (the actual problem) | `landscape/06-structures.md` |
| `6.1.2` | 6.1.2 Siting rules | `landscape/06-structures.md` |
| `6.1.3` | 6.1.3 Footprint, mass, readability | `landscape/06-structures.md` |
| `6.1.4` | 6.1.4 Testbed placement (seed 1) | `landscape/06-structures.md` |
| `6.2` | 6.2 Dungeon entrances — archetypes (ruling, stage-3) | `landscape/06-structures.md` |
| `6.2.1` | 6.2.1 Adit (sloped ground, ≥ 6 m relief) | `landscape/06-structures.md` |
| `6.2.2` | 6.2.2 Sunken barrow (flat ground) — the flat-ground answer | `landscape/06-structures.md` |
| `6.2.3` | 6.2.3 Attractor status (C1/C2) | `landscape/06-structures.md` |
| `6.3` | 6.3 True-darkness places (stage-4 ruling) | `landscape/06-structures.md` |
| `7` | 7. Testbed application (worldgen v2, что core реализует первым) | `landscape/07-testbed.md` |
| `7.1` | 7.1 The plan (feature list, in pass order) | `landscape/07-testbed.md` |
| `7.1b` | 7.1b Acceptance vantages — the two frames (design, binding on the tour) | `landscape/07-testbed.md` |
| `7.0a` | 7.0a Re-siting the barrow after the L0 raise (stage-4 ruling) | `landscape/07-testbed.md` |
| `7.1a` | 7.1a Plan vs generated truth (seed 1, stage-3b probes) | `landscape/07-testbed.md` |
| `7.2` | 7.2 Why this layout satisfies the contracts | `landscape/07-testbed.md` |
| `7.3` | 7.3 Implementation order for core (highest impact first) | `landscape/07-testbed.md` |
| `8` | 8. The stand maps — briefs (user-ratified в1/в2/в5/в6/в15, stage-5) | `landscape/08-stand-maps.md` |
| `8.1` | 8.1 Stand 1 — FOREST: the walk-and-look map | `landscape/08-stand-maps.md` |
| `8.2` | 8.2 Stand 2 — RIVER + CASTLE: the 25–35 m river and the walled city | `landscape/08-stand-maps.md` |
| `9` | 9. Sources | `landscape/09-sources.md` |
| `10` | 10. THE OBJECT GRAMMAR — what stands on the heightmap (stage-5, from the 16 reference frames) | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.1` | 10.1 D2 — «равнина ухабистая» is a DISPERSION, and it is measured DETRENDED | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.1.1` | 10.1.1 The hole in the current contract | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.1.2` | 10.1.2 The instrument: `GROUND_RELIEF_SIGMA_20M` | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.1.3` | 10.1.3 The picture-side control: THE GROUND MUST CUT ITSELF | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.2` | 10.2 D2b — THE SUB-4-METRE BAND IS NOT THE HEIGHTMAP'S JOB, AND NO NUMBER OF OCTAVES WILL CHANGE THAT | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.3` | 10.3 D1 — nothing stands on an axis, and TILT IS NOT JITTER | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.3.1` | 10.3.1 The rule that most implementations get wrong | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.3.2` | 10.3.2 The tilt table | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.3.3` | 10.3.3 The verticals that survive, and what they owe | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.4` | 10.4 RULE 33 APPLIED TO SCATTER — the read-distance ladder, and the diagnosis it produces | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.4.1` | 10.4.1 THE FLATNESS COMPLAINT IS A MID-FIELD COMPLAINT | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.4.2` | 10.4.2 The corollary: A CLASS SERVES ONE BAND ONLY | `landscape/10-object-grammar/10-1-d1-d2-and-the-read-distance-ladder.md` |
| `10.5` | 10.5 THE PLACEMENT BRIEFS | `landscape/10-object-grammar/10-5-placement-briefs-b1-b9.md` |
| `B1` | B1 — BOULDERS (валуны), 0.8–4 m | `landscape/10-object-grammar/10-5-placement-briefs-b1-b9.md` |
| `B2` | B2 — ROCK OUTCROPS (выходы породы), 3–25 m | `landscape/10-object-grammar/10-5-placement-briefs-b1-b9.md` |
| `B3` | B3 — FENCE LINES (изгороди) — the cheapest thing in this document, and it is also an INSTRUMENT | `landscape/10-object-grammar/10-5-placement-briefs-b1-b9.md` |
| `B4` | B4 — TOWERS AND RUINS | `landscape/10-object-grammar/10-5-placement-briefs-b1-b9.md` |
| `B5` | B5 — KERBS, STEPS, RETAINING WALLS (бордюрчики) | `landscape/10-object-grammar/10-5-placement-briefs-b1-b9.md` |
| `B6` | B6 — SHRUB AND SCRUB CLUMPS (куртины кустарника) | `landscape/10-object-grammar/10-5-placement-briefs-b1-b9.md` |
| `B7` | B7 — LEANING DEAD TREES (наклонённые сухие деревья) | `landscape/10-object-grammar/10-5-placement-briefs-b1-b9.md` |
| `B8` | B8 — TIMBER SPANS AND BRIDGES | `landscape/10-object-grammar/10-5-placement-briefs-b1-b9.md` |
| `B9` | B9 — WINDMILL / WORKING STRUCTURE | `landscape/10-object-grammar/10-5-placement-briefs-b1-b9.md` |
| `10.6` | 10.6 BUILD ORDER, AND WHY THIS ORDER | `landscape/10-object-grammar/10-6-build-order-numbers-acceptance.md` |
| `10.7` | 10.7 NUMBERS REQUESTED (Rule 35 — via lead, to `docs/NUMBERS.md`) | `landscape/10-object-grammar/10-6-build-order-numbers-acceptance.md` |
| `10.8` | 10.8 ACCEPTANCE — the frame pairs (Rule 27) | `landscape/10-object-grammar/10-6-build-order-numbers-acceptance.md` |
| `10.9` | 10.9 HAZE — the two verdicts render is waiting on (stage-5) | `landscape/10-object-grammar/10-9-haze-verdicts.md` |
| `10.9.1` | 10.9.1 Verdict 1 — `LANDMARK_HAZE_ONSET` stays a SITING rule. CONFIRMED, and it stops being a tabled metre value | `landscape/10-object-grammar/10-9-haze-verdicts.md` |
| `10.9.2` | 10.9.2 Verdict 2, part one — THE PREMISE UNDER 1400 IS MINE, AND IT WAS WITHDRAWN BEFORE THIS CONVERSATION STARTED | `landscape/10-object-grammar/10-9-haze-verdicts.md` |
| `10.9.3` | 10.9.3 Verdict 2, part two — WHAT I AM OBLIGED TO PROTECT, as three checkable propositions | `landscape/10-object-grammar/10-9-haze-verdicts.md` |
| `10.9.4` | 10.9.4 What the height lever changes — and the one refinement it forces | `landscape/10-object-grammar/10-9-haze-verdicts.md` |
| `10.9.5` | 10.9.5 Propagation — the correction §1.6.1 owed and never paid | `landscape/10-object-grammar/10-9-haze-verdicts.md` |
| `10.10` | 10.10 THE ARMS CAME BACK AND MOVED THREE OF MY OWN LINES (stage-5) | `landscape/10-object-grammar/10-10-arms-moved-three-lines.md` |
| `10.10.1` | 10.10.1 H2 — WITHDRAWN from the haze question, ACCEPTED as a terrain defect, and it generalises to a rule | `landscape/10-object-grammar/10-10-arms-moved-three-lines.md` |
| `10.10.2` | 10.10.2 H1 — re-derived on p05, with the control known, and it becomes a BUDGET | `landscape/10-object-grammar/10-10-arms-moved-three-lines.md` |
| `10.10.3` | 10.10.3 H3 — RETIRED, and not as a demotion: it was §10.9.1 wearing a second hat | `landscape/10-object-grammar/10-10-arms-moved-three-lines.md` |
| `10.10.4` | 10.10.4 The frame-2 vantage — accepted, and the tabled coordinate has now rotted three times | `landscape/10-object-grammar/10-10-arms-moved-three-lines.md` |
| `10.10.5` | 10.10.5 A1 ALREADY HAS ITS «BEFORE» FRAME, AND IT WAS SHOT FOR ANOTHER QUESTION | `landscape/10-object-grammar/10-10-arms-moved-three-lines.md` |
| `10.11` | 10.11 CLOSING THE HAZE LOOP — and running Rule 47 across my own criteria (stage-5) | `landscape/10-object-grammar/10-11-closing-the-haze-loop.md` |
| `10.11.1` | 10.11.1 The Rule 34 flag is closed, and H1 is RATIFIED | `landscape/10-object-grammar/10-11-closing-the-haze-loop.md` |
| `10.11.2` | 10.11.2 Rule 48 has a POSITIVE form, and it is what licenses H1 | `landscape/10-object-grammar/10-11-closing-the-haze-loop.md` |
| `10.11.3` | 10.11.3 RULE 47 RUN ACROSS MY OWN CRITERIA — three are exposed, and core is measuring two of them this week | `landscape/10-object-grammar/10-11-closing-the-haze-loop.md` |
| `10.12` | 10.12 σ WAS THE WRONG INSTRUMENT — D2 re-derived on the gradient (stage-5) | `landscape/10-object-grammar/10-12-sigma-was-the-wrong-instrument.md` |
| `10.12.1` | 10.12.1 σ is RETIRED as a gate — not re-floored | `landscape/10-object-grammar/10-12-sigma-was-the-wrong-instrument.md` |
| `10.12.2` | 10.12.2 The replacement is not a better proxy — it is the thing itself | `landscape/10-object-grammar/10-12-sigma-was-the-wrong-instrument.md` |
| `10.12.3` | 10.12.3 THE ACTIONABLE FINDING — the approved meso band straddles the failure line | `landscape/10-object-grammar/10-12-sigma-was-the-wrong-instrument.md` |
| `10.12.4` | 10.12.4 §2.7's fifth octave — REASSIGNED, not deleted | `landscape/10-object-grammar/10-12-sigma-was-the-wrong-instrument.md` |
| `10.12.5` | 10.12.5 LF-8 — REBUILT ON CONNECTIVITY, and the rebuild makes it truer | `landscape/10-object-grammar/10-12-sigma-was-the-wrong-instrument.md` |
| `10.12.6` | 10.12.6 The authored clearing (в9) — EXEMPT, with a derived bound, ruled explicitly rather than by silence | `landscape/10-object-grammar/10-12-sigma-was-the-wrong-instrument.md` |
| `10.12.7` | 10.12.7 The mid-ground result — and the number to report is 8, not 17 | `landscape/10-object-grammar/10-12-sigma-was-the-wrong-instrument.md` |
| `10.13` | 10.13 STATE AT WIND-DOWN — handoff, open items, and what would reopen each ruling | `landscape/10-object-grammar/10-13-state-at-wind-down.md` |
| `10.13.1` | 10.13.1 The D2 problem statement — recorded standalone, because it is worth a day to whoever reads it next | `landscape/10-object-grammar/10-13-state-at-wind-down.md` |
| `10.13.2` | 10.13.2 The three rulings, with their alternatives and costs | `landscape/10-object-grammar/10-13-state-at-wind-down.md` |
| `10.13.3` | 10.13.3 §2.7's fifth octave — done, and done harder than asked | `landscape/10-object-grammar/10-13-state-at-wind-down.md` |
| `10.13.4` | 10.13.4 Open items carried forward — the register | `landscape/10-object-grammar/10-13-state-at-wind-down.md` |
| `10.13.5` | 10.13.5 Not mine, recorded so it is not lost | `landscape/10-object-grammar/10-13-state-at-wind-down.md` |
| `10.14` | 10.14 THE MAN-MADE MID-GROUND — B3–B9 released, and three of my own briefs corrected on the way | `landscape/10-object-grammar/10-14-man-made-mid-ground.md` |
| `10.14.1` | 10.14.1 What step 1 licenses, and the one thing it does not | `landscape/10-object-grammar/10-14-man-made-mid-ground.md` |
| `10.14.2` | 10.14.2 B3 — FENCE LINES. RELEASED, and my "it is also an INSTRUMENT" claim needed repair before it was true | `landscape/10-object-grammar/10-14-man-made-mid-ground.md` |
| `10.14.3` | 10.14.3 B5 — KERBS, STEPS, RETAINING WALLS. RELEASED, and one requested row deleted before it was approved | `landscape/10-object-grammar/10-14-man-made-mid-ground.md` |
| `10.14.4` | 10.14.4 B4 — TOWERS. RELEASED, and I have to correct my own correction to `REFERENCE_FRAMES.md` | `landscape/10-object-grammar/10-14-man-made-mid-ground.md` |
| `10.14.5` | 10.14.5 B8 and B9 — released with **no new numbers for B8 at all** | `landscape/10-object-grammar/10-14-man-made-mid-ground.md` |
| `10.14.6` | 10.14.6 What is deliberately NOT requested | `landscape/10-object-grammar/10-14-man-made-mid-ground.md` |
| `10.14.7` | 10.14.7 NUMBERS REQUESTED (Rule 35 — via lead) | `landscape/10-object-grammar/10-14-man-made-mid-ground.md` |
| `10.14.8` | 10.14.8 ACCEPTANCE — A2 corrected, A7 added (Rule 27) | `landscape/10-object-grammar/10-14-man-made-mid-ground.md` |
| `10.15` | 10.15 THE FOUR OPEN ITEMS — closed, and three of the four close against my own lines | `landscape/10-object-grammar/10-15-four-open-items-closed.md` |
| `10.15.1` | 10.15.1 REJECTION 3 — the span floor is RETIRED today, and the replacement is a ratio, not a new dimension | `landscape/10-object-grammar/10-15-four-open-items-closed.md` |
| `10.15.2` | 10.15.2 The 229 m² floor — RETIRED as a gate, re-denominated, and the 2.5× margin is probably hiding a LOSS | `landscape/10-object-grammar/10-15-four-open-items-closed.md` |
| `10.15.3` | 10.15.3 `CROWN_BASE_FRACTION_MIN` — the fraction goes, and the absolute that replaces it is NOT 2.2 m | `landscape/10-object-grammar/10-15-four-open-items-closed.md` |
| `10.15.4` | 10.15.4 The clearing в9 — the exemption is WITHDRAWN, because I misread the contract I was exempting | `landscape/10-object-grammar/10-15-four-open-items-closed.md` |

