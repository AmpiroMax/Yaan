/*
Created: 09:08:2026 - 11:05:22
Last updated: 10:08:2026 - 12:11:07
Module: engine/world
File: engine/world/sources/WorldgenScatter.cpp

Responsibility:
- P5 implementation: species lattices (oak/pine/birch), clearing lattice with
  the forced forest-ruin clearing, forest-edge bushes, loose stones, outcrop
  clusters, the watchpoint cluster. All exclusion rules of §2.2/§2.4/§5.

Key items:
- build_scatter, in_forest_mass.

Dependencies:
- Uses: WorldgenScatter.h, WorldgenMacro.h, WorldgenNoise.h, config.
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): per-cell rng seeded by mix64(seed, stream, cell) —
  never by chunk. Fixed pass order = fixed instance order inside a chunk.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — P5 implementation.
- 09:08:2026 - 13:12:19: Stage 3b amendments: pine ring -> radial ridge strips (§5.2/§1.3); L0 sight wedges reject over-angling trees near POI sightlines (LANDMARK_CLEARANCE_FACTOR); crag treeless band via treeline; canopy_height_at.
- 09:08:2026 - 14:03:23: Micro-relief batch: curb stones along corridor margins (PATH_CURB_SPACING/DENSITY, margin band between groove edge and corridor edge, 0.25-0.55 m Stones, deterministic per corridor step).
- 09:08:2026 - 14:49:01: Scatter-in-water fix (part 2): ScatterCtx::dry_enough(p, margin) is now THE water gate for every pass — trees/bushes/stones/curbs and the forced watchpoint cluster (which sits on a ford by design and previously bypassed all gates: a pine and boulders stood in the channel). Margins TREE/BUSH/STONE_WATER_MARGIN keep trunks clear of the drawn plane edge.
- 09:08:2026 - 17:45:08: §6.2: standing stones flanking each entrance approach (paired avenue, placed by rule — they must read as INTENTIONAL, which scatter cannot do) + the exclusion ring keeping trees, bushes and loose stones off the mound and forecourt so the silhouette survives.
- 09:08:2026 - 18:58:01: Live-play fix: scatter resolves against the FINAL ground (macro + carve + entrance works + pads). Sampling the pre-stamp field buried props by exactly the mound's local rise — measured up to 2.4 m at the river barrow.
- 09:08:2026 - 22:54:32: Tree occlusion heights sourced from OAK/PINE/BIRCH_HEIGHT_MAX instead of a local table that read 12/18/10 against a world built at 32/38/22 (pine 2.1x under). The sight-wedge filter and the C1 canopy field were both lied to; the wedges never failed.
- 10:08:2026 - 01:58:00: Birch lattice derives from BIRCH_BANKLINE_SPACING_MIN/MAX (design ruling on core's Rule 32 question: bank-line accent spacing = crown width x 1.1-1.5, NOT TREE_SPACING_FOREST; the hard-coded 8.0 sat inside the band by luck). Mid-of-band derivation, same shape as oak/pine.
- 10:08:2026 - 11:51:23: scatter_forest_floor(): snags (grey in the wood, pale
  in the open — two materials on one asset, and a 6x density split is why they
  are two lattices), big bushes, fallen logs laid ACROSS the slope by derived
  yaw, deadfall. Lattice cell derived from the declared per-hectare band rather
  than tabled, so the NUMBERS row stays the single source. THE CANOPY OCCLUSION
  ENVELOPE is now SPECIES_HEIGHT_MAX x TREE_MATURITY_GIANT_MULT_MAX: a 1.5x
  giant oak is 48 m where the raycast modelled 32 — the same "half the world's
  height" defect the species-height constants were introduced to stop, one
  factor further out. The corridor clause on floor_ok was missing in the first
  cut and WorldgenV2Tests caught it: §2.4 is asserted over every non-Stone
  instance, not over trees, and a blanket rule a new species may opt out of is
  not a rule.
- 10:08:2026 - 11:59:55: §5.11 scatter_path_edges(): flora's seven edge species
  on the margins, on THEIR datum (the worn edge, outward — never the
  centreline), with the edge ramp applied EXACTLY ONCE from PathSample::edge
  and per_100m normalised by the ramp's own integral. The field term is
  max(clump, edge x richness), a FLOOR and not a product: written as a product
  the swept classes' margins go to exactly zero, which the suite now fails on.
- 10:08:2026 - 12:11:07: scatter_forest_ground() — BR-3's DENOMINATOR. The first
  cut sampled positions and asked "is there a trunk within reach", which is the
  anchor question read backwards and is off by the odds of asking it: at 44
  stems/ha a random point has a trunk within 3 m ~13% of the time, so an
  authored 40/ha placed 0.82/ha. No reach tuning fixes that — the STRUCTURE was
  wrong. Flora derived the density FROM the stem count, so the loop is over
  stems, with for_cells_margin so a trunk over the border still dresses into
  this chunk.
*/

#include "engine/world/sources/WorldgenScatter.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/core/math/sources/FloraEdgeRules.h"
#include "engine/world/sources/WorldgenNoise.h"
#include "engine/world/sources/Worldgen.h"

#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

constexpr float TAU = 6.28318530717958647692f;
constexpr float TREE_SLOPE = static_cast<float>(config::TREE_SLOPE_MAX);
constexpr float CORRIDOR_HALF = static_cast<float>(config::CORRIDOR_WIDTH) * 0.5f;
constexpr float EYE_M = static_cast<float>(config::PLAYER_EYE_HEIGHT);
constexpr float CLEARANCE = static_cast<float>(config::LANDMARK_CLEARANCE_FACTOR);

// Water clearance margins (scatter-internal placement tuning, meters): keep
// trunks and boulders clear of the drawn water plane's edge so nothing reads
// as growing out of the water. Trees need more than stones — a trunk base
// clipping the plane is far more obvious than a pebble at the waterline.
constexpr float TREE_WATER_MARGIN = 3.0f;
constexpr float BUSH_WATER_MARGIN = 2.0f;
constexpr float STONE_WATER_MARGIN = 1.5f;

// Species max heights (LANDSCAPE §5 size rows — design data, used for the
// occlusion canopy and the sight-wedge angle tests).
// SOURCED FROM THE SAME CONSTANTS RENDER DRAWS FROM, never re-tabled here.
// These were hard-coded at 12 / 18 / 10 while the world was BUILT at 32 / 38 /
// 22 -- pine modelled at 2.1x under its drawn height. Every tree standing
// inside a landmark sight wedge was admitted because this filter believed it
// was 18 m tall, and the C1 occlusion field inherited the same lie. Design's
// tall-tree ruling reached render and never reached the world's occlusion
// model: a correct fix in one consumer with the mechanism left broken in
// another.
//
// A tabled height that must agree with a drawn height WILL disagree again; the
// only question is when. So there is no table here.
constexpr float OAK_MAX_H = static_cast<float>(config::OAK_HEIGHT_MAX);
constexpr float PINE_MAX_H = static_cast<float>(config::PINE_HEIGHT_MAX);
constexpr float BIRCH_MAX_H = static_cast<float>(config::BIRCH_HEIGHT_MAX);

