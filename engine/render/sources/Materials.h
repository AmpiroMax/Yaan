/*
Created: 09:08:2026 - 11:00:00
Last updated: 09:08:2026 - 14:11:37
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
*/

#pragma once

#include "engine/core/config/sources/Constants.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <cmath>
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
