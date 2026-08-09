/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 13:12:19
Module: engine/world
File: engine/world/sources/WorldgenSites.cpp

Responsibility:
- P4 implementation: hamlet building ring around the common (tavern faces the
  lake, trader at the corridor entry — §6/§7.1), pad slope scorer, shrine /
  dungeon / tower placement, corridor distance.

Key items:
- build_sites, pads_height, corridor_distance.

Dependencies:
- Uses: WorldgenSites.h, WorldgenMacro.h, Worldgen.h (WorldGenRng), config,
  core/serialization ContentHash (archetype ids).
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): fixed attempt counts, deterministic fallbacks,
  sequential WorldEntityIds in placement order (Q56 anchor).
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — P4 implementation.
- 09:08:2026 - 13:12:19: Stage 3b amendments: corridor_distance moved to TestbedLayout.h; placer fallback scores dry-over-flat-over-wet and shies from banks.
*/

#include "engine/world/sources/WorldgenSites.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenNoise.h"
#include "engine/world/sources/Worldgen.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>

namespace dfn::world {

namespace {

constexpr float PAD_SLOPE_MAX = static_cast<float>(config::BUILDING_PAD_SLOPE_MAX);
constexpr float WATER_MARGIN = static_cast<float>(config::BUILDING_WATER_MARGIN);
constexpr float TAU = 6.28318530717958647692f;

/// Carved terrain height (macro + hydrology, no pads — pads are being placed).
float ground_height(uint64_t seed, const TestbedLayout& layout, const HydrologyData& hydro,
                    glm::vec2 p) {
    return water_at(hydro, layout, p, macro_height(seed, layout, p)).height;
}

/// Terrain slope angle (radians) from central differences at +-2 m.
float ground_slope(uint64_t seed, const TestbedLayout& layout, const HydrologyData& hydro,
                   glm::vec2 p) {
    const float d = 2.0f;
    const float hx = ground_height(seed, layout, hydro, {p.x + d, p.y})
                   - ground_height(seed, layout, hydro, {p.x - d, p.y});
    const float hz = ground_height(seed, layout, hydro, {p.x, p.y + d})
                   - ground_height(seed, layout, hydro, {p.x, p.y - d});
    return std::atan(std::sqrt(hx * hx + hz * hz) / (2.0f * d));
}

/// Pad radius for an archetype: half of (max footprint side x 1.5) (§6).
float pad_radius(const SiteArchetype& a) {
    const float w = a.bounds_max.x - a.bounds_min.x;
    const float l = a.bounds_max.z - a.bounds_min.z;
    return 0.75f * std::max(w, l);
}

struct Placer {
    uint64_t seed;
    const TestbedLayout& layout;
    const HydrologyData& hydro;
    SitesData& out;
    WorldEntityId next_id = 1;

    void emit(SiteType type, glm::vec2 pos, float yaw) {
        const SiteArchetype& a = site_archetype(type);
        const float r = pad_radius(a);
        const float h = ground_height(seed, layout, hydro, pos);
        out.pads.push_back(BuildingPad{pos, r, r * 0.5f, h});
        out.entities.push_back(GeneratedEntityRecord{
            next_id++, serialization::fnv1a64(a.content_id), pos, yaw});
        out.types.push_back(type);
    }

