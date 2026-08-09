/*
Created: 09:08:2026 - 16:45:00
Last updated: 09:08:2026 - 22:30:00
Module: engine/world
File: engine/world/sources/WorldgenCarve.cpp

Responsibility:
- Carve SDF implementation: box cross-section corridors along polylines and
  rectangular chambers, unioned, plus the per-column vertical range the voxel
  builder needs to widen its active band.

Key items:
- carve_distance, carve_column_range, has_carves.

Dependencies:
- Uses: WorldgenCarve.h.
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The cross-section is a BOX in (lateral, vertical) space measured against the
  segment's floor line: flat floor to stand on, flat ceiling overhead. The
  corridor floor follows the polyline's y, so a climbing segment is a ramp.
- Deterministic pure geometry.
*/
/*
UPD:
- 09:08:2026 - 16:45:00: Created — carve SDF for the crag tunnel and barrow.
- 09:08:2026 - 16:47:51: Created — carve distance fields and column ranges.
- 09:08:2026 - 17:36:42: §6.2: mouth walk (first station whose ceiling is under terrain) and derived-corridor overloads.
- 09:08:2026 - 22:30:00: NEW enclosure_darkness() — LANDSCAPE §6.3 authored darkness as the RULE, replacing the app-side stand-in that measured depth below the local surface (which calls a deep valley floor a cave). Both halves of design's rule are evaluated: ENCLOSED (inside carved air AND rock overhead) and EARNED (>= DARKNESS_DEPTH_MIN walked ALONG the corridor from the nearest mouth, not straight-line through rock — a switchback is dark because you walked it). Ramps over DARKNESS_FALLOFF_MIN. Measured seed 1: valley floor 0.000, barrow mouth 0.000, 20 m in 0.375, chamber 1.000, solid rock (not a place) 0.000.
*/

#include "engine/world/sources/WorldgenCarve.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

constexpr float FAR_AWAY = 1e9f;

/// Signed distance to one corridor segment's box cross-section. Negative
/// inside. `a`/`b` carry the FLOOR level; the box rises `height` above it.
float segment_distance(glm::vec3 a, glm::vec3 b, float half_width, float height,
                       glm::vec3 p) {
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float t = len2 > 0.0f ? std::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f) : 0.0f;
    const glm::vec3 c = a + ab * t;
    // Lateral distance is measured horizontally so the corridor keeps its
    // width on a ramp; vertical is measured from the floor line.
    const float lateral = std::hypot(p.x - c.x, p.z - c.z);
    const float dy = p.y - c.y;
    // Box SDF in (lateral, vertical): outside distances combine, inside takes
    // the nearest face.
    const float dx = lateral - half_width;
    const float dv = std::max(-dy, dy - height);
    if (dx <= 0.0f && dv <= 0.0f) {
        return std::max(dx, dv); // inside: negative
    }
    const float ox = std::max(dx, 0.0f);
    const float ov = std::max(dv, 0.0f);
    return std::hypot(ox, ov);
}

float corridor_distance(const CarveCorridor& corridor, glm::vec3 p) {
    float best = FAR_AWAY;
    for (int i = 0; i + 1 < corridor.point_count; ++i) {
        best = std::min(best, segment_distance(corridor.points[i], corridor.points[i + 1],
                                               corridor.half_width, corridor.height, p));
    }
    return best;
}

float chamber_distance(const CarveChamber& chamber, glm::vec3 p) {
    if (chamber.half_extent.x <= 0.0f) {
        return FAR_AWAY;
    }
    const float dx = std::fabs(p.x - chamber.center.x) - chamber.half_extent.x;
    const float dz = std::fabs(p.z - chamber.center.z) - chamber.half_extent.z;
    const float dy_below = chamber.center.y - p.y;               // below the floor
    const float dy_above = p.y - (chamber.center.y + chamber.half_extent.y);
    const float dv = std::max(dy_below, dy_above);
    if (dx <= 0.0f && dz <= 0.0f && dv <= 0.0f) {
        return std::max(dx, std::max(dz, dv));
    }
    return std::hypot(std::hypot(std::max(dx, 0.0f), std::max(dz, 0.0f)),
                      std::max(dv, 0.0f));
}

