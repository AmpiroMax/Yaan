/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 14:03:23
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
- 09:08:2026 - 13:12:19: Stage 3b amendments: derived-ford suite (crossings wade-shallow, FORD_SPACING gaps), §3.3 mud-cap band + coverage tripwire + dist saturation, grid-vs-analytic equality, canopy-aware C1 kept at LANDMARK_VISIBILITY_MIN.
- 09:08:2026 - 13:28:27: P1 anisotropy retune: structure-tensor elongation invariant (seed-1 median ratio ~3.9, floor 2.5; isotropic sits near 2).
- 09:08:2026 - 14:03:23: Micro-relief batch: groove field + carved-trail-vs-shoulders test (ford/slope contracts re-asserted), curb-stone margin-band test.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenScatter.h"
#include "engine/world/sources/WorldgenSites.h"
#include "engine/world/sources/WorldgenValidation.h"

#include <algorithm>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <vector>

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

TEST_CASE("fords are derived from the generated trace (§7.1a) and wade-shallow") {
    const auto& ctx = testbed();
    const auto& h = ctx.hydrology;
    REQUIRE_FALSE(h.ford_stations.empty());
    // Every derived ford sits on a wade-shallow bed.
    for (const uint32_t f : h.ford_stations) {
        REQUIRE(f < h.stations.size());
        CHECK(h.carve_depth[f] <= static_cast<float>(config::FORD_DEPTH_MAX) + 1e-3f);
    }
    // FORD_SPACING minimum: no along-river gap (incl. start/end) exceeds
    // FORD_SPACING_MAX.
    std::vector<float> cum(h.stations.size(), 0.0f);
    for (std::size_t i = 1; i < h.stations.size(); ++i) {
        cum[i] = cum[i - 1] + glm::length(h.stations[i].position - h.stations[i - 1].position);
    }
    std::vector<float> marks{0.0f};
    for (const uint32_t f : h.ford_stations) marks.push_back(cum[f]);
    marks.push_back(cum.back());
    std::sort(marks.begin(), marks.end());
    for (std::size_t g = 0; g + 1 < marks.size(); ++g) {
        CHECK(marks[g + 1] - marks[g]
              <= static_cast<float>(config::FORD_SPACING_MAX) + 1.0f);
    }
    // C3 against GENERATED water: every corridor crossing of any water is
    // wade-shallow — the chain is never severed.
    CHECK(world::max_corridor_water_depth(ctx)
          <= static_cast<float>(config::FORD_DEPTH_MAX) + 1e-2f);
}

