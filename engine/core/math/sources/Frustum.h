/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/core/math
File: engine/core/math/sources/Frustum.h

Responsibility:
- View frustum as six planes, extracted from a view-projection matrix, for
  CPU-side culling (chunk meshes, entities).

Key items:
- Plane: normal + signed distance.
- Frustum: six planes + intersection classification against AABB/sphere.

Dependencies:
- Uses: glm, Aabb.
- Used by: engine/render culling, world streaming heuristics, editor.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Convention: plane normals point INSIDE the frustum; a point is inside when
  signed distance >= 0 for all six planes.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — plane-based frustum with AABB/sphere
  tests.
*/

#pragma once

#include "engine/core/math/sources/Aabb.h"

#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace dfn::math {

/// Plane in constant-normal form: dot(normal, p) + d == 0. `normal` is unit.
struct Plane {
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float d = 0.0f;

    /// Signed distance of `point` to the plane (positive on the normal side).
    [[nodiscard]] float signed_distance(const glm::vec3& point) const;
};

/// Result of a frustum classification. `Intersects` means partially inside;
/// callers that only need a boolean treat Intersects as visible.
enum class Containment : uint8_t {
    Outside,
    Intersects,
    Inside,
};

/// Six-plane view frustum. Normals point inside.
struct Frustum {
    enum PlaneIndex : uint8_t { Left = 0, Right, Bottom, Top, Near, Far, COUNT };

    std::array<Plane, PlaneIndex::COUNT> planes;

    /// Extracts the frustum from `proj * view` (Gribb-Hartmann). Works for any
    /// projection produced by glm; planes are normalized.
    [[nodiscard]] static Frustum from_view_proj(const glm::mat4& view_proj);

    /// Conservative AABB test: never returns Outside for a visible box; may
    /// return Intersects for a barely-hidden one (fine for culling).
    [[nodiscard]] Containment classify(const Aabb& box) const;
    [[nodiscard]] Containment classify_sphere(const glm::vec3& center, float radius) const;

    /// Boolean convenience: classify(box) != Outside.
    [[nodiscard]] bool visible(const Aabb& box) const;
};

} // namespace dfn::math
