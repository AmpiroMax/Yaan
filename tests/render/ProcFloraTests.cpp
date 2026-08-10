/*
Created: 09:08:2026 - 19:38:20
Last updated: 10:08:2026 - 11:59:40
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
- 10:08:2026 - 01:59:06: The §5.10 forest-floor cases: snag limb-reach band
  with BOTH neighbouring objects as controls (bare pole = the real rejected
  instance, winter oak = the tree-with-zero-leaves it must not be); snag split
  as one-geometry-two-materials; log ground contact per axial slice with the
  floated copy as control; upper-side moss in patches with the mossless snag
  as the gate control; the maturity draw's full-band distribution (Rule 31)
  with the historic top-60 % defect rebuilt as the control; >= 3 card planes
  per cluster and WORST-AZIMUTH coverage with single-plane and parallel-plane
  controls. INSTRUMENT FIX: cards_of() strode 6 vertices per card over a
  4-vertex-per-card buffer, so every "card" it measured was one and a half
  real cards — the channel-map case one page up had asserted % 4 == 0 all
  along; two green tests held contradictory beliefs about one buffer.
- 10:08:2026 - 02:49:15: Moss-below-grass-band assertion (design's acceptance
  rule) with the grass reference as its own failing control.
- 10:08:2026 - 11:07:33: Margin-richness invariant on design's ORDERING with
  the pre-ruling flat table as the rejected control; the kept-verge-is-not-
  bare-ground clause; the PathClass ordinal mapping pinned (nothing else can
  check it across the DAG).
- 10:08:2026 - 11:14:45: Design's bounds on the moss residual (strictly under
  the dirt weight, moss only); the ordinal-mapping test now says WHY it failed,
  so whoever trips it fixes the mapping rather than the expectation.
- 10:08:2026 - 11:24:00: The edge-floor and kept-verge invariants moved to
  core with clump_field_edged(); tombstones name both clauses so a successor
  can tell 'moved' from 'dropped'.
- 10:08:2026 - 11:59:40: Routing invariant — every ordinal flora_owns() claims
  BUILDS non-empty geometry with real extent, at every LOD, walking the enum by
  VALUE. Control is the old three-tree predicate, which every §5.10/§5.11
  ordinal must fail: that predicate is why the forest floor drew as bare earth.
*/

