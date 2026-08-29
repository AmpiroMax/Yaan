/*
Module: engine/render
File: engine/render/sources/PathMesher.cpp

Responsibility:
- Path ribbon construction: the tread frame per station, the cross-section
  knots, class-run splitting, arc length, bounds.

Key items:
- build_path_pieces().

Dependencies:
- Uses: PathMesher.h, engine/core/math (path_wear_profile), glm.
- Used by: dfn_render; tests/render/PathMesherTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure and deterministic: no GPU, no ECS, no clock, no RNG.
- Never re-derive the wear curve here or in the shader — call core's
  math::path_wear_profile. See the header.
*/

#include "engine/render/sources/PathMesher.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::render {

namespace {

// The tread's plan tangent at station `i` of [first, last]. Central difference
// inside, one-sided at the ends. Falls back to +X only for a degenerate route
// (every station at one point), which build_path_pieces refuses anyway.
glm::vec2 plan_tangent(std::span<const math::PathStation> s, std::size_t i,
                       std::size_t first, std::size_t last) {
    const std::size_t a = (i > first) ? i - 1 : i;
    const std::size_t b = (i < last) ? i + 1 : i;
    glm::vec2 t = s[b].position - s[a].position;
    const float len = std::sqrt(t.x * t.x + t.y * t.y);
    if (len < 1e-6f) {
        return {1.0f, 0.0f};
    }
    return t / len;
}

uint32_t pack_path_color(float wear, uint8_t path_class) {
    const auto to8 = [](float v) {
        return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    // g encodes the atlas cell: (ordinal + 0.5) / 4 lands in the middle of the
    // cell's quarter, so fs_path's floor(g * 4) is robust to the byte rounding
    // at both ends.
    const float g = (static_cast<float>(path_class) + 0.5f) * 0.25f;
    return 0xFF000000u                 // a = 1: sky visibility, as everywhere
           | (0u << 16)                // b = 0: reserved
           | (to8(g) << 8)             // g = class
           | to8(wear);                // r = wear
}

} // namespace

std::vector<PathPiece> build_path_pieces(std::span<const math::PathStation> stations,
                                         std::span<const uint32_t> route_offsets) {
    std::vector<PathPiece> pieces;
    if (stations.empty() || route_offsets.size() < 2) {
        return pieces; // a stand with no paths (Rule 32)
    }

    // Cross-section offsets in u = |across| / worn_half_width, centre outward,
    // plus the discarded margin knot. Symmetric, so the strip is built by
    // mirroring this into negative u.
    constexpr int SIDE_KNOTS = PATH_CROSS_KNOTS + 1; // u = 0 .. 1 inclusive
    float knot_u[SIDE_KNOTS];
    for (int k = 0; k < SIDE_KNOTS; ++k) {
        knot_u[k] = static_cast<float>(k) / static_cast<float>(PATH_CROSS_KNOTS);
    }

    for (std::size_t r = 0; r + 1 < route_offsets.size(); ++r) {
        const std::size_t first = route_offsets[r];
        const std::size_t last_excl = route_offsets[r + 1];
        if (last_excl > stations.size() || last_excl - first < 2) {
            continue;
        }
        const std::size_t last = last_excl - 1;

        // Arc length along the tread, in 3D: a route that climbs 40 m over
        // 200 m of plan would tile its material 2% short if the rise were
        // ignored, and the stone-steps class is exactly where the rise is
        // largest.
        std::vector<float> arc(last_excl - first, 0.0f);
        for (std::size_t i = first + 1; i <= last; ++i) {
            const glm::vec2 d = stations[i].position - stations[i - 1].position;
            const float dy = stations[i].tread_height - stations[i - 1].tread_height;
            arc[i - first] = arc[i - 1 - first]
                           + std::sqrt(d.x * d.x + d.y * d.y + dy * dy);
        }

        // Split into pieces at class changes AND every PATH_PIECE_STATIONS.
        // Both cuts DUPLICATE the station, so consecutive pieces share their
        // seam geometry exactly and neither a gap nor an interpolated ordinal
        // can appear between them.
        std::size_t piece_begin = first;
        while (piece_begin < last) {
            const uint8_t cls = stations[piece_begin].path_class;
            std::size_t piece_end = piece_begin + 1;
            while (piece_end < last
                   && stations[piece_end].path_class == cls
                   && piece_end - piece_begin < PATH_PIECE_STATIONS) {
                ++piece_end;
            }
            // The seam station belongs to BOTH pieces; the next piece starts
            // where this one ended.
            const std::size_t count = piece_end - piece_begin + 1;

            // Verts per station: the knots of both halves share u = 0, so
            // 2*SIDE_KNOTS-1 of them, plus one discarded margin knot each side.
            const auto ring = static_cast<uint32_t>(2 * SIDE_KNOTS + 1);

            PathPiece piece;
            piece.path_class = cls;
            piece.mesh.vertices.reserve(count * ring);
            piece.mesh.indices.reserve((count - 1) * (ring - 1) * 6);

            for (std::size_t i = piece_begin; i <= piece_end; ++i) {
                const math::PathStation& st = stations[i];
                const glm::vec2 t = plan_tangent(stations, i, first, last);
                const glm::vec2 right{t.y, -t.x};

                // The tread is FLAT ACROSS by construction (core flattened the
                // ground to this profile), so the surface normal only tilts
                // along the route.
                float dh_ds = 0.0f;
                {
                    const std::size_t a = (i > first) ? i - 1 : i;
                    const std::size_t b = (i < last) ? i + 1 : i;
                    const glm::vec2 d = stations[b].position - stations[a].position;
                    const float run = std::sqrt(d.x * d.x + d.y * d.y);
                    if (run > 1e-6f) {
                        dh_ds = (stations[b].tread_height - stations[a].tread_height) / run;
                    }
                }
                const glm::vec3 tangent3 =
                    glm::normalize(glm::vec3{t.x, dh_ds, t.y});
                const glm::vec3 right3{right.x, 0.0f, right.y};
                glm::vec3 normal = glm::cross(tangent3, right3);
                if (normal.y < 0.0f) {
                    normal = -normal;
                }
                normal = glm::normalize(normal);

                const float w = std::max(st.worn_half_width, 0.01f);
                const float along = arc[i - first];

                // Ring order: -1-margin .. 0 .. +1+margin, i.e. left margin,
                // left knots (outer -> centre), right knots (centre -> outer),
                // right margin. Built as one monotonically increasing sweep so
                // the quad loop below needs no special case at the centre.
                for (uint32_t v = 0; v < ring; ++v) {
                    float across_m = 0.0f;
                    float wear = 0.0f;
                    if (v == 0) {
                        across_m = -(w + PATH_EDGE_MARGIN_M);
                    } else if (v == ring - 1) {
                        across_m = w + PATH_EDGE_MARGIN_M;
                    } else {
                        // v = 1 .. ring-2 covers the knots: the left half runs
                        // u = 1 down to 0, the right half 0 up to 1. The u = 0
                        // knot is shared, so there are 2*SIDE_KNOTS-1 of them
                        // and the ring is one bigger than that on each margin.
                        const int idx = static_cast<int>(v) - 1;
                        const int mid = SIDE_KNOTS - 1; // index of u = 0
                        const int k = std::abs(idx - mid);
                        const float u = knot_u[k];
                        across_m = (idx < mid ? -u : u) * w;
                        wear = math::path_wear_profile(u);
                    }
                    platform::Vertex vert;
                    vert.position = {st.position.x + right.x * across_m,
                                     st.tread_height,
                                     st.position.y + right.y * across_m};
                    vert.normal = normal;
                    vert.uv = {across_m, along};
                    vert.color_rgba = pack_path_color(wear, cls);
                    piece.mesh.vertices.push_back(vert);
                    piece.bounds.expand(vert.position);
                }
            }

            // Quads between consecutive rings.
            const auto rings = static_cast<uint32_t>(count);
            for (uint32_t s = 0; s + 1 < rings; ++s) {
                for (uint32_t v = 0; v + 1 < ring; ++v) {
                    const uint32_t a = s * ring + v;
                    const uint32_t b = a + 1;
                    const uint32_t c = (s + 1) * ring + v;
                    const uint32_t d = c + 1;
                    piece.mesh.indices.insert(piece.mesh.indices.end(),
                                              {a, c, b, b, c, d});
                }
            }
            pieces.push_back(std::move(piece));
            piece_begin = piece_end;
        }
    }
    return pieces;
}

} // namespace dfn::render
