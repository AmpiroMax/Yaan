/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 13:12:19
Module: engine/world
File: engine/world/sources/WorldgenValidation.cpp

Responsibility:
- Validation pass implementation: monotonic river walk, heightfield raycast
  toward the L0 tower, corridor slope averaging.

Key items:
- river_is_monotonic, landmark_visibility_fraction, max_corridor_avg_slope.

Dependencies:
- Uses: WorldgenValidation.h, WorldgenMacro.h, WorldgenScatter.h, config.
- Used by: dfn_world, worldgen tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic sampling grids only — validation must agree across runs.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — implementation.
- 09:08:2026 - 13:12:19: Stage 3b amendments: C1 raycast against terrain + canopy with LANDMARK_CLEARANCE_FACTOR (tangent comparison); max_corridor_water_depth.
*/

#include "engine/world/sources/WorldgenValidation.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenCastle.h"
#include "engine/world/sources/WorldgenScatter.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <vector>

namespace dfn::world {

namespace {
constexpr float CHUNK_SIZE_M = static_cast<float>(config::CHUNK_SIZE);
constexpr float EYE_M = static_cast<float>(config::PLAYER_EYE_HEIGHT);
constexpr float WALK_SLOPE = static_cast<float>(config::PLAYER_MAX_SLOPE);
constexpr float CLEARANCE = static_cast<float>(config::LANDMARK_CLEARANCE_FACTOR);
constexpr float STANDPOINT_GRID_M = 32.0f; // coarse but deterministic sampling
constexpr float RAY_STEP_M = 4.0f;
} // namespace

bool river_is_monotonic(const HydrologyData& hydro) {
    if (!hydro.ok) {
        return false;
    }
    for (std::size_t s = 0; s + 1 < hydro.segment_offsets.size(); ++s) {
        for (uint32_t i = hydro.segment_offsets[s] + 1; i < hydro.segment_offsets[s + 1]; ++i) {
            if (hydro.stations[i].surface_height
                > hydro.stations[i - 1].surface_height + 1e-4f) {
                return false; // a climbing river = failed generation
            }
        }
    }
    return true;
}

float landmark_visibility_fraction(const WorldGenContext& ctx) {
    const TestbedLayout& layout = ctx.params.layout;
    const glm::vec2 peak = layout.crag.center;
    // Aim at the tower ruin's mid height on the peak (§6: 10-15 m topper) —
    // same aim as the P5 sight wedges.
    const float target_y = terrain_height(ctx, peak) + L0_AIM_ABOVE_PEAK;

    const glm::vec2 lo{static_cast<float>(ctx.params.min_chunk.x) * CHUNK_SIZE_M,
                       static_cast<float>(ctx.params.min_chunk.z) * CHUNK_SIZE_M};
    const glm::vec2 hi{static_cast<float>(ctx.params.max_chunk.x + 1) * CHUNK_SIZE_M,
                       static_cast<float>(ctx.params.max_chunk.z + 1) * CHUNK_SIZE_M};

    uint32_t open = 0;
    uint32_t visible = 0;
    for (float z = lo.y + STANDPOINT_GRID_M * 0.5f; z < hi.y; z += STANDPOINT_GRID_M) {
        for (float x = lo.x + STANDPOINT_GRID_M * 0.5f; x < hi.x; x += STANDPOINT_GRID_M) {
            const glm::vec2 p{x, z};
            // "Open walkable ground": dry, walkable slope, not inside forest
            // masses (trees occlude) and not on the landmark itself.
            const SurfacePoint sp = surface_point(ctx, p);
            if (sp.water_surface != math::NO_WATER) continue;
            if (in_forest_mass(layout, p)) continue;
            if (crag_distance(layout, p) < layout.crag.radius) continue;
            const float d = 2.0f;
            const float hx = terrain_height(ctx, {p.x + d, p.y})
                           - terrain_height(ctx, {p.x - d, p.y});
            const float hz = terrain_height(ctx, {p.x, p.y + d})
                           - terrain_height(ctx, {p.x, p.y - d});
            if (std::atan(std::sqrt(hx * hx + hz * hz) / (2.0f * d)) > WALK_SLOPE) continue;
            if (castle_occluder_height(ctx.sites.castle, p) > 0.0f) continue; // inside the mass
            ++open;

            // Canopy-aware raycast standpoint -> tower top with the C4
            // clearance factor: the L0's elevation angle must exceed every
            // occluder's (terrain + canopy) by LANDMARK_CLEARANCE_FACTOR.
            // Compared via tangents — all angles here are < 0.2 rad, where
            // the factor transfers within a fraction of a percent.
            const float eye_y = sp.height + EYE_M;
            const glm::vec2 to_peak = peak - p;
            const float dist = glm::length(to_peak);
            if (dist < 1.0f) {
                ++visible;
                continue;
            }
            const glm::vec2 dir = to_peak / dist;
            const float t_l0 = (target_y - eye_y) / dist;
            bool blocked = false;
            for (float t = RAY_STEP_M; t < dist - RAY_STEP_M; t += RAY_STEP_M) {
                const glm::vec2 q = p + dir * t;
                const float terrain = terrain_height(ctx, q);
                // Occlusion heightfield = terrain + canopy + CASTLE MASS
                // (§6.1.1: the castle enters exactly like canopy and may
                // never be the reason the L0 fails C1).
                const float occ_top =
                    terrain
                    + std::max(canopy_height_at(ctx.params.seed, layout, q, terrain),
                               castle_occluder_height(ctx.sites.castle, q));
                const float t_occ = (occ_top - eye_y) / t;
                if (t_occ * CLEARANCE > t_l0) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) ++visible;
        }
    }
    return open == 0 ? 0.0f : static_cast<float>(visible) / static_cast<float>(open);
}

float max_corridor_water_depth(const WorldGenContext& ctx) {
    float worst = 0.0f;
    for (const CorridorLayout& c : ctx.params.layout.corridors) {
        for (int i = 0; i + 1 < c.point_count; ++i) {
            const glm::vec2 a = c.points[i];
            const glm::vec2 b = c.points[i + 1];
            const float seg_len = glm::length(b - a);
            const int steps = std::max(1, static_cast<int>(seg_len / 2.0f));
            for (int s = 0; s <= steps; ++s) {
                const float t = static_cast<float>(s) / static_cast<float>(steps);
                const SurfacePoint sp = surface_point(ctx, a + (b - a) * t);
                if (sp.water_surface != math::NO_WATER) {
                    worst = std::max(worst, sp.water_surface - sp.height);
                }
            }
        }
    }
    return worst;
}

CastleHierarchy castle_hierarchy(const WorldGenContext& ctx) {
    CastleHierarchy out;
    const TestbedLayout& layout = ctx.params.layout;
    const CastleBuild& castle = ctx.sites.castle;
    const glm::vec2 peak_xz = layout.crag.center;
    const float peak_y = terrain_height(ctx, peak_xz);
    const float crown_base = peak_y - (peak_y - castle.pad_height) / 3.0f; // top third
    out.skyline_ceiling = peak_y - static_cast<float>(config::CASTLE_SKYLINE_MARGIN);
    if (!castle.valid) {
        return out;
    }
    out.top_elevation = castle.top_elevation();

    // Attractors: the L0 plus every L1. Castle + barrow are ONE composite POI
    // ("the seat", §6.1.2), so the castle is credited only when the barrow is
    // not already counted for that standpoint.
    struct Attractor {
        glm::vec2 pos;
        float top;
        bool is_seat;
    };
    std::vector<Attractor> attractors;
    attractors.push_back({peak_xz, peak_y + L0_AIM_ABOVE_PEAK, false});
    for (std::size_t i = 0; i < ctx.sites.entities.size(); ++i) {
        const SiteType type = ctx.sites.types[i];
        const glm::vec2 pos = ctx.sites.entities[i].position_xz;
        const bool seat = type == SiteType::CastleKeep
                       || (type == SiteType::DungeonEntrance
                           && glm::length(pos - castle.center)
                                  <= static_cast<float>(config::CASTLE_BARROW_DIST_MAX));
        switch (type) {
        case SiteType::Tavern: // the hamlet counts once, via its anchor
        case SiteType::Shrine:
        case SiteType::DungeonEntrance:
        case SiteType::CastleKeep:
            attractors.push_back(
                {pos, terrain_height(ctx, pos) + site_archetype(type).bounds_max.y, seat});
            break;
        default:
            break;
        }
    }

    const glm::vec2 lo{static_cast<float>(ctx.params.min_chunk.x) * CHUNK_SIZE_M,
                       static_cast<float>(ctx.params.min_chunk.z) * CHUNK_SIZE_M};
    const glm::vec2 hi{static_cast<float>(ctx.params.max_chunk.x + 1) * CHUNK_SIZE_M,
                       static_cast<float>(ctx.params.max_chunk.z + 1) * CHUNK_SIZE_M};

    // Line-of-sight over the same occlusion heightfield C1 uses.
    const auto visible = [&](glm::vec2 from, float eye_y, glm::vec2 to, float top_y) {
        const glm::vec2 delta = to - from;
        const float dist = glm::length(delta);
        if (dist < 1.0f) return true;
        const glm::vec2 dir = delta / dist;
        const float t_target = (top_y - eye_y) / dist;
        for (float t = RAY_STEP_M; t < dist - RAY_STEP_M; t += RAY_STEP_M) {
            const glm::vec2 q = from + dir * t;
            const float terrain = terrain_height(ctx, q);
            const float occ = terrain
                            + std::max(canopy_height_at(ctx.params.seed, layout, q, terrain),
                                       castle_occluder_height(castle, q));
            if ((occ - eye_y) / t > t_target) return false;
        }
        return true;
    };

    for (float z = lo.y + STANDPOINT_GRID_M * 0.5f; z < hi.y; z += STANDPOINT_GRID_M) {
        for (float x = lo.x + STANDPOINT_GRID_M * 0.5f; x < hi.x; x += STANDPOINT_GRID_M) {
            const glm::vec2 p{x, z};
            const SurfacePoint sp = surface_point(ctx, p);
            if (sp.water_surface != math::NO_WATER) continue;
            if (in_forest_mass(layout, p)) continue;
            if (crag_distance(layout, p) < layout.crag.radius) continue;
            if (castle_occluder_height(castle, p) > 0.0f) continue;
            const float eye_y = sp.height + EYE_M;

            uint32_t count = 0;
            bool seat_counted = false;
            for (const Attractor& a : attractors) {
                if (!visible(p, eye_y, a.pos, a.top)) continue;
                if (a.is_seat) {
                    if (seat_counted) continue; // composite POI counts once
                    seat_counted = true;
                }
                ++count;
            }
            out.max_attractors = std::max(out.max_attractors, count);

            // R4 dominance + R2 crown, only where both are visible at range.
            const float d_castle = glm::length(p - castle.center);
            const float d_peak = glm::length(p - peak_xz);
            if (d_castle < 300.0f || d_peak < 1.0f) continue;
            if (!visible(p, eye_y, castle.center, castle.top_elevation())) continue;
            if (!visible(p, eye_y, peak_xz, peak_y + L0_AIM_ABOVE_PEAK)) continue;
            const float castle_sub = (castle.top_elevation() - eye_y) / d_castle;
            const float crag_sub = (peak_y + L0_AIM_ABOVE_PEAK - eye_y) / d_peak;
            if (crag_sub > 0.0f) {
                out.max_ratio = std::max(out.max_ratio, castle_sub / crag_sub);
            }
            // R2: does the castle mass rise into the L0's crown along this ray?
            const glm::vec2 dir = glm::normalize(peak_xz - p);
            for (float t = RAY_STEP_M; t < d_peak; t += RAY_STEP_M) {
                const glm::vec2 q = p + dir * t;
                const float mass = castle_occluder_height(castle, q);
                if (mass <= 0.0f) continue;
                const float mass_top = terrain_height(ctx, q) + mass;
                // Elevation the ray to the crown base has at this distance.
                const float crown_ray = eye_y + (crown_base - eye_y) * (t / d_peak);
                if (mass_top > crown_ray) {
                    out.crown_occluded = true;
                }
            }
        }
    }
    return out;
}

float max_corridor_avg_slope(const WorldGenContext& ctx) {
    float worst = 0.0f;
    for (const CorridorLayout& c : ctx.params.layout.corridors) {
        float climb = 0.0f;
        float length = 0.0f;
        for (int i = 0; i + 1 < c.point_count; ++i) {
            const glm::vec2 a = c.points[i];
            const glm::vec2 b = c.points[i + 1];
            const float seg_len = glm::length(b - a);
            const int steps = std::max(1, static_cast<int>(seg_len / RAY_STEP_M));
            float h_prev = terrain_height(ctx, a);
            for (int s = 1; s <= steps; ++s) {
                const float t = static_cast<float>(s) / static_cast<float>(steps);
                const float h = terrain_height(ctx, a + (b - a) * t);
                climb += std::fabs(h - h_prev);
                h_prev = h;
            }
            length += seg_len;
        }
        if (length > 0.0f) {
            worst = std::max(worst, std::atan(climb / length));
        }
    }
    return worst;
}

} // namespace dfn::world