#include "engine/render/sources/FloraSkeleton.h"
#include "engine/render/sources/FloraEdgeRules.h"
#include "engine/render/sources/FloraField.h"
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
    FloraSpecies::SnagPale, FloraSpecies::StuntedPine, FloraSpecies::MossPatch,
    FloraSpecies::FlowerCarpet, FloraSpecies::FlowerAccent,
    FloraSpecies::FlowerJewel,  FloraSpecies::FlowerUmbel,
    FloraSpecies::Mushroom,     FloraSpecies::PebbleCluster,
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
        const SpeciesParams& sp = species_params(s);
        for (const FloraShape& sh : shapes) {
            for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
                const FloraMesh m = build_flora_mesh(s, v, sh, FloraLod::Full);
                // The floor scales with THIS VARIANT'S built crown, not the
                // species nominal: a wide height band (the stunted pine spans
                // 2x) makes the nominal radius a fiction for small variants.
                float top = 0.0f;
                for (const platform::Vertex& vx : m.wood.vertices) {
                    top = std::max(top, vx.position.y);
                }
                const float crown_r_v = top * sp.crown_width_frac * 0.5f;
                for (size_t i = 0; i < m.cards.vertices.size(); i += 4) {
                    const glm::vec3 a = m.cards.vertices[i].position;
                    const glm::vec3 c = m.cards.vertices[i + 2].position;
                    // Diagonal of the quad -> half-width, via the card aspect.
                    const float half_diag = glm::length(c - a) * 0.5f;
                    CHECK(half_diag >= 0.2f * crown_r_v);
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

/// Centroid and corner reach of one card. A card is FOUR vertices and six
/// indices (emit_leaf_card appends an indexed quad; the card is planar, so its
/// two triangles legally share vertices).
///
/// INSTRUMENT BUG, FIXED 10.08.2026: this helper strode SIX vertices per card
/// on the assumption that a quad is two vertex-owning triangles. It is not, in
/// this one stream — so every "card" this measured was one-and-a-half real
/// cards, misaligned after the first. The channel-map case right above it had
/// asserted % 4 == 0 all along; two tests in one file held contradictory
/// beliefs about the same buffer and both were green, which is why this note
/// is long. Thresholds downstream were re-measured after the fix.
struct Card {
    glm::vec3 centre{0.0f};
    float reach = 0.0f;
};

std::vector<Card> cards_of(const MeshData& m) {
    std::vector<Card> out;
    for (size_t i = 0; i + 4 <= m.vertices.size(); i += 4) {
        Card c;
        for (size_t k = 0; k < 4; ++k) c.centre += m.vertices[i + k].position;
        c.centre /= 4.0f;
        for (size_t k = 0; k < 4; ++k) {
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

// ===========================================================================
// THE §5.10 FOREST FLOOR — sixteen constants, zero consumers, finally OBJECTS
// (10.08.2026). Same Rule 30 discipline: every invariant ships with the case
// it must reject, and where a real rejected instance exists, IT is the
// control — the bare-pole snag this generator used to build, and the floating
// cylinder a log must never be.
// ===========================================================================

namespace {

/// Limb reach measured AGAINST THE LOCAL TRUNK AXIS, not against the origin:
/// vertices are binned by height and measured from their bin's own centroid,
/// so trunk sweep and lean cancel out and a swept bare pole cannot smuggle in
/// reach it does not have. Bins below `y_min` are skipped (the root flare is
/// ground contact, not a limb).
float limb_reach_frac(const MeshData& m, float y_min, float height) {
    constexpr float BIN = 1.0f;
    struct Acc {
        glm::vec3 sum{0.0f};
        int n = 0;
    };
    std::vector<Acc> bins(static_cast<size_t>(height / BIN) + 2);
    auto bin_of = [&](float y) { return static_cast<size_t>((y - y_min) / BIN); };
    for (const platform::Vertex& v : m.vertices) {
        if (v.position.y < y_min || v.position.y > height + 1.0f) continue;
        Acc& a = bins[bin_of(v.position.y)];
        a.sum += v.position;
        ++a.n;
    }
    float worst = 0.0f;
    for (const platform::Vertex& v : m.vertices) {
        if (v.position.y < y_min || v.position.y > height + 1.0f) continue;
        const Acc& a = bins[bin_of(v.position.y)];
        if (a.n < 3) continue;
        const glm::vec3 c = a.sum / static_cast<float>(a.n);
        const glm::vec2 d{v.position.x - c.x, v.position.z - c.z};
        worst = std::max(worst, glm::length(d));
    }
    return worst / std::max(height, 1e-3f);
}

} // namespace

TEST_CASE("floor: a snag is its own object — not a pole, not a winter tree") {
    // Design's model (flora.md §3.4): a snag carries truncated STUBS where the
    // limbs snapped, and what separates it from its two neighbouring objects
    // was MEASURED before the threshold was chosen (Rule 30: which quantity a
    // threshold belongs on is itself a measurement — the first draft put the
    // ceiling on limb REACH, and the data refused it: winter oak reach
    // measures 0.106-0.136 of height against snag 0.072-0.121, overlap at
    // every threshold, because a bin-centroid axis halves an asymmetric
    // crown's apparent reach).
    //
    // The quantity that DOES separate is RAMIFICATION — the count of off-axis
    // wood faces. A snag is a few fat remnants (measured 7-45 across both
    // materials and all variants); a live winter broadleaf is many fine
    // branches (oak 95-179, willow 95-197). Winter BIRCH measures 36-61 —
    // genuinely adjacent to the snag band, and honestly so: a dead birch and
    // a leafless winter birch are near-confusable in the field too. That
    // separation is carried by bark VALUE and the broken top, not by gross
    // limb statistics, and winter is not yet a shipped season; recorded here
    // so a successor knows the ceiling was scoped to oak/willow on purpose.
    constexpr float STUB_REACH_MIN = 0.04f;  // vs the pole (which has none)
    constexpr float STUB_REACH_MAX = 0.16f;  // stubs stay TRUNCATED
    constexpr int OFFAXIS_MIN = 6;
    constexpr int OFFAXIS_MAX = 70;
    auto offaxis_faces = [](const MeshData& m, float y_min, float height) {
        constexpr float BIN = 1.0f;
        struct Acc {
            glm::vec3 sum{0.0f};
            int n = 0;
        };
        std::vector<Acc> bins(static_cast<size_t>(height / BIN) + 2);
        for (const platform::Vertex& v : m.vertices) {
            if (v.position.y < y_min || v.position.y > height + 1.0f) continue;
            Acc& a = bins[static_cast<size_t>((v.position.y - y_min) / BIN)];
            a.sum += v.position;
            ++a.n;
        }
        int cnt = 0;
        for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
            const glm::vec3 mid = (m.vertices[m.indices[i]].position
                                   + m.vertices[m.indices[i + 1]].position
                                   + m.vertices[m.indices[i + 2]].position)
                / 3.0f;
            if (mid.y < y_min || mid.y > height + 1.0f) continue;
            const Acc& bn = bins[static_cast<size_t>((mid.y - y_min) / BIN)];
            if (bn.n < 3) continue;
            const glm::vec3 ax = bn.sum / static_cast<float>(bn.n);
            const glm::vec2 d{mid.x - ax.x, mid.z - ax.z};
            if (glm::length(d) > 0.55f) ++cnt;
        }
        return cnt;
    };
    const FloraSpecies snags[] = {FloraSpecies::Snag, FloraSpecies::SnagPale};
    for (const FloraSpecies s : snags) {
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const FloraMesh f = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            float top = 0.0f;
            for (const platform::Vertex& vx : f.wood.vertices) {
                top = std::max(top, vx.position.y);
            }
            REQUIRE(top > 5.0f);
            const float reach = limb_reach_frac(f.wood, top * 0.25f, top);
            CHECK(reach >= STUB_REACH_MIN);
            CHECK(reach <= STUB_REACH_MAX);
            const int n = offaxis_faces(f.wood, top * 0.25f, top);
            CHECK(n >= OFFAXIS_MIN);
            CHECK(n <= OFFAXIS_MAX);
        }
    }

    // CONTROL 1 — the REAL rejected instance: the bare tapered pole this
    // generator built until 10.08.2026. Must fail both floors. (Straight
    // rather than swept, and that is honest: the bin-centroid metric cancels
    // sweep by construction, so sweep is not what separates them.)
    {
        MeshData pole;
        const float h = 15.0f;
        const float r = 0.36f;
        for (int seg = 0; seg < 5; ++seg) {
            const float y0 = h * static_cast<float>(seg) / 5.0f;
            const float y1 = h * static_cast<float>(seg + 1) / 5.0f;
            const float r0 = r * (1.0f - 0.8f * static_cast<float>(seg) / 5.0f);
            const float r1 = r * (1.0f - 0.8f * static_cast<float>(seg + 1) / 5.0f);
            for (int k = 0; k < 5; ++k) {
                const float a0 = 6.2831853f * static_cast<float>(k) / 5.0f;
                const float a1 = 6.2831853f * static_cast<float>(k + 1) / 5.0f;
                quad(pole, {std::cos(a0) * r0, y0, std::sin(a0) * r0},
                     {std::cos(a0) * r1, y1, std::sin(a0) * r1},
                     {std::cos(a1) * r1, y1, std::sin(a1) * r1},
                     {std::cos(a1) * r0, y0, std::sin(a1) * r0}, 0xFFFFFFFFu);
            }
        }
        CHECK_FALSE(limb_reach_frac(pole, 15.0f * 0.25f, 15.0f) >= STUB_REACH_MIN);
        CHECK_FALSE(offaxis_faces(pole, 15.0f * 0.25f, 15.0f) >= OFFAXIS_MIN);
    }

    // CONTROL 2 — the neighbouring LIVE objects: winter oak and winter willow
    // (trees with zero leaves) must fail the ramification CEILING. If they do
    // not, "snag" and "dead-looking winter tree" are one object and the split
    // asset is a lie. (Winter birch is exempt by measurement — see the header
    // comment.)
    for (const FloraSpecies live : {FloraSpecies::DaleOak, FloraSpecies::ValeWillow}) {
        const FloraMesh w = build_flora_mesh(live, 3, FloraShape{}, FloraLod::Full,
                                             FloraSeason::Winter);
        float top = 0.0f;
        for (const platform::Vertex& vx : w.wood.vertices) {
            top = std::max(top, vx.position.y);
        }
        CHECK_FALSE(offaxis_faces(w.wood, top * 0.25f, top) <= OFFAXIS_MAX);
    }
}

TEST_CASE("floor: the snag split is ONE asset with two materials") {
    // Design §5.10: «a pale snag alone in a meadow is a landmark; a grey snag
    // in a wood is weather» — the same geometry, two values, two densities.
    // Geometry identity is asserted so nobody can quietly fork the shape, and
    // the VALUE separation is asserted so the two looks stay two looks.
    float lum_grey = 0.0f;
    float lum_pale = 0.0f;
    for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
        for (const FloraLod lod : LODS) {
            const FloraMesh grey =
                build_flora_mesh(FloraSpecies::Snag, v, FloraShape{}, lod);
            const FloraMesh pale =
                build_flora_mesh(FloraSpecies::SnagPale, v, FloraShape{}, lod);
            REQUIRE(grey.wood.vertices.size() == pale.wood.vertices.size());
            REQUIRE(grey.wood.indices == pale.wood.indices);
            for (size_t i = 0; i < grey.wood.vertices.size(); ++i) {
                CHECK(grey.wood.vertices[i].position.x
                      == pale.wood.vertices[i].position.x);
                CHECK(grey.wood.vertices[i].position.y
                      == pale.wood.vertices[i].position.y);
                CHECK(grey.wood.vertices[i].position.z
                      == pale.wood.vertices[i].position.z);
            }
        }
        const FloraMesh grey =
            build_flora_mesh(FloraSpecies::Snag, v, FloraShape{}, FloraLod::Full);
        const FloraMesh pale =
            build_flora_mesh(FloraSpecies::SnagPale, v, FloraShape{}, FloraLod::Full);
        auto mean_lum = [](const MeshData& m) {
            double sum = 0.0;
            for (const platform::Vertex& vx : m.vertices) {
                sum += 0.30 * chan_r(vx.color_rgba) + 0.60 * chan_g(vx.color_rgba)
                    + 0.10 * chan_b(vx.color_rgba);
            }
            return static_cast<float>(sum / std::max<size_t>(m.vertices.size(), 1));
        };
        lum_grey = mean_lum(grey.wood);
        lum_pale = mean_lum(pale.wood);
        // The open-ground look must be readably brighter — it is a legitimate
        // L2 guide, the forest look is texture. 1.25x is well past a palette
        // step and well under the birch bole, which stays the brightest LIVE
        // flora value.
        CHECK(lum_pale >= lum_grey * 1.25f);
    }
    // CONTROL: the same species against itself measures 1.0x and must FAIL the
    // separation clause — i.e. the clause cannot be satisfied by accident.
    CHECK_FALSE(lum_grey >= lum_grey * 1.25f);
}

TEST_CASE("floor: a log contacts the ground along its length") {
    // THE REJECTED INSTANCE IS A FLOATING CYLINDER. A log mesh guarantees
    // contact on FLAT ground by construction: every axial slice of the tube
    // dips below the ground datum (y = 0). On real terrain core places it
    // across the fall line; micro relief under 0.2 x radius is absorbed by the
    // same burial. Checked per SLICE, not on the whole mesh — a log whose butt
    // is buried and whose tip hovers passes a whole-mesh min and is exactly
    // the floating-log artefact.
    const FloraSpecies logs[] = {FloraSpecies::FallenLog, FloraSpecies::Deadfall};
    for (const FloraSpecies s : logs) {
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const FloraMesh f = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            // Contact is a property of the TRUNK, so it is measured on the
            // BURIED geometry: the x-span of vertices below the ground datum
            // must cover (nearly) the whole log, with no slice of that span
            // empty. Measuring slices of the full mesh extent was the first
            // version, and it failed on its own instrument: a stub's splinter
            // tip pokes past the tube's end, so the last slice held only
            // above-ground stub geometry and "the log floats" was reported
            // about a slice with no log in it.
            float x_lo = 1e9f;
            float x_hi = -1e9f;
            for (const platform::Vertex& vx : f.wood.vertices) {
                if (vx.position.y >= -0.01f) continue;
                x_lo = std::min(x_lo, vx.position.x);
                x_hi = std::max(x_hi, vx.position.x);
            }
            // The buried span exists and is the length of the log, not a
            // buried butt with a hovering tip. The floor is the species'
            // MINIMUM length (a variant may legally draw it), less 10 % for
            // ring geometry.
            REQUIRE(x_hi - x_lo > 1.5f);
            CHECK(x_hi - x_lo >= species_params(s).height_min * 0.9f);
            // No long stretch of the span hovers: the largest gap between
            // consecutive buried x-positions stays under 40 % of the span.
            // (An equal-slice binning was the first version and it failed on
            // its own instrument: the bins were finer than the mesh's vertex
            // rings, so a 3-segment deadfall "hovered" in slices that simply
            // had no ring in them. The gap between RINGS is the honest
            // resolution of the question.)
            std::vector<float> xs;
            for (const platform::Vertex& vx : f.wood.vertices) {
                if (vx.position.y < -0.01f) xs.push_back(vx.position.x);
            }
            std::sort(xs.begin(), xs.end());
            float max_gap = 0.0f;
            for (size_t i = 1; i < xs.size(); ++i) {
                max_gap = std::max(max_gap, xs[i] - xs[i - 1]);
            }
            CHECK(max_gap <= (x_hi - x_lo) * 0.40f);

            // CONTROL: the same log floated up by three metres — clear of the
            // root plate's own depth — has NO buried geometry at all. This is
            // the artefact by name.
            int buried_after_float = 0;
            for (const platform::Vertex& vx : f.wood.vertices) {
                if (vx.position.y + 3.0f < -0.01f) ++buried_after_float;
            }
            CHECK_FALSE(buried_after_float > 0);
        }
    }
}

TEST_CASE("floor: moss lives on the UPPER side of a log, in patches") {
    // Research §A7: associative placement — moss grows where rain and light
    // land. On the mesh that means up-facing faces only, and in PATCHES: an
    // all-green top is paint, not moss.
    const FloraSpecies logs[] = {FloraSpecies::FallenLog, FloraSpecies::Deadfall};
    for (const FloraSpecies s : logs) {
        const SpeciesParams& sp = species_params(s);
        const uint32_t moss_a = pack(sp.moss_color);
        const uint32_t moss_b = pack(sp.moss_color * MOSS_TONE_B);
        int up_mossed_total = 0;
        int up_total = 0;
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const FloraMesh f = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            int up_mossed = 0;
            int up_bare = 0;
            for (size_t i = 0; i + 2 < f.wood.indices.size(); i += 3) {
                const platform::Vertex& a = f.wood.vertices[f.wood.indices[i]];
                const bool mossed =
                    a.color_rgba == moss_a || a.color_rgba == moss_b;
                if (a.normal.y < -0.2f) {
                    // The underside NEVER mosses; moss under a log is the
                    // "pasted decal" read.
                    CHECK_FALSE(mossed);
                }
                if (a.normal.y > 0.40f) {
                    ++up_total;
                    if (mossed) {
                        ++up_mossed;
                        ++up_mossed_total;
                    } else {
                        ++up_bare;
                    }
                }
            }
            // A log that declares moss CARRIES moss («поваленные деревья … с
            // мохом» is the brief, not a probability).
            CHECK(up_mossed > 0);
            // PATCHES, not paint — but only where the class has enough
            // up-faces for "patchy" to be expressible. A deadfall piece has a
            // handful of up-faces and may legitimately moss them all.
            if (s == FloraSpecies::FallenLog) {
                CHECK(up_bare > 0);
            }
        }
        // Aggregate cover near the declared fraction — the moss_cover field is
        // a real parameter, not a suggestion.
        const float cover = static_cast<float>(up_mossed_total)
            / static_cast<float>(std::max(up_total, 1));
        CHECK(cover >= sp.moss_cover * 0.5f);
        CHECK(cover <= sp.moss_cover * 1.6f);
    }
    // CONTROL: a species that declares no moss carries none — the classifier
    // itself would pass a green-splattered snag, so the zero case is what
    // proves the gate is the moss pass and not the paint bucket.
    {
        const SpeciesParams& log_sp = species_params(FloraSpecies::FallenLog);
        const uint32_t moss_a = pack(log_sp.moss_color);
        const uint32_t moss_b = pack(log_sp.moss_color * MOSS_TONE_B);
        const FloraMesh snag =
            build_flora_mesh(FloraSpecies::Snag, 1, FloraShape{}, FloraLod::Full);
        for (const platform::Vertex& vx : snag.wood.vertices) {
            CHECK(vx.color_rgba != moss_a);
            CHECK(vx.color_rgba != moss_b);
        }
    }
}

