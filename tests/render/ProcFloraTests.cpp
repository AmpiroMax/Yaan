/*
Created: 09:08:2026 - 19:38:20
Last updated: 09:08:2026 - 19:38:20
Module: tests/render
File: tests/render/ProcFloraTests.cpp

Responsibility:
- The flora invariant suite: determinism, triangle budgets, the two hard
  geometric floors (canopy clearance, shadow-caster minimum), envelope
  conformance, size bands, mesh well-formedness and neighbour analysis.

Key items:
- doctest cases over build_flora_mesh / analyse_neighbourhood.

Dependencies:
- Uses: ProcFlora.h, FloraSpecies.h, generated Constants.h, doctest.
- Used by: dfn_tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md §6.
- These are INVARIANTS a successor must be able to trust, not smoke tests:
  each one encodes a rule that cost a cross-zone negotiation to establish.
*/
/*
UPD:
- 09:08:2026 - 19:38:20: Created — stage-4 flora suite.
*/

#include "engine/render/sources/ProcFlora.h"

#include "engine/core/config/sources/Constants.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace dfn;
using namespace dfn::render;

namespace {

const FloraSpecies ALL[] = {
    FloraSpecies::DaleOak, FloraSpecies::HighlandPine, FloraSpecies::RiverBirch,
    FloraSpecies::ValeWillow, FloraSpecies::Snag,      FloraSpecies::Bush,
    FloraSpecies::BigBush,  FloraSpecies::FallenLog,   FloraSpecies::Deadfall,
};

const FloraLod LODS[] = {FloraLod::Full, FloraLod::Reduced, FloraLod::Silhouette};

/// Lowest vertex that belongs to foliage (identified by the foliage colour).
float lowest_foliage_y(const MeshData& m, uint32_t leaf_color) {
    float lo = 1e9f;
    for (const platform::Vertex& v : m.vertices) {
        if (v.color == leaf_color) lo = std::min(lo, v.position.y);
    }
    return lo;
}

} // namespace

TEST_CASE("flora: generation is deterministic") {
    for (const FloraSpecies s : ALL) {
        for (const FloraLod lod : LODS) {
            const MeshData a = build_flora_mesh(s, 3, FloraShape{}, lod);
            const MeshData b = build_flora_mesh(s, 3, FloraShape{}, lod);
            REQUIRE(a.vertices.size() == b.vertices.size());
            REQUIRE(a.indices == b.indices);
            for (size_t i = 0; i < a.vertices.size(); ++i) {
                CHECK(a.vertices[i].position.x == doctest::Approx(b.vertices[i].position.x));
                CHECK(a.vertices[i].position.y == doctest::Approx(b.vertices[i].position.y));
                CHECK(a.vertices[i].position.z == doctest::Approx(b.vertices[i].position.z));
            }
        }
    }
}

TEST_CASE("flora: variants actually differ") {
    // The point of variants is that a forest is not a clone army.
    const MeshData a = build_flora_mesh(FloraSpecies::DaleOak, 0, FloraShape{}, FloraLod::Full);
    const MeshData b = build_flora_mesh(FloraSpecies::DaleOak, 7, FloraShape{}, FloraLod::Full);
    bool differs = a.vertices.size() != b.vertices.size();
    if (!differs) {
        for (size_t i = 0; i < a.vertices.size(); ++i) {
            if (std::fabs(a.vertices[i].position.y - b.vertices[i].position.y) > 1e-3f) {
                differs = true;
                break;
            }
        }
    }
    CHECK(differs);
}

TEST_CASE("flora: triangle budgets hold (TREE_TRI_BUDGET_MAX)") {
    const auto cap = static_cast<size_t>(config::TREE_TRI_BUDGET_MAX);
    for (const FloraSpecies s : ALL) {
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const MeshData full = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            CHECK(full.triangle_count() <= cap);
            const MeshData sil = build_flora_mesh(s, v, FloraShape{}, FloraLod::Silhouette);
            CHECK(sil.triangle_count() <= 120u);
            const MeshData red = build_flora_mesh(s, v, FloraShape{}, FloraLod::Reduced);
            CHECK(red.triangle_count() <= full.triangle_count());
        }
    }
}

TEST_CASE("flora: canopy clearance floor is never violated") {
    // docs/specs/flora.md §3.5 — в32 / CANOPY_CLEARANCE_MIN. Checked against the
    // LOWEST FOLIAGE VERTEX, not the nominal crown base, because that is what a
    // player's head actually meets (drooping species are the reason).
    const auto floor_m = static_cast<float>(config::CANOPY_CLEARANCE_MIN);
    FloraShape understory;
    understory.understory = true;
    understory.maturity = 0.4f;
    const FloraShape shapes[] = {FloraShape{}, understory};

    for (const FloraSpecies s : ALL) {
        if (!is_canopy_tree(s)) continue;
        const uint32_t leaf = pack(species_params(s).foliage_color);
        for (const FloraShape& sh : shapes) {
            for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
                const MeshData m = build_flora_mesh(s, v, sh, FloraLod::Full);
                const float lo = lowest_foliage_y(m, leaf);
                if (lo < 1e8f) {
                    CHECK(lo >= doctest::Approx(floor_m).epsilon(0.05));
                }
            }
        }
    }
}

TEST_CASE("flora: no branch below the shadow-caster floor") {
    // Render measured SHADOW_TEXEL_M = 0.15625; a caster under ~0.31 m casts
    // NOTHING. We do not model twigs (docs/specs/flora.md §3.5). Proxy check:
    // no emitted geometry is thinner than the floor in cross-section, tested by
    // the smallest ring radius the generator can emit.
    for (const FloraSpecies s : ALL) {
        const SpeciesParams& sp = species_params(s);
        CHECK(sp.min_branch_diameter >= 0.30f);
    }
}

