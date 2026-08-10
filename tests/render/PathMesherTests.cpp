/*
Created: 10:08:2026 - 12:26:05
Last updated: 10:08:2026 - 12:26:05
Module: tests
File: tests/render/PathMesherTests.cpp

Responsibility:
- Unit tests for the §8.1 path surface ribbon: the cross-section's fidelity to
  core's wear profile (MEASURED, not assumed), class-run splitting, the tread
  frame, arc length through a bend, culling granularity, degenerate inputs.

Key items:
- doctest cases over build_path_pieces() and the path atlas.

Dependencies:
- Uses: doctest, engine/render PathMesher + ProcTexture, engine/core/math.
- Used by: ctest (render_path_mesher).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE KNOT-ERROR CASE IS THE LOAD-BEARING ONE. It is the guard that lets the
  shader NOT contain a copy of core's wear formula: if core retunes the
  profile's curvature, the sampled cross-section stops matching it and this
  reds, instead of the two zones silently disagreeing about where a path ends.
*/
/*
UPD:
- 10:08:2026 - 12:26:05: Created — the path surface splat.
*/

#include "engine/render/sources/PathMesher.h"
#include "engine/render/sources/ProcTexture.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using dfn::math::PathStation;
using dfn::render::build_path_pieces;
using dfn::render::PathPiece;
using dfn::render::PATH_CROSS_KNOTS;
using dfn::render::PATH_EDGE_MARGIN_M;
using dfn::render::PATH_PIECE_STATIONS;

namespace {

// A straight route along +X at a constant height, one class.
std::vector<PathStation> straight(std::size_t n, float half_width, uint8_t cls,
                                  float step = 4.0f, float height = 10.0f) {
    std::vector<PathStation> s(n);
    for (std::size_t i = 0; i < n; ++i) {
        s[i].position = {static_cast<float>(i) * step, 0.0f};
        s[i].tread_height = height;
        s[i].worn_half_width = half_width;
        s[i].path_class = cls;
    }
    return s;
}

// Decodes the vertex colour channels the shader reads.
float vert_wear(uint32_t rgba) { return static_cast<float>(rgba & 0xFFu) / 255.0f; }
uint8_t vert_class(uint32_t rgba) {
    const float g = static_cast<float>((rgba >> 8) & 0xFFu) / 255.0f;
    return static_cast<uint8_t>(g * 4.0f);
}

} // namespace

TEST_CASE("an empty network is an empty answer, not a failure") {
    CHECK(build_path_pieces({}, {}).empty());
    const auto s = straight(4, 1.1f, 1);
    const std::vector<uint32_t> one_station{0u, 1u};
    // A single station has no direction and cannot be a ribbon.
    CHECK(build_path_pieces(s, one_station).empty());
}

