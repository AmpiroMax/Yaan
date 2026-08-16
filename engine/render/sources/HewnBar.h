/*
Created: 16:08:2026 - 22:28:22
Last updated: 16:08:2026 - 22:28:22
Module: engine/render
File: engine/render/sources/HewnBar.h

Responsibility:
- THE ONE BUILDER, shared: the hewn-bar mesher (a closed chamfered prism swept
  along an axis), its Material description, its deterministic Rng and its tone
  jitter — moved VERBATIM out of PartForge.cpp the day a second forge
  (PropForge, furniture) needed the same bars. One function called by both,
  never two copies (Rule 39: a shadow copy of a chain is a latent defect).

Key items:
- HewnMaterial / hewn_tone(): colour + wear jitter, packed for the mesh.
- HewnRng: deterministic xorshift; same params, same bytes, on every machine.
- hewn_bar() / hewn_block(): the closed prism and its axis-aligned shorthand.

Dependencies:
- Uses: ProcMesh.h (MeshData, tri/quad/pack), glm.
- Used by: PartForge.cpp (building kit), PropForge.cpp (furniture).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 52 lives HERE: every bar is capped at both ends — a volume, not a tube.
- PURE AND DETERMINISTIC. No std::mt19937, no wall clock, no state outside
  the caller's HewnRng.
*/
/*
UPD:
- 16:08:2026 - 22:28:22: Вынос из PartForge.cpp без изменения геометрии: имена
  получили префикс Hewn (Material/Rng/tone в заголовке движка без префикса —
  ловушка для каждого включившего), тела перенесены дословно.
*/

#pragma once

#include "engine/render/sources/ProcMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <vector>

namespace dfn::render {

/// Deterministic, cheap, and NOT std::mt19937: the same params must give the
/// same bytes on every machine that ever rebuilds a kit (Rule 13.1's spirit
/// applied to objects).
struct HewnRng {
    uint64_t s;
    explicit HewnRng(uint64_t seed)
        : s(seed * 6364136223846793005ull + 1442695040888963407ull) {}
    float unit() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return static_cast<float>((s >> 40) & 0xFFFFFF) / 16777216.0f;
    }
    /// Symmetric around zero: the wobble a timber gets must be as likely to
    /// bend one way as the other, or a wall of studs leans.
    float sym(float amount) { return (unit() * 2.0f - 1.0f) * amount; }
};

struct HewnMaterial {
    glm::vec3 color;
    float jitter;   ///< per-face tone spread at wear = 1
    float chamfer;  ///< fraction of the half-section cut off the corners
    float wobble;   ///< how far a section ring wanders at wear = 1, metres
};

[[nodiscard]] inline uint32_t hewn_tone(const HewnMaterial& mat, float wear,
                                        HewnRng& rng) {
    // Wear darkens (weather greys wood down, it never brightens it) and widens
    // the spread, so a worn wall's boards differ from each other and a new
    // one's do not.
    const float dark = 1.0f - 0.22f * wear;
    const float j = 1.0f + rng.sym(mat.jitter * (0.35f + 0.65f * wear));
    glm::vec3 c = mat.color * dark * j;
    c = glm::clamp(c, glm::vec3{0.02f}, glm::vec3{1.0f});
    return pack(c);
}

[[nodiscard]] inline glm::vec3 hewn_perp(const glm::vec3& axis) {
    const glm::vec3 ref = std::fabs(axis.y) > 0.9f ? glm::vec3{1.0f, 0.0f, 0.0f}
                                                   : glm::vec3{0.0f, 1.0f, 0.0f};
    return glm::normalize(glm::cross(ref, axis));
}

