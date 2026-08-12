/*
Created: 09:08:2026 - 19:52:10
Last updated: 12:08:2026 - 23:52:00
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
- 12:08:2026 - 23:52:00: SIX CASES FOR THE TWO MOONS (W9), each with the control its NUMBERS row
  names. Both moons up, apart and lit in the FIRST FRAME (control: the shipped
  one-moon model, which returns illumination 0.000 there — the defect the epochs
  exist for). Both discs bigger than the sun, 2.54x and 1.18x. The pair never
  repeats its configuration inside 400 days (CONTROL: Skyrim's real shipped 6:5
  speed ratio, which repeats in 140). Latitude divergence clears two Masser
  diameters (CONTROL: identical orbits, which cannot diverge at all). Zero
  inclination and zero eccentricity reproduce the shipped arc exactly, so the
  new terms are additions and not a rewrite. MY FIRST RESONANCE TEST MEASURED
  THE WRONG THING and its control PASSED: it counted which separations the pair
  visits, and every ratio visits them all — a resonant pair simply revisits them
  sooner. "Which values" was never the question; "how soon again" is.
*/

#include "engine/render/sources/SkyModel.h"

#include "engine/core/config/sources/Constants.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

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

// ===========================================================================
// TWO MOONS (W9). The registry's 24 rows had no consumer for two days; these
// are the assertions that make them one. Every case ships the control the
// NUMBERS row itself names — in two of them the control is a REAL SHIPPED GAME
// or the code that shipped here yesterday, which is Rule 30's strongest form.
// ===========================================================================

namespace {

constexpr float TAU_T = 6.28318530718f;

float elevation(const glm::vec3& d) { return d.y; }

float wrap_pi(float a) {
    while (a > 3.14159265f) { a -= TAU_T; }
    while (a < -3.14159265f) { a += TAU_T; }
    return a;
}

} // namespace

TEST_CASE("BOTH moons are up, apart and lit IN THE FIRST FRAME") {
    // This is MASSER/SECUNDA_ELONGATION_EPOCH's own justification, and the row
    // exists because the shipped one-moon model failed it: `angle = sun +
    // phase*TAU` at day 0 put the moon at elongation 0 — new, unlit, and inside
    // the sun's glare — so the game had never once STARTED with a visible moon,
    // and the tour freezes exactly that hour. «Луна сильно маленькая» is what a
    // person says about something they have never seen.
    const float t0 = static_cast<float>(dfn::config::START_TIME_OF_DAY);
    const auto m = dfn::render::moon_state_at(dfn::render::masser(), t0, 0.0);
    const auto s = dfn::render::moon_state_at(dfn::render::secunda(), t0, 0.0);
    CHECK(elevation(m.direction) > 0.0f);
    CHECK(elevation(s.direction) > 0.0f);
    // A terminator is only readable if the disc is neither new nor full.
    CHECK(m.illumination > 0.15f);
    CHECK(m.illumination < 0.95f);
    CHECK(s.illumination > 0.15f);
    CHECK(s.illumination < 0.95f);
    // Neither sits inside the sun's exclusion.
    CHECK(m.observable);
    CHECK(s.observable);
    // And they are far enough apart to read as TWO moons rather than a blur:
    // the row claims ~30 deg, and the floor is two Masser diameters.
    const float sep = std::acos(std::clamp(
        glm::dot(m.direction, s.direction), -1.0f, 1.0f));
    INFO("separation at first frame = ", sep * 57.2958f, " deg");
    CHECK(sep > 2.0f * static_cast<float>(dfn::config::MASSER_ANGULAR_DIAMETER));

    // CONTROL — the model that shipped. Fed the same first frame it produces a
    // NEW moon: illumination 0, which is the defect the epochs exist to fix.
    CHECK(moon_illumination(0.0f) == doctest::Approx(0.0f));
}

TEST_CASE("BOTH moons are bigger than the sun (the user asked for it)") {
    // «пусть луна больше солнца будет» — and it holds for the PAIR, not only
    // for the big one, which is the honest reading of the request.
    const float sun = static_cast<float>(dfn::config::SUN_ANGULAR_DIAMETER);
    CHECK(dfn::render::masser().angular_diameter > sun);
    CHECK(dfn::render::secunda().angular_diameter > sun);
    CHECK(dfn::render::masser().angular_diameter / sun
          == doctest::Approx(2.54f).epsilon(0.02));
    CHECK(dfn::render::secunda().angular_diameter / sun
          == doctest::Approx(1.18f).epsilon(0.02));
    // Masser is "well over twice Secunda's size" (the lore constraint the row
    // cites), and that is a RATIO assertion, not two size assertions.
    CHECK(dfn::render::masser().angular_diameter
              / dfn::render::secunda().angular_diameter
          > 2.0f);
}

