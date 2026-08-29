/*
Module: engine/core/math
File: engine/core/math/sources/Ray.h

Responsibility:
- Ray in world space (meters): origin + unit direction, for picking, interaction
  reach checks, and line-of-sight queries.

Key items:
- Ray: {origin, direction} with point_at helper.

Dependencies:
- Uses: glm.
- Used by: Intersect.h tests, render picking, gameplay interaction system.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- `direction` is unit length by contract; constructors/factories normalize.
*/

#pragma once

#include <glm/vec3.hpp>

namespace dfn::math {

/// World-space ray. `direction` is unit length by contract — factory functions
/// normalize; if you fill fields manually, keep the invariant.
struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};

    /// Builds a ray, normalizing `dir`. Precondition: dir != 0.
    [[nodiscard]] static Ray from_origin_dir(const glm::vec3& origin, const glm::vec3& dir);

    /// Builds the ray through two points. Precondition: from != to.
    [[nodiscard]] static Ray from_to(const glm::vec3& from, const glm::vec3& to);

    /// Point at parameter t (meters along the ray).
    [[nodiscard]] glm::vec3 point_at(float t) const { return origin + direction * t; }
};

} // namespace dfn::math
