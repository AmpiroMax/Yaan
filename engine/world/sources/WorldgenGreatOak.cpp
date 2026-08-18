/*
Created: 12:08:2026 - 22:49:00
Last updated: 18:08:2026 - 18:55:48
Module: engine/world
File: engine/world/sources/WorldgenGreatOak.cpp

Responsibility:
- Implementation of the great oak's placement pass (see the header for the
  contract and for why every number here is derived).

Key items:
- place_great_oaks(), the derived sizes, the two queries.

Dependencies:
- Uses: WorldgenGreatOak.h, WorldgenPlacement.h, WorldgenMacro.h,
  WorldgenHydrology.h, Worldgen.h, config.
- Used by: Worldgen.cpp, WorldgenScatter.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): the search is a fixed lattice of fixed candidate
  counts read in a fixed order from a seeded stream. Do not make it depend on
  iteration order of anything, on a chunk, or on floating-point tie-breaks that
  a compiler may reassociate.
*/
/*
UPD:
- 12:08:2026 - 22:49:00: Created.
- 18:08:2026 - 18:55:48: The pass's cost measured and recorded (~0.6 s per world
  build, two arms of one binary). Not optimised: the obvious saving moves the
  chosen site, and the acceptance frames are shot against the site.
*/

#include "engine/world/sources/WorldgenGreatOak.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenHydrology.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenNoise.h"
#include "engine/world/sources/WorldgenPlacement.h"
#include "engine/world/sources/WorldgenScatter.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

constexpr float TAU = 6.28318530717958647692f;

/// The great oak's own placement stream, keyed by candidate cell.
WorldGenRng oak_cell_rng(uint64_t seed, int64_t gx, int64_t gz) {
    uint64_t s = noise::mix64(seed ^ (0xC0FFEE5CA77E12ull + STREAM_GREAT_OAK));
    s = noise::mix64(s ^ static_cast<uint64_t>(gx));
    s = noise::mix64(s ^ static_cast<uint64_t>(gz));
    return WorldGenRng{s};
}

/// How many candidate positions are drawn inside one separation cell. It buys
/// resolution of the search and nothing else — the ACCEPTED site is decided by
/// the rules below, never by the draw count.
constexpr int CANDIDATES_PER_CELL = 1024;

/// THE PASS'S COST, MEASURED RATHER THAN ASSUMED: ~0.6 s per world build
/// (33.34 s against 32.72 s for the same suite run with DFN_NO_GREAT_OAK=1,
/// one machine, one binary). Every test that builds the testbed world and
/// every app start pays it once. Written down rather than fixed: the obvious
/// saving — running the crown-ring gates only on the current best candidate
/// instead of on every legal one — would change which site wins ties, and that
/// silently moves a landmark the acceptance frames are already shot against.
/// Whoever spends it must re-shoot docs/acceptance/core-great-oak-*.


} // namespace

float great_oak_height_m() {
    return static_cast<float>(config::OAK_HEIGHT_MAX)
         * static_cast<float>(config::TREE_MATURITY_GIANT_MULT_MAX);
}

float great_oak_crown_radius_m() { return great_oak_height_m(); }

float great_oak_clearing_radius_m() {
    // The neighbour's crown radius is half the forest lattice spacing: that is
    // what «crowns now touch» means at TREE_SPACING_FOREST, and it is flora's
    // own measured description of the lattice the giants were lost in.
    const float neighbour_crown_r = static_cast<float>(config::TREE_SPACING_FOREST_MIN
                                                       + config::TREE_SPACING_FOREST_MAX)
                                  * 0.25f;
    return great_oak_crown_radius_m() + neighbour_crown_r;
}

float great_oak_separation_m() { return readable_distance_m(2.0f * great_oak_crown_radius_m()); }

bool in_great_oak_clearing(std::span<const GreatOakSite> sites, glm::vec2 world) {
    for (const GreatOakSite& s : sites) {
        if (glm::length(world - s.pos) < s.clearing_radius) return true;
    }
    return false;
}