/// THE CANOPY OCCLUSION ENVELOPE IS NOT THE SPECIES MAX HEIGHT.
///
/// A tree's drawn height is its species max TIMES its maturity multiplier, and
/// the top tier goes to TREE_MATURITY_GIANT_MULT_MAX (design §5.10: a giant is
/// a DaleOak with maturity > 1, not a second species). So the ceiling a
/// sightline has to clear is SPECIES_HEIGHT_MAX x GIANT_MULT_MAX — a 1.5x oak
/// is 48 m where the raycast used to model 32.
///
/// This is the SAME DEFECT the OAK/PINE/BIRCH constants above were written to
/// stop, one factor further out: a world modelled at a fraction of its drawn
/// height. It was caught before it shipped because the maturity bands became
/// registry rows with two zones reading them (Rule 35), and the hard-coded 1.2
/// "max scale margin" below was a third of the way to the right answer by
/// accident. Both consumers now read the row.
constexpr float GIANT_MULT = static_cast<float>(config::TREE_MATURITY_GIANT_MULT_MAX);

/// L0 sight wedges (§1.3 C4 enforcement): 2D wedges from each POI standpoint
/// to the L0 footprint; trees inside a wedge whose canopy top would subtend
/// >= L0_angle / LANDMARK_CLEARANCE_FACTOR from the standpoint are rejected.
/// Angle comparisons use tangents (angles here are < 0.2 rad; documented
/// small-angle equivalence).
struct SightWedges {
    struct Standpoint {
        glm::vec2 pos;
        float eye_y;
        glm::vec2 dir; ///< toward the crag center, normalized
        float dist;    ///< to the crag center
        float t_l0;    ///< tangent of the L0's elevation angle
    };
    std::vector<Standpoint> points;
    float crag_radius = 0.0f;

    /// True if a tree of top height `top_y` at `p` would violate the
    /// clearance factor inside any wedge.
    [[nodiscard]] bool rejects(glm::vec2 p, float top_y) const {
        for (const Standpoint& sp : points) {
            const glm::vec2 rel = p - sp.pos;
            const float proj = glm::dot(rel, sp.dir);
            if (proj < 10.0f || proj > sp.dist) continue;
            const float perp = std::fabs(rel.x * sp.dir.y - rel.y * sp.dir.x);
            if (perp > crag_radius * proj / sp.dist) continue;
            const float t_tree = (top_y - sp.eye_y) / proj;
            if (t_tree * CLEARANCE >= sp.t_l0) {
                return true;
            }
        }
        return false;
    }
};

/// Deterministic rng for one lattice cell of one scatter stream.
WorldGenRng cell_rng(uint64_t seed, uint32_t stream, int64_t gx, int64_t gz) {
    uint64_t s = noise::mix64(seed ^ (0xC0FFEE5CA77E12ull + stream));
    s = noise::mix64(s ^ static_cast<uint64_t>(gx));
    s = noise::mix64(s ^ static_cast<uint64_t>(gz));
    return WorldGenRng{s};
}

bool in_rect(glm::vec4 rect, glm::vec2 p) {
    return p.x >= rect.x && p.y >= rect.y && p.x < rect.z && p.y < rect.w;
}

bool in_oak(const TestbedLayout& layout, glm::vec2 p) {
    for (const glm::vec4& r : layout.forests.oak_rects) {
        if (in_rect(r, p)) return true;
    }
    return false;
}

bool in_pine(const TestbedLayout& layout, glm::vec2 p) {
    const glm::vec2 rel = p - layout.crag.center;
    const float d = glm::length(rel);
    if (d >= layout.forests.pine_annulus_r0 && d < layout.forests.pine_annulus_r1) {
        // Radial ridge strips, never a solid ring (§5.2 / §1.3 C1 knob):
        // sectors of the foothill annulus, pine on strip_duty of each.
        const float bearing = std::atan2(rel.y, rel.x); // [-pi, pi]
        const float sector =
            (bearing + 3.14159265358979f) / TAU * layout.forests.pine_strip_count;
        if (sector - std::floor(sector) < layout.forests.pine_strip_duty) {
            return true;
        }
    }
    return in_rect(layout.forests.pine_strip, p);
}

/// Clearing test (§2.2): jittered lattice of CLEARING_INTERVAL cells active
/// inside forest masses, plus the forced forest-ruin clearing (§7.1).
bool in_clearing(uint64_t seed, const TestbedLayout& layout, glm::vec2 p) {
    if (glm::length(p - layout.forests.forced_clearing_center)
        < layout.forests.forced_clearing_radius) {
        return true;
    }
    const float cell = static_cast<float>(config::CLEARING_INTERVAL_MIN
                                          + config::CLEARING_INTERVAL_MAX) * 0.5f;
    const int64_t gx0 = static_cast<int64_t>(std::floor(p.x / cell));
    const int64_t gz0 = static_cast<int64_t>(std::floor(p.y / cell));
    for (int64_t gz = gz0 - 1; gz <= gz0 + 1; ++gz) {
        for (int64_t gx = gx0 - 1; gx <= gx0 + 1; ++gx) {
            WorldGenRng rng = cell_rng(seed, STREAM_SCATTER_CLEARING, gx, gz);
            const glm::vec2 center{
                (static_cast<float>(gx) + 0.2f + rng.next_float01() * 0.6f) * cell,
                (static_cast<float>(gz) + 0.2f + rng.next_float01() * 0.6f) * cell};
            if (!in_oak(layout, center) && !in_pine(layout, center)) {
                continue; // clearings live inside forest masses only
            }
            const float radius =
                static_cast<float>(config::CLEARING_RADIUS_MIN)
                + rng.next_float01()
                      * static_cast<float>(config::CLEARING_RADIUS_MAX
                                           - config::CLEARING_RADIUS_MIN);
            if (glm::length(p - center) < radius) {
                return true;
            }
        }
    }
    return false;
}

struct ScatterCtx {
    uint64_t seed;
    const TestbedLayout& layout;
    const HydrologyData& hydro;
    const SitesData& sites;
    const SightWedges& wedges;
    /// The §8.1 stand's own passes. EMPTY ON THE TESTBED, and an empty grid
    /// samples 0 while an empty network flattens by 0 — so this is one code
    /// path, not a branch (Rule 32), and the testbed's scatter is byte-identical.
    const ErosionGrid& erosion;
    const PathNetwork& paths;
    glm::vec2 chunk_min, chunk_max;
    std::vector<math::ScatterInstance>& out;

