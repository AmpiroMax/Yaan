/*
Created: 09:08:2026 - 11:00:00
Last updated: 10:08:2026 - 21:13:39
Module: engine/render
File: engine/render/sources/Materials.h

Responsibility:
- The single place for stage-3 look-dev material/atmosphere values and the
  builder of the frame RenderEnvironment. Everything visual-tunable lives here
  (thresholds ship to the GPU as uniforms — no shader recompile to retune).

Key items:
- Look-dev constants (LOOKDEV_*): splat thresholds, fog span, sun/sky colors,
  water parameters, atlas cell size, texture tiling density.
- make_default_environment(): constants -> platform::RenderEnvironment.

Dependencies:
- Uses: IRenderer.h (RenderEnvironment), generated Constants.h (CAMERA_FAR).
- Used by: RenderSystem; engine/app or editor may tune via
  RenderSystem::environment().

Notes:
- EXPLICITLY TEMPORARY look-dev values (approved at the stage-3 sync,
  10:48): once the landscape design bible fixes biome palettes and water
  levels, these migrate to NUMBERS.md / design-driven data. Listed in the
  stage-3 DONE report for that migration.
- Fog is tied to CAMERA_FAR so the world always dissolves into sky before the
  far plane (and before the testbed edge can read as a cliff into void).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Tune values HERE, not in shaders and not in the backend (Rule 14 spirit).
*/
/*
UPD:
- 09:08:2026 - 11:00:00: Stage 3 — initial look-dev environment.
- 09:08:2026 - 11:57:20: Stage 3b — rock slope band now derived from the
  design constants SLOPE_GRASS_MAX/SLOPE_ROCK_MIN (1 - cos conversion; the
  flat-worldgen placeholder band is gone); water edge margin for per-body
  water meshes.
- 09:08:2026 - 14:11:37: Sun lowered to afternoon height for readable dynamic
  shadows (в1). The app's day/night cycle (в2) overrides all of sun/ambient/
  fog/sky per frame via RenderSystem::environment(); these remain the defaults.
- 09:08:2026 - 21:12:00: LOOKDEV_SHADOW_CASTER_KEEP_M — the radius that keeps
  off-screen shadow casters alive once frustum culling exists.
- 10:08:2026 - 21:13:39: THE SPLAT TABLE. splat_weights_of(VoxelMaterial) plus the
  SurfaceClass overload through core's voxel_material_of(), and pack_splat().
  BLEND_CLASS_ROCK_W moves here from TerrainMesher.cpp: it has two consumers
  now, so it stopped belonging to one of them (Rule 35). No `default:` in the
  switch, deliberately -- a new material must break the build rather than
  render as grass, which is how the blend class was lost. Value 0.5 preserved
  unchanged and marked unverified; changing it inside a structural fix would
  have made both unmeasurable.
*/

#pragma once

#include "engine/core/config/sources/Constants.h"
#include "engine/core/math/sources/VoxelField.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>