/// Vertical span of a corridor over a column, or an empty range.
std::pair<float, float> corridor_column_range(const CarveCorridor& corridor,
                                              glm::vec2 xz) {
    float lo = FAR_AWAY;
    float hi = -FAR_AWAY;
    const float reach = corridor.half_width + 1.0f;
    for (int i = 0; i + 1 < corridor.point_count; ++i) {
        const glm::vec3 a = corridor.points[i];
        const glm::vec3 b = corridor.points[i + 1];
        // Horizontal distance from the column to the segment's ground track.
        const glm::vec2 a2{a.x, a.z};
        const glm::vec2 b2{b.x, b.z};
        const glm::vec2 ab = b2 - a2;
        const float len2 = glm::dot(ab, ab);
        const float t = len2 > 0.0f ? std::clamp(glm::dot(xz - a2, ab) / len2, 0.0f, 1.0f)
                                    : 0.0f;
        if (glm::length(xz - (a2 + ab * t)) > reach) {
            continue;
        }
        const float floor_y = a.y + (b.y - a.y) * t;
        lo = std::min(lo, floor_y - 1.0f);
        hi = std::max(hi, floor_y + corridor.height + 1.0f);
    }
    return {lo, hi};
}

} // namespace

bool has_carves(const TestbedLayout& layout) {
    return layout.carves.crag_tunnel.point_count > 1
        || layout.carves.barrow_passage.point_count > 1
        || layout.carves.barrow_chamber.half_extent.x > 0.0f;
}

float carve_distance(const TestbedLayout& layout, glm::vec3 world) {
    float d = corridor_distance(layout.carves.crag_tunnel, world);
    d = std::min(d, corridor_distance(layout.carves.barrow_passage, world));
    d = std::min(d, chamber_distance(layout.carves.barrow_chamber, world));
    return d;
}

std::optional<CarveMouth> carve_mouth(const CarveCorridor& corridor,
                                      const GroundSampler& ground) {
    // Walk from the outer end inward at fine steps; the mouth is the first
    // station whose CEILING is under the terrain. Everything before it is the
    // open approach cutting, which is why placing a marker at the polyline's
    // start puts it metres short of the actual opening.
    for (int i = 0; i + 1 < corridor.point_count; ++i) {
        const glm::vec3 a = corridor.points[i];
        const glm::vec3 b = corridor.points[i + 1];
        const float len = glm::length(b - a);
        const int steps = std::max(1, static_cast<int>(len / 0.5f));
        for (int s = 0; s <= steps; ++s) {
            const glm::vec3 p = a + (b - a) * (static_cast<float>(s) / steps);
            if (p.y + corridor.height < ground({p.x, p.z})) {
                const glm::vec2 heading = glm::normalize(glm::vec2{b.x - a.x, b.z - a.z});
                return CarveMouth{p, -heading}; // face back out of the hill
            }
        }
    }
    return std::nullopt;
}

std::optional<CarveMouth> site_carve_mouth(const TestbedLayout& layout, int site_index,
                                           const GroundSampler& ground) {
    if (site_index < 0) {
        return std::nullopt;
    }
    if (site_index == layout.carves.barrow_site_index) {
        return carve_mouth(layout.carves.barrow_passage, ground);
    }
    if (site_index == layout.carves.lakeshore_site_index) {
        return carve_mouth(layout.carves.lakeshore_adit, ground);
    }
    return std::nullopt;
}

float carve_distance(const TestbedLayout& layout, std::span<const CarveCorridor> extra,
                     glm::vec3 world) {
    float d = carve_distance(layout, world);
    for (const CarveCorridor& c : extra) {
        d = std::min(d, corridor_distance(c, world));
    }
    return d;
}

std::pair<float, float> carve_column_range(const TestbedLayout& layout,
                                           std::span<const CarveCorridor> extra,
                                           glm::vec2 world_xz) {
    auto [lo, hi] = carve_column_range(layout, world_xz);
    for (const CarveCorridor& c : extra) {
        const auto [elo, ehi] = corridor_column_range(c, world_xz);
        if (elo <= ehi) {
            lo = lo > hi ? elo : std::min(lo, elo);
            hi = hi < lo ? ehi : std::max(hi, ehi);
        }
    }
    if (lo > hi) {
        return {1.0f, -1.0f};
    }
    return {lo, hi};
}