TEST_CASE("the cross-section reproduces core's wear profile within the derived knot error") {
    // THE CLAIM: sampling math::path_wear_profile at PATH_CROSS_KNOTS uniform
    // knots and interpolating linearly costs at most h^2/4 = 0.010 of wear.
    // Measured against core's function at fine resolution across the tread, on
    // the real ribbon rather than on the arithmetic.
    const float w = 1.1f;
    const auto s = straight(4, w, 1);
    const std::vector<uint32_t> offs{0u, 4u};
    const auto pieces = build_path_pieces(s, offs);
    REQUIRE(pieces.size() == 1);
    const PathPiece& p = pieces.front();

    // The ring of the second station (interior, so its frame is central).
    const std::size_t ring = static_cast<std::size_t>(2 * (PATH_CROSS_KNOTS + 1) + 1);
    REQUIRE(p.mesh.vertices.size() % ring == 0);

    // Rebuild the piecewise-linear wear the rasteriser will interpolate, from
    // the vertices of one ring, and compare it to core's curve.
    std::vector<std::pair<float, float>> knots; // (across_m, wear)
    for (std::size_t v = ring; v < 2 * ring; ++v) {
        knots.emplace_back(p.mesh.vertices[v].uv.x, vert_wear(p.mesh.vertices[v].color_rgba));
    }
    std::sort(knots.begin(), knots.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    float worst = 0.0f;
    for (int i = 0; i <= 2000; ++i) {
        const float across = -w + 2.0f * w * static_cast<float>(i) / 2000.0f;
        const float truth = dfn::math::path_wear_profile(std::fabs(across) / w);
        // Linear interpolation between the bracketing knots.
        float lerped = 0.0f;
        for (std::size_t k = 1; k < knots.size(); ++k) {
            if (across <= knots[k].first) {
                const float span = knots[k].first - knots[k - 1].first;
                const float t = span > 1e-6f ? (across - knots[k - 1].first) / span : 0.0f;
                lerped = knots[k - 1].second + (knots[k].second - knots[k - 1].second) * t;
                break;
            }
        }
        worst = std::max(worst, std::fabs(lerped - truth));
    }
    INFO("worst wear error across the tread: ", worst);
    // h^2/4 for h = 1/PATH_CROSS_KNOTS, plus the 1/255 the colour byte costs.
    const float bound = 0.25f / static_cast<float>(PATH_CROSS_KNOTS * PATH_CROSS_KNOTS)
                      + 1.0f / 255.0f;
    CHECK(worst <= bound);

    // AND THE CONTROL: the same comparison against a DIFFERENT curve must fail,
    // or the bound above is passing because everything passes it.
    float worst_wrong = 0.0f;
    for (int i = 0; i <= 2000; ++i) {
        const float across = -w + 2.0f * w * static_cast<float>(i) / 2000.0f;
        const float u = std::fabs(across) / w;
        const float wrong = std::max(0.0f, 1.0f - u); // linear instead of 1-u^2
        float lerped = 0.0f;
        for (std::size_t k = 1; k < knots.size(); ++k) {
            if (across <= knots[k].first) {
                const float span = knots[k].first - knots[k - 1].first;
                const float t = span > 1e-6f ? (across - knots[k - 1].first) / span : 0.0f;
                lerped = knots[k - 1].second + (knots[k].second - knots[k - 1].second) * t;
                break;
            }
        }
        worst_wrong = std::max(worst_wrong, std::fabs(lerped - wrong));
    }
    INFO("worst error against the WRONG curve: ", worst_wrong);
    CHECK(worst_wrong > bound * 5.0f);
}

TEST_CASE("wear is 1 at the centreline and 0 at the worn edge") {
    const auto s = straight(4, 2.0f, 2);
    const std::vector<uint32_t> offs{0u, 4u};
    const auto pieces = build_path_pieces(s, offs);
    REQUIRE(pieces.size() == 1);
    float max_wear = 0.0f;
    for (const auto& v : pieces[0].mesh.vertices) {
        max_wear = std::max(max_wear, vert_wear(v.color_rgba));
        // Nothing outside the worn edge claims any wear at all.
        if (std::fabs(v.uv.x) > 2.0f + 1e-4f) {
            CHECK(vert_wear(v.color_rgba) == doctest::Approx(0.0f));
        }
        // And nothing at the edge does either.
        if (std::fabs(std::fabs(v.uv.x) - 2.0f) < 1e-4f) {
            CHECK(vert_wear(v.color_rgba) == doctest::Approx(0.0f).epsilon(0.01));
        }
    }
    CHECK(max_wear == doctest::Approx(1.0f).epsilon(0.01));
}

TEST_CASE("the mesh runs past the worn edge, and everything past it is discarded") {
    const float w = 1.1f;
    const auto s = straight(4, w, 1);
    const std::vector<uint32_t> offs{0u, 4u};
    const auto pieces = build_path_pieces(s, offs);
    REQUIRE(pieces.size() == 1);
    float widest = 0.0f;
    for (const auto& v : pieces[0].mesh.vertices) {
        widest = std::max(widest, std::fabs(v.uv.x));
    }
    // The GEOMETRY's boundary is not the SURFACE's boundary — that is the whole
    // reason §8.1's "never a decal ribbon" is satisfied by a ribbon.
    CHECK(widest == doctest::Approx(w + PATH_EDGE_MARGIN_M));
}

TEST_CASE("a piece never spans a class change") {
    // Cobble for four stations, then a hint-path: ordinals 0 and 2, so an
    // interpolated seam would paint a band of ordinal 1 (dirt) that core never
    // routed. This is the case that made pieces per-class in the first place.
    auto s = straight(8, 1.4f, 0);
    for (std::size_t i = 4; i < 8; ++i) {
        s[i].path_class = 2;
    }
    const std::vector<uint32_t> offs{0u, 8u};
    const auto pieces = build_path_pieces(s, offs);
    REQUIRE(pieces.size() >= 2);
    for (const PathPiece& p : pieces) {
        for (const auto& v : p.mesh.vertices) {
            CHECK(vert_class(v.color_rgba) == p.path_class);
        }
        // No piece carries an ordinal that is not in the input.
        CHECK((p.path_class == 0 || p.path_class == 2));
    }
    // And the pieces meet: the seam station belongs to both, so there is no gap.
    float end_of_first = -1e9f;
    float start_of_second = 1e9f;
    for (const auto& v : pieces[0].mesh.vertices) end_of_first = std::max(end_of_first, v.position.x);
    for (const auto& v : pieces[1].mesh.vertices) start_of_second = std::min(start_of_second, v.position.x);
    CHECK(start_of_second == doctest::Approx(end_of_first));
}

TEST_CASE("long routes are split so the frustum has something to reject") {
    // 200 stations of one class at 4 m: one piece would be an 800 m bounding
    // box that is on screen always.
    const auto s = straight(200, 1.1f, 1);
    const std::vector<uint32_t> offs{0u, 200u};
    const auto pieces = build_path_pieces(s, offs);
    CHECK(pieces.size() >= 6);
    for (const PathPiece& p : pieces) {
        const float span = p.bounds.size().x;
        INFO("piece span ", span, " m");
        CHECK(span <= static_cast<float>(PATH_PIECE_STATIONS) * 4.0f + 1.0f);
        CHECK(p.bounds.valid());
    }
}

TEST_CASE("the tread is flat across and follows the profile along") {
    // A route that climbs: every ring must be level (the ground was flattened
    // to this profile), while the surface normal tilts along the route.
    auto s = straight(6, 1.1f, 3);
    for (std::size_t i = 0; i < s.size(); ++i) {
        s[i].tread_height = 10.0f + 2.0f * static_cast<float>(i); // 2 m per 4 m
    }
    const std::vector<uint32_t> offs{0u, 6u};
    const auto pieces = build_path_pieces(s, offs);
    REQUIRE(!pieces.empty());
    const std::size_t ring = static_cast<std::size_t>(2 * (PATH_CROSS_KNOTS + 1) + 1);
    const PathPiece& p = pieces.front();
    for (std::size_t r = 0; r * ring < p.mesh.vertices.size(); ++r) {
        const float y0 = p.mesh.vertices[r * ring].position.y;
        for (std::size_t v = 0; v < ring; ++v) {
            CHECK(p.mesh.vertices[r * ring + v].position.y == doctest::Approx(y0));
        }
    }
    // Normal tilts back against the climb, and still points up.
    const glm::vec3 n = p.mesh.vertices[ring].normal;
    CHECK(n.y > 0.0f);
    CHECK(n.y < 0.999f);
    CHECK(std::fabs(glm::length(n) - 1.0f) < 1e-3f);
}

TEST_CASE("arc length is measured along the tread, not across the map") {
    // A right-angle bend and a climb: uv.y at the last station must be the sum
    // of the 3D segment lengths, not the straight-line distance. Material that
    // tiles by plan distance stretches through every bend.
    std::vector<PathStation> s(3);
    s[0].position = {0.0f, 0.0f};
    s[1].position = {4.0f, 0.0f};
    s[2].position = {4.0f, 4.0f};
    for (auto& st : s) {
        st.worn_half_width = 1.1f;
        st.path_class = 1;
    }
    s[0].tread_height = 0.0f;
    s[1].tread_height = 3.0f; // 3-4-5
    s[2].tread_height = 3.0f;
    const std::vector<uint32_t> offs{0u, 3u};
    const auto pieces = build_path_pieces(s, offs);
    REQUIRE(!pieces.empty());
    float longest = 0.0f;
    for (const auto& v : pieces[0].mesh.vertices) {
        longest = std::max(longest, v.uv.y);
    }
    CHECK(longest == doctest::Approx(5.0f + 4.0f)); // not sqrt(32) = 5.66
}

TEST_CASE("determinism: the same network builds the same bytes") {
    const auto s = straight(40, 1.3f, 1);
    const std::vector<uint32_t> offs{0u, 40u};
    const auto a = build_path_pieces(s, offs);
    const auto b = build_path_pieces(s, offs);
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        REQUIRE(a[i].mesh.vertices.size() == b[i].mesh.vertices.size());
        for (std::size_t v = 0; v < a[i].mesh.vertices.size(); ++v) {
            CHECK(a[i].mesh.vertices[v].position.x == b[i].mesh.vertices[v].position.x);
            CHECK(a[i].mesh.vertices[v].color_rgba == b[i].mesh.vertices[v].color_rgba);
        }
        CHECK(a[i].mesh.indices == b[i].mesh.indices);
    }
}

