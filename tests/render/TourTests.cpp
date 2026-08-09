/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 11:57:20
Module: tests
File: tests/render/TourTests.cpp

Responsibility:
- Unit tests for the screenshot tour: env parsing, step walking against the
  null renderer (headless smoke path, Rule 3), camera pose application.

Key items:
- doctest cases over dfn::render::Tour + NullRenderer.

Dependencies:
- Uses: doctest, engine/render Tour, null renderer backend.
- Used by: ctest (render_tour).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial tests.
- 09:08:2026 - 11:57:20: Stage 3b — ground-relative resolution, focus
  position, testbed_steps route shape.
*/

#include "engine/render/sources/Tour.h"

#include "engine/platform/render/sources/null/CreateNullRenderer.h"

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

using dfn::render::Tour;
using dfn::render::TourStep;

namespace {

const std::string TMP_DIR =
    (std::filesystem::temp_directory_path() / "dfn_tour_test").string();

} // namespace

TEST_CASE("enabled_by_env honors DFN_TOUR") {
    ::unsetenv("DFN_TOUR");
    CHECK_FALSE(Tour::enabled_by_env());
    ::setenv("DFN_TOUR", "1", 1);
    CHECK(Tour::enabled_by_env());
    ::setenv("DFN_TOUR", "0", 1);
    CHECK_FALSE(Tour::enabled_by_env());
    ::unsetenv("DFN_TOUR");
}

TEST_CASE("internal_res_from_env parses WxH and falls back on junk") {
    const glm::uvec2 fallback{640, 360};
    ::unsetenv("DFN_INTERNAL_RES");
    CHECK(Tour::internal_res_from_env(fallback) == fallback);
    ::setenv("DFN_INTERNAL_RES", "320x180", 1);
    CHECK(Tour::internal_res_from_env(fallback) == glm::uvec2{320, 180});
    ::setenv("DFN_INTERNAL_RES", "garbage", 1);
    CHECK(Tour::internal_res_from_env(fallback) == fallback);
    ::unsetenv("DFN_INTERNAL_RES");
}

TEST_CASE("tour walks all steps headless and reports completion once") {
    auto renderer = dfn::platform::create_null_renderer();
    REQUIRE(renderer != nullptr);

    Tour tour;
    tour.begin({{"a", {0.0f, 1.0f, 0.0f}, 0.0f, 0.0f, 2},
                {"b", {5.0f, 1.0f, 5.0f}, 1.0f, -0.2f, 0}},
               TMP_DIR);
    REQUIRE(tour.active());
    CHECK(tour.current_step() == 0);

    // The tour must terminate in bounded frames even though the null backend
    // rejects screenshots (Rule 3: headless is a runnable mode).
    int frames = 0;
    bool done = false;
    while (!done && frames < 100) {
        done = tour.post_frame(*renderer);
        ++frames;
    }
    CHECK(done);
    CHECK_FALSE(tour.active());
    CHECK(frames > 4); // waits + flush frames actually happened
    CHECK(tour.post_frame(*renderer)); // stays done

    std::filesystem::remove_all(TMP_DIR);
}

TEST_CASE("apply pins the camera to the current step with a static pose") {
    Tour tour;
    tour.begin({{"vantage", {3.0f, 2.0f, 1.0f}, 0.5f, -0.25f, 5}}, TMP_DIR);

    dfn::render::FirstPersonCamera camera;
    tour.apply(camera);
    const auto pose = camera.interpolated_pose(0.37f); // any alpha: static pose
    CHECK(pose.position.x == doctest::Approx(3.0f));
    CHECK(pose.position.y == doctest::Approx(2.0f));
    CHECK(pose.position.z == doctest::Approx(1.0f));
    CHECK(pose.yaw == doctest::Approx(0.5f));
    CHECK(pose.pitch == doctest::Approx(-0.25f));
    std::filesystem::remove_all(TMP_DIR);
}

TEST_CASE("default steps satisfy the stage-3 acceptance shape") {
    const auto steps = Tour::default_steps();
    REQUIRE(steps.size() == 6); // stage-3 route: texture, horizon, slope,
                                // water valley, overview, sky
    for (const auto& step : steps) {
        CHECK_FALSE(step.label.empty());
        CHECK(step.position.y > 0.0f); // above the flat test chunk's ground
    }
}

TEST_CASE("ground-relative steps resolve through the begin callback") {
    Tour tour;
    // Ground function: a tilted plane so each vantage sees a distinct height.
    tour.begin({{"rel", {100.0f, 1.7f, 200.0f}, 0.0f, 0.0f, 5, true}}, TMP_DIR,
               [](glm::vec2 p) { return p.x * 0.1f; });

    CHECK(tour.focus_position().y == doctest::Approx(100.0f * 0.1f + 1.7f));
    dfn::render::FirstPersonCamera camera;
    tour.apply(camera);
    CHECK(camera.interpolated_pose(0.0f).position.y
          == doctest::Approx(100.0f * 0.1f + 1.7f));
    std::filesystem::remove_all(TMP_DIR);
}

TEST_CASE("ground-relative steps without a callback fall back to absolute y") {
    Tour tour;
    tour.begin({{"rel", {10.0f, 3.0f, 10.0f}, 0.0f, 0.0f, 5, true}}, TMP_DIR);
    CHECK(tour.focus_position().y == doctest::Approx(3.0f));
    std::filesystem::remove_all(TMP_DIR);
}

TEST_CASE("testbed steps cover the §7.1 must-have frames inside the testbed") {
    const auto steps = Tour::testbed_steps();
    REQUIRE(steps.size() == 7); // crag from hamlet, crag final approach,
                                // river ford, lake bluff, hamlet, forest
                                // species, overview
    for (const auto& step : steps) {
        CHECK_FALSE(step.label.empty());
        CHECK(step.ground_relative); // heights resolve against real terrain
        CHECK(step.wait_frames >= 30); // streaming refocus must settle
        // Inside the 1024 m testbed (LANDSCAPE §7 canvas).
        CHECK(step.position.x > 0.0f);
        CHECK(step.position.x < 1024.0f);
        CHECK(step.position.z > 0.0f);
        CHECK(step.position.z < 1024.0f);
    }
    CHECK(steps.front().label == "crag_from_vaelmere"); // the L0 money shot
}
