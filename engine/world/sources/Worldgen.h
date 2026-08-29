/*
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

#pragma once

#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/ReliefLayer.h"
#include "engine/world/sources/TestbedLayout.h"
#include "engine/world/sources/WorldFormat.h"
#include "engine/world/sources/WorldgenErosion.h"
#include "engine/world/sources/WorldgenFlow.h"
#include "engine/world/sources/WorldgenFinds.h"
#include "engine/world/sources/WorldgenGreatOak.h"
#include "engine/world/sources/WorldgenHydrology.h"
#include "engine/world/sources/WorldgenPaths.h"
#include "engine/world/sources/WorldgenSites.h"

#include <cmath>
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
    /// PADS A COMPOSITION AUTHORED — the terraces of a town, the flat under a
    /// house. Empty on every world that does not declare them, and an empty
    /// list is a no-op BIT FOR BIT: this is what lets the feature exist without
    /// moving the pinned testbed heightmap by a single sample.
    ///
    /// They are applied LAST in the pass stack, after the generator's own pads,
    /// because an authored flat is the strongest statement anyone makes about
    /// the ground: a composer who cut a terrace means it.
    std::vector<BuildingPad> composed_pads;
    /// WATERCOURSES A COMPOSITION AUTHORED. Applied AFTER the pads, because a
    /// river runs THROUGH a terrace rather than under it: the user asked for
    /// one arm through the town itself, and a pad that won over the water
    /// would fill in its own canal.
    std::vector<RiverChannel> composed_rivers;
    /// THE GROUND A HUMAN PAINTED, with a brush, while looking at it. Applied
    /// LAST of everything — after the generator's passes, after the authored
    /// pads and after the watercourses — because a stroke is the most recent
    /// and most particular statement anybody makes about this ground: a
    /// composer who stood on a hillside and pulled it up meant that spot and no
    /// other, and nothing derived from a rule may argue with him about it.
    ///
    /// Empty on every map that declares no `relief =` sidecar, and an empty
    /// layer is a no-op BIT FOR BIT — guaranteed by an explicit branch rather
    /// than by float arithmetic, so the pinned testbed heightmap digest holds.
    ReliefLayer composed_relief;
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
    /// THE DRAINAGE (docs/design/TERRAIN_REFERENCE.md §6). Valleys cut where
    /// the landform actually sends its water, baked once per world and sampled
    /// as a delta. It is a WORLD-LEVEL pass and cannot be anything else:
    /// drainage area is a catchment quantity, so a chunk computing its own
    /// would produce channels that stop at its border.
    FlowGrid flow;
    /// §8.1 path network (в7/в24). Empty on stands that do not declare paths;
    /// sampling an empty network reports "far from any path", so consumers
    /// need no stand check.
    PathNetwork paths;
    /// BR-6 find layer (в20). Empty on stands that place no finds.
    std::vector<Find> finds;
    /// GIANT_OAKS §2: the landmark trees. A WORLD-LEVEL pass and not a scatter
    /// lattice, because its spacing is derived from the read distance of an
    /// 96 m crown and comes out LARGER THAN THIS WORLD'S DIAGONAL — the rarity
    /// is a consequence, so the count is an output. Every chunk must see the
    /// same list or a clearing would have a seam down the middle of it.
    std::vector<GreatOakSite> great_oaks;
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

/// THE ARM of the central difference every slope in this zone is measured
/// with (NUMBERS.md, TERRAIN_SLOPE_STENCIL_M).
///
/// IT IS ITS OWN NUMBER AND NO LONGER HEIGHTMAP_STEP, and that separation is
/// the point rather than a detail. Slope here answers "what KIND of ground is
/// this" — it paints rock against grass, admits scatter and judges walkability
/// — and that is a property of the landform at human scale, not of how densely
/// we happen to store it. Tied to the storage step, every future refinement of
/// storage would silently repaint the world: on a shorter arm the same hillside
/// reads systematically steeper, because the same rise is divided by a shorter
/// run and the micro-relief across it is no longer averaged out.
///
/// It also used to be an AGREEMENT BY COINCIDENCE. terrain_slope() took its arm
/// from HEIGHTMAP_STEP while WorldgenScatter, WorldgenSites and
/// WorldgenValidation each held a bare `d = 2.0f`; the four matched only
/// because the storage step happened to be 2.0. The first edit to that row
/// would have split the classifier from the scatter and from the walkability
/// gate with nothing turning red (Rule 32).
inline constexpr float SLOPE_STENCIL_ARM_M =
    static_cast<float>(config::TERRAIN_SLOPE_STENCIL_M);

/// THE central-difference slope (radians) of any height function, on THE arm.
///
/// A template because its four callers sample four different heights — the
/// final field, the field without pads (pads are being placed), the scatter
/// pass's own carved ground — and the thing that must not differ between them
/// is the STENCIL, not the sampler.
template <typename HeightFn>
[[nodiscard]] float central_difference_slope(const HeightFn& height, glm::vec2 p) {
    const float d = SLOPE_STENCIL_ARM_M;
    const float hx = height(glm::vec2{p.x + d, p.y}) - height(glm::vec2{p.x - d, p.y});
    const float hz = height(glm::vec2{p.x, p.y + d}) - height(glm::vec2{p.x, p.y - d});
    return std::atan(std::sqrt(hx * hx + hz * hz) / (2.0f * d));
}

/// Terrain slope (radians) at a world position: central differences of the
/// FINAL height field on SLOPE_STENCIL_ARM_M.
///
/// The arm is a world constant and not "whatever grid the caller is on", which
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
///
/// `authored` is the composer's surface brush, OPTIONAL and null everywhere it
/// does not exist (Rule 26: the contract grew, no existing call changed). It is
/// consulted after the water-coverage clause and before every derived one, and
/// that placement is the rule rather than an accident of order: an authored
/// class answers "what is this ground MADE OF", which is a different question
/// from "is it under water" — the two only share an enum. A composer who paints
/// rock on a lake bed gets a rocky bed, not a rock island.
[[nodiscard]] math::SurfaceClass classify_surface(const TestbedLayout& layout,
                                                  glm::vec2 world, float height,
                                                  const WaterSample& water,
                                                  float slope_rad,
                                                  const ReliefLayer* authored = nullptr);

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
