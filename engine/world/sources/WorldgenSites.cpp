/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 18:58:01
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
- 09:08:2026 - 15:18:34: Castle: solved first (its terrace outranks ordinary pads), elements appended to the shared record list keeping WorldEntityIds sequential; pads_height applies the terrace + ramp.
- 09:08:2026 - 17:36:42: §6.2: entrances no longer use the pad scorer at all — relief within 25 m selects adit vs sunken barrow, the generator stamps the mound/forecourt it needs on flat ground, and the marker is derived from the mouth with an explicit floor height. Hand-authored carves outrank generated stubs.
- 09:08:2026 - 18:58:01: Live-play fixes: (a) the forecourt now runs from the flank lintel PAST the mound rim to natural grade — it previously ended while still on the mound, so the rim walled off the exit and the barrow read as 'facing into rock, as if there is no entrance'; (b) the mound is a paraboloid DOME instead of a smoothstep plateau-with-rim ('crooked, just a square'); (c) the cut flares outward so it reads as a way in rather than a slot.
*/

#include "engine/world/sources/WorldgenSites.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/world/sources/WorldgenCarve.h"
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

    void emit(SiteType type, glm::vec2 pos, float yaw, bool with_pad = true,
              float ground_y = NO_GROUND_Y) {
        const SiteArchetype& a = site_archetype(type);
        if (with_pad) {
            const float r = pad_radius(a);
            const float h = ground_height(seed, layout, hydro, pos);
            out.pads.push_back(BuildingPad{pos, r, r * 0.5f, h});
        }
        out.entities.push_back(GeneratedEntityRecord{
            next_id++, serialization::fnv1a64(a.content_id), pos, yaw, ground_y});
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

/// §6.2 selection rule: the relief available within 25 m decides the archetype.
/// Returns (best rise, direction of the DOWNhill side, i.e. where a portal
/// would face out).
std::pair<float, glm::vec2> local_relief(uint64_t seed, const TestbedLayout& layout,
                                         const HydrologyData& hydro, glm::vec2 c) {
    const float h0 = ground_height(seed, layout, hydro, c);
    float best_rise = 0.0f;
    glm::vec2 uphill{1.0f, 0.0f};
    for (int i = 0; i < 8; ++i) {
        const float ang = static_cast<float>(i) * (TAU / 8.0f);
        const glm::vec2 d{std::cos(ang), std::sin(ang)};
        const float rise = ground_height(seed, layout, hydro, c + d * 25.0f) - h0;
        if (rise > best_rise) {
            best_rise = rise;
            uphill = d;
        }
    }
    return {best_rise, uphill};
}

EntranceWorks build_entrance_works(uint64_t seed, const TestbedLayout& layout,
                                   const HydrologyData& hydro, glm::vec2 site) {
    constexpr float MIN_RELIEF = static_cast<float>(config::DUNGEON_ENTRANCE_MIN_RELIEF);
    constexpr float MOUND_R = static_cast<float>(config::BARROW_MOUND_RADIUS);
    constexpr float MOUND_H = static_cast<float>(config::BARROW_MOUND_HEIGHT);
    constexpr float FC_LEN = static_cast<float>(config::BARROW_FORECOURT_LENGTH);
    constexpr float FC_HALF = static_cast<float>(config::BARROW_FORECOURT_WIDTH) * 0.5f;
    constexpr float FC_DEPTH = static_cast<float>(config::BARROW_FORECOURT_DEPTH);

    EntranceWorks w;
    w.valid = true;
    w.center = site;
    const auto [rise, uphill] = local_relief(seed, layout, hydro, site);
    // The portal faces DOWNhill — out of the slope, toward the approach.
    w.outward = -uphill;
    w.mound_radius = MOUND_R;
    w.forecourt_length = FC_LEN;
    w.forecourt_half_width = FC_HALF;

    const float grade = ground_height(seed, layout, hydro, site);
    if (rise >= MIN_RELIEF) {
        // Natural hillside: cut the adit straight into the slope, mouth on the
        // slope normal. No mound — the relief is already there.
        w.mounded = false;
        w.mound_height = 0.0f;
        // Step out to where the ground has dropped, and put the portal there.
        w.portal = site + w.outward * 10.0f;
        w.portal_floor = ground_height(seed, layout, hydro, w.portal) - 0.6f;
    } else {
        // Flat ground: BUILD the relief (§6.2). A mound gives the silhouette a
        // hole in flat ground can never have, and a cut forecourt walks the
        // player down to a lintel in its flank.
        w.mounded = true;
        w.mound_height = MOUND_H;
        // The lintel sits partway up the FLANK (§6.2: "walks the player down
        // to a lintel in its flank"), so the doorway is a dark face in the
        // dome and reads as an entrance from outside. The bug was never the
        // lintel's position — it was that the cut ran only
        // BARROW_FORECOURT_LENGTH and so ended while still ON the mound,
        // leaving the way out blocked by the rim ("faces into rock, as if
        // there is no entrance"). The cut must reach natural grade BEYOND the
        // rim, so its length is the distance still to travel over the mound
        // PLUS the ruled length of open approach.
        const float portal_offset = MOUND_R * 0.45f;
        w.portal = site + w.outward * portal_offset;
        w.portal_floor = grade - FC_DEPTH;
        w.forecourt_length = (MOUND_R - portal_offset) + FC_LEN;
    }
    // The adit: from just outside the portal, in under the mound/hillside.
    const glm::vec3 mouth_outer{w.portal.x + w.outward.x * 2.0f, w.portal_floor,
                                w.portal.y + w.outward.y * 2.0f};
    const glm::vec3 inner{w.portal.x - w.outward.x * 18.0f, w.portal_floor,
                          w.portal.y - w.outward.y * 18.0f};
    w.adit.points[0] = mouth_outer;
    w.adit.points[1] = inner;
    w.adit.point_count = 2;
    w.adit.half_width = 1.5f;
    w.adit.height = 2.6f;
    return w;
}

} // namespace

