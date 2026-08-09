/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 14:03:23
Module: engine/world
File: engine/world/sources/WorldgenMacro.h

Responsibility:
- Worldgen v2 pass P1 (LANDSCAPE.md §2.1, §7.3): the macro heightfield as a
  pure position-based function — base fBm (octaves from dfn::config), valley
  pow-redistribution, L0 crag ridged stamp, knoll/bluff bumps, lake basin
  stamp. Position-based => neighboring chunks sample identical values on
  shared edges (the exact-stitch guarantee at WORLDGEN_MAX_HEIGHT range).

Key items:
- macro_height(): terrain height BEFORE hydrology carve and pads.
- crag_distance(): stamp-footprint query for classification (rockline).

Dependencies:
- Uses: WorldgenNoise.h, TestbedLayout.h, engine/core/config.
- Used by: WorldgenHydrology (coarse grid), Worldgen.cpp (per-sample),
  WorldgenSites/Scatter (placement queries).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): keep this a pure function of (seed, layout, world).
  No caches, no chunk state. All numeric knobs from dfn::config or the layout.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — P1 macro v2 (fBm + redistribution + stamps).
- 09:08:2026 - 13:12:19: Stage 3b amendments: L0_AIM_ABOVE_PEAK shared by C1 validation and P5 sight wedges.
- 09:08:2026 - 13:28:27: P1 anisotropy retune: STREAM_HILL_AXIS for the landform-anisotropy axis field (§2.1).
- 09:08:2026 - 14:03:23: Micro-relief batch: path_groove_depth exposed (corridor trails carved 15 cm, ford-safe by pipeline order); STREAM_SCATTER_CURB.
*/

#pragma once

#include "engine/world/sources/TestbedLayout.h"

#include <cstdint>
#include <glm/vec2.hpp>

namespace dfn::world {

/// Noise stream ids per pass (WorldgenNoise lattice streams / WorldGenRng pass
/// tags). Base fBm octaves are streams 0..2 (stage-2 compatible values).
enum WorldgenStream : uint32_t {
    STREAM_OCTAVE_BASE = 0,   // 0..2: base fBm octaves
    STREAM_HILL_AXIS = 8,     // landform-anisotropy axis field (§2.1)
    STREAM_CRAG_RIDGED = 16,  // 16..17: crag ridged noise
    STREAM_RIVER_JITTER = 24, // sinuosity displacement
    STREAM_SITES = 32,        // P4 site placement rng
    STREAM_SCATTER_TREE = 40, // 40..44: per-species scatter lattices
    STREAM_SCATTER_CLEARING = 48,
    STREAM_SCATTER_OUTCROP = 52,
    STREAM_SCATTER_CURB = 56, // corridor-margin curb stones (micro-relief batch)
};

/// Where visibility rays and sight wedges AIM on the L0: this many meters
/// above the peak terrain (the watchtower topper's mid height, §6 10-15 m).
inline constexpr float L0_AIM_ABOVE_PEAK = 8.0f;

/// Macro terrain height (meters) at a world position: P1 only — no river
/// carve, no building pads. Clamped to [0, WORLDGEN_MAX_HEIGHT].
[[nodiscard]] float macro_height(uint64_t seed, const TestbedLayout& layout, glm::vec2 world);

/// Distance from `world` to the crag stamp center (meters). The stamp
/// footprint is d < layout.crag.radius (classification: rock above rockline).
[[nodiscard]] float crag_distance(const TestbedLayout& layout, glm::vec2 world);

/// Path-groove carve depth (meters, >= 0) at `world` (micro-relief batch):
/// PATH_GROOVE_DEPTH on the corridor centerline, smooth fade to 0 at
/// PATH_GROOVE_HALF_WIDTH. Applied inside macro_height BEFORE the river carve
/// (the channel clamp overrides it in-water, so ford shallowness is
/// untouchable); constant along the path, so corridor slopes are unaffected.
/// Exposed for the groove test.
[[nodiscard]] float path_groove_depth(const TestbedLayout& layout, glm::vec2 world);

} // namespace dfn::world
