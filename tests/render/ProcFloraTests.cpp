/*
Created: 09:08:2026 - 19:38:20
Last updated: 09:08:2026 - 21:18:02
Module: tests/render
File: tests/render/ProcFloraTests.cpp

Responsibility:
- The flora invariant suite: determinism, triangle budgets, the two hard
  geometric floors (canopy clearance, shadow-caster minimum), envelope
  conformance, size bands, mesh well-formedness, neighbour analysis, and the
  leaf-card contracts (mask porosity profile, atlas value order, the foliage
  vertex-colour channel map).

Key items:
- doctest cases over build_flora_mesh / analyse_neighbourhood /
  generate_leaf_atlas.

Dependencies:
- Uses: ProcFlora.h, FloraSpecies.h, FloraCards.h, generated Constants.h,
  doctest.
- Used by: dfn_tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md §6.
- These are INVARIANTS a successor must be able to trust, not smoke tests:
  each one encodes a rule that cost a cross-zone negotiation to establish.
- A GREEN SUITE IS NOT EVIDENCE THAT A TREE READS. 31 000 assertions once
  passed over a tree with no leaves at all (§3.7.3). Rule 27's frame is the
  verification; this file only stops known defects from coming back.
*/
/*
UPD:
- 09:08:2026 - 19:38:20: Created — stage-4 flora suite.
- 09:08:2026 - 20:21:13: Leaf cards: the suite now reads BOTH streams
  (FloraMesh.wood / .cards), plus new cases for the mask's measured porosity
  profile, the season value-order rule, the foliage channel map and the
  per-card constancy of the value jitter.
- 09:08:2026 - 21:02:02: Card legibility floor case (no detached scraps),
  added after reading the first card frame.
- 09:08:2026 - 21:18:02: design's CROWN_ASPECT_MAX acceptance rule, measured on
  the BUILT tree and PER VARIANT (pooling variants measures the height spread as
  if it were one crown's shape, and did exactly that in the first report);
  crown-width FLOOR, since only the ceiling of design's band was ever asserted
  and the birch had drifted a third under its brief with a green suite.
*/

#include "engine/render/sources/FloraSkeleton.h"
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

size_t total_tris(const FloraMesh& f) {
    return f.wood.triangle_count() + f.cards.triangle_count();
}

/// Lowest vertex that belongs to FOLIAGE, in either stream: leaf-coloured
/// geometry in the opaque stream (conifer tiers, bush blobs, silhouette shells)
/// plus every card vertex, since the card stream is nothing but foliage.
float lowest_foliage_y(const FloraMesh& f, uint32_t leaf_color) {
    float lo = 1e9f;
    for (const platform::Vertex& v : f.wood.vertices) {
        if (v.color_rgba == leaf_color) lo = std::min(lo, v.position.y);
    }
    for (const platform::Vertex& v : f.cards.vertices) {
        lo = std::min(lo, v.position.y);
    }
    return lo;
}

float triangle_area(const MeshData& m, size_t i) {
    const glm::vec3 a = m.vertices[m.indices[i]].position;
    const glm::vec3 b = m.vertices[m.indices[i + 1]].position;
    const glm::vec3 c = m.vertices[m.indices[i + 2]].position;
    return 0.5f * glm::length(glm::cross(b - a, c - a));
}

float widest_radius(const FloraMesh& f) {
    float widest = 0.0f;
    for (const MeshData* m : {&f.wood, &f.cards}) {
        for (const platform::Vertex& v : m->vertices) {
            widest = std::max(widest, std::sqrt(v.position.x * v.position.x
                                                + v.position.z * v.position.z));
        }
    }
    return widest;
}

float highest_y(const FloraMesh& f) {
    float top = 0.0f;
    for (const MeshData* m : {&f.wood, &f.cards}) {
        for (const platform::Vertex& v : m->vertices) top = std::max(top, v.position.y);
    }
    return top;
}

/// Byte accessors for the frozen 0xAABBGGRR vertex colour.
uint8_t chan_r(uint32_t c) { return static_cast<uint8_t>(c & 0xFFu); }
uint8_t chan_g(uint32_t c) { return static_cast<uint8_t>((c >> 8) & 0xFFu); }
uint8_t chan_b(uint32_t c) { return static_cast<uint8_t>((c >> 16) & 0xFFu); }
uint8_t chan_a(uint32_t c) { return static_cast<uint8_t>((c >> 24) & 0xFFu); }

float luminance(glm::vec3 c) { return 0.30f * c.r + 0.60f * c.g + 0.10f * c.b; }

} // namespace

TEST_CASE("flora: generation is deterministic") {
    for (const FloraSpecies s : ALL) {
        for (const FloraLod lod : LODS) {
            const FloraMesh a = build_flora_mesh(s, 3, FloraShape{}, lod);
            const FloraMesh b = build_flora_mesh(s, 3, FloraShape{}, lod);
            REQUIRE(a.wood.vertices.size() == b.wood.vertices.size());
            REQUIRE(a.cards.vertices.size() == b.cards.vertices.size());
            REQUIRE(a.wood.indices == b.wood.indices);
            REQUIRE(a.cards.indices == b.cards.indices);
            for (size_t i = 0; i < a.wood.vertices.size(); ++i) {
                CHECK(a.wood.vertices[i].position.x
                      == doctest::Approx(b.wood.vertices[i].position.x));
                CHECK(a.wood.vertices[i].position.y
                      == doctest::Approx(b.wood.vertices[i].position.y));
            }
            for (size_t i = 0; i < a.cards.vertices.size(); ++i) {
                CHECK(a.cards.vertices[i].position.y
                      == doctest::Approx(b.cards.vertices[i].position.y));
                CHECK(a.cards.vertices[i].color_rgba == b.cards.vertices[i].color_rgba);
            }
        }
    }
}

