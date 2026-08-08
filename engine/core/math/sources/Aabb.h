/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/core/math
File: engine/core/math/sources/Aabb.h

Responsibility:
- Axis-aligned bounding box in world or model space (meters). Thin extension of
  glm (Rule 2) — glm types are used directly, never wrapped.

Key items:
- Aabb: {min, max} box with center/extent/contains/expand helpers.

Dependencies:
- Uses: glm.
- Used by: math::Frustum culling, world chunk bounds, render LocalBounds mapping,
  physics broadphase queries.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Units are meters (Rule 14). Keep this a POD; heavy queries go to Intersect.h.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — POD AABB with trivial inline helpers.
*/

#pragma once

#include <glm/vec3.hpp>

namespace dfn::math {

/// Axis-aligned box, meters. Invariant: min <= max componentwise for a valid box;
/// a default-constructed Aabb is "inverted empty" so that expand() can grow it
/// from nothing.
struct Aabb {
    glm::vec3 min{1e30f, 1e30f, 1e30f};
    glm::vec3 max{-1e30f, -1e30f, -1e30f};

    [[nodiscard]] static constexpr Aabb from_min_max(const glm::vec3& mn, const glm::vec3& mx) {
        return Aabb{mn, mx};
    }

    /// Box from center and HALF extents.
    [[nodiscard]] static Aabb from_center_half_extents(const glm::vec3& center,
                                                       const glm::vec3& half_extents);

    [[nodiscard]] bool valid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    [[nodiscard]] glm::vec3 center() const { return (min + max) * 0.5f; }
    [[nodiscard]] glm::vec3 size() const { return max - min; }
    [[nodiscard]] glm::vec3 half_extents() const { return (max - min) * 0.5f; }

    /// Grows the box to contain `point` / `other`.
    void expand(const glm::vec3& point);
    void expand(const Aabb& other);

    /// True iff `point` is inside or on the boundary.
    [[nodiscard]] bool contains(const glm::vec3& point) const;

    /// True iff the boxes overlap (touching counts).
    [[nodiscard]] bool overlaps(const Aabb& other) const;

    /// This box transformed by an affine matrix (result is the AABB of the
    /// transformed corners). Declared here, implemented in stage 2.
    [[nodiscard]] Aabb transformed(const glm::mat4& transform) const;
};

} // namespace dfn::math
