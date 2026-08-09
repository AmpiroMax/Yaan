/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
Module: tests
File: tests/render/CameraTests.cpp

Responsibility:
- Unit tests for FirstPersonCamera: pose interpolation (endpoints, shortest-arc
  yaw, pitch clamp), view/proj construction, direction conventions.

Key items:
- doctest cases over dfn::render::FirstPersonCamera.

Dependencies:
- Uses: doctest, engine/render FirstPersonCamera, dfn::config.
- Used by: ctest (render_camera).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial tests.
*/

#include "engine/render/sources/FirstPersonCamera.h"

#include "engine/core/config/sources/Constants.h"

#include <doctest/doctest.h>
#include <glm/gtc/constants.hpp>

using dfn::render::CameraPose;
using dfn::render::FirstPersonCamera;

namespace {
constexpr float PI = glm::pi<float>();
}

TEST_CASE("interpolation hits both endpoints exactly") {
    FirstPersonCamera cam;
    const CameraPose a{{0.0f, 0.0f, 0.0f}, 0.2f, 0.1f};
    const CameraPose b{{4.0f, 2.0f, -6.0f}, 0.8f, -0.3f};
    cam.set_poses(a, b);

    const CameraPose at0 = cam.interpolated_pose(0.0f);
    CHECK(at0.position.x == doctest::Approx(a.position.x));
    CHECK(at0.yaw == doctest::Approx(a.yaw));
    CHECK(at0.pitch == doctest::Approx(a.pitch));

    const CameraPose at1 = cam.interpolated_pose(1.0f);
    CHECK(at1.position.z == doctest::Approx(b.position.z));
    CHECK(at1.yaw == doctest::Approx(b.yaw));
    CHECK(at1.pitch == doctest::Approx(b.pitch));

    const CameraPose mid = cam.interpolated_pose(0.5f);
    CHECK(mid.position.x == doctest::Approx(2.0f));
    CHECK(mid.yaw == doctest::Approx(0.5f));
}

TEST_CASE("yaw interpolates over the shortest arc across the +-pi seam") {
    FirstPersonCamera cam;
    // From just below +pi to just above -pi: the short way crosses the seam.
    const CameraPose a{{0.0f, 0.0f, 0.0f}, PI - 0.1f, 0.0f};
    const CameraPose b{{0.0f, 0.0f, 0.0f}, -PI + 0.1f, 0.0f};
    cam.set_poses(a, b);
    const CameraPose mid = cam.interpolated_pose(0.5f);
    // Midpoint is at the seam (+-pi), NOT at 0 (the long way).
    const float dist_to_seam = std::min(std::abs(mid.yaw - PI), std::abs(mid.yaw + PI));
    CHECK(dist_to_seam == doctest::Approx(0.0f).epsilon(1e-3));
}

TEST_CASE("pitch is clamped to CAMERA_PITCH_LIMIT") {
    const auto limit = static_cast<float>(dfn::config::CAMERA_PITCH_LIMIT);
    FirstPersonCamera cam;
    const CameraPose extreme{{0.0f, 0.0f, 0.0f}, 0.0f, 3.0f}; // beyond the limit
    cam.set_poses(extreme, extreme);
    CHECK(cam.interpolated_pose(1.0f).pitch == doctest::Approx(limit));
}

TEST_CASE("view matrix respects the frozen direction conventions") {
    FirstPersonCamera cam;
    const CameraPose pose{{10.0f, 5.0f, 20.0f}, 0.0f, 0.0f}; // yaw 0 -> -Z
    cam.set_poses(pose, pose);

    // Eye maps to the view-space origin.
    const glm::mat4 view = cam.view(1.0f);
    const glm::vec4 eye_vs = view * glm::vec4(pose.position, 1.0f);
    CHECK(eye_vs.x == doctest::Approx(0.0f));
    CHECK(eye_vs.y == doctest::Approx(0.0f));
    CHECK(eye_vs.z == doctest::Approx(0.0f));

    // A point one meter north (-Z) sits ahead (negative view-space z, RH).
    const glm::vec4 ahead = view * glm::vec4(10.0f, 5.0f, 19.0f, 1.0f);
    CHECK(ahead.z == doctest::Approx(-1.0f));

    // forward/right conventions: yaw 0 faces -Z, right is +X.
    CHECK(cam.forward(1.0f).z == doctest::Approx(-1.0f));
    CHECK(cam.right(1.0f).x == doctest::Approx(1.0f));

    // Positive yaw turns right (toward +X).
    const CameraPose east{{0.0f, 0.0f, 0.0f}, glm::half_pi<float>(), 0.0f};
    cam.set_poses(east, east);
    CHECK(cam.forward(1.0f).x == doctest::Approx(1.0f));
}

TEST_CASE("projection uses zero-to-one depth (RH_ZO)") {
    FirstPersonCamera cam;
    cam.set_projection(static_cast<float>(dfn::config::CAMERA_FOV_Y), 16.0f / 9.0f,
                       static_cast<float>(dfn::config::CAMERA_NEAR),
                       static_cast<float>(dfn::config::CAMERA_FAR));
    const glm::mat4 proj = cam.proj();

    const auto ndc_depth = [&](float z_view) {
        const glm::vec4 clip = proj * glm::vec4(0.0f, 0.0f, -z_view, 1.0f);
        return clip.z / clip.w;
    };
    CHECK(ndc_depth(static_cast<float>(dfn::config::CAMERA_NEAR))
          == doctest::Approx(0.0f).epsilon(1e-4)); // near -> 0 (ZO, not -1)
    CHECK(ndc_depth(static_cast<float>(dfn::config::CAMERA_FAR))
          == doctest::Approx(1.0f).epsilon(1e-4)); // far -> 1
}
