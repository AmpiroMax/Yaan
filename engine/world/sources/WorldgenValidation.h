/*
Created: 09:08:2026 - 11:05:22
Last updated: 11:08:2026 - 15:15:55
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
- 09:08:2026 - 15:31:04: Rule C2-testbed: max_coequal_visible (+ raw and without-castle variants) replacing the absolute bound on the testbed path; POI_VISIBLE_COUNT_MAX renamed to POI_VISIBLE_COUNT_MAX_REGION (region-scale only, design ruling).
- 09:08:2026 - 15:36:59: Large-mass guard: max_coequal_large — the widest coequal group whose every member subtends >= COEQUAL_LARGE_PX, held to 2 while POI_COEQUAL_VISIBLE_MAX (now 3) governs threshold-scale groups.
- 11:08:2026 - 15:15:55: ground_relief_20m / relief_floor_binds / flattest_legal_standpoints. NOTE: GROUND_RELIEF_SIGMA_20M_MIN was withdrawn the same day (NUMBERS 0825317) -- sigma survives here as the trend-ranking machinery, not as a contract quantity.
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
    /// Must stay <= POI_COEQUAL_VISIBLE_MAX. Attractors that read against the
    /// L0's BODY rather than against sky are not counted: that is R1's own
    /// mechanism (§6.1.1 — "a silhouette that cannot reach the horizon cannot
    /// steal it"), applied per standpoint. `max_coequal_visible_raw` below is
    /// the same measure WITHOUT that exemption, so the raw crowd stays
    /// visible to design rather than being silently absorbed.
    uint32_t max_coequal_visible = 0;
    uint32_t max_coequal_visible_raw = 0;
    /// The same R1-adjusted measure with the castle removed entirely. As with
    /// the attractor count, the castle's CONTRIBUTION is what the castle pass
    /// can be held to; a crowd that exists without it is a layout finding.
    uint32_t max_coequal_visible_without_castle = 0;
    /// LARGE-MASS GUARD: the widest coequal group in which EVERY member
    /// subtends at least COEQUAL_LARGE_PX. Three marks at the readability
    /// floor are a vista; three big masses are competing choices, so this one
    /// is held to 2 rather than POI_COEQUAL_VISIBLE_MAX.
    uint32_t max_coequal_large = 0;
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

// --- §10.1 THE BUMPINESS INSTRUMENT -------------------------------------------

/// The disc radius of `GROUND_RELIEF_SIGMA_20M_*`. It is NOT a NUMBERS.md row
/// on purpose: the radius is part of the constant's own NAME there, and a
/// second row saying 20 would be a shadow copy that stops agreeing the moment
/// the name changes (Rule 39 — the same argument that kept
/// `TOWER_MINOR_DIM_PER_DISTANCE` out of NUMBERS.md).
inline constexpr float GROUND_RELIEF_DISC_RADIUS = 20.0f;

/// One reading of §10.1.2's instrument at one standpoint.
///
/// THE INSTRUMENT: sample the FINAL terrain height inside a disc of radius
/// `GROUND_RELIEF_DISC_RADIUS`, fit and SUBTRACT a least-squares plane, and
/// take the standard deviation of the residual.
///
/// Detrended, because peak-to-trough over a window rewards a tilted plane and a
/// tilted plane is exactly what «ухабистая» is not. Fixed radius, because the
/// window is what names the BAND (above ~40 m is eaten by the plane fit, below
/// ~4 m sits under `LOD_VOXEL_SIZE_L0`). σ of a residual and not a max, because
/// one lucky bump must not buy a pass.
struct GroundRelief {
    float sigma = 0.0f;       ///< σ of the residual after the plane, m — THE MEASURE
    float trend_slope = 0.0f; ///< slope of the fitted plane, rad — the SELECTION property
    float p2p = 0.0f;         ///< raw peak-to-trough in the disc, m (diagnostic only)
    uint32_t samples = 0;
};

/// Reads the instrument at `centre`. Samples on the `HEIGHTMAP_STEP` lattice,
/// which is the resolution the drawn ground actually has — measuring the
/// continuous field finer than it is ever built would report relief the player
/// cannot see.
[[nodiscard]] GroundRelief ground_relief_20m(const WorldGenContext& ctx, glm::vec2 centre);

/// §10.1.2's legality clause: the floor binds on open, dry, ungraded ground.
/// NOT binding — and therefore excluded from the standpoint search — on
/// corridor masks, building pads, entrance works, the castle terrace and the
/// shore band, all of which are flattened by an approved rule.
[[nodiscard]] bool relief_floor_binds(const WorldGenContext& ctx, glm::vec2 world);

/// The standpoint search, and it is deliberately BLIND TO σ.
///
/// A metric may not find its subject by the property it is testing: ranking
/// candidates by "flattest" measured as σ would pick the smoothest ground in
/// every arm and then report that it is smooth, and adding an octave would move
/// the standpoints as well as the reading. So candidates are ranked by
/// `trend_slope` — the plane the instrument throws AWAY — which is a different
/// property of the same disc, and one the §10.1 octaves barely move.
///
/// Even that is used only ONCE, to choose the pinned standpoints recorded in
/// `docs/acceptance/`; both arms of a before/after then read those same
/// coordinates. Returned in ascending trend slope, ties broken by scan order.
[[nodiscard]] std::vector<glm::vec2> flattest_legal_standpoints(const WorldGenContext& ctx,
                                                                std::size_t count,
                                                                float search_step = 16.0f);

} // namespace dfn::world
