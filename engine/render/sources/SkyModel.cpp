/*
Created: 09:08:2026 - 19:04:20
Last updated: 09:08:2026 - 19:04:20
Module: engine/render
File: engine/render/sources/SkyModel.cpp

Responsibility:
- SkyModel implementation: sun/moon geometry from the normalized clock and the
  dawn/day/dusk/night colour ramps written into RenderEnvironment.

Key items:
- sun_direction_at / moon_direction_at / moon_illumination / apply_sky_time.

Dependencies:
- Uses: SkyModel.h, Materials.h (day look-dev anchors), generated Constants.h
  (CAMERA_FAR for the fog span), glm.
- Used by: engine/app (per frame), tests.

Notes:
- Colour ramps are keyed off SUN ELEVATION (dot with up), not off the clock:
  the sky must look the same at a given sun height whatever the day length is,
  and the app's debug 50x time key must not change the palette, only its speed.
- Night is deliberately PLAYABLE-DARK under the moon (user decision): the night
  ambient keeps a navigable blue floor, and moonlight adds a real directional
  term on top. True darkness is reserved for interiors, where the torch and
  (once core lands sky visibility) the ambient falloff take over.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure: no GPU, no ECS, no wall clock. Inputs are fractions.
*/
/*
UPD:
- 09:08:2026 - 19:04:20: Created with the day/night stage.
*/

#include "engine/render/sources/SkyModel.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/Materials.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace dfn::render {

namespace {

constexpr float TAU = 6.28318530718f;

// The sun's arc is tilted toward the south (+Z) so it never passes exactly
// overhead: a zenith sun flattens every shadow to nothing at noon and the
// world loses its ground contact. Matches the afternoon look-dev sun.
constexpr float SKY_ARC_TILT = 0.45f;

// --- Palette anchors (look-dev; NUMBERS.md migration list) -----------------
// Day values come from Materials.h so the noon sky is identical to the look
// the design frames were accepted against; only dusk and night are new.
constexpr glm::vec3 SKY_ZENITH_NIGHT{0.020f, 0.035f, 0.085f};
constexpr glm::vec3 SKY_HORIZON_NIGHT{0.050f, 0.070f, 0.130f};
constexpr glm::vec3 SKY_ZENITH_DUSK{0.20f, 0.20f, 0.42f};
constexpr glm::vec3 SKY_HORIZON_DUSK{0.86f, 0.46f, 0.26f}; // low sun burn
constexpr glm::vec3 SUN_COLOR_LOW{1.00f, 0.55f, 0.28f};    // horizon warmth
constexpr glm::vec3 AMBIENT_NIGHT{0.070f, 0.090f, 0.150f}; // navigable blue
constexpr glm::vec3 AMBIENT_DUSK{0.26f, 0.24f, 0.28f};
// The moon: cold white, and CONSTANT — a dim moon is dim because moon_light
// drops, not because the disc changes colour. The same value tints the ground
// (the shader scales it by DFN_MOON_GROUND_MAX); the night's blue comes from
// the ambient floor, not from tinting the moon itself.
constexpr glm::vec3 MOON_DISC_COLOR{0.88f, 0.91f, 1.00f};

float clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

float smooth01(float edge0, float edge1, float x) {
    const float t = clamp01((x - edge0) / std::max(edge1 - edge0, 1e-6f));
    return t * t * (3.0f - 2.0f * t);
}

glm::vec3 mix3(const glm::vec3& a, const glm::vec3& b, float t) {
    return a + (b - a) * t;
}

// Direction on the tilted arc for an angle where 0 = below (midnight) and
// pi/2 = due east horizon... expressed directly: `angle` runs with the clock.
glm::vec3 arc_direction(float angle) {
    // Rises in the east (+X), sets in the west (-X), culminating south (+Z).
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    const glm::vec3 dir{c, s, SKY_ARC_TILT * (1.0f - std::fabs(s) * 0.35f)};
    return glm::normalize(dir);
}

} // namespace

glm::vec3 sun_direction_at(float day_fraction) {
    // 0.25 -> east horizon (elevation 0), 0.5 -> highest, 0.75 -> west horizon.
    const float angle = (day_fraction - 0.25f) * TAU;
    return arc_direction(angle);
}

