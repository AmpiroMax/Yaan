/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 00:42:03
Module: tests
File: tests/core/MathTests.cpp

Responsibility:
- Math suite: Aabb ops, ray intersections (box/sphere/triangle/plane),
  frustum classification.

Dependencies:
- Uses: doctest, dfn_core (math), glm.
- Used by: ctest (test_math).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — initial suite.
*/

#include "engine/core/math/sources/Aabb.h"
#include "engine/core/math/sources/Frustum.h"
#include "engine/core/math/sources/HeightField.h"
#include "engine/core/math/sources/Intersect.h"
#include "engine/core/math/sources/Ray.h"

#include <array>
#include <doctest/doctest.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace dfn::math;

TEST_CASE("aabb expand/contains/overlaps") {
    Aabb box; // inverted-empty
    CHECK_FALSE(box.valid());
    box.expand({1.0f, 2.0f, 3.0f});
    box.expand({-1.0f, 0.0f, 1.0f});
    CHECK(box.valid());
    CHECK(box.min == glm::vec3(-1.0f, 0.0f, 1.0f));
    CHECK(box.max == glm::vec3(1.0f, 2.0f, 3.0f));
    CHECK(box.contains({0.0f, 1.0f, 2.0f}));
    CHECK(box.contains(box.min)); // boundary counts
    CHECK_FALSE(box.contains({0.0f, 1.0f, 4.0f}));
    CHECK(box.center() == glm::vec3(0.0f, 1.0f, 2.0f));

    const Aabb other = Aabb::from_center_half_extents({2.0f, 1.0f, 2.0f}, {1.0f, 1.0f, 1.0f});
    CHECK(box.overlaps(other)); // touching at x = 1
    CHECK(aabb_vs_aabb(box, other));
    const Aabb far_box = Aabb::from_min_max({10.0f, 10.0f, 10.0f}, {11.0f, 11.0f, 11.0f});
    CHECK_FALSE(box.overlaps(far_box));
}

TEST_CASE("aabb transformed covers rotated corners") {
    const Aabb unit = Aabb::from_min_max({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f});
    // 45 degrees around Y: footprint grows to sqrt(2).
    const glm::mat4 rot =
        glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), {0.0f, 1.0f, 0.0f});
    const Aabb rotated = unit.transformed(rot);
    CHECK(rotated.max.x == doctest::Approx(std::sqrt(2.0f)).epsilon(0.001));
    CHECK(rotated.max.y == doctest::Approx(1.0f));
    // Pure translation shifts.
    const glm::mat4 move = glm::translate(glm::mat4(1.0f), {10.0f, 0.0f, 0.0f});
    CHECK(unit.transformed(move).min.x == doctest::Approx(9.0f));
}

