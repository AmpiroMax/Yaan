/*
Created: 09:08:2026 - 11:05:22
Last updated: 12:08:2026 - 22:55:00
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
- 09:08:2026 - 15:18:34: Castle validation: the castle mass enters the C1 occlusion heightfield like canopy and its footprint is excluded from standpoints; hierarchy + access invariants implemented on the same raycast machinery.
- 09:08:2026 - 15:31:04: Rule C2-testbed implemented on the R4 subtended-angle machinery: apparent SIZE (object height / distance, not the elevation angle of its top — that conflated size with ground elevation, and R4 now uses the corrected measure too), §1.5 readability gate (sub-8 px specks cannot crowd), R1 body-backing exemption, L0 exempt, composite POIs once; widest-coequal-group via a sorted sliding window.
- 09:08:2026 - 15:36:59: Large-mass guard implemented over the same grouping (filter to large members, then widest coequal window); PX_PER_RAD factored out of the readability threshold.
- 09:08:2026 - 19:13:01: C1 CORRECTNESS FIX: the landmark's own body is no longer counted as an occluder of itself. The aim point is peak + L0_AIM_ABOVE_PEAK (fixed) while LANDMARK_CLEARANCE_FACTOR multiplies against terrain essentially at peak height, so near-summit ground out-angled the summit once 0.2*(peak - eye) exceeded the aim margin — above a ~60 m peak the test returned 0.000 for EVERY standpoint regardless of the world, and below it the measure was biased down. Seed 1 C1 was 0.621 measured, is 0.776 true. This invalidated the recorded 'raising the peak lowers clearance' finding, which was an artifact of this bug rather than a property of landmarks.
- 11:08:2026 - 15:15:55: §10.1's detrended bumpiness probe and the standpoint search, which ranks by TREND and never by the sigma it reports. relief_floor_binds' exemption list aligned to WorldgenRelief's masks -- where they disagreed the floor was being read on ground the generator was told to keep flat.
- 12:08:2026 - 22:55:00: canopy_height_at(ctx, ...) — the C1 raycast now sees the
  great oak, which stands in a gap in the forest mask; PX_PER_RAD reads
  WorldgenPlacement's one definition (it had two in this zone).
*/

#include "engine/world/sources/WorldgenValidation.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenCastle.h"
#include "engine/world/sources/WorldgenPlacement.h"
#include "engine/world/sources/WorldgenScatter.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <utility>
#include <vector>

