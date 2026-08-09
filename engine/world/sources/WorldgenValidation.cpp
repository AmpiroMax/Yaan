/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 11:05:22
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
*/

#include "engine/world/sources/WorldgenValidation.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenScatter.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {
constexpr float CHUNK_SIZE_M = static_cast<float>(config::CHUNK_SIZE);
constexpr float EYE_M = static_cast<float>(config::PLAYER_EYE_HEIGHT);
constexpr float WALK_SLOPE = static_cast<float>(config::PLAYER_MAX_SLOPE);
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
    // Aim at the tower ruin's mid height on the peak (§6: 10-15 m topper).
    const float target_y = terrain_height(ctx, peak) + 8.0f;

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
            ++open;

            // Heightfield raycast standpoint -> tower top.
            const float eye_y = sp.height + EYE_M;
            const glm::vec2 to_peak = peak - p;
            const float dist = glm::length(to_peak);
            if (dist < 1.0f) {
                ++visible;
                continue;
            }
            const glm::vec2 dir = to_peak / dist;
            bool blocked = false;
            for (float t = RAY_STEP_M; t < dist - RAY_STEP_M; t += RAY_STEP_M) {
                const float ray_y = eye_y + (target_y - eye_y) * (t / dist);
                if (terrain_height(ctx, p + dir * t) > ray_y + 0.25f) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) ++visible;
        }
    }
    return open == 0 ? 0.0f : static_cast<float>(visible) / static_cast<float>(open);
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
