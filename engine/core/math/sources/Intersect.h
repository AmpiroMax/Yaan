/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/core/math
File: engine/core/math/sources/Intersect.h

Responsibility:
- Free intersection tests between the core math shapes (Ray, Aabb, sphere,
  triangle, plane). Pure functions, no state.

Key items:
- RayHit: hit result (t along the ray).
- ray_vs_aabb / ray_vs_sphere / ray_vs_triangle / ray_vs_plane / aabb_vs_aabb.

Dependencies:
- Uses: glm, Aabb, Ray, Frustum (Plane).
- Used by: gameplay interaction/picking, render culling helpers, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions only; add new shapes here rather than as methods, so shape
  headers stay POD-small.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — ray/box/sphere/triangle/plane tests.
*/

#pragma once

#include "engine/core/math/sources/Aabb.h"
#include "engine/core/math/sources/Frustum.h"
#include "engine/core/math/sources/Ray.h"

#include <optional>

namespace dfn::math {

/// Ray intersection result. `t` is meters along the ray (>= 0); `point` is the
/// hit position; `normal` is the surface normal at the hit (unit, facing the ray
/// origin's side).
struct RayHit {
    float t = 0.0f;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
};

/// Nearest hit of `ray` against `box` within [0, max_t], nullopt on miss.
/// A ray starting inside the box hits at t = 0 with the normal of the exit face
/// flipped toward the origin.
[[nodiscard]] std::optional<RayHit> ray_vs_aabb(const Ray& ray, const Aabb& box,
                                                float max_t = 1e30f);

/// Nearest hit against a sphere (center, radius in meters).
[[nodiscard]] std::optional<RayHit> ray_vs_sphere(const Ray& ray, const glm::vec3& center,
                                                  float radius, float max_t = 1e30f);

/// Hit against triangle (a, b, c), counter-clockwise front face (Moeller-Trumbore).
/// Backface hits are reported too; check the normal if you need to reject them.
[[nodiscard]] std::optional<RayHit> ray_vs_triangle(const Ray& ray, const glm::vec3& a,
                                                    const glm::vec3& b, const glm::vec3& c,
                                                    float max_t = 1e30f);

/// Hit against an infinite plane; nullopt when parallel or behind the origin.
[[nodiscard]] std::optional<RayHit> ray_vs_plane(const Ray& ray, const Plane& plane,
                                                 float max_t = 1e30f);

/// Overlap test, touching counts. Same as Aabb::overlaps; here for symmetry.
[[nodiscard]] bool aabb_vs_aabb(const Aabb& a, const Aabb& b);

} // namespace dfn::math
