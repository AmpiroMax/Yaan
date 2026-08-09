/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 11:05:22
Module: engine/world
File: engine/world/sources/WorldgenSites.h

Responsibility:
- Worldgen v2 pass P4 (LANDSCAPE.md §6, §7.3): site placement — the hamlet's
  buildings around its common, shrine, dungeon entrances, tower ruin — each on
  a flattened building pad (BUILDING_PAD_SLOPE_MAX scorer), plus the corridor
  mask helper (§2.4) shared with P5/P6 and validation.

Key items:
- BuildingPad, SitesData, build_sites().
- pads_height(): the pad flatten stamp applied per sample.
- corridor_distance(): distance to the POI-chain corridor centerlines.

Dependencies:
- Uses: TestbedLayout.h, WorldgenHydrology.h, SiteComponents.h, Chunk.h
  (GeneratedEntityRecord), config.
- Used by: Worldgen.cpp, WorldgenScatter, WorldgenValidation, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): placement rng = WorldGenRng stream STREAM_SITES;
  candidate loops have fixed attempt counts and deterministic tie-breaks.
  WorldEntityIds are sequential in (site, building) order — save deltas anchor
  to them (Q56); reordering sites is a worldgen_version bump.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — P4 sites & pads.
*/

#pragma once

#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/SiteComponents.h"
#include "engine/world/sources/TestbedLayout.h"
#include "engine/world/sources/WorldgenHydrology.h"

#include <vector>

namespace dfn::world {

/// One flattened building pad (footprint x 1.5, LANDSCAPE §6): terrain inside
/// `radius` is leveled to `height`, blending back over `blend` meters.
struct BuildingPad {
    glm::vec2 center{0.0f};
    float radius = 0.0f;
    float blend = 0.0f;
    float height = 0.0f;
};

/// P4 output: pads (terrain stamps) + site entity records with their types.
/// entities[i] pairs with types[i]; WorldEntityIds are 1-based sequential.
struct SitesData {
    std::vector<BuildingPad> pads;
    std::vector<GeneratedEntityRecord> entities;
    std::vector<SiteType> types;
};

/// Places all layout sites. Heights are sampled from macro + hydrology carve
/// (pads must not fight the river; buildings keep BUILDING_WATER_MARGIN).
[[nodiscard]] SitesData build_sites(uint64_t seed, const TestbedLayout& layout,
                                    const HydrologyData& hydro);

/// Applies pad flattening to terrain height `h` at `world` (P4 stamp).
[[nodiscard]] float pads_height(const SitesData& sites, glm::vec2 world, float h);

/// Distance (meters) from `world` to the nearest corridor centerline (§2.4).
/// Corridor mask = distance <= CORRIDOR_WIDTH / 2.
[[nodiscard]] float corridor_distance(const TestbedLayout& layout, glm::vec2 world);

} // namespace dfn::world
