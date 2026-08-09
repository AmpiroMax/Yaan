<!--
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 13:28:27
-->
<!--
UPD:
- 09:08:2026 - 00:16:55: Stage 1 — public contract documented (headers only, no implementation yet).
- 09:08:2026 - 00:42:03: Stage 2 — Chunk/Worldgen/ChunkManager implemented; ChunkManager::open_generated added (in-memory generator, lead directive — .dfw IO and SaveDelta deferred to stage 3); value-noise gentle hills with global quantization range for exact edge stitching; suites tests/core/{WorldgenTests,ChunkManagerTests}.cpp.
- 09:08:2026 - 11:05:22: Stage 3b — worldgen v2 per LANDSCAPE.md: pass modules (Macro/Hydrology/Sites/Scatter/Validation + TestbedLayout + SiteComponents), WORLDGEN_MAX_HEIGHT quantization, Chunk gained SurfaceData + scatter, ChunkManager gained surfacefield/scatter/water_bodies and site-entity component attachment; suite tests/core/WorldgenV2Tests.cpp.
- 09:08:2026 - 13:12:19: Stage 3b design amendments: derived fords (HydrologyData::ford_stations), §3.3 mud cap + ford bed raise, canopy-aware C1 with clearance factor + sight wedges, pine ridge strips, dist_to_water saturation at DIST_TO_WATER_RANGE, grid-pass chunk generation + binned hydrology (large-domain contexts 11x faster).
- 09:08:2026 - 13:28:27: P1 anisotropy retune (§2.1): mid octave input-stretched along the drifting axis field (fixed-frame blending); WorldgenWater.cpp split from WorldgenHydrology.cpp (query vs build).
-->

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