TEST_CASE("floor: the maturity draw covers 25/60/12/3 over the WHOLE band") {
    // Rule 31: a field is verified over its declared range before anything is
    // tuned against it. This project has already lived through a seeded spread
    // that silently returned only the top 60 % of its range — every constant
    // fitted against it was fitted against a lie.
    constexpr int N = 200;
    int giant = 0;
    int mature = 0;
    int small = 0; // sub-mature + sapling (their value bands overlap by design)
    int sapling_only = 0; // below 0.5: unambiguously sapling
    float lo = 10.0f;
    float hi = 0.0f;
    int mature_buckets[6] = {0, 0, 0, 0, 0, 0};
    for (int ix = 0; ix < N; ++ix) {
        for (int iz = 0; iz < N; ++iz) {
            const glm::vec2 p{static_cast<float>(ix) * 3.7f,
                              static_cast<float>(iz) * 3.7f};
            const float m = flora_maturity_for(p);
            lo = std::min(lo, m);
            hi = std::max(hi, m);
            if (m >= 1.15f) {
                ++giant;
            } else if (m >= 0.85f) {
                ++mature;
                const int b = std::min(5, static_cast<int>((m - 0.85f) / 0.05f));
                ++mature_buckets[b];
            } else {
                ++small;
                if (m < 0.5f) ++sapling_only;
            }
        }
    }
    const float total = static_cast<float>(N) * static_cast<float>(N);
    // Tier shares against TREE_MATURITY_*_PCT (25/60/12/3), +-1.5 % absolute.
    CHECK(std::fabs(static_cast<float>(giant) / total
                    - static_cast<float>(config::TREE_MATURITY_GIANT_PCT) / 100.0f)
          < 0.015f);
    CHECK(std::fabs(static_cast<float>(mature) / total
                    - static_cast<float>(config::TREE_MATURITY_MATURE_PCT) / 100.0f)
          < 0.015f);
    CHECK(std::fabs(static_cast<float>(small) / total
                    - static_cast<float>(config::TREE_MATURITY_SUBMATURE_PCT
                                         + config::TREE_MATURITY_YOUNG_PCT)
                          / 100.0f)
          < 0.015f);
    // Saplings below the sub-mature floor exist at all (the 0.40-0.50 stretch
    // belongs to them alone).
    CHECK(sapling_only > 0);
    // The WHOLE band is reached: both ends, not the top third.
    CHECK(lo <= 0.43f);
    CHECK(hi >= 1.46f);
    // Uniform WITHIN the mature band: six buckets, each within 30 % of even.
    for (const int b : mature_buckets) {
        const float share = static_cast<float>(b) / static_cast<float>(mature);
        CHECK(share > (1.0f / 6.0f) * 0.7f);
        CHECK(share < (1.0f / 6.0f) * 1.3f);
    }

    // CONTROL (Rule 31's historic defect, rebuilt): a draw squeezed into the
    // top 60 % of its input range must FAIL the tier shares. This is exactly
    // the "no random field ever returned below 0.4" bug from the massif model.
    {
        int giant_c = 0;
        for (int i = 0; i < 10000; ++i) {
            const float u = 0.4f + 0.6f * static_cast<float>(i) / 10000.0f;
            if (u < static_cast<float>(config::TREE_MATURITY_GIANT_PCT) / 100.0f) {
                ++giant_c;
            }
        }
        CHECK_FALSE(std::fabs(static_cast<float>(giant_c) / 10000.0f
                              - static_cast<float>(config::TREE_MATURITY_GIANT_PCT)
                                    / 100.0f)
                    < 0.015f);
    }
}

