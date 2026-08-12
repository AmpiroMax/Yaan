/*
Created: 09:08:2026 - 18:58:10
Last updated: 12:08:2026 - 23:52:00
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
- 12:08:2026 - 23:52:00: TWO MOONS (W9) — the orbital half. MoonElements / masser() / secunda() /
  moon_state_at(): elongation from the WORLD CLOCK through each moon's own
  synodic period (so the pair cannot share one number the way the shipped model
  forced), inclination about a RETROGRADE node line, the equation of centre, and
  the apparent radius that swings with it. Every constant is an existing NUMBERS
  row from a block that had had no consumer for two days. NOT DONE AND NOT MINE:
  RenderEnvironment carries ONE moon, so the second cannot reach the shader
  until the lead lands the fields (Rule 26).
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

// ===========================================================================
// TWO MOONS (W9, the user's request verbatim: «в идеале я хочу 2 луны и чтоб
// обе вокруг земли крутились, пусть луна и днем и ночью видна, но не ярко
// днем, плюс пусть луна больше солнца будет»).
//
// EVERY NUMBER BELOW IS A NUMBERS ROW ALREADY DERIVED AND ALREADY GENERATED —
// the W9 block landed 10.08 with 24 rows and has had NO consumer since, which
// is why `MASSER_SYNODIC_DAYS` sits in the registry beside a `LUNAR_MONTH_DAYS`
// that was retired specifically to stop a two-moon codebase grabbing the wrong
// period. Nothing here is invented; this is the arithmetic that reads them.
//
// WHAT IS STILL MISSING AND IS NOT MINE TO ADD: `RenderEnvironment` carries ONE
// moon (direction / colour / phase / light). A second moon needs a second set
// of fields and two more env-block slots, and `IRenderer.h` is a frozen
// contract (Rule 26) — that diff is the lead's. The pure geometry lives here
// now so that when the fields land, the shader half is mechanical and the
// numbers have already been tested.
// ===========================================================================

/// The orbital elements of one moon. A struct rather than two parallel sets of
/// free functions because the whole point of the W9 block is that the two moons
/// differ ONLY in their numbers: same construction, ratios chosen at the golden
/// section so the pair never resonates (SECUNDA_SYNODIC_DAYS = 28/phi, and the
/// registry's control for that row is a REAL SHIPPED GAME — Skyrim's 6:5 speeds
/// bring its moons together every five nights).
struct MoonElements {
    float synodic_days;       ///< days from one new moon to the next
    float elongation_epoch;   ///< elongation at elapsed_days 0, radians
    float inclination;        ///< orbit tilt out of the sun's arc, radians
    float node_epoch;         ///< longitude of the ascending node at day 0
    float node_period_days;   ///< RETROGRADE node regression period
    float eccentricity;       ///< orbit eccentricity (equation of centre)
    float angular_diameter;   ///< mean apparent diameter, radians
    float disc_luma;          ///< quantiser luma of the lit disc
};

/// Masser — the big rusty one. Angular diameter 5.60 deg = 2.55x the sun's
/// disc, which is the user's «луна больше солнца» satisfied by the larger of
/// the pair; Secunda's 2.60 deg is 1.18x, so it holds for BOTH.
[[nodiscard]] MoonElements masser();

/// Secunda — the small neutral one. Its periods are Masser's divided by the
/// golden ratio, in both the synodic and the nodal term.
[[nodiscard]] MoonElements secunda();

/// Where a moon is and what it looks like at a moment.
struct MoonState {
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; ///< TOWARD the moon, normalized
    float elongation = 0.0f;   ///< angle from the sun along the orbit, radians
    float phase = 0.0f;        ///< 0 = new, 0.5 = full, wraps at 1 (shader form)
    float illumination = 0.0f; ///< lit fraction of the disc, 0..1
    float angular_radius = 0.0f; ///< HALF the apparent diameter now, radians
    float solar_separation = 0.0f; ///< angle to the sun, radians
    bool observable = false;   ///< outside MOON_SOLAR_EXCLUSION of the sun
};

/// The moon's state at a moment. `day_fraction` is the time of day (same
/// convention as sun_direction_at) and `elapsed_days` is the world clock in
/// game days — the phase is DERIVED from it through the synodic period rather
/// than being passed in, which is what makes the two moons disagree with each
/// other instead of sharing one number.
[[nodiscard]] MoonState moon_state_at(const MoonElements& m, float day_fraction,
                                      double elapsed_days);

/// Writes the whole sky/lighting state for this moment into `env`: sun and
/// moon direction, sun/ambient/fog/sky colours, moonlight and stars. Every
/// other field of `env` (splat thresholds, water, point light) is untouched,
/// so the app may override any single value after the call.
void apply_sky_time(platform::RenderEnvironment& env, float day_fraction,
                    float lunar_phase);

} // namespace dfn::render
