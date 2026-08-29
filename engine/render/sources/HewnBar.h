/*
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

#pragma once

#include "engine/render/sources/PartsAtlas.h"
#include "engine/render/sources/ProcMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
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
    /// WHICH ATLAS TILES THIS MATERIAL WEARS. Default is untextured, and that
    /// path stays byte-for-byte what it was — it is the control arm.
    PartSkin skin{};
};

/// The per-face tone SPREAD, as a multiplier. Pulled out of hewn_tone so the
/// textured path can encode the same draw into the vertex blue channel
/// instead of into the albedo: one definition, one rng draw, no second copy
/// of the weathering law (Rule 39).
[[nodiscard]] inline float hewn_tone_scale(const HewnMaterial& mat, float wear,
                                           HewnRng& rng) {
    return 1.0f + rng.sym(mat.jitter * (0.35f + 0.65f * wear));
}

[[nodiscard]] inline uint32_t hewn_tone(const HewnMaterial& mat, float wear,
                                        HewnRng& rng) {
    // Wear darkens (weather greys wood down, it never brightens it) and widens
    // the spread, so a worn wall's boards differ from each other and a new
    // one's do not.
    const float dark = 1.0f - 0.22f * wear;
    const float j = hewn_tone_scale(mat, wear, rng);
    glm::vec3 c = mat.color * dark * j;
    c = glm::clamp(c, glm::vec3{0.02f}, glm::vec3{1.0f});
    return pack(c);
}

/// One textured triangle. The uvs are RAW TILE SPACE (metres / span) and the
/// tile index rides the colour — see PartsAtlas.h's mesh-side contract.
/// Flat-shaded exactly like ProcMesh's tri(): same winding, same degenerate
/// guard, same vertex-per-face duplication, with the uv filled in.
inline void hewn_skin_tri(MeshData& m, glm::vec3 a, glm::vec3 b, glm::vec3 c,
                          glm::vec2 ua, glm::vec2 ub, glm::vec2 uc,
                          uint32_t color) {
    const glm::vec3 cr = glm::cross(b - a, c - a);
    const float len = glm::length(cr);
    const glm::vec3 n = len > 1e-8f ? cr / len : glm::vec3{0.0f, 1.0f, 0.0f};
    const auto base = static_cast<uint32_t>(m.vertices.size());
    m.vertices.push_back({a, n, ua, color});
    m.vertices.push_back({b, n, ub, color});
    m.vertices.push_back({c, n, uc, color});
    m.indices.insert(m.indices.end(), {base, base + 1, base + 2});
}

/// One textured quad: TWO triangles with their own normals, like quad(). A
/// wobbled section ring makes a quad non-planar, and one shared normal across
/// both halves would shade a bent face as if it were flat.
inline void hewn_skin_quad(MeshData& m, glm::vec3 a, glm::vec3 b, glm::vec3 c,
                           glm::vec3 d, glm::vec2 ua, glm::vec2 ub, glm::vec2 uc,
                           glm::vec2 ud, uint32_t color) {
    hewn_skin_tri(m, a, b, c, ua, ub, uc, color);
    hewn_skin_tri(m, a, c, d, ua, uc, ud, color);
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

    // THE TEXTURE FRAME, and it costs nothing when the material is untextured.
    // `arc[i]` is metres of surface from the section's first vertex around to
    // vertex i, so u runs CONTINUOUSLY around the piece: the same fibre leaves
    // one face and arrives on the next in the right place, and a chamfer wears
    // the grain the two faces it cuts between share. v is metres along the
    // sweep, which is the piece's own axis — timber grain runs down a beam
    // because that is where the tree put it.
    const bool skinned = mat.skin.textured;
    const float span = mat.skin.span_m > 1e-3f ? mat.skin.span_m : 1.0f;
    std::vector<float> arc(static_cast<std::size_t>(n) + 1, 0.0f);
    if (skinned) {
        for (int i = 0; i < n; ++i) {
            const glm::vec2 a = sec[static_cast<std::size_t>(i)];
            const glm::vec2 b = sec[static_cast<std::size_t>((i + 1) % n)];
            const glm::vec2 d{(b.x - a.x) * hw, (b.y - a.y) * hh};
            arc[static_cast<std::size_t>(i) + 1] =
                arc[static_cast<std::size_t>(i)] + std::sqrt(d.x * d.x + d.y * d.y);
        }
    }

    for (int s = 0; s < segments; ++s) {
        const auto& p = rings[static_cast<std::size_t>(s)];
        const auto& q = rings[static_cast<std::size_t>(s) + 1];
        const float v0 = len * static_cast<float>(s) / static_cast<float>(segments) / span;
        const float v1 = len * static_cast<float>(s + 1) / static_cast<float>(segments) / span;
        for (int i = 0; i < n; ++i) {
            const int k = (i + 1) % n;
            if (skinned) {
                const float u0 = arc[static_cast<std::size_t>(i)] / span;
                const float u1 = arc[static_cast<std::size_t>(i) + 1] / span;
                const uint32_t col = parts_skin_color(mat.skin.side, mat.skin.side_tone,
                                                      hewn_tone_scale(mat, wear, rng));
                hewn_skin_quad(m, p[static_cast<std::size_t>(i)], p[static_cast<std::size_t>(k)],
                               q[static_cast<std::size_t>(k)], q[static_cast<std::size_t>(i)],
                               {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}, col);
            } else {
                quad(m, p[static_cast<std::size_t>(i)], p[static_cast<std::size_t>(k)],
                     q[static_cast<std::size_t>(k)], q[static_cast<std::size_t>(i)],
                     hewn_tone(mat, wear, rng));
            }
        }
    }
    const auto& first = rings.front();
    const auto& last = rings.back();
    if (skinned) {
        // THE CAPS WEAR THEIR OWN TILE, and on timber that tile is END GRAIN.
        // A log wall shows its cut ends at every corner tie, and side grain
        // drawn across an end is the one wood mistake everybody sees.
        const uint32_t col = parts_skin_color(mat.skin.end, mat.skin.end_tone,
                                              hewn_tone_scale(mat, wear, rng));
        for (int i = 0; i < n; ++i) {
            const int k = (i + 1) % n;
            const auto& a = sec[static_cast<std::size_t>(i)];
            const auto& b = sec[static_cast<std::size_t>(k)];
            // The cap is mapped in its OWN plane (section metres), centred on
            // the axis: an end face is a cut across the piece, not a
            // continuation of the surface that runs along it.
            const glm::vec2 ua{a.x * hw / span, a.y * hh / span};
            const glm::vec2 ub{b.x * hw / span, b.y * hh / span};
            const glm::vec2 uc{0.0f, 0.0f};
            hewn_skin_tri(m, centres.front(), first[static_cast<std::size_t>(k)],
                          first[static_cast<std::size_t>(i)], uc, ub, ua, col);
            hewn_skin_tri(m, centres.back(), last[static_cast<std::size_t>(i)],
                          last[static_cast<std::size_t>(k)], uc, ua, ub, col);
        }
    } else {
        const uint32_t cap = hewn_tone(mat, wear, rng);
        for (int i = 0; i < n; ++i) {
            const int k = (i + 1) % n;
            tri(m, centres.front(), first[static_cast<std::size_t>(k)],
                first[static_cast<std::size_t>(i)], cap);
            tri(m, centres.back(), last[static_cast<std::size_t>(i)],
                last[static_cast<std::size_t>(k)], cap);
        }
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