TEST_CASE("cards: >= 3 planes per cluster, and coverage holds at the WORST azimuth") {
    // Render-spec floor (10.08.2026): card count buys ANGULAR COVERAGE, chosen
    // against the worst azimuth. Two crossed planes have viewing directions
    // where their projected area collapses and the cluster all but vanishes —
    // the birch showed it as a line of bare poles surviving a rewrite that had
    // genuinely fixed the shape, and the pine carried the same exposure at
    // lower odds. Three planes spread over the azimuths cannot all be edge-on
    // at once. The failure is a property of viewing ANGLE, not distance, so
    // the floor binds at Reduced as well as Full.
    struct Cluster {
        std::vector<glm::vec3> normals;
        std::vector<float> areas;
    };
    auto clusters_of = [](const MeshData& cards) {
        // Cards of one cluster share their centre (emit_card_cluster places
        // every plane of a cluster at one `at`).
        std::vector<glm::vec3> keys;
        std::vector<Cluster> out;
        for (size_t i = 0; i + 4 <= cards.vertices.size(); i += 4) {
            glm::vec3 c{0.0f};
            for (size_t k = 0; k < 4; ++k) c += cards.vertices[i + k].position;
            c /= 4.0f;
            const glm::vec3 e1 =
                cards.vertices[i + 1].position - cards.vertices[i].position;
            const glm::vec3 e2 =
                cards.vertices[i + 3].position - cards.vertices[i].position;
            const glm::vec3 cr = glm::cross(e1, e2);
            const float area = glm::length(cr);
            size_t found = keys.size();
            for (size_t k = 0; k < keys.size(); ++k) {
                if (glm::length(keys[k] - c) < 0.05f) {
                    found = k;
                    break;
                }
            }
            if (found == keys.size()) {
                keys.push_back(c);
                out.emplace_back();
            }
            out[found].normals.push_back(area > 1e-6f ? cr / area
                                                      : glm::vec3{0.0f, 0.0f, 1.0f});
            out[found].areas.push_back(area);
        }
        return out;
    };
    auto coverage_ratio = [](const Cluster& cl) {
        // Projected card area summed over the cluster, from 36 horizontal
        // bearings — the player walks around a tree, not over it.
        float worst = 1e9f;
        float best = 0.0f;
        for (int a = 0; a < 36; ++a) {
            const float az = 6.2831853f * static_cast<float>(a) / 36.0f;
            const glm::vec3 view{std::cos(az), 0.0f, std::sin(az)};
            float sum = 0.0f;
            for (size_t i = 0; i < cl.normals.size(); ++i) {
                sum += cl.areas[i] * std::fabs(glm::dot(cl.normals[i], view));
            }
            worst = std::min(worst, sum);
            best = std::max(best, sum);
        }
        return best > 1e-6f ? worst / best : 0.0f;
    };
    constexpr float COVERAGE_RATIO_MIN = 0.30f;
    for (const FloraSpecies s : ALL) {
        if (!has_leaf_cards(s)) continue;
        for (const FloraLod lod : {FloraLod::Full, FloraLod::Reduced}) {
            for (uint32_t v = 0; v < FLORA_VARIANTS; v += 3) {
                const FloraMesh f = build_flora_mesh(s, v, FloraShape{}, lod);
                const auto cls = clusters_of(f.cards);
                REQUIRE_FALSE(cls.empty());
                for (const Cluster& cl : cls) {
                    CHECK(cl.normals.size() >= 3);
                    CHECK(coverage_ratio(cl) >= COVERAGE_RATIO_MIN);
                }
            }
        }
    }

    // CONTROL 1: a single plane — coverage collapses to ~0 edge-on. This is
    // the pre-fix pine spray.
    {
        Cluster one;
        one.normals.push_back(glm::normalize(glm::vec3{1.0f, 0.1f, 0.0f}));
        one.areas.push_back(1.0f);
        CHECK_FALSE(coverage_ratio(one) >= COVERAGE_RATIO_MIN);
    }
    // CONTROL 2: near-parallel planes — plane COUNT without angular SPREAD
    // buys nothing, which is why the rule says coverage, not count.
    {
        Cluster par;
        par.normals.push_back(glm::normalize(glm::vec3{1.0f, 0.15f, 0.05f}));
        par.normals.push_back(glm::normalize(glm::vec3{1.0f, -0.12f, -0.06f}));
        par.normals.push_back(glm::normalize(glm::vec3{1.0f, 0.05f, 0.1f}));
        par.areas = {1.0f, 1.0f, 1.0f};
        CHECK_FALSE(coverage_ratio(par) >= COVERAGE_RATIO_MIN);
    }
}