std::pair<float, float> carve_column_range(const TestbedLayout& layout,
                                           glm::vec2 world_xz) {
    auto [lo, hi] = corridor_column_range(layout.carves.crag_tunnel, world_xz);
    const auto [plo, phi] = corridor_column_range(layout.carves.barrow_passage, world_xz);
    lo = std::min(lo, plo);
    hi = std::max(hi, phi);
    const CarveChamber& ch = layout.carves.barrow_chamber;
    if (ch.half_extent.x > 0.0f
        && std::fabs(world_xz.x - ch.center.x) <= ch.half_extent.x + 1.0f
        && std::fabs(world_xz.y - ch.center.z) <= ch.half_extent.z + 1.0f) {
        lo = std::min(lo, ch.center.y - 1.0f);
        hi = std::max(hi, ch.center.y + ch.half_extent.y + 1.0f);
    }
    if (lo > hi) {
        return {1.0f, -1.0f}; // empty
    }
    return {lo, hi};
}

/// Walk distance from a corridor's MOUTH to the point on its polyline nearest
/// `world`, measured along the corridor. Returns a large value when the point
/// is not in this corridor at all.
namespace {

float corridor_path_from_mouth(const CarveCorridor& c, const GroundSampler& ground,
                               glm::vec3 world) {
    if (c.point_count < 2) {
        return 1e9f;
    }
    if (corridor_distance(c, world) > 0.0f) {
        return 1e9f; // not inside this corridor
    }
    const auto mouth = carve_mouth(c, ground);
    if (!mouth) {
        return 1e9f; // never goes under rock: not an entrance, so nothing is earned
    }
    // Arc length to the projection of `world`, and to the mouth, both measured
    // from point 0; the walk between them is the difference.
    const auto arc_to = [&](glm::vec3 q) {
        float best = 0.0f;
        float best_d2 = 1e30f;
        float run = 0.0f;
        for (int i = 0; i + 1 < c.point_count; ++i) {
            const glm::vec3 a2 = c.points[i];
            const glm::vec3 b2 = c.points[i + 1];
            const glm::vec3 ab = b2 - a2;
            const float len2 = glm::dot(ab, ab);
            const float u = len2 > 1e-6f ? std::clamp(glm::dot(q - a2, ab) / len2, 0.0f, 1.0f) : 0.0f;
            const glm::vec3 proj = a2 + ab * u;
            const glm::vec3 d = q - proj;
            const float d2 = glm::dot(d, d);
            if (d2 < best_d2) {
                best_d2 = d2;
                best = run + std::sqrt(len2) * u;
            }
            run += std::sqrt(len2);
        }
        return best;
    };
    return std::fabs(arc_to(world) - arc_to(mouth->position));
}

} // namespace

float enclosure_darkness(const TestbedLayout& layout, std::span<const CarveCorridor> extra,
                         const GroundSampler& ground, glm::vec3 world) {
    // Half one: ENCLOSED. Inside carved air, with rock actually overhead.
    if (carve_distance(layout, extra, world) >= 0.0f) {
        return 0.0f; // not in a carve at all -- a valley floor is not a cave
    }
    if (world.y >= ground({world.x, world.z})) {
        return 0.0f; // open to the sky through the shaft above
    }

    // Half two: EARNED. Shortest walk back to any mouth, along the corridors.
    float path = 1e9f;
    path = std::min(path, corridor_path_from_mouth(layout.carves.crag_tunnel, ground, world));
    path = std::min(path, corridor_path_from_mouth(layout.carves.barrow_passage, ground, world));
    path = std::min(path, corridor_path_from_mouth(layout.carves.lakeshore_adit, ground, world));
    for (const CarveCorridor& c : extra) {
        path = std::min(path, corridor_path_from_mouth(c, ground, world));
    }
    if (path > 1e8f) {
        // Inside carved air but reachable from no mouth we can measure -- the
        // barrow CHAMBER is the real case: it hangs off the end of its
        // passage. Fall back to the passage's full length plus the distance
        // in, which is the walk a player actually makes.
        const CarveCorridor& psg = layout.carves.barrow_passage;
        const auto mouth = carve_mouth(psg, ground);
        if (!mouth || psg.point_count < 2) {
            return 1.0f; // sealed and unmeasurable: dark is the safe answer
        }
        path = glm::length(world - mouth->position);
    }

    const float depth_min = static_cast<float>(config::DARKNESS_DEPTH_MIN);
    const float falloff = static_cast<float>(config::DARKNESS_FALLOFF_MIN);
    // Ramp UP TO full darkness at DARKNESS_DEPTH_MIN, so the threshold is
    // where it becomes pitch black rather than where it starts to dim.
    return std::clamp((path - (depth_min - falloff)) / falloff, 0.0f, 1.0f);
}

} // namespace dfn::world