namespace dfn::world {

namespace {
constexpr float CHUNK_SIZE_M = static_cast<float>(config::CHUNK_SIZE);
constexpr float EYE_M = static_cast<float>(config::PLAYER_EYE_HEIGHT);
constexpr float WALK_SLOPE = static_cast<float>(config::PLAYER_MAX_SLOPE);
constexpr float CLEARANCE = static_cast<float>(config::LANDMARK_CLEARANCE_FACTOR);
// Rule C2-testbed: attractors are "coequal" when their subtended heights lie
// within this ratio of each other (the L0 is exempt — C1 mandates it).
constexpr float COEQUAL_RATIO = static_cast<float>(config::COEQUAL_ANGLE_RATIO);
// §1.5 readability: a silhouette needs SILHOUETTE_MIN_PX to read as a shape,
// so an attractor only competes for attention when its apparent size clears
// that. Vertical angular resolution = INTERNAL_RES_H / CAMERA_FOV_Y.
// PX_PER_RAD lives in WorldgenPlacement.h now (Rule 32: it had three copies,
// and a read-distance that disagrees with itself is how "readable" comes to
// mean two things in one build).
const float READABLE_MIN_APPARENT =
    static_cast<float>(config::SILHOUETTE_MIN_PX) / screen_px_per_rad();
// Large-mass guard: above this apparent size an attractor is a mass, not a
// mark on the horizon, and three of them crowd even though three marks do not.
const float LARGE_MIN_APPARENT = static_cast<float>(config::COEQUAL_LARGE_PX) / screen_px_per_rad();
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
                // THE LANDMARK IS NOT AN OCCLUDER OF ITSELF. C1 asks "can you
                // see the crag"; its own body is the thing being seen. Counting
                // it made the near-summit terrain, scaled by
                // LANDMARK_CLEARANCE_FACTOR, out-angle the aim point as soon as
                // 0.2*(peak - eye) exceeded L0_AIM_ABOVE_PEAK — i.e. above a
                // ~60 m peak the test reported 0.000 for EVERY standpoint no
                // matter what the world looked like, and below it the measure
                // was already being dragged down. This was the whole basis of
                // the recorded "raising the peak lowers clearance" finding.
                if (crag_distance(layout, q) < layout.crag.radius) {
                    continue;
                }
                const float terrain = terrain_height(ctx, q);
                // Occlusion heightfield = terrain + canopy + CASTLE MASS
                // (§6.1.1: the castle enters exactly like canopy and may
                // never be the reason the L0 fails C1).
                const float occ_top =
                    terrain
                    + std::max(canopy_height_at(ctx, q, terrain),
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
        float base;     ///< ground at the attractor's own foot, meters
        bool is_seat;   ///< part of the castle+barrow composite POI
        bool is_castle; ///< the castle itself (removed for the baseline)
        bool is_l0;     ///< the dominant landmark — exempt from Rule C2-testbed
    };
    // Apparent SIZE (subtended height): the attractor's own height divided by
    // its distance — NOT the elevation angle of its top, which would conflate
    // "how big it looks" with "how high the ground under it is".
    const auto apparent = [](const Attractor& a, glm::vec2 from) {
        const float d = glm::length(a.pos - from);
        return d > 1.0f ? std::max(0.0f, a.top - a.base) / d : 0.0f;
    };
    std::vector<Attractor> attractors;
        // The L0's foot: the terrain at its footprint edge, so its apparent size
    // is the whole crag mass, not just the tower on top.
    const float peak_base = terrain_height(ctx, peak_xz + glm::vec2{layout.crag.radius, 0.0f});
    attractors.push_back(
        {peak_xz, peak_y + L0_AIM_ABOVE_PEAK, peak_base, false, false, true});
    for (std::size_t i = 0; i < ctx.sites.entities.size(); ++i) {
        const SiteType type = ctx.sites.types[i];
        const glm::vec2 pos = ctx.sites.entities[i].position_xz;
        const bool seat = type == SiteType::CastleSolar
                       || (type == SiteType::DungeonEntrance
                           && glm::length(pos - castle.center)
                                  <= static_cast<float>(config::CASTLE_BARROW_DIST_MAX));
        switch (type) {
        case SiteType::Tavern: // the hamlet counts once, via its anchor
        case SiteType::Shrine:
        case SiteType::DungeonEntrance:
        case SiteType::CastleSolar:
            attractors.push_back({pos,
                                  terrain_height(ctx, pos) + site_archetype(type).bounds_max.y,
                                  terrain_height(ctx, pos), seat,
                                  type == SiteType::CastleSolar, false});
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
    // `with_castle` off measures the layout as it would be with no castle at
    // all (neither attractor nor occluder) — the C2 baseline.
    const auto visible = [&](glm::vec2 from, float eye_y, glm::vec2 to, float top_y,
                             bool with_castle) {
        const glm::vec2 delta = to - from;
        const float dist = glm::length(delta);
        if (dist < 1.0f) return true;
        const glm::vec2 dir = delta / dist;
        const float t_target = (top_y - eye_y) / dist;
        for (float t = RAY_STEP_M; t < dist - RAY_STEP_M; t += RAY_STEP_M) {
            const glm::vec2 q = from + dir * t;
            const float terrain = terrain_height(ctx, q);
            float occ = terrain + canopy_height_at(ctx, q, terrain);
            if (with_castle) {
                occ = std::max(occ, terrain + castle_occluder_height(castle, q));
            }
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
            uint32_t count_no_castle = 0;
            bool seat_counted = false;
            bool seat_counted_nc = false;
            // Subtended heights of the visible non-L0 attractors, for the
            // Rule C2-testbed pairwise test (same tangent measure as R4).
            std::vector<float> subtended;      // R1-adjusted (the gate)
            std::vector<float> subtended_raw;   // every readable attractor
            std::vector<float> subtended_no_castle; // R1-adjusted, castle removed
            for (const Attractor& a : attractors) {
                if (visible(p, eye_y, a.pos, a.top, true)) {
                    if (!a.is_seat || !seat_counted) {
                        ++count;
                        seat_counted = seat_counted || a.is_seat;
                        if (!a.is_l0) {
                            // Only attractors that actually READ can crowd
                            // each other (§1.5) — a 4 px speck on the horizon
                            // is not competing with anything.
                            const float size = apparent(a, p);
                            if (size >= READABLE_MIN_APPARENT) {
                                subtended_raw.push_back(size);
                                // R1: an attractor standing inside the L0's
                                // angular footprint, nearer than the peak,
                                // reads against the crag's body and cannot
                                // steal the skyline — it is part of the L0's
                                // composition, not a coequal rival.
                                const glm::vec2 to_peak = peak_xz - p;
                                const float peak_dist = glm::length(to_peak);
                                bool backed_by_l0 = false;
                                if (peak_dist > 1.0f) {
                                    const glm::vec2 u = to_peak / peak_dist;
                                    const glm::vec2 rel = a.pos - p;
                                    const float along = glm::dot(rel, u);
                                    const float lateral =
                                        std::fabs(rel.x * u.y - rel.y * u.x);
                                    backed_by_l0 =
                                        along > 0.0f && along < peak_dist
                                        && lateral < layout.crag.radius;
                                }
                                if (!backed_by_l0) {
                                    subtended.push_back(size);
                                    if (!a.is_castle) subtended_no_castle.push_back(size);
                                }
                            }
                        }
                    }
                }
                if (!a.is_castle && visible(p, eye_y, a.pos, a.top, false)) {
                    if (!a.is_seat || !seat_counted_nc) {
                        ++count_no_castle;
                        seat_counted_nc = seat_counted_nc || a.is_seat;
                    }
                }
            }
            // "No coequal crowd": the largest group whose apparent sizes all
            // lie within COEQUAL_ANGLE_RATIO of one another. Comparability is
            // an interval property once sorted (a,b comparable iff
            // max/min <= ratio), so the largest such group is the widest
            // sliding window satisfying back/front <= ratio.
            const auto widest_coequal_group = [](std::vector<float>& sizes) {
                std::sort(sizes.begin(), sizes.end());
                std::size_t lo = 0;
                uint32_t best = 0;
                for (std::size_t hi_i = 0; hi_i < sizes.size(); ++hi_i) {
                    while (sizes[lo] > 0.0f && sizes[hi_i] / sizes[lo] > COEQUAL_RATIO) {
                        ++lo;
                    }
                    best = std::max(best, static_cast<uint32_t>(hi_i - lo + 1));
                }
                return best;
            };
            out.max_coequal_visible =
                std::max(out.max_coequal_visible, widest_coequal_group(subtended));
            out.max_coequal_visible_raw =
                std::max(out.max_coequal_visible_raw, widest_coequal_group(subtended_raw));
            out.max_coequal_visible_without_castle =
                std::max(out.max_coequal_visible_without_castle,
                         widest_coequal_group(subtended_no_castle));
            // Guard: same grouping over the large members only, so every
            // member of a counted group is >= COEQUAL_LARGE_PX by filtering.
            std::vector<float> large;
            for (const float s : subtended) {
                if (s >= LARGE_MIN_APPARENT) large.push_back(s);
            }
            out.max_coequal_large =
                std::max(out.max_coequal_large, widest_coequal_group(large));
            out.max_attractors = std::max(out.max_attractors, count);
            out.max_attractors_without_castle =
                std::max(out.max_attractors_without_castle, count_no_castle);

            // R4 dominance + R2 crown, only where both are visible at range.
            const float d_castle = glm::length(p - castle.center);
            const float d_peak = glm::length(p - peak_xz);
            if (d_castle < 300.0f || d_peak < 1.0f) continue;
            if (!visible(p, eye_y, castle.center, castle.top_elevation(), true)) continue;
            if (!visible(p, eye_y, peak_xz, peak_y + L0_AIM_ABOVE_PEAK, true)) continue;
            const float castle_sub =
                (castle.top_elevation() - castle.pad_height) / d_castle;
            const float crag_sub = (peak_y + L0_AIM_ABOVE_PEAK - peak_base) / d_peak;
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

CastleAccess castle_access(const WorldGenContext& ctx) {
    CastleAccess out;
    const CastleBuild& castle = ctx.sites.castle;
    if (!castle.valid) {
        return out;
    }
    // --- Access invariant: walk the ramp centreline from its foot up to the
    // gate threshold at fine spacing. Linear grade means the average IS the
    // local slope; the step check catches any discontinuity a later terrace
    // edit might introduce.
    const glm::vec2 foot = castle_ramp_foot(castle);
    const glm::vec2 gate = castle_gate_point(castle);
    const float run = glm::length(gate - foot);
    const int steps = std::max(4, static_cast<int>(run / 0.5f));
    float prev = terrain_height(ctx, foot);
    const float start_h = prev;
    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float h = terrain_height(ctx, foot + (gate - foot) * t);
        out.ramp_max_step = std::max(out.ramp_max_step, std::fabs(h - prev));
        prev = h;
    }
    out.ramp_avg_slope = run > 0.0f ? std::atan(std::fabs(prev - start_h) / run) : 0.0f;

    // --- Barrow sightline: from the yard and from the gate to the Backbarrow
    // entrance, over terrain + canopy only (the castle's own mass is not an
    // occluder for this ruling).
    glm::vec2 barrow{0.0f};
    bool found = false;
    for (std::size_t i = 0; i < ctx.sites.entities.size(); ++i) {
        if (ctx.sites.types[i] != SiteType::DungeonEntrance) continue;
        const glm::vec2 pos = ctx.sites.entities[i].position_xz;
        if (glm::length(pos - castle.center)
            <= static_cast<float>(config::CASTLE_BARROW_DIST_MAX) + 20.0f) {
            barrow = pos;
            found = true;
            break;
        }
    }
    if (!found) {
        return out;
    }
    const float barrow_top =
        terrain_height(ctx, barrow) + site_archetype(SiteType::DungeonEntrance).bounds_max.y;

    const auto clear_line = [&](glm::vec2 from) {
        const float eye_y = terrain_height(ctx, from) + EYE_M;
        const glm::vec2 delta = barrow - from;
        const float dist = glm::length(delta);
        if (dist < 1.0f) return true;
        const glm::vec2 dir = delta / dist;
        const float t_target = (barrow_top - eye_y) / dist;
        for (float t = 2.0f; t < dist - 2.0f; t += 2.0f) {
            const glm::vec2 q = from + dir * t;
            const float terrain = terrain_height(ctx, q);
            const float occ =
                terrain + canopy_height_at(ctx, q, terrain);
            if ((occ - eye_y) / t > t_target) return false;
        }
        return true;
    };
    out.barrow_visible_from_yard = clear_line(castle_yard_point(castle));
    out.barrow_visible_from_gate = clear_line(gate);
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

// --- §10.1 THE BUMPINESS INSTRUMENT -------------------------------------------

GroundRelief ground_relief_20m(const WorldGenContext& ctx, glm::vec2 centre) {
    constexpr float R = GROUND_RELIEF_DISC_RADIUS;
    constexpr float STEP = static_cast<float>(config::HEIGHTMAP_STEP);
    const int n = static_cast<int>(R / STEP);

    // The disc is SYMMETRIC about the centre, so sum(x) = sum(z) = sum(xz) = 0
    // and the least-squares plane has a closed form: a = Sxh/Sxx, b = Szh/Szz,
    // c = mean(h). Solving a 3x3 would give the same answer with a pivot to get
    // wrong; the symmetry is a property of the sampling, so it is used.
    double sxx = 0.0, szz = 0.0, sxh = 0.0, szh = 0.0, sh = 0.0;
    uint32_t count = 0;
    float lo = 1e9f, hi = -1e9f;
    for (int j = -n; j <= n; ++j) {
        for (int i = -n; i <= n; ++i) {
            const float dx = static_cast<float>(i) * STEP;
            const float dz = static_cast<float>(j) * STEP;
            if (dx * dx + dz * dz > R * R) continue;
            const float h = terrain_height(ctx, centre + glm::vec2{dx, dz});
            sxx += static_cast<double>(dx) * dx;
            szz += static_cast<double>(dz) * dz;
            sxh += static_cast<double>(dx) * h;
            szh += static_cast<double>(dz) * h;
            sh += h;
            lo = std::min(lo, h);
            hi = std::max(hi, h);
            ++count;
        }
    }
    GroundRelief out;
    if (count == 0) return out;
    const double a = sxx > 0.0 ? sxh / sxx : 0.0;
    const double b = szz > 0.0 ? szh / szz : 0.0;
    const double c = sh / static_cast<double>(count);

    double ss = 0.0;
    for (int j = -n; j <= n; ++j) {
        for (int i = -n; i <= n; ++i) {
            const float dx = static_cast<float>(i) * STEP;
            const float dz = static_cast<float>(j) * STEP;
            if (dx * dx + dz * dz > R * R) continue;
            const double r = static_cast<double>(terrain_height(ctx, centre + glm::vec2{dx, dz}))
                           - (a * dx + b * dz + c);
            ss += r * r;
        }
    }
    out.samples = count;
    out.sigma = static_cast<float>(std::sqrt(ss / static_cast<double>(count)));
    out.trend_slope = static_cast<float>(std::atan(std::sqrt(a * a + b * b)));
    out.p2p = hi - lo;
    return out;
}

bool relief_floor_binds(const WorldGenContext& ctx, glm::vec2 world) {
    // Everything excluded here is flattened by an APPROVED rule, so a low
    // reading in it is compliance and not a defect (§10.1.2's exemption list).
    // THE TWO LISTS MUST AGREE. WorldgenRelief.h's masks and this exemption
    // list are the same clause read from two sides, and where they disagreed
    // the floor was being measured on ground the generator had been told to
    // keep flat: the corridor taper runs from half a width to a full width, so
    // excluding only the half-width left a 5 m ring of "legal" ground with the
    // relief faded out of it (σ 0.037 m on ground the search then ranked
    // flattest). Every threshold below is the OUTER edge of the matching mask.
    const TestbedLayout& layout = ctx.params.layout;
    if (corridor_distance(layout, world) < static_cast<float>(config::CORRIDOR_WIDTH)) {
        return false;
    }
    if (layout.crag.radius > 0.0f
        && glm::length(world - layout.crag.center) < layout.crag.radius * 1.25f) {
        return false; // the massif and its mask fade — §2.8 owns that surface
    }
    for (const BuildingPad& pad : ctx.sites.pads) {
        if (glm::length(world - pad.center) < pad.radius + pad.blend) return false;
    }
    for (const EntranceWorks& w : ctx.sites.entrances) {
        if (!w.valid) continue;
        if (glm::length(world - w.center) < w.mound_radius) return false;
        if (glm::length(world - w.portal) < w.forecourt_length) return false;
    }
    // The shore band: water flattens its own margins (§2.7), and §3.3 sizes
    // that band as SHORE_SAND_DIST. Wet ground is not ground.
    const WaterSample w =
        water_at(ctx.hydrology, layout, world, macro_height(ctx.params.seed, layout, world));
    if (w.water_surface != math::NO_WATER) return false;
    if (w.dist_to_water < static_cast<float>(config::SHORE_SAND_DIST)) return false;
    // The path tread on stands that have one.
    if (ctx.paths.flatten_at(world, terrain_height(ctx, world)) != 0.0f) return false;
    return true;
}

std::vector<glm::vec2> flattest_legal_standpoints(const WorldGenContext& ctx, std::size_t count,
                                                  float search_step) {
    const float extent_min_x = static_cast<float>(ctx.params.min_chunk.x) * CHUNK_SIZE_M;
    const float extent_min_z = static_cast<float>(ctx.params.min_chunk.z) * CHUNK_SIZE_M;
    const float extent_max_x = static_cast<float>(ctx.params.max_chunk.x + 1) * CHUNK_SIZE_M;
    const float extent_max_z = static_cast<float>(ctx.params.max_chunk.z + 1) * CHUNK_SIZE_M;

    std::vector<std::pair<float, glm::vec2>> ranked; // (trend slope, position)
    // Keep a disc-radius margin off the world edge so every candidate's window
    // is a full disc: a clipped disc reads a different band than a whole one.
    const float margin = GROUND_RELIEF_DISC_RADIUS + 1.0f;
    for (float z = extent_min_z + margin; z <= extent_max_z - margin; z += search_step) {
        for (float x = extent_min_x + margin; x <= extent_max_x - margin; x += search_step) {
            const glm::vec2 p{x, z};
            if (!relief_floor_binds(ctx, p)) continue;
            // Ranked on the TREND ONLY. The disc is read once and σ is
            // discarded here on purpose — see the header.
            const GroundRelief g = ground_relief_20m(ctx, p);
            ranked.emplace_back(g.trend_slope, p);
        }
    }
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<glm::vec2> out;
    for (std::size_t i = 0; i < ranked.size() && i < count; ++i) out.push_back(ranked[i].second);
    return out;
}

} // namespace dfn::world
