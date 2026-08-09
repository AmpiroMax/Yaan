/*
Created: 09:08:2026 - 19:52:10
Last updated: 09:08:2026 - 19:52:10
Module: tests
File: tests/render/SkyModelTests.cpp

Responsibility:
- Unit tests for the day/night model: sun and moon geometry, the phase ->
  position coupling, and the environment invariants the look depends on.

Key items:
- doctest cases over sun_direction_at / moon_direction_at / moon_illumination /
  apply_sky_time.

Dependencies:
- Uses: doctest, engine/render SkyModel.
- Used by: ctest (render_sky_model).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. GPU-free.
*/
/*
UPD:
- 09:08:2026 - 19:52:10: Initial tests with the day/night stage.
*/

#include "engine/render/sources/SkyModel.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

using dfn::platform::RenderEnvironment;
using dfn::render::apply_sky_time;
using dfn::render::moon_direction_at;
using dfn::render::moon_illumination;
using dfn::render::sun_direction_at;

TEST_CASE("the sun rises in the east, culminates, sets in the west") {
    CHECK(sun_direction_at(0.0f).y < -0.5f);   // midnight: well below
    CHECK(sun_direction_at(0.5f).y > 0.5f);    // noon: well above
    CHECK(sun_direction_at(0.25f).y == doctest::Approx(0.0f).epsilon(0.05));
    CHECK(sun_direction_at(0.75f).y == doctest::Approx(0.0f).epsilon(0.05));
    CHECK(sun_direction_at(0.25f).x > 0.5f);   // sunrise in the east (+X)
    CHECK(sun_direction_at(0.75f).x < -0.5f);  // sunset in the west
    // The arc is tilted south so noon is never straight overhead: a zenith sun
    // flattens every shadow and the world loses its ground contact.
    CHECK(sun_direction_at(0.5f).z > 0.1f);
    // Wrapping is seamless.
    CHECK(sun_direction_at(1.25f).y == doctest::Approx(sun_direction_at(0.25f).y));
}

TEST_CASE("moon phase and position are one thing, not two") {
    const float day = 0.75f; // sunset
    const glm::vec3 sun = sun_direction_at(day);
    // Full moon sits opposite the sun -> it rises exactly as the sun sets.
    // Not a perfect antipode: both ride the same SOUTH-TILTED arc (our stand-in
    // for the ecliptic), so the shared southward component keeps the dot around
    // -0.66 rather than -1. What matters is the opposite HEADING and that both
    // sit on the horizon at the same moment.
    const glm::vec3 full = moon_direction_at(day, 0.5f);
    CHECK(glm::dot(sun, full) < -0.5f);
    CHECK(sun.x < -0.5f);   // sun setting in the west
    CHECK(full.x > 0.5f);   // moon rising in the east, the same instant
    CHECK(full.y == doctest::Approx(0.0f).epsilon(0.08)); // on the horizon
    // New moon rides with the sun -> invisible in daylight, sets with it.
    const glm::vec3 fresh = moon_direction_at(day, 0.0f);
    CHECK(glm::dot(sun, fresh) > 0.9f);
}

TEST_CASE("illumination runs new -> full -> new") {
    CHECK(moon_illumination(0.0f) == doctest::Approx(0.0f));
    CHECK(moon_illumination(0.5f) == doctest::Approx(1.0f));
    CHECK(moon_illumination(0.25f) == doctest::Approx(0.5f));
    CHECK(moon_illumination(0.75f) == doctest::Approx(0.5f));
    CHECK(moon_illumination(1.5f) == doctest::Approx(1.0f)); // wraps
}

TEST_CASE("day and night environments differ in the ways the look depends on") {
    RenderEnvironment noon;
    apply_sky_time(noon, 0.5f, 0.5f);
    RenderEnvironment night;
    apply_sky_time(night, 0.0f, 0.5f);

    // Stars are a night-only thing, and the sun stops contributing at night.
    CHECK(noon.star_intensity == doctest::Approx(0.0f));
    CHECK(night.star_intensity > 0.3f);
    CHECK(night.sun_color.r == doctest::Approx(0.0f));
    CHECK(noon.sun_color.r > 0.5f);

    // Night is PLAYABLE-DARK (user decision): dimmer than day by a lot, but
    // never black — the surface must stay navigable under the moon.
    const float night_ambient = night.ambient_color.r + night.ambient_color.g
                              + night.ambient_color.b;
    const float day_ambient = noon.ambient_color.r + noon.ambient_color.g
                            + noon.ambient_color.b;
    CHECK(night_ambient > 0.15f);
    CHECK(night_ambient < day_ambient * 0.5f);
    // ...and it is blue, not grey.
    CHECK(night.ambient_color.b > night.ambient_color.r * 1.5f);

    // Fog always matches the horizon: that is what keeps the world edge and
    // the streaming boundary invisible at every hour.
    CHECK(noon.fog_color.r == doctest::Approx(noon.sky_horizon_color.r));
    CHECK(night.fog_color.b == doctest::Approx(night.sky_horizon_color.b));
}

TEST_CASE("moon phase changes real ground brightness, not just the disc") {
    RenderEnvironment full;
    apply_sky_time(full, 0.0f, 0.5f); // midnight, full moon (high)
    RenderEnvironment dark;
    apply_sky_time(dark, 0.0f, 0.0f); // midnight, new moon
    CHECK(full.moon_light > 0.5f);
    CHECK(dark.moon_light == doctest::Approx(0.0f));
    // The disc colour is constant — a dim moon is dim through moon_light.
    CHECK(full.moon_color.r == doctest::Approx(dark.moon_color.r));
    // A bright moon washes stars out somewhat, but never removes them.
    CHECK(full.star_intensity < dark.star_intensity);
    CHECK(full.star_intensity > 0.2f);
}

TEST_CASE("apply_sky_time leaves non-sky fields alone") {
    RenderEnvironment env;
    env.terrain_tiles_per_chunk = 17.0f;
    env.water_color = {0.1f, 0.2f, 0.3f, 0.4f};
    env.point_light_count = 1;
    env.point_lights[0].radius_m = 9.0f;
    apply_sky_time(env, 0.33f, 0.2f);
    CHECK(env.terrain_tiles_per_chunk == doctest::Approx(17.0f));
    CHECK(env.water_color.a == doctest::Approx(0.4f));
    CHECK(env.point_light_count == 1);
    CHECK(env.point_lights[0].radius_m == doctest::Approx(9.0f));
}