namespace dfn::render {

// --- Look-dev constants (see header note: temporary until the design doc) ---

// Sun from the south (slightly east), afternoon height. Retuned when dynamic
// shadows landed (в1): the old near-noon ESE sun cast stubby shadows that hid
// behind their objects from the tour vantages. At ~42 deg elevation with a
// southern azimuth an oak throws a ~11 m shadow to the north and objects
// visibly sit on the ground (the quality bar of the batch) in the
// west/north-facing frames.
inline const glm::vec3 LOOKDEV_SUN_DIRECTION =
    glm::normalize(glm::vec3(0.30f, 0.62f, 0.62f));
inline constexpr glm::vec3 LOOKDEV_SUN_COLOR{1.00f, 0.96f, 0.88f};
inline constexpr glm::vec3 LOOKDEV_AMBIENT_COLOR{0.34f, 0.36f, 0.40f}; // cool skylight

inline constexpr glm::vec3 LOOKDEV_SKY_ZENITH{0.25f, 0.42f, 0.66f};
inline constexpr glm::vec3 LOOKDEV_SKY_HORIZON{0.63f, 0.71f, 0.80f}; // == fog color

// Fog span as fractions of CAMERA_FAR. Stage 3b: widened (0.25/0.70 ->
// 0.30/0.85) so the L0 crag reads from the hamlet ~570 m away (C1 /
// LANDMARK_VISIBILITY_MIN — at the old span the landmark dissolved into the
// sky). The testbed edge sits ~800+ m from the tour vantages: >= 0.9 fogged.
inline constexpr float LOOKDEV_FOG_START_FRAC = 0.30f;
inline constexpr float LOOKDEV_FOG_END_FRAC = 0.85f;

// Terrain splat (slope = 1 - normal.y; heights in meters, world space).
// Stage 3b: worldgen v2 has real crags, so the slope band derives from the
// DESIGN constants (LANDSCAPE §4 / NUMBERS.md): grass ends at SLOPE_GRASS_MAX
// (0.52 rad) and hard rock starts at SLOPE_ROCK_MIN (0.70 rad), converted from
// slope angle to the shader's 1 - cos(angle) measure. The in-shader slope rock
// AUGMENTS the surface-class weights baked from core's SurfaceFieldView (the
// design truth); the flat-worldgen placeholder band (0.0025-0.0060) is gone.
inline const float LOOKDEV_ROCK_SLOPE_START =
    1.0f - std::cos(static_cast<float>(config::SLOPE_GRASS_MAX));
inline const float LOOKDEV_ROCK_SLOPE_END =
    1.0f - std::cos(static_cast<float>(config::SLOPE_ROCK_MIN));

// --- THE splat table -------------------------------------------------------
//
// Splat weight of the GrassRockBlend class (LANDSCAPE §4 priority 3): a mid
// value so the shader's ordered dither band straddles it between grass and
// rock. UNVERIFIED as a look-dev value — it has never been measured, and see
// the note under splat_weights_of() for the reason that matters less than it
// looks. It lives here rather than in a mesher because it now has two
// consumers (Rule 35).
inline constexpr float BLEND_CLASS_ROCK_W = 0.5f;

/// The three splat channels a terrain vertex carries: R sand / G rock /
/// B water-bed. This is a SHADER CONTRACT (fs_terrain.sc), not a world fact,
/// which is why the table lives in render and is keyed off world's material.
struct SplatWeights {
    float sand = 0.0f;
    float rock = 0.0f;
    float bed = 0.0f;
};

/// THE mapping from a surface material to its splat weights — one table, read
/// by BOTH meshers.
///
/// It exists because there used to be two. `TerrainMesher` switched over
/// `SurfaceClass` and `VoxelMesher` over `VoxelMaterial`; each was exhaustive
/// within its own enum, so the compiler was satisfied and nothing checked that
/// the two answered the same question the same way. They did not: the blend
/// class drew at rock 0.5 through the heightfield path and 0.0 through the
/// voxel path, on the same chunk-load branch (Rule 39 — a shadow copy of a
/// chain, with a compiler guarantee supplying false comfort).
///
/// No `default:` here, deliberately: a new VoxelMaterial must break the build
/// rather than silently render as grass, which is exactly how the blend class
/// was lost in the first place.
[[nodiscard]] constexpr SplatWeights splat_weights_of(math::VoxelMaterial m) {
    switch (m) {
    case math::VoxelMaterial::Sand:
        return {1.0f, 0.0f, 0.0f};
    case math::VoxelMaterial::Rock:
        return {0.0f, 1.0f, 0.0f};
    // Dirt is sub-surface fill AND carved cave wall AND (via voxel_material_of)
    // river/lake bed: the darkest atlas cell is the closest thing to bare
    // earth, and a tunnel wall reading as grass would be worse than as mud.
    case math::VoxelMaterial::Dirt:
        return {0.0f, 0.0f, 1.0f};
    case math::VoxelMaterial::GrassRockBlend:
        return {0.0f, BLEND_CLASS_ROCK_W, 0.0f};
    case math::VoxelMaterial::Grass:
    case math::VoxelMaterial::Air: // never lands on a vertex
        return {};
    }
    return {}; // unreachable for a valid enumerator
}

/// Same table, reached from the design-truth class through world's projection.
/// Going through `voxel_material_of` rather than switching on `SurfaceClass`
/// separately is the point: it is what makes ONE table serve both meshers.
[[nodiscard]] constexpr SplatWeights splat_weights_of(math::SurfaceClass c) {
    return splat_weights_of(math::voxel_material_of(c));
}

/// Packs the splat weights into a vertex colour. 0xAABBGGRR; alpha is sky
/// visibility (255 = open sky), reserved on the heightfield path.
[[nodiscard]] inline uint32_t pack_splat(SplatWeights w, uint8_t sky = 255) {
    const auto q = [](float v) {
        return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return (static_cast<uint32_t>(sky) << 24) | (q(w.bed) << 16) | (q(w.rock) << 8)
           | q(w.sand);
}
// --- end splat table -------------------------------------------------------
// Radius around the eye inside which an off-screen mesh is STILL submitted,
// because the backend double-submits every opaque draw into the sun shadow map
// and a caster culled from the camera would take its shadow with it. It mirrors
// the backend's SHADOW_HALF_EXTENT_M: past that distance the shadow volume does
// not cover the caster anyway, so dropping it is free. THESE TWO NUMBERS MUST
// MATCH — both are on the NUMBERS.md migration list, which is where the
// duplication goes away.
inline constexpr float LOOKDEV_SHADOW_CASTER_KEEP_M = 320.0f;

inline constexpr float LOOKDEV_SAND_BLEND_M = 1.5f;
inline constexpr float LOOKDEV_TERRAIN_TILES_PER_CHUNK = 32.0f; // 8 m per repeat

// Water surface.
inline constexpr glm::vec4 LOOKDEV_WATER_COLOR{0.16f, 0.30f, 0.34f, 0.62f};
inline constexpr glm::vec2 LOOKDEV_WATER_SCROLL_UV{0.020f, 0.013f};
inline constexpr float LOOKDEV_WATER_UV_TILE_M = 24.0f; // meters per texture repeat
// Per-body water meshes extend past the carved banks by this margin; the
// overlap hides under terrain via the depth test, so shorelines never gap.
inline constexpr float LOOKDEV_WATER_EDGE_MARGIN_M = 2.0f;

// Procedural texture assets.
// §8.1 path surface: how far the tread is raised above core's longitudinal
// profile, metres.
//
// A STOPGAP WITH A DERIVED SIZE, NOT A TUNED ONE — and the reason it exists is
// worth reading before anyone "cleans it up" to zero.
//
// Core flattens the HEIGHT FIELD to the tread and sinks it by
// PATH_GROOVE_DEPTH, and its own suite measures that a ribbon drawn at the
// profile clears that field. But the ground the player SEES is not that field:
// terrain is drawn from the VOXEL surface, whose lattice is VOXEL_SIZE = 1 m.
// A 0.15 m groove cannot exist on a 1 m lattice, and the extracted surface can
// stand a whole voxel ABOVE the smooth height — so the tread, drawn where core
// put it, is buried under its own ground for most of its length. Measured on
// the forest stand at a lift sweep: 0.00 m showed nothing at all, 0.35 m showed
// the nearest four metres, and only at ~0.8 m did the road become continuous.
//
// The lift is therefore VOXEL_SIZE: one voxel is exactly how far the drawn
// surface can stand above the field, so it is the smallest value that CANNOT
// bury the tread, and it is a number that follows the lattice if the lattice
// ever changes. The cost is that the tread floats by up to a voxel where the
// surface sits low, which will read at grazing angles.
//
// THE REAL FIX IS NOT HERE: the path is a groove in the ground and the drawn
// ground should carry it. Requested from core — carve the tread into the voxel
// volume, or expose the drawn surface as a query render can conform to. When
// either lands this drops to a hair and this comment becomes the history.
inline constexpr float PATH_SURFACE_LIFT_M = static_cast<float>(config::VOXEL_SIZE);

// §8.1 path surface: tile repeats per METRE of tread (the path mesh's uv is
// already in metres, so this cannot ride on the terrain's per-CHUNK tiling).
// Sized from the cobble cell, which is the only one of the four whose element
// size is a real-world fact: the atlas cell carries 9x9 set stones, and a set
// stone is ~0.25 m, so one repeat should span ~2.2 m of ground.
inline constexpr float PATH_TILES_PER_M = 0.45f;

inline constexpr uint32_t LOOKDEV_ATLAS_CELL_PX = 128;
inline constexpr uint32_t LOOKDEV_WATER_TEX_PX = 128;
inline constexpr uint32_t LOOKDEV_TEXTURE_SEED = 1337; // visual seed, not worldgen

// Builds the default frame environment from the constants above.
// `sand_height_m` is the current waterline reference (sand shows slightly
// above it); pass a very low value to effectively disable the sand band.
[[nodiscard]] inline platform::RenderEnvironment
make_default_environment(float sand_height_m = -1000.0f) {
    platform::RenderEnvironment env;
    env.sun_direction = LOOKDEV_SUN_DIRECTION;
    env.sun_color = LOOKDEV_SUN_COLOR;
    env.ambient_color = LOOKDEV_AMBIENT_COLOR;
    env.fog_color = LOOKDEV_SKY_HORIZON;
    env.fog_start_m = LOOKDEV_FOG_START_FRAC * static_cast<float>(config::CAMERA_FAR);
    env.fog_end_m = LOOKDEV_FOG_END_FRAC * static_cast<float>(config::CAMERA_FAR);
    env.sky_zenith_color = LOOKDEV_SKY_ZENITH;
    env.sky_horizon_color = LOOKDEV_SKY_HORIZON;
    env.sand_height_m = sand_height_m;
    env.sand_blend_m = LOOKDEV_SAND_BLEND_M;
    env.rock_slope_start = LOOKDEV_ROCK_SLOPE_START;
    env.rock_slope_end = LOOKDEV_ROCK_SLOPE_END;
    env.terrain_tiles_per_chunk = LOOKDEV_TERRAIN_TILES_PER_CHUNK;
    env.water_color = LOOKDEV_WATER_COLOR;
    env.water_scroll_uv = LOOKDEV_WATER_SCROLL_UV;
    env.time_seconds = 0.0f;
    return env;
}

} // namespace dfn::render