SitesData build_sites(uint64_t seed, const TestbedLayout& layout, const HydrologyData& hydro) {
    SitesData out;
    // The castle terrace is solved FIRST: its pad is large and its elements
    // must not be shoved around by ordinary building placement (§6.1).
    out.castle = solve_castle(seed, layout, hydro);
    Placer placer{seed, layout, hydro, out};
    WorldGenRng rng = WorldGenRng::for_chunk(seed, ChunkCoord{0, 0}, STREAM_SITES);

    // §6.2 entrance works first: the mound is terrain, and the marker and adit
    // are derived from it.
    out.entrances.assign(std::size(layout.sites), EntranceWorks{});
    for (std::size_t si = 0; si < std::size(layout.sites); ++si) {
        if (layout.sites[si].kind != SiteKind::DungeonEntrance) {
            continue;
        }
        const auto hand_carved = site_carve_mouth(
            layout, static_cast<int>(si),
            [&](glm::vec2 p) { return ground_height(seed, layout, hydro, p); });
        if (hand_carved) {
            continue; // designed route; no generated works
        }
        out.entrances[si] =
            build_entrance_works(seed, layout, hydro, layout.sites[si].position);
    }

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
            // DERIVED, NOT SCORED: if this entrance has a carve, its marker
            // belongs at the carve's mouth facing out — the pad scorer knows
            // about slope and dryness but nothing about "a mouth needs a
            // hillside to face out of", and left one marker 10 m outside its
            // own passage and another on the crown of the bluff it should sit
            // under. Same rule as fords being derived from the generated trace.
            const auto ground = [&](glm::vec2 p) {
                return ground_height(seed, layout, hydro, p);
            };
            // §6.2: entrances never use the pad scorer. Prefer the works adit
            // (which exists for every dungeon), then any hand-authored carve.
            // A hand-authored carve (the Backbarrow passage) is a DESIGNED
            // route and always wins over a generated stub.
            if (const auto mouth = site_carve_mouth(layout, si, ground)) {
                const glm::vec2 pos =
                    glm::vec2{mouth->position.x, mouth->position.z} + mouth->outward * 1.5f;
                placer.emit(SiteType::DungeonEntrance, pos,
                            std::atan2(mouth->outward.x, -mouth->outward.y), false,
                            mouth->position.y);
                break;
            }
            const EntranceWorks& works = out.entrances[si];
            if (works.valid) {
                placer.emit(SiteType::DungeonEntrance, works.portal,
                            std::atan2(works.outward.x, -works.outward.y), false,
                            works.portal_floor);
                break;
            }
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

    // Castle elements enter the shared record list last, so their
    // WorldEntityIds continue the deterministic sequence (Q56 anchor).
    for (std::size_t i = 0; i < out.castle.entities.size(); ++i) {
        GeneratedEntityRecord rec = out.castle.entities[i];
        rec.world_id = static_cast<WorldEntityId>(out.entities.size() + 1);
        out.castle.entities[i].world_id = rec.world_id;
        out.entities.push_back(rec);
        out.types.push_back(out.castle.types[i]);
    }
    return out;
}

float entrance_works_height(const SitesData& sites, glm::vec2 world, float h) {
    for (const EntranceWorks& w : sites.entrances) {
        if (!w.valid) {
            continue;
        }
        if (w.mounded) {
            // DOME, not a plateau. smoothstep flattens the top and stands the
            // rim up like a wall, which is what read as "crooked, just a
            // square". A paraboloid falls continuously from the crown to zero
            // slope at the rim: round from every angle, no flat top, no step.
            const float d = glm::length(world - w.center);
            if (d < w.mound_radius) {
                const float t = d / w.mound_radius;
                h += w.mound_height * (1.0f - t * t);
            }
        }
        // Cut forecourt: a trench running out from the portal, deepest at the
        // lintel and ramping up to grade at its outer end, so the player walks
        // DOWN into it and never falls in.
        const glm::vec2 rel = world - w.portal;
        const float along = glm::dot(rel, w.outward);
        const float across = std::fabs(rel.x * w.outward.y - rel.y * w.outward.x);
        // The cut FLARES outward — narrow at the lintel, wider at the mouth of
        // the approach. A constant-width slot is what made the works read as a
        // box cut into the hill rather than a way in.
        const float flare = w.forecourt_half_width
                          * (1.0f + std::clamp(along / w.forecourt_length, 0.0f, 1.0f));
        if (along >= -2.0f && along <= w.forecourt_length && across <= flare) {
            const float t = std::clamp(along / w.forecourt_length, 0.0f, 1.0f);
            const float floor_h = w.portal_floor + (h - w.portal_floor) * t;
            h = std::min(h, floor_h);
        }
    }
    return h;
}

float pads_height(const SitesData& sites, glm::vec2 world, float h) {
    // The castle terrace first: ordinary pads are small and never overlap it,
    // but if one ever did, the building pad should win locally.
    h = castle_pad_height(sites.castle, world, h);
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
