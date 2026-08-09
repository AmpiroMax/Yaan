<!--
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:42:03
-->
<!--
UPD:
- 09:08:2026 - 00:16:55: Stage 1 spec — contracts for engine/core and engine/world; boundary agreements with render, sim, and lead recorded.
- 09:08:2026 - 00:42:03: Stage 2 — ECS/time/events/math implemented (+ ContentHash for the determinism test); world: value-noise worldgen with global quantization range (exact edge stitch), ChunkManager streaming via open_generated (in-memory generator, lead directive; .dfw IO + SaveDelta deferred to stage 3). Resolved former open items: SIM_MAX_CATCHUP_STEPS and chunk radii landed in NUMBERS.md; gen_constants contract confirmed (dfn_generated/Constants.h); shared components authored by lead in Components.h. Test suites registered in tests/core.cmake.
-->

# Spec: `core` (engine/core + engine/world)

## Zone of responsibility

Everything under `engine/core` and `engine/world` (Rule 25):

- `engine/core/ecs` — the ECS: generational entities, sparse-set component pools,
  views, resources, batch spawn/destroy, entity-to-group (chunk) index.
- `engine/core/math` — thin glm extensions: Aabb, Frustum, Ray, intersections,
  and the cross-zone `HeightFieldView` boundary type.
- `engine/core/time` — monotonic Clock + FixedTimestep accumulator (Rule 12).
- `engine/core/events` — typed EventBus (immediate + queued dispatch).
- `engine/core/types` — TypeId (no RTTI), generic `Handle<Tag>`.
- `engine/core/serialization` — Rule 7 section-based BinaryWriter/BinaryReader,
  frozen FNV-1a 64 content hash.
- `engine/core/config` — consuming side of the constants generated from
  NUMBERS.md.
- `engine/world` — chunk data types, .dfw world format, ChunkManager streaming,
  offline deterministic worldgen API, save-delta types and codec.

NOT in the zone: `engine/core/components` (lead-owned shared components — shapes
proposed, files authored by the lead).

## Public interface

All paths repo-root absolute; namespace root `dfn`. Stage 1 = declarations +
doc comments; template/method bodies land in stage 2 without changing signatures.

### engine/core/ecs (namespace `dfn::ecs`)

- `sources/EntityId.h` — `EntityId {uint32 index, uint32 generation}`; `null()`,
  `is_null()`, `packed()`, `EntityIdHash`. `GroupId` (uint64, `NO_GROUP = 0`) —
  opaque streaming group key.
- `sources/ComponentPool.h` — `IComponentPool` (type-erased: `remove`,
  `remove_batch`, `has`, `size`, `clear`); `ComponentPool<T>` (`add`, `get`,
  dense access `data_at`/`owner_at`). Components are plain movable structs
  (Rule 8); pools store them in `std::vector<T>` (components holding
  `std::string`/`std::vector` members are supported — moves preserved).
- `sources/View.h` — `View<Ts...>` yielding `(EntityId, Ts&...)`; smallest pool
  drives; iteration order unspecified; `each(fn)` convenience. No exclusion
  filters, no sorting (frozen without them — sim confirmed not needed, stage 2).
- `sources/World.h` — `World`: `spawn`/`destroy`/`destroy_deferred`/
  `flush_destroyed`/`alive`/`entity_count`/`clear`; **batch (Rule 11)**:
  `spawn_batch(span<EntityId> out, GroupId)`, `destroy_batch(span<const EntityId>)`,
  `destroy_group(GroupId)`, `add_batch<T>(ids, prototype)` and
  `add_batch<T>(ids, values)` (one pool visit per call — chunk streaming attaches
  Transform/PreviousTransform/RenderMesh/LocalBounds cheaply, per lead request);
  **group index (Q22)**: `set_group`, `group_of`, `entities_in_group`;
  components: `add`/`remove`/`get`/`has`; queries: `view<Ts...>()`; resources:
  `add_resource`/`resource`/`has_resource` (Rule 10). Single-threaded by contract.

### engine/core/math (namespace `dfn::math`)