    [[nodiscard]] bool inside_chunk(glm::vec2 p) const {
        return p.x >= chunk_min.x && p.x < chunk_max.x && p.y >= chunk_min.y
            && p.y < chunk_max.y;
    }
    /// FINAL terrain height — macro, hydrology carve, entrance works AND pads.
    /// Sampling anything earlier is what buried props up to 2.4 m: the barrow
    /// mound is stamped after the base field, so a prop placed against the
    /// pre-stamp height sinks by exactly the mound's local rise (and floats by
    /// the forecourt's depth on the cut side). Anything standing ON the ground
    /// must be resolved against the ground that actually ships.
    /// ... and, on the forest stand, LF-8's erosion delta and the path tread.
    /// Those were missing when the stand was first wired, which put every
    /// instance at its PRE-EROSION height — the same class of bug as the
    /// pre-stamp props above, and invisible until something stands in a gully.
    [[nodiscard]] float ground(glm::vec2 p) const {
        const float base = water_at(hydro, layout, p, macro_height(seed, layout, p)).height
                         + erosion.sample(p);
        const float worked = pads_height(sites, p, entrance_works_height(sites, p, base));
        return worked + paths.flatten_at(p, worked);
    }
    [[nodiscard]] float dist_to_water(glm::vec2 p) const {
        return water_at(hydro, layout, p, macro_height(seed, layout, p)).dist_to_water;
    }
    /// THE water gate for every scatter pass (forced clusters included):
    /// the sample must be dry per the same water_at truth the classifier and
    /// the drawn water primitives use, and `margin` meters clear of the water
    /// edge so nothing reads as standing in the water.
    [[nodiscard]] bool dry_enough(glm::vec2 p, float margin) const {
        const WaterSample w = water_at(hydro, layout, p, macro_height(seed, layout, p));
        return w.water_surface == math::NO_WATER && w.dist_to_water >= margin;
    }
    [[nodiscard]] float slope(glm::vec2 p) const {
        const float d = 2.0f;
        const float hx = ground({p.x + d, p.y}) - ground({p.x - d, p.y});
        const float hz = ground({p.x, p.y + d}) - ground({p.x, p.y - d});
        return std::atan(std::sqrt(hx * hx + hz * hz) / (2.0f * d));
    }
    /// §6.2 exclusion ring: nothing natural grows over an entrance. The mound
    /// exists to make a silhouette a hole in flat ground cannot have, and a
    /// stand of oaks on top of it destroys exactly that.
    [[nodiscard]] bool near_entrance(glm::vec2 p) const {
        const float margin = static_cast<float>(config::ENTRANCE_SCATTER_EXCLUSION_MARGIN);
        for (const EntranceWorks& w : sites.entrances) {
            if (!w.valid) continue;
            if (glm::length(p - w.center) < w.mound_radius + margin) return true;
            if (glm::length(p - w.portal) < w.forecourt_length + margin) return true;
        }
        // Hand-authored entrances have no works; keep their approach clear too.
        for (std::size_t i = 0; i < sites.entities.size(); ++i) {
            if (sites.types[i] != SiteType::DungeonEntrance) continue;
            if (glm::length(p - sites.entities[i].position_xz) < margin + 4.0f) return true;
        }
        return false;
    }

    [[nodiscard]] bool on_pad(glm::vec2 p) const {
        for (const BuildingPad& pad : sites.pads) {
            if (glm::length(p - pad.center) < pad.radius + pad.blend + 2.0f) return true;
        }
        return false;
    }
    /// The crag's treeless band (§1.3 C4 knob): no trees above the stamp's
    /// treeline, which sits below the rock splat line.
    [[nodiscard]] bool on_crag_treeless(glm::vec2 p, float h) const {
        return glm::length(p - layout.crag.center) < layout.crag.radius
            && h >= layout.crag.treeline;
    }

    void add(glm::vec2 p, math::ScatterSpecies species, float yaw, float scale) {
        out.push_back(math::ScatterInstance{{p.x, ground(p), p.y}, yaw, scale, species});
    }

    /// Common tree suitability (§5 global rules + §2.4 corridor protection +
    /// §1.3 sight wedges — `species_max_h` is the §5 species max height).
    [[nodiscard]] bool tree_ok(glm::vec2 p, float min_water_dist, float species_max_h) const {
        if (corridor_distance(layout, p) < CORRIDOR_HALF + 2.0f) return false;
        if (on_pad(p) || near_entrance(p)) return false;
        if (!dry_enough(p, min_water_dist)) return false;
        const float h = ground(p);
        if (on_crag_treeless(p, h)) return false;
        if (wedges.rejects(p, h + species_max_h * GIANT_MULT)) return false; // the giant tier
        return slope(p) <= TREE_SLOPE;
    }

    /// Iterates lattice cells of size `cell` overlapping the chunk.
    /// for_cells with `rings` extra cells of margin: for ANCHORED classes,
    /// whose instance is offset from a parent that may sit in a neighbouring
    /// cell. The instance's RESOLVED position still decides which chunk owns
    /// it, so borders stay half-open and nothing is placed twice.
    template <typename Fn> void for_cells_margin(float cell, int rings, Fn&& fn) const {
        const int64_t gx0 = static_cast<int64_t>(std::floor(chunk_min.x / cell)) - rings;
        const int64_t gx1 = static_cast<int64_t>(std::floor((chunk_max.x - 0.001f) / cell)) + rings;
        const int64_t gz0 = static_cast<int64_t>(std::floor(chunk_min.y / cell)) - rings;
        const int64_t gz1 = static_cast<int64_t>(std::floor((chunk_max.y - 0.001f) / cell)) + rings;
        for (int64_t gz = gz0; gz <= gz1; ++gz) {
            for (int64_t gx = gx0; gx <= gx1; ++gx) {
                fn(gx, gz, glm::vec2{static_cast<float>(gx) * cell,
                                     static_cast<float>(gz) * cell});
            }
        }
    }

    template <typename Fn> void for_cells(float cell, Fn&& fn) const {
        const int64_t gx0 = static_cast<int64_t>(std::floor(chunk_min.x / cell));
        const int64_t gx1 = static_cast<int64_t>(std::floor((chunk_max.x - 0.001f) / cell));
        const int64_t gz0 = static_cast<int64_t>(std::floor(chunk_min.y / cell));
        const int64_t gz1 = static_cast<int64_t>(std::floor((chunk_max.y - 0.001f) / cell));
        for (int64_t gz = gz0; gz <= gz1; ++gz) {
            for (int64_t gx = gx0; gx <= gx1; ++gx) {
                fn(gx, gz, glm::vec2{static_cast<float>(gx) * cell,
                                     static_cast<float>(gz) * cell});
            }
        }
    }
};


// ---------------------------------------------------------------------------
// §5.10 THE FOREST FLOOR
//
// Until this pass existed every constant below was marked НЕ ПОСТРОЕНО in
// NUMBERS.md with the same sentence repeated eleven times: «меши есть в render,
// правило есть в документе дизайна, в мире нет ничего — пол леса сегодня это
// голая земля». The meshes were built, the rule was written, and nothing stood
// on the ground.
//
// SPACING FROM DENSITY, and it is the mid of each declared band, matching the
// derivation the tree lattices already use: a density of D per hectare is one
// instance per 10000/D square metres, so the lattice cell is sqrt(10000/D).
// Stating it as a conversion rather than as a spacing constant is what keeps
// the NUMBERS row the single source — a tabled spacing would be a second place
// for the density to live.
// ---------------------------------------------------------------------------

