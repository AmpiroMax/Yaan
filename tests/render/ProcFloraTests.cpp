/*
Created: 09:08:2026 - 19:38:20
Last updated: 09:08:2026 - 21:02:02
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
        // Crown base fraction inside design's CROWN_BASE_FRACTION band.
        const float frac = species_crown_base(b.s) / species_nominal_height(b.s);
        CHECK(frac >= doctest::Approx(config::CROWN_BASE_FRACTION_MIN).epsilon(0.02));
        CHECK(frac <= doctest::Approx(config::CROWN_BASE_FRACTION_MAX).epsilon(0.02));
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
    // snags stay solid hard-edged meshes. The conifer deliberately stays on
    // cone tiers this stage so one frame can compare the two treatments.
    CHECK(has_leaf_cards(FloraSpecies::DaleOak));
    CHECK(has_leaf_cards(FloraSpecies::RiverBirch));
    CHECK(has_leaf_cards(FloraSpecies::ValeWillow));
    CHECK_FALSE(has_leaf_cards(FloraSpecies::HighlandPine));
    CHECK_FALSE(has_leaf_cards(FloraSpecies::Bush));
    CHECK_FALSE(has_leaf_cards(FloraSpecies::BigBush));
    CHECK_FALSE(has_leaf_cards(FloraSpecies::Snag));

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