- `sources/Aabb.h` — `Aabb {min, max}` (meters), expand/contains/overlaps/
  transformed; default-constructed = inverted-empty for expand-from-nothing.
- `sources/Frustum.h` — `Plane`, `Containment {Outside, Intersects, Inside}`,
  `Frustum::from_view_proj` (Gribb-Hartmann, normals point inside),
  `classify(Aabb)`, `classify_sphere`, `visible`.
- `sources/Ray.h` — `Ray {origin, unit direction}`, factories normalize.
- `sources/Intersect.h` — `RayHit {t, point, normal}`; `ray_vs_aabb`,
  `ray_vs_sphere`, `ray_vs_triangle` (Moeller-Trumbore), `ray_vs_plane`,
  `aabb_vs_aabb`. Pure functions.
- `sources/HeightField.h` — **FROZEN boundary contract** (see Dependencies):
  `HeightFieldView {ivec2 chunk_coord, vec2 origin, uint32 resolution, float
  step, span<const uint16> heights, float height_scale, float height_offset}`;
  row-major, x fastest; +X east, +Z south, Y up;
  `height_m = height_offset + raw * height_scale` (scale = meters per raw unit,
  worldgen precomputes `(max-min)/65535`); shared edge rows between neighbors.

### engine/core/time (namespace `dfn::time`)

- `sources/Clock.h` — monotonic `Clock::tick() -> double seconds`. App/tools
  only; gameplay never reads wall-clock (Rule 12).
- `sources/FixedTimestep.h` — `FixedTimestep(step_dt, max_catchup_steps)`;
  `accumulate(frame_dt) -> uint32 steps` (excess dropped past the clamp),
  `alpha() -> double [0,1)` for render interpolation, `reset()`. Values come
  from generated constants; `SIM_MAX_CATCHUP_STEPS` pending in NUMBERS.md.

### engine/core/events (namespace `dfn::events`)

- `sources/EventBus.h` — `subscribe<E>(handler) -> SubscriptionId`,
  `unsubscribe`, `publish<E>` (synchronous — used for ChunkLoaded/ChunkUnloaded
  ordering guarantees), `post<E>` + `pump()` (queued gameplay events; pump once
  per tick by app). Events are plain copyable structs; single-threaded.

### engine/core/types (namespace `dfn::types`)

- `sources/TypeId.h` — `TypeId` (size_t), `type_id<T>()` counter-based, no RTTI;
  never serialized (not stable across runs — Rule 7 formats use explicit tags).
- `sources/Handle.h` — `Handle<Tag, Storage=uint32> {value; valid(); invalid()}`,
  0 = invalid (matches platform-interface handle convention); `HandleHash`.

### engine/core/serialization (namespace `dfn::serialization`)

- `sources/BinaryWriter.h` — `SectionTag` + `make_tag(a,b,c,d)`; `BinaryWriter`:
  `begin_file(magic, version)`, `begin_section(tag, section_version)` /
  `end_section()` (length back-patched), explicit-LE primitives (`write_u8..u64`,
  `i8..i64`, `f32/f64` as IEEE bits, `bool`, `bytes`, `string` = u32 len + UTF-8),
  `buffer()`, atomic `save_to_file`. Container layout:
  `magic:u32 version:u32 (tag:u32 section_version:u16 byte_length:u64 payload)*`.
- `sources/BinaryReader.h` — `open(span, expected_magic)` / `open_file`;
  `container_version()`; `next_section() -> optional<SectionInfo>` (skips any
  unread remainder — this is skip-unknown, Rule 7); bounds-checked `read_*` with
  latched `ok()` (corrupt data fails soft, never crashes).
- `sources/ContentHash.h` — `fnv1a64(bytes|string_view)`; streaming `Fnv1a64`
  with `update`, `update_u64` (LE bytes), `update_length_prefixed` (mandatory
  for multi-field identities). **Algorithm frozen forever** (names voice files
  on disk, Q79): FNV-1a 64, basis 14695981039346656037, prime 1099511628211,
  seedless, platform-independent.

### engine/core/config (namespace `dfn::config`)