/// Lattice cell (m) for a per-hectare density band, taken at the band's mid.
[[nodiscard]] float cell_for_density_ha(double min_per_ha, double max_per_ha) {
    const auto mid = static_cast<float>(min_per_ha + max_per_ha) * 0.5f;
    return std::sqrt(10000.0f / std::max(mid, 0.01f));
}

/// Downslope azimuth at `p` (the direction water runs), or 0 on flat ground.
[[nodiscard]] float downslope_yaw(const ScatterCtx& ctx, glm::vec2 p) {
    constexpr float D = 3.0f;
    const float gx = ctx.ground({p.x + D, p.y}) - ctx.ground({p.x - D, p.y});
    const float gz = ctx.ground({p.x, p.y + D}) - ctx.ground({p.x, p.y - D});
    if (std::fabs(gx) < 1e-4f && std::fabs(gz) < 1e-4f) {
        return 0.0f;
    }
    return std::atan2(-gx, -gz);
}

/// Ground a piece of dead wood may occupy: dry, off the pads and entrances,
/// out of the §2.4 corridors, and — the clause the tree passes do not have —
/// OFF THE TREAD.
///
/// The corridor clause is here because the first cut left it out and
/// WorldgenV2Tests caught it immediately: §2.4 corridors are protected ground,
/// and the suite asserts it over EVERY non-Stone instance rather than over
/// trees. I would have argued that a 0.25 m deadfall blocks no sightline —
/// and the argument is beside the point twice over. A corridor is kept clear
/// because it is walked and looked along, and a trunk lying across it is a
/// roadblock for the same reason a trunk across a tread is; and a blanket rule
/// that a new species may opt out of is not a rule.
[[nodiscard]] bool floor_ok(const ScatterCtx& ctx, glm::vec2 p, float clear_of_tread_m) {
    if (corridor_distance(ctx.layout, p) < CORRIDOR_HALF + 2.0f) {
        return false;
    }
    if (!ctx.dry_enough(p, BUSH_WATER_MARGIN) || ctx.on_pad(p) || ctx.near_entrance(p)) {
        return false;
    }
    const PathSample ps = ctx.paths.sample(p);
    return ps.dist_from_worn_edge > clear_of_tread_m;
}

/// §5.10: standing dead wood, downed wood, and the shrub mass.
void scatter_forest_floor(ScatterCtx& ctx) {
    const bool mass_anywhere =
        ctx.layout.forests.oak_rects[0].z > ctx.layout.forests.oak_rects[0].x
        || ctx.layout.forests.pine_annulus_r1 > 0.0f;
    if (!mass_anywhere) {
        return;
    }

    // (a) SNAGS, and the split is TWO MATERIALS ON ONE ASSET (design §5.10):
    // grey-brown and dense inside the wood, where it is weather; bone-pale and
    // rare in the open, where a single standing dead trunk is a landmark
    // somebody navigates by. The densities differ by 6x for that reason, so
    // they are two lattices and not one lattice with a material flag.
    {
        const float cell = cell_for_density_ha(config::SNAG_DENSITY_FOREST_MIN,
                                               static_cast<double>(config::SNAG_DENSITY_FOREST_MAX));
        ctx.for_cells(cell, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
            WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_FLOOR + 0, gx, gz);
            const glm::vec2 p = corner + glm::vec2{rng.next_float01(), rng.next_float01()} * cell;
            if (!ctx.inside_chunk(p) || !in_forest_interior(ctx.seed, ctx.layout, p)) return;
            if (!floor_ok(ctx, p, 1.5f)) return;
            ctx.add(p, math::ScatterSpecies::Snag, rng.next_float01() * TAU,
                    0.75f + rng.next_float01() * 0.5f);
        });
    }
    {
        const float cell = cell_for_density_ha(config::SNAG_DENSITY_OPEN_MIN,
                                               config::SNAG_DENSITY_OPEN_MAX);
        ctx.for_cells(cell, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
            WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_FLOOR + 1, gx, gz);
            const glm::vec2 p = corner + glm::vec2{rng.next_float01(), rng.next_float01()} * cell;
            if (!ctx.inside_chunk(p)) return;
            if (!in_open_ground(ctx.seed, ctx.layout, p) || !floor_ok(ctx, p, 2.0f)) return;
            ctx.add(p, math::ScatterSpecies::SnagPale, rng.next_float01() * TAU,
                    0.8f + rng.next_float01() * 0.5f);
        });
    }

    // (b) BIG BUSHES. Flora measured this to be BR-5's load-bearing occluder —
    // not the dead wood, which a ray still 0.5 m up at the find sails over
    // (bush 0.725 of bearings at 60 m, big bush 0.239, fallen log 0.050, snag
    // 0.008, deadfall 0.000). Sized for §5.10 all the same, not to feed a
    // validator: it is the class the user asked for by name.
    {
        const float cell = cell_for_density_ha(static_cast<double>(config::BIGBUSH_DENSITY_MIN),
                                               static_cast<double>(config::BIGBUSH_DENSITY_MAX));
        ctx.for_cells(cell, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
            WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_FLOOR + 2, gx, gz);
            const glm::vec2 p = corner + glm::vec2{rng.next_float01(), rng.next_float01()} * cell;
            if (!ctx.inside_chunk(p) || !in_forest_mass(ctx.layout, p)) return;
            if (!floor_ok(ctx, p, 1.0f)) return;
            ctx.add(p, math::ScatterSpecies::BigBush, rng.next_float01() * TAU,
                    0.8f + rng.next_float01() * 0.4f);
        });
    }

    // (c) FALLEN LOGS, ACROSS THE SLOPE — в17's phrasing and a real fact: a
    // trunk that comes down on a slope rolls until it lies along the contour,
    // and one lying down the fall line is what a reader notices as wrong even
    // if they cannot say why. The yaw is therefore DERIVED from the gradient
    // (downslope + 90 deg) with only a few degrees of jitter, not drawn.
    const auto scatter_logs = [&](uint32_t stream, math::ScatterSpecies sp, float cell,
                                  float scale_lo, float scale_hi) {
        ctx.for_cells(cell, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
            WorldGenRng rng = cell_rng(ctx.seed, stream, gx, gz);
            const glm::vec2 p = corner + glm::vec2{rng.next_float01(), rng.next_float01()} * cell;
            if (!ctx.inside_chunk(p) || !in_forest_interior(ctx.seed, ctx.layout, p)) return;
            if (!floor_ok(ctx, p, 1.5f)) return;
            constexpr float QUARTER = TAU * 0.25f;
            constexpr float JITTER = 0.22f; // ~12 deg either way: settled, not surveyed
            const float yaw = downslope_yaw(ctx, p) + QUARTER
                            + (rng.next_float01() - 0.5f) * 2.0f * JITTER;
            ctx.add(p, sp, yaw, scale_lo + rng.next_float01() * (scale_hi - scale_lo));
        });
    };
    scatter_logs(STREAM_SCATTER_FLOOR + 3, math::ScatterSpecies::FallenLog,
                 cell_for_density_ha(static_cast<double>(config::LOG_DENSITY_BIG_MIN),
                                     static_cast<double>(config::LOG_DENSITY_BIG_MAX)),
                 0.85f, 1.3f);
    // The SMALL row is the deadfall tier: LOG_DENSITY_SMALL is 15-30/ha against
    // the big row's 3-8, which is litter rather than landmark. Mapping it to
    // FallenLog as well would have put 25 full trunks on every hectare.
    scatter_logs(STREAM_SCATTER_FLOOR + 4, math::ScatterSpecies::Deadfall,
                 cell_for_density_ha(static_cast<double>(config::LOG_DENSITY_SMALL_MIN),
                                     static_cast<double>(config::LOG_DENSITY_SMALL_MAX)),
                 0.7f, 1.15f);
}


