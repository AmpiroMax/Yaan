/*
Created: 09:08:2026 - 00:16:55
Last updated: 11:08:2026 - 15:15:55
Module: engine/world
File: engine/world/sources/Worldgen.h

Responsibility:
- The offline world generation API (Q13): seeded, strictly deterministic
  (Rule 13.1), produces a .dfw world file. The game NEVER generates at runtime —
  tools/worldgen (lead-owned CLI) calls this library and the game only reads
  the result.

Key items:
- WorldGenParams: seed + extent + TestbedLayout (LANDSCAPE §7.1 layout table).
- WorldGenContext / build_world_context: world-level P2/P4 passes, built once.
- terrain_height / surface_point: the final height field and P3 outputs.
- generate_world(): params -> .dfw file on disk.
- generate_chunk(): single-chunk generation, exposed for determinism tests.

Dependencies:
- Uses: Chunk.h, TestbedLayout.h, WorldFormat.h, WorldgenHydrology.h,
  WorldgenSites.h, engine/core/config.
- Used by: tools/worldgen CLI, determinism tests (Rule 13.1, first-commit test),
  editor re-generation flows (via lead).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM IS NON-NEGOTIABLE (Rule 13.1): same params -> byte-identical .dfw
  on every platform. No std::rand, no unordered iteration into output, no float
  fast-math dependence; all randomness flows from the seeded engine below.
- WorldEntityIds are assigned deterministically (chunk-ordered, stable across
  regeneration with the same params) — save deltas depend on it (Q56).
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — offline deterministic worldgen API
  (Q13, Rule 13.1) with per-chunk entry point for tests.
- 09:08:2026 - 11:05:22: Stage 3b — worldgen v2 (LANDSCAPE.md): WorldGenParams
  gains the TestbedLayout field (additive, lead-approved); WorldGenContext +
  build_world_context so streaming builds hydrology/sites once; terrain and
  surface queries exposed for validation/tests.
- 10:08:2026 - 10:40:28: LF-8 (§2.10, в17): WorldGenContext carries the baked ErosionGrid.
  Empty unless the layout declares the pass, and sampling an empty grid is 0 —
  so the QUERY is unconditional and only the BUILD is gated (Rule 32).
- 10:08:2026 - 10:52:15: §8.1 path network on WorldGenContext (в7/в24). Empty on stands
  that declare no paths, and an empty network reports "far from any path", so
  consumers need no stand check.
- 10:08:2026 - 10:55:03: BR-6 find layer on WorldGenContext (в20).
- 10:08:2026 - 19:55:51: compose_passes() published: the pass stack had three
  open-coded copies (terrain_height, generate_chunk, the coarse node builder)
  and two of them were never told when the forest stand's branch landed. One
  definition now, called by all three.
- 11:08:2026 - 15:15:55: compose_passes takes the WaterSample: §2.7's relief tapers across the shore band and needs dist_to_water. No caller pays a field evaluation for it -- all three already held the sample.
*/

#pragma once

#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/TestbedLayout.h"
#include "engine/world/sources/WorldFormat.h"
#include "engine/world/sources/WorldgenErosion.h"
#include "engine/world/sources/WorldgenFinds.h"
#include "engine/world/sources/WorldgenHydrology.h"
#include "engine/world/sources/WorldgenPaths.h"
#include "engine/world/sources/WorldgenSites.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace dfn::world {

/// Inputs of a full world generation. Everything influencing output is HERE —
/// if a knob is added, it must be serialized into WorldInfo so a world file
/// records exactly how it was made (layout serialization lands with .dfw IO).
struct WorldGenParams {
    uint64_t seed = 0;
    ChunkCoord min_chunk;   ///< Inclusive extent; the testbed is 4x4 chunks (Q45).
    ChunkCoord max_chunk;
    TestbedLayout layout{}; ///< Feature/site layout table (LANDSCAPE §7.1 defaults).
};

/// Precomputed world-level passes (P2 hydrology, P4 sites) shared by every
/// chunk of one generation. Pure function of params (deterministic); building
/// it per chunk is valid but wasteful — ChunkManager builds it once.
struct WorldGenContext {
    WorldGenParams params;
    HydrologyData hydrology;
    SitesData sites;
    /// LF-8 (§2.10, в17) baked delta. Empty unless the layout declares erosion;
    /// sampling an empty grid returns 0, so the query is unconditional and only
    /// the BUILD is gated — one code path, per Rule 32.
    ErosionGrid erosion;
    /// §8.1 path network (в7/в24). Empty on stands that do not declare paths;
    /// sampling an empty network reports "far from any path", so consumers
    /// need no stand check.
    PathNetwork paths;
    /// BR-6 find layer (в20). Empty on stands that place no finds.
    std::vector<Find> finds;
};

/// Builds the world-level context (macro field is implicit — position-based).
[[nodiscard]] WorldGenContext build_world_context(const WorldGenParams& params);

