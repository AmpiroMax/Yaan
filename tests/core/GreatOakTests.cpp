/*
Created: 12:08:2026 - 22:52:00
Last updated: 12:08:2026 - 22:52:00
Module: tests/core
File: tests/core/GreatOakTests.cpp

Responsibility:
- THE GREAT OAK'S PLACEMENT, measured IN THE GENERATOR (Rule 47): how many
  giants the world holds, how far apart they are, and how many other trees
  stand inside the clearing each one is entitled to. A frame confirms these
  numbers; it never produces them, because a count read off a picture falls
  with the haze while the placement is unchanged.

Key items:
- the derived sizes against flora's measured floor;
- the clearing, counted over the real scatter, with its zero-dose control arm;
- the occlusion envelope, which must know a silhouette that stands in a gap in
  the forest mask.

Dependencies:
- Uses: dfn_world (worldgen + placement), the shipped layout asset.
- Used by: ctest (run from the repo ROOT — it opens the asset by relative path).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- BOTH ARMS COME FROM ONE BINARY (Rule 30/48): the control is DFN_NO_GREAT_OAK
  set around a second build_world_context, never a rebuild and never an
  older commit — a rebuild also moves the terrain and answers a different
  question.
*/
/*
UPD:
- 12:08:2026 - 22:52:00: Created with the placement pass.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/LayoutLoad.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenGreatOak.h"
#include "engine/world/sources/WorldgenPlacement.h"
#include "engine/world/sources/WorldgenScatter.h"
#include "engine/world/sources/WorldgenValidation.h"

#include <cstdlib>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace dfn;
using world::WorldGenParams;

namespace {

WorldGenParams shipped_params() {
    WorldGenParams p;
    p.seed = 1;
    p.min_chunk = {0, 0};
    p.max_chunk = {static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1,
                   static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1};
    const auto lr = world::load_layout_file(
        "games/daggerfall_n/assets/world/testbed_layout.json", p.layout);
    REQUIRE_MESSAGE(lr.ok, "layout asset must load (run ctest from the repo root)");
    return p;
}

const world::WorldGenContext& shipped_world() {
    static const world::WorldGenContext ctx = world::build_world_context(shipped_params());
    return ctx;
}

/// THE ZERO-DOSE ARM: the same binary, the same seed, the same layout, with the
/// whole class switched off — no giants, therefore no clearings.
const world::WorldGenContext& control_world() {
    static const world::WorldGenContext ctx = [] {
        setenv("DFN_NO_GREAT_OAK", "1", 1);
        world::WorldGenContext c = world::build_world_context(shipped_params());
        unsetenv("DFN_NO_GREAT_OAK");
        return c;
    }();
    return ctx;
}

bool is_tree(math::ScatterSpecies s) {
    switch (s) {
    case math::ScatterSpecies::OakTree:
    case math::ScatterSpecies::PineTree:
    case math::ScatterSpecies::BirchTree:
    case math::ScatterSpecies::Snag:
    case math::ScatterSpecies::SnagPale:
    case math::ScatterSpecies::StuntedPine:
        return true;
    default:
        return false;
    }
}

/// Counts OTHER trees standing within `radius` of `centre`, over the real
/// per-chunk scatter of `ctx` — the generator's own answer, not a frame's.
int trees_within(const world::WorldGenContext& ctx, glm::vec2 centre, float radius) {
    const auto chunk = static_cast<float>(config::CHUNK_SIZE);
    const int c0x = static_cast<int>(std::floor((centre.x - radius) / chunk));
    const int c1x = static_cast<int>(std::floor((centre.x + radius) / chunk));
    const int c0z = static_cast<int>(std::floor((centre.y - radius) / chunk));
    const int c1z = static_cast<int>(std::floor((centre.y + radius) / chunk));
    int n = 0;
    for (int cz = c0z; cz <= c1z; ++cz) {
        for (int cx = c0x; cx <= c1x; ++cx) {
            if (cx < ctx.params.min_chunk.x || cx > ctx.params.max_chunk.x
                || cz < ctx.params.min_chunk.z || cz > ctx.params.max_chunk.z) {
                continue;
            }
            const glm::vec2 lo{static_cast<float>(cx) * chunk, static_cast<float>(cz) * chunk};
            for (const math::ScatterInstance& i :
                 world::build_scatter(ctx, lo, lo + glm::vec2{chunk, chunk})) {
                if (!is_tree(i.species)) continue;
                if (glm::length(glm::vec2{i.position.x, i.position.z} - centre) < radius) ++n;
            }
        }
    }
    return n;
}

} // namespace

TEST_CASE("GIANT_OAKS §2: the giant's sizes are DERIVED, and they clear flora's floor") {
    const float h = world::great_oak_height_m();
    const float crown_r = world::great_oak_crown_radius_m();
    const float clearing = world::great_oak_clearing_radius_m();
    const float sep = world::great_oak_separation_m();
    MESSAGE("canopy top " << h << " m, crown radius " << crown_r << " m (diameter "
                          << 2.0f * crown_r << " m), clearing radius " << clearing
                          << " m, separation " << sep << " m");

    // The envelope core already promises for a giant tier, unchanged.
    CHECK(h == doctest::Approx(static_cast<float>(config::OAK_HEIGHT_MAX)
                               * static_cast<float>(config::TREE_MATURITY_GIANT_MULT_MAX)));
    // flora's requirement, stated on their own frame: "площадка радиусом не
    // менее 50 м, свободная от других деревьев" (crown 92 m across plus a
    // margin). It is somebody else's FLOOR, so this asserts our derivation
    // clears it — it does not fit our derivation to it (Rule 45).
    CHECK(clearing >= 50.0f);
    // And the rarity is a CONSEQUENCE: the separation is the crown's own read
    // distance. In a 2x2 km world that is larger than the diagonal, which is
    // why the count below is what it is.
    CHECK(sep == doctest::Approx(world::readable_distance_m(2.0f * crown_r)));
}

TEST_CASE("GIANT_OAKS §2: the world holds giants, and no two compete") {
    const auto& ctx = shipped_world();
    const auto& giants = ctx.great_oaks;
    const float world_m = static_cast<float>(config::WORLD_EXTENT_CHUNKS)
                        * static_cast<float>(config::CHUNK_SIZE);
    MESSAGE("giants placed: " << giants.size() << " in a " << world_m << " m world (diagonal "
                              << world_m * 1.41421356f << " m), separation "
                              << world::great_oak_separation_m() << " m");
    for (const world::GreatOakSite& g : giants) {
        MESSAGE("  giant at (" << g.pos.x << ", " << g.pos.y << "), ground " << g.ground_y
                               << " m, crown radius " << g.crown_radius << " m, clearing "
                               << g.clearing_radius << " m, chained " << g.chained);
    }
    REQUIRE(!giants.empty());
    for (std::size_t i = 0; i < giants.size(); ++i) {
        for (std::size_t j = i + 1; j < giants.size(); ++j) {
            CHECK(glm::length(giants[i].pos - giants[j].pos)
                  >= world::great_oak_separation_m());
        }
    }
    // The named oak of §6 needs a sea cliff and this world has no sea. If this
    // ever fires, the chain was awarded by somebody's taste rather than by the
    // map, which is the one thing §6 forbids.
    for (const world::GreatOakSite& g : giants) CHECK_FALSE(g.chained);
}

TEST_CASE("GIANT_OAKS §2: THE CLEARING — no other tree stands inside it") {
    const auto& ctx = shipped_world();
    REQUIRE(!ctx.great_oaks.empty());
    const world::GreatOakSite& g = ctx.great_oaks.front();

    const int inside = trees_within(ctx, g.pos, g.clearing_radius);
    // THE CONTROL (Rule 30/48): the same disc of the same world with the pass
    // switched off. If this is not positive the assertion above is describing
    // ground that was empty anyway, and it discriminates nothing.
    const int control = trees_within(control_world(), g.pos, g.clearing_radius);
    MESSAGE("trees inside the " << g.clearing_radius << " m clearing: " << inside
                                << "   (zero-dose arm: " << control << ")");
    CHECK(inside == 0);
    CHECK(control > 0);

    // And the giant itself IS in the world's scatter, once.
    int giants_emitted = 0;
    const auto chunk = static_cast<float>(config::CHUNK_SIZE);
    for (int cz = ctx.params.min_chunk.z; cz <= ctx.params.max_chunk.z; ++cz) {
        for (int cx = ctx.params.min_chunk.x; cx <= ctx.params.max_chunk.x; ++cx) {
            const glm::vec2 lo{static_cast<float>(cx) * chunk, static_cast<float>(cz) * chunk};
            for (const math::ScatterInstance& i :
                 world::build_scatter(ctx, lo, lo + glm::vec2{chunk, chunk})) {
                if (i.species == math::ScatterSpecies::GreatOak) ++giants_emitted;
            }
        }
    }
    MESSAGE("great-oak instances emitted across the world: " << giants_emitted);
    CHECK(giants_emitted == static_cast<int>(ctx.great_oaks.size()));
}

TEST_CASE("GIANT_OAKS §1: the second landmark does not argue with the massif or the castle") {
    // The question the lead asked out loud, answered with two arms of one
    // binary rather than with reasoning: a 96 m crown at 48 m is the second
    // dominant landmark of this world, so C1 (the massif is visible from the
    // valley), the castle's crown-occlusion rule and the coequal-crowd count
    // all apply to IT, not only to buildings.
    const auto& shipped = shipped_world();
    const auto& control = control_world();

    const float c1_shipped = world::landmark_visibility_fraction(shipped);
    const float c1_control = world::landmark_visibility_fraction(control);
    const world::CastleHierarchy h_shipped = world::castle_hierarchy(shipped);
    const world::CastleHierarchy h_control = world::castle_hierarchy(control);
    MESSAGE("C1 landmark visibility: " << c1_shipped << " with the giant, " << c1_control
                                       << " without (floor " << config::LANDMARK_VISIBILITY_MIN
                                       << ")");
    MESSAGE("crown occluded: " << h_shipped.crown_occluded << " / " << h_control.crown_occluded
                               << ";  coequal visible: " << h_shipped.max_coequal_visible << " / "
                               << h_control.max_coequal_visible << ";  large-mass group: "
                               << h_shipped.max_coequal_large << " / "
                               << h_control.max_coequal_large);

    // The floor is the world's, not this pass's — but this pass is now on the
    // hook for it, because the envelope it added is a real occluder.
    CHECK(c1_shipped >= static_cast<float>(config::LANDMARK_VISIBILITY_MIN));
    // And the giant's CONTRIBUTION, which is the number this pass owns: it may
    // not be the reason any of the three degrade.
    CHECK(c1_shipped >= c1_control - 0.001f);
    CHECK(h_shipped.crown_occluded == h_control.crown_occluded);
    CHECK(h_shipped.max_coequal_large <= h_control.max_coequal_large);
}

TEST_CASE("GIANT_OAKS §1: the occlusion envelope knows the giant's SILHOUETTE") {
    const auto& ctx = shipped_world();
    REQUIRE(!ctx.great_oaks.empty());
    const world::GreatOakSite& g = ctx.great_oaks.front();

    // At the trunk and — the half that is easy to miss — at nine tenths of the
    // crown radius, 43 m off the trunk, where the forest mask reports a
    // clearing and the world nonetheless carries a 48 m canopy.
    const glm::vec2 rim = g.pos + glm::vec2{0.9f * g.crown_radius, 0.0f};
    const float at_trunk = world::canopy_height_at(ctx, g.pos, world::terrain_height(ctx, g.pos));
    const float at_rim = world::canopy_height_at(ctx, rim, world::terrain_height(ctx, rim));
    const float outside = world::canopy_height_at(
        ctx, g.pos + glm::vec2{1.4f * g.crown_radius, 0.0f},
        world::terrain_height(ctx, g.pos + glm::vec2{1.4f * g.crown_radius, 0.0f}));
    MESSAGE("envelope: trunk " << at_trunk << " m, rim(0.9 r) " << at_rim << " m, outside(1.4 r) "
                               << outside << " m; trees actually standing inside the crown: "
                               << trees_within(ctx, g.pos, g.crown_radius));
    CHECK(at_trunk >= g.height);
    CHECK(at_rim >= g.height);
    // Every other tree is gone from that disc, so the envelope's answer there
    // can only have come from the giant.
    CHECK(trees_within(ctx, g.pos, g.crown_radius) == 0);
}