TEST_CASE("flora: variants actually differ") {
    // The point of variants is that a forest is not a clone army.
    const FloraMesh a = build_flora_mesh(FloraSpecies::DaleOak, 0, FloraShape{},
                                         FloraLod::Full);
    const FloraMesh b = build_flora_mesh(FloraSpecies::DaleOak, 7, FloraShape{},
                                         FloraLod::Full);
    bool differs = a.wood.vertices.size() != b.wood.vertices.size();
    if (!differs) {
        for (size_t i = 0; i < a.wood.vertices.size(); ++i) {
            if (std::fabs(a.wood.vertices[i].position.y - b.wood.vertices[i].position.y)
                > 1e-3f) {
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
            const FloraMesh full = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            CHECK(total_tris(full) <= cap);
            const FloraMesh sil =
                build_flora_mesh(s, v, FloraShape{}, FloraLod::Silhouette);
            CHECK(total_tris(sil) <= 120u);
            const FloraMesh red =
                build_flora_mesh(s, v, FloraShape{}, FloraLod::Reduced);
            CHECK(total_tris(red) <= total_tris(full));
        }
    }
}

TEST_CASE("flora: canopy clearance floor is never violated") {
    // docs/specs/flora.md §3.5 — в32 / CANOPY_CLEARANCE_MIN. Checked against the
    // LOWEST FOLIAGE VERTEX, not the nominal crown base, because that is what a
    // player's head actually meets (drooping species are the reason, and a leaf
    // card hangs below the point it is placed at).
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
                const FloraMesh m = build_flora_mesh(s, v, sh, FloraLod::Full);
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
    // NOTHING. We do not model twigs (docs/specs/flora.md §3.5).
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
            const FloraMesh m = build_flora_mesh(b.s, v, FloraShape{}, FloraLod::Full);
            // The upper bound is TIGHT on purpose. The height band is a
            // cross-zone contract: core's canopy occlusion and design's C4
            // arithmetic use these maxima, so a tree that overshoots is
            // silently taller than the model everyone validates against. A
            // loose tolerance here hid exactly that bug once already.
            CHECK(highest_y(m) <= b.hi * 1.02f);
            CHECK(highest_y(m) >= b.lo * 0.80f);
        }
        // Crown base fraction against design's revised rule. CROWN_BASE_FRACTION_MIN
        // is a WALKABILITY FLOOR — more clear trunk is never worse for walking
        // under a canopy — and _MAX stopped being a binding cap when the birch
        // showed that the same number was silently doing a second job (setting
        // the crown's ASPECT, purely because it is a fraction of height). The
        // base is now DERIVED per species; the birch has its own landed band.
        const float frac = species_crown_base(b.s) / species_nominal_height(b.s);
        CHECK(frac >= doctest::Approx(config::CROWN_BASE_FRACTION_MIN).epsilon(0.02));
        if (b.s == FloraSpecies::RiverBirch) {
            CHECK(frac >= doctest::Approx(config::BIRCH_CROWN_BASE_FRACTION_MIN)
                              .epsilon(0.02));
            CHECK(frac <= doctest::Approx(config::BIRCH_CROWN_BASE_FRACTION_MAX)
                              .epsilon(0.02));
        } else {
            CHECK(frac <= doctest::Approx(config::CROWN_BASE_FRACTION_MAX).epsilon(0.02));
        }
    }
}

TEST_CASE("flora: every canopy species actually HAS a crown") {
    // THE TEST THAT WAS MISSING. The first rendered frame showed birches as
    // bare poles: their primary branches (0.17 m) fall under the 0.35 m shadow
    // floor, and the generator terminated them WITHOUT emitting the foliage
    // that §3.5 says must attach to the parent instead. 31k assertions passed
    // over a tree with zero leaves, because every one of them measured budgets,
    // bounds and clearances — none asked whether the thing had a crown.
    for (const FloraSpecies s : ALL) {
        if (!is_canopy_tree(s)) continue;
        const SpeciesParams& sp = species_params(s);
        const uint32_t leaf = pack(sp.foliage_color);
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const FloraMesh m = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            // Measured as foliage AREA, not vertex count: a conifer's cone
            // tiers cover the whole envelope with very few vertices, so a
            // vertex-share threshold fails the pine while passing a bald oak.
            // Area is what the eye integrates, so area is what we assert.
            float leaf_area = 0.0f;
            float highest_leaf = 0.0f;
            for (size_t i = 0; i + 2 < m.wood.indices.size(); i += 3) {
                const platform::Vertex& a = m.wood.vertices[m.wood.indices[i]];
                if (a.color_rgba != leaf) continue;
                leaf_area += triangle_area(m.wood, i);
                highest_leaf = std::max(highest_leaf, a.position.y);
            }
            for (size_t i = 0; i + 2 < m.cards.indices.size(); i += 3) {
                leaf_area += triangle_area(m.cards, i);
                highest_leaf = std::max(
                    highest_leaf, m.cards.vertices[m.cards.indices[i]].position.y);
            }
            // A crown covers its envelope: bar is one crown cross-section.
            const float crown_r = species_crown_radius(s);
            CHECK(leaf_area >= crown_r * crown_r);
            // And it sits in the crown, not down the trunk.
            CHECK(highest_leaf > species_crown_base(s));
        }
    }
}

TEST_CASE("flora: crown width stays inside the envelope") {
    // Added after a real bug: the envelope's VERTICAL clip was implemented and
    // its RADIAL clip was not, so oaks came out 24.5 m wide against a 10-16 m
    // brief and pines 24.9 m against 6-9 m. Width is not cosmetic — design
    // derived TREE_SPACING_FOREST (12-18 m) FROM the crown width, so a crown
    // twice its spec silently turns a thinned forest back into a closed one.
    // Cards are inside the same budget: their reach, not the notional cluster
    // radius, is what the envelope containment is run against.
    struct Band { FloraSpecies s; float max_diameter; };
    const Band bands[] = {
        {FloraSpecies::DaleOak, 16.0f},
        {FloraSpecies::HighlandPine, 9.0f},
        {FloraSpecies::RiverBirch, 7.0f},
        {FloraSpecies::ValeWillow, 16.0f},
    };
    for (const Band& b : bands) {
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const FloraMesh m = build_flora_mesh(b.s, v, FloraShape{}, FloraLod::Full);
            CHECK(widest_radius(m) * 2.0f <= b.max_diameter);
        }
    }
}

TEST_CASE("flora: CROWN ASPECT CEILING (design's §5 acceptance rule)") {
    // The rule that would have caught the birch on the first build instead of
    // the fourth screenshot. Two authored rules — crown width and crown base —
    // multiplied into a container 2.3x taller than wide, and FOUR attempts at
    // rearranging its CONTENTS failed, because the mass IS the container.
    //
    // MEASURED ON THE BUILT TREE, NOT ON THE SPEC, and that detail is
    // load-bearing: the birch's container was 1.8:1 while the tree it produced
    // was 2.3:1, so a ceiling checked against the parameters would have passed
    // the tree that fails. Same discipline as design's polyline-perimeter rule
    // for the massif — measure the artefact, not the intention.
    //
    // PER VARIANT, never pooled: pooling 12 variants of different heights into
    // one box measures the variant SPREAD as if it were one crown's shape, and
    // it inflated this number by ~15 % when it was first reported.
    const auto ceiling = static_cast<float>(config::CROWN_ASPECT_MAX);
    for (const FloraSpecies s : ALL) {
        if (!is_canopy_tree(s)) continue;
        const SpeciesParams& sp = species_params(s);
        // Conifers are exempt as a property of their SILHOUETTE BRIEF (a cone
        // or spire is meant to be narrow), not as a species list — so nobody
        // later claims the exemption for a broadleaf that happens to be tall.
        if (sp.envelope == CrownEnvelope::Cone) continue;
        const uint32_t leaf = pack(sp.foliage_color);
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const FloraMesh m = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            float w = 0.0f, lo = 1e9f, hi = -1e9f;
            auto take = [&](const platform::Vertex& vx) {
                w = std::max(w, std::sqrt(vx.position.x * vx.position.x
                                          + vx.position.z * vx.position.z));
                lo = std::min(lo, vx.position.y);
                hi = std::max(hi, vx.position.y);
            };
            for (const platform::Vertex& vx : m.cards.vertices) take(vx);
            for (const platform::Vertex& vx : m.wood.vertices) {
                if (vx.color_rgba == leaf) take(vx);
            }
            REQUIRE(w > 0.1f);
            CHECK((hi - lo) / (2.0f * w) <= ceiling);
        }
    }
}