TEST_CASE("ray vs aabb: hit, miss, inside") {
    const Aabb box = Aabb::from_min_max({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f});
    const Ray hit_ray = Ray::from_origin_dir({-5.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    const auto hit = ray_vs_aabb(hit_ray, box);
    REQUIRE(hit.has_value());
    CHECK(hit->t == doctest::Approx(4.0f));
    CHECK(hit->normal == glm::vec3(-1.0f, 0.0f, 0.0f));

    CHECK_FALSE(ray_vs_aabb(Ray::from_origin_dir({-5.0f, 3.0f, 0.0f}, {1.0f, 0.0f, 0.0f}), box)
                    .has_value());
    CHECK_FALSE(ray_vs_aabb(hit_ray, box, 3.0f).has_value()); // beyond max_t

    const auto inside = ray_vs_aabb(Ray::from_origin_dir({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}), box);
    REQUIRE(inside.has_value());
    CHECK(inside->t == doctest::Approx(0.0f));
}

TEST_CASE("ray vs sphere and plane") {
    const auto hit =
        ray_vs_sphere(Ray::from_origin_dir({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}),
                      {0.0f, 0.0f, 0.0f}, 1.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->t == doctest::Approx(4.0f));
    CHECK(hit->normal.z == doctest::Approx(1.0f));
    CHECK_FALSE(
        ray_vs_sphere(Ray::from_origin_dir({0.0f, 5.0f, 5.0f}, {0.0f, 0.0f, -1.0f}),
                      {0.0f, 0.0f, 0.0f}, 1.0f)
            .has_value());

    const Plane ground{{0.0f, 1.0f, 0.0f}, 0.0f};
    const auto phit =
        ray_vs_plane(Ray::from_origin_dir({0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f}), ground);
    REQUIRE(phit.has_value());
    CHECK(phit->t == doctest::Approx(3.0f));
    CHECK_FALSE(
        ray_vs_plane(Ray::from_origin_dir({0.0f, 3.0f, 0.0f}, {1.0f, 0.0f, 0.0f}), ground)
            .has_value()); // parallel
}

TEST_CASE("ray vs triangle: front, back, edge-miss") {
    const glm::vec3 a{-1.0f, 0.0f, -1.0f};
    const glm::vec3 b{1.0f, 0.0f, -1.0f};
    const glm::vec3 c{0.0f, 0.0f, 1.0f};
    const auto hit =
        ray_vs_triangle(Ray::from_origin_dir({0.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f}), a, b, c);
    REQUIRE(hit.has_value());
    CHECK(hit->t == doctest::Approx(2.0f));
    CHECK(hit->normal.y == doctest::Approx(1.0f)); // toward the origin side

    // From below: backface still reported, normal flipped toward origin.
    const auto back =
        ray_vs_triangle(Ray::from_origin_dir({0.0f, -2.0f, 0.0f}, {0.0f, 1.0f, 0.0f}), a, b, c);
    REQUIRE(back.has_value());
    CHECK(back->normal.y == doctest::Approx(-1.0f));

    CHECK_FALSE(
        ray_vs_triangle(Ray::from_origin_dir({5.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f}), a, b, c)
            .has_value());
}

TEST_CASE("frustum classification against a perspective camera") {
    // Camera at origin looking down -Z.
    const glm::mat4 view =
        glm::lookAt(glm::vec3(0.0f), {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f});
    const glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    const Frustum frustum = Frustum::from_view_proj(proj * view);

    const Aabb ahead = Aabb::from_center_half_extents({0.0f, 0.0f, -10.0f}, glm::vec3(1.0f));
    const Aabb behind = Aabb::from_center_half_extents({0.0f, 0.0f, 10.0f}, glm::vec3(1.0f));
    const Aabb beyond_far = Aabb::from_center_half_extents({0.0f, 0.0f, -200.0f}, glm::vec3(1.0f));
    CHECK(frustum.classify(ahead) == Containment::Inside);
    CHECK(frustum.classify(behind) == Containment::Outside);
    CHECK(frustum.classify(beyond_far) == Containment::Outside);
    CHECK(frustum.visible(ahead));
    CHECK_FALSE(frustum.visible(behind));

    // Box straddling the near plane region intersects.
    const Aabb straddle = Aabb::from_center_half_extents({0.0f, 0.0f, 0.0f}, glm::vec3(1.0f));
    CHECK(frustum.classify(straddle) == Containment::Intersects);

    CHECK(frustum.classify_sphere({0.0f, 0.0f, -10.0f}, 1.0f) == Containment::Inside);
    CHECK(frustum.classify_sphere({0.0f, 0.0f, 10.0f}, 1.0f) == Containment::Outside);
}

TEST_CASE("heightfield view decodes heights per the frozen formula") {
    std::array<uint16_t, 4> samples{0, 100, 200, 65535};
    HeightFieldView v;
    v.resolution = 2;
    v.heights = samples;
    v.height_scale = 0.001f;
    v.height_offset = 10.0f;
    CHECK(v.height_at(0, 0) == doctest::Approx(10.0f));
    CHECK(v.height_at(1, 0) == doctest::Approx(10.1f));
    CHECK(v.height_at(0, 1) == doctest::Approx(10.2f));
    CHECK(v.height_at(1, 1) == doctest::Approx(10.0f + 65.535f));
}
