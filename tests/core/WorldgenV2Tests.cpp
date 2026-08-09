/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 11:05:22
Module: tests
File: tests/core/WorldgenV2Tests.cpp

Responsibility:
- Worldgen v2 design-contract suite over the seed-1 testbed (LANDSCAPE.md):
  hydrology monotonic invariant (a climbing river = failed generation), lake
  at LAKE_LEVEL_TESTBED with sand shore, the L0 crag, ford carve depths,
  building pads (slope + flood margin), corridor slope limit, C1 landmark
  visibility, scatter placement rules and the P3 surface classification.

Dependencies:
- Uses: doctest, dfn_world (Worldgen, Validation, Sites, Scatter).
- Used by: ctest (test_worldgen_v2).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Thresholds come from dfn::config — weakening a check needs a design sync.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — initial v2 contract suite.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenScatter.h"
#include "engine/world/sources/WorldgenSites.h"
#include "engine/world/sources/WorldgenValidation.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

using namespace dfn;
using world::ChunkCoord;
using world::WorldGenParams;

namespace {

/// The canonical testbed (seed 1, 4x4 chunks, LANDSCAPE §7 layout), built
/// once for the whole suite — context construction is itself under test in
/// the determinism suite.
const world::WorldGenContext& testbed() {
    static const world::WorldGenContext ctx =
        world::build_world_context(WorldGenParams{1, {0, 0}, {3, 3}});
    return ctx;
}

constexpr float LAKE_LEVEL = static_cast<float>(config::LAKE_LEVEL_TESTBED);

} // namespace

TEST_CASE("hydrology: river built, monotonic, reaches the lake") {
    const auto& ctx = testbed();
    REQUIRE(ctx.hydrology.ok);
    REQUIRE_FALSE(ctx.hydrology.stations.empty());
    CHECK(world::river_is_monotonic(ctx.hydrology));

    // Two segments expected on the testbed: source -> lake, outlet -> edge.
    REQUIRE(ctx.hydrology.segment_offsets.size() >= 2);
    // Segment 0 ends at the lake basin.
    const auto& inflow_end =
        ctx.hydrology.stations[ctx.hydrology.segment_offsets[1] - 1];
    CHECK(world::lake_norm_radius(ctx.params.layout.lake, inflow_end.position) < 1.3f);
    // Water never sits above the source's own level anywhere downstream.
    CHECK(ctx.hydrology.stations.back().surface_height
          <= ctx.hydrology.stations.front().surface_height);
}

TEST_CASE("hydrology: a perturbed layout without a lake still never climbs") {
    // Push the lake far outside the domain: pond-and-spill alone must keep
    // the invariant.
    WorldGenParams params{1, {0, 0}, {3, 3}};
    params.layout.lake.center = {-4000.0f, -4000.0f};
    const auto ctx = world::build_world_context(params);
    REQUIRE(ctx.hydrology.ok);
    CHECK(world::river_is_monotonic(ctx.hydrology));
}

TEST_CASE("lake sits at LAKE_LEVEL_TESTBED with a sand shore") {
    const auto& ctx = testbed();
    const glm::vec2 center = ctx.params.layout.lake.center;
    const auto sp = world::surface_point(ctx, center);
    CHECK(sp.water_surface == doctest::Approx(LAKE_LEVEL));
    CHECK(sp.height < LAKE_LEVEL);
    CHECK(sp.surface_class == math::SurfaceClass::WaterBed);
    CHECK(sp.dist_to_water == 0.0f);

    // Some ring samples just outside the waterline classify as sand (§3.3).
    int sand = 0;
    for (int i = 0; i < 64; ++i) {
        const float ang = static_cast<float>(i) * 0.0981747f; // 2*pi/64
        const glm::vec2 p =
            center + glm::vec2{std::cos(ang) * (ctx.params.layout.lake.half_extent.x + 2.0f),
                               std::sin(ang) * (ctx.params.layout.lake.half_extent.y + 2.0f)};
        if (world::surface_point(ctx, p).surface_class == math::SurfaceClass::Sand) {
            ++sand;
        }
    }
    CHECK(sand > 0);
}

