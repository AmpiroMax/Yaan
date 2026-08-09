/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 15:18:34
Module: engine/world
File: engine/world/sources/WorldgenValidation.h

Responsibility:
- Worldgen v2 validation passes (LANDSCAPE.md executable checks): the river
  monotonic-water invariant (§3.1.4), the C1 landmark-visibility raycast
  (LANDMARK_VISIBILITY_MIN) and the §2.4 corridor slope limit. Pure queries
  over a WorldGenContext — tests fail seeds/layouts through these.

Key items:
- river_is_monotonic, landmark_visibility_fraction, max_corridor_avg_slope.

Dependencies:
- Uses: Worldgen.h (context), config.
- Used by: worldgen tests; later the offline worldgen tool (failed-seed gate).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- These are the DESIGN CONTRACT gates: weakening a threshold here without a
  design/lead sync is a violation (constants come from dfn::config).
- Raycasts test terrain occlusion only (trees are readability fabric, not
  walls — documented v2 simplification).
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — validation passes for tests.
- 09:08:2026 - 13:12:19: Stage 3b amendments: canopy-aware clearance semantics documented; max_corridor_water_depth (C3 vs generated water).
- 09:08:2026 - 15:18:34: Castle validation: CastleHierarchy (R3 top/ceiling, R4 ratio, R2 crown, C2 attractors with and without the castle) and CastleAccess (ramp slope/step, Backbarrow sightline from yard and gate).
*/

#pragma once

#include "engine/world/sources/Worldgen.h"

namespace dfn::world {

/// §3.1.4: true iff every river station's water surface is <= its upstream
/// neighbor's (a climbing river = failed generation) AND hydrology built ok.
[[nodiscard]] bool river_is_monotonic(const HydrologyData& hydro);

/// C1 / LANDMARK_VISIBILITY_MIN: fraction of open walkable standpoints (grid
/// sampled, eye height, water/forest/crag/steep excluded) from which the L0
/// crag tower top is visible over the OCCLUSION heightfield — terrain PLUS
/// canopy (§1.1 amendment) — with the C4 clearance factor: the L0's subtended
/// angle must exceed every intervening occluder by LANDMARK_CLEARANCE_FACTOR.
[[nodiscard]] float landmark_visibility_fraction(const WorldGenContext& ctx);

/// §2.4: the worst corridor's average along-path slope (radians). Must stay
/// under CORRIDOR_SLOPE_MAX.
[[nodiscard]] float max_corridor_avg_slope(const WorldGenContext& ctx);

/// C3 vs GENERATED water (§7.1a/§7.2 amendment): the worst water depth (m)
/// where any POI-chain corridor crosses generated water. Rivers are crossed
/// only at fords, so this must stay <= FORD_DEPTH_MAX; a deeper crossing
/// means the chain is severed (failed generation for the layout).
[[nodiscard]] float max_corridor_water_depth(const WorldGenContext& ctx);

/// Castle hierarchy checks (LANDSCAPE §6.1.1), all measured from the same
/// standpoint grid C1 uses.
struct CastleHierarchy {
    float top_elevation = 0.0f;    ///< pad + tallest element, meters (R3)
    float skyline_ceiling = 0.0f;  ///< L0 peak - CASTLE_SKYLINE_MARGIN (R3 limit)
    float max_ratio = 0.0f;        ///< worst castle/crag subtended height, >= 300 m (R4)
    bool crown_occluded = false;   ///< castle hides the L0's top third anywhere (R2)
    uint32_t max_attractors = 0;   ///< most attractors visible from one standpoint
    /// Same measure with the castle removed as attractor AND occluder. The
    /// difference is the castle's own contribution — design's "check both
    /// directions" (§6.1.1). The ABSOLUTE bound POI_VISIBLE_COUNT_MAX_REGION
    /// is region-scale only: it is unsatisfiable on the testbed, where C3
    /// packs POIs at ~3x region density and holding <= 3 would push C1 under
    /// its own floor (design ruling; C1 wins). On the testbed the binding
    /// rule is `max_coequal_visible` below; this pair stays as the castle's
    /// contribution gate.
    uint32_t max_attractors_without_castle = 0;
    /// RULE C2-TESTBED ("no coequal crowd"): the most attractors of COMPARABLE
    /// apparent size visible from one standpoint — comparable meaning their
    /// subtended heights lie within COEQUAL_ANGLE_RATIO of each other. The L0
    /// is EXEMPT (C1 mandates its ubiquity) and composite POIs count once.
    /// Must stay <= POI_COEQUAL_VISIBLE_MAX.
    uint32_t max_coequal_visible = 0;
};
[[nodiscard]] CastleHierarchy castle_hierarchy(const WorldGenContext& ctx);

/// The two story-mandated castle terrain invariants (§6.1.2), guarded on every
/// run exactly like the L0 sightlines — a later pine retune, scatter change or
/// terrace edit can break them, so they are re-validated, not authored once.
struct CastleAccess {
    /// ACCESS INVARIANT: the graded approach ramp, corridor foot -> gate
    /// threshold, must meet the §2.4 corridor rules end to end.
    float ramp_avg_slope = 0.0f; ///< radians; <= CORRIDOR_SLOPE_MAX
    float ramp_max_step = 0.0f;  ///< meters between adjacent samples; <= PLAYER_STEP_HEIGHT
    /// BARROW SIGHTLINE: the Backbarrow entrance must be visible from both the
    /// yard and the gate over terrain + canopy (the castle's own walls are not
    /// occluders here — the ruling is about the terrace's cut/fill and later
    /// passes, §6.1.2).
    bool barrow_visible_from_yard = false;
    bool barrow_visible_from_gate = false;
};
[[nodiscard]] CastleAccess castle_access(const WorldGenContext& ctx);

} // namespace dfn::world
