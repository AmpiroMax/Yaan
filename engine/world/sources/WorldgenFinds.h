/*
Created: 10:08:2026 - 10:55:03
Last updated: 10:08:2026 - 10:55:03
Module: engine/world
File: engine/world/sources/WorldgenFinds.h

Responsibility:
- BR-6, THE FIND LAYER (LANDSCAPE §1.7, user-ratified в20): the mailbox tier
  between POIs that makes walking itself the content. Two regimes with their
  own linear densities — dense near roads, sparse in the wild — placed so BR-5
  holds (the meso tier hides a find until you come round the crest).

Key items:
- FindKind / Find: the placed instance (catalog is design's, siting is mine).
- FindRegime: which cadence a find was placed at, so the gap statistics can be
  measured PER REGIME (a mixed median hides both).
- build_finds(): the pass.

Dependencies:
- Uses: WorldgenPaths.h (the network the road regime is seeded along), glm.
- Used by: Worldgen.cpp, scatter, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- BR-6's TAIL CLAUSE IS THE POINT (Rule 31): a mean can hide a desert. No gap
  on a road-adjacent route may exceed FIND_GAP_MAX_MULT x the regime's
  spacing, and that is asserted, not the median alone.
- The real rejected instance IS the control here: the current world has no
  find layer at all, so every gap is infinite. Any threshold above zero
  encounters stands above it.
*/
/*
UPD:
- 10:08:2026 - 10:55:03: Created — BR-6 two-regime find placement with BR-5 siting.
*/

#pragma once

#include "engine/world/sources/WorldgenPaths.h"

#include <cstdint>
#include <functional>
#include <span>
#include <glm/vec2.hpp>
#include <vector>

namespace dfn::world {

/// §1.7's "what a find is" list. The catalog belongs to design and flora;
/// siting belongs here.
enum class FindKind : uint8_t {
    MushroomRing = 0,
    AbandonedCart = 1,
    StrangeStone = 2,
    Spring = 3,
    SpireCluster = 4,
};

/// Which cadence placed this find. Kept on the instance because BR-6's
/// statistics are PER REGIME: a median over both regimes mixed is a number
/// about neither.
enum class FindRegime : uint8_t {
    NearRoad = 0,
    Wilderness = 1,
};

struct Find {
    FindKind kind = FindKind::StrangeStone;
    FindRegime regime = FindRegime::NearRoad;
    glm::vec2 position{0.0f, 0.0f};
    float height = 0.0f;
    /// BR-5's measurement, kept on the instance so the acceptance reads what
    /// the generator produced rather than recomputing a different ray.
    float occluded_fraction = 0.0f;
};

struct FindParams {
    /// Ring radii BR-5 measures occlusion over.
    float ring_min_m = 40.0f;
    float ring_max_m = 80.0f;
    int ring_bearings = 24;
    /// "Near a road" means within this of the network — outside it the wild
    /// cadence applies. 25 m: beyond the rich edge (2.5 m) and beyond the
    /// encounter radius' own reach off the tread, so the two regimes do not
    /// overlap on the same ground.
    float road_band_m = 25.0f;
    float lateral_min_m = 3.0f;  ///< a find sits BESIDE the tread, never on it
    float lateral_max_m = 9.0f;
};

/// Places both regimes. `height` is the shipped terrain field.
[[nodiscard]] std::vector<Find> build_finds(uint64_t seed, const PathNetwork& net,
                                            glm::vec2 domain_min, glm::vec2 domain_max,
                                            const FindParams& params,
                                            const std::function<float(glm::vec2)>& height);

// --- BR-5's composed-scene instrument (§1.7 ruling, 10.08.2026) ---------------
//
// DESIGN RULED THE OCCLUDER SET, NOT JUST THE BAR: on the forest stand BR-5 is
// measured against terrain + real placed oak trunks + real placed Bush/BigBush,
// because the terrain side was never meant to carry that job alone there. The
// bare-terrain reading (0.03/0.06 median at 40/80 m) is kept forever as the
// must-fail control — it is the literal "forest with the forest deleted".
//
// This is NOT the C1/C4 canopy transmittance model. That one is built for crown
// occlusion of a distant landmark and returns zero blocked here by construction:
// an eye at 1.7 m and a find top at 0.5 m both sit under crown_base, so every
// ray passes beneath every crown. The right shape is a stem-level ray-vs-disc
// test, which is what this is.

/// One occluder: a vertical cylinder standing on the ground.
///
/// `top_y` is an ABSOLUTE world height, not a height above ground. A ray that
/// passes over a bush is not blocked by it, and "above ground" would have to be
/// re-resolved against the terrain at the bush rather than at the ray — which
/// is the same two-surfaces mistake that cost this zone a session today.
struct OccluderDisc {
    glm::vec2 center{0.0f, 0.0f};
    float radius = 0.0f;
    float top_y = 0.0f;
};

/// THE DISC GEOMETRY, AND IT IS DELIBERATELY NOT DEFAULTED TO ANYTHING.
///
/// Rule 35, predicted rather than discovered: a trunk radius already has two
/// consumers (render's mesh, sim's collision — `species_trunk_radius()` says so
/// in its own comment) and this instrument makes a third. `engine/world` cannot
/// include `engine/render` (Rule 1, DAG siblings), so a literal here would be a
/// third copy of a number nobody owns, which is exactly the defect this project
/// spent today paying for in a different guise.
///
/// The fields are therefore REQUIRED INPUT. When the NUMBERS.md rows land, one
/// caller changes; until then every call site states its provisional geometry
/// out loud and can be found by grep.
struct OccluderGeometry {
    float oak_trunk_radius_m = 0.0f;   ///< below crown_base, scaled by instance scale
    float oak_trunk_top_frac = 0.0f;   ///< crown_base as a fraction of tree height
    float oak_height_m = 0.0f;         ///< nominal, scaled by instance scale
    float bush_radius_m = 0.0f;
    float bush_height_m = 0.0f;
    float big_bush_radius_m = 0.0f;
    float big_bush_height_m = 0.0f;
};

/// Builds the occluder set for one seed from REAL PLACED instances (design:
/// "never a mean-density approximation"). Only the three classes the gate may
/// depend on are emitted — FallenLog/snag/deadfall are excluded BY CAUSE, not
/// by size: they are sized for the user's brief, never for a validator, so a
/// gate that leans on them would be tuned by changing scenery (Rule 36).
[[nodiscard]] std::vector<OccluderDisc> build_find_occluders(
    std::span<const math::ScatterInstance> scatter, const OccluderGeometry& geom,
    const std::function<float(glm::vec2)>& height);

/// BR-5's occluded fraction AT ONE RING RADIUS. Per-distance by construction:
/// design sharpened the rule so a strong far reading can never buy cover for a
/// weak near one, and the way to make that unforgettable is to give the
/// function no way to pool. Pass `discs` empty for the bare-terrain control.
[[nodiscard]] float occluded_fraction_at(const std::function<float(glm::vec2)>& height,
                                         std::span<const OccluderDisc> discs, glm::vec2 find,
                                         float ring_radius_m, int bearings);

/// Spacing (m of route) each regime is placed at, derived from в20's cadence:
/// FIND_SPACING_BASE_S seconds of walking at WALK_SPEED, times the regime
/// multiplier. Exposed so the acceptance measures against the SAME derivation
/// the placement used (Rule 35 within one zone: one number, one source).
[[nodiscard]] float find_spacing_m(FindRegime regime);

} // namespace dfn::world
