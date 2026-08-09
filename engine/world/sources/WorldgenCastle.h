/*
Created: 09:08:2026 - 15:20:00
Last updated: 09:08:2026 - 15:20:00
Module: engine/world
File: engine/world/sources/WorldgenCastle.h

Responsibility:
- The castle pass (LANDSCAPE.md §6.1 ruling, hall-castle revision):
  "Harrowward", House Corvane's gentry hall on the crag's SW foot — terraced
  spur pad with a graded approach RAMP (the access invariant), the minimal
  mass (curtain wall + gatehouse + hall + solar around an open yard), and the
  castle's contribution to the C1 occlusion heightfield.

Key items:
- CastleBuild: solved pad, ramp, element heights and placed records.
- solve_castle(): sites the pad/ramp and solves heights against R3.
- castle_pad_height(): terrace + ramp stamp, applied in the height pipeline.
- castle_occluder_height(): castle mass for the canopy-style C1 raycast.
- castle_yard_point() / castle_gate_point(): the two standpoints the
  yard->Backbarrow sightline invariant is measured from.

Dependencies:
- Uses: TestbedLayout.h, SiteComponents.h, Chunk.h, WorldgenHydrology.h, config.
- Used by: WorldgenSites (P4), WorldgenValidation (C1/R2/R3/R4, ramp,
  sightline), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- R3 IS BINDING AND OUTRANKS THE HEIGHT TABLE (§6.1.1): pad elevation plus the
  tallest element (the solar) must stay CASTLE_SKYLINE_MARGIN below the L0
  peak. Heights shrink to fit; the crag is NEVER raised and a C1 drop is never
  accepted (fix order: lower pad -> shift pad south -> cut height LAST).
- Horizontal-dominant mass (§6.1.3): a long hall with ONE modest vertical. Do
  not reintroduce a keep or corner towers — that ruling is settled.
- The ACCESS INVARIANT is terrain, not decoration: the ramp is cut by the same
  stamp as the terrace, and a pad reachable only by a scarp is a FAILED
  placement. The gate faces the valley/ford and the pad is never rotated
  (settled, do not re-litigate).
- The castle never creates or moves a ford (fords stay derived, §7.1a).
*/
/*
UPD:
- 09:08:2026 - 15:20:00: Created — castle pass per design's §6.1 ruling
  (hall-castle revision: hall/solar mass, access ramp, barrow sightline).
*/

#pragma once

#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/SiteComponents.h"
#include "engine/world/sources/TestbedLayout.h"
#include "engine/world/sources/WorldgenHydrology.h"

#include <vector>

namespace dfn::world {

/// The solved castle: pad + ramp geometry, the heights actually built (post
/// R3), and the placed element records. `valid` is false when none is built.
/// One terraced ward. The fortress is a CHAIN of these stepping down the spur,
/// not one slab: a single 120 m terrace cannot hold a 6 m cut budget on a
/// hillside, and it also swallowed the Backbarrow carve 54 m away. Ward 0 is
/// the uphill, OLDEST ward nearest the barrow (story: the Corvanes fortified
/// out of fear of what they buried — which is also the order the cut budget
/// wants, arrived at independently).
struct CastleWard {
    glm::vec2 center{0.0f};
    float half_size = 0.0f;
    float blend = 0.0f;
    float height = 0.0f; ///< terrace surface elevation
    float cut = 0.0f;
    float fill = 0.0f;
};

struct CastleBuild {
    bool valid = false;
    glm::vec2 center{0.0f};
    float half_size = 0.0f;  ///< half the ward chain's overall span
    float blend = 0.0f;
    float pad_height = 0.0f; ///< ward 0's surface: the reference elevation
    float cut = 0.0f;        ///< WORST cut across all wards (<= CASTLE_PAD_CUT_MAX)
    float fill = 0.0f;       ///< worst fill across all wards
    CastleWard wards[3]{};
    int ward_count = 0;
    float gate_yaw = 0.0f;   ///< radians; gate faces the valley/ford approach

    /// Approach ramp (access invariant): a graded band on the gate side,
    /// running from the pad edge out to `ramp_length`, half as wide as
    /// CORRIDOR_WIDTH. Its surface interpolates linearly from the pad to
    /// natural grade, so it carries no step and a constant, gentle slope.
    glm::vec2 gate_dir{0.0f, 1.0f}; ///< unit, pad center -> gate -> approach
    float ramp_length = 0.0f;
    float ramp_half_width = 0.0f;

    float hall_height = 0.0f;
    float solar_height = 0.0f;
    float wall_height = 0.0f;
    float gate_height = 0.0f;

    /// Highest point of the built mass, absolute meters. The solar stands on
    /// ward 0, which is the highest terrace.
    [[nodiscard]] float top_elevation() const {
        return (ward_count > 0 ? wards[0].height : pad_height) + solar_height;
    }

    std::vector<GeneratedEntityRecord> entities; ///< world_ids assigned by P4
    std::vector<SiteType> types;                 ///< parallel to entities
};

/// Sites the castle, cuts its ramp and solves its heights against R3. Terrain
/// is sampled WITHOUT pads (macro + hydrology carve), so this runs before
/// P4's pad stamps exist. Deterministic and side-effect free.
[[nodiscard]] CastleBuild solve_castle(uint64_t seed, const TestbedLayout& layout,
                                       const HydrologyData& hydro);

/// Terrace + ramp stamp: flattens the pad, blends its skirt, and grades the
/// approach ramp. Applied where ordinary building pads are applied.
[[nodiscard]] float castle_pad_height(const CastleBuild& castle, glm::vec2 world, float h);

/// Height of the castle mass above the pad at `world` (0 outside the mass) —
/// the castle's entry into the C1 occlusion heightfield, exactly like canopy.
[[nodiscard]] float castle_occluder_height(const CastleBuild& castle, glm::vec2 world);

/// Yard centre (the open tithe-yard) — a sightline standpoint (§6.1.2).
[[nodiscard]] glm::vec2 castle_yard_point(const CastleBuild& castle);

/// Gate threshold, just inside the gatehouse — the other sightline standpoint
/// and the top of the approach ramp.
[[nodiscard]] glm::vec2 castle_gate_point(const CastleBuild& castle);

/// Outer foot of the approach ramp, where it meets natural grade.
[[nodiscard]] glm::vec2 castle_ramp_foot(const CastleBuild& castle);

} // namespace dfn::world
