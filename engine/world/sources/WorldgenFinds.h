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

/// Spacing (m of route) each regime is placed at, derived from в20's cadence:
/// FIND_SPACING_BASE_S seconds of walking at WALK_SPEED, times the regime
/// multiplier. Exposed so the acceptance measures against the SAME derivation
/// the placement used (Rule 35 within one zone: one number, one source).
[[nodiscard]] float find_spacing_m(FindRegime regime);

} // namespace dfn::world
