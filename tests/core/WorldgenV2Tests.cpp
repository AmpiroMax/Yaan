/*
Created: 09:08:2026 - 11:05:22
Last updated: 18:08:2026 - 12:06:09
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
- 09:08:2026 - 14:41:26: Frame-05 bed fix: NEW invariant — every WaterBed sample is covered by a drawable primitive (lake / pond plane / river ribbon); guards the seed-vs-coverage conflation class of bug.
- 09:08:2026 - 14:49:01: Scatter-in-water fix: NEW invariant — no scatter instance sits in water per water_at, nor under any drawn pond plane (twin of the WaterBed-coverage invariant, from the placement side).
- 09:08:2026 - 15:18:34: Castle suite: terrace cut/pad-surface slope, R3, horizontal-dominant mass order, ford command + barrow band, access ramp (slope/step), Backbarrow sightline, R2/R4, and the C2 castle-contribution check; P4 roster updated (castle elements share one terrace, so pads + castle entities == entities).
- 09:08:2026 - 15:31:04: Rule C2-testbed check on the castle's contribution (the seed-1 layout forms a castle-free crowd at (304,304): hamlet + shrine + lakeshore cave), with all three measures reported in the INFO.
- 09:08:2026 - 15:36:59: Rule C2-testbed now gates the absolute bound (3) plus the large-mass guard (2), keeping the mandatory raw/unexempted disclosure and the castle-contribution check.
- 09:08:2026 - 17:45:08: §6.2: pad accounting restated as the NEW invariant — entities == pads + castle elements + derived entrances — with each entrance checked to carry an explicit carve floor. The old assertion encoded the rule this change replaced.
- 09:08:2026 - 19:33:58: Fortress revision: terrace flatness is measured PER WARD (one box across the chain measures the steps between wards, which are supposed to exist) plus a new check that the chain steps down toward the approach.
- 10:08:2026 - 00:10:41: NEW invariant "one patch of ground carries exactly one water surface" (no cell in two ponds, no cell at two levels, pond_planes == wet non-lake cells with no stacked centres) plus its Rule 30 control, a hand-built pond pair sharing a cell that the same checkers must flag -- including a same-level variant proving the two checkers are not one measurement twice.
- 10:08:2026 - 01:48:11: Flat-reach tests (grill в23 / §3.1 amendment): drawn pond level == swum station level at every station standing in a pond (with a vacuity guard), lake settled plane <= design constant, and the Rule 30 control — Pond::spill_level is the record of what the OLD construction drew, so 'spill - level > 0.5 m somewhere' proves the rejected instance really occurs and the clamp really binds.
- 10:08:2026 - 11:51:23: ScatterSpecies ordinals pinned — render's
  flora_species_of() switches on them for the MESH across a DAG seam, and its
  `default` returns Bush, so an ordinal walking off the mapping draws a forest
  floor of snags and logs as a field of shrubs without failing anything.
- 10:08:2026 - 20:20:20: §5.12 apron acceptance with THREE controls: the rule
  must fire on a giant canopy on the flank, must NOT fire on the same canopy
  far away (the arm that catches the naive global reading), and must NOT fire
  on a ground-hugging canopy on the flank — the rule is about the silhouette,
  not about the massif being off-limits.
- 10:08:2026 - 20:26:55: §5.12's RESIDUAL recorded as a named gap, with the
  apron-disabled counterfactual as its other arm: 300 m west 39.5%->38.4%,
  350 m 45.6%->38.1%, 500 m 42.0%->31.0% of the massif's low silhouette
  hidden by canopies. The apron does real work and more of it with distance,
  but a third of the silhouette is still hidden by trees standing OFF the
  massif — the case the scoping excludes and the global reading over-corrects.
  Asserted at BOTH ENDS so the gap cannot drift silently either way.
- 11:08:2026 - 15:15:55: §2.1 anisotropy: the probe caught a new octave ignoring the land's grain (3.61 -> 2.22, hill octave untouched); re-sampled through the shared axis field it reads 2.92 against a 3.83 counterfactual (DFN_NO_RELIEF=1).
- 13:08:2026 - 20:40:00: P4's pad accounting named a LIST where it meant a CLASS. It enumerated DungeonEntrance as the one site kind never scored onto a pad; WallTorch joined that class with the carve lights (4e1c64d) and broke the identity by exactly its own count, 13. Replaced by a `carve_derived` predicate, and the "floor comes from the carve" check extended to the whole class. Expected counts untouched in either direction -- what changed is which side of the identity a carve-derived site is counted on. Semantics belong to zone `dungeon`; done under the lead's cut in its absence and filed in BOARD.md for confirmation.
- 18:08:2026 - 12:06:09: «grid-pass ... matches the analytic surface_point» брал мировую точку
  как `256.0f + x * 2.0f` — начало чанка руками, шаг решётки голым числом. Из-за
  литерала случай проверял, что HEIGHTMAP_STEP равен 2.0, а не сеточный проход,
  и при переводе шага на 1.0 м покраснел на КАЖДОМ отсчёте, указывая на
  worldgen, который был ни при чём. Позиция выводится из CHUNK_SIZE и
  HEIGHTMAP_STEP. (Настоящий разъезд классов поверхности случай тоже поймал —
  557 утверждений, — и это была честная поломка pass B, см. Worldgen.cpp.)
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenCastle.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenScatter.h"
#include "engine/world/sources/WorldgenSites.h"
#include "engine/world/sources/WorldgenValidation.h"

#include <algorithm>
#include <cstdlib>
#include <doctest/doctest.h>
#include <map>
#include <set>
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

TEST_CASE("every WaterBed sample is covered by a drawable water primitive") {
    // The invariant that closes the stage-3 "wide dark bed" class of bug: if
    // a sample classifies as WaterBed (submerged terrain), some primitive in
    // water_bodies() — the lake, a pond plane, or the river ribbon — must sit
    // over it, or render draws bare bed where water should be. Was violated
    // by fill_level doubling as the distance-field seed set (river trace
    // cells flooded their whole 16 m coarse cell).
    const auto& ctx = testbed();
    const auto& h = ctx.hydrology;
    std::vector<math::LakePlane> planes{h.lake};
    planes.insert(planes.end(), h.pond_planes.begin(), h.pond_planes.end());

    int bed = 0;
    int uncovered = 0;
    glm::vec2 worst{0.0f};
    float worst_gap = 0.0f;
    for (float z = 2.0f; z < 1024.0f; z += 4.0f) {
        for (float x = 2.0f; x < 1024.0f; x += 4.0f) {
            const glm::vec2 p{x, z};
            const auto sp = world::surface_point(ctx, p);
            if (sp.surface_class != math::SurfaceClass::WaterBed) continue;
            ++bed;
            bool covered = false;
            for (const auto& plane : planes) {
                const float dx = (p.x - plane.center.x) / plane.half_extent.x;
                const float dz = (p.y - plane.center.y) / plane.half_extent.y;
                // Ellipse for the lake, its bounding box for pond cells.
                if (dx * dx + dz * dz < 1.0f
                    || (std::fabs(dx) <= 1.0f && std::fabs(dz) <= 1.0f)) {
                    covered = true;
                    break;
                }
            }
            if (!covered) {
                // River ribbon: within the local channel half width.
                float best = std::numeric_limits<float>::max();
                uint32_t bi = 0;
                for (uint32_t i = 0; i < h.stations.size(); ++i) {
                    const float d = glm::length(h.stations[i].position - p);
                    if (d < best) {
                        best = d;
                        bi = i;
                    }
                }
                const float gap = best - h.stations[bi].half_width;
                if (gap <= 0.5f) { // half a sample step of tolerance
                    covered = true;
                } else if (gap > worst_gap) {
                    worst_gap = gap;
                    worst = p;
                }
            }
            if (!covered) ++uncovered;
        }
    }
    REQUIRE(bed > 100); // the testbed does have water
    INFO("worst uncovered bed at (" << worst.x << ", " << worst.y << "), gap " << worst_gap
                                    << " m");
    CHECK(uncovered == 0);
}

TEST_CASE("nothing scattered stands in water (and planes never flood scatter)") {
    // Twin of the WaterBed-coverage invariant, from the other side: no
    // instance may sit in water per water_at, and no instance may sit under a
    // DRAWN water primitive either — a per-pond bounding-box plane used to
    // paint water over dry ground with birches on it (they read ankle-deep).
    const auto& ctx = testbed();
    const auto& h = ctx.hydrology;
    int checked = 0;
    int wet = 0;
    int under_plane = 0;
    for (int cz = 0; cz <= 3; ++cz) {
        for (int cx = 0; cx <= 3; ++cx) {
            const auto chunk = world::generate_chunk(ctx, ChunkCoord{cx, cz});
            for (const auto& inst : chunk.scatter) {
                ++checked;
                const glm::vec2 p{inst.position.x, inst.position.z};
                if (world::surface_point(ctx, p).water_surface != math::NO_WATER) ++wet;
                for (const auto& plane : h.pond_planes) {
                    if (std::fabs(p.x - plane.center.x) <= plane.half_extent.x
                        && std::fabs(p.y - plane.center.y) <= plane.half_extent.y
                        && inst.position.y < plane.surface_height) {
                        ++under_plane;
                        break;
                    }
                }
            }
        }
    }
    REQUIRE(checked > 1000);
    CHECK(wet == 0);
    CHECK(under_plane == 0);
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
    // Pad accounting, stated exactly (§6.1, §6.2). Every ordinary building
    // stands on its own BuildingPad. Sites that do NOT come in two kinds, and
    // the second kind is a CLASS rather than a list:
    //  - the castle's elements share ONE terrace (§6.1);
    //  - CARVE-DERIVED sites are placed from their carve geometry and never
    //    scored onto a pad at all, because the pad scorer looks for flat dry
    //    ground and these things live where there is none. Dungeon entrances
    //    were the first member — a cave mouth cannot exist on flat dry ground,
    //    which is precisely how one marker ended up 10 m from its own passage
    //    and another on the crown of the bluff it should sit under.
    //
    // WALL TORCHES JOINED THAT CLASS and this accounting did not know it, which
    // is the whole of the failure this line used to produce (32 against 19, and
    // 13 is exactly the torch count). They hang on the walls of carved
    // corridors, placed by the same roof predicate the darkness gate uses, so
    // they are derived from the carve in precisely the sense the entrances are
    // — a torch on a corridor wall is not standing on the heightfield at all.
    // The EXPECTED COUNTS BELOW ARE UNTOUCHED: what changed is which side of
    // the identity a carve-derived site is counted on, not how many there are.
    //
    // The semantics here belong to zone `dungeon` (WallTorch arrived with the
    // carve lights in 4e1c64d); this edit is the lead's cut in its absence, and
    // it is flagged in BOARD.md for dungeon to confirm or overturn.
    const auto carve_derived = [](world::SiteType t) {
        return t == world::SiteType::DungeonEntrance || t == world::SiteType::WallTorch;
    };
    std::size_t derived_entrances = 0;
    std::size_t derived_sites = 0;
    for (const world::SiteType type : sites.types) {
        if (type == world::SiteType::DungeonEntrance) ++derived_entrances;
        if (carve_derived(type)) ++derived_sites;
    }
    CHECK(derived_entrances == static_cast<std::size_t>(config::TESTBED_DUNGEONS));
    REQUIRE(sites.entities.size()
            == sites.pads.size() + sites.castle.entities.size() + derived_sites);
    // ...and each really is derived: its floor comes from the carve, not from
    // the heightfield, which cannot report a floor cut below the surface.
    for (std::size_t i = 0; i < sites.entities.size(); ++i) {
        if (!carve_derived(sites.types[i])) continue;
        CHECK(sites.entities[i].ground_y != world::NO_GROUND_Y);
    }

    // Roster: 1 tavern + 1 trader + dwellings/barns within HAMLET_SIZE, one
    // shrine, TESTBED_DUNGEONS entrances, one tower ruin, plus the castle mass.
    int tavern = 0, trader = 0, dwelling = 0, barn = 0, shrine = 0, dungeon = 0, tower = 0;
    int hall = 0, wall = 0, gatehouse = 0, solar = 0;
    for (const world::SiteType t : sites.types) {
        switch (t) {
        case world::SiteType::Tavern: ++tavern; break;
        case world::SiteType::Trader: ++trader; break;
        case world::SiteType::Dwelling: ++dwelling; break;
        case world::SiteType::Barn: ++barn; break;
        case world::SiteType::Shrine: ++shrine; break;
        case world::SiteType::DungeonEntrance: ++dungeon; break;
        case world::SiteType::TowerRuin: ++tower; break;
        case world::SiteType::CastleHall: ++hall; break;
        case world::SiteType::CastleWall: ++wall; break;
        case world::SiteType::CastleGatehouse: ++gatehouse; break;
        case world::SiteType::CastleSolar: ++solar; break;
        }
    }
    // The hall-castle mass (§6.1.3): one of each, horizontal-dominant.
    CHECK(hall == static_cast<int>(config::CASTLE_COUNT_TESTBED));
    CHECK(wall == static_cast<int>(config::CASTLE_COUNT_TESTBED));
    CHECK(gatehouse == static_cast<int>(config::CASTLE_COUNT_TESTBED));
    CHECK(solar == static_cast<int>(config::CASTLE_COUNT_TESTBED));
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
    // Pads are the first entries; castle records are appended after them.
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

TEST_CASE("castle: terrace, R3 skyline margin and siting (§6.1)") {
    const auto& ctx = testbed();
    const auto& castle = ctx.sites.castle;
    REQUIRE(castle.valid);

    // Terrace: the CUT is the ruled exception; the pad SURFACE still obeys
    // BUILDING_PAD_SLOPE_MAX.
    CHECK(castle.cut <= static_cast<float>(config::CASTLE_PAD_CUT_MAX) + 1e-3f);
    // Flatness is a property of EACH WARD's own surface. The fortress is a
    // chain of terraces stepping down the spur, so sampling one box across the
    // whole span measures the steps BETWEEN wards, which are supposed to exist.
    float worst_pad_slope = 0.0f;
    for (int wi = 0; wi < castle.ward_count; ++wi) {
        const auto& ward = castle.wards[wi];
        const float reach = ward.half_size - 6.0f;
        for (float z = -reach; z <= reach; z += 4.0f) {
        for (float x = -reach; x <= reach; x += 4.0f) {
            const glm::vec2 p = ward.center + glm::vec2{x, z};
            const float d = 2.0f;
            const float hx = world::terrain_height(ctx, {p.x + d, p.y})
                           - world::terrain_height(ctx, {p.x - d, p.y});
            const float hz = world::terrain_height(ctx, {p.x, p.y + d})
                           - world::terrain_height(ctx, {p.x, p.y - d});
            worst_pad_slope =
                std::max(worst_pad_slope,
                         std::atan(std::sqrt(hx * hx + hz * hz) / (2.0f * d)));
        }
        }
    }
    CHECK(worst_pad_slope <= static_cast<float>(config::BUILDING_PAD_SLOPE_MAX));
    // The chain must actually STEP DOWN toward the approach.
    for (int wi = 1; wi < castle.ward_count; ++wi) {
        CHECK(castle.wards[wi].height < castle.wards[wi - 1].height);
    }

    // R3: pad + tallest element stays CASTLE_SKYLINE_MARGIN below the L0.
    const float peak = world::terrain_height(ctx, ctx.params.layout.crag.center);
    CHECK(castle.top_elevation()
          <= peak - static_cast<float>(config::CASTLE_SKYLINE_MARGIN) + 1e-3f);
    // Horizontal-dominant mass (§6.1.3): the solar is the single vertical and
    // the wall band is the lowest step.
    CHECK(castle.solar_height > castle.hall_height);
    CHECK(castle.hall_height > castle.wall_height);

    // Siting: commands a derived ford, stands over the barrow, and never
    // creates water features of its own.
    float d_ford = 1e9f;
    for (const uint32_t f : ctx.hydrology.ford_stations) {
        d_ford = std::min(d_ford,
                          glm::length(ctx.hydrology.stations[f].position - castle.center));
    }
    CHECK(d_ford <= static_cast<float>(config::CASTLE_FORD_COMMAND_DIST));
    float d_barrow = 1e9f;
    for (std::size_t i = 0; i < ctx.sites.entities.size(); ++i) {
        if (ctx.sites.types[i] != world::SiteType::DungeonEntrance) continue;
        d_barrow = std::min(d_barrow,
                            glm::length(ctx.sites.entities[i].position_xz - castle.center));
    }
    CHECK(d_barrow >= static_cast<float>(config::CASTLE_BARROW_DIST_MIN));
    CHECK(d_barrow <= static_cast<float>(config::CASTLE_BARROW_DIST_MAX));
}

TEST_CASE("castle: access ramp and Backbarrow sightline (binding invariants)") {
    const auto& ctx = testbed();
    const auto access = world::castle_access(ctx);
    // ACCESS INVARIANT (§6.1.2): a commoner walks up from the corridor.
    CHECK(access.ramp_avg_slope <= static_cast<float>(config::CORRIDOR_SLOPE_MAX));
    CHECK(access.ramp_max_step <= static_cast<float>(config::PLAYER_STEP_HEIGHT));
    // BARROW SIGHTLINE (§6.1.2): the seat lives within sight of the evidence.
    CHECK(access.barrow_visible_from_yard);
    CHECK(access.barrow_visible_from_gate);
}

TEST_CASE("castle: hierarchy — the crag stays L0 (§6.1.1)") {
    const auto& ctx = testbed();
    const auto h = world::castle_hierarchy(ctx);
    // R4: at valley range the castle never rivals the crag's silhouette.
    CHECK(h.max_ratio <= static_cast<float>(config::CASTLE_SILHOUETTE_RATIO));
    // R2: flank occlusion is the desired read; crown occlusion is forbidden.
    CHECK_FALSE(h.crown_occluded);
    // The castle must not push any standpoint over the attractor bound — its
    // own contribution is what this pass gates (design ruling: the absolute
    // POI_VISIBLE_COUNT_MAX_REGION is region-scale only and unsatisfiable on
    // the testbed, where C3 packs POIs at ~3x region density).
    CHECK(h.max_attractors == h.max_attractors_without_castle);
    INFO("attractors with castle " << h.max_attractors << ", baseline "
                                   << h.max_attractors_without_castle
                                   << ", region-only bound "
                                   << config::POI_VISIBLE_COUNT_MAX_REGION);

    // RULE C2-TESTBED ("no coequal crowd"): at most POI_COEQUAL_VISIBLE_MAX
    // attractors of comparable apparent size (within COEQUAL_ANGLE_RATIO of
    // each other in subtended height) from any standpoint. The L0 is exempt;
    // composite POIs count once; attractors read against the L0's body are
    // exempt by R1; sub-readable specks (§1.5) do not compete.
    //
    // As with the absolute bound, the seed-1 LAYOUT already forms a crowd the
    // castle is not part of (hamlet + shrine + lakeshore cave from the meadow
    // around (304,304) — the castle is R1-exempt there, reading against the
    // crag). So the castle pass gates on its own CONTRIBUTION and the absolute
    // number is reported to design.
    CHECK(h.max_coequal_visible <= static_cast<uint32_t>(config::POI_COEQUAL_VISIBLE_MAX));
    // LARGE-MASS GUARD: the limit tightens to 2 when every member of the group
    // is a mass (>= COEQUAL_LARGE_PX), not a mark at the readability floor.
    CHECK(h.max_coequal_large <= 2);
    // The castle must still not be the one that creates a crowd.
    CHECK(h.max_coequal_visible == h.max_coequal_visible_without_castle);
    INFO("coequal crowd: " << h.max_coequal_visible << " (raw, no R1 exemption: "
                           << h.max_coequal_visible_raw << "; without castle: "
                           << h.max_coequal_visible_without_castle << "; large-mass group: "
                           << h.max_coequal_large << "), bound "
                           << config::POI_COEQUAL_VISIBLE_MAX);
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
    // THE SAMPLE'S WORLD POSITION IS DERIVED, NOT TYPED. It was
    // `256.0f + x * 2.0f`: the chunk origin from CHUNK_SIZE by hand and the
    // lattice step as a bare literal. The literal made this case a test of
    // HEIGHTMAP_STEP being 2.0 rather than of the grid pass, and it failed on
    // every sample the moment the step moved — pointing at worldgen, which was
    // innocent.
    const float step = static_cast<float>(config::HEIGHTMAP_STEP);
    const glm::vec2 chunk_origin{static_cast<float>(config::CHUNK_SIZE) * 1.0f,
                                 static_cast<float>(config::CHUNK_SIZE) * 2.0f};
    for (uint32_t z = 0; z < res; z += 17) {
        for (uint32_t x = 0; x < res; x += 17) {
            const glm::vec2 world = chunk_origin
                                  + glm::vec2{static_cast<float>(x) * step,
                                              static_cast<float>(z) * step};
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
    // octave pushes the seed-1 median to ~3.8; an isotropic field sits near
    // ~2 — the 2.5 floor trips if the stretch ever regresses.
    //
    // IT ALSO TRIPS IF A NEW OCTAVE IGNORES THE GRAIN, and that is what it
    // caught. §2.7's meso octave (25-60 m) first went in sampled isotropically
    // and this median fell 3.61 -> 2.22 with the hill octave and
    // HILL_ANISOTROPY both untouched: an isotropic layer laid over ridgelets
    // does not sit beside the grain, it erases it. Re-sampled through
    // aniso_octave_sample the same octave reads 2.92 — the ground is bumpier
    // AND still has one local long axis. The counterfactual is one env away
    // (DFN_NO_RELIEF=1 -> 3.83), so the cost of the bumpiness is visible
    // rather than hidden inside a pass.
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
    MESSAGE("hill-band anisotropy median " << ratios[ratios.size() / 2]);
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

namespace {

/// Cells claimed by more than one pond. A cell belongs to exactly one body of
/// water; anything else means the pond fill duplicated it rather than handing
/// it over when the water rose.
[[nodiscard]] std::size_t cells_in_two_ponds(const world::HydrologyData& h) {
    std::map<uint32_t, int> owners;
    for (const world::Pond& p : h.ponds) {
        for (const uint32_t c : p.cells) ++owners[c];
    }
    return static_cast<std::size_t>(
        std::count_if(owners.begin(), owners.end(), [](const auto& kv) { return kv.second > 1; }));
}

/// Cells claimed at more than one water LEVEL. This is the half that reaches
/// the player: two coplanar planes at different heights over one patch of
/// ground means the drawn surface depends on draw order and can disagree with
/// the height the swimmer floats at.
[[nodiscard]] std::size_t cells_at_two_levels(const world::HydrologyData& h) {
    std::map<uint32_t, std::set<int>> levels;
    for (const world::Pond& p : h.ponds) {
        for (const uint32_t c : p.cells) levels[c].insert(static_cast<int>(std::lround(p.level * 100.0f)));
    }
    return static_cast<std::size_t>(std::count_if(
        levels.begin(), levels.end(), [](const auto& kv) { return kv.second.size() > 1; }));
}

} // namespace

TEST_CASE("one patch of ground carries exactly one water surface") {
    // THE DEFECT THIS PINS: the trace revisits ground it has already flooded,
    // and every revisit used to APPEND the cell to another pond carrying its
    // own stale level. Measured before the fix on the 2x2 km world: 17335 cell
    // entries over 1042 distinct cells, one cell claimed by 36 ponds, 94.5% of
    // cells claimed at more than one level, and 17336 drawable planes summing
    // to 106% of the world's area against 7.01% real water. It exhausted every
    // GPU buffer handle at startup, which is how it was found -- but the
    // expensive half was that the drawn water level and the swum water level
    // were two copies of one fact, free to disagree.
    for (const int side : {4, 8}) {
        CAPTURE(side);
        const world::WorldGenContext ctx = world::build_world_context(
            WorldGenParams{1, {0, 0}, {side - 1, side - 1}, world::TestbedLayout{}});
        const world::HydrologyData& h = ctx.hydrology;

        CHECK(cells_in_two_ponds(h) == 0);
        CHECK(cells_at_two_levels(h) == 0);

        // The drawable primitives ARE the coverage truth, not a parallel copy:
        // one plane per wet non-lake cell, each carrying that cell's fill level
        // exactly. This is the assertion that would have caught 17336.
        std::size_t wet_cells = 0;
        for (uint32_t c = 0; c < h.fill_level.size(); ++c) {
            if (h.fill_level[c] == math::NO_WATER) continue;
            const glm::vec2 p = h.grid_origin
                              + glm::vec2{static_cast<float>(c % h.grid_w),
                                          static_cast<float>(c / h.grid_w)}
                                    * static_cast<float>(config::WORLDGEN_HYDRO_GRID_STEP);
            if (world::lake_norm_radius(ctx.params.layout.lake, p) < 1.0f) continue;
            ++wet_cells;
        }
        CHECK(h.pond_planes.size() == wet_cells);

        std::set<std::pair<int, int>> centres;
        for (const math::LakePlane& L : h.pond_planes) {
            centres.insert({static_cast<int>(std::lround(L.center.x)),
                            static_cast<int>(std::lround(L.center.y))});
        }
        CHECK(centres.size() == h.pond_planes.size()); // no stacking
    }
}

TEST_CASE("the water-surface uniqueness checks reject a world that violates them") {
    // Rule 30: the case the test above exists to REJECT, run against the same
    // checkers, which must FAIL it. Without this the two CHECKs above are a
    // description of whatever the generator happens to emit -- and they would
    // have read green on a generator with no ponds at all.
    world::HydrologyData bad;
    world::Pond a;
    a.level = 10.0f;
    a.cells = {7, 8, 9};
    world::Pond b;
    b.level = 14.0f;  // the same ground, claimed again at a higher level
    b.cells = {9, 10};
    bad.ponds = {a, b};

    CHECK(cells_in_two_ponds(bad) == 1);
    CHECK(cells_at_two_levels(bad) == 1);

    // And the same-level variant: duplicated ownership WITHOUT a level
    // conflict is still a duplicate plane, so the two checkers are not
    // measuring one thing twice.
    world::HydrologyData same_level = bad;
    same_level.ponds[1].level = 10.0f;
    CHECK(cells_in_two_ponds(same_level) == 1);
    CHECK(cells_at_two_levels(same_level) == 0);
}

namespace {

/// Coarse-cell index of a world position on the hydrology grid, or INVALID.
[[nodiscard]] uint32_t hydro_cell_of(const world::HydrologyData& h, glm::vec2 p) {
    const float cell = static_cast<float>(config::WORLDGEN_HYDRO_GRID_STEP);
    const int32_t x = static_cast<int32_t>(std::floor((p.x - h.grid_origin.x) / cell));
    const int32_t z = static_cast<int32_t>(std::floor((p.y - h.grid_origin.y) / cell));
    if (x < 0 || z < 0 || x >= static_cast<int32_t>(h.grid_w)
        || z >= static_cast<int32_t>(h.grid_h)) {
        return std::numeric_limits<uint32_t>::max();
    }
    return static_cast<uint32_t>(z) * h.grid_w + static_cast<uint32_t>(x);
}

} // namespace

TEST_CASE("the pond is a flat reach: drawn level equals swum level at every station") {
    // grill в23 / §3.1 amendment (design-ratified): a pond is a flat reach of
    // the river. The monotone pass does not descend through it — a station
    // inside a pond carries EXACTLY the level the pond is drawn at, so the
    // water the player sees and the water the player swims are one number.
    // Before the amendment the drawn plane sat at the spill saddle while the
    // stations swam at min(entry, spill): 7.98 m apart at the largest pond.
    for (const int side : {4, 8}) {
        CAPTURE(side);
        const world::WorldGenContext ctx = world::build_world_context(
            WorldGenParams{1, {0, 0}, {side - 1, side - 1}, world::TestbedLayout{}});
        const world::HydrologyData& h = ctx.hydrology;
        REQUIRE(h.ok);

        std::map<uint32_t, float> pond_level_of_cell;
        for (const world::Pond& p : h.ponds) {
            for (const uint32_t c : p.cells) pond_level_of_cell[c] = p.level;
        }
        int stations_in_ponds = 0;
        float worst = 0.0f;
        for (const math::RiverStation& st : h.stations) {
            const auto it = pond_level_of_cell.find(hydro_cell_of(h, st.position));
            if (it == pond_level_of_cell.end()) continue;
            ++stations_in_ponds;
            worst = std::max(worst, std::fabs(st.surface_height - it->second));
            CHECK(std::fabs(st.surface_height - it->second) <= 1e-3f);
            // The drawn plane reads the same array the swimmer reads.
            CHECK(h.fill_level[it->first] == doctest::Approx(it->second));
        }
        MESSAGE("side " << side << ": " << stations_in_ponds
                        << " stations inside ponds, worst drawn-vs-swum gap " << worst
                        << " m");
        // Rule 30a: the invariant needs subjects — a world where no station
        // ever stands in a pond would pass vacuously and prove nothing.
        REQUIRE(stations_in_ponds > 0);

        // §3.2 extension: the lake is a flat reach too — its settled plane
        // never sits above the design constant, and segment 1 (the outlet)
        // starts at or below it.
        CHECK(h.lake.surface_height <= LAKE_LEVEL + 1e-4f);
        if (h.segment_offsets.size() > 2) {
            CHECK(h.stations[h.segment_offsets[1]].surface_height
                  <= h.lake.surface_height + 1e-4f);
        }
    }
}

TEST_CASE("the impossible pond is unconstructible — and the old construction built one") {
    // Rule 30 with the REAL rejected instance as the control. Pond::spill_level
    // records the saddle height the OLD construction drew the pond at; the new
    // level is min(spill, river entry). So "spill_level - level > 0" is not a
    // synthetic case: it is the measured record, on this very world, of a pond
    // the old code drew ABOVE the river that feeds it — the player saw water
    // at spill_level and swam at level. The threshold sits below the recorded
    // 7.98 m instance and above float noise.
    float worst = 0.0f;
    int clamped = 0;
    for (const int side : {4, 8}) {
        CAPTURE(side);
        const world::WorldGenContext ctx = world::build_world_context(
            WorldGenParams{1, {0, 0}, {side - 1, side - 1}, world::TestbedLayout{}});
        for (const world::Pond& p : ctx.hydrology.ponds) {
            // Unconstructible: no pond ever sits above its own spill saddle,
            // and (via the flat-reach test above) none above its entry level.
            CHECK(p.level <= p.spill_level + 1e-4f);
            if (p.spill_level - p.level > 0.01f) {
                ++clamped;
                worst = std::max(worst, p.spill_level - p.level);
            }
        }
    }
    MESSAGE(clamped << " ponds clamped below their spill saddle, worst " << worst << " m");
    // The control must have teeth: at least one pond on these worlds was
    // genuinely lowered, by a gap a player would notice (> 0.5 m). If this
    // ever fails, the clamp stopped binding and the flat-reach test above has
    // gone vacuous on the pond-vs-spill axis.
    REQUIRE(clamped > 0);
    CHECK(worst > 0.5f);
}

TEST_CASE("ScatterSpecies ordinals are a cross-zone contract and are pinned") {
    // render::flora_species_of() switches on these to pick a MESH, across a DAG
    // seam where neither declaration can see the other. A renumber does not
    // fail to build — it redresses the world, and worse, that function's
    // `default` returns Bush, so an ordinal that walks off the end of the
    // mapping silently draws a field of shrubs where the forest floor was.
    // This test is the only thing standing between those two facts.
    using math::ScatterSpecies;
    CHECK(static_cast<uint8_t>(ScatterSpecies::OakTree) == 0);
    CHECK(static_cast<uint8_t>(ScatterSpecies::PineTree) == 1);
    CHECK(static_cast<uint8_t>(ScatterSpecies::BirchTree) == 2);
    CHECK(static_cast<uint8_t>(ScatterSpecies::Bush) == 3);
    CHECK(static_cast<uint8_t>(ScatterSpecies::Stone) == 4);
    CHECK(static_cast<uint8_t>(ScatterSpecies::Snag) == 5);
    CHECK(static_cast<uint8_t>(ScatterSpecies::SnagPale) == 6);
    CHECK(static_cast<uint8_t>(ScatterSpecies::BigBush) == 7);
    CHECK(static_cast<uint8_t>(ScatterSpecies::FallenLog) == 8);
    CHECK(static_cast<uint8_t>(ScatterSpecies::Deadfall) == 9);
    CHECK(static_cast<uint8_t>(ScatterSpecies::MossPatch) == 10);
    CHECK(static_cast<uint8_t>(ScatterSpecies::FlowerCarpet) == 11);
    CHECK(static_cast<uint8_t>(ScatterSpecies::FlowerAccent) == 12);
    CHECK(static_cast<uint8_t>(ScatterSpecies::FlowerJewel) == 13);
    CHECK(static_cast<uint8_t>(ScatterSpecies::FlowerUmbel) == 14);
    CHECK(static_cast<uint8_t>(ScatterSpecies::Mushroom) == 15);
    CHECK(static_cast<uint8_t>(ScatterSpecies::PebbleCluster) == 16);
    CHECK(static_cast<uint8_t>(ScatterSpecies::StuntedPine) == 17);
}

TEST_CASE("§5.12: the forest does not stand on the massif's apron") {
    // DESIGN RULED FOR THE APRON because the forest was eating the mountain:
    // with scatter suppressed the west 300 m frame shows a pointed tor, band
    // lips and a shoulder break; with the trees on it is a low featureless
    // hump. The mechanism is that a mountain missing its bottom third loses the
    // bench and the flare, and what survives is the upper cap — convex on ANY
    // mountain, which is the dome. No shape change can fix it; only clearing
    // the foot can.
    //
    // The rule is a HEIGHT rule at the massif foot and the RADIUS IS AN OUTPUT.
    // Measured seed 1: the apron reaches 162 m at its tightest bearing, against
    // a pine annulus that starts at 140 m — i.e. pines were starting ON the
    // foot, inside the 120-162 m hem where the flank is still climbing.
    const world::WorldGenParams params{1, {0, 0}, {3, 3}};
    const world::WorldGenContext ctx = world::build_world_context(params);
    const world::TestbedLayout& lay = ctx.params.layout;
    const auto CH = static_cast<float>(config::CHUNK_SIZE);
    const auto GIANT = static_cast<float>(config::TREE_MATURITY_GIANT_MULT_MAX);

    std::vector<math::ScatterInstance> trees;
    for (int cz = 0; cz < 4; ++cz) {
        for (int cx = 0; cx < 4; ++cx) {
            const auto s = world::build_scatter(ctx, {static_cast<float>(cx) * CH, static_cast<float>(cz) * CH}, {static_cast<float>(cx + 1) * CH, static_cast<float>(cz + 1) * CH});
            for (const math::ScatterInstance& i : s) {
                if (i.species == math::ScatterSpecies::OakTree
                    || i.species == math::ScatterSpecies::PineTree
                    || i.species == math::ScatterSpecies::BirchTree) {
                    trees.push_back(i);
                }
            }
        }
    }
    REQUIRE(trees.size() > 1500);

    // THE CLAIM: nothing shipped stands on the apron.
    const auto species_max_h = [](math::ScatterSpecies s) {
        switch (s) {
        case math::ScatterSpecies::PineTree:
            return static_cast<float>(config::PINE_HEIGHT_MAX);
        case math::ScatterSpecies::BirchTree:
            return static_cast<float>(config::BIRCH_HEIGHT_MAX);
        default:
            return static_cast<float>(config::OAK_HEIGHT_MAX);
        }
    };
    int on_apron = 0;
    for (const math::ScatterInstance& t : trees) {
        const glm::vec2 p{t.position.x, t.position.z};
        if (world::breaks_massif_apron(params.seed, lay.crag, p,
                                       t.position.y + species_max_h(t.species) * GIANT)) {
            ++on_apron;
        }
    }
    INFO("trees standing on the apron: ", on_apron, " of ", trees.size());
    CHECK(on_apron == 0);

    // CONTROL 1 — THE RULE MUST BE ABLE TO FIRE (Rule 30a). A giant canopy
    // placed on the massif's own flank is exactly what the rule exists to
    // reject, and if this ever stops being rejected the predicate has been
    // scoped out of existence rather than satisfied.
    const glm::vec2 flank = lay.crag.center + glm::vec2{lay.crag.radius * 0.5f, 0.0f};
    const float flank_h = world::terrain_height(ctx, flank);
    CHECK(world::breaks_massif_apron(params.seed, lay.crag, flank,
                                     flank_h + static_cast<float>(config::PINE_HEIGHT_MAX)));
    // CONTROL 2 — AND IT MUST NOT FIRE EVERYWHERE. The same tall canopy far
    // from the massif is legal; a rule that rejected it would be deleting the
    // forest rather than clearing the foot. This is the arm that would have
    // caught the naive reading of design's sentence, which excludes every tree
    // within ~670 m of a standpoint because a tree in front of your face
    // obscures a mountain too.
    const glm::vec2 away = lay.crag.center + glm::vec2{lay.crag.radius * 6.0f, 0.0f};
    CHECK_FALSE(world::breaks_massif_apron(params.seed, lay.crag, away,
                                           world::terrain_height(ctx, away) + 60.0f));
    // CONTROL 3 — A GROUND-HUGGING CANOPY ON THE FLANK IS LEGAL. The rule is
    // about the SILHOUETTE, not about the massif being off-limits: stunted
    // pines and scrub below the cliffline are exactly what §5.12 wants there.
    CHECK_FALSE(world::breaks_massif_apron(params.seed, lay.crag, flank, flank_h + 0.5f));

    // AND THE FOREST STILL EXISTS. Clearing the foot must not have emptied the
    // annulus — a bare ring would be worse than the forest (design's words).
    int in_annulus = 0;
    for (const math::ScatterInstance& t : trees) {
        const float d = glm::length(glm::vec2{t.position.x, t.position.z} - lay.crag.center);
        if (d >= lay.forests.pine_annulus_r0 && d < lay.forests.pine_annulus_r1) {
            ++in_annulus;
        }
    }
    INFO("trees surviving in the pine annulus: ", in_annulus);
    CHECK(in_annulus > 100);

    // §5.12 IS NOT CLOSED BY THIS, AND THE RESIDUAL IS RECORDED RATHER THAN
    // LEFT TO BE INFERRED. Design's acceptance is "the forest does not eat the
    // massif base in the valley frame". Measured from the west, counting how
    // much of the massif's own sub-cliffline surface is hidden behind tree
    // canopies, with the apron disabled as the counterfactual arm:
    //
    //   from 300 m west:  39.5% hidden -> 38.4%   (apron off -> on)
    //   from 350 m west:  45.6% hidden -> 38.1%
    //   from 500 m west:  42.0% hidden -> 31.0%
    //
    // So the apron does real work and does more of it with distance, but
    // ROUGHLY A THIRD OF THE LOW SILHOUETTE IS STILL HIDDEN. The trees doing
    // it stand OFF the massif's stamp, between the viewer and the mountain —
    // exactly the case the scoping decision above excludes, and exactly the
    // case the naive global reading over-corrects into a ~670 m clearcut.
    // Neither reading is right and the middle is design's to rule, not mine to
    // invent: this is a NAMED GAP (§5.11's habit), not an exemption.
    //
    // The gap is asserted at BOTH ENDS so it cannot drift silently in either
    // direction: if it ever falls below 15% something has closed §5.12 and this
    // case must be rewritten into the real gate; if it climbs past 50% the
    // apron has stopped working and that is a regression.
    const glm::vec2 west_eye = lay.crag.center - glm::vec2{350.0f, 0.0f};
    const float eye_y = world::terrain_height(ctx, west_eye)
                      + static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const float datum = world::terrain_height(ctx, lay.crag.center)
                      - static_cast<float>(config::L0_RELIEF);
    const float cliff_y = datum + static_cast<float>(config::L0_RELIEF)
                                      * static_cast<float>(config::MASSIF_CLIFFLINE_FRAC);
    int samples = 0;
    int hidden = 0;
    for (int b = -30; b <= 30; ++b) {
        const glm::vec2 dir{std::cos(static_cast<float>(b) * 0.01f),
                            std::sin(static_cast<float>(b) * 0.01f)};
        for (float r = lay.crag.radius; r > 5.0f; r -= 4.0f) {
            const glm::vec2 pt = lay.crag.center - dir * r;
            const float h = world::terrain_height(ctx, pt);
            if (h > cliff_y || h < datum + 2.0f) continue;
            ++samples;
            for (const math::ScatterInstance& t : trees) {
                const glm::vec2 seg = pt - west_eye;
                const float len2 = glm::dot(seg, seg);
                if (len2 < 1e-3f) continue;
                const glm::vec2 c{t.position.x, t.position.z};
                float u = glm::dot(c - west_eye, seg) / len2;
                u = std::clamp(u, 0.0f, 1.0f);
                const glm::vec2 off = c - (west_eye + seg * u);
                const float rad = 0.65f * t.scale; // flora's mid-ray bole
                if (glm::dot(off, off) > rad * rad) continue;
                if (eye_y + (h - eye_y) * u
                    <= t.position.y + species_max_h(t.species) * t.scale * GIANT) {
                    ++hidden;
                    break;
                }
            }
        }
    }
    REQUIRE(samples > 200);
    const float frac_hidden = static_cast<float>(hidden) / static_cast<float>(samples);
    INFO("low silhouette hidden by trees from 350 m west: ", frac_hidden);
    CHECK(frac_hidden < 0.50f); // regression guard
    CHECK(frac_hidden > 0.15f); // the NAMED GAP: §5.12 is not closed
}
