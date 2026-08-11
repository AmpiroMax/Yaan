/*
Created: 09:08:2026 - 11:05:22
Last updated: 11:08:2026 - 15:15:55
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
- 10:08:2026 - 20:20:20: §5.12 / LF-4 breaks_massif_apron(): the apron rule,
  derived. Scoped by the massif's OWN stamp rather than by a distance literal,
  because the ruling's sentence read globally excludes every tree within ~670 m
  of a standpoint (measured) — a tree in front of your face obscures a mountain
  too. The radius is an OUTPUT: 162 m at seed 1's tightest bearing.
- 11:08:2026 - 15:15:55: streams for the §2.7 meso octave, §10.5 B1 boulders and B2 outcrops; aniso_octave_sample published so every octave that must share the land's grain samples it the same way.
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
    STREAM_GROUND_MESO = 25,    // 25..26: §2.7 meso octave (the missing middle band)
    STREAM_GROUND_MESO_AMP = 27, // slow field varying the meso amplitude
    STREAM_SCATTER_BOULDER = 64, // 64..65: §10.5 B1 boulder clusters / open draws
    STREAM_OUTCROP = 96,        // 96..127: §10.5 B2 outcrop cell draws (slots)
    STREAM_OUTCROP_BEDDING = 128, // 128..129: dip azimuth/magnitude, 200 m coherent
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

/// §5.12 / LF-4 — THE APRON RULE, AND IT IS DERIVED, NEVER A TABLED RADIUS.
///
/// True if a canopy reaching `canopy_top_y` (absolute elevation) would obscure
/// the massif's silhouette below `MASSIF_CLIFFLINE_FRAC`. Design ruled this as
/// "a HEIGHT rule at the massif foot, not a clearing", explicitly to avoid
/// §7.1a's trap of tabling a radius — so the RADIUS IS AN OUTPUT: it falls out
/// of the seed's own profile wherever the flank rises far enough that a tree
/// standing on it breaks the low outline.
///
/// Scoped to the massif by the massif's OWN stamp rather than by a distance
/// literal: off the stamp this is always false, so ordinary forest anywhere
/// else is untouched. That scoping is load-bearing — read as a global rule the
/// same sentence excludes every tree within ~670 m of a standpoint, because a
/// tree in front of your face obscures a mountain too.
///
/// Why the mountain needs it (design measured it, 10.08.2026): the pine annulus
/// begins at 140 m, INSIDE the 120-162 m hem where the massif is still
/// climbing. Pines do not start at the foot, they start ON it, and a mountain
/// missing its bottom third reads as a dome no shape change can fix.
[[nodiscard]] bool breaks_massif_apron(uint64_t seed, const CragStamp& crag, glm::vec2 world,
                                       float canopy_top_y);

/// §2.7 ground micro-relief (meters, signed): two octaves at the ruled
/// GROUND_MICRO_* wavelengths, amplitude drifting between the ruled bounds.
/// On the testbed it is applied only to the massif's benches (the §3.3
/// shoreline finding — see massif_height); the forest stand applies it
/// GENERALLY (§2.7's "general terrain" ruling; that stand has no water, so
/// the shore-taper clause is vacuous there and activates with the first
/// wet stand). Exposed so WorldgenForest composes the same octave rather
/// than a second copy (Rule 32).
[[nodiscard]] float ground_micro_relief(uint64_t seed, glm::vec2 world);

/// §2.1's ANISOTROPIC SAMPLING, exported so every octave that must share the
/// land's grain samples it the same way (Rule 32).
///
/// Returns value_noise in [0,1] for `stream`/`cell`, but read along the local
/// long axis: each WORLDGEN_OCTAVE1_CELL of the axis lattice carries one angle
/// in [0, pi), the along-axis input is compressed by HILL_ANISOTROPY and the
/// cross-axis input stays 1:1, with the four corner frames blended so the field
/// is seamless.
///
/// WHY ANY NEW OCTAVE MUST USE IT: an isotropic layer laid over the ridgelets
/// does not sit beside the grain, it erases it. Measured on seed 1 when §2.7's
/// meso octave first went in isotropically — open-meadow structure-tensor
/// anisotropy 3.61 -> 2.22 against a 2.5 floor, with the hill octave and
/// HILL_ANISOTROPY both untouched.
[[nodiscard]] float aniso_octave_sample(uint64_t seed, uint32_t stream, float cell,
                                        glm::vec2 world);

/// Path-groove carve depth (meters, >= 0) at `world` (micro-relief batch):
/// PATH_GROOVE_DEPTH on the corridor centerline, smooth fade to 0 at
/// PATH_GROOVE_HALF_WIDTH. Applied inside macro_height BEFORE the river carve
/// (the channel clamp overrides it in-water, so ford shallowness is
/// untouchable); constant along the path, so corridor slopes are unaffected.
/// Exposed for the groove test.
[[nodiscard]] float path_groove_depth(const TestbedLayout& layout, glm::vec2 world);

} // namespace dfn::world