TEST_CASE("the path atlas is four distinct tiling cells, one per PathClass") {
    const uint32_t cell = 32;
    const auto atlas = dfn::render::generate_path_atlas(cell, 7u);
    REQUIRE(atlas.size() == static_cast<std::size_t>(cell * 2) * (cell * 2) * 4);

    // Determinism, and independence from the terrain atlas.
    CHECK(atlas == dfn::render::generate_path_atlas(cell, 7u));
    CHECK(atlas != dfn::render::generate_terrain_atlas(cell, 7u));

    // The four cells must actually differ — the ordinal-keyed lookup is
    // meaningless if two classes draw the same thing.
    const auto cell_bytes = [&](uint32_t idx) {
        std::vector<uint8_t> out;
        const uint32_t side = cell * 2;
        const uint32_t x0 = (idx & 1u) * cell;
        const uint32_t y0 = (idx >> 1u) * cell;
        for (uint32_t y = 0; y < cell; ++y) {
            for (uint32_t x = 0; x < cell; ++x) {
                const std::size_t o = (static_cast<std::size_t>(y0 + y) * side + (x0 + x)) * 4;
                out.insert(out.end(), atlas.begin() + static_cast<long>(o),
                           atlas.begin() + static_cast<long>(o) + 4);
            }
        }
        return out;
    };
    for (uint32_t i = 0; i < 4; ++i) {
        for (uint32_t j = i + 1; j < 4; ++j) {
            INFO("path atlas cells ", i, " and ", j, " are identical");
            CHECK(cell_bytes(i) != cell_bytes(j));
        }
    }

    // The road must be LIGHTER than the forest floor it cuts through, or the
    // whole feature is invisible at 640x360 before any texture resolves
    // (PALETTE SIGNAL STRENGTH). Measured as mean luminance against the
    // terrain atlas's grass cell.
    const auto mean_luma = [](const std::vector<uint8_t>& px) {
        double sum = 0.0;
        for (std::size_t i = 0; i + 3 < px.size(); i += 4) {
            sum += 0.30 * px[i] + 0.59 * px[i + 1] + 0.11 * px[i + 2];
        }
        return sum / static_cast<double>(px.size() / 4);
    };
    const auto terrain = dfn::render::generate_terrain_atlas(cell, 7u);
    std::vector<uint8_t> grass;
    for (uint32_t y = 0; y < cell; ++y) {
        for (uint32_t x = 0; x < cell; ++x) {
            const std::size_t o = (static_cast<std::size_t>(y) * (cell * 2) + x) * 4;
            grass.insert(grass.end(), terrain.begin() + static_cast<long>(o),
                         terrain.begin() + static_cast<long>(o) + 4);
        }
    }
    const double dirt_road = mean_luma(cell_bytes(1));
    const double grass_luma = mean_luma(grass);
    INFO("packed earth ", dirt_road, " vs grass ", grass_luma);
    CHECK(dirt_road > grass_luma + 20.0);
}