TEST_CASE("flora: sizes stay inside the design bands") {
    struct Band { FloraSpecies s; float lo; float hi; };
    const Band bands[] = {
        {FloraSpecies::DaleOak, static_cast<float>(config::OAK_HEIGHT_MIN),
         static_cast<float>(config::OAK_HEIGHT_MAX)},
        {FloraSpecies::HighlandPine, static_cast<float>(config::PINE_HEIGHT_MIN),
         static_cast<float>(config::PINE_HEIGHT_MAX)},
        {FloraSpecies::RiverBirch, static_cast<float>(config::BIRCH_HEIGHT_MIN),
         static_cast<float>(config::BIRCH_HEIGHT_MAX)},
    };
    for (const Band& b : bands) {
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const MeshData m = build_flora_mesh(b.s, v, FloraShape{}, FloraLod::Full);
            float top = 0.0f;
            for (const platform::Vertex& vx : m.vertices) top = std::max(top, vx.position.y);
            CHECK(top >= b.lo * 0.85f);       // branches may not reach the nominal top
            CHECK(top <= b.hi * 1.15f);       // nor overshoot it meaningfully
        }
        // Crown base fraction inside design's CROWN_BASE_FRACTION band.
        const float frac = species_crown_base(b.s) / species_nominal_height(b.s);
        CHECK(frac >= doctest::Approx(config::CROWN_BASE_FRACTION_MIN).epsilon(0.02));
        CHECK(frac <= doctest::Approx(config::CROWN_BASE_FRACTION_MAX).epsilon(0.02));
    }
}

TEST_CASE("flora: meshes are well-formed") {
    for (const FloraSpecies s : ALL) {
        const MeshData m = build_flora_mesh(s, 5, FloraShape{}, FloraLod::Full);
        REQUIRE(m.indices.size() % 3 == 0);
        CHECK(m.indices.size() > 0);
        for (const uint32_t i : m.indices) {
            REQUIRE(i < m.vertices.size());
        }
        for (const platform::Vertex& v : m.vertices) {
            const float len = std::sqrt(v.normal.x * v.normal.x + v.normal.y * v.normal.y
                                        + v.normal.z * v.normal.z);
            CHECK(len == doctest::Approx(1.0f).epsilon(0.01));
        }
        // No degenerate triangles.
        for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
            const glm::vec3 a = m.vertices[m.indices[i]].position;
            const glm::vec3 b = m.vertices[m.indices[i + 1]].position;
            const glm::vec3 c = m.vertices[m.indices[i + 2]].position;
            const glm::vec3 n = glm::cross(b - a, c - a);
            CHECK(glm::length(n) > 1e-7f);
        }
    }
}

TEST_CASE("flora: trunk buries itself (root flare below origin)") {
    // GROUND_SINK_FRAC 0.12 cannot cover 0.84 m of ground drop across a 1.2 m
    // trunk on TREE_SLOPE_MAX — the flare does it in geometry instead.
    for (const FloraSpecies s : ALL) {
        if (!is_canopy_tree(s) && s != FloraSpecies::Snag) continue;
        const MeshData m = build_flora_mesh(s, 2, FloraShape{}, FloraLod::Full);
        float lowest = 1e9f;
        for (const platform::Vertex& v : m.vertices) lowest = std::min(lowest, v.position.y);
        CHECK(lowest <= -0.9f);
    }
}

TEST_CASE("flora: neighbour analysis produces shyness and lean") {
    std::vector<math::ScatterInstance> inst;
    // Two oaks close enough for their crowns to overlap, plus a lone one far off.
    inst.push_back({{0.0f, 0.0f, 0.0f}, 0.0f, 1.0f, math::ScatterSpecies::OakTree});
    inst.push_back({{8.0f, 0.0f, 0.0f}, 0.0f, 1.0f, math::ScatterSpecies::OakTree});
    inst.push_back({{500.0f, 0.0f, 500.0f}, 0.0f, 1.0f, math::ScatterSpecies::OakTree});

    const std::vector<FloraShape> shapes = analyse_neighbourhood(inst, inst.size());
    REQUIRE(shapes.size() == 3);

    // Crowded pair: non-zero shyness pointing AT the neighbour, lean AWAY.
    CHECK(shapes[0].shyness > 0.0f);
    CHECK(shapes[0].shy_dir.x > 0.5f);  // neighbour is at +X
    CHECK(shapes[0].lean > 0.0f);
    CHECK(shapes[0].lean_dir.x < -0.5f); // leans away, toward -X
    CHECK(shapes[1].shy_dir.x < -0.5f);

    // Isolated tree: neither.
    CHECK(shapes[2].shyness == doctest::Approx(0.0f));
    CHECK(shapes[2].lean == doctest::Approx(0.0f));
}

TEST_CASE("flora: variant selection is stable and position-keyed") {
    CHECK(flora_variant_for({123.5f, -44.25f}) == flora_variant_for({123.5f, -44.25f}));
    CHECK(flora_variant_for({0.0f, 0.0f}) < FLORA_VARIANTS);
    bool any_diff = false;
    for (int i = 1; i < 40 && !any_diff; ++i) {
        any_diff = flora_variant_for({0.0f, 0.0f})
            != flora_variant_for({static_cast<float>(i) * 7.0f, 0.0f});
    }
    CHECK(any_diff);
}