- `sources/Constants.h` — single include point re-exporting
  `dfn_generated/Constants.h` (emitted by lead-owned `tools/gen_constants` at
  build time). Contract: `inline constexpr`, names exactly as NUMBERS.md tables,
  types by kind (`SIM_TICK_RATE` uint32, `SIM_DT` double, speeds/sizes float,
  counts uint32).

### engine/world (namespace `dfn::world`)

- `sources/Chunk.h` — `ChunkCoord {int32 x, z}`; `chunk_group(coord) -> GroupId`
  (bit 63 set — never collides with NO_GROUP) and `chunk_of_group`;
  `chunk_at_position(vec2)`; `Heightmap {vector<uint16> samples, height_scale,
  height_offset}` with `view(coord) -> math::HeightFieldView`, `height_at`,
  bilinear `sample_world`; `WorldEntityId` (uint64, deterministic, worldgen-
  assigned — the save-delta anchor, Q56); `GeneratedEntityRecord {world_id,
  archetype (fnv1a64 of content id), position_xz, yaw}`; `Chunk {coord,
  heightmap, entities}`.
- `sources/WorldFormat.h` — `.dfw` container: `WORLD_MAGIC` 'DFNW', version 1;
  sections `INFO` (WorldInfo: seed, worldgen_version, min/max chunk), one
  `CHNK` per chunk + optional `ENTS` (per-chunk sections so streaming reads
  decode selectively). `WorldFileReader` (open = index only; `load_chunk` on
  demand), `WorldFileWriter` (worldgen/editor only — the game never writes .dfw,
  Q13; deterministic chunk order = part of byte-identical output).
- `sources/ChunkManager.h` — events `ChunkLoaded`/`ChunkUnloaded` (published
  synchronously; **unload fires before memory is freed**); `ChunkStreamingParams
  {load_radius, unload_radius}` (hysteresis; NUMBERS entries pending);
  `ChunkManager`: `open(world_file, SaveDelta*, params)`, `update(focus, ecs,
  bus)` (load: decode -> delta overlay -> `spawn_batch(group)` -> `add_batch`
  prototypes -> publish; unload: publish -> `destroy_group` -> free),
  `unload_all`, `is_loaded`, `loaded_chunks`, `heightfield(coord)`,
  `chunk(coord)`, `height_at(world_xz)`.
- `sources/Worldgen.h` — `WorldGenParams {seed, min/max chunk}`;
  `generate_world(params, out_file) -> WorldGenResult` (byte-identical output,
  Rule 13.1); `generate_chunk(params, coord)` (bit-identical to the same chunk
  in a full run; chunk depends only on params + coord); `WorldGenRng`
  (SplitMix64 streams keyed by seed/coord/pass, rejection-sampled ranges — no
  modulo bias).
- `sources/SaveDelta.h` — `.dfs` container: `SAVE_MAGIC` 'DFNS', version 1;
  sections `META`, `EDLT`, `DSPN` + registered module sections.
  `EntityDelta {world_id, Kind {Destroyed, Moved, StateChanged}, position_xz,
  yaw}`; `DynamicSpawn` (per-save `DynamicEntityId`); `ChunkDelta` buckets;
  `SaveDelta {world_seed, chunks}`; `SaveSectionHooks {tag, version, write(W&,
  const ecs::World&), read(R&, ecs::World&, stored_version) -> bool}`;
  `SaveDeltaCodec {register_section, write_save, read_save}` — unknown sections
  preserved verbatim across read/rewrite.

## Internal design

- **ECS storage** (stage 2): `World::Impl` holds generation/alive vectors, a free
  list, `unordered_map<TypeId, unique_ptr<IComponentPool>>`, deferred-destroy
  list, resources map, and the group index (`unordered_map<GroupId,
  vector<EntityId>>` + per-entity back-reference for O(1) `set_group`).
  Semantics inherited from Quicky's proven ECS (sparse map -> dense array,
  swap-and-pop); `remove_batch` sorts the batch per pool to amortize lookups.
  `destroy_batch` visits each pool once with the whole batch instead of
  N-entities x M-pools virtual calls.
