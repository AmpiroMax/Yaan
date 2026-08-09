/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 00:42:03
Module: engine/core/math
File: engine/core/math/sources/Frustum.cpp

Responsibility:
- Frustum extraction from a view-projection matrix (Gribb-Hartmann) and
  AABB/sphere classification.

Key items:
- Frustum::from_view_proj / classify / classify_sphere / visible.

Dependencies:
- Uses: Frustum.h, glm (matrix row access).
- Used by: dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Plane normals point INSIDE the frustum (header contract).
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — implementation.
*/

#include "engine/core/math/sources/Frustum.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_access.hpp>

namespace dfn::math {

float Plane::signed_distance(const glm::vec3& point) const {
    return glm::dot(normal, point) + d;
}

Frustum Frustum::from_view_proj(const glm::mat4& view_proj) {
    // Gribb-Hartmann: with rows r0..r3 of (proj * view), the six planes are
    // r3 +- ri (i = 0, 1, 2). Signs chosen so normals point inside.
    const glm::vec4 r0 = glm::row(view_proj, 0);
    const glm::vec4 r1 = glm::row(view_proj, 1);
    const glm::vec4 r2 = glm::row(view_proj, 2);
    const glm::vec4 r3 = glm::row(view_proj, 3);

    const glm::vec4 raw[PlaneIndex::COUNT] = {
        r3 + r0, // Left
        r3 - r0, // Right
        r3 + r1, // Bottom
        r3 - r1, // Top
        r3 + r2, // Near (GL-style clip; consistent for glm::perspective)
        r3 - r2, // Far
    };

    Frustum f;
    for (std::size_t i = 0; i < f.planes.size(); ++i) {
        const glm::vec3 n{raw[i].x, raw[i].y, raw[i].z};
        const float len = glm::length(n);
        f.planes[i] = Plane{n / len, raw[i].w / len};
    }
    return f;
}

Containment Frustum::classify(const Aabb& box) const {
    bool intersects = false;
    for (const Plane& p : planes) {
        // Positive vertex: the box corner farthest along the plane normal.
        const glm::vec3 positive{p.normal.x >= 0.0f ? box.max.x : box.min.x,
                                 p.normal.y >= 0.0f ? box.max.y : box.min.y,
                                 p.normal.z >= 0.0f ? box.max.z : box.min.z};
        if (p.signed_distance(positive) < 0.0f) {
            return Containment::Outside;
        }
        const glm::vec3 negative{p.normal.x >= 0.0f ? box.min.x : box.max.x,
                                 p.normal.y >= 0.0f ? box.min.y : box.max.y,
                                 p.normal.z >= 0.0f ? box.min.z : box.max.z};
        if (p.signed_distance(negative) < 0.0f) {
            intersects = true;
        }
    }
    return intersects ? Containment::Intersects : Containment::Inside;
}

Containment Frustum::classify_sphere(const glm::vec3& center, float radius) const {
    bool intersects = false;
    for (const Plane& p : planes) {
        const float dist = p.signed_distance(center);
        if (dist < -radius) {
            return Containment::Outside;
        }
        if (dist < radius) {
            intersects = true;
        }
    }
    return intersects ? Containment::Intersects : Containment::Inside;
}

bool Frustum::visible(const Aabb& box) const {
    return classify(box) != Containment::Outside;
}

} // namespace dfn::math