// ===========================================================================
// THE CLUMP FIELD (в19г, design-blessed 10.08.2026, LANDSCAPE §1.7 BR-4).
// Rule 31 in full: the raw field is verified UNIFORM over its whole declared
// range before anything is tuned against it, with the un-equalized noise as
// the failing control — this project has already shipped a seeded spread that
// never left the top 60 % of its range.
// ===========================================================================

TEST_CASE("clump: the raw field is UNIFORM over [0,1] (Rule 31)") {
    constexpr int N = 220;
    for (uint8_t ci = 0; ci < CLUMP_CLASS_COUNT; ++ci) {
        const auto c = static_cast<ClumpClass>(ci);
        const float step = clump_params(c).wavelength_m * 0.37f;
        int bins[10] = {};
        float lo = 1.0f;
        float hi = 0.0f;
        for (int ix = 0; ix < N; ++ix) {
            for (int iz = 0; iz < N; ++iz) {
                const float u = clump_raw(
                    c, {static_cast<float>(ix) * step, static_cast<float>(iz) * step},
                    777u);
                lo = std::min(lo, u);
                hi = std::max(hi, u);
                ++bins[std::min(9, static_cast<int>(u * 10.0f))];
            }
        }
        // Both ends of the range are REACHED (the historic defect was a field
        // that never returned below 0.4)...
        CHECK(lo < 0.02f);
        CHECK(hi > 0.98f);
        // ...and every decile carries its share. Spatial correlation widens
        // the variance, hence 2 % absolute rather than binomial-tight.
        for (const int b : bins) {
            const float share = static_cast<float>(b) / (static_cast<float>(N) * N);
            CHECK(share > 0.08f);
            CHECK(share < 0.12f);
        }
    }

    // CONTROL: the UN-equalized value noise must FAIL the decile check — it is
    // bell-shaped, so its outer bins starve. If this control ever passes, the
    // equalization step has been deleted and every coverage number in the
    // registry silently stopped meaning what it says.
    {
        int bins[10] = {};
        for (int ix = 0; ix < N; ++ix) {
            for (int iz = 0; iz < N; ++iz) {
                const float u = clump_detail::value_noise(
                    {static_cast<float>(ix) * 0.61f, static_cast<float>(iz) * 0.61f},
                    777u);
                ++bins[std::clamp(static_cast<int>(u * 10.0f), 0, 9)];
            }
        }
        bool all_bins_fair = true;
        for (const int b : bins) {
            const float share = static_cast<float>(b) / (static_cast<float>(N) * N);
            if (share <= 0.08f || share >= 0.12f) all_bins_fair = false;
        }
        CHECK_FALSE(all_bins_fair);
    }
}

TEST_CASE("clump: coverage is EXACT and the field saturates in drift cores") {
    // Because the raw field is uniform, "coverage 0.18" must mean exactly the
    // top 18 % of ground. This is what makes the parameter AUTHORSHIP rather
    // than a suggestion the noise reinterprets.
    constexpr int N = 220;
    for (uint8_t ci = 0; ci < CLUMP_CLASS_COUNT; ++ci) {
        const auto c = static_cast<ClumpClass>(ci);
        const ClumpParams p = clump_params(c);
        const float step = p.wavelength_m * 0.61f;
        int covered = 0;
        float top = 0.0f;
        for (int ix = 0; ix < N; ++ix) {
            for (int iz = 0; iz < N; ++iz) {
                const float f = clump_field(
                    c, {static_cast<float>(ix) * step, static_cast<float>(iz) * step},
                    777u);
                CHECK(f >= 0.0f);
                CHECK(f <= 1.0f);
                if (f > 0.0f) ++covered;
                top = std::max(top, f);
            }
        }
        const float frac = static_cast<float>(covered) / (static_cast<float>(N) * N);
        CHECK(std::fabs(frac - p.coverage) < 0.025f);
        // Drift interiors reach full strength — a field that never saturates
        // is a global density dimmer, not clumping.
        CHECK(top > 0.95f);
    }
}

TEST_CASE("clump: wavelength IS the drift scale, and classes are independent") {
    const ClumpClass c = ClumpClass::Flowers;
    const float wl = clump_params(c).wavelength_m;
    double near_d = 0.0;
    double far_d = 0.0;
    double cross = 0.0;
    constexpr int N = 4000;
    for (int i = 0; i < N; ++i) {
        const int col = i % 63;
        const int row = i / 63; // integer ON PURPOSE: a lattice row index
        const glm::vec2 p{static_cast<float>(col) * 7.3f,
                          static_cast<float>(row) * 7.7f};
        const float u0 = clump_raw(c, p, 777u);
        near_d += std::fabs(clump_raw(c, p + glm::vec2{wl * 0.15f, 0.0f}, 777u) - u0);
        far_d += std::fabs(clump_raw(c, p + glm::vec2{wl * 4.0f, 1.7f * wl}, 777u) - u0);
        // Independence across classes: the mushroom field at the SAME point.
        const float m0 = clump_raw(ClumpClass::Mushrooms, p, 777u);
        cross += (static_cast<double>(u0) - 0.5) * (static_cast<double>(m0) - 0.5);
    }
    // Nearby (a sixth of a wavelength) the field barely moves; four
    // wavelengths away it is as unrelated as two uniform draws (E|u-v| = 1/3).
    CHECK(near_d / N < 0.12);
    CHECK(far_d / N > 0.26);
    // Class independence: covariance of two independent U(0,1) is 0
    // (1/12 = 0.083 would be perfect correlation).
    CHECK(std::fabs(cross / N) < 0.012);
    // CONTROL for the independence metric: a field against ITSELF measures
    // full covariance, so the bound above cannot be satisfied by accident.
    double self = 0.0;
    for (int i = 0; i < N; ++i) {
        const int col = i % 63;
        const int row = i / 63; // integer ON PURPOSE: a lattice row index
        const glm::vec2 p{static_cast<float>(col) * 7.3f,
                          static_cast<float>(row) * 7.7f};
        const float u0 = clump_raw(c, p, 777u);
        self += (static_cast<double>(u0) - 0.5) * (static_cast<double>(u0) - 0.5);
    }
    CHECK_FALSE(std::fabs(self / N) < 0.012);
}

// THE EDGE-FLOOR INVARIANT MOVED TO CORE (10.08.2026) together with
// clump_field_edged(), which is deleted: the BR-3 gradient is applied ONCE by
// the caller from PathSample::edge, so the property is now composition-level
// and cannot be asserted from this file. It travelled WITH its controls, and
// both clauses are named here so a successor can tell "moved" from "dropped":
//   (i) the floor never SUBTRACTS — edged >= the bare field everywhere;
//  (ii) a KEPT VERGE IS NOT BARE GROUND — at maintenance 0 the margin falls
//       back to the field value, never to zero, and the discriminating case is
//       ground where the field is ZERO (elsewhere the two models agree).
// If core's suite does not carry these, they exist nowhere.

