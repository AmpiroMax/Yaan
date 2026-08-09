/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 11:05:22
Module: engine/core/math
File: engine/core/math/sources/SurfaceField.h

Responsibility:
- Stage-3b ADDITIVE companion of the frozen HeightFieldView: per-sample terrain
  surface data (dist-to-water, water surface, surface class), scatter instances
  and explicit water-body primitives handed from engine/world to engine/render.
  HeightField.h itself stays untouched (frozen stage-1 contract).

Key items:
- SurfaceClass, NO_WATER, SurfaceFieldView: per-sample splat/water inputs.
- ScatterSpecies, ScatterInstance: per-chunk vegetation/stone instance data.
- LakePlane, RiverStation: explicit water-body primitives for water meshing.

Dependencies:
- Uses: glm, <span>, <cstdint>.
- Used by: world::Chunk/ChunkManager (producer), engine/render splat & water
  materials and scatter drawing (consumer; agreed with render 09:08:2026).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- BOUNDARY CONTRACT agreed core<->render (stage 3b, recorded in both specs).
  Grid conventions (row-major x fastest, +X east +Z south, shared edge rows)
  and lifetime (ChunkLoaded until after ChunkUnloaded dispatch) are identical
  to HeightFieldView. Changes only via message to render + spec update.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — surface/scatter/water handoff contract as
  agreed with render (floats unquantized; explicit water primitives requested
  by render for plane/ribbon water materials).
*/

#pragma once

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <span>

namespace dfn::math {

/// Design-truth surface material mask per LANDSCAPE.md §4 (priority-resolved:
/// sand > rock > blend > grass; water bed under water). Render may refine the
/// blend in-shader from slope/dist_to_water; the class keeps visual == gameplay
/// truth (grass never renders above PLAYER_MAX_SLOPE).
enum class SurfaceClass : uint8_t {
    Grass = 0,
    GrassRockBlend = 1,
    Rock = 2,
    Sand = 3,
    WaterBed = 4,
};

/// Sentinel for SurfaceFieldView::water_surface where a sample is dry.
inline constexpr float NO_WATER = -1000.0f;

/// Non-owning per-sample surface data of one terrain chunk. Same grid,
/// conventions and lifetime as the HeightFieldView of the same chunk.
struct SurfaceFieldView {
    glm::ivec2 chunk_coord{0, 0};
    glm::vec2 origin{0.0f, 0.0f};
    uint32_t resolution = 0;
    float step = 0.0f;
    /// Horizontal meters to the nearest water EDGE; 0 inside water.
    std::span<const float> dist_to_water;
    /// Water surface height (m) where the sample is covered by water;
    /// NO_WATER elsewhere. Monotonically non-increasing downstream (invariant).
    std::span<const float> water_surface;
    /// SurfaceClass values (uint8_t).
    std::span<const uint8_t> surface_class;
};

/// Species of a scattered instance (P5 meso pass). Append-only enum — render
/// maps species -> mesh; worldgen never decides drawing.
enum class ScatterSpecies : uint8_t {
    OakTree = 0,
    PineTree = 1,
    BirchTree = 2,
    Bush = 3,
    Stone = 4,
};

/// One scattered vegetation/stone instance, produced deterministically per
/// chunk by worldgen P5. Position is world-space (y = terrain height).
/// scale 1.0 = the species' nominal size; render decides meshes/instancing.
struct ScatterInstance {
    glm::vec3 position{0.0f};
    float yaw = 0.0f;   ///< radians
    float scale = 1.0f; ///< uniform, relative to the species' nominal size
    ScatterSpecies species = ScatterSpecies::OakTree;
};

/// Explicit lake water body (axis-aligned ellipse footprint) for plane-style
/// water rendering. The per-sample water_surface remains the coverage truth.
struct LakePlane {
    glm::vec2 center{0.0f};      ///< world x/z, meters
    glm::vec2 half_extent{0.0f}; ///< ellipse semi-axes, meters
    float surface_height = 0.0f; ///< meters
};

/// One resampled river centerline station for ribbon-style water rendering.
/// Stations are ordered source -> mouth; flow direction at station i is
/// (position[i+1] - position[i]) normalized. surface_height is monotonically
/// non-increasing along the array (worldgen invariant, tested).
struct RiverStation {
    glm::vec2 position{0.0f};    ///< world x/z of the centerline, meters
    float surface_height = 0.0f; ///< water surface, meters
    float half_width = 0.0f;     ///< channel half width, meters
};

} // namespace dfn::math
