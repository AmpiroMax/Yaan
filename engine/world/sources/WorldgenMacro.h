/*
Created: 09:08:2026 - 11:05:22
Last updated: 10:08:2026 - 11:51:23
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
- 09:08:2026 - 17:45:08: §6.2: STREAM_SCATTER_MARKER for entrance standing stones.
- 09:08:2026 - 21:37:57: Banded massif streams: STREAM_MASSIF_PROFILE/LOBE/BAND/RISER; bearing_field/bearing_ridged gain a band index that decorrelates successive contour bands within one stream.
- 09:08:2026 - 21:37:57: STREAM_MASSIF_MICRO / STREAM_MASSIF_MICRO_AMP for bench micro-relief; polygon_radius replaces the circle-sampled lobe field.
- 10:08:2026 - 02:59:28: Stand selector (§8): macro_height branches to the forest stand's field when layout.stand == Forest (testbed path untouched); STREAM_FOREST_* / STREAM_EROSION / STREAM_PATHS / STREAM_FINDS / STREAM_SCATTER_FLOOR stream ids; ground_micro_relief exposed (one §2.7 octave, two consumers — Rule 32).
- 10:08:2026 - 11:51:23: STREAM_SCATTER_EDGE (§5.11 rich-edge rows).
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
    STREAM_MASSIF_PROFILE = 18, // per-bearing profile exponent (§2.8)
    STREAM_MASSIF_LOBE = 19,    // per-bearing radial extent
    STREAM_MASSIF_BAND = 20,    // contour band spans
    STREAM_MASSIF_RISER = 21,   // cliff/ramp class per (band, sector)
    STREAM_MASSIF_TOR = 17,      // summit tor slabs (§2.8.4)
    STREAM_MASSIF_MICRO = 22,   // ground micro-relief (§2.7 "terrain never flattens")
    STREAM_MASSIF_MICRO_AMP = 23, // slow field varying the micro amplitude
    STREAM_RIVER_JITTER = 24, // sinuosity displacement
    STREAM_SITES = 32,        // P4 site placement rng
    STREAM_SCATTER_TREE = 40, // 40..44: per-species scatter lattices
    STREAM_SCATTER_CLEARING = 48,
    STREAM_SCATTER_OUTCROP = 52,
    STREAM_SCATTER_CURB = 56, // corridor-margin curb stones (micro-relief batch)
    STREAM_SCATTER_MARKER = 60, // entrance standing stones (§6.2 findability)
    STREAM_FOREST_BASE = 70,  // forest stand: base rolling field (§8.1 LF-1)
    STREAM_FOREST_GRIVE = 71, // forest stand: LF-2 ridge-and-swale field
    STREAM_FOREST_GRIVE_AXIS = 72, // LF-2 drifting grive axis field
    STREAM_FOREST_GRIVE_AMP = 73,  // LF-2 slow amplitude field (2-5 m draw)
    STREAM_EROSION = 74,      // LF-8 droplet seeding
    STREAM_PATHS = 75,        // §8.1 path network draws
    STREAM_FINDS = 76,        // BR-6 find placement draws
    STREAM_SCATTER_FLOOR = 80, // 80..85: §5.10 forest floor lattices
    STREAM_SCATTER_EDGE = 88,  // 88..95: §5.11 rich-edge species per rule row
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

/// §2.7 ground micro-relief (meters, signed): two octaves at the ruled
/// GROUND_MICRO_* wavelengths, amplitude drifting between the ruled bounds.
/// On the testbed it is applied only to the massif's benches (the §3.3
/// shoreline finding — see massif_height); the forest stand applies it
/// GENERALLY (§2.7's "general terrain" ruling; that stand has no water, so
/// the shore-taper clause is vacuous there and activates with the first
/// wet stand). Exposed so WorldgenForest composes the same octave rather
/// than a second copy (Rule 32).
[[nodiscard]] float ground_micro_relief(uint64_t seed, glm::vec2 world);

/// Path-groove carve depth (meters, >= 0) at `world` (micro-relief batch):
/// PATH_GROOVE_DEPTH on the corridor centerline, smooth fade to 0 at
/// PATH_GROOVE_HALF_WIDTH. Applied inside macro_height BEFORE the river carve
/// (the channel clamp overrides it in-water, so ford shallowness is
/// untouchable); constant along the path, so corridor slopes are unaffected.
/// Exposed for the groove test.
[[nodiscard]] float path_groove_depth(const TestbedLayout& layout, glm::vec2 world);

} // namespace dfn::world