// ---------------------------------------------------------------------------
// §5.11 THE RICH EDGE (в8/в19в, BR-3)
//
// «Обочина — самая богатая полоса» (research §A6.3). The composition order is
// design's amendment and it is binding:
//
//     density = base x clump(class, xz) x edge_gradient x richness x exclusions
//
// with THE EDGE GRADIENT AS A FLOOR ON THE FIELD rather than a factor of it,
// so a coverage gap can never bare a path margin. Written out:
//
//     field(p) = max(clump(p), edge(p) * richness(path_class))
//     rho(p)   = (per_100m / 100 m) * field(p) / INTEGRAL(edge over the band)
//
// FOUR THINGS IN THAT ARE EASY TO GET WRONG AND ALL FOUR WERE FLAGGED BEFORE
// THE CODE EXISTED, which is why they are spelled out here:
//
// 1. THE EDGE RAMP IS APPLIED ONCE. flora's clump_field_edged() computed its
//    own; called with PathSample::edge as well it would have SQUARED the band
//    and moved its peak inward. It is deleted and this calls plain
//    clump_field(). The symptom would have been «обочина жидковата», which
//    nobody diagnoses as a units bug.
// 2. per_100m IS A TOTAL COUNT ACROSS THE BAND — neither a peak nor a mean
//    density. So the ramp shapes the DISTRIBUTION and not the amount, and the
//    magnitude is normalised by the ramp's own integral, READ FROM THE RAMP
//    (edge_band_integral) rather than pasted, so retuning the ramp does not
//    silently move every count in flora's table.
// 3. THE DATUM IS THE WORN EDGE, OUTWARD — never the centreline. PathSample
//    reports dist_from_worn_edge directly so nothing here reconstructs it from
//    a distance and a guessed width.
// 4. THE FLOOR NEVER SUBTRACTS AND A KEPT VERGE IS NOT BARE GROUND. max(), not
//    a product: at richness 0 the margin falls back to the FIELD value, never
//    to zero, because §1.1 does not stop at the town gate and a margin
//    suppressed to nothing re-makes «земля плоская и мёртвая» inside the
//    settlement. BR-3's ratio is therefore scoped to the unmaintained classes
//    and A COBBLED STREET FAILING IT IS A PASS.
// ---------------------------------------------------------------------------

void scatter_path_edges(ScatterCtx& ctx) {
    const PathNetwork& net = ctx.paths;
    if (net.routes.empty()) {
        return;
    }
    // Lateral resolution of the placement integral. 0.25 m is a quarter of the
    // 1.0 m narrowest band and well inside the 0.35 m peak — the ramp has one
    // corner and integrating a corner is not where the error lives.
    constexpr float DX = 0.25f;
    // A station cannot seed anything further than its own reach, so a station
    // whose reach misses the chunk is skipped before any draw. Without this the
    // pass walks all six routes for all sixteen chunks.
    const float max_reach = net.rich_edge_band_m + 6.0f;

    for (std::size_t ri = 0; ri < net.routes.size(); ++ri) {
        const PathRoute& r = net.routes[ri];
        for (std::size_t si = 0; si + 1 < r.points.size(); ++si) {
            const glm::vec2 a = r.points[si];
            const glm::vec2 b = r.points[si + 1];
            if (a.x < ctx.chunk_min.x - max_reach || a.x > ctx.chunk_max.x + max_reach
                || a.y < ctx.chunk_min.y - max_reach || a.y > ctx.chunk_max.y + max_reach) {
                continue;
            }
            const glm::vec2 d = b - a;
            const float seg_len = glm::length(d);
            if (seg_len < 1e-3f) {
                continue;
            }
            const glm::vec2 t = d / seg_len;
            const glm::vec2 n{-t.y, t.x};
            const float half = path_half_width(r.classes[si]);
            const auto cls_ordinal = static_cast<uint8_t>(r.classes[si]);

            for (std::size_t k = 0; k < math::FLORA_EDGE_RULE_COUNT; ++k) {
                const math::FloraEdgeRule& rule = math::FLORA_EDGE_RULES[k];
                if (rule.habitat != math::EdgeHabitat::PathMargin || !rule.common_scatter
                    || rule.per_100m <= 0.0f) {
                    continue; // THE JEWEL IS A BUDGET, NOT A PROBABILITY: it is
                              // placed at finds and pearls, never here.
                }
                const float integral = math::edge_band_integral(rule, net.rich_edge_band_m);
                if (integral <= 1e-4f) {
                    continue;
                }
                const float rich = rule.richness.by_ordinal(cls_ordinal);
                // instances per metre of route per metre of lateral offset,
                // before the field: per_100m over 100 m of route, spread by the
                // ramp and normalised so the total is per_100m whatever the
                // ramp's shape.
                const float base = rule.per_100m / 100.0f / integral;

                for (int side = -1; side <= 1; side += 2) {
                    for (float x = rule.band_min_m + DX * 0.5f; x < rule.band_max_m; x += DX) {
                        const glm::vec2 p =
                            a + t * (seg_len * 0.5f)
                            + n * (static_cast<float>(side) * (half + x));
                        if (!ctx.inside_chunk(p)) {
                            continue;
                        }
                        // The datum, from the query rather than reconstructed.
                        const PathSample ps = net.sample(p);
                        const float edge = ps.edge;
                        const float clump = rule.clump_applies
                                              ? math::clump_field(rule.clump, p, static_cast<uint32_t>(ctx.seed))
                                              : 1.0f;
                        const float field = std::max(clump, edge * rich);
                        const float expected = base * field * seg_len * DX;
                        if (expected <= 0.0f) {
                            continue;
                        }
                        // One deterministic draw per (route, station, rule,
                        // side, lateral step) — keyed by the tuple, not by a
                        // running counter, so a chunk computes exactly the same
                        // instances whichever of its neighbours asked first.
                        const auto lat = static_cast<int64_t>(x / DX);
                        WorldGenRng rng =
                            cell_rng(ctx.seed, STREAM_SCATTER_EDGE + static_cast<uint32_t>(k),
                                     static_cast<int64_t>(ri * 4096 + si),
                                     lat * 4 + (side > 0 ? 1 : 0));
                        if (rng.next_float01() >= std::min(expected, 1.0f)) {
                            continue;
                        }
                        if (!ctx.dry_enough(p, 0.5f) || ctx.on_pad(p) || ctx.near_entrance(p)) {
                            continue;
                        }
                        ctx.add(p, rule.species, rng.next_float01() * TAU,
                                0.8f + rng.next_float01() * 0.4f);
                    }
                }
            }
        }
    }
}


