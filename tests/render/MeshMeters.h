/*
Created: 17:08:2026 - 13:23:29
Last updated: 17:08:2026 - 13:23:29
Module: tests
File: tests/render/MeshMeters.h

Responsibility:
- Shared mesh meters for the part-forge suites: half-edge closedness (a
  sealed hull pairs every directed edge with its mate) and divergence-theorem
  signed volume (outward winding reads positive). One definition, three
  suites — a per-file copy is Rule 39's shadow chain in miniature.

Key items:
- meshtest::half_edge_defects() / signed_volume().

Dependencies:
- Uses: engine/render MeshData.
- Used by: PartForgeJointTests.cpp, PartForgeRoofTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- These are TEST instruments. Production code that needs closedness talks to
  engine/world's check_panel_solid or the shell instrument, not to this.
*/
/*
UPD:
- 17:08:2026 - 13:23:29: Вынос измерителей из PartForgeJointTests.cpp (второй
  потребитель — тесты крыш).
*/

#pragma once

#include "engine/render/sources/ProcMesh.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <utility>

namespace meshtest {

/// Quantized position key: parts are flat-shaded (vertices duplicated per
/// face), so edge identity must be POSITIONAL. 0.1 mm buckets — two distinct
/// kit vertices never sit closer than centimetres.
struct QPos {
    int64_t x, y, z;
    bool operator<(const QPos& o) const {
        if (x != o.x) return x < o.x;
        if (y != o.y) return y < o.y;
        return z < o.z;
    }
};

inline QPos qpos(const glm::vec3& p) {
    const auto q = [](float v) { return static_cast<int64_t>(std::llround(v * 10000.0f)); };
    return {q(p.x), q(p.y), q(p.z)};
}

/// A closed, consistently wound mesh pairs every directed half-edge (a->b)
/// with its mate (b->a). The return is the count of UNMATED half-edges:
/// 0 = sealed hull(s), anything else counts open boundary or a flipped face.
inline int half_edge_defects(const dfn::render::MeshData& m) {
    std::map<std::pair<QPos, QPos>, int> he;
    for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const QPos a = qpos(m.vertices[m.indices[i]].position);
        const QPos b = qpos(m.vertices[m.indices[i + 1]].position);
        const QPos c = qpos(m.vertices[m.indices[i + 2]].position);
        ++he[{a, b}];
        ++he[{b, c}];
        ++he[{c, a}];
    }
    int defects = 0;
    for (const auto& [edge, n] : he) {
        const auto mate = he.find({edge.second, edge.first});
        const int mated = mate == he.end() ? 0 : mate->second;
        if (n != mated) {
            defects += std::abs(n - mated);
        }
    }
    return defects;
}

/// Divergence-theorem volume. Positive iff the winding faces OUTWARD (CCW
/// from outside, the renderer's contract) for closed geometry.
inline double signed_volume(const dfn::render::MeshData& m) {
    double v = 0.0;
    for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const glm::vec3 a = m.vertices[m.indices[i]].position;
        const glm::vec3 b = m.vertices[m.indices[i + 1]].position;
        const glm::vec3 c = m.vertices[m.indices[i + 2]].position;
        v += static_cast<double>(glm::dot(a, glm::cross(b, c))) / 6.0;
    }
    return v;
}

} // namespace meshtest
