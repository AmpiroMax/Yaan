/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 11:05:22
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
- MONOTONIC WATER INVARIANT (§3.1.4): station water heights never increase
  downstream. Enforced by construction, asserted in build, guarded by test.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — P2 hydrology (trace, ponds, carve, fords,
  lake; distance field for dist_to_water).
*/

#pragma once

#include "engine/core/math/sources/SurfaceField.h"
#include "engine/world/sources/TestbedLayout.h"

#include <cstdint>
#include <glm/vec2.hpp>
#include <vector>

namespace dfn::world {

/// A pond created by pond-and-spill (§3.1 step 2): coarse cells filled to a
/// spill level. Water surface = level; bed carved slightly below.
struct Pond {
    std::vector<uint32_t> cells; ///< coarse-grid indices (z * grid_w + x)
    float level = 0.0f;          ///< spill (saddle) height, meters
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

    std::vector<Pond> ponds;
    math::LakePlane lake;

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