TEST_CASE("clump: mushroom second stage — rings are RINGS, clusters are not") {
    // Design's blessed split: within a drift, mushrooms are parent-child, and
    // even parents ring (a ring is also a BR-6 find-catalog entry). A ring
    // means near-constant radius; a cluster means spread radii.
    glm::vec2 out[16];
    const int n_ring = mushroom_ring_offsets(42ull * 2ull, out, 16); // even
    REQUIRE(n_ring >= 5);
    float mean_r = 0.0f;
    for (int i = 0; i < n_ring; ++i) mean_r += glm::length(out[i]);
    mean_r /= static_cast<float>(n_ring);
    float dev = 0.0f;
    for (int i = 0; i < n_ring; ++i) {
        dev += std::fabs(glm::length(out[i]) - mean_r);
    }
    CHECK(mean_r > 0.6f);
    CHECK(dev / static_cast<float>(n_ring) / mean_r < 0.12f); // a RING

    glm::vec2 out2[16];
    const int n_cl = mushroom_ring_offsets(43ull * 2ull + 1ull, out2, 16); // odd
    REQUIRE(n_cl >= 3);
    float mean2 = 0.0f;
    for (int i = 0; i < n_cl; ++i) mean2 += glm::length(out2[i]);
    mean2 /= static_cast<float>(n_cl);
    float dev2 = 0.0f;
    for (int i = 0; i < n_cl; ++i) {
        dev2 += std::fabs(glm::length(out2[i]) - mean2);
    }
    // CONTROL for the ring metric: the cluster must FAIL the ring criterion.
    CHECK_FALSE(dev2 / static_cast<float>(std::max(n_cl, 1)) / std::max(mean2, 0.01f)
                < 0.12f);

    // Determinism.
    glm::vec2 again[16];
    const int n2 = mushroom_ring_offsets(42ull * 2ull, again, 16);
    REQUIRE(n2 == n_ring);
    for (int i = 0; i < n2; ++i) {
        CHECK(again[i].x == out[i].x);
        CHECK(again[i].y == out[i].y);
    }
}

// ===========================================================================
// THE RICH EDGE SET (в8/в19в) + the §5.12 talus apron. Species built BEFORE
// paths exist, so the day core's path generator lands the edges are ready.
// ===========================================================================

TEST_CASE("edge: ground patches sink into the ground and stay small") {
    const FloraSpecies patches[] = {
        FloraSpecies::MossPatch,   FloraSpecies::FlowerCarpet,
        FloraSpecies::FlowerAccent, FloraSpecies::FlowerJewel,
        FloraSpecies::FlowerUmbel, FloraSpecies::Mushroom,
        FloraSpecies::PebbleCluster,
    };
    for (const FloraSpecies s : patches) {
        const SpeciesParams& sp = species_params(s);
        for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
            const FloraMesh f = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            CHECK(f.cards.vertices.empty()); // patches are SOLID by design §5
            float lo = 1e9f;
            float hi = -1e9f;
            float wide = 0.0f;
            for (const platform::Vertex& vx : f.wood.vertices) {
                lo = std::min(lo, vx.position.y);
                hi = std::max(hi, vx.position.y);
                wide = std::max(wide, std::sqrt(vx.position.x * vx.position.x
                                                + vx.position.z * vx.position.z));
            }
            // Something is buried (a patch grows OUT of the ground)...
            CHECK(lo < -0.005f);
            // ...nothing floats off into tree scale...
            CHECK(hi <= sp.height_max * 1.9f);
            // ...and the footprint honours its declared radius (elements plus
            // their own size; 1.6 covers element reach at the rim).
            CHECK(wide <= sp.patch_radius * 1.6f + sp.element_radius * 2.0f);
            // A patch stays cheap: these live in the hundreds per path.
            CHECK(f.wood.triangle_count() <= 150u);
        }
    }
}

TEST_CASE("edge: flower heads and caps are ATTACHED, at 0.2 m as at 20 m") {
    // The complaint this zone exists to answer — foliage hanging where nothing
    // supports it — applies to a flower head exactly as to an oak crown. Every
    // accent-coloured vertex (head/cap) must be within touching distance of
    // some non-accent vertex (tuft, stem, dome).
    const FloraSpecies flowered[] = {
        FloraSpecies::FlowerCarpet, FloraSpecies::FlowerAccent,
        FloraSpecies::FlowerJewel,  FloraSpecies::FlowerUmbel,
        FloraSpecies::Mushroom,
    };
    for (const FloraSpecies s : flowered) {
        const SpeciesParams& sp = species_params(s);
        for (uint32_t v = 0; v < FLORA_VARIANTS; v += 2) {
            const FloraMesh f = build_flora_mesh(s, v, FloraShape{}, FloraLod::Full);
            // Accent tones vary per element (0.8-1.5x of two base colours), so
            // classify by what a vertex is NOT: green tuft, stem, or ground.
            const uint32_t green = pack(sp.foliage_color);
            const uint32_t stem = pack(sp.trunk_color);
            std::vector<glm::vec3> support;
            for (const platform::Vertex& vx : f.wood.vertices) {
                if (vx.color_rgba == green || vx.color_rgba == stem) {
                    support.push_back(vx.position);
                }
            }
            REQUIRE_FALSE(support.empty());
            const float touch = std::max(0.30f, sp.element_radius * 3.0f);
            for (const platform::Vertex& vx : f.wood.vertices) {
                if (vx.color_rgba == green || vx.color_rgba == stem) continue;
                float best = 1e9f;
                for (const glm::vec3& p : support) {
                    best = std::min(best, glm::length(vx.position - p));
                }
                CHECK(best <= touch);
            }
        }
    }
    // CONTROL: a head floated a metre above its tuft must FAIL the touch
    // distance — i.e. the classifier really is measuring attachment, not
    // vacuously passing everything.
    {
        const glm::vec3 tuft_top{0.0f, 0.2f, 0.0f};
        const glm::vec3 floated{0.0f, 1.2f, 0.0f};
        CHECK_FALSE(glm::length(floated - tuft_top) <= 0.35f);
    }
}

TEST_CASE("edge: the four flowers are separable from grass and each other") {
    // Design's acceptance basis for the set (value first, hue second; the
    // final judgement is theirs, from the species-line frame — these floors
    // only stop a silent drift). Full-colour basis: user ruling.
    const SpeciesParams& carpet = species_params(FloraSpecies::FlowerCarpet);
    const SpeciesParams& accent = species_params(FloraSpecies::FlowerAccent);
    const SpeciesParams& jewel = species_params(FloraSpecies::FlowerJewel);
    const SpeciesParams& umbel = species_params(FloraSpecies::FlowerUmbel);

    // (b) is the VALUE carrier: brightest of the four against the grass.
    const float grass_lum = luminance({0.30f, 0.42f, 0.18f});
    CHECK(luminance(accent.accent_color) > grass_lum * 1.8f);
    CHECK(luminance(umbel.accent_color) > grass_lum * 1.5f);
    // (a) reads by HUE at modest value cost: blue channel dominates.
    CHECK(carpet.accent_color.b > carpet.accent_color.g * 1.5f);
    CHECK(carpet.accent_color.b > carpet.accent_color.r * 1.5f);
    // (c) is saturated red, and DARK enough never to fight the accent.
    CHECK(jewel.accent_color.r > (jewel.accent_color.g + jewel.accent_color.b) * 2.0f);
    CHECK(luminance(jewel.accent_color) < luminance(accent.accent_color) * 0.5f);
    // Pairwise separation floor between all four accents.
    const glm::vec3 cols[] = {carpet.accent_color, accent.accent_color,
                              jewel.accent_color, umbel.accent_color};
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            CHECK(glm::length(cols[i] - cols[j]) > 0.25f);
        }
    }
    // CONTROL: two copies of one colour fail the pairwise floor.
    CHECK_FALSE(glm::length(cols[0] - cols[0]) > 0.25f);

    // MOSS STAYS BELOW THE GRASS BAND (design's acceptance rule, 10.08.2026):
    // moss is by design the species closest to grass, so EVERY moss tone —
    // including the MOSS_TONE_B variation, on patches and on logs — keeps a
    // readable 0.05 of luminance under the grass reference. If the shipped
    // grass darkens, moss follows it down rather than converging.
    {
        const float grass = luminance(GRASS_BAND_REFERENCE);
        const glm::vec3 moss_tones[] = {
            species_params(FloraSpecies::MossPatch).accent_color,
            species_params(FloraSpecies::MossPatch).accent_color_b,
            species_params(FloraSpecies::FallenLog).moss_color,
            species_params(FloraSpecies::FallenLog).moss_color * MOSS_TONE_B,
            species_params(FloraSpecies::Deadfall).moss_color * MOSS_TONE_B,
        };
        for (const glm::vec3& tone : moss_tones) {
            CHECK(luminance(tone) <= grass - 0.05f);
        }
        // CONTROL: the grass reference itself must FAIL the rule.
        CHECK_FALSE(luminance(GRASS_BAND_REFERENCE) <= grass - 0.05f);
    }
}

