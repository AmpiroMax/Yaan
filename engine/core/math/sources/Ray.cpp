/*
Module: engine/core/math
File: engine/core/math/sources/Ray.cpp

Responsibility:
- Ray factory functions (normalizing constructors).

Key items:
- Ray::from_origin_dir / from_to.

Dependencies:
- Uses: Ray.h, glm.
- Used by: dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/core/math/sources/Ray.h"

#include <glm/geometric.hpp>

namespace dfn::math {

Ray Ray::from_origin_dir(const glm::vec3& origin, const glm::vec3& dir) {
    return Ray{origin, glm::normalize(dir)};
}

Ray Ray::from_to(const glm::vec3& from, const glm::vec3& to) {
    return Ray{from, glm::normalize(to - from)};
}

} // namespace dfn::math
