/*
Created: 28:08:2026 - 16:35:00
Last updated: 28:08:2026 - 16:35:00
Module: engine/render
File: engine/render/sources/TreeBark.cpp

Responsibility:
- pack_wind()/bark_tube(): the wood primitives both tree forges share
  (see TreeBark.h for the contract and the move's licence).

Dependencies:
- Uses: TreeBark.h, FloraBuild.h (safe_normalize, perp_of, TAU), ProcMesh.h.
- Used by: TreeForge.cpp, TreeForgeV2.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- MOVED, NOT REWRITTEN (28.08). Byte-for-byte the code that stood in
  TreeForge.cpp's anonymous namespace; every shelf .dfo re-baked to the same
  content_hash across the move. Change nothing here without repeating that
  comparison — the whole first iteration hangs off this mapping.
*/
/*
UPD:
- 28:08:2026 - 16:35:00: Создан — тело pack_wind/bark_tube перенесено из
  TreeForge.cpp без единой правки; сверка хэшей полки до/после — без изменений.
*/

#include "engine/render/sources/TreeBark.h"

#include "engine/render/sources/FloraBuild.h"
#include "engine/render/sources/ProcMesh.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/geometric.hpp>

namespace dfn::render {

/// Vertex colour for textured wood on the foliage program: r = sway weight
/// (honest distance-from-support — the lead's 09f75eb shader derives all
/// three wind bands from this one weight), g = per-tree phase, b (value
/// jitter) = 0.5 neutral, a (sky vis) = 0.55.
[[nodiscard]] uint32_t pack_wind(float sway, float phase) {
    const auto to_byte = [](float v) {
        return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    // 0xAABBGGRR: a=0x8C (sky vis 0.55), b=0x80 (jitter 0.5).
    return 0x8C800000u | (to_byte(phase) << 8) | to_byte(sway);
}

/// One tapered tube segment with BARK UVs. Same geometry as tube_segment, but
/// each face maps into the given atlas tile rect: u runs around the
/// circumference, v along the segment's own length, both through a TRIANGLE
/// WAVE so the mapping mirror-repeats inside the tile and never crosses its
/// border into a neighbouring leaf tile (the atlas cannot wrap). The tile is
/// drawn mirror-symmetric, so the fold line is invisible by construction.
/// wind_c0 colours the p0 ring, wind_c1 the p1 ring (and the tip vertex):
/// the sway weight must be CONTINUOUS along a limb, or adjacent segments
/// translate by different amounts under wind and the joint cracks open.
/// u_hint PARALLEL-TRANSPORTS the texture frame along a limb (user, gallery
/// 21:18: «текстуры не прямые, везде по-разному идут; дерево растёт в одном
/// направлении, кора соответственно»): pass the previous segment's frame and
/// the furrows run straight down the limb instead of twisting at every joint
/// (perp_of() alone picks an arbitrary frame per segment).
void bark_tube(MeshData& m, glm::vec3 p0, glm::vec3 p1, glm::vec3 axis, float r0,
               float r1, int sides, glm::vec4 uv_rect, float v0_m, float circum_m,
               uint32_t wind_c0, uint32_t wind_c1, glm::vec3* u_hint) {
    glm::vec3 u_axis;
    if (u_hint != nullptr && glm::length(*u_hint) > 1e-4f) {
        const glm::vec3 proj = *u_hint - axis * glm::dot(*u_hint, axis);
        u_axis = safe_normalize(proj, perp_of(axis));
        *u_hint = u_axis; // hand the transported frame back to the caller
    } else {
        u_axis = perp_of(axis);
    }
    const glm::vec3 v_axis = glm::cross(axis, u_axis);
    const float len = glm::length(p1 - p0);
    // PLAIN WRAP, not mirror: the bark tile is torus-periodic since the
    // FloraCards v2 field (mirror-repeat made every ridge a kaleidoscope
    // pair, which is what the user's «прямоугольнички» frame was showing).
    const auto tri_wave = [](float t) { return t - std::floor(t); };
    // Metres of trunk surface one full tile covers. ~2.6 m keeps the furrow
    // pitch believable on a 0.4 m oak and a 10 m colossus alike.
    constexpr float TILE_SPAN_M = 2.6f;
    const float du = uv_rect.z - uv_rect.x;
    const float dv = uv_rect.w - uv_rect.y;
    // EACH RING WEARS ITS OWN CIRCUMFERENCE (user, gallery 20:42: «кора везде
    // разная, где-то вытянута, где-то сжата»): a taper mapped with the butt
    // ring's girth stretches the tile toward the thin end, and the stretch
    // jumps at every joint. circum_m is the BUTT ring's girth; the tip ring
    // scales it by r1/r0, which is continuous across joints by construction.
    const float circum1_m = circum_m * ((r0 > 1e-5f) ? (r1 / r0) : 1.0f);
    // A vertex-folded frac CANNOT survive a tile boundary crossing INSIDE a
    // face: the face between u=0.76 and u=frac(1.52)=0.52 renders mirrored
    // and compressed, and on any trunk thicker than ~0.8 m (girth > tile)
    // every other face crosses — which is exactly the herringbone the 50 m
    // oak wore (user, 17.08: «ёлочка» on thick boles). The cure is to SPLIT
    // each face at every tile boundary in u and v, so every emitted cell maps
    // one monotonic in-tile span. Thin branches (girth and segment under one
    // tile) emit the same single quad they always did.
    const auto lerp_wind = [](uint32_t c0, uint32_t c1, float t) {
        const auto mix = [&](int shift) {
            const float a = static_cast<float>((c0 >> shift) & 0xFFu);
            const float b = static_cast<float>((c1 >> shift) & 0xFFu);
            return static_cast<uint32_t>(a + (b - a) * t + 0.5f) & 0xFFu;
        };
        return (c0 & 0xFF000000u) | (mix(16) << 16) | (mix(8) << 8) | mix(0);
    };
    const auto breaks_of = [](float b0, float b1, std::vector<float>& out) {
        out.clear();
        out.push_back(b0);
        for (float k = std::floor(b0) + 1.0f; k < b1; k += 1.0f) {
            if (k > b0) out.push_back(k);
        }
        out.push_back(b1);
    };
    std::vector<float> ub, vb;
    for (int i = 0; i < sides; ++i) {
        const float a0 = TAU * static_cast<float>(i) / static_cast<float>(sides);
        const float a1 = TAU * static_cast<float>(i + 1) / static_cast<float>(sides);
        const glm::vec3 d0 = u_axis * std::cos(a0) + v_axis * std::sin(a0);
        const glm::vec3 d1 = u_axis * std::cos(a1) + v_axis * std::sin(a1);
        const float dr = r0 - r1;
        const float slope = (len > 1e-5f) ? (dr / len) : 0.0f;
        const glm::vec3 n0 = safe_normalize(d0 + axis * slope, d0);
        const glm::vec3 n1 = safe_normalize(d1 + axis * slope, d1);
        const float s0 = static_cast<float>(i) / static_cast<float>(sides);
        const float s1 = static_cast<float>(i + 1) / static_cast<float>(sides);
        // Tile-space spans of this face (butt girth rules the split lines;
        // the taper's top-ring scale stays as a shear WITHIN cells, which is
        // the same joint-continuity contract as before).
        const float U0 = circum_m * s0 / TILE_SPAN_M;
        const float U1 = circum_m * s1 / TILE_SPAN_M;
        const float V0 = v0_m / TILE_SPAN_M;
        const float V1 = (v0_m + len) / TILE_SPAN_M;
        breaks_of(U0, U1, ub);
        breaks_of(V0, V1, vb);
        // Corner positions of the whole face; cells interpolate bilinearly,
        // which keeps every cell exactly in the original face plane.
        const glm::vec3 c00 = p0 + d0 * r0;             // (u0, v0)
        const glm::vec3 c10 = p0 + d1 * r0;             // (u1, v0)
        const glm::vec3 c01 = (r1 <= 1e-4f) ? p1 : p1 + d0 * r1;
        const glm::vec3 c11 = (r1 <= 1e-4f) ? p1 : p1 + d1 * r1;
        const float uspan = std::max(U1 - U0, 1e-6f);
        const float vspan = std::max(V1 - V0, 1e-6f);
        for (size_t vi = 0; vi + 1 < vb.size(); ++vi) {
            for (size_t uix = 0; uix + 1 < ub.size(); ++uix) {
                const float fa = (ub[uix] - U0) / uspan;
                const float fb = (ub[uix + 1] - U0) / uspan;
                const float ta = (vb[vi] - V0) / vspan;
                const float tb = (vb[vi + 1] - V0) / vspan;
                // In-tile uv: monotonic inside the cell by construction.
                const float ubase = std::floor(ub[uix]);
                const float vbase = std::floor(vb[vi]);
                const float va_t = vb[vi] - vbase, vb_t = vb[vi + 1] - vbase;
                // The taper's per-ring girth: scale the top edge's u toward
                // circum1 inside the cell (shear, never a fold).
                const float taper = (circum_m > 1e-5f) ? circum1_m / circum_m
                                                       : 1.0f;
                const auto uv_at = [&](float f, float t) {
                    // Absolute tile-space u, the top ring sheared toward the
                    // taper's girth; the cell's own tile origin subtracted.
                    const float scale = 1.0f + (taper - 1.0f) * t;
                    const float u_in = std::clamp(
                        (U0 + uspan * f) * scale - ubase, 0.0f, 1.0f);
                    const float v_in = std::clamp(
                        va_t + (vb_t - va_t)
                            * ((t - ta) / std::max(tb - ta, 1e-6f)),
                        0.0f, 1.0f);
                    return glm::vec2{uv_rect.x + du * u_in, uv_rect.y + dv * v_in};
                };
                const auto pos_at = [&](float f, float t) {
                    const glm::vec3 bot = c00 + (c10 - c00) * f;
                    const glm::vec3 top = c01 + (c11 - c01) * f;
                    return bot + (top - bot) * t;
                };
                const auto n_at = [&](float f) {
                    return safe_normalize(n0 + (n1 - n0) * f, n0);
                };
                const auto w_at = [&](float t) {
                    return lerp_wind(wind_c0, wind_c1, t);
                };
                const auto base = static_cast<uint32_t>(m.vertices.size());
                m.vertices.push_back({pos_at(fa, ta), n_at(fa), uv_at(fa, ta), w_at(ta)});
                m.vertices.push_back({pos_at(fa, tb), n_at(fa), uv_at(fa, tb), w_at(tb)});
                m.vertices.push_back({pos_at(fb, tb), n_at(fb), uv_at(fb, tb), w_at(tb)});
                m.vertices.push_back({pos_at(fb, ta), n_at(fb), uv_at(fb, ta), w_at(ta)});
                m.indices.insert(m.indices.end(),
                                 {base, base + 1, base + 2, base, base + 2, base + 3});
            }
        }
    }
    (void)tri_wave;
}



} // namespace dfn::render