// ---------------------------------------------------------------------------
// §5.11 THE FOREST FLOOR'S GROUND COVER — BR-3's DENOMINATOR
//
// This pass exists because a measurement had nothing to divide by. BR-3 claims
// the margin is richer than THE GROUND, and the ground carried no ground cover
// at all, so the first acceptance read a ratio of ~27000 and could not fail.
// The cause was dimensional: flora's ForestFloor rows carried per_100m = 0,
// and per_100m is a count per 100 LINEAR metres — a forest floor is not a
// linear feature, so the rows had 0 by default rather than by decision. Flora
// authored the areal figures with derivations (spec §3.13); they are placed
// here.
//
// TREATED AS BASE DENSITIES, i.e. BEFORE the clump field, which is design's
// composition order and is what flora states outright for the mushroom row
// ("mushrooms carry the tightest field in the set, so the realised world shows
// rings with most of the wood bare — which is the intent"). The moss row does
// not say so either way, and I have applied the SAME treatment rather than
// invent a per-row exception: one rule, measured, reported back.
// ---------------------------------------------------------------------------

/// Ground cover on the forest floor: the §5.11 rows whose habitat is areal.
///
/// ANCHORED CLASSES ITERATE THEIR ANCHORS. The first cut sampled positions on
/// a lattice and asked "is there a trunk within reach", which is the same
/// sentence read backwards and is off by the odds of the question: at 44
/// stems/ha a random point has a trunk within 3 m about 13% of the time, so
/// 40/ha placed 0.82/ha — a 49x shortfall that no amount of tuning the reach
/// would have fixed, because the structure was wrong rather than the number.
/// Flora derived this density FROM the stem count (44 stems/ha x ~2/3 carrying
/// a basal patch), so the loop has to be over stems. Their own conversion says
/// so out loud: 40/ha / 44 stems/ha ~= 0.9 patches per trunk.
void scatter_forest_ground(ScatterCtx& ctx) {
    // §A7's associative grammar: an anchored class sits ON the shade azimuth of
    // its neighbour, TOUCHING it. North is -Z in this world's convention, and
    // the offset is a trunk-radius-ish 0.6 m so the patch reads as growing at
    // the base rather than as a decal dropped nearby.
    constexpr float ANCHOR_TOUCH_M = 0.6f;
    const float spacing = static_cast<float>(config::TREE_SPACING_FOREST_MIN
                                             + config::TREE_SPACING_FOREST_MAX) * 0.5f;

    for (std::size_t k = 0; k < math::FLORA_EDGE_RULE_COUNT; ++k) {
        const math::FloraEdgeRule& rule = math::FLORA_EDGE_RULES[k];
        if (rule.habitat != math::EdgeHabitat::ForestFloor || !rule.common_scatter
            || rule.per_m2 <= 0.0f) {
            continue;
        }
        // ONE TRUNK PER LATTICE CELL, so the expected instances per trunk is
        // the areal density times the cell's area — derived, so a change to
        // TREE_SPACING_FOREST moves it rather than leaving a stale per-anchor
        // constant behind.
        const float per_anchor = rule.per_m2 * spacing * spacing;
        // Cells one ring out too: a trunk just over the border dresses into
        // this chunk, and reading the instance list back would drop exactly
        // those and leave a seam of bare trunks along every chunk edge.
        ctx.for_cells_margin(spacing, 1, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
            WorldGenRng trunk_rng = cell_rng(ctx.seed, STREAM_SCATTER_TREE + 0, gx, gz);
            const glm::vec2 trunk =
                corner + glm::vec2{trunk_rng.next_float01(), trunk_rng.next_float01()} * spacing;
            // THE SAME acceptance chain the trunk pass applies, CALLED and not
            // restated, so a rejected trunk cannot be dressed.
            if (!in_oak(ctx.layout, trunk) || in_clearing(ctx.seed, ctx.layout, trunk)) return;
            if (!ctx.tree_ok(trunk, TREE_WATER_MARGIN, OAK_MAX_H)) return;

            WorldGenRng rng =
                cell_rng(ctx.seed, STREAM_SCATTER_EDGE + 32u + static_cast<uint32_t>(k), gx, gz);
            const glm::vec2 p = trunk + glm::vec2{0.0f, -ANCHOR_TOUCH_M};
            // Composition order, minus the edge term this habitat has none of
            // (a forest floor has no worn edge to be a margin of):
            // base x clump x exclusions.
            const float field =
                rule.clump_applies
                  ? math::clump_field(rule.clump, p, static_cast<uint32_t>(ctx.seed))
                  : 1.0f;
            if (rng.next_float01() >= per_anchor * field) return;
            if (!ctx.inside_chunk(p)) return; // the RESOLVED position picks the owner
            if (!ctx.dry_enough(p, 0.5f) || ctx.on_pad(p) || ctx.near_entrance(p)) return;
            // Off the margin: that band is the OTHER rule's ground, and an
            // instance inside it would be counted by BR-3's numerator having
            // been placed by its denominator.
            if (ctx.paths.sample(p).dist_from_worn_edge < ctx.paths.rich_edge_band_m) return;
            ctx.add(p, rule.species, rng.next_float01() * TAU,
                    0.8f + rng.next_float01() * 0.4f);
        });
    }
}

/// Forest trees: per-species lattice; oak fills its rects, pine its annulus
/// and strip; birch lines the banks (§5.1-§5.3).
void scatter_trees(ScatterCtx& ctx) {
    const float spacing = static_cast<float>(config::TREE_SPACING_FOREST_MIN
                                             + config::TREE_SPACING_FOREST_MAX) * 0.5f;
    // Oak (stream 40).
    ctx.for_cells(spacing, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
        WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_TREE + 0, gx, gz);
        const glm::vec2 p = corner + glm::vec2{rng.next_float01(), rng.next_float01()} * spacing;
        if (!ctx.inside_chunk(p) || !in_oak(ctx.layout, p)) return;
        if (in_clearing(ctx.seed, ctx.layout, p) || !ctx.tree_ok(p, TREE_WATER_MARGIN, OAK_MAX_H)) return;
        ctx.add(p, math::ScatterSpecies::OakTree, rng.next_float01() * TAU,
                0.8f + rng.next_float01() * 0.4f);
    });
    // Pine (stream 41) — slightly tighter (§5.2 spacing 4-7).
    ctx.for_cells(spacing - 1.0f, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
        WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_TREE + 1, gx, gz);
        const glm::vec2 p =
            corner + glm::vec2{rng.next_float01(), rng.next_float01()} * (spacing - 1.0f);
        if (!ctx.inside_chunk(p) || !in_pine(ctx.layout, p) || in_oak(ctx.layout, p)) return;
        if (in_clearing(ctx.seed, ctx.layout, p) || !ctx.tree_ok(p, TREE_WATER_MARGIN, PINE_MAX_H)) return;
        ctx.add(p, math::ScatterSpecies::PineTree, rng.next_float01() * TAU,
                0.8f + rng.next_float01() * 0.4f);
    });
    // Birch (stream 42): banks only, outside the sand band (§5.3 + §5 "never
    // on sand"), loose lines — 45% keep. Spacing = mid of the
    // BIRCH_BANKLINE_SPACING band, same derivation shape as oak/pine above —
    // but from its OWN band: birch is a bank-line accent whose spacing is
    // crown width x 1.1-1.5 (a guide line reads as a line when crowns nearly
    // touch), design-ruled NOT to derive from TREE_SPACING_FOREST. The old
    // hard-coded 8.0 sat inside the band by luck, not by derivation (Rule 32:
    // one species pinned where the others derive).
    const float birch_spacing =
        static_cast<float>(config::BIRCH_BANKLINE_SPACING_MIN
                           + config::BIRCH_BANKLINE_SPACING_MAX) * 0.5f;
    ctx.for_cells(birch_spacing, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
        WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_TREE + 2, gx, gz);
        const glm::vec2 p =
            corner + glm::vec2{rng.next_float01(), rng.next_float01()} * birch_spacing;
        if (!ctx.inside_chunk(p) || rng.next_float01() > 0.45f) return;
        const float d = ctx.dist_to_water(p);
        if (d <= static_cast<float>(config::SHORE_SAND_DIST)
            || d > static_cast<float>(config::BIRCH_WATER_DIST)) {
            return;
        }
        if (in_oak(ctx.layout, p) || !ctx.tree_ok(p, TREE_WATER_MARGIN, BIRCH_MAX_H)) return;
        ctx.add(p, math::ScatterSpecies::BirchTree, rng.next_float01() * TAU,
                0.85f + rng.next_float01() * 0.3f);
    });
}

