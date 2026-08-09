/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 11:05:22
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
*/

#pragma once

#include "engine/world/sources/Worldgen.h"

namespace dfn::world {

/// §3.1.4: true iff every river station's water surface is <= its upstream
/// neighbor's (a climbing river = failed generation) AND hydrology built ok.
[[nodiscard]] bool river_is_monotonic(const HydrologyData& hydro);

/// C1 / LANDMARK_VISIBILITY_MIN: fraction of open walkable standpoints (grid
/// sampled, eye height, water/forest/crag/steep excluded) from which the L0
/// crag tower top is visible over the terrain.
[[nodiscard]] float landmark_visibility_fraction(const WorldGenContext& ctx);

/// §2.4: the worst corridor's average along-path slope (radians). Must stay
/// under CORRIDOR_SLOPE_MAX.
[[nodiscard]] float max_corridor_avg_slope(const WorldGenContext& ctx);

} // namespace dfn::world
