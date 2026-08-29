/*
Module: engine/core/math
File: engine/core/math/sources/Aabb.cpp

Responsibility:
- Aabb non-trivial operations: expansion, containment, overlap, affine transform.

Key items:
- Aabb::expand / contains / overlaps / transformed / from_center_half_extents.

Dependencies:
- Uses: Aabb.h, glm.
- Used by: dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/core/math/sources/Aabb.h"

#include <glm/common.hpp>
#include <glm/mat4x4.hpp>

namespace dfn::math {

Aabb Aabb::from_center_half_extents(const glm::vec3& center, const glm::vec3& half_extents) {
    return Aabb{center - half_extents, center + half_extents};
}

void Aabb::expand(const glm::vec3& point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
}

void Aabb::expand(const Aabb& other) {
    if (!other.valid()) {
        return;
    }
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
}

bool Aabb::contains(const glm::vec3& p) const {
    return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z
        && p.z <= max.z;
}

bool Aabb::overlaps(const Aabb& o) const {
    return min.x <= o.max.x && max.x >= o.min.x && min.y <= o.max.y && max.y >= o.min.y
        && min.z <= o.max.z && max.z >= o.min.z;
}

Aabb Aabb::transformed(const glm::mat4& transform) const {
    Aabb result; // inverted-empty; expand() grows it over the 8 corners
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 corner{(i & 1) != 0 ? max.x : min.x,
                               (i & 2) != 0 ? max.y : min.y,
                               (i & 4) != 0 ? max.z : min.z};
        result.expand(glm::vec3(transform * glm::vec4(corner, 1.0f)));
    }
    return result;
}

} // namespace dfn::math