TEST_CASE("flora: crown width has a FLOOR, not only a ceiling") {
    // The §3.7 pattern again — a rule stated in full and implemented in half.
    // Design's crown widths are BANDS (oak 10-16 m, birch 5-7 m) and only the
    // maximum was ever asserted, so the birch drifted to 3.6-4.5 m — a third
    // narrower than its brief — without a single test noticing.
    //
    // Checked at NOMINAL size only. It cannot be a per-instance invariant:
    // design's own maturity tiers scale trees from x0.4 to x1.5, which takes
    // crowns far outside any band by construction and on purpose.
    struct Band { FloraSpecies s; float lo; float hi; };
    const Band bands[] = {
        {FloraSpecies::DaleOak, 10.0f, 16.0f},
        {FloraSpecies::RiverBirch, 5.0f, 7.0f},
    };
    for (const Band& b : bands) {
        const SpeciesParams& sp = species_params(b.s);
        // The nominal-height variant: the band describes the species, and the
        // variant spread around it is the intended variation.
        float widest = 0.0f;
        const float nominal = species_nominal_height(b.s);
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const FloraMesh m = build_flora_mesh(b.s, v, FloraShape{}, FloraLod::Full);
            float top = 0.0f, w = 0.0f;
            for (const platform::Vertex& vx : m.cards.vertices) {
                top = std::max(top, vx.position.y);
                w = std::max(w, std::sqrt(vx.position.x * vx.position.x
                                          + vx.position.z * vx.position.z));
            }
            // Take the variant closest to nominal height.
            if (std::fabs(top - nominal) < nominal * 0.12f) {
                widest = std::max(widest, w * 2.0f);
            }
        }
        REQUIRE(widest > 0.0f);
        CHECK(widest >= b.lo);
        CHECK(widest <= b.hi);
        CHECK(sp.crown_width_frac > 0.0f);
    }
}

TEST_CASE("flora: crown shyness actually narrows the crown") {
    FloraShape shy;
    shy.shyness = 0.5f;
    shy.shy_dir = {1.0f, 0.0f};
    const FloraMesh plain =
        build_flora_mesh(FloraSpecies::DaleOak, 1, FloraShape{}, FloraLod::Full);
    const FloraMesh pulled =
        build_flora_mesh(FloraSpecies::DaleOak, 1, shy, FloraLod::Full);
    auto reach_plus_x = [](const FloraMesh& f) {
        float r = 0.0f;
        for (const MeshData* m : {&f.wood, &f.cards}) {
            for (const platform::Vertex& v : m->vertices) r = std::max(r, v.position.x);
        }
        return r;
    };
    CHECK(reach_plus_x(pulled) < reach_plus_x(plain));
}

TEST_CASE("flora: meshes are well-formed") {
    for (const FloraSpecies s : ALL) {
        const FloraMesh f = build_flora_mesh(s, 5, FloraShape{}, FloraLod::Full);
        for (const MeshData* m : {&f.wood, &f.cards}) {
            REQUIRE(m->indices.size() % 3 == 0);
            for (const uint32_t i : m->indices) {
                REQUIRE(i < m->vertices.size());
            }
            for (const platform::Vertex& v : m->vertices) {
                const float len = std::sqrt(v.normal.x * v.normal.x
                                            + v.normal.y * v.normal.y
                                            + v.normal.z * v.normal.z);
                CHECK(len == doctest::Approx(1.0f).epsilon(0.01));
            }
            for (size_t i = 0; i + 2 < m->indices.size(); i += 3) {
                CHECK(triangle_area(*m, i) > 1e-7f);
            }
        }
        CHECK(f.wood.indices.size() > 0);
    }
}

