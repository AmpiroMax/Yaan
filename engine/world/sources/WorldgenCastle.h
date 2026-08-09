/*
Created: 09:08:2026 - 15:20:00
Last updated: 09:08:2026 - 15:20:00
Module: engine/world
File: engine/world/sources/WorldgenCastle.h

Responsibility:
- The castle pass (LANDSCAPE.md §6.1 ruling): House Corvane's seat on the
  crag's SW foot — terraced spur pad (a documented BUILDING_PAD_SLOPE_MAX
  exception for the CUT, not for the pad surface), the minimal mass (keep +
  curtain wall + gatehouse + 2 corner towers), and the castle's contribution
  to the C1 occlusion heightfield.

Key items:
- CastleBuild: solved pad + element heights + placed records.
- solve_castle(): sites the pad and heights against R3 (skyline margin).
- castle_pad_height(): the terrace stamp applied inside the height pipeline.
- castle_occluder_height(): castle mass for the canopy-style C1 raycast.

Dependencies:
- Uses: TestbedLayout.h, SiteComponents.h, Chunk.h, config.
- Used by: WorldgenSites (P4), WorldgenValidation (C1/R2/R3/R4), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- R3 IS BINDING AND OUTRANKS THE HEIGHT TABLE (§6.1.1): pad elevation plus the
  tallest element must stay CASTLE_SKYLINE_MARGIN below the L0 peak. Heights
  are reduced to fit; the crag is NEVER raised and a C1 drop is never accepted
  (fix order: lower pad -> shift pad south -> cut tower height LAST).
- The castle never creates or moves a ford (fords stay derived, §7.1a); it
  only commands the nearest existing one.
*/
/*
UPD:
- 09:08:2026 - 15:20:00: Created — castle pass per design's §6.1 ruling.
*/

#pragma once

#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/SiteComponents.h"
#include "engine/world/sources/TestbedLayout.h"
#include "engine/world/sources/WorldgenHydrology.h"

#include <vector>

namespace dfn::world {

/// The solved castle: pad geometry, the heights actually built (post-R3), and
/// the placed element records. `valid` is false when no castle is configured.
struct CastleBuild {
    bool valid = false;
    glm::vec2 center{0.0f};
    float half_size = 0.0f;  ///< CASTLE_PAD_SIZE / 2
    float blend = 0.0f;      ///< terrace skirt width (pad edges blend 1.5x pad)
    float pad_height = 0.0f; ///< terrace surface elevation, meters
    float cut = 0.0f;        ///< max material removed above the pad (<= CASTLE_PAD_CUT_MAX)
    float fill = 0.0f;       ///< max material added below the pad
    float gate_yaw = 0.0f;   ///< radians; gate faces the approach corridor

    float keep_height = 0.0f;
    float wall_height = 0.0f;
    float tower_height = 0.0f;
    float gate_height = 0.0f;

    /// Highest point of the built mass, absolute meters (pad + tallest).
    [[nodiscard]] float top_elevation() const { return pad_height + keep_height; }

    std::vector<GeneratedEntityRecord> entities; ///< world_ids assigned by P4
    std::vector<SiteType> types;                 ///< parallel to entities
};

/// Sites the castle and solves its heights against R3. Terrain is sampled
/// WITHOUT pads (macro + hydrology carve), so this runs before P4's pad
/// stamps exist. Deterministic and side-effect free.
[[nodiscard]] CastleBuild solve_castle(uint64_t seed, const TestbedLayout& layout,
                                       const HydrologyData& hydro);

/// Terrace stamp: flattens the pad and blends its skirt. Applied in the same
/// place as ordinary building pads.
[[nodiscard]] float castle_pad_height(const CastleBuild& castle, glm::vec2 world, float h);

/// Height of the castle mass above the pad at `world` (0 outside the mass) —
/// the castle's entry into the C1 occlusion heightfield, exactly like canopy.
[[nodiscard]] float castle_occluder_height(const CastleBuild& castle, glm::vec2 world);

} // namespace dfn::world
