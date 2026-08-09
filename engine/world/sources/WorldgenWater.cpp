/*
Created: 09:08:2026 - 13:28:27
Last updated: 09:08:2026 - 13:28:27
Module: engine/world
File: engine/world/sources/WorldgenWater.cpp

Responsibility:
- Per-sample water QUERY side of P2 hydrology (split out of
  WorldgenHydrology.cpp near the 800-line limit): the carve + water surface +
  distance sampling over a built HydrologyData — water_at / carve_height and
  their shared implementation.

Key items:
- water_sample_impl (single source of height math), water_at, carve_height.

Dependencies:
- Uses: WorldgenHydrology.h, WorldgenNoise.h, generated constants.
- Used by: dfn_world (Worldgen.cpp, WorldgenSites/Scatter, ChunkManager).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- water_at and carve_height MUST return identical heights (single impl,
  with_distance only toggles bookkeeping) — generate_chunk equality depends
  on it and a test pins it.
- Position-based and stateless (Rule 13.1): chunk independence relies on it.
*/
/*
UPD:
- 09:08:2026 - 13:28:27: Split from WorldgenHydrology.cpp (file-size limit;
  hydrology build vs query responsibilities separated).
*/

#include "engine/world/sources/WorldgenHydrology.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>

namespace dfn::world {

namespace {

using noise::smoothstep01;

constexpr float CELL = static_cast<float>(config::WORLDGEN_HYDRO_GRID_STEP);
constexpr float LAKE_LEVEL_M = static_cast<float>(config::LAKE_LEVEL_TESTBED);
constexpr float FORD_DEPTH_M = static_cast<float>(config::FORD_DEPTH_MAX);
constexpr float BANK_BLEND = static_cast<float>(config::RIVER_BANK_BLEND_FACTOR);
constexpr float DIST_RANGE_M = static_cast<float>(config::DIST_TO_WATER_RANGE);
constexpr uint32_t INVALID = std::numeric_limits<uint32_t>::max();

} // namespace

namespace {

/// Single implementation behind water_at and carve_height: `with_distance`
/// skips only distance/near-level bookkeeping, never height math — the two
/// paths return identical heights by construction.
WaterSample water_sample_impl(const HydrologyData& hydro, const TestbedLayout& layout,
                              glm::vec2 world, float h, bool with_distance) {
    WaterSample out;
    float dist = std::numeric_limits<float>::max();

    // --- Lake --------------------------------------------------------------------
    const float q = lake_norm_radius(layout.lake, world);
    if (q < 1.0f) {
        dist = 0.0f;
        out.near_level = LAKE_LEVEL_M;
        if (h < LAKE_LEVEL_M) {
            out.water_surface = LAKE_LEVEL_M;
        }
    } else if (with_distance) {
        const float from_center = glm::length(world - layout.lake.center);
        const float lake_d = (q - 1.0f) / q * from_center;
        if (lake_d < dist) {
            dist = lake_d;
            out.near_level = LAKE_LEVEL_M;
        }
    }

    // --- River (nearest station via 3x3 bins) ------------------------------------
    if (!hydro.stations.empty() && hydro.bin_size > 0.0f) {
        const glm::vec2 rel = world - hydro.grid_origin;
        const int32_t bx = static_cast<int32_t>(std::floor(rel.x / hydro.bin_size));
        const int32_t bz = static_cast<int32_t>(std::floor(rel.y / hydro.bin_size));
        uint32_t nearest = INVALID;
        float best_d = std::numeric_limits<float>::max();
        for (int32_t dz = -1; dz <= 1; ++dz) {
            for (int32_t dx = -1; dx <= 1; ++dx) {
                const int32_t x = bx + dx, z = bz + dz;
                if (x < 0 || z < 0 || x >= static_cast<int32_t>(hydro.bins_w)
                    || z >= static_cast<int32_t>(hydro.bins_h)) {
                    continue;
                }
                for (const uint32_t i :
                     hydro.station_bins[static_cast<std::size_t>(z) * hydro.bins_w
                                        + static_cast<std::size_t>(x)]) {
                    const float d = glm::length(hydro.stations[i].position - world);
                    if (d < best_d) {
                        best_d = d;
                        nearest = i;
                    }
                }
            }
        }
        if (nearest != INVALID) {
            const math::RiverStation& st = hydro.stations[nearest];
            const float wl = st.surface_height;
            const float hw = st.half_width;
            if (best_d <= hw) {
                // Trapezoid channel (§3.1 step 5): the bed is CLAMPED into the
                // designed cross-section band — carved down to the trapezoid,
                // but also raised up to full depth. The raise is what makes a
                // ford a ford (§3.1 step 6 "raise the bed"): with carve_depth
                // capped at FORD_DEPTH_MAX there, natural deeper terrain can
                // never leave a swimming hole on a crossing.
                const float t = best_d / hw;
                const float prof = t <= 0.5f ? 1.0f : (1.0f - t) * 2.0f;
                const float depth = hydro.carve_depth[nearest];
                h = std::clamp(h, wl - depth, wl - depth * prof);
                if (h < wl) {
                    out.water_surface = std::max(out.water_surface, wl);
                }
                dist = 0.0f;
                out.near_level = wl;
            } else {
                const float band = BANK_BLEND * (2.0f * hw); // factor x full width
                if (best_d <= hw + band && h > wl) {
                    const float s = smoothstep01((best_d - hw) / band);
                    h = wl + (h - wl) * s; // banks descend smoothly to the waterline
                }
                if (best_d - hw < dist) {
                    dist = best_d - hw;
                    out.near_level = wl;
                }
            }
        }
    }

    // --- Ponds (coarse cells) ------------------------------------------------------
    if (out.water_surface == math::NO_WATER && !hydro.fill_level.empty()) {
        const int32_t cx = static_cast<int32_t>(std::floor((world.x - hydro.grid_origin.x) / CELL));
        const int32_t cz = static_cast<int32_t>(std::floor((world.y - hydro.grid_origin.y) / CELL));
        if (cx >= 0 && cz >= 0 && cx < static_cast<int32_t>(hydro.grid_w)
            && cz < static_cast<int32_t>(hydro.grid_h)) {
            const float fill =
                hydro.fill_level[static_cast<std::size_t>(cz) * hydro.grid_w
                                 + static_cast<std::size_t>(cx)];
            if (fill != math::NO_WATER && q >= 1.0f && h < fill) {
                out.water_surface = fill;
                h = std::min(h, fill - FORD_DEPTH_M); // shallow pond bed
                if (corridor_distance(layout, world)
                    <= static_cast<float>(config::CORRIDOR_WIDTH) * 0.5f + 2.0f) {
                    // A corridor wades through this pond: raise the bed so the
                    // crossing stays ford-shallow (§3.1 step 6 applies to any
                    // water the chain crosses, not only the channel).
                    h = std::max(h, fill - FORD_DEPTH_M);
                }
                dist = 0.0f;
                out.near_level = fill;
            }
        }
    }

    // --- Far-field distance from the coarse Dijkstra grid --------------------------
    if (with_distance && dist > 0.0f && !hydro.coarse_dist.empty()) {
        const float fx = std::clamp((world.x - hydro.grid_origin.x) / CELL, 0.0f,
                                    static_cast<float>(hydro.grid_w - 1));
        const float fz = std::clamp((world.y - hydro.grid_origin.y) / CELL, 0.0f,
                                    static_cast<float>(hydro.grid_h - 1));
        const uint32_t x0 = static_cast<uint32_t>(fx), z0 = static_cast<uint32_t>(fz);
        const uint32_t x1 = std::min(x0 + 1, hydro.grid_w - 1);
        const uint32_t z1 = std::min(z0 + 1, hydro.grid_h - 1);
        const float tx = fx - static_cast<float>(x0), tz = fz - static_cast<float>(z0);
        auto at = [&](uint32_t x, uint32_t z) {
            return hydro.coarse_dist[static_cast<std::size_t>(z) * hydro.grid_w + x];
        };
        const float d0 = at(x0, z0) + (at(x1, z0) - at(x0, z0)) * tx;
        const float d1 = at(x0, z1) + (at(x1, z1) - at(x0, z1)) * tx;
        dist = std::min(dist, d0 + (d1 - d0) * tz);
    }

    out.height = h;
    // Valid to at least SETTLEMENT_WATER_DIST, saturated at DIST_TO_WATER_RANGE
    // (§3.3 range requirement — bounded for render's field packing).
    out.dist_to_water = std::clamp(dist, 0.0f, DIST_RANGE_M);
    return out;
}

} // namespace

WaterSample water_at(const HydrologyData& hydro, const TestbedLayout& layout, glm::vec2 world,
                     float h) {
    return water_sample_impl(hydro, layout, world, h, true);
}

float carve_height(const HydrologyData& hydro, const TestbedLayout& layout, glm::vec2 world,
                   float h) {
    return water_sample_impl(hydro, layout, world, h, false).height;
}

} // namespace dfn::world