float great_oak_canopy_at(std::span<const GreatOakSite> sites, glm::vec2 world) {
    float top = 0.0f;
    for (const GreatOakSite& s : sites) {
        if (glm::length(world - s.pos) < s.crown_radius) {
            top = std::max(top, s.height);
        }
    }
    return top;
}

std::vector<GreatOakSite> place_great_oaks(const WorldGenContext& ctx) {
    std::vector<GreatOakSite> out;
    // THE ZERO-DOSE ARM (Rule 30/48), in the shipped binary and not in a
    // rebuild: no giants, therefore no clearings, therefore the world flora
    // photographed. Everything else in the frame is identical by construction.
    if (std::getenv("DFN_NO_GREAT_OAK") != nullptr) return out;

    const uint64_t seed = ctx.params.seed;
    const TestbedLayout& layout = ctx.params.layout;
    // The §8.1 forest stand is a MEASURED stand: its per-hectare acceptances
    // divide by its own ground, and lifting a hectare of oaks out of it would
    // move BR-3/BR-5's denominators without anybody asking for it. The giants
    // land on the world stands; the stand's own giant is design's call.
    if (layout.stand != StandId::Testbed) return out;

    const float height = great_oak_height_m();
    const float crown_r = great_oak_crown_radius_m();
    const float clearing_r = great_oak_clearing_radius_m();
    const float separation = great_oak_separation_m();

    const glm::vec2 domain_min{static_cast<float>(ctx.params.min_chunk.x)
                                   * static_cast<float>(config::CHUNK_SIZE),
                               static_cast<float>(ctx.params.min_chunk.z)
                                   * static_cast<float>(config::CHUNK_SIZE)};
    const glm::vec2 domain_max{static_cast<float>(ctx.params.max_chunk.x + 1)
                                   * static_cast<float>(config::CHUNK_SIZE),
                               static_cast<float>(ctx.params.max_chunk.z + 1)
                                   * static_cast<float>(config::CHUNK_SIZE)};

    const auto pre_p4_ground = [&](glm::vec2 p) {
        return water_at(ctx.hydrology, layout, p, macro_height(seed, layout, p)).height;
    };
    const SightWedges wedges = build_sight_wedges(layout, pre_p4_ground);

    const auto ground = [&](glm::vec2 p) { return terrain_height(ctx, p); };
    const auto dry_enough = [&](glm::vec2 p, float margin) {
        const WaterSample w = water_at(ctx.hydrology, layout, p, macro_height(seed, layout, p));
        return w.water_surface == math::NO_WATER && w.dist_to_water >= margin;
    };

    // THE CANDIDATE'S GATES. Each is somebody else's existing rule, applied at
    // the giant's own scale — that scale is the only new thing here.
    const auto legal = [&](glm::vec2 p, float& out_ground) {
        // THE ANCHOR, and the first cut did not have it: A GREAT OAK IS AN OAK
        // and stands in the oak's own domain (§5.1's forest masses).
        //
        // WITHOUT IT THE PASS PUT THE GIANT AT (1740, 201) — the flattest legal
        // ground in a 2 x 2 km world, which is the empty quarter no layout
        // authors and no tree grows in. Every assertion passed and the CONTROL
        // ARM READ ZERO: the clearing had cleared nothing, because there was
        // nothing there. That is Rule 27's "a vantage that cannot fail" wearing
        // placement's clothes — the criterion was satisfiable by putting the
        // subject where the question is meaningless. The clearing is only worth
        // anything where the neighbours flora measured actually stand.
        if (!in_forest_mass(layout, p)) return false;
        // Trunk gates: the ordinary tree rules, unchanged.
        if (corridor_distance(layout, p)
            < static_cast<float>(config::CORRIDOR_WIDTH) * 0.5f + 2.0f) {
            return false;
        }
        if (!dry_enough(p, 3.0f)) return false;
        const float h = ground(p);
        if (glm::length(p - layout.crag.center) < layout.crag.radius && h >= layout.crag.treeline) {
            return false; // the crag's treeless band
        }
        // Crown-scale gates. A 96 m canopy is not a column and must not be
        // tested as one: it stands off the pad ring and out of the entrance
        // ring by its RIM, and it enters a sight wedge by its rim too.
        if (on_building_pad(ctx.sites, p, crown_r)) return false;
        if (near_entrance_works(ctx.sites, p, crown_r)) return false;
        if (wedges.rejects_disc(p, crown_r, h + height)) return false;
        for (int i = 0; i < 12; ++i) {
            const float a = TAU * static_cast<float>(i) / 12.0f;
            const glm::vec2 q = p + glm::vec2{std::cos(a), std::sin(a)} * crown_r;
            if (q.x < domain_min.x || q.x >= domain_max.x || q.y < domain_min.y
                || q.y >= domain_max.y) {
                return false; // half a landmark hanging over the world edge
            }
            if (breaks_massif_apron(seed, layout.crag, q, ground(q) + height)) return false;
        }
        out_ground = h;
        return true;
    };

    // THE SCORE, and it is the crown's own criterion rather than a taste:
    // a horizontal disc 96 m across needs 96 m of ground that does not run
    // away from it, so the candidate is judged by the mean slope FROM THE
    // TRUNK TO THE CROWN RIM — TREE_SLOPE_MAX read at the object's own scale
    // (Rule 33's habit applied to placement instead of to detail).
    const auto crown_slope = [&](glm::vec2 p, float h0) {
        float sum = 0.0f;
        constexpr int N = 12;
        for (int i = 0; i < N; ++i) {
            const float a = TAU * static_cast<float>(i) / static_cast<float>(N);
            const glm::vec2 q = p + glm::vec2{std::cos(a), std::sin(a)} * crown_r;
            sum += std::fabs(ground(q) - h0);
        }
        return std::atan((sum / static_cast<float>(N)) / crown_r);
    };

    const int64_t gx0 = static_cast<int64_t>(std::floor(domain_min.x / separation));
    const int64_t gx1 = static_cast<int64_t>(std::floor((domain_max.x - 0.001f) / separation));
    const int64_t gz0 = static_cast<int64_t>(std::floor(domain_min.y / separation));
    const int64_t gz1 = static_cast<int64_t>(std::floor((domain_max.y - 0.001f) / separation));
    for (int64_t gz = gz0; gz <= gz1; ++gz) {
        for (int64_t gx = gx0; gx <= gx1; ++gx) {
            WorldGenRng rng = oak_cell_rng(seed, gx, gz);
            bool found = false;
            glm::vec2 best{0.0f};
            float best_ground = 0.0f;
            float best_score = 0.0f;
            for (int i = 0; i < CANDIDATES_PER_CELL; ++i) {
                const glm::vec2 p{
                    (static_cast<float>(gx) + rng.next_float01()) * separation,
                    (static_cast<float>(gz) + rng.next_float01()) * separation};
                if (p.x < domain_min.x || p.x >= domain_max.x || p.y < domain_min.y
                    || p.y >= domain_max.y) {
                    continue;
                }
                float h = 0.0f;
                if (!legal(p, h)) continue;
                const float score = crown_slope(p, h);
                if (score > static_cast<float>(config::TREE_SLOPE_MAX)) continue;
                if (!found || score < best_score) {
                    found = true;
                    best = p;
                    best_ground = h;
                    best_score = score;
                }
            }
            if (!found) continue;
            bool too_close = false;
            for (const GreatOakSite& s : out) {
                if (glm::length(best - s.pos) < separation) {
                    too_close = true;
                    break;
                }
            }
            if (too_close) continue;
            GreatOakSite site;
            site.pos = best;
            site.ground_y = best_ground;
            site.height = height;
            site.crown_radius = crown_r;
            site.clearing_radius = clearing_r;
            site.chained = false; // GIANT_OAKS §6: no sea in this world, no named oak
            out.push_back(site);
        }
    }
    return out;
}

} // namespace dfn::world
