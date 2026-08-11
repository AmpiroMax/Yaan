/*
Created: 11:08:2026 - 15:12:44
Last updated: 11:08:2026 - 15:12:44
Module: engine/world
File: engine/world/sources/WorldgenOutcrop.h

Responsibility:
- LANDSCAPE §10.5 B2 — ROCK OUTCROPS, the heightmap's bones breaking through
  the soil. Slabs (0.1-0.6 m proud, 3-15 m across) and bosses (2-8 m proud,
  5-25 m across), sited where erosion STRIPS (convex curvature) and never in
  hollows, bedded on a dip azimuth coherent over BEDDING_AZIMUTH_COHERENCE.

Key items:
- outcrop_height(): additive height (m, >= 0) at a world position.
- outcrop_at(): the placement query — what stands where, for the frame census
  and for B1's source rule.

Dependencies:
- Uses: WorldgenMacro.h, TestbedLayout.h, config.
- Used by: Worldgen.cpp (compose_passes), WorldgenScatter (B1's source rule,
  B6's skirts), WorldgenCensus (the mid-field count).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THIS CLASS IS TERRAIN, NOT SCATTER, and that is a §10.2 ruling rather than an
  implementation preference. The heightmap owns 4 m and up; a 5-25 m mass with
  2-8 m of relief is squarely in the heightmap's band, and putting it there
  buys the shadow under its lip, its occlusion, its collision and its splat for
  nothing. Boulders (0.8-4 m) stay meshes for the same reason, from the other
  side of the same seam.
- WHY THIS CLASS COMES FIRST (§10.4.2): readable size = distance/30, so a 25 m
  mass reads as an OBJECT to 750 m. It is the only natural class whose read
  distance covers 150-750 m — the band where «плоско как в майнкрафте» lives.
  Boulders expire at 120 m and cannot fix the mid field at any density.
- DETERMINISM (Rule 13.1): pure function of (seed, layout, world). The lattice
  confines each candidate to the middle half of its cell so a sample is reached
  only by its 3x3 neighbourhood, and the expensive curvature test runs only for
  cells whose disc actually covers the sample.
*/
/*
UPD:
- 11:08:2026 - 15:12:44: Created — §10.5 B2.
*/

#pragma once

#include "engine/world/sources/TestbedLayout.h"

#include <cstdint>
#include <glm/vec2.hpp>
#include <vector>

namespace dfn::world {

/// One placed outcrop, as the generator knows it. THE COUNT IS ESTABLISHED
/// HERE (Rule 47): a frame census projects these, it never segments a picture.
struct Outcrop {
    glm::vec2 centre{0.0f};
    float extent = 0.0f;      ///< plan radius, m (OUTCROP_EXTENT is the diameter band)
    float proud = 0.0f;       ///< how far it stands above the soil, m
    float dip = 0.0f;         ///< bedding dip, rad
    glm::vec2 dip_dir{1.0f, 0.0f}; ///< dip azimuth, coherent over 200 m
    bool boss = false;        ///< false = pavement/slab, true = boss/tor
};

/// The outcrop whose disc covers `world`, if any (the largest one, so an
/// overlap resolves the same way for every sample). `hit` false when none.
struct OutcropHit {
    bool hit = false;
    Outcrop rock{};
};
[[nodiscard]] OutcropHit outcrop_at(uint64_t seed, const TestbedLayout& layout, glm::vec2 world);

/// Additive rock height (m, >= 0) at `world`. Zero off every outcrop.
[[nodiscard]] float outcrop_height(uint64_t seed, const TestbedLayout& layout, glm::vec2 world);

/// Every outcrop whose centre lies in [min, max) — the population, for the
/// census, for B1's «a boulder came from somewhere» rule, and for tests.
[[nodiscard]] std::vector<Outcrop> outcrops_in(uint64_t seed, const TestbedLayout& layout,
                                               glm::vec2 area_min, glm::vec2 area_max);

} // namespace dfn::world