TEST_CASE("flora: trunk buries itself (root flare below origin)") {
    // GROUND_SINK_FRAC 0.12 cannot cover 0.84 m of ground drop across a 1.2 m
    // trunk on TREE_SLOPE_MAX — the flare does it in geometry instead.
    for (const FloraSpecies s : ALL) {
        if (!is_canopy_tree(s) && s != FloraSpecies::Snag) continue;
        const FloraMesh m = build_flora_mesh(s, 2, FloraShape{}, FloraLod::Full);
        float lowest = 1e9f;
        for (const platform::Vertex& v : m.wood.vertices) {
            lowest = std::min(lowest, v.position.y);
        }
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

TEST_CASE("flora: wind phase differs between neighbours") {
    // A stand whose trees share a phase does not ripple, it pulses as one
    // object. Phase is derived from POSITION, not from the 12 variants, so
    // adjacent trees are never in lockstep.
    std::vector<math::ScatterInstance> inst;
    for (int i = 0; i < 16; ++i) {
        inst.push_back({{static_cast<float>(i) * 13.0f, 0.0f, 4.0f},
                        0.0f, 1.0f, math::ScatterSpecies::OakTree});
    }
    const std::vector<FloraShape> shapes = analyse_neighbourhood(inst, inst.size());
    int distinct = 0;
    for (size_t i = 1; i < shapes.size(); ++i) {
        if (std::fabs(shapes[i].wind_phase - shapes[i - 1].wind_phase) > 1e-4f) {
            ++distinct;
        }
    }
    CHECK(distinct >= 14);
    for (const FloraShape& sh : shapes) {
        CHECK(sh.wind_phase >= 0.0f);
        CHECK(sh.wind_phase <= 1.0f);
    }
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

// --- Leaf cards ------------------------------------------------------------

TEST_CASE("cards: the foliage vertex channel map is honoured") {
    // render's vs_foliage/fs_foliage contract. Getting this wrong does not
    // crash and does not fail any geometric test — it just makes the canopy
    // move or shade wrongly, which is why it is asserted here.
    //   r = sway weight (0 at the attachment, 1 at the free edge)
    //   g = per-instance phase           b = per-card value jitter
    //   a = sky visibility (render's channel — flora leaves it at 255)
    FloraShape sh;
    sh.wind_phase = 0.375f;
    const FloraMesh m =
        build_flora_mesh(FloraSpecies::DaleOak, 4, sh, FloraLod::Full);
    REQUIRE(m.cards.vertices.size() >= 4);
    REQUIRE(m.cards.vertices.size() % 4 == 0);

    const auto expect_g = static_cast<uint8_t>(0.375f * 255.0f + 0.5f);
    uint8_t sway_min = 255;
    uint8_t sway_max = 0;
    int gradient_cards = 0;
    for (size_t i = 0; i < m.cards.vertices.size(); i += 4) {
        const uint32_t c0 = m.cards.vertices[i].color_rgba;
        uint8_t card_min = 255;
        uint8_t card_max = 0;
        for (size_t k = 0; k < 4; ++k) {
            const uint32_t c = m.cards.vertices[i + k].color_rgba;
            // b and g are PER CARD: identical on all of the card's vertices.
            // Per-vertex b would turn the value jitter into a gradient across
            // the card, which is not what a jitter is.
            CHECK(chan_b(c) == chan_b(c0));
            CHECK(chan_g(c) == expect_g);
            // a is render's sky-visibility channel and flora does not touch it.
            CHECK(chan_a(c) == 255u);
            card_min = std::min(card_min, chan_r(c));
            card_max = std::max(card_max, chan_r(c));
        }
        if (card_max > card_min) ++gradient_cards;
        sway_min = std::min(sway_min, card_min);
        sway_max = std::max(sway_max, card_max);
    }
    // Sway must be a FIELD, not a constant: a card whose four corners share one
    // weight translates rigidly and reads as a flag rather than as foliage
    // bending about its branch.
    CHECK(gradient_cards * 4 >= static_cast<int>(m.cards.vertices.size() / 2));
    CHECK(sway_max >= 240u); // the outer crown reaches the full sway
    CHECK(sway_min <= 200u); // and the inner crown does not
}

TEST_CASE("cards: no card is too small to read (no detached scraps)") {
    // Envelope containment shrinks a card to fit. Where the envelope is narrow
    // — the bottom of a birch's vase, the tip of a cone — that shrinking runs
    // past the point where a card can join the crown mass, and the result
    // renders as a detached scrap hanging under the crown. Measured in the
    // first card frame; the floor is a FRACTION of the crown radius so it
    // scales with maturity.
    FloraShape sapling;
    sapling.maturity = 0.4f;
    const FloraShape shapes[] = {FloraShape{}, sapling};
    for (const FloraSpecies s : ALL) {
        if (!has_leaf_cards(s)) continue;
        const float crown_r = species_crown_radius(s);
        for (const FloraShape& sh : shapes) {
            for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
                const FloraMesh m = build_flora_mesh(s, v, sh, FloraLod::Full);
                for (size_t i = 0; i < m.cards.vertices.size(); i += 4) {
                    const glm::vec3 a = m.cards.vertices[i].position;
                    const glm::vec3 c = m.cards.vertices[i + 2].position;
                    // Diagonal of the quad -> half-width, via the card aspect.
                    const float half_diag = glm::length(c - a) * 0.5f;
                    CHECK(half_diag >= 0.2f * crown_r * sh.maturity);
                }
            }
        }
    }
}

TEST_CASE("cards: only the intended species carry them") {
    // Design §5: TREE FOLIAGE is cards; trunks, branches, bushes, logs and
    // snags stay solid hard-edged meshes.
    // THE CONIFER MOVED, AND THE EXPERIMENT IS WHY. The pine was deliberately
    // held on solid cone tiers for one stage so that a single verification frame
    // would carry both treatments side by side and ANSWER whether needles need
    // cards, instead of the answer being guessed. The user's verdict on that
    // frame was «елки просто юбки большие». The experiment ran, it returned a
    // result, and this assertion is the result — not a relaxation.
    CHECK(has_leaf_cards(FloraSpecies::DaleOak));
    CHECK(has_leaf_cards(FloraSpecies::RiverBirch));
    CHECK(has_leaf_cards(FloraSpecies::ValeWillow));
    CHECK(has_leaf_cards(FloraSpecies::HighlandPine));
    CHECK_FALSE(has_leaf_cards(FloraSpecies::Bush));
    CHECK_FALSE(has_leaf_cards(FloraSpecies::BigBush));
    CHECK_FALSE(has_leaf_cards(FloraSpecies::Snag));
    CHECK_FALSE(has_leaf_cards(FloraSpecies::FallenLog));
    CHECK_FALSE(has_leaf_cards(FloraSpecies::Deadfall));

    for (const FloraSpecies s : ALL) {
        const FloraMesh m = build_flora_mesh(s, 1, FloraShape{}, FloraLod::Full);
        CHECK(m.cards.vertices.empty() == !has_leaf_cards(s));
        // The Silhouette LOD keeps a solid shell at every species: at that
        // range the outline is the whole information and a cutout buys only
        // shimmer and overdraw.
        const FloraMesh sil =
            build_flora_mesh(s, 1, FloraShape{}, FloraLod::Silhouette);
        CHECK(sil.cards.vertices.empty());
    }
}

TEST_CASE("cards: winter strips deciduous foliage and keeps the skeleton") {
    // LANDSCAPE §5.11 — winter costs ONE boolean, and the already-generated
    // skeleton IS the bare tree.
    const FloraMesh oak_w = build_flora_mesh(FloraSpecies::DaleOak, 2, FloraShape{},
                                             FloraLod::Full, FloraSeason::Winter);
    CHECK(oak_w.cards.vertices.empty());
    CHECK(oak_w.wood.vertices.size() > 0);

    // Summer and autumn are the SAME GEOMETRY: the card stores an atlas tile,
    // the atlas stores the colour. That identity is what makes a season change
    // a texture swap rather than a world rebuild, so it is asserted, not
    // assumed.
    const FloraMesh s_ = build_flora_mesh(FloraSpecies::DaleOak, 2, FloraShape{},
                                          FloraLod::Full, FloraSeason::Summer);
    const FloraMesh a_ = build_flora_mesh(FloraSpecies::DaleOak, 2, FloraShape{},
                                          FloraLod::Full, FloraSeason::Autumn);
    REQUIRE(s_.cards.vertices.size() == a_.cards.vertices.size());
    for (size_t i = 0; i < s_.cards.vertices.size(); ++i) {
        CHECK(s_.cards.vertices[i].uv.x == doctest::Approx(a_.cards.vertices[i].uv.x));
        CHECK(s_.cards.vertices[i].color_rgba == a_.cards.vertices[i].color_rgba);
    }
}

TEST_CASE("cards: every uv lands inside a real atlas tile") {
    const LeafAtlas atlas = generate_leaf_atlas();
    const float tile_u = 1.0f / static_cast<float>(LEAF_ATLAS_SHAPES);
    const float tile_v = 1.0f / static_cast<float>(LEAF_ATLAS_TONES);
    for (const FloraSpecies s : ALL) {
        if (!has_leaf_cards(s)) continue;
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const FloraMesh m = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            for (const platform::Vertex& vx : m.cards.vertices) {
                CHECK(vx.uv.x >= 0.0f);
                CHECK(vx.uv.x <= 1.0f);
                CHECK(vx.uv.y >= 0.0f);
                CHECK(vx.uv.y <= 1.0f);
                // A card never straddles two tiles: both of its u values sit in
                // the same column and both v values in the same row.
                const int col = static_cast<int>(vx.uv.x / tile_u);
                const int row = static_cast<int>(vx.uv.y / tile_v);
                CHECK(col < static_cast<int>(LEAF_ATLAS_SHAPES));
                CHECK(row < static_cast<int>(LEAF_ATLAS_TONES));
            }
        }
    }
    CHECK(atlas.width == LEAF_ATLAS_TILE_PX * LEAF_ATLAS_SHAPES);
    CHECK(atlas.height == LEAF_ATLAS_TILE_PX * LEAF_ATLAS_TONES);
}

TEST_CASE("atlas: generation is deterministic and alpha is binary") {
    const LeafAtlas a = generate_leaf_atlas(32, FloraSeason::Summer);
    const LeafAtlas b = generate_leaf_atlas(32, FloraSeason::Summer);
    REQUIRE(a.pixels.size() == b.pixels.size());
    CHECK(a.pixels == b.pixels);
    for (size_t i = 3; i < a.pixels.size(); i += 4) {
        // The material is an alpha TEST at 640x360 under a 64-colour palette
        // post: a soft edge becomes dither, i.e. noise on few-pixel geometry.
        CHECK((a.pixels[i] == 0u || a.pixels[i] == 255u));
    }
}

TEST_CASE("atlas: mostly opaque body, ragged eroded edge, a few real gaps") {
    // THE MEASUREMENT THAT DECIDES THE MASK (docs/specs/flora.md §3.10). The
    // reference crowns are 79-86 % leaf in the interior and only their outer
    // ~25 % of radius reaches 16-28 % sky. "Porous everywhere" was measured and
    // REFUTED — building even lace would miss the reference AND fall under
    // render's ~0.31 m mask-feature floor, so it would be invisible in the
    // direct view and alias in the shadow map.
    //
    // The two properties are separated on purpose, because a naive
    // coverage-by-radius metric cannot tell them apart (it reports a wedge's
    // narrow end as porosity):
    //   1) ENCLOSED GAPS — transparent texels NOT reachable from the tile
    //      border. This is the literal see-through, and it must stay a few per
    //      cent, not a lace.
    //   2) EDGE EROSION — the outline is bitten and lobed rather than a clean
    //      ellipse, measured as an isoperimetric ratio. This is where the
    //      porosity budget actually goes.
    const uint32_t px = 64;
    const LeafAtlas atlas = generate_leaf_atlas(px, FloraSeason::Summer);
    for (uint32_t shape_i = 0; shape_i < LEAF_ATLAS_SHAPES; ++shape_i) {
        auto solid = [&](uint32_t x, uint32_t y) {
            return atlas.pixels[(static_cast<size_t>(y) * atlas.width
                                 + shape_i * px + x) * 4u + 3u] != 0u;
        };
        // Flood-fill the transparent texels that touch the tile border.
        std::vector<uint8_t> outside(static_cast<size_t>(px) * px, 0u);
        std::vector<uint32_t> queue;
        auto push = [&](uint32_t x, uint32_t y) {
            const size_t i = static_cast<size_t>(y) * px + x;
            if (outside[i] != 0u || solid(x, y)) return;
            outside[i] = 1u;
            queue.push_back(y * px + x);
        };
        for (uint32_t i = 0; i < px; ++i) {
            push(i, 0);
            push(i, px - 1);
            push(0, i);
            push(px - 1, i);
        }
        for (size_t head = 0; head < queue.size(); ++head) {
            const uint32_t x = queue[head] % px;
            const uint32_t y = queue[head] / px;
            if (x > 0) push(x - 1, y);
            if (x + 1 < px) push(x + 1, y);
            if (y > 0) push(x, y - 1);
            if (y + 1 < px) push(x, y + 1);
        }
        size_t body = 0, gaps = 0, perimeter = 0;
        for (uint32_t y = 0; y < px; ++y) {
            for (uint32_t x = 0; x < px; ++x) {
                if (outside[static_cast<size_t>(y) * px + x] != 0u) continue;
                ++body;
                if (!solid(x, y)) ++gaps;
            }
        }
        for (uint32_t y = 0; y < px; ++y) {
            for (uint32_t x = 0; x < px; ++x) {
                if (!solid(x, y)) continue;
                // Digital perimeter = exposed EDGES, not boundary pixels: a
                // one-texel spike then costs four, which is the whole point.
                perimeter += (x == 0 || !solid(x - 1, y)) ? 1u : 0u;
                perimeter += (x + 1 == px || !solid(x + 1, y)) ? 1u : 0u;
                perimeter += (y == 0 || !solid(x, y - 1)) ? 1u : 0u;
                perimeter += (y + 1 == px || !solid(x, y + 1)) ? 1u : 0u;
            }
        }
        REQUIRE(body > 200);
        const double gap_frac = static_cast<double>(gaps) / static_cast<double>(body);
        // Mostly opaque: a handful of gaps, never lace.
        CHECK(gap_frac <= 0.08);
        // ...and the ragged edge is real. A smooth digital ellipse scores
        // ~1.62 on this ratio (64 / 4*pi^2, and it is scale-free); anything
        // near that means somebody smoothed the outline into a blob, which is
        // the failure this number exists to catch.
        const double area = static_cast<double>(body - gaps);
        const double ragged = static_cast<double>(perimeter)
            * static_cast<double>(perimeter) / (4.0 * 3.14159265 * area);
        CHECK(ragged >= 2.5);
    }
    // The broadleaf default must carry at least one gap you can see sky
    // through: that is the user's «сквозь листву можно смотреть», expressed at
    // card scale, and it is not optional.
    {
        size_t transparent_inside = 0;
        const uint32_t mid = px / 2;
        for (uint32_t y = mid - 12; y < mid + 12; ++y) {
            for (uint32_t x = mid - 12; x < mid + 12; ++x) {
                if (atlas.pixels[(static_cast<size_t>(y) * atlas.width + x) * 4u + 3u]
                    == 0u) {
                    ++transparent_inside;
                }
            }
        }
        CHECK(transparent_inside >= 20);
    }
}

TEST_CASE("atlas: species VALUE ORDER holds in every season") {
    // Design §5.11's acceptance test for any season anyone ever proposes.
    // §1.5 separates our species by VALUE, not hue — pale birch, mid oak, dark
    // pine — so a palette that turns oak and birch into two similar warm
    // mid-values destroys the read at SILHOUETTE_MIN_PX in exactly one season.
    const FloraSeason seasons[] = {FloraSeason::Summer, FloraSeason::Autumn,
                                   FloraSeason::Winter};
    for (const FloraSeason s : seasons) {
        const float birch = luminance(leaf_tone_color(LeafTone::BirchLight, s));
        const float oak = luminance(leaf_tone_color(LeafTone::OakMid, s));
        const float willow = luminance(leaf_tone_color(LeafTone::WillowDark, s));
        const float conifer = luminance(leaf_tone_color(LeafTone::ConiferDark, s));
        CHECK(birch > oak);
        CHECK(oak > willow);
        CHECK(oak > conifer);
        // A tone band must actually SPAN values, or "several values of one
        // foliage in one crown" is a claim rather than a property.
        CHECK(luminance(leaf_tone_color(LeafTone::OakSunlit, s))
              > luminance(leaf_tone_color(LeafTone::OakDeep, s)) * 1.4f);
    }
    // Winter strips deciduous foliage and keeps needles.
    CHECK_FALSE(leaf_tone_has_foliage(LeafTone::OakMid, FloraSeason::Winter));
    CHECK(leaf_tone_has_foliage(LeafTone::ConiferDark, FloraSeason::Winter));
    CHECK(leaf_tone_has_foliage(LeafTone::OakMid, FloraSeason::Autumn));
}

// ===========================================================================
// THE THREE INVARIANTS THE 09.08.2026 REJECTION EXISTS TO CREATE.
// Each ships with the case it must REJECT, and that case is asserted to FAIL
// (Rule 30). An invariant nothing fails is a description, not an invariant —
// and this zone has already shipped a suite of 31 000 assertions that were all
// green over a tree with no leaves.
// ===========================================================================

namespace {

/// Centroid and corner reach of card `i` (six vertices, two triangles).
struct Card {
    glm::vec3 centre{0.0f};
    float reach = 0.0f;
};

std::vector<Card> cards_of(const MeshData& m) {
    std::vector<Card> out;
    for (size_t i = 0; i + 6 <= m.vertices.size(); i += 6) {
        Card c;
        for (size_t k = 0; k < 6; ++k) c.centre += m.vertices[i + k].position;
        c.centre /= 6.0f;
        for (size_t k = 0; k < 6; ++k) {
            c.reach = std::max(c.reach, glm::length(m.vertices[i + k].position - c.centre));
        }
        out.push_back(c);
    }
    return out;
}

/// Distance from `p` to the nearest point ON the wood SURFACE, approximated by
/// each triangle's vertices, edge midpoints and centroid.
///
/// The first version measured to the nearest wood VERTEX, which is simpler and
/// which I documented as pessimistic — and it duly produced a false failure: one
/// conifer spray sitting ON the leader at 94 % of tree height measured 1.73 m
/// away, because the trunk is built from 7 segments over 31 m and its vertex
/// rings are 4.4 m apart. The card was touching wood; the ruler had no marks
/// there. Sampling the surface rather than its corners costs one loop and
/// removes a whole class of false positive. THE THRESHOLD DID NOT MOVE — this
/// is a better instrument, not a relaxed rule.
float gap_to_wood(const MeshData& wood, glm::vec3 p) {
    float best = 1e9f;
    for (const platform::Vertex& v : wood.vertices) {
        best = std::min(best, glm::length(v.position - p));
    }
    for (size_t i = 0; i + 2 < wood.indices.size(); i += 3) {
        const glm::vec3 a = wood.vertices[wood.indices[i]].position;
        const glm::vec3 b = wood.vertices[wood.indices[i + 1]].position;
        const glm::vec3 c = wood.vertices[wood.indices[i + 2]].position;
        best = std::min(best, glm::length((a + b + c) / 3.0f - p));
        best = std::min(best, glm::length((a + b) * 0.5f - p));
        best = std::min(best, glm::length((b + c) * 0.5f - p));
        best = std::min(best, glm::length((a + c) * 0.5f - p));
    }
    return best;
}

} // namespace

TEST_CASE("REJECTION 1: every leaf cluster hangs off a branch that exists") {
    // «дубы имеют случайно наложенные листья, не прикрепляющиеся к ветвям» —
    // the user, 09.08.2026, and he was describing the code exactly:
    // scatter_envelope_clusters() distributed the crown over the envelope
    // "independently of the skeleton". Measured on the geometry he rejected:
    // an oak's average leaf card sat 2.60 m from the nearest wood and the worst
    // sat 6.91 m.
    //
    // Space colonization makes this unrepresentable rather than merely
    // forbidden: an attraction point survives only until a node comes within
    // the kill distance of it, so foliage placed on consumed points IS on a
    // branch. The threshold is expressed in units of the card's OWN size,
    // because that is what "attached" means visually — a card whose centre is
    // within its own reach of the wood overlaps that wood on screen.
    // 1.5, and the margin is real: measured over every species and variant the
    // gap is 0.19-0.28 card-reaches at the median and 1.37 at the very worst.
    constexpr float MAX_GAP_IN_CARD_REACHES = 1.5f;
    // Measured across every species and variant: the median gap is 0.19-0.28
    // card-reaches and the very worst single card is 1.37.
    constexpr double MEAN_GAP_MAX = 0.60;
    for (const FloraSpecies s : ALL) {
        if (!has_leaf_cards(s)) continue;
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const FloraMesh f = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            REQUIRE_FALSE(f.cards.vertices.empty());
            double sum = 0.0;
            size_t n = 0;
            for (const Card& c : cards_of(f.cards)) {
                const float gap = gap_to_wood(f.wood, c.centre);
                CHECK(gap <= c.reach * MAX_GAP_IN_CARD_REACHES);
                sum += gap / std::max(c.reach, 0.01f);
                ++n;
            }
            // The per-card ceiling catches one stray cluster; the MEAN is what
            // catches a whole crown that has drifted off its skeleton, and the
            // whole crown is what the user was looking at. Two clauses, because
            // this zone's signature failure is a rule stated in full and
            // implemented in half (flora.md §3.7).
            CHECK(sum / static_cast<double>(std::max<size_t>(n, 1)) <= MEAN_GAP_MAX);
        }
    }

    // --- THE CONTROL: the geometry the user rejected must FAIL this test. ----
    // Rebuilt rather than described, and rebuilt HONESTLY: the rejected trees
    // did not have a dense crown skeleton with loose foliage over it. Three of
    // the four species had NO crown skeleton at all — §3.7 defect 3 measured
    // primary-branch diameters of 0.168 m (birch) and 0.317 m (pine) against a
    // 0.35 m shadow floor, so every branch terminated instantly and what was
    // left was a bole with a cloud of leaves around it. That is the control: a
    // bare trunk, plus clusters distributed over the crown envelope on the
    // golden angle, which is exactly what scatter_envelope_clusters() did.
    {
        const FloraSpecies s = FloraSpecies::DaleOak;
        const SpeciesParams& sp = species_params(s);
        const float height = species_nominal_height(s);
        const float base = height * sp.crown_base_frac;
        const float crown_r = height * sp.crown_width_frac * 0.5f;
        const float trunk_r = height * sp.trunk_radius_frac;
        MeshData bole;
        for (int seg = 0; seg <= 24; ++seg) {
            const float y = height * static_cast<float>(seg) / 24.0f;
            for (int k = 0; k < 5; ++k) {
                const float a = 6.2831853f * static_cast<float>(k) / 5.0f;
                bole.vertices.push_back({{std::cos(a) * trunk_r, y, std::sin(a) * trunk_r},
                                         {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, 0xFFFFFFFFu});
            }
        }
        double sum = 0.0;
        size_t total = 0;
        const int count = sp.cluster_count;
        for (int i = 0; i < count; ++i) {
            const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
            const float y = base + (height - base) * std::pow(u, 0.8f);
            const float env = crown_r
                * (0.30f + 0.70f * std::sin((y - base) / (height - base) * 3.14159265f));
            const float az = 2.39996323f * static_cast<float>(i);
            const float rf = 0.68f * (0.30f + 0.70f * std::fabs(std::sin(az * 1.7f)));
            const glm::vec3 at{std::cos(az) * env * rf, y, std::sin(az) * env * rf};
            const float reach = crown_r * sp.cluster_radius_frac;
            ++total;
            sum += gap_to_wood(bole, at) / reach;
        }
        REQUIRE(total > 0);
        // The control fails on the MEAN, and that is the honest way round: the
        // old scatter also put a good number of clusters near the trunk axis
        // (that was the drill-bit defect of §3.7.5), so a per-cluster pass rate
        // understates it. What the user saw, and what was measured on the
        // rejected oak, was a whole crown averaging 2.60 m off its wood.
        CHECK(sum / static_cast<double>(total) > MEAN_GAP_MAX);
    }
}

TEST_CASE("REJECTION 2: the conifer is a stack of whorls, not a skirt") {
    // «елки просто юбки большие». A skirt is a SURFACE OF REVOLUTION: whatever
    // its profile, it is SMOOTH down its whole length. A whorled conifer is
    // layers — a whorl is a YEAR, the years are not evenly spaced, and the older
    // whorls are self-pruned — so its foliage varies sharply with height.
    //
    // MEASURED AS VERTICAL ROUGHNESS, and the first two metrics I wrote for this
    // did not discriminate. Counting "rows with gaps" failed twice over: against
    // the row's own occupied span, a bare stick between two whorls scored a
    // perfect 1.0 for being a stick; against the envelope, a smooth analytic
    // cone scored GAPPIER than the real pine, because the control's profile and
    // the generator's envelope were different shapes. Both versions passed. Only
    // running them against a known-bad object showed they were measuring
    // nothing, which is the whole of Rule 30 in one afternoon.
    //
    // Roughness is the quantity that cannot be faked by a solid: mean absolute
    // change in row fill from one height to the next. Measured — pine 0.150 to
    // 0.232 across the twelve variants, a tapering cone 0.037, a solid of
    // revolution following this generator's own envelope 0.034. Four to five
    // times of separation with no overlap.
    constexpr int ROWS = 24;
    constexpr int COLS = 48;
    constexpr float ROUGHNESS_MIN = 0.10f;

    auto profile = [](const FloraMesh& f, float base, float top, float half_w) {
        // The denominator is the REAL envelope, taken from the generator's own
        // envelope_radius_at() rather than restated here. Restating it went
        // stale within the hour once the cone gained its 0.18 apex floor
        // (design §5.2 wants the tip >= 1.5 m wide), and a test that keeps its
        // own copy of a shape measures the copy.
        const CrownVolume env_v{CrownEnvelope::Cone, base, top, half_w};
        std::vector<std::vector<uint8_t>> hit(ROWS, std::vector<uint8_t>(COLS, 0));
        auto mark = [&](glm::vec3 p) {
            const int r = static_cast<int>((p.y - base) / std::max(top - base, 1e-3f)
                                           * static_cast<float>(ROWS));
            const int c = static_cast<int>((p.x + half_w)
                                           / std::max(2.0f * half_w, 1e-3f)
                                           * static_cast<float>(COLS));
            if (r >= 0 && r < ROWS && c >= 0 && c < COLS) {
                hit[static_cast<size_t>(r)][static_cast<size_t>(c)] = 1;
            }
        };
        for (const MeshData* m : {&f.cards, &f.wood}) {
            for (size_t i = 0; i + 2 < m->indices.size(); i += 3) {
                const glm::vec3 a = m->vertices[m->indices[i]].position;
                const glm::vec3 b = m->vertices[m->indices[i + 1]].position;
                const glm::vec3 c = m->vertices[m->indices[i + 2]].position;
                // ADAPTIVE barycentric sweep. A fixed 6x6 lattice was the first
                // version and it silently broke the control: a cone's side
                // triangle is tall and thin and its two base vertices share a
                // height, so 28 samples landed on 7 distinct rows and the solid
                // cone measured as four empty rows in five. The sampler must be
                // denser than the grid it samples INTO, which is a property of
                // the triangle rather than a constant.
                const float longest = std::max(
                    {glm::length(b - a), glm::length(c - a), glm::length(c - b)});
                const float cell = std::max((top - base) / static_cast<float>(ROWS),
                                            2.0f * half_w / static_cast<float>(COLS));
                const int steps = std::clamp(
                    static_cast<int>(longest / std::max(cell, 1e-3f)) + 2, 4, 40);
                for (int u = 0; u <= steps; ++u) {
                    for (int w = 0; u + w <= steps; ++w) {
                        const float fu = static_cast<float>(u) / static_cast<float>(steps);
                        const float fw = static_cast<float>(w) / static_cast<float>(steps);
                        mark(a * (1.0f - fu - fw) + b * fu + c * fw);
                    }
                }
            }
        }
        std::vector<float> fill(ROWS, 0.0f);
        for (int r = 0; r < ROWS; ++r) {
            const float y = base + (top - base) * (static_cast<float>(r) + 0.5f)
                    / static_cast<float>(ROWS);
            const float expect = std::max(
                2.0f, static_cast<float>(COLS) * envelope_radius_at(env_v, y)
                          / std::max(half_w, 1e-3f));
            int on = 0;
            for (int c = 0; c < COLS; ++c) on += hit[static_cast<size_t>(r)][static_cast<size_t>(c)];
            fill[static_cast<size_t>(r)] = static_cast<float>(on) / expect;
        }
        return fill;
    };
    auto roughness = [](const std::vector<float>& fill) {
        double sum = 0.0;
        size_t n = 0;
        for (size_t i = 1; i < fill.size(); ++i) {
            if (fill[i] <= 0.0f && fill[i - 1] <= 0.0f) continue;
            sum += std::fabs(static_cast<double>(fill[i]) - fill[i - 1]);
            ++n;
        }
        return static_cast<float>(sum / static_cast<double>(std::max<size_t>(n, 1)));
    };

    const FloraSpecies s = FloraSpecies::HighlandPine;
    const SpeciesParams& sp = species_params(s);
    for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
        const FloraMesh f = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
        float top = 0.0f;
        float half_w = 0.0f;
        for (const MeshData* m : {&f.wood, &f.cards}) {
            for (const platform::Vertex& vv : m->vertices) {
                top = std::max(top, vv.position.y);
                half_w = std::max(half_w, std::fabs(vv.position.x));
            }
        }
        const std::vector<float> fill =
            profile(f, top * sp.crown_base_frac, top, std::max(half_w, 0.5f));
        int occupied = 0;
        for (const float x : fill) {
            if (x > 0.0f) ++occupied;
        }
        REQUIRE(occupied >= ROWS / 2); // the crown must exist at all
        CHECK(roughness(fill) >= ROUGHNESS_MIN);
    }

    // --- THE CONTROLS: two skirts, and BOTH must fail. ----------------------
    // Rule 30 names the cone by name, because three shape invariants were once
    // shipped in one evening that a cone passed. Two are used here rather than
    // one: a plain tapering cone, and — the fairer and harsher control — a solid
    // of revolution that follows THIS GENERATOR'S OWN envelope exactly, which is
    // the shape build_cone_tiers() drew and the shape the user called «юбка».
    // A control that differs from the thing under test in some other way as well
    // is not a control, it is a second experiment.
    {
        const float base = 12.0f;
        const float top = 33.0f;
        const float r = 4.4f;
        FloraMesh taper;
        for (int i = 0; i < 16; ++i) {
            const float a0 = 6.2831853f * static_cast<float>(i) / 16.0f;
            const float a1 = 6.2831853f * static_cast<float>(i + 1) / 16.0f;
            tri(taper.wood, {std::cos(a0) * r, base, std::sin(a0) * r}, {0.0f, top, 0.0f},
                {std::cos(a1) * r, base, std::sin(a1) * r}, 0xFFFFFFFFu);
        }
        CHECK_FALSE(roughness(profile(taper, base, top, r)) >= ROUGHNESS_MIN);

        FloraMesh skirt;
        const CrownVolume ev{CrownEnvelope::Cone, base, top, r};
        for (int k = 0; k < 40; ++k) {
            const float y0 = base + (top - base) * static_cast<float>(k) / 40.0f;
            const float y1 = base + (top - base) * static_cast<float>(k + 1) / 40.0f;
            const float r0 = envelope_radius_at(ev, y0);
            const float r1 = envelope_radius_at(ev, y1);
            for (int i = 0; i < 20; ++i) {
                const float a0 = 6.2831853f * static_cast<float>(i) / 20.0f;
                const float a1 = 6.2831853f * static_cast<float>(i + 1) / 20.0f;
                quad(skirt.wood, {std::cos(a0) * r0, y0, std::sin(a0) * r0},
                     {std::cos(a0) * r1, y1, std::sin(a0) * r1},
                     {std::cos(a1) * r1, y1, std::sin(a1) * r1},
                     {std::cos(a1) * r0, y0, std::sin(a1) * r0}, 0xFFFFFFFFu);
            }
        }
        CHECK_FALSE(roughness(profile(skirt, base, top, r)) >= ROUGHNESS_MIN);
    }
}

