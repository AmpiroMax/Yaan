/*
Created: 09:08:2026 - 11:05:22
Last updated: 10:08:2026 - 11:51:23
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
- PathStation, PathGoalMark: the §8.1 path network as render-side primitives.
- path_wear_profile / path_edge_profile: THE cross-section, one definition,
  called by world's PathNetwork::sample and by render's ribbon alike.

Dependencies:
- Uses: glm, <algorithm>, <span>, <cstdint>.
- Used by: world::Chunk/ChunkManager (producer), engine/render splat & water
  materials and scatter drawing (consumer; agreed with render 09:08:2026).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- BOUNDARY CONTRACT agreed core<->render (stage 3b, recorded in both specs).
  Grid conventions (row-major x fastest, +X east +Z south, shared edge rows)
  and lifetime (ChunkLoaded until after ChunkUnloaded dispatch) are identical
  to HeightFieldView. Changes only via message to render + spec update.
- THE PATH CROSS-SECTION LIVES HERE AND NOWHERE ELSE. World wears the ground
  with it, render draws the tread with it and flora plants the verge against
  it. Three copies of one ramp is three chances to drift, and the drift would
  show as a verge that no longer sits on the edge it was measured against —
  which nobody would read as a code defect. Retune the numbers in this file.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — surface/scatter/water handoff contract as
  agreed with render (floats unquantized; explicit water primitives requested
  by render for plane/ribbon water materials).
- 10:08:2026 - 11:19:15: §8.1 path handoff (render's request, 10.08.2026):
  PathStation / PathGoalMark primitives + the two cross-section profiles
  moved OUT of WorldgenPaths.cpp so both zones call the same function.
- 10:08:2026 - 11:51:23: ScatterSpecies gains §5.10's forest floor, §5.11's
  rich-edge set and §5.12's krummholz (append-only, and the ordinals are the
  same silent cross-DAG contract PathClass is — pinned). Until this the meshes
  existed in render and the rules existed in design's document and the forest
  floor in the world was bare earth, eleven NUMBERS rows deep.
*/

#pragma once

#include <algorithm>
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
/// APPEND-ONLY. Render maps species -> mesh from these ordinals across a DAG
/// seam it cannot static_assert (render::flora_species_of), so a reorder
/// silently redresses the world — the same class of defect as PathClass, and
/// pinned the same way (tests/core/WorldgenV2Tests.cpp).
enum class ScatterSpecies : uint8_t {
    OakTree = 0,
    PineTree = 1,
    BirchTree = 2,
    Bush = 3,
    Stone = 4,
    // --- §5.10 THE FOREST FLOOR. Added 10.08.2026: until then every one of
    // --- these NUMBERS rows was marked НЕ ПОСТРОЕНО with zero consumers —
    // --- the meshes existed in render, the rule existed in design's document,
    // --- and the forest floor in the world was bare earth.
    Snag = 5,      ///< standing dead, IN FOREST: grey-brown weathered, texture
    SnagPale = 6,  ///< standing dead, IN THE OPEN: bone-white, a real landmark
    BigBush = 7,   ///< the mass-forming shrub (BR-5's load-bearing occluder)
    FallenLog = 8, ///< a big trunk down, laid ACROSS the slope
    Deadfall = 9,  ///< small broken wood, the litter tier
    // --- §5.11 THE RICH EDGE SET. Ground cover; placement is field-modulated
    // --- (math::clump_field) and, on a path margin, edge-weighted once.
    MossPatch = 10,
    FlowerCarpet = 11,
    FlowerAccent = 12,
    FlowerJewel = 13,
    FlowerUmbel = 14,
    Mushroom = 15,
    PebbleCluster = 16,
    /// §5.12 talus apron: the wind-formed dwarf conifer of the scree band.
    StuntedPine = 17,
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

/// One station of a §8.1 path centreline, for render's surface ribbon.
/// Stations are ordered along the route; the tangent at station i is
/// (position[i+1] - position[i]) normalized.
struct PathStation {
    glm::vec2 position{0.0f};     ///< world x/z of the centreline, meters
    /// The SMOOTHED longitudinal profile — the tread. This is the height the
    /// terrain has been flattened TO, minus the groove: world sinks the ground
    /// to (tread_height - PATH_GROOVE_DEPTH) on the tread itself, so a ribbon
    /// drawn at tread_height sits PATH_GROOVE_DEPTH proud of the flattened
    /// ground and cannot z-fight it.
    float tread_height = 0.0f;
    float worn_half_width = 0.0f; ///< half-width of the trodden surface, meters
    /// world::PathClass ordinal. THE ORDINALS ARE A PINNED CROSS-ZONE
    /// CONTRACT (world::PathClassTests): world, render and flora all key off
    /// the same 0..3 across DAG seams no static_assert can reach.
    uint8_t path_class = 0;
};

/// A §8.1 path GOAL — the thing a route actually goes to (BR-2 clause (i): a
/// route's endpoints are always registered goals, so "a path to nowhere" is
/// not representable). Render marks them; the tour stands at them.
struct PathGoalMark {
    glm::vec2 position{0.0f};
    float height = 0.0f;
    uint8_t kind = 0;        ///< world::GoalKind ordinal (pinned, same reason)
    float importance = 0.0f; ///< [0,1]; drives the class of the routes reaching it
};

/// Where the rich edge PEAKS, measured outward from the worn edge (meters).
///
/// It is NOT at the edge itself and it is NOT a fraction of the band: the
/// first few centimetres past the tread are still pressed flat by feet that
/// stray off it, so the verge starts a boot's width out. Expressed absolutely
/// because it is a fact about feet, not about how wide anyone drew the band —
/// normalizing it would make the peak MOVE when the band is retuned, and the
/// verge would silently walk away from the tread it was measured against.
///
/// REQUESTED NUMBERS row (Rule 35): read by world, render and flora.
inline constexpr float PATH_EDGE_PEAK_M = 0.35f;

/// Wear across the trodden surface, [0,1]: 1 at the bare worn centre, 0 at the
/// worn edge and beyond. `u` = |dist_to_center| / worn_half_width.
[[nodiscard]] inline float path_wear_profile(float u) {
    return std::clamp(1.0f - u * u, 0.0f, 1.0f);
}

/// BR-3's rich-edge weight, [0,1]. `edge_m` is flora's datum — meters from the
/// OUTER EDGE OF THE WORN SURFACE, outward, negative on the tread itself.
/// `band_m` is the margin band's reach (PathNetwork::rich_edge_band_m).
///
/// Zero on the tread and zero past the band, rising over PATH_EDGE_PEAK_M and
/// decaying across the rest. This is the OPPOSITE claim to path_wear_profile
/// about the same ground, which is why they are two functions and not one
/// signed number: feet make one and the absence of feet makes the other.
[[nodiscard]] inline float path_edge_profile(float edge_m, float band_m) {
    if (edge_m < 0.0f || edge_m >= band_m || band_m <= PATH_EDGE_PEAK_M) {
        return 0.0f;
    }
    return (edge_m < PATH_EDGE_PEAK_M)
             ? (edge_m / PATH_EDGE_PEAK_M)
             : std::max(0.0f, 1.0f - (edge_m - PATH_EDGE_PEAK_M)
                                         / (band_m - PATH_EDGE_PEAK_M));
}

} // namespace dfn::math