/// Bushes on forest-mass edges (<= 10 m outside a mask, §5.4).
void scatter_bushes(ScatterCtx& ctx) {
    const float cell = 6.0f;
    ctx.for_cells(cell, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
        WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_TREE + 3, gx, gz);
        const glm::vec2 p = corner + glm::vec2{rng.next_float01(), rng.next_float01()} * cell;
        if (!ctx.inside_chunk(p) || in_forest_mass(ctx.layout, p)) return;
        bool near_edge = false;
        for (const glm::vec2 off :
             {glm::vec2{10.0f, 0.0f}, {-10.0f, 0.0f}, {0.0f, 10.0f}, {0.0f, -10.0f}}) {
            if (in_forest_mass(ctx.layout, p + off)) {
                near_edge = true;
                break;
            }
        }
        if (!near_edge) return;
        const float density =
            static_cast<float>(config::BUSH_EDGE_DENSITY_MIN)
            + rng.next_float01() * static_cast<float>(config::BUSH_EDGE_DENSITY_MAX
                                                      - config::BUSH_EDGE_DENSITY_MIN);
        if (rng.next_float01() > density * cell * cell) return;
        if (ctx.on_pad(p) || ctx.near_entrance(p) || !ctx.dry_enough(p, BUSH_WATER_MARGIN)
            || corridor_distance(ctx.layout, p) < CORRIDOR_HALF) {
            return;
        }
        ctx.add(p, math::ScatterSpecies::Bush, rng.next_float01() * TAU,
                0.8f + rng.next_float01() * 0.5f);
    });
}

/// Loose stones (§2.3 density) and outcrop clusters (§2.2, OUTCROP_CELL) plus
/// the forced watchpoint cluster with its lone skyline pine (§7.1).
void scatter_stones(ScatterCtx& ctx) {
    const float cell = 10.0f;
    ctx.for_cells(cell, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
        WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_OUTCROP + 1, gx, gz);
        const glm::vec2 p = corner + glm::vec2{rng.next_float01(), rng.next_float01()} * cell;
        if (!ctx.inside_chunk(p)) return;
        const float density =
            static_cast<float>(config::STONE_DENSITY_MIN)
            + rng.next_float01() * static_cast<float>(config::STONE_DENSITY_MAX
                                                      - config::STONE_DENSITY_MIN);
        // Shore mask doubles loose-stone density (§3.3).
        const float d_water = ctx.dist_to_water(p);
        const float mult = d_water <= static_cast<float>(config::SHORE_SAND_DIST) ? 2.0f : 1.0f;
        if (rng.next_float01() > density * mult * cell * cell) return;
        if (ctx.on_pad(p) || ctx.near_entrance(p) || !ctx.dry_enough(p, STONE_WATER_MARGIN)
            || corridor_distance(ctx.layout, p) < CORRIDOR_HALF) {
            return;
        }
        ctx.add(p, math::ScatterSpecies::Stone, rng.next_float01() * TAU,
                0.2f + rng.next_float01() * 0.4f); // 0.2-0.6 m loose stones
    });
    const float ocell = static_cast<float>(config::OUTCROP_CELL);
    ctx.for_cells(ocell, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
        WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_OUTCROP, gx, gz);
        if (rng.next_float01() < 0.3f) return; // 30% skip (§2.2)
        const glm::vec2 center =
            corner + glm::vec2{0.15f + rng.next_float01() * 0.7f,
                               0.15f + rng.next_float01() * 0.7f} * ocell;
        if (in_forest_mass(ctx.layout, center)) return; // open land only
        const uint32_t count = rng.next_range(2, 6);
        for (uint32_t b = 0; b < count; ++b) {
            const float ang = rng.next_float01() * TAU;
            const float rad = rng.next_float01() * 8.0f;
            const glm::vec2 p = center + glm::vec2{std::cos(ang), std::sin(ang)} * rad;
            const float scale = 1.0f + rng.next_float01() * 2.0f; // 1-3 m boulders
            if (!ctx.inside_chunk(p) || ctx.on_pad(p) || ctx.near_entrance(p)
                || !ctx.dry_enough(p, STONE_WATER_MARGIN)) continue;
            if (corridor_distance(ctx.layout, p) < CORRIDOR_HALF + scale) continue;
            if (ctx.slope(p) > TREE_SLOPE) continue;
            ctx.add(p, math::ScatterSpecies::Stone, rng.next_float01() * TAU, scale);
        }
    });
    // Curb stones along corridor margins (micro-relief batch): sparse small
    // stones in the band between the path groove edge and the corridor edge —
    // the trail reads as tended without blocking it (corridor mask stays
    // clear of anything > 1 m by §2.4; curbs are 0.25-0.55 m). Deterministic
    // per (corridor, segment, step) — chunk-independent like all scatter.
    const float curb_spacing = static_cast<float>(config::PATH_CURB_SPACING);
    const float groove_hw = static_cast<float>(config::PATH_GROOVE_HALF_WIDTH);
    for (int c = 0; c < static_cast<int>(std::size(ctx.layout.corridors)); ++c) {
        const CorridorLayout& cor = ctx.layout.corridors[c];
        for (int s = 0; s + 1 < cor.point_count; ++s) {
            const glm::vec2 a = cor.points[s];
            const glm::vec2 b = cor.points[s + 1];
            const float len = glm::length(b - a);
            if (len < 1.0f) continue;
            const glm::vec2 dir = (b - a) / len;
            const glm::vec2 perp{-dir.y, dir.x};
            const int steps = static_cast<int>(len / curb_spacing);
            for (int k = 0; k <= steps; ++k) {
                WorldGenRng rng =
                    cell_rng(ctx.seed, STREAM_SCATTER_CURB, c * 64 + s, k);
                if (rng.next_float01() > static_cast<float>(config::PATH_CURB_DENSITY)) {
                    continue;
                }
                const float along = std::min(
                    (static_cast<float>(k) + rng.next_float01() * 0.7f) * curb_spacing,
                    len);
                const float side = rng.next_float01() < 0.5f ? -1.0f : 1.0f;
                const float lateral = groove_hw + 0.4f + rng.next_float01() * 1.4f;
                const glm::vec2 p = a + dir * along + perp * (side * lateral);
                if (!ctx.inside_chunk(p) || ctx.on_pad(p)) continue;
                if (!ctx.dry_enough(p, STONE_WATER_MARGIN)) continue;
                ctx.add(p, math::ScatterSpecies::Stone, rng.next_float01() * TAU,
                        0.25f + rng.next_float01() * 0.3f);
            }
        }
    }

    // Watchpoint (§7.1): outcrop cluster + lone skyline pine, deterministic.
    // The watchpoint sits ON a ford by design (§7.1), so these forced
    // placements must respect the water gate like every other pass — nothing
    // stands in the channel.
    const glm::vec2 wp = ctx.layout.watchpoint;
    if (ctx.inside_chunk(wp)) {
        WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_OUTCROP + 2, 0, 0);
        for (int b = 0; b < 4; ++b) {
            const float ang = rng.next_float01() * TAU;
            const glm::vec2 p = wp + glm::vec2{std::cos(ang), std::sin(ang)}
                                         * (2.0f + rng.next_float01() * 4.0f);
            const float scale = 1.2f + rng.next_float01() * 1.2f;
            if (!ctx.dry_enough(p, STONE_WATER_MARGIN)) continue;
            ctx.add(p, math::ScatterSpecies::Stone, rng.next_float01() * TAU, scale);
        }
        const glm::vec2 pine_p = wp + glm::vec2{3.0f, -2.0f};
        if (ctx.dry_enough(pine_p, TREE_WATER_MARGIN)) {
            ctx.add(pine_p, math::ScatterSpecies::PineTree, rng.next_float01() * TAU, 1.25f);
        }
    }
}