float moon_illumination(float lunar_phase) {
    const float phase = lunar_phase - std::floor(lunar_phase);
    // 0 = new (dark), 0.5 = full. Smooth, symmetric around full.
    return 0.5f * (1.0f - std::cos(phase * TAU));
}

glm::vec3 moon_direction_at(float day_fraction, float lunar_phase) {
    // Elongation from the sun IS the phase angle: full (0.5) sits opposite the
    // sun and therefore rises at sunset; new (0) rides with the sun and is
    // invisible in daylight. Same arc, offset in time.
    const float phase = lunar_phase - std::floor(lunar_phase);
    const float angle = (day_fraction - 0.25f) * TAU + phase * TAU;
    return arc_direction(angle);
}

void apply_sky_time(platform::RenderEnvironment& env, float day_fraction,
                    float lunar_phase) {
    const float day = day_fraction - std::floor(day_fraction);
    const glm::vec3 sun = sun_direction_at(day);
    const glm::vec3 moon = moon_direction_at(day, lunar_phase);

    // Everything below is keyed off sun ELEVATION, never off the clock, so the
    // palette is identical at a given sun height whatever the day length.
    const float elevation = sun.y;
    const float day_t = smooth01(SKY_NIGHT_ELEVATION, SKY_DAY_ELEVATION, elevation);
    // Dusk peaks when the sun sits ON the horizon and fades both ways.
    const float dusk_t = 1.0f - smooth01(0.0f, 0.30f, std::fabs(elevation));
    const float night_t = 1.0f - day_t;

    env.sun_direction = sun;
    // Sun colour reddens as it drops; below the horizon it stops contributing
    // (the backend also disables shadows there).
    const float low_t = 1.0f - smooth01(0.0f, 0.35f, elevation);
    const glm::vec3 sun_hue = mix3(LOOKDEV_SUN_COLOR, SUN_COLOR_LOW, low_t);
    env.sun_color = sun_hue * clamp01(smooth01(SKY_NIGHT_ELEVATION, 0.10f, elevation));

    // Ambient: day -> dusk -> a navigable night blue (user decision: night is
    // playable-dark under the moon, not a torch-only experience).
    glm::vec3 ambient = mix3(AMBIENT_NIGHT, LOOKDEV_AMBIENT_COLOR, day_t);
    ambient = mix3(ambient, AMBIENT_DUSK, dusk_t * 0.5f);
    env.ambient_color = ambient;

    // Sky gradient: night -> dusk burn -> day.
    glm::vec3 zenith = mix3(SKY_ZENITH_NIGHT, LOOKDEV_SKY_ZENITH, day_t);
    glm::vec3 horizon = mix3(SKY_HORIZON_NIGHT, LOOKDEV_SKY_HORIZON, day_t);
    zenith = mix3(zenith, SKY_ZENITH_DUSK, dusk_t * 0.65f);
    horizon = mix3(horizon, SKY_HORIZON_DUSK, dusk_t * 0.85f);
    env.sky_zenith_color = zenith;
    env.sky_horizon_color = horizon;
    // Fog always matches the horizon so distant terrain melts into the sky at
    // every hour — the rule that keeps the world edge invisible.
    env.fog_color = horizon;
    env.fog_start_m = LOOKDEV_FOG_START_FRAC * static_cast<float>(config::CAMERA_FAR);
    env.fog_end_m = LOOKDEV_FOG_END_FRAC * static_cast<float>(config::CAMERA_FAR);

    // Moon: the disc is drawn whenever it is up; its LIGHT also scales with
    // how much of it is lit (user decision: phases change real brightness).
    env.moon_direction = moon;
    env.moon_phase = lunar_phase - std::floor(lunar_phase);
    env.moon_color = MOON_DISC_COLOR;
    const float moon_up = smooth01(-0.05f, 0.20f, moon.y);
    env.moon_light = moon_illumination(lunar_phase) * moon_up * night_t;

    // Stars: out only once the sun is properly down, and washed out by a bright
    // moon. Explicit field, so overcast can later zero it without touching
    // anything else.
    const float star_night = 1.0f - smooth01(SKY_NIGHT_ELEVATION, 0.02f, elevation);
    env.star_intensity = star_night * (1.0f - 0.45f * env.moon_light);
}

} // namespace dfn::render
