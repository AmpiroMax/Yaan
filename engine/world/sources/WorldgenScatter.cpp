/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 11:05:22
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
*/

#include "engine/world/sources/WorldgenScatter.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenNoise.h"
#include "engine/world/sources/Worldgen.h"

#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

constexpr float TAU = 6.28318530717958647692f;
constexpr float TREE_SLOPE = static_cast<float>(config::TREE_SLOPE_MAX);
constexpr float CORRIDOR_HALF = static_cast<float>(config::CORRIDOR_WIDTH) * 0.5f;

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
    const float d = glm::length(p - layout.crag.center);
    if (d >= layout.forests.pine_annulus_r0 && d < layout.forests.pine_annulus_r1) return true;
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
    glm::vec2 chunk_min, chunk_max;
    std::vector<math::ScatterInstance>& out;

    [[nodiscard]] bool inside_chunk(glm::vec2 p) const {
        return p.x >= chunk_min.x && p.x < chunk_max.x && p.y >= chunk_min.y
            && p.y < chunk_max.y;
    }
    /// Carved terrain height (pads excluded — instances never stand on pads).
    [[nodiscard]] float ground(glm::vec2 p) const {
        return water_at(hydro, layout, p, macro_height(seed, layout, p)).height;
    }
    [[nodiscard]] float dist_to_water(glm::vec2 p) const {
        return water_at(hydro, layout, p, macro_height(seed, layout, p)).dist_to_water;
    }
    [[nodiscard]] float slope(glm::vec2 p) const {
        const float d = 2.0f;
        const float hx = ground({p.x + d, p.y}) - ground({p.x - d, p.y});
        const float hz = ground({p.x, p.y + d}) - ground({p.x, p.y - d});
        return std::atan(std::sqrt(hx * hx + hz * hz) / (2.0f * d));
    }
    [[nodiscard]] bool on_pad(glm::vec2 p) const {
        for (const BuildingPad& pad : sites.pads) {
            if (glm::length(p - pad.center) < pad.radius + pad.blend + 2.0f) return true;
        }
        return false;
    }
    [[nodiscard]] bool on_crag_rock(glm::vec2 p, float h) const {
        return glm::length(p - layout.crag.center) < layout.crag.radius
            && h >= layout.crag.rockline;
    }

    void add(glm::vec2 p, math::ScatterSpecies species, float yaw, float scale) {
        out.push_back(math::ScatterInstance{{p.x, ground(p), p.y}, yaw, scale, species});
    }

    /// Common tree suitability (§5 global rules + §2.4 corridor protection).
    [[nodiscard]] bool tree_ok(glm::vec2 p, float min_water_dist) const {
        if (corridor_distance(layout, p) < CORRIDOR_HALF + 2.0f) return false;
        if (on_pad(p)) return false;
        if (dist_to_water(p) < min_water_dist) return false;
        const float h = ground(p);
        if (on_crag_rock(p, h)) return false;
        return slope(p) <= TREE_SLOPE;
    }

    /// Iterates lattice cells of size `cell` overlapping the chunk.
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
        if (in_clearing(ctx.seed, ctx.layout, p) || !ctx.tree_ok(p, 3.0f)) return;
        ctx.add(p, math::ScatterSpecies::OakTree, rng.next_float01() * TAU,
                0.8f + rng.next_float01() * 0.4f);
    });
    // Pine (stream 41) — slightly tighter (§5.2 spacing 4-7).
    ctx.for_cells(spacing - 1.0f, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
        WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_TREE + 1, gx, gz);
        const glm::vec2 p =
            corner + glm::vec2{rng.next_float01(), rng.next_float01()} * (spacing - 1.0f);
        if (!ctx.inside_chunk(p) || !in_pine(ctx.layout, p) || in_oak(ctx.layout, p)) return;
        if (in_clearing(ctx.seed, ctx.layout, p) || !ctx.tree_ok(p, 3.0f)) return;
        ctx.add(p, math::ScatterSpecies::PineTree, rng.next_float01() * TAU,
                0.8f + rng.next_float01() * 0.4f);
    });
    // Birch (stream 42): banks only, outside the sand band (§5.3 + §5 "never
    // on sand"), loose lines — 45% keep.
    ctx.for_cells(8.0f, [&](int64_t gx, int64_t gz, glm::vec2 corner) {
        WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_TREE + 2, gx, gz);
        const glm::vec2 p = corner + glm::vec2{rng.next_float01(), rng.next_float01()} * 8.0f;
        if (!ctx.inside_chunk(p) || rng.next_float01() > 0.45f) return;
        const float d = ctx.dist_to_water(p);
        if (d <= static_cast<float>(config::SHORE_SAND_DIST)
            || d > static_cast<float>(config::BIRCH_WATER_DIST)) {
            return;
        }
        if (in_oak(ctx.layout, p) || !ctx.tree_ok(p, 0.0f)) return;
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
        if (ctx.on_pad(p) || ctx.dist_to_water(p) < 2.0f
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
        if (ctx.on_pad(p) || d_water < 0.5f
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
            if (!ctx.inside_chunk(p) || ctx.on_pad(p) || ctx.dist_to_water(p) < 1.0f) continue;
            if (corridor_distance(ctx.layout, p) < CORRIDOR_HALF + scale) continue;
            if (ctx.slope(p) > TREE_SLOPE) continue;
            ctx.add(p, math::ScatterSpecies::Stone, rng.next_float01() * TAU, scale);
        }
    });
    // Watchpoint (§7.1): outcrop cluster + lone skyline pine, deterministic.
    const glm::vec2 wp = ctx.layout.watchpoint;
    if (ctx.inside_chunk(wp)) {
        WorldGenRng rng = cell_rng(ctx.seed, STREAM_SCATTER_OUTCROP + 2, 0, 0);
        for (int b = 0; b < 4; ++b) {
            const float ang = rng.next_float01() * TAU;
            const glm::vec2 p = wp + glm::vec2{std::cos(ang), std::sin(ang)}
                                         * (2.0f + rng.next_float01() * 4.0f);
            ctx.add(p, math::ScatterSpecies::Stone, rng.next_float01() * TAU,
                    1.2f + rng.next_float01() * 1.2f);
        }
        ctx.add(wp + glm::vec2{3.0f, -2.0f}, math::ScatterSpecies::PineTree,
                rng.next_float01() * TAU, 1.25f);
    }
}

} // namespace

bool in_forest_mass(const TestbedLayout& layout, glm::vec2 world) {
    return in_oak(layout, world) || in_pine(layout, world);
}

std::vector<math::ScatterInstance> build_scatter(uint64_t seed, const TestbedLayout& layout,
                                                 const HydrologyData& hydro,
                                                 const SitesData& sites, glm::vec2 chunk_min,
                                                 glm::vec2 chunk_max) {
    std::vector<math::ScatterInstance> out;
    ScatterCtx ctx{seed, layout, hydro, sites, chunk_min, chunk_max, out};
    scatter_trees(ctx);
    scatter_bushes(ctx);
    scatter_stones(ctx);
    return out;
}

} // namespace dfn::world
