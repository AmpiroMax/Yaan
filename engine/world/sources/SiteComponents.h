/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 11:05:22
Module: engine/world
File: engine/world/sources/SiteComponents.h

Responsibility:
- Site-type ECS component attached to worldgen P4 entities (buildings, shrine,
  dungeon entrances, tower ruin) and the placeholder archetype table mapping
  SiteType -> content id string, provisional RenderMesh id and local bounds
  (silhouette boxes per LANDSCAPE.md §6 until real content data files exist).

Key items:
- SiteType, SiteMarker (component, Rule 8 plain data).
- SiteArchetype, site_archetype(): the placeholder archetype table.

Dependencies:
- Uses: glm.
- Used by: WorldgenSites (records), ChunkManager (spawn attachment), gameplay
  (site queries via ECS view), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- PROVISIONAL placeholder mesh ids 1..7: RenderMesh ids are registry-assigned
  dense ids (stage-1 sync decision). The id table here is the worldgen-side
  half of that registry until the lead lands a shared one; render maps the
  same ids to placeholder prisms (communicated 09:08:2026). Do not reuse ids.
- Real content moves to data files (Rule 5) with the archetype/content sync;
  footprint numbers cite LANDSCAPE.md §6 and are design's to tune.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — SiteMarker component + placeholder
  archetype table (§6 silhouettes, provisional mesh ids).
*/

#pragma once

#include "engine/core/serialization/sources/ContentHash.h"

#include <cstdint>
#include <glm/vec3.hpp>
#include <optional>

namespace dfn::world {

/// What a P4 site entity is. Values are stable (serialized via archetype ids,
/// not this enum — but keep append-only anyway).
enum class SiteType : uint8_t {
    Dwelling = 0,
    Trader = 1,
    Tavern = 2,
    Barn = 3,
    Shrine = 4,
    DungeonEntrance = 5,
    TowerRuin = 6,
};

/// ECS component attached to every P4 site entity at chunk spawn. Derived
/// from the entity record's archetype hash (site_type_from_archetype).
struct SiteMarker {
    SiteType type = SiteType::Dwelling;
};

/// Placeholder archetype: content id (hashed into GeneratedEntityRecord),
/// provisional RenderMesh id, and model-space bounds (LANDSCAPE §6 footprint
/// x height; min.y = 0 at the pad surface).
struct SiteArchetype {
    SiteType type;
    const char* content_id;  ///< developer-facing id, hashed via fnv1a64
    uint32_t mesh_id;        ///< provisional dense RenderMesh id (see notice)
    glm::vec3 bounds_min;
    glm::vec3 bounds_max;
};

/// The archetype for a site type. Footprints per LANDSCAPE.md §6.
[[nodiscard]] inline const SiteArchetype& site_archetype(SiteType type) {
    static constexpr SiteArchetype TABLE[] = {
        {SiteType::Dwelling, "site.dwelling", 1, {-3.0f, 0.0f, -4.0f}, {3.0f, 5.5f, 4.0f}},
        {SiteType::Trader, "site.trader", 2, {-4.0f, 0.0f, -5.0f}, {4.0f, 6.5f, 5.0f}},
        {SiteType::Tavern, "site.tavern", 3, {-5.0f, 0.0f, -7.0f}, {5.0f, 8.5f, 7.0f}},
        {SiteType::Barn, "site.barn", 4, {-4.0f, 0.0f, -6.0f}, {4.0f, 7.5f, 6.0f}},
        {SiteType::Shrine, "site.shrine", 5, {-2.5f, 0.0f, -2.5f}, {2.5f, 12.0f, 2.5f}},
        {SiteType::DungeonEntrance, "site.dungeon_entrance", 6, {-2.0f, 0.0f, -2.0f},
         {2.0f, 4.0f, 2.0f}},
        {SiteType::TowerRuin, "site.tower_ruin", 7, {-2.0f, 0.0f, -2.0f}, {2.0f, 12.0f, 2.0f}},
    };
    return TABLE[static_cast<uint8_t>(type)];
}

/// Inverse lookup: archetype content hash -> site type (used when spawning
/// chunk entities from GeneratedEntityRecords). nullopt for non-site
/// archetypes (future content kinds pass through untouched).
[[nodiscard]] inline std::optional<SiteType> site_type_from_archetype(uint64_t archetype) {
    for (uint8_t t = 0; t <= static_cast<uint8_t>(SiteType::TowerRuin); ++t) {
        const SiteArchetype& a = site_archetype(static_cast<SiteType>(t));
        if (serialization::fnv1a64(a.content_id) == archetype) {
            return a.type;
        }
    }
    return std::nullopt;
}

} // namespace dfn::world