TEST_CASE("edge: the rule table is coherent, and the jewel is a BUDGET") {
    bool jewel_seen = false;
    bool edge_species_covered[7] = {};
    const FloraSpecies edge_species[7] = {
        FloraSpecies::MossPatch,    FloraSpecies::FlowerCarpet,
        FloraSpecies::FlowerAccent, FloraSpecies::FlowerJewel,
        FloraSpecies::FlowerUmbel,  FloraSpecies::Mushroom,
        FloraSpecies::PebbleCluster,
    };
    for (size_t i = 0; i < FLORA_EDGE_RULE_COUNT; ++i) {
        const FloraEdgeRule& r = FLORA_EDGE_RULES[i];
        CHECK(r.band_min_m <= r.band_max_m);
        CHECK(r.per_100m >= 0.0f);
        // Linear habitats need a linear density; area habitats use the class
        // base density instead and legally carry 0 here.
        if (r.habitat == EdgeHabitat::PathMargin && r.common_scatter) {
            CHECK(r.per_100m > 0.0f);
        }
        if (flora_species_of(r.species) == FloraSpecies::FlowerJewel) {
            jewel_seen = true;
            // DESIGN'S RULING: rarity is a placement budget, not a
            // probability. A jewel row that enters the common scatter is the
            // rule this test exists to reject.
            CHECK_FALSE(r.common_scatter);
            CHECK(r.assoc == EdgeAssociation::NearFindOnly);
        }
        for (int k = 0; k < 7; ++k) {
            if (flora_species_of(r.species) == edge_species[k]) edge_species_covered[k] = true;
        }
    }
    CHECK(jewel_seen);
    for (int k = 0; k < 7; ++k) {
        CHECK(edge_species_covered[k]); // every edge species has a home
    }
}

TEST_CASE("edge: the stunted pine is a dwarf, not a sapling and not a bush") {
    // §5.12: krummholz on the talus apron. What makes it read as a WIND-FORMED
    // TREE: dwarf height band, wider for its height than the forest pine,
    // foliage nearly to the ground, and needles that survive winter.
    const SpeciesParams& kp = species_params(FloraSpecies::StuntedPine);
    for (uint32_t v = 0; v < FLORA_VARIANTS; ++v) {
        const FloraMesh f = build_flora_mesh(FloraSpecies::StuntedPine, v,
                                             FloraShape{}, FloraLod::Full);
        const float top = highest_y(f);
        CHECK(top >= kp.height_min * 0.8f);
        CHECK(top <= kp.height_max * 1.05f);
        // Wider than the forest pine relative to height: the squat read.
        const float w = widest_radius(f);
        CHECK(w * 2.0f / top >= 0.35f);
        // Foliage nearly to the ground (an obstacle, not canopy — deliberately
        // NOT subject to CANOPY_CLEARANCE_MIN, same exemption as bushes).
        float lowest_card = 1e9f;
        for (const platform::Vertex& vx : f.cards.vertices) {
            lowest_card = std::min(lowest_card, vx.position.y);
        }
        REQUIRE(lowest_card < 1e8f);
        CHECK(lowest_card < 1.8f);
    }
    CHECK_FALSE(is_canopy_tree(FloraSpecies::StuntedPine));
    // Winter: a conifer keeps its needles.
    const FloraMesh w = build_flora_mesh(FloraSpecies::StuntedPine, 1, FloraShape{},
                                         FloraLod::Full, FloraSeason::Winter);
    CHECK_FALSE(w.cards.vertices.empty());
    // CONTROL: the forest pine fails the dwarf band — same generator, so if
    // the two ever converge, one of them has lost its numbers.
    const FloraMesh forest = build_flora_mesh(FloraSpecies::HighlandPine, 1,
                                              FloraShape{}, FloraLod::Full);
    CHECK_FALSE(highest_y(forest) <= kp.height_max * 1.05f);
}

