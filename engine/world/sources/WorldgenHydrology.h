/*
Module: engine/world
File: engine/world/sources/WorldgenHydrology.h

Responsibility:
- Worldgen v2 pass P2 (LANDSCAPE.md §3): river source -> greedy descent with
  pond-and-spill -> Chaikin smoothing + sinuosity -> monotonic water-surface
  levels -> trapezoid carve with fords; the lake at LAKE_LEVEL_TESTBED.
  Built ONCE per WorldGenParams (pure function of seed + layout + extent);
  per-sample queries are position-based so chunks stay independent.

Key items:
- HydrologyData: stations (math::RiverStation), segments, ponds, lake plane,
  coarse fill/distance grids, ok flag (a climbing river = failed generation).
- build_hydrology(): the P2 pass.
- water_at(): per-sample carve + water surface + dist-to-water query.

Dependencies:
- Uses: TestbedLayout.h, WorldgenMacro.h, core/math/SurfaceField.h, config.
- Used by: Worldgen.cpp, WorldgenSites, WorldgenScatter, ChunkManager
  (water_bodies for render), WorldgenValidation, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): all iteration orders fixed (index tie-breaks in
  heaps/argmins); randomness only via WorldgenNoise streams.
- MONOTONIC WATER INVARIANT (§3.1.4, flat-reach form): station water heights
  never increase downstream, are CONSTANT across any standing body, and the
  drawn level of a pond equals the swum level by construction. Enforced by
  construction, asserted in build, guarded by test.
*/

#pragma once

#include "engine/core/math/sources/SurfaceField.h"
#include "engine/world/sources/TestbedLayout.h"

#include <cstdint>
#include <glm/vec2.hpp>
#include <vector>

namespace dfn::world {

/// A pond created by pond-and-spill (§3.1 step 2): coarse cells filled to one
/// FLAT level. A pond is a flat reach of the river (grill в23): its level is
/// min(spill saddle, the level the river ENTERS at), so the drawn plane and
/// the swum surface are the same number by construction — a pond sitting
/// above the river that feeds it is unconstructible. Bed carved from `level`.
struct Pond {
    std::vector<uint32_t> cells; ///< coarse-grid indices (z * grid_w + x)
    float level = 0.0f;          ///< flat water level, meters
    /// Spill (saddle) height recorded at creation, BEFORE the entering-river
    /// clamp. level <= spill_level always; level < spill_level is the case
    /// the flat-reach rule exists for (the old construction drew the pond at
    /// spill_level while the river swam through it at `level` — kept so the
    /// control test can prove that pond really occurs).
    float spill_level = 0.0f;
};

/// Output of the hydrology pass. Coarse grids are worldgen-internal; the
/// station/lake data is exposed to render via ChunkManager::water_bodies().
struct HydrologyData {
    // River centerline, resampled every RIVER_STATION_SPACING meters.
    // Segments: [segment_offsets[i], segment_offsets[i+1]) — segment 0 is
    // source->lake, segment 1 lake-outlet->map edge (a segment may be absent).
    std::vector<math::RiverStation> stations;
    std::vector<uint32_t> segment_offsets; ///< size = segment count + 1
    std::vector<float> carve_depth;        ///< per station, ford-adjusted (m)

    /// Derived ford station indices (§3.1 step 6, §7.1a design ruling): one
    /// where each POI-chain corridor crosses the GENERATED trace, plus fills
    /// so no along-river gap exceeds FORD_SPACING_MAX. Sorted by arclength.
    std::vector<uint32_t> ford_stations;

    std::vector<Pond> ponds;
    math::LakePlane lake;
    /// Drawable primitives for the surviving ponds (centroid + bounding
    /// half-extent + level), so every water-covered sample has a body render
    /// can draw over it — ChunkManager appends these to water_bodies().lakes.
    std::vector<math::LakePlane> pond_planes;

    // Coarse grids (HYDRO grid step), row-major x fastest.
    glm::vec2 grid_origin{0.0f};
    uint32_t grid_w = 0, grid_h = 0;
    std::vector<float> fill_level; ///< water level per cell; math::NO_WATER if dry
    std::vector<float> coarse_dist; ///< distance-to-water per cell node, meters

    bool ok = false; ///< false => failed generation (trace/monotonic failure)

    // Station spatial bins for near-river queries (bin = 32 m).
    std::vector<std::vector<uint32_t>> station_bins;
    uint32_t bins_w = 0, bins_h = 0;
    float bin_size = 0.0f;
};

/// Per-sample water query result (P2 carve + P3 water outputs).
struct WaterSample {
    float height = 0.0f;                 ///< terrain after carve, meters
    float water_surface = math::NO_WATER; ///< water covering this sample, or NO_WATER
    float dist_to_water = 0.0f;          ///< horizontal meters to nearest water edge
    float near_level = math::NO_WATER;   ///< surface height of the nearest water
                                          ///< body when one is within exact range
                                          ///< (shore-mask input), else NO_WATER
};

/// Builds hydrology over the world rect [domain_min, domain_max] (meters).
/// Deterministic; sets ok=false instead of producing an invalid river.
/// Pond water beyond the §3.3 bed/mud cap — max(SHORE_SAND_DIST, 2 x local
/// river width) from the trace — is pruned (the carve still cuts the channel
/// through drained basins), so wide mud flats cannot exist by construction.
[[nodiscard]] HydrologyData build_hydrology(uint64_t seed, const TestbedLayout& layout,
                                            glm::vec2 domain_min, glm::vec2 domain_max);

/// Applies the river/lake/pond carve to macro height `h` at `world` and
/// reports water surface + distance to water. Position-based (chunk-free).
[[nodiscard]] WaterSample water_at(const HydrologyData& hydro, const TestbedLayout& layout,
                                   glm::vec2 world, float h);

/// Carve-only fast path (no distance field / coverage math) — used by the
/// height pipeline's gradient samples. MUST return exactly water_at().height.
[[nodiscard]] float carve_height(const HydrologyData& hydro, const TestbedLayout& layout,
                                 glm::vec2 world, float h);

} // namespace dfn::world