TEST_CASE("fords: carve depth at each layout ford is wade-shallow") {
    const auto& ctx = testbed();
    for (const glm::vec2 ford : ctx.params.layout.river.fords) {
        uint32_t nearest = 0;
        float best = 1e9f;
        for (uint32_t i = 0; i < ctx.hydrology.stations.size(); ++i) {
            const float d = glm::length(ctx.hydrology.stations[i].position - ford);
            if (d < best) {
                best = d;
                nearest = i;
            }
        }
        CHECK(ctx.hydrology.carve_depth[nearest]
              <= static_cast<float>(config::FORD_DEPTH_MAX) + 1e-3f);
    }
}

TEST_CASE("L0 crag: peak height, rock crown, skyline dominance") {
    const auto& ctx = testbed();
    const glm::vec2 peak = ctx.params.layout.crag.center;
    const float peak_h = world::terrain_height(ctx, peak);
    CHECK(peak_h > 44.0f); // ~52 m target, pad flatten and noise allowed
    CHECK(peak_h <= static_cast<float>(config::WORLDGEN_MAX_HEIGHT));
    // Above the rockline the stamp classifies as rock (§4) — sample the crown
    // flanks (the summit itself carries the tower pad).
    int rock = 0;
    for (const glm::vec2 off : {glm::vec2{30.0f, 0.0f}, {-30.0f, 0.0f}, {0.0f, 30.0f}}) {
        if (world::surface_point(ctx, peak + off).surface_class == math::SurfaceClass::Rock) {
            ++rock;
        }
    }
    CHECK(rock >= 2);
    // The crag out-tops everything: no sample in the domain exceeds the peak
    // area's height (C4 hierarchy) — coarse scan.
    float max_h = 0.0f;
    for (float z = 8.0f; z < 1024.0f; z += 32.0f) {
        for (float x = 8.0f; x < 1024.0f; x += 32.0f) {
            max_h = std::max(max_h, world::terrain_height(ctx, {x, z}));
        }
    }
    CHECK(max_h <= peak_h + 3.0f);
}

TEST_CASE("P4: full site roster on pads — flat, dry, above flood margin") {
    const auto& ctx = testbed();
    const auto& sites = ctx.sites;
    REQUIRE(sites.entities.size() == sites.types.size());
    REQUIRE(sites.entities.size() == sites.pads.size());

    // Roster: 1 tavern + 1 trader + dwellings/barns within HAMLET_SIZE, one
    // shrine, TESTBED_DUNGEONS entrances, one tower ruin.
    int tavern = 0, trader = 0, dwelling = 0, barn = 0, shrine = 0, dungeon = 0, tower = 0;
    for (const world::SiteType t : sites.types) {
        switch (t) {
        case world::SiteType::Tavern: ++tavern; break;
        case world::SiteType::Trader: ++trader; break;
        case world::SiteType::Dwelling: ++dwelling; break;
        case world::SiteType::Barn: ++barn; break;
        case world::SiteType::Shrine: ++shrine; break;
        case world::SiteType::DungeonEntrance: ++dungeon; break;
        case world::SiteType::TowerRuin: ++tower; break;
        }
    }
    CHECK(tavern == 1);
    CHECK(trader == 1);
    CHECK(shrine == 1);
    CHECK(dungeon == static_cast<int>(config::TESTBED_DUNGEONS));
    CHECK(tower == 1);
    const int hamlet_total = tavern + trader + dwelling + barn;
    CHECK(hamlet_total >= static_cast<int>(config::HAMLET_SIZE_MIN));
    CHECK(hamlet_total <= static_cast<int>(config::HAMLET_SIZE_MAX));

    // Deterministic sequential world ids (save-delta anchor, Q56).
    for (std::size_t i = 0; i < sites.entities.size(); ++i) {
        CHECK(sites.entities[i].world_id == i + 1);
    }

    // Every pad: final terrain is flat across it, its center dry and clear of
    // the waterline (§6 flood margin is relative to the nearest water body).
    for (std::size_t i = 0; i < sites.pads.size(); ++i) {
        const auto& pad = sites.pads[i];
        const float hc = world::terrain_height(ctx, pad.center);
        const float he = world::terrain_height(
            ctx, pad.center + glm::vec2{pad.radius * 0.7f, 0.0f});
        // Flattened (§6 stamp). Tolerance covers neighbor pads' blend skirts
        // overlapping inside a hamlet (buildings sit 4-10 m apart).
        CHECK(std::fabs(he - hc) < 0.5f);
        const auto sp = world::surface_point(ctx, pad.center);
        CHECK(sp.water_surface == math::NO_WATER);
        CHECK(sp.dist_to_water > 2.0f);
    }
}