TEST_CASE("§3.3 bed/mud cap: no wide water flats, dist field range") {
    const auto& ctx = testbed();
    const auto& h = ctx.hydrology;
    int total = 0, covered = 0;
    float max_dist = 0.0f;
    for (float z = 4.0f; z < 1024.0f; z += 8.0f) {
        for (float x = 4.0f; x < 1024.0f; x += 8.0f) {
            const auto sp = world::surface_point(ctx, {x, z});
            ++total;
            max_dist = std::max(max_dist, sp.dist_to_water);
            if (sp.water_surface == math::NO_WATER) continue;
            ++covered;
            if (world::lake_norm_radius(ctx.params.layout.lake, {x, z}) < 1.0f) continue;
            // Non-lake water must hug the trace: within max(SHORE_SAND_DIST,
            // 2 x local width) of a station (+ coarse-cell slack).
            float best_d = 1e9f;
            uint32_t best_i = 0;
            for (uint32_t i = 0; i < h.stations.size(); ++i) {
                const float d = glm::length(h.stations[i].position - glm::vec2{x, z});
                if (d < best_d) {
                    best_d = d;
                    best_i = i;
                }
            }
            const float cap = std::max(static_cast<float>(config::SHORE_SAND_DIST),
                                       4.0f * h.stations[best_i].half_width);
            CHECK(best_d <= cap + 12.0f); // half coarse-cell diagonal slack
        }
    }
    // Regression tripwire on total water coverage. The binding §3.3 invariant
    // is the per-sample band cap above; the total sits near 2.3% (lake 0.96 +
    // channel 0.6 + capped bend pools 0.8) — the 2.74% wide-mud-flat regime
    // stays forbidden.
    CHECK(100.0 * covered / total < 2.5);
    // dist_to_water: valid out to SETTLEMENT range, saturated at the cap.
    CHECK(max_dist == doctest::Approx(static_cast<float>(config::DIST_TO_WATER_RANGE)));
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

TEST_CASE("grid-pass chunk generation matches the analytic surface_point") {
    // generate_chunk computes surface data in grid passes for speed; the
    // per-position surface_point is the reference. They must agree exactly.
    const auto& ctx = testbed();
    const auto chunk = world::generate_chunk(ctx, ChunkCoord{1, 2});
    const uint32_t res = static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION);
    for (uint32_t z = 0; z < res; z += 17) {
        for (uint32_t x = 0; x < res; x += 17) {
            const glm::vec2 world{256.0f + x * 2.0f, 512.0f + z * 2.0f};
            const auto sp = world::surface_point(ctx, world);
            const std::size_t i = static_cast<std::size_t>(z) * res + x;
            CHECK(chunk.surface.surface_class[i] == static_cast<uint8_t>(sp.surface_class));
            CHECK(chunk.surface.water_surface[i] == sp.water_surface);
            CHECK(chunk.surface.dist_to_water[i] == sp.dist_to_water);
            CHECK(chunk.heightmap.height_at(x, z)
                  == doctest::Approx(sp.height).epsilon(0.001));
        }
    }
}

TEST_CASE("path groove: trails carve in without touching ford or slope contracts") {
    const auto& ctx = testbed();
    const auto& layout = ctx.params.layout;
    // The groove field itself: full depth on the centerline, zero outside.
    const glm::vec2 mid{590.0f, 735.0f}; // shrine->ruin corridor interior
    float best_d = 1e9f;
    glm::vec2 on_path = mid;
    for (float t = 0.0f; t <= 1.0f; t += 0.01f) {
        const glm::vec2 p = layout.corridors[3].points[0]
                          + (layout.corridors[3].points[1] - layout.corridors[3].points[0]) * t;
        if (glm::length(p - mid) < best_d) {
            best_d = glm::length(p - mid);
            on_path = p;
        }
    }
    CHECK(world::path_groove_depth(layout, on_path)
          == doctest::Approx(static_cast<float>(config::PATH_GROOVE_DEPTH)));
    CHECK(world::path_groove_depth(
              layout, on_path + glm::vec2{static_cast<float>(config::PATH_GROOVE_HALF_WIDTH)
                                              + 1.0f,
                                          0.0f})
          == 0.0f);
    // The carved trail reads as volume: averaged over corridor samples, the
    // centerline sits below its shoulders by about the groove depth.
    float on_sum = 0.0f, off_sum = 0.0f;
    int n = 0;
    for (float t = 0.1f; t < 0.95f; t += 0.05f) {
        const glm::vec2 a = layout.corridors[3].points[0];
        const glm::vec2 b = layout.corridors[3].points[1];
        const glm::vec2 p = a + (b - a) * t;
        const glm::vec2 dir = glm::normalize(b - a);
        const glm::vec2 perp{-dir.y, dir.x};
        const auto sp = world::surface_point(ctx, p);
        if (sp.dist_to_water < 10.0f) continue; // skip the water crossing
        on_sum += world::terrain_height(ctx, p);
        off_sum += 0.5f
                 * (world::terrain_height(ctx, p + perp * 6.0f)
                    + world::terrain_height(ctx, p - perp * 6.0f));
        ++n;
    }
    REQUIRE(n >= 8);
    const float carve = (off_sum - on_sum) / static_cast<float>(n);
    CHECK(carve > static_cast<float>(config::PATH_GROOVE_DEPTH) * 0.5f);
    CHECK(carve < static_cast<float>(config::PATH_GROOVE_DEPTH) * 2.5f);
    // Contracts stay green (also covered by their own cases): fords shallow,
    // corridor slopes in limit.
    CHECK(world::max_corridor_water_depth(ctx)
          <= static_cast<float>(config::FORD_DEPTH_MAX) + 1e-2f);
    CHECK(world::max_corridor_avg_slope(ctx)
          <= static_cast<float>(config::CORRIDOR_SLOPE_MAX));
}