    /// Picks a position near `want` satisfying the pad slope + flood margin;
    /// falls back to the best-scored candidate (dry beats flat beats wet —
    /// deterministic).
    glm::vec2 place(WorldGenRng& rng, glm::vec2 want, float jitter_radius) {
        glm::vec2 best = want;
        float best_score = std::numeric_limits<float>::max();
        for (int attempt = 0; attempt < 6; ++attempt) {
            glm::vec2 cand = want;
            if (attempt > 0) {
                const float ang = rng.next_float01() * TAU;
                const float rad = rng.next_float01() * jitter_radius;
                cand += glm::vec2{std::cos(ang), std::sin(ang)} * rad;
            }
            const float slope = ground_slope(seed, layout, hydro, cand);
            const WaterSample ws = water_at(hydro, layout, cand, macro_height(seed, layout, cand));
            // Flood margin (§6) is relative to the NEAREST water body: a dry
            // hollow far from any water is legal ground.
            const bool dry = ws.water_surface == math::NO_WATER
                          && (ws.near_level == math::NO_WATER
                              || ws.dist_to_water > 8.0f * WATER_MARGIN
                              || ws.height >= ws.near_level + WATER_MARGIN - 0.001f);
            if (slope <= PAD_SLOPE_MAX && dry) {
                return cand;
            }
            const float score = (dry ? 0.0f : 1000.0f) + slope
                              + std::max(0.0f, 4.0f - ws.dist_to_water); // shy of banks
            if (score < best_score) {
                best_score = score;
                best = cand;
            }
        }
        return best;
    }
};

} // namespace

SitesData build_sites(uint64_t seed, const TestbedLayout& layout, const HydrologyData& hydro) {
    SitesData out;
    Placer placer{seed, layout, hydro, out};
    WorldGenRng rng = WorldGenRng::for_chunk(seed, ChunkCoord{0, 0}, STREAM_SITES);

    for (uint8_t si = 0; si < static_cast<uint8_t>(std::size(layout.sites)); ++si) {
        const SiteLayout& site = layout.sites[si];
        switch (site.kind) {
        case SiteKind::Hamlet: {
            // Hamlet ring (§6): 1 tavern + 1 trader + 3-5 dwellings + 1-2 barns
            // around a common; total clamped to HAMLET_SIZE_MAX.
            const float common_r =
                static_cast<float>(config::HAMLET_COMMON_RADIUS_MIN)
                + rng.next_float01()
                      * static_cast<float>(config::HAMLET_COMMON_RADIUS_MAX
                                           - config::HAMLET_COMMON_RADIUS_MIN);
            uint32_t dwellings = rng.next_range(3, 5);
            uint32_t barns = rng.next_range(1, 2);
            while (2 + dwellings + barns > static_cast<uint32_t>(config::HAMLET_SIZE_MAX)) {
                if (barns > 1) {
                    --barns;
                } else {
                    --dwellings;
                }
            }
            // Build the ordered type list: tavern at the common's head (away
            // from the lake so its front faces the water across the common),
            // trader toward the corridor entry (first corridor = to shrine).
            std::vector<SiteType> ring{SiteType::Tavern, SiteType::Trader};
            for (uint32_t i = 0; i < dwellings; ++i) ring.push_back(SiteType::Dwelling);
            for (uint32_t i = 0; i < barns; ++i) ring.push_back(SiteType::Barn);

            const glm::vec2 to_lake = glm::normalize(layout.lake.center - site.position);
            const glm::vec2 to_shrine = glm::normalize(layout.sites[1].position - site.position);
            const float tavern_angle = std::atan2(-to_lake.y, -to_lake.x);
            const float trader_angle = std::atan2(to_shrine.y, to_shrine.x);

            for (std::size_t b = 0; b < ring.size(); ++b) {
                float angle;
                if (b == 0) {
                    angle = tavern_angle;
                } else if (b == 1) {
                    angle = trader_angle;
                } else {
                    // Remaining slots spread evenly, jittered, skipping the
                    // tavern's slot so the head of the common stays its own.
                    const float t = static_cast<float>(b - 1) / static_cast<float>(ring.size() - 1);
                    angle = tavern_angle + TAU * t + (rng.next_float01() - 0.5f) * 0.4f;
                }
                const SiteArchetype& a = site_archetype(ring[b]);
                const float ring_r = common_r + pad_radius(a) + 2.0f + rng.next_float01() * 3.0f;
                const glm::vec2 want =
                    site.position + glm::vec2{std::cos(angle), std::sin(angle)} * ring_r;
                const glm::vec2 pos = placer.place(rng, want, 6.0f);
                // Face the common center, +-30 deg (§6).
                const glm::vec2 to_common = site.position - pos;
                const float face = std::atan2(to_common.x, -to_common.y) // yaw 0 = -Z
                                 + (rng.next_float01() * 2.0f - 1.0f) * 0.52f;
                placer.emit(ring[b], pos, face);
            }
            break;
        }
        case SiteKind::Shrine: {
            const glm::vec2 pos = placer.place(rng, site.position, 8.0f);
            const glm::vec2 to_town = layout.sites[0].position - pos;
            placer.emit(SiteType::Shrine, pos, std::atan2(to_town.x, -to_town.y));
            break;
        }
        case SiteKind::DungeonEntrance: {
            const glm::vec2 pos = placer.place(rng, site.position, 10.0f);
            // Face away from the crag (barrow: south face) or toward the town.
            const glm::vec2 away = pos - layout.crag.center;
            const glm::vec2 dir = glm::length(away) < layout.crag.radius
                                    ? away
                                    : layout.sites[0].position - pos;
            placer.emit(SiteType::DungeonEntrance, pos, std::atan2(dir.x, -dir.y));
            break;
        }
        case SiteKind::TowerRuin: {
            // Exactly on the crag peak (§7.1) — no slope scoring, the pad
            // flattens the summit.
            placer.emit(SiteType::TowerRuin, site.position, rng.next_float01() * TAU);
            break;
        }
        }
    }
    return out;
}

float pads_height(const SitesData& sites, glm::vec2 world, float h) {
    for (const BuildingPad& pad : sites.pads) {
        const float d = glm::length(world - pad.center);
        if (d >= pad.radius + pad.blend) {
            continue;
        }
        if (d <= pad.radius) {
            h = pad.height;
        } else {
            const float s = noise::smoothstep01((d - pad.radius) / pad.blend);
            h = pad.height + (h - pad.height) * s;
        }
    }
    return h;
}

} // namespace dfn::world