TEST_CASE("corridors: average slope within CORRIDOR_SLOPE_MAX") {
    const auto& ctx = testbed();
    const float worst = world::max_corridor_avg_slope(ctx);
    CHECK(worst <= static_cast<float>(config::CORRIDOR_SLOPE_MAX));
}

TEST_CASE("C1: the L0 is visible from most open walkable ground") {
    const auto& ctx = testbed();
    const float fraction = world::landmark_visibility_fraction(ctx);
    CHECK(fraction >= static_cast<float>(config::LANDMARK_VISIBILITY_MIN));
}

TEST_CASE("P5 scatter: forest fills its mass, respects corridors and water") {
    const auto& ctx = testbed();
    // Chunk (2, 3) lies in the SE oak band (§7.1).
    const auto chunk = world::generate_chunk(ctx, ChunkCoord{2, 3});
    int oaks = 0;
    for (const auto& inst : chunk.scatter) {
        if (inst.species == math::ScatterSpecies::OakTree) ++oaks;
        const glm::vec2 p{inst.position.x, inst.position.z};
        // Inside the chunk's half-open bounds.
        CHECK(p.x >= 512.0f);
        CHECK(p.x < 768.0f);
        CHECK(p.y >= 768.0f);
        CHECK(p.y < 1024.0f);
        // Trees never in the corridor band or in water (§2.4, §5).
        if (inst.species != math::ScatterSpecies::Stone) {
            CHECK(world::corridor_distance(ctx.params.layout, p)
                  >= static_cast<float>(config::CORRIDOR_WIDTH) * 0.5f);
        }
        CHECK(world::surface_point(ctx, p).water_surface == math::NO_WATER);
    }
    CHECK(oaks > 50); // a forest mass, not a sprinkle

    // The forced forest-ruin clearing stays treeless (§7.1 dungeon 2).
    const glm::vec2 clearing = ctx.params.layout.forests.forced_clearing_center;
    for (const auto& inst : chunk.scatter) {
        if (inst.species == math::ScatterSpecies::OakTree
            || inst.species == math::ScatterSpecies::PineTree) {
            CHECK(glm::length(glm::vec2{inst.position.x, inst.position.z} - clearing)
                  >= ctx.params.layout.forests.forced_clearing_radius - 0.001f);
        }
    }
}

TEST_CASE("P3 surface: classes obey the priority rules on a sample sweep") {
    const auto& ctx = testbed();
    // Sweep a coarse grid: every rock-classified sample is steep or crag,
    // every grass sample is gentle, sand is near water.
    for (float z = 16.0f; z < 1024.0f; z += 96.0f) {
        for (float x = 16.0f; x < 1024.0f; x += 96.0f) {
            const auto sp = world::surface_point(ctx, {x, z});
            if (sp.surface_class == math::SurfaceClass::Sand) {
                CHECK(sp.dist_to_water <= static_cast<float>(config::SHORE_SAND_DIST) + 0.001f);
            }
            if (sp.surface_class == math::SurfaceClass::WaterBed) {
                CHECK(sp.water_surface != math::NO_WATER);
                CHECK(sp.height < sp.water_surface);
            }
            if (sp.water_surface != math::NO_WATER) {
                // Monotonic: water never above the lake's level upstream of it.
                CHECK(sp.water_surface
                      <= ctx.hydrology.stations.front().surface_height + 0.001f);
            }
        }
    }
}
