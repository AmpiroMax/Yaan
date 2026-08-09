/*
Created: 09:08:2026 - 18:58:10
Last updated: 09:08:2026 - 18:58:10
Module: engine/render
File: engine/render/sources/SkyModel.h

Responsibility:
- The day/night look model (user decisions в1/в2): turns the app's normalized
  clock into a complete frame environment — sun and moon direction, light and
  sky/fog colours through dawn, day, dusk and night, moonlight and star
  intensity. The app owns the clock; this file owns how the sky LOOKS.

Key items:
- apply_sky_time(env, day_fraction, lunar_phase): the single app-facing call.
- sun_direction_at / moon_direction_at: the geometry, exposed for tests and
  for gameplay-side questions like "is it night".
- TORCH_COLOR / TORCH_RADIUS_M: the carried-light look-dev values.

Dependencies:
- Uses: IRenderer.h (RenderEnvironment), Materials.h (day look-dev values), glm.
- Used by: engine/app (once per frame, before RenderSystem::render), tests.

Notes:
- Look-dev values live here and in Materials.h, on the NUMBERS.md migration
  list (Rule 14) — the same standing exception the stage-3 material constants
  got. Nothing here is a gameplay constant: day LENGTH belongs to the app
  (в13 is still an open user question), which is exactly why this API takes a
  normalized fraction and cannot be invalidated by that answer.
- The moon's DIRECTION is derived from its phase: elongation from the sun is
  the phase angle, so a full moon rises at sunset and a new moon rides with
  the sun. That consistency is free and it is what makes a sky read as real.
- Weather (clear/overcast/fog, decided but NOT this stage) drops in on top:
  overcast pushes star_intensity and moon_light down and lifts fog, without
  touching this geometry. Deliberately left possible, deliberately not built.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this pure: no GPU, no ECS, no clock reads. Inputs are fractions.
*/
/*
UPD:
- 09:08:2026 - 18:58:10: Created for the day/night stage (в1/в2): sun/moon
  geometry, phase-derived moon direction, dawn/dusk palette, stars.
*/

#pragma once

#include "engine/platform/render/interfaces/IRenderer.h"

#include <glm/vec3.hpp>

namespace dfn::render {

/// Sun elevation (dot with up) below which the sun contributes no light and
/// the night palette has fully taken over.
inline constexpr float SKY_NIGHT_ELEVATION = -0.12f;
/// Sun elevation above which it is unambiguously day (no dawn/dusk tint).
inline constexpr float SKY_DAY_ELEVATION = 0.22f;

/// Carried light (torch/lantern) look-dev — the app assigns these into
/// RenderEnvironment::point_light_* so no numbers are hardcoded there.
inline constexpr glm::vec3 TORCH_COLOR{1.00f, 0.62f, 0.28f};
inline constexpr float TORCH_RADIUS_M = 9.0f;

/// Direction TOWARD the sun for a normalized time of day.
/// day_fraction: 0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset.
/// The arc is tilted south (the testbed's look-dev convention), so the sun
/// never passes exactly overhead and shadows always have a direction.
[[nodiscard]] glm::vec3 sun_direction_at(float day_fraction);

/// Direction TOWARD the moon. Its elongation from the sun IS the phase angle:
/// lunar_phase 0.5 (full) puts it opposite the sun, 0 (new) beside it.
[[nodiscard]] glm::vec3 moon_direction_at(float day_fraction, float lunar_phase);

/// Fraction of the moon's disc that is lit, 0 (new) .. 1 (full).
[[nodiscard]] float moon_illumination(float lunar_phase);

/// Writes the whole sky/lighting state for this moment into `env`: sun and
/// moon direction, sun/ambient/fog/sky colours, moonlight and stars. Every
/// other field of `env` (splat thresholds, water, point light) is untouched,
/// so the app may override any single value after the call.
void apply_sky_time(platform::RenderEnvironment& env, float day_fraction,
                    float lunar_phase);

} // namespace dfn::render
