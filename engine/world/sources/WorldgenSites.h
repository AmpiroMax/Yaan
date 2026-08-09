/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 17:36:42
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
- 09:08:2026 - 13:12:19: Stage 3b amendments: corridor_distance moved to TestbedLayout.h.
- 09:08:2026 - 15:18:34: Castle: SitesData carries the CastleBuild (its terrace is a separate square stamp with its own cut allowance).
- 09:08:2026 - 17:36:42: §6.2: EntranceWorks (mound + cut forecourt + derived adit) and entrance_works_height.
*/

#pragma once

#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/SiteComponents.h"
#include "engine/world/sources/TestbedLayout.h"
#include "engine/world/sources/WorldgenCastle.h"
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

/// Entrance works for one dungeon (LANDSCAPE §6.2). The generator MAKES THE
/// RELIEF IT NEEDS: on flat ground a chambered-barrow mound plus a cut
/// forecourt restores a hillside to face out of, so the same adit logic then
/// applies inside the mound flank. On ground that already has relief, only the
/// adit is cut. Entrances never use the building-pad scorer — flat and dry is
/// exactly where a cave mouth cannot exist.
struct EntranceWorks {
    bool valid = false;
    bool mounded = false;      ///< false = natural relief was sufficient
    glm::vec2 center{0.0f};    ///< mound centre / site anchor
    glm::vec2 outward{0.0f, 1.0f}; ///< approach direction, portal faces this way
    float mound_radius = 0.0f;
    float mound_height = 0.0f;
    glm::vec2 portal{0.0f};    ///< where the lintel sits, on the flank
    float portal_floor = 0.0f; ///< absolute floor height at the portal
    float forecourt_length = 0.0f;
    float forecourt_half_width = 0.0f;
    CarveCorridor adit{};      ///< derived passage in, mouth at the portal
};

/// P4 output: pads (terrain stamps) + site entity records with their types.
/// entities[i] pairs with types[i]; WorldEntityIds are 1-based sequential.
struct SitesData {
    std::vector<BuildingPad> pads;
    std::vector<GeneratedEntityRecord> entities;
    std::vector<SiteType> types;
    /// The castle (§6.1): its terrace is a separate square stamp with its own
    /// cut allowance, and its elements are appended to `entities`/`types`.
    CastleBuild castle;
    /// One per dungeon entrance (§6.2), in layout site order.
    std::vector<EntranceWorks> entrances;
};

/// Places all layout sites. Heights are sampled from macro + hydrology carve
/// (pads must not fight the river; buildings keep BUILDING_WATER_MARGIN).
[[nodiscard]] SitesData build_sites(uint64_t seed, const TestbedLayout& layout,
                                    const HydrologyData& hydro);

/// Applies the §6.2 entrance works (mound + cut forecourt) to terrain height
/// `h`. Runs BEFORE building pads and AFTER hydrology, so the mound is real
/// ground that scatter, slope and the heightfield all see.
[[nodiscard]] float entrance_works_height(const SitesData& sites, glm::vec2 world, float h);

/// Applies pad flattening to terrain height `h` at `world` (P4 stamp).
/// (corridor_distance moved to TestbedLayout.h — pure layout geometry, now
/// also consumed by hydrology ford beds.)
[[nodiscard]] float pads_height(const SitesData& sites, glm::vec2 world, float h);

} // namespace dfn::world