- **View**: smallest pool drives; others probed via sparse map. Kept allocation-
  free (Quicky's `std::function entity_at_fn_` will be replaced by an index into
  the pool tuple).
- **FixedTimestep**: classic accumulator; `alpha = accumulator / step_dt` after
  extracting whole steps; catch-up clamp drops excess time (documented).
- **EventBus**: per-TypeId subscriber vectors; queued events stored in a
  type-erased buffer with a dispatch thunk per type; `pump()` loops until the
  queue is empty so handler-posted events deliver in the same pump.
- **Serialization**: writer back-patches section lengths (single growable
  buffer); reader indexes nothing eagerly except the header — `next_section`
  walks tags, `section_end_` guards every read. Atomic file writes = temp file
  + rename in the destination directory.
- **Heightmap encode**: worldgen computes per-chunk min/max, stores
  `offset = min`, `scale = (max - min) / 65535`, quantizes samples; shared edge
  rows are generated from the same noise field so neighbor chunks agree exactly.
- **ChunkManager**: resident map `unordered_map<packed coord, Chunk + scratch>`;
  per-update budget (chunks loaded per tick) is an implementation knob surfaced
  to NUMBERS when tuned. Save-delta overlay: `Destroyed` records filter the
  spawn list, `Moved` patch records before spawn, `DynamicSpawn` append.
- **Worldgen determinism**: all passes draw from `WorldGenRng` streams keyed by
  (seed, chunk, pass); output writing follows a fixed chunk order; no floats in
  file layout decisions. Determinism test: generate twice, compare file hashes
  (and cross-check `generate_chunk` vs full-run chunks).

## Dependencies

Uses: C++ std, glm (Rule 2), generated constants. Nothing else — `core` depends
on nothing, `world` depends on `core` only (Rule 1).

Used by: every other zone. Boundary agreements (Rule 26), all confirmed in
stage-1 messages:

1. **HeightFieldView (core <-> render <-> sim) — FROZEN.** Lives in
   `engine/core/math/sources/HeightField.h` because world, render, and physics
   are DAG siblings; all three include core. world::ChunkManager produces it;
   render's TerrainMesher (engine/render) triangulates into IRenderer Vertex —
   triangulation is render's, not world's; sim's physics feeds it to
   `IPhysics::create_terrain`, converting raw -> float meters on their side.
   Formula `height_m = offset + raw * scale` (meters per raw unit); row-major,
   x fastest; +X east, +Z south, Y up; shared edge rows; lifetime from
   ChunkLoaded until after ChunkUnloaded dispatch (unload published before
   free). Render confirmed final; sim confirmed (their Request B).
2. **Chunk event wiring (app, lead).** Render/physics cannot include world, so
   the app layer subscribes to ChunkLoaded/ChunkUnloaded and passes the
   HeightFieldView to their systems. Flagged to the lead; group-sync item.
3. **Shared components (lead-owned).** Transform {vec3 position, quat rotation
   {1,0,0,0}, vec3 scale{1}} + PreviousTransform (same fields, written by sim
   tick, read by render interpolation) proposed by me and render identically;
   lead authored them in `engine/core/components/sources/Components.h` along
   with CameraPose/PreviousCameraPose/RenderMesh/LocalBounds. My batch API
   (`spawn_batch` + `add_batch` prototypes) makes the chunk-spawn component set
   cheap, per lead's request. Open sync item: LocalBounds as raw min/max vs
   `math::Aabb`.
4. **ECS surface (core <-> sim, core <-> render) — confirmed by both.**
   dfn::ecs, include `engine/core/ecs/sources/World.h`; view iteration order
   unspecified; components with std::vector/std::string members supported; no
   exclusion filters/sorting in stage 2 (sim explicitly declined); Rule 8/15
   patterns (NpcActionQueue as plain component) fit without ECS extensions.
5. **Save section hooks (world <-> sim) — confirmed.** `SaveSectionHooks` /
   `SaveDeltaCodec::register_section` as specified above; gameplay migrates its
   sections internally via `stored_version`; container-level migration is mine;
   unknown sections preserved. Sim's NPC save-state list (position, schedule
   phase, stats counters, inventory, flags) maps to their own sections +
   `EntityDelta::StateChanged` markers.
