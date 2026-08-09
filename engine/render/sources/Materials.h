/*
Created: 09:08:2026 - 11:00:00
Last updated: 09:08:2026 - 11:00:00
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
*/

#pragma once

#include "engine/core/config/sources/Constants.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <glm/geometric.hpp>

namespace dfn::render {

// --- Look-dev constants (see header note: temporary until the design doc) ---

// Sun from the south-east, fairly high — long readable slopes, warm light.
inline const glm::vec3 LOOKDEV_SUN_DIRECTION =
    glm::normalize(glm::vec3(0.35f, 0.80f, 0.45f));
inline constexpr glm::vec3 LOOKDEV_SUN_COLOR{1.00f, 0.96f, 0.88f};
inline constexpr glm::vec3 LOOKDEV_AMBIENT_COLOR{0.34f, 0.36f, 0.40f}; // cool skylight

inline constexpr glm::vec3 LOOKDEV_SKY_ZENITH{0.25f, 0.42f, 0.66f};
inline constexpr glm::vec3 LOOKDEV_SKY_HORIZON{0.63f, 0.71f, 0.80f}; // == fog color

// Fog span as fractions of CAMERA_FAR: fully fogged well before the far plane
// (also hides the streaming edge: the loaded box ends ~640 m from eye level).
inline constexpr float LOOKDEV_FOG_START_FRAC = 0.25f;
inline constexpr float LOOKDEV_FOG_END_FRAC = 0.70f;

// Terrain splat (slope = 1 - normal.y; heights in meters, world space).
// Measured on the seed-1 testbed (probe over all 16 chunks): slope p50 0.0007,
// p95 0.0031, p99 0.0049, max 0.0101 — the stage-2 worldgen is a near-flat
// plain, so the band below catches only its steepest ~2%. These are uniforms:
// when core's worldgen v2 (crags/valleys) lands, design retunes here without
// touching shaders; realistic mountain values will be ~0.07-0.18.
inline constexpr float LOOKDEV_ROCK_SLOPE_START = 0.0025f;
inline constexpr float LOOKDEV_ROCK_SLOPE_END = 0.0060f;
inline constexpr float LOOKDEV_SAND_BLEND_M = 1.5f;
inline constexpr float LOOKDEV_TERRAIN_TILES_PER_CHUNK = 32.0f; // 8 m per repeat

// Water surface.
inline constexpr glm::vec4 LOOKDEV_WATER_COLOR{0.16f, 0.30f, 0.34f, 0.62f};
inline constexpr glm::vec2 LOOKDEV_WATER_SCROLL_UV{0.020f, 0.013f};
inline constexpr float LOOKDEV_WATER_UV_TILE_M = 24.0f; // meters per texture repeat

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
