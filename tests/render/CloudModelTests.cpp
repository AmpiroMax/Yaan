/*
Created: 10:08:2026 - 03:13:00
Last updated: 10:08:2026 - 03:13:00
Module: tests
File: tests/render/CloudModelTests.cpp

Responsibility:
- Unit tests for the cloud drift model (W4): the drift is a pure function of
  time, moves the coverage PATTERN downwind, obeys the state's wind
  multiplier, and apply_clouds touches only its own fields.

Key items:
- doctest cases over cloud_drift_offset / apply_clouds.

Dependencies:
- Uses: doctest, engine/render CloudModel.
- Used by: ctest (render_cloud_model).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. GPU-free.
*/
/*
UPD:
- 10:08:2026 - 03:13:00: Initial tests with the cloud pass.
*/

#include "engine/render/sources/CloudModel.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

using dfn::platform::RenderEnvironment;
using dfn::render::apply_clouds;
using dfn::render::cloud_drift_offset;
using dfn::render::WIND_FIELD_DRIFT_SPEED_MPS;

TEST_CASE("the coverage pattern travels DOWNWIND at the NUMBERS speed") {
    const glm::vec2 wind{1.0f, 0.0f};
    const glm::vec2 o30 = cloud_drift_offset(wind, 1.0f, 30.0f);
    // Samplers read field(p + offset): a NEGATIVE offset along the wind is
    // what moves the pattern toward +wind across the world (W2.3: weather
    // arrives from upwind). The sign is the whole feature — a positive offset
    // would ship weather marching INTO the wind, which no frame would catch
    // without this assertion.
    CHECK(o30.x == doctest::Approx(-WIND_FIELD_DRIFT_SPEED_MPS * 30.0f));
    CHECK(o30.y == doctest::Approx(0.0f));
    // The acceptance pair: 30 s of game time moves the field 300 m — half a
    // WIND_FIELD_WAVELENGTH, the "unmistakable at 640x360" derivation.
    CHECK(glm::length(o30) == doctest::Approx(300.0f));
    // Pure function of time: same inputs, same offset (the W2.5 form — any
    // reported frame reproduces from its timestamp alone).
    CHECK(cloud_drift_offset(wind, 1.0f, 30.0f) == o30);
}

TEST_CASE("drift direction follows the wind, never a second wind") {
    const glm::vec2 wind = glm::normalize(glm::vec2{0.87f, 0.50f});
    const glm::vec2 o = cloud_drift_offset(wind, 1.0f, 10.0f);
    // Anti-parallel to the wind vector, exactly.
    const glm::vec2 dir = glm::normalize(o);
    CHECK(dir.x == doctest::Approx(-wind.x));
    CHECK(dir.y == doctest::Approx(-wind.y));
    // An unnormalized direction must not scale the speed (a stale env value
    // is a direction, not a magnitude).
    const glm::vec2 o_scaled = cloud_drift_offset(wind * 7.0f, 1.0f, 10.0f);
    CHECK(glm::length(o_scaled) == doctest::Approx(glm::length(o)));
}

TEST_CASE("CONTROL: a becalmed state pins the field still") {
    // Rule 30's rejected case for the multiplier: weather_wind_mult 0 (a
    // dead-calm state) must freeze the drift entirely; if this returned any
    // motion the state tuple would not actually govern the wind.
    const glm::vec2 wind{1.0f, 0.0f};
    CHECK(cloud_drift_offset(wind, 0.0f, 1000.0f) == glm::vec2{0.0f, 0.0f});
    // Degenerate direction: stand still rather than drift along garbage.
    CHECK(cloud_drift_offset({0.0f, 0.0f}, 1.0f, 1000.0f)
          == glm::vec2{0.0f, 0.0f});
}

TEST_CASE("apply_clouds writes drift + wavelength and NOTHING else") {
    RenderEnvironment env;
    env.wind_direction = {0.0f, 1.0f};
    env.weather_wind_mult = 2.0f;
    const float cover = env.cloud_cover;
    const float cumulus = env.cloud_cumulus;
    const float shadow = env.cloud_shadow;
    apply_clouds(env, 5.0f);
    // Offset: -dir * speed * mult * t.
    CHECK(env.cloud_offset_m.x == doctest::Approx(0.0f));
    CHECK(env.cloud_offset_m.y
          == doctest::Approx(-WIND_FIELD_DRIFT_SPEED_MPS * 2.0f * 5.0f));
    CHECK(env.cloud_wavelength_m == doctest::Approx(600.0f));
    // The STATE tuple is the app's/schedule's to write; the drift model must
    // never touch it (two writers of one tuple is the Rule 35 state defect).
    CHECK(env.cloud_cover == cover);
    CHECK(env.cloud_cumulus == cumulus);
    CHECK(env.cloud_shadow == shadow);
}
