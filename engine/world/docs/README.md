
# engine/world

## Responsibility

The world layer (pure data + generation, core-only per Rule 1): chunk data types
(Q48), the .dfw world file format (Q13, Q49), chunk streaming with batch ECS
operations (Rule 11), offline deterministic worldgen (Rule 13.1), and save
deltas (Q56).

## Key types

- `ChunkCoord`, `chunk_group`, `Heightmap`, `WorldEntityId`,
  `GeneratedEntityRecord`, `Chunk` (`sources/Chunk.h`) — chunk data model;
  `Heightmap::view()` yields the frozen `math::HeightFieldView` boundary type.
- `WORLD_MAGIC`, `WorldInfo`, `WorldFileReader`/`WorldFileWriter`
  (`sources/WorldFormat.h`) — .dfw container; per-chunk sections for selective
  streaming reads.
- `ChunkManager`, `ChunkLoaded`/`ChunkUnloaded`, `ChunkStreamingParams`
  (`sources/ChunkManager.h`) — residency around the focus position; batch
  spawn/destroy per chunk; synchronous events (unload fires before free).
  Also the far-terrain half: `world_bounds_xz()` (the GENERATED extent),
  `request_coarse_nodes()` (async, per-update row budget, nearest-to-focus
  first), `coarse_heightfield()` / `coarse_surfacefield()`,
  `release_coarse_node()` (the ONLY thing that frees a delivered node).
- `CoarseNode`, `CoarseNodeData`, `build_coarse_rows`
  (`sources/CoarseTerrain.h`) — a coarse LOD node IS a `math::HeightFieldView`:
  129 samples with the shared edge row, step = the level's voxel size
  (1/4/8/16/32/64 m), 128 voxels per node at every level, world origin =
  node coord * node size on a grid rooted at world zero so growing the world
  renumbers nothing. Nodes are built a few rows per update, not all at once.
- `generate_world`/`generate_chunk`, `WorldGenParams` (now carrying the
  `TestbedLayout`), `WorldGenContext`/`build_world_context`,
  `terrain_height`/`surface_point`, `WorldGenRng` (`sources/Worldgen.h`) —
  offline, seeded, byte-identical output. v2 pass modules:
  `WorldgenNoise/Macro/Hydrology/Sites/Scatter/Validation` (P1 stamps +
  valley redistribution, P2 river/lake/fords with the monotonic water
  invariant, P3 surface classification, P4 pads/sites, P5 scatter, C1/§2.4
  validation). `TestbedLayout` (`sources/TestbedLayout.h`) is the LANDSCAPE
  §7.1 layout table as generator input data; `SiteMarker`/`site_archetype`
  (`sources/SiteComponents.h`) the site component + placeholder archetypes.
- `SaveDelta`, `EntityDelta`, `DynamicSpawn`, `SaveDeltaCodec`,
  `SaveSectionHooks` (`sources/SaveDelta.h`) — delta-vs-generated-world saves;
  gameplay contributes sections via registered hooks.

## Usage example

```cpp
dfn::world::ChunkManager chunks;
// Stage 2: in-memory deterministic generator (open(file) arrives in stage 3).
chunks.open_generated({seed, {-10, -10}, {10, 10}},
                      {static_cast<uint32_t>(dfn::config::CHUNK_LOAD_RADIUS),
                       static_cast<uint32_t>(dfn::config::CHUNK_UNLOAD_RADIUS)});
chunks.update(player_pos, ecs, bus);            // loads/unloads, batch ECS ops
auto hf = chunks.heightfield({3, -2});          // -> render mesher / physics
float ground = chunks.height_at({812.f, -95.f}).value_or(0.f);
```

## Dependencies

Uses `engine/core/{ecs,events,math,serialization,config}` and glm only — no
physics, no rendering (Rule 1). Used by engine/app (owns ChunkManager, wires
events to render/physics), tools/worldgen CLI, gameplay (save hooks,
height queries via app-provided access), tests (determinism, format round-trip).

## §2.7 general relief and §10.5 B2 outcrops (added 11:08:2026 - 14:58:39)

- `WorldgenRelief.h` — the general ground relief: the meso octave (25-60 m /
  1.5-4 m, previously a NUMBERS row with no consumer anywhere) plus micro,
  masked by shore / corridor / massif and rank-equalized so the realized band
  is the declared band. Applied in exactly ONE place: `compose_passes`.
- `WorldgenOutcrop.h` — §10.5 B2 slabs and bosses as TERRAIN, not meshes
  (§10.2: the heightmap owns 4 m and up). Anchored on convex curvature,
  forbidden in hollows, bedding dip coherent over 200 m.
- `WorldgenValidation.h` — `ground_relief_20m()`, §10.1's detrended bumpiness
  instrument, plus `relief_floor_binds()` whose exemption list must stay equal
  to WorldgenRelief's masks.
- `WorldgenScatter.h` — `build_scatter()` now takes the whole `WorldGenContext`.
  It used to take six pieces and hold its own copy of the pass stack; that copy
  never learned about the relief pass and instances floated or sank by up to
  0.59 m. A signature that cannot express "some of the passes" cannot drift.
- COUNTERFACTUAL: `DFN_NO_RELIEF=1` stands the relief, the rock and the boulder
  pass down together, reproducing the previous world byte for byte.