/// THE PASS STACK — the ONE statement of what the finished ground is, and the
/// only one. Every producer of a height sample calls this: terrain_height()
/// (the continuous field), generate_chunk() (the heightmap that becomes the
/// drawn and collided mesh), and the coarse LOD node builder.
///
/// IT IS A FUNCTION RATHER THAN THREE COPIES BECAUSE IT WAS THREE COPIES.
/// Two of them said "the chain is water -> entrance works -> pads -> clamp",
/// which was true when written; when the forest stand's branch (LF-8 erosion,
/// then the path flatten) landed in terrain_height, neither copy was told.
/// The result was measured, not feared: the drawn ground stood up to 1.50 m
/// from the ground everything was PLACED on, and the path groove was absent
/// from the drawn world entirely. Rule 35's state clause — two copies drift
/// whether they are numbers or passes. Do not re-open-code this chain.
///
/// `macro` is the P1 field at `world` and `carved` the water-carve result
/// (`water_at(...).height` and `carve_height(...)` are the same call by
/// construction); both are passed in so a caller holding them adds no field
/// evaluation.
///
/// `water` is the hydrology sample at `world` — the carve result AND the shore
/// distance, because §2.7's general relief pass (WorldgenRelief.h) tapers its
/// amplitude across the shore band and therefore needs both. It used to be the
/// carved height alone; passing the whole sample adds no field evaluation,
/// since every caller already holds it.
[[nodiscard]] float compose_passes(const WorldGenContext& ctx, glm::vec2 world, float macro,
                                   const WaterSample& water);

/// Final terrain height (m) at a world position — compose_passes() evaluated
/// from scratch. The continuous field the heightmaps sample.
[[nodiscard]] float terrain_height(const WorldGenContext& ctx, glm::vec2 world);

/// Terrain slope (radians) at a world position: central differences of the
/// FINAL height field at +-HEIGHTMAP_STEP.
///
/// The step is HEIGHTMAP_STEP and not "whatever grid the caller is on", which
/// makes the slope — and therefore the surface CLASS derived from it — a pure
/// function of world position. That is a requirement, not a tidiness: a coarse
/// LOD node measuring slope at its own 8 m or 32 m spacing would classify the
/// same ground differently at every level, so render's cross-fade between two
/// levels would swap the material as well as the geometry.
[[nodiscard]] float terrain_slope(const WorldGenContext& ctx, glm::vec2 world);

/// The LANDSCAPE §4 surface classification, first match wins. One definition,
/// called by surface_point(), by the chunk builder and by the coarse LOD node
/// builder — three copies of a priority chain is three chances to drift, and a
/// drift here shows up as a material seam rather than as a failing test.
[[nodiscard]] math::SurfaceClass classify_surface(const TestbedLayout& layout,
                                                  glm::vec2 world, float height,
                                                  const WaterSample& water,
                                                  float slope_rad);

/// Full per-position surface sample (P3 outputs + final height).
struct SurfacePoint {
    float height = 0.0f;
    float water_surface = math::NO_WATER; ///< water covering this point, or NO_WATER
    float dist_to_water = 0.0f;
    math::SurfaceClass surface_class = math::SurfaceClass::Grass;
};
[[nodiscard]] SurfacePoint surface_point(const WorldGenContext& ctx, glm::vec2 world);

/// Result of generate_world with a human-readable failure reason (tool output).
struct WorldGenResult {
    bool ok = false;
    std::string error; ///< Empty on success. Tool-facing text, not localized
                       ///< (developer tooling, not a user-facing game string).
};

/// Generates the whole region and writes `out_file` (.dfw) atomically.
/// Deterministic: identical params produce a byte-identical file (Rule 13.1);
/// the determinism test regenerates twice and compares hashes.
[[nodiscard]] WorldGenResult generate_world(const WorldGenParams& params,
                                            const std::filesystem::path& out_file);

/// Generates a single chunk in isolation. Must produce bit-identical data to
/// the same chunk inside generate_world (chunk generation depends only on
/// params + coord — neighbor-aware passes derive shared edges from the same
/// noise field, not from neighbor state). Exposed for the determinism test and
/// the editor's preview. Builds a throwaway context — prefer the overload
/// below when generating more than one chunk.
[[nodiscard]] Chunk generate_chunk(const WorldGenParams& params, ChunkCoord coord);

/// Same, against a prebuilt context (ctx.params is the source of truth).
[[nodiscard]] Chunk generate_chunk(const WorldGenContext& ctx, ChunkCoord coord);

/// The deterministic PRNG used by all worldgen passes: SplitMix64 streams keyed
/// by (seed, coord, pass tag). Declared here so tests can pin its outputs;
/// gameplay dice do NOT use this (they roll on the simulation's own RNG).
struct WorldGenRng {
    /// Stream for one (chunk, pass) pair. Same inputs -> same sequence, on
    /// every platform.
    [[nodiscard]] static WorldGenRng for_chunk(uint64_t seed, ChunkCoord coord, uint32_t pass_tag);

    /// Next raw 64 bits.
    [[nodiscard]] uint64_t next_u64();
    /// Uniform float in [0, 1).
    [[nodiscard]] float next_float01();
    /// Uniform integer in [min, max] (inclusive), rejection-sampled (no modulo
    /// bias — bias would silently diverge across value ranges).
    [[nodiscard]] uint32_t next_range(uint32_t min, uint32_t max);

    uint64_t state = 0;
};

} // namespace dfn::world