/// THE ONE BUILDER. A closed chamfered prism of half-width `hw` (across `side`)
/// and half-height `hh` (across `up`), swept `len` metres along `along` from
/// `origin`, which sits at the CENTRE of the starting face. Split into
/// `segments` so wear can bend it; capped at both ends so it is a volume and
/// not a tube (Rule 52).
inline void hewn_bar(MeshData& m, glm::vec3 origin, glm::vec3 along, glm::vec3 up,
                     float len, float hw, float hh, const HewnMaterial& mat,
                     float wear, HewnRng& rng, int segments = 2,
                     float taper = 1.0f) {
    constexpr float TAU = 6.28318530718f;
    along = glm::normalize(along);
    glm::vec3 side = glm::cross(up, along);
    if (glm::length(side) < 1e-4f) {
        side = hewn_perp(along);
    }
    side = glm::normalize(side);
    up = glm::normalize(glm::cross(along, side));

    const float c = mat.chamfer;
    // Section as (u, v) fractions of (hw, hh). Eight points when chamfered,
    // four when not: a zero chamfer through the eight-point path would emit
    // four degenerate quads whose normals are undefined.
    std::vector<glm::vec2> sec;
    if (c > 0.01f) {
        sec = {{-1.0f + c, -1.0f}, {1.0f - c, -1.0f}, {1.0f, -1.0f + c}, {1.0f, 1.0f - c},
               {1.0f - c, 1.0f},   {-1.0f + c, 1.0f}, {-1.0f, 1.0f - c}, {-1.0f, -1.0f + c}};
    } else {
        sec = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
    }
    const int n = static_cast<int>(sec.size());
    segments = std::max(1, segments);

    const float wob = mat.wobble * wear;
    std::vector<std::vector<glm::vec3>> rings(static_cast<std::size_t>(segments) + 1);
    std::vector<glm::vec3> centres(static_cast<std::size_t>(segments) + 1);
    for (int s = 0; s <= segments; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(segments);
        // Ends stay put: two bars butted end to end must still meet, so the
        // wobble is zero at t=0 and t=1 and largest in the middle.
        const float bend = std::sin(t * TAU * 0.5f);
        const glm::vec3 centre = origin + along * (len * t)
                               + side * (rng.sym(wob) * bend)
                               + up * (rng.sym(wob) * bend);
        centres[static_cast<std::size_t>(s)] = centre;
        const float scale = 1.0f + (taper - 1.0f) * t;
        auto& ring = rings[static_cast<std::size_t>(s)];
        ring.resize(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            ring[static_cast<std::size_t>(i)] =
                centre + side * (sec[static_cast<std::size_t>(i)].x * hw * scale)
                       + up * (sec[static_cast<std::size_t>(i)].y * hh * scale);
        }
    }

    for (int s = 0; s < segments; ++s) {
        const auto& p = rings[static_cast<std::size_t>(s)];
        const auto& q = rings[static_cast<std::size_t>(s) + 1];
        for (int i = 0; i < n; ++i) {
            const int k = (i + 1) % n;
            quad(m, p[static_cast<std::size_t>(i)], p[static_cast<std::size_t>(k)],
                 q[static_cast<std::size_t>(k)], q[static_cast<std::size_t>(i)],
                 hewn_tone(mat, wear, rng));
        }
    }
    const auto& first = rings.front();
    const auto& last = rings.back();
    const uint32_t cap = hewn_tone(mat, wear, rng);
    for (int i = 0; i < n; ++i) {
        const int k = (i + 1) % n;
        tri(m, centres.front(), first[static_cast<std::size_t>(k)],
            first[static_cast<std::size_t>(i)], cap);
        tri(m, centres.back(), last[static_cast<std::size_t>(i)],
            last[static_cast<std::size_t>(k)], cap);
    }
}

/// An axis-aligned block from its min corner. Just the bar with no chamfer and
/// no bend, named for what a composer thinks he is placing.
inline void hewn_block(MeshData& m, glm::vec3 min, glm::vec3 size,
                       const HewnMaterial& mat, float wear, HewnRng& rng,
                       int segments = 1) {
    hewn_bar(m, min + glm::vec3{size.x * 0.5f, size.y * 0.5f, 0.0f},
             {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, size.z, size.x * 0.5f,
             size.y * 0.5f, mat, wear, rng, segments);
}

} // namespace dfn::render