6. **Content hash (core <-> sim, render) — confirmed.** FNV-1a 64 from
   `ContentHash.h`, frozen parameters, `update_length_prefixed` for composite
   identities (voice segments, Q79/Q80). Render uses it for asset name hashes
   (truncated to 32 in RenderMesh fields unless the lead widens them — sync
   item).
7. **gen_constants (core <-> lead).** Emit path `<build>/dfn_generated/
   Constants.h`, namespace dfn::config, names/types as documented in
   `config/sources/Constants.h`. Communicated to the lead; awaiting objection.

## Step-by-step plan

Stage 1 (this changeset): spec + public headers + module docs; boundary
agreements recorded above. Zero .cpp.

Stage 2 (skeleton, Q37/Q51), in dependency order:
1. `types` + `math` bodies; doctest suites (Aabb/Frustum/Intersect edge cases).
2. `ecs` bodies (World::Impl, pools, view, groups, batches); unit tests incl.
   generation-reuse, batch spawn/destroy, group churn.
3. `time` + `events` bodies; accumulator determinism test, bus ordering test.
4. `serialization` bodies; round-trip + truncation/corruption + skip-unknown
   tests; hash golden values pinned.
5. `world` Chunk/WorldFormat bodies; format round-trip test.
6. Worldgen v0: flat-ish testbed terrain via noise (one pass), deterministic;
   the Rule 13.1 double-generation test lands with the FIRST worldgen commit.
7. ChunkManager v0: residency ring, batch spawn of terrain-only chunks, events;
   integration test with null backends (Rule 3) — one flat chunk under the
   walking skeleton (Q51).
8. SaveDeltaCodec v0: META/EDLT/DSPN round-trip; hooks registry (gameplay
   sections arrive when sim lands theirs).
Each numbered step ends green (build + tests + header_check) before the next.

## How it is verified

- `python3 tools/header_check.py --all` (stage 1: passes now).
- Stage 2 doctest suites per module (headless, null backends — Rule 3):
  ECS lifecycle/generational safety/batch semantics; math golden cases; ring
  accumulator; bus ordering; serialization round-trip + corrupt-input fuzzing;
  world format round-trip.
- **Worldgen determinism test (Rule 13.1, non-negotiable): same params ->
  byte-identical .dfw, run from the first worldgen commit**; plus
  `generate_chunk` vs full-run cross-check.
- Save tests: write -> read -> write produces identical bytes (incl. preserved
  unknown sections); version-migration fixtures from version 1 on.
- Streaming integration test: scripted focus path over the testbed, asserts
  batch-only ECS mutation (spawn/destroy call counters), event ordering
  (ChunkUnloaded before free), no leaks (entity_count returns to baseline).
- Visual verification of anything render-visible happens via render's
  screenshot tour (Rule 27) — my terrain data feeds it in stage 2.

## What this zone does NOT do

- No rendering, no meshing: HeightFieldView is data; triangulation, LOD, and
  IRenderer calls are engine/render's (agreed, Rule 26).
- No physics: terrain collision bodies belong to engine/physics via IPhysics.
- No platform/backend includes ever (Rule 1); no interfaces/ folders (Rule 0).
- No gameplay logic: no stats, dice, NpcAction, quests, dialogue — engine/
  gameplay's. World's GeneratedEntityRecord stores only archetype id + pose;
  instantiating components from archetypes is gameplay's/lead's wiring.
- No shared component authoring: engine/core/components is lead-owned; I only
  propose shapes.
- No runtime world generation: worldgen runs offline via tools/worldgen (Q13);
  the game reads .dfw and writes only save deltas (Q56).
- No content parsing (JSON/TOML) in stage 1-2 contracts; when it arrives it
  stays in core/serialization per the tree, but is a separate sync.
- No threading promises: World and EventBus are single-threaded by contract;
  parallelism (chunk IO worker) stays inside implementations.
- No user-facing strings (Rule 5); tool/error strings are developer-facing only.