TEST_CASE("REJECTION 3: no canopy tree is a bare pole with a tuft on top") {
    // «белое дерево выглядит как пальма… как острые пики, но не деревья.»
    // A palm is not a proportion failure — the birch passed CROWN_ASPECT_MAX at
    // 1.02 while reading as a palm, which is exactly why a fourth invariant was
    // needed instead of a fifth attempt at the third one. A palm is a
    // STRUCTURAL fact: the branches all leave the stem at ONE height, so the
    // trunk is bare everywhere else.
    //
    // Measured as the vertical spread of non-axial WOOD — the range of heights
    // at which the tree has material away from its own stem axis — plus the
    // vertical span of the foliage itself. A tree has limbs over a stretch of
    // its bole and a crown with depth; a palm has both at one point.
    //
    // THE FLOOR SITS ABOVE THE ARTEFACT THAT WAS ACTUALLY REJECTED, not just
    // above a synthetic worst case. Design's sharpening of Rule 30, and it is
    // right: a synthetic control is the EASY reject, and a floor placed below
    // every real failure is a description rather than a test. The first version
    // of this invariant shipped with a synthetic palm at 0.06 against a 0.15
    // floor — and it would have PASSED the birch the user rejected, which
    // measured 0.17-0.19.
    //
    // Applying it took a measurement rather than a threshold bump, and the
    // measurement said something useful: **it cannot be applied to the limb-
    // spread clause.** Repaired, the birch measures 0.399-0.442 — but the OAK's
    // smallest variant sits at **0.166**, BELOW the rejected birch. A compact
    // crown on a short tree and a tuft on a tall pole produce the same number
    // from different objects, so no limb-spread floor separates accepted from
    // rejected without failing an accepted species.
    //
    // FOLIAGE SPAN DOES SEPARATE THEM, and by construction rather than by luck.
    // The rejected birch had its crown base at 0.58 of height, so its foliage
    // could not span more than 0.42 of the tree whatever went in it; every
    // accepted species measures 0.49-0.76. A floor at 0.45 therefore rejects the
    // whole CLASS the rejected birch belonged to — any tree whose crown starts
    // above ~0.55 — rather than one instance of it.
    //
    // Measured, this build: limb spread — oak 0.166-0.341, pine 0.240-0.353,
    // birch 0.399-0.442, willow 0.586-0.679, synthetic palm 0.06. Foliage span —
    // pine 0.493-0.505, oak 0.549-0.602, birch 0.555-0.595, willow 0.711-0.758,
    // rejected birch < 0.42 by construction, synthetic palm 0.06.
    // Note what this does NOT assert: that a low branch exists. Space
    // colonization gives apical dominance for free — a seed low on the bole
    // never wins an attractor against one higher up — and a clean lower bole is
    // correct for every species in this catalog. The invariant is about the
    // CROWN having structure, not about the trunk having twigs.
    constexpr float LIMB_SPREAD_MIN = 0.15f;
    constexpr float FOLIAGE_SPAN_MIN = 0.45f;
    for (const FloraSpecies s : ALL) {
        if (!is_canopy_tree(s)) continue;
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const FloraMesh f = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            float top = 0.0f;
            for (const platform::Vertex& vv : f.wood.vertices) {
                top = std::max(top, vv.position.y);
            }
            const float axial = species_trunk_radius(s) * 1.6f;
            float lo = 1e9f;
            float hi = -1e9f;
            for (const platform::Vertex& vv : f.wood.vertices) {
                const float r = std::sqrt(vv.position.x * vv.position.x
                                          + vv.position.z * vv.position.z);
                if (r <= axial) continue; // still the bole
                lo = std::min(lo, vv.position.y);
                hi = std::max(hi, vv.position.y);
            }
            REQUIRE(hi > lo); // there ARE limbs
            CHECK((hi - lo) / top >= LIMB_SPREAD_MIN);
            // Second clause, and it is the stronger one: how much of the tree's
            // height the FOLIAGE occupies. A palm's rosette is 5-8 % of the
            // trunk it sits on. This is measured on the built geometry, so a
            // species whose crown drifts small — which is what happened to the
            // birch once already, at a third under its brief with a green suite
            // — cannot hide behind a crown-base fraction that looks right on
            // paper.
            float flo = 1e9f;
            float fhi = -1e9f;
            for (const platform::Vertex& vv : f.cards.vertices) {
                flo = std::min(flo, vv.position.y);
                fhi = std::max(fhi, vv.position.y);
            }
            if (fhi > flo) CHECK((fhi - flo) / top >= FOLIAGE_SPAN_MIN);
        }
    }

    // --- THE CONTROL: a pole with everything at the top must FAIL. -----------
    // Built to be exactly what the rejected birch was — a tapered stem with all
    // its limbs springing from one node near the tip.
    {
        MeshData palm;
        const float h = 19.0f;
        const float top_frac = 0.90f;
        for (int seg = 0; seg < 6; ++seg) {
            const float y0 = h * static_cast<float>(seg) / 6.0f;
            const float y1 = h * static_cast<float>(seg + 1) / 6.0f;
            tri(palm, {-0.25f, y0, 0.0f}, {0.25f, y0, 0.0f}, {0.0f, y1, 0.2f},
                0xFFFFFFFFu);
        }
        for (int i = 0; i < 7; ++i) {
            const float a = 6.2831853f * static_cast<float>(i) / 7.0f;
            tri(palm, {0.0f, h * top_frac, 0.0f},
                {std::cos(a) * 3.0f, h * 0.96f, std::sin(a) * 3.0f},
                {std::cos(a + 0.4f) * 3.0f, h * 0.94f, std::sin(a + 0.4f) * 3.0f},
                0xFFFFFFFFu);
        }
        const float axial = 0.4f;
        float lo = 1e9f;
        float hi = -1e9f;
        for (const platform::Vertex& vv : palm.vertices) {
            const float r = std::sqrt(vv.position.x * vv.position.x
                                      + vv.position.z * vv.position.z);
            if (r <= axial) continue;
            lo = std::min(lo, vv.position.y);
            hi = std::max(hi, vv.position.y);
        }
        REQUIRE(hi > lo);
        // BOTH clauses must reject it, or only one of them is load-bearing and
        // the other is decoration.
        CHECK_FALSE((hi - lo) / h >= LIMB_SPREAD_MIN); // the tuft spans 0.06
        const float tuft_span = h * 0.96f - h * top_frac;
        CHECK_FALSE(tuft_span / h >= FOLIAGE_SPAN_MIN);
    }
}