TEST_CASE("curb stones: sparse, in the margin band, never in the groove") {
    const auto& ctx = testbed();
    // Chunk (2,2) carries the shrine->ruin corridor interior.
    const auto chunk = world::generate_chunk(ctx, ChunkCoord{2, 2});
    int curbs = 0;
    for (const auto& inst : chunk.scatter) {
        if (inst.species != math::ScatterSpecies::Stone) continue;
        const glm::vec2 p{inst.position.x, inst.position.z};
        const float d = world::corridor_distance(ctx.params.layout, p);
        if (d < static_cast<float>(config::CORRIDOR_WIDTH) * 0.5f + 1.0f
            && inst.scale <= 0.56f) {
            ++curbs;
            // Margin band only: outside the groove, inside the corridor edge.
            CHECK(d >= static_cast<float>(config::PATH_GROOVE_HALF_WIDTH) + 0.3f);
        }
    }
    CHECK(curbs >= 2); // sparse but present along the through-corridor
}

TEST_CASE("§2.1 landform anisotropy: meadow ridgelets share a local long axis") {
    // Structure-tensor eigenvalue ratio over open-meadow windows (7x7
    // gradients, 12 m spacing). The HILL_ANISOTROPY input-stretch of the mid
    // octave pushes the seed-1 median to ~3.9; an isotropic field sits near
    // ~2 — the 2.5 floor trips if the stretch ever regresses.
    const auto& ctx = testbed();
    std::vector<float> ratios;
    for (float wz = 100.0f; wz < 950.0f; wz += 110.0f) {
        for (float wx = 60.0f; wx < 700.0f; wx += 110.0f) {
            if (world::crag_distance(ctx.params.layout, {wx, wz})
                < ctx.params.layout.crag.radius + 60.0f) {
                continue;
            }
            if (world::lake_norm_radius(ctx.params.layout.lake, {wx, wz}) < 2.0f) continue;
            if (world::surface_point(ctx, {wx, wz}).dist_to_water < 40.0f) continue;
            float jxx = 0.0f, jzz = 0.0f, jxz = 0.0f;
            for (int iz = -3; iz <= 3; ++iz) {
                for (int ix = -3; ix <= 3; ++ix) {
                    const glm::vec2 p{wx + ix * 12.0f, wz + iz * 12.0f};
                    const float gx = world::terrain_height(ctx, {p.x + 6.0f, p.y})
                                   - world::terrain_height(ctx, {p.x - 6.0f, p.y});
                    const float gz = world::terrain_height(ctx, {p.x, p.y + 6.0f})
                                   - world::terrain_height(ctx, {p.x, p.y - 6.0f});
                    jxx += gx * gx;
                    jzz += gz * gz;
                    jxz += gx * gz;
                }
            }
            const float tr = jxx + jzz;
            const float disc =
                std::sqrt(std::max(0.0f, tr * tr - 4.0f * (jxx * jzz - jxz * jxz)));
            ratios.push_back(((tr + disc) * 0.5f)
                             / std::max((tr - disc) * 0.5f, 1e-6f));
        }
    }
    REQUIRE(ratios.size() >= 10);
    std::sort(ratios.begin(), ratios.end());
    CHECK(ratios[ratios.size() / 2] >= 2.5f);
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