TEST_CASE("edge: margin richness follows the SWEEP fiction, per path class") {
    // Design's ruling (10.08.2026) and the fiction that decides every number:
    // A RICH MARGIN IS WHAT GROWS WHERE NOBODY SWEEPS. Cobble through a
    // settlement is swept by the people who live there; a hint-path is BR-3's
    // specimen class and carries the full band. The test asserts the ORDERING
    // the fiction implies, not the literals, so re-tuning a value stays legal
    // and inverting the fiction does not.
    int path_rows = 0;
    for (size_t i = 0; i < FLORA_EDGE_RULE_COUNT; ++i) {
        const FloraEdgeRule& r = FLORA_EDGE_RULES[i];
        const PathClassRichness& w = r.richness;
        if (r.habitat != EdgeHabitat::PathMargin) {
            // The column is meaningless off a path, and identity there so a
            // consumer that multiplies by it anyway is harmless.
            CHECK(w.cobble == doctest::Approx(1.0f));
            CHECK(w.dirt == doctest::Approx(1.0f));
            CHECK(w.faint_trail == doctest::Approx(1.0f));
            CHECK(w.stone_steps == doctest::Approx(1.0f));
            continue;
        }
        ++path_rows;
        // DESIGN RULED AN ORDERING, NOT FOUR CONSTANTS (10.08.2026), and the
        // reason is worth keeping: four per-class numbers would be four things
        // to tune, while «less tended means more overgrown» is what the
        // fiction actually claims. So the assertions are ordinal.
        // hint >= dirt > cobble, with the dirt/cobble step STRICT: a dirt road
        // must SHOW a peak (it is not required to reach RICH_EDGE_RATIO) and
        // cobble must not.
        CHECK(w.faint_trail >= w.dirt);
        CHECK(w.dirt > w.cobble);
        // The hint-path is BR-3's specimen class — the one the ratio is
        // MEASURED on — and carries the full band.
        CHECK(w.faint_trail == doctest::Approx(1.0f));
        // The maintained end has no peak worth the name.
        CHECK(w.cobble <= 0.35f);
        CHECK(w.dirt <= 0.75f);
        // Every weight is a multiplier.
        for (uint8_t k = 0; k < 4; ++k) {
            CHECK(w.by_ordinal(k) >= 0.0f);
            CHECK(w.by_ordinal(k) <= 1.0f);
        }
        // Design's two named stair cases, asserted by species rather than by
        // value: moss lives in the shaded JOINTS, flowers never do.
        if (flora_species_of(r.species) == FloraSpecies::MossPatch) {
            CHECK(w.stone_steps >= 0.5f);
            // THE MOSS RESIDUAL IS BOUNDED (design, 10.08.2026). It is argued
            // from fiction — «life survives where the broom cannot reach», the
            // damp joint being a mechanism rather than a mood — and a number
            // argued from fiction is exactly the kind that grows later. So:
            // STRICTLY under the dirt weight, or the ordering loses its teeth;
            // and moss ONLY, never generalised to "damp species" (enforced by
            // this clause living under a MossPatch test, plus the flowers and
            // mushrooms taking 0 on cobble above).
            CHECK(w.cobble < w.dirt);
            CHECK(w.cobble <= 0.30f);
        }
        if (flora_species_of(r.species) == FloraSpecies::FlowerCarpet
            || flora_species_of(r.species) == FloraSpecies::FlowerAccent) {
            CHECK(w.stone_steps == doctest::Approx(0.0f));
        }
    }
    REQUIRE(path_rows >= 6); // the margin set is actually being measured

    // CONTROL — THE REAL REJECTED INSTANCE: this table as it stood BEFORE the
    // ruling, i.e. no maintenance modelled at all, which gardened a town
    // gutter as lushly as a woodland trail. Flat weights must FAIL the
    // suppression clause; if they passed, the column would be decorative.
    {
        const PathClassRichness flat{1.0f, 1.0f, 1.0f, 1.0f};
        CHECK_FALSE(flat.cobble <= 0.35f);
        // ...while still satisfying monotonicity, which is exactly why
        // monotonicity ALONE could not have caught it. Two clauses, because
        // this zone's signature failure is a rule implemented in half.
        CHECK(flat.faint_trail >= flat.dirt);
    }
    // (The kept-verge-is-not-bare-ground assertions that stood here moved to
    // core with clump_field_edged() — see the tombstone above the mushroom
    // ring case. What REMAINS testable from this zone is the table itself:
    // that the weights are ordered, bounded, and mean what the fiction says.
    // The property that a maintenance weight of 0 leaves the base presence
    // untouched is a property of the COMPOSITION, and the composition now
    // lives in core.)

    // CONTROL 2: the ordinal accessor is bound to core's PathClass order, and
    // a mismatch is silent (the two enums cannot see each other across the
    // DAG). Pin the mapping so a reorder on either side breaks a test rather
    // than permuting a landscape.
    {
        // The failure message has to say WHY (design's ask): whoever trips
        // this must fix the MAPPING, not the expectation. If you are reading
        // this because the line below went red, core's PathClass enum has been
        // reordered and every margin weight in FloraEdgeRules.h now applies to
        // the wrong path class — silently, because `world` and `render` are
        // DAG siblings and neither can see the other's enum. A cobbled street
        // will bloom and a hint-path will be swept. Fix PathClassRichness's
        // field order to match the new PathClass, do not relax this test.
        INFO("PathClassRichness field order must match core's PathClass "
             "ordinals (0 Cobble, 1 Dirt, 2 FaintTrail, 3 StoneSteps, "
             "engine/world/sources/WorldgenPaths.h). A reorder on either side "
             "PERMUTES THE MARGIN WEIGHTS SILENTLY — fix the mapping, never "
             "this expectation.");
        const PathClassRichness w{0.1f, 0.2f, 0.3f, 0.4f};
        CHECK(w.by_ordinal(0) == doctest::Approx(0.1f)); // Cobble
        CHECK(w.by_ordinal(1) == doctest::Approx(0.2f)); // Dirt
        CHECK(w.by_ordinal(2) == doctest::Approx(0.3f)); // FaintTrail
        CHECK(w.by_ordinal(3) == doctest::Approx(0.4f)); // StoneSteps
        CHECK(w.by_ordinal(9) == doctest::Approx(1.0f)); // unknown -> identity
    }
}

TEST_CASE("routing: every species core can place BUILDS something") {
    // THE BUG THIS EXISTS TO MAKE IMPOSSIBLE (render, 10.08.2026): core grew
    // math::ScatterSpecies from 5 to 18; render's build_scatter_mesh switched
    // over the original five and returned an EMPTY MeshData for the rest, and
    // build_scatter_batches skipped empties silently. So the world placed
    // snags, big bushes, fallen logs and deadfall, the forest floor drew as
    // BARE EARTH, and every test in both zones stayed green — absence
    // presenting as a neutral state, which is the same failure that hid the
    // missing site meshes for a whole stage.
    //
    // Flora cannot fix render's switch from here, but it CAN guarantee the
    // half it owns: that every ordinal it claims actually produces geometry.
    // Walk the enum by VALUE rather than by a hand-written list, or this test
    // acquires the very blind spot it is meant to remove.
    for (uint8_t ord = 0; ord <= static_cast<uint8_t>(math::ScatterSpecies::StuntedPine);
         ++ord) {
        const auto s = static_cast<math::ScatterSpecies>(ord);
        CAPTURE(ord);
        if (!flora_owns(s)) continue;
        const FloraSpecies fs = flora_species_of(s);
        for (const FloraLod lod : LODS) {
            const FloraMesh m = build_flora_mesh(fs, 3, FloraShape{}, lod);
            const size_t tris = m.wood.triangle_count() + m.cards.triangle_count();
            // Non-empty is the whole point: an empty mesh is what drew nothing.
            CHECK(tris > 0);
            // ...and it must have real extent, since a degenerate mesh would
            // pass a triangle count and still render as nothing visible.
            float hi = 0.0f;
            for (const MeshData* md : {&m.wood, &m.cards}) {
                for (const platform::Vertex& v : md->vertices) {
                    hi = std::max(hi, std::abs(v.position.y));
                }
            }
            CHECK(hi > 0.01f);
        }
    }

    // Stone is NOT flora's, and the mapping's default is why that matters:
    // flora_species_of(Stone) answers Bush, so routing a boulder down the
    // flora path would draw a shrub. The predicate is what keeps it out.
    CHECK_FALSE(flora_owns(math::ScatterSpecies::Stone));

    // CONTROL — the rejected instance, rebuilt: the OLD routing predicate,
    // which named only the three canopy trees. Every §5.10 and §5.11 ordinal
    // must fail it, which is precisely why the forest floor drew as bare
    // earth. If this control ever passes, someone has narrowed flora_owns()
    // back to a tree list.
    auto old_is_tree = [](math::ScatterSpecies s) {
        return s == math::ScatterSpecies::OakTree || s == math::ScatterSpecies::PineTree
            || s == math::ScatterSpecies::BirchTree;
    };
    for (const math::ScatterSpecies s :
         {math::ScatterSpecies::Snag, math::ScatterSpecies::SnagPale,
          math::ScatterSpecies::BigBush, math::ScatterSpecies::FallenLog,
          math::ScatterSpecies::Deadfall, math::ScatterSpecies::MossPatch,
          math::ScatterSpecies::StuntedPine}) {
        CHECK(flora_owns(s));
        CHECK_FALSE(old_is_tree(s)); // the old predicate dropped every one
    }
}
