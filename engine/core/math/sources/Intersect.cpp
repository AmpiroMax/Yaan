/*
Module: engine/core/math
File: engine/core/math/sources/Intersect.cpp

Responsibility:
- Intersection tests: ray vs AABB (slab), sphere, triangle (Moeller-Trumbore),
  plane; AABB vs AABB.

Key items:
- ray_vs_aabb / ray_vs_sphere / ray_vs_triangle / ray_vs_plane / aabb_vs_aabb.

Dependencies:
- Uses: Intersect.h, glm.
- Used by: dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Semantics documented in Intersect.h (inside-box t=0, backface triangle hits
  reported) are contract; keep tests in sync when touching edge cases.
*/

#include "engine/core/math/sources/Intersect.h"

#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::math {

namespace {
constexpr float EPSILON = 1e-8f;
} // namespace

std::optional<RayHit> ray_vs_aabb(const Ray& ray, const Aabb& box, float max_t) {
    float t_enter = 0.0f;
    float t_exit = max_t;
    int enter_axis = -1;

    for (int axis = 0; axis < 3; ++axis) {
        const float o = ray.origin[axis];
        const float d = ray.direction[axis];
        const float mn = box.min[axis];
        const float mx = box.max[axis];
        if (std::fabs(d) < EPSILON) {
            if (o < mn || o > mx) {
                return std::nullopt; // parallel and outside the slab
            }
            continue;
        }
        float t0 = (mn - o) / d;
        float t1 = (mx - o) / d;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        if (t0 > t_enter) {
            t_enter = t0;
            enter_axis = axis;
        }
        t_exit = t1 < t_exit ? t1 : t_exit;
        if (t_enter > t_exit) {
            return std::nullopt;
        }
    }

    RayHit hit;
    hit.t = t_enter;
    hit.point = ray.point_at(hit.t);
    if (enter_axis < 0) {
        // Origin inside the box (or degenerate): t = 0, normal faces back
        // along the ray so it points toward the origin's side.
        hit.normal = -ray.direction;
    } else {
        glm::vec3 n{0.0f};
        n[enter_axis] = ray.direction[enter_axis] > 0.0f ? -1.0f : 1.0f;
        hit.normal = n;
    }
    return hit;
}

std::optional<RayHit> ray_vs_sphere(const Ray& ray, const glm::vec3& center, float radius,
                                    float max_t) {
    const glm::vec3 oc = ray.origin - center;
    const float b = glm::dot(oc, ray.direction);
    const float c = glm::dot(oc, oc) - radius * radius;
    const float disc = b * b - c;
    if (disc < 0.0f) {
        return std::nullopt;
    }
    const float sq = std::sqrt(disc);
    float t = -b - sq;
    if (t < 0.0f) {
        t = -b + sq; // origin inside: exit point
    }
    if (t < 0.0f || t > max_t) {
        return std::nullopt;
    }
    RayHit hit;
    hit.t = t;
    hit.point = ray.point_at(t);
    const glm::vec3 outward = (hit.point - center) / radius;
    hit.normal = glm::dot(outward, ray.direction) > 0.0f ? -outward : outward;
    return hit;
}

std::optional<RayHit> ray_vs_triangle(const Ray& ray, const glm::vec3& a, const glm::vec3& b,
                                      const glm::vec3& c, float max_t) {
    // Moeller-Trumbore.
    const glm::vec3 ab = b - a;
    const glm::vec3 ac = c - a;
    const glm::vec3 pvec = glm::cross(ray.direction, ac);
    const float det = glm::dot(ab, pvec);
    if (std::fabs(det) < EPSILON) {
        return std::nullopt; // parallel to the triangle plane
    }
    const float inv_det = 1.0f / det;
    const glm::vec3 tvec = ray.origin - a;
    const float u = glm::dot(tvec, pvec) * inv_det;
    if (u < 0.0f || u > 1.0f) {
        return std::nullopt;
    }
    const glm::vec3 qvec = glm::cross(tvec, ab);
    const float v = glm::dot(ray.direction, qvec) * inv_det;
    if (v < 0.0f || u + v > 1.0f) {
        return std::nullopt;
    }
    const float t = glm::dot(ac, qvec) * inv_det;
    if (t < 0.0f || t > max_t) {
        return std::nullopt;
    }
    RayHit hit;
    hit.t = t;
    hit.point = ray.point_at(t);
    const glm::vec3 face_normal = glm::normalize(glm::cross(ab, ac));
    hit.normal = det < 0.0f ? -face_normal : face_normal; // toward the origin side
    return hit;
}

std::optional<RayHit> ray_vs_plane(const Ray& ray, const Plane& plane, float max_t) {
    const float denom = glm::dot(plane.normal, ray.direction);
    if (std::fabs(denom) < EPSILON) {
        return std::nullopt; // parallel
    }
    const float t = -plane.signed_distance(ray.origin) / denom;
    if (t < 0.0f || t > max_t) {
        return std::nullopt;
    }
    RayHit hit;
    hit.t = t;
    hit.point = ray.point_at(t);
    hit.normal = denom < 0.0f ? plane.normal : -plane.normal;
    return hit;
}

bool aabb_vs_aabb(const Aabb& a, const Aabb& b) { return a.overlaps(b); }

} // namespace dfn::math