TEST_CASE("the two moons NEVER settle into a rhythm — and Skyrim's do") {
    // SECUNDA_SYNODIC_DAYS = 28/phi exists so that no simple fraction fits the
    // period ratio and the pair therefore never returns to a configuration it
    // has already stood in. The claim is operational and it is about the PAIR,
    // not about either moon: find the SHORTEST interval after which BOTH moons
    // come back to where they were, to within a tolerance a player could see.
    //
    // THE FIRST VERSION OF THIS TEST MEASURED THE WRONG THING and passed its own
    // control: it counted how many distinct SEPARATIONS the pair visited, and
    // every ratio visits them all — a resonant pair simply revisits them in a
    // short cycle. "Which values" was never the question; "how soon again" is.
    const auto shortest_repeat = [](float period_b, float tol_rad) {
        const float pa = dfn::render::masser().synodic_days;
        for (int day = 1; day <= 400; ++day) {
            const float d = static_cast<float>(day);
            const float da = std::fabs(wrap_pi(TAU_T * d / pa));
            const float db = std::fabs(wrap_pi(TAU_T * d / period_b));
            if (da < tol_rad && db < tol_rad) {
                return day;
            }
        }
        return 0; // no repeat inside 400 days
    };
    // A tenth of a turn is 36 deg — well over a Masser diameter, so this is a
    // generous tolerance and the control has to fail it anyway.
    const float tol = TAU_T / 36.0f; // 10 deg
    const int golden = shortest_repeat(dfn::render::secunda().synodic_days, tol);
    INFO("shortest configuration repeat, golden ratio = ", golden, " days");
    CHECK(golden == 0); // never, inside more than a year of play

    // CONTROL, AND IT IS A REAL SHIPPED GAME. Skyrim's fMasserSpeed 0.25 against
    // fSecundaSpeed 0.30 is exactly 6:5, so its moons return to the same pairing
    // on a short cycle. Given the same construction that ratio must FAIL here,
    // and it does — which is what makes this an invariant rather than a
    // description.
    const int resonant =
        shortest_repeat(dfn::render::masser().synodic_days * 5.0f / 6.0f, tol);
    INFO("shortest configuration repeat, Skyrim's 6:5 = ", resonant, " days");
    CHECK(resonant > 0);
    CHECK(resonant <= 140);
}

TEST_CASE("the moons' LATITUDES separate, and equal orbits must fail it") {
    // SECUNDA_INCLINATION's row: max latitude divergence over a full beat
    // period must clear two Masser diameters, and it claims 17.14 deg against
    // 10.02. AGGREGATION: the maximum. DENOMINATOR: the larger moon's diameter.
    const auto max_divergence = [](const dfn::render::MoonElements& a,
                                   const dfn::render::MoonElements& b) {
        float worst = 0.0f;
        for (int k = 0; k <= 4530; ++k) {
            const double d = static_cast<double>(k) * 0.01;
            const auto sa = dfn::render::moon_state_at(a, 0.30f, d);
            const auto sb = dfn::render::moon_state_at(b, 0.30f, d);
            worst = std::max(worst, std::fabs(std::asin(std::clamp(
                sa.direction.y, -1.0f, 1.0f))
                - std::asin(std::clamp(sb.direction.y, -1.0f, 1.0f))));
        }
        return worst;
    };
    const float floor_rad =
        2.0f * static_cast<float>(dfn::config::MASSER_ANGULAR_DIAMETER);
    const float real = max_divergence(dfn::render::masser(),
                                      dfn::render::secunda());
    INFO("max latitude divergence = ", real * 57.2958f, " deg, floor ",
         floor_rad * 57.2958f);
    CHECK(real > floor_rad);

    // CONTROL — the row names it: equal inclinations AND equal nodes give a
    // pair that can never separate in latitude at all.
    dfn::render::MoonElements twin = dfn::render::secunda();
    twin.inclination = dfn::render::masser().inclination;
    twin.node_epoch = dfn::render::masser().node_epoch;
    twin.node_period_days = dfn::render::masser().node_period_days;
    twin.synodic_days = dfn::render::masser().synodic_days;
    twin.elongation_epoch = dfn::render::masser().elongation_epoch;
    twin.eccentricity = dfn::render::masser().eccentricity;
    const float control = max_divergence(dfn::render::masser(), twin);
    INFO("CONTROL (identical orbits) divergence = ", control * 57.2958f, " deg");
    CHECK(control < 1e-4f);
    CHECK(real > floor_rad);
}

TEST_CASE("CONTROL: a moon with no inclination and no eccentricity IS the old arc") {
    // The new geometry must contain the shipped one as its zero case, or the
    // two terms are not additions but a rewrite, and nothing above would be
    // measuring what it claims.
    dfn::render::MoonElements flat = dfn::render::masser();
    flat.inclination = 0.0f;
    flat.eccentricity = 0.0f;
    for (float t : {0.1f, 0.3f, 0.55f, 0.8f}) {
        const auto s = dfn::render::moon_state_at(flat, t, 3.0);
        const glm::vec3 old = moon_direction_at(t, s.phase);
        CHECK(glm::dot(s.direction, old) == doctest::Approx(1.0f).epsilon(1e-4));
    }
}

TEST_CASE("the phase is DERIVED from the clock, not handed in") {
    // Each moon walks its own synodic period: one full turn of elongation per
    // MASSER_SYNODIC_DAYS, and the two must disagree by construction.
    const auto m = dfn::render::masser();
    const auto a = dfn::render::moon_state_at(m, 0.3f, 0.0);
    const auto b = dfn::render::moon_state_at(m, 0.3f,
                                              static_cast<double>(m.synodic_days));
    CHECK(std::fabs(wrap_pi(a.elongation - b.elongation)) < 1e-3f);
    const auto half = dfn::render::moon_state_at(
        m, 0.3f, static_cast<double>(m.synodic_days) * 0.5);
    CHECK(std::fabs(wrap_pi(half.elongation - a.elongation - 3.14159265f))
          < 1e-3f);
    // Same day, different moons, different phases — the whole point.
    const auto s = dfn::render::moon_state_at(dfn::render::secunda(), 0.3f, 9.0);
    const auto mm = dfn::render::moon_state_at(m, 0.3f, 9.0);
    CHECK(std::fabs(wrap_pi(mm.elongation - s.elongation)) > 0.2f);
}