/// §6.2 findability: standing stones flanking the approach. They read as
/// INTENTIONAL at distance, which nothing natural does — that is the whole
/// job, so they are placed by rule (paired, on the approach axis), not
/// scattered. They are exempt from the exclusion ring they stand inside.
void scatter_entrance_markers(ScatterCtx& ctx) {
    for (std::size_t si = 0; si < ctx.sites.entrances.size(); ++si) {
        const EntranceWorks& w = ctx.sites.entrances[si];
        if (!w.valid) continue;
        WorldGenRng rng =
            cell_rng(ctx.seed, STREAM_SCATTER_MARKER, static_cast<int64_t>(si), 0);
        const uint32_t count = rng.next_range(
            static_cast<uint32_t>(config::STANDING_STONE_COUNT_MIN),
            static_cast<uint32_t>(config::STANDING_STONE_COUNT_MAX));
        const glm::vec2 side{-w.outward.y, w.outward.x};
        const float lateral = w.forecourt_half_width + 2.0f;
        for (uint32_t i = 0; i < count; ++i) {
            // Alternate sides, walking outward from the portal: a pair, then a
            // second pair further out — an avenue, not a ring.
            const float along = 4.0f + static_cast<float>(i / 2) * 5.0f;
            const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
            const glm::vec2 p = w.portal + w.outward * along + side * (sign * lateral);
            if (!ctx.inside_chunk(p)) continue;
            const float height =
                static_cast<float>(config::STANDING_STONE_HEIGHT_MIN)
                + rng.next_float01()
                      * static_cast<float>(config::STANDING_STONE_HEIGHT_MAX
                                           - config::STANDING_STONE_HEIGHT_MIN);
            ctx.add(p, math::ScatterSpecies::Stone, rng.next_float01() * TAU, height);
        }
    }
}

} // namespace

bool in_forest_mass(const TestbedLayout& layout, glm::vec2 world) {
    return in_oak(layout, world) || in_pine(layout, world);
}

bool in_forest_interior(uint64_t seed, const TestbedLayout& layout, glm::vec2 world) {
    return in_forest_mass(layout, world) && !in_clearing(seed, layout, world);
}

bool in_open_ground(uint64_t seed, const TestbedLayout& layout, glm::vec2 world) {
    return !in_forest_mass(layout, world) || in_clearing(seed, layout, world);
}

float canopy_height_at(uint64_t seed, const TestbedLayout& layout, glm::vec2 world,
                       float terrain_h) {
    if (glm::length(world - layout.crag.center) < layout.crag.radius
        && terrain_h >= layout.crag.treeline) {
        return 0.0f; // the crag's treeless band
    }
    const bool pine = in_pine(layout, world);
    const bool oak = in_oak(layout, world);
    if (!pine && !oak) {
        return 0.0f;
    }
    if (in_clearing(seed, layout, world)) {
        return 0.0f;
    }
    return (pine ? PINE_MAX_H : OAK_MAX_H) * GIANT_MULT;
}

std::vector<math::ScatterInstance> build_scatter(uint64_t seed, const TestbedLayout& layout,
                                                 const HydrologyData& hydro,
                                                 const SitesData& sites,
                                                 const ErosionGrid& erosion,
                                                 const PathNetwork& paths, glm::vec2 chunk_min,
                                                 glm::vec2 chunk_max) {
    std::vector<math::ScatterInstance> out;

    // §1.3 sight wedges: POI standpoints (sites + watchpoint) -> the L0.
    SightWedges wedges;
    wedges.crag_radius = layout.crag.radius;
    const auto ground_at = [&](glm::vec2 p) {
        return water_at(hydro, layout, p, macro_height(seed, layout, p)).height;
    };
    const float l0_top =
        ground_at(layout.crag.center) + L0_AIM_ABOVE_PEAK;
    const auto add_standpoint = [&](glm::vec2 pos) {
        const glm::vec2 to_crag = layout.crag.center - pos;
        const float dist = glm::length(to_crag);
        if (dist < layout.crag.radius) return; // standing on the L0 itself
        SightWedges::Standpoint sp;
        sp.pos = pos;
        sp.eye_y = ground_at(pos) + EYE_M;
        sp.dir = to_crag / dist;
        sp.dist = dist;
        sp.t_l0 = (l0_top - sp.eye_y) / dist;
        wedges.points.push_back(sp);
    };
    for (const SiteLayout& site : layout.sites) add_standpoint(site.position);
    add_standpoint(layout.watchpoint);

    ScatterCtx ctx{seed, layout, hydro, sites, wedges, erosion, paths, chunk_min,
                   chunk_max, out};
    scatter_trees(ctx);
    scatter_bushes(ctx);
    scatter_stones(ctx);
    scatter_entrance_markers(ctx);
    scatter_forest_floor(ctx);
    scatter_path_edges(ctx);
    scatter_forest_ground(ctx);
    return out;
}

} // namespace dfn::world
