/*
Module: engine/world
File: engine/world/sources/WorldgenGreatOak.h

Responsibility:
- THE GREAT OAK'S PLACEMENT (docs/GIANT_OAKS.md §2, core's half): where the
  landmark trees stand, how far apart they must be, and the clearing each one
  needs. Every quantity here is DERIVED from an approved constant — none of
  them is a density or a coordinate anybody chose.

Key items:
- GreatOakSite: one placed giant (trunk, ground, crown, clearing).
- great_oak_* derived sizes: canopy top, crown radius, clearing radius,
  separation.
- place_great_oaks(): the pass, run once per world in build_world_context.
- in_great_oak_clearing() / great_oak_canopy_at(): the two queries every other
  pass asks.

Dependencies:
- Uses: glm only (WorldGenContext is forward-declared, because Worldgen.h holds
  the site list and would otherwise include this header in a cycle).
- Used by: Worldgen.cpp (the pass), WorldgenScatter.cpp (the clearing and the
  occlusion envelope), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): the site list is a pure function of (seed, layout,
  the finished ground). No chunk may influence it — every chunk computes the
  same list or the clearing would have a seam down the middle of it.
- NOTHING HERE IS A TUNABLE. If a number below wants to move, it moves in
  docs/NUMBERS.md or in the rule it is derived from (Rule 14/35).
*/

#pragma once

#include <glm/vec2.hpp>

#include <span>
#include <vector>

namespace dfn::world {

struct WorldGenContext;

/// One placed giant. `crown_radius` is the LOWER crown's radius — for this
/// species it equals the tree's height (the user's own headline rule), which
/// is why every other number in this file is about width and not about height.
struct GreatOakSite {
    glm::vec2 pos{0.0f};        ///< trunk, world XZ
    float ground_y = 0.0f;      ///< finished ground under the trunk (compose_passes)
    float height = 0.0f;        ///< canopy top above ground_y
    float crown_radius = 0.0f;  ///< = height
    float clearing_radius = 0.0f;
    /// The named oak of GIANT_OAKS §6 (the golden chain). ALWAYS FALSE TODAY
    /// and deliberately so: §6 binds that tree to a high sea cliff, this world
    /// has no sea, and promoting the only giant we do have would be inventing
    /// the site the section forbids inventing.
    bool chained = false;
};

/// THE CANOPY TOP CORE ALREADY PROMISES FOR A GIANT, re-used rather than
/// re-derived: OAK_HEIGHT_MAX x TREE_MATURITY_GIANT_MULT_MAX, the occlusion
/// envelope every sight wedge and the C1 raycast are already written against.
/// The drawn species is 34-46 m (flora's table), so this is a ceiling and not
/// a claim — and a ceiling is what an occlusion envelope must be.
[[nodiscard]] float great_oak_height_m();

/// Lower-crown radius = tree height (GIANT_OAKS §1, verbatim: «чья нижняя
/// часть кроны будет в радиусе равна высоте»). flora implements the same rule
/// as crown_radius_per_height = 1.0.
[[nodiscard]] float great_oak_crown_radius_m();

/// THE CLEARING flora measured, derived rather than tabled: the giant's own
/// crown, plus the radius of the neighbour whose crown must not reach into it.
/// A forest oak's crown radius is half the lattice spacing by construction —
/// that is what "crowns now touch at 12-18 m" means — so the margin is
/// TREE_SPACING_FOREST/2 and nothing else.
[[nodiscard]] float great_oak_clearing_radius_m();

/// THE RARITY, AND IT IS A CONSEQUENCE (GIANT_OAKS §2). Two giants inside each
/// other's read distance do one composition job twice and the world reads as a
/// garden, so the separation IS the read distance of the crown:
/// size x (INTERNAL_RES_H / CAMERA_FOV_Y) / SILHOUETTE_MIN_PX, the same ladder
/// §10.4 states as d = 30 x S.
[[nodiscard]] float great_oak_separation_m();

/// The pass. Deterministic; empty when DFN_NO_GREAT_OAK is set (the zero-dose
/// arm of Rule 30/48 — the whole class, clearing included, in one binary).
[[nodiscard]] std::vector<GreatOakSite> place_great_oaks(const WorldGenContext& ctx);

/// True if `world` lies inside some giant's clearing.
[[nodiscard]] bool in_great_oak_clearing(std::span<const GreatOakSite> sites, glm::vec2 world);

/// Occluder height ABOVE GROUND at `world` contributed by the giants: the
/// canopy top under the crown disc, 0 outside it.
///
/// THIS IS THE HALF THAT IS EASY TO FORGET. The world's canopy envelope is
/// indexed by forest mask, and a giant stands in a clearing — where that mask
/// says "no canopy". Without this the occlusion model would carry a 48 m tree
/// as open sky, which is the "модель вдвое ниже мира" defect at eight times the
/// width instead of one and a half times the height.
[[nodiscard]] float great_oak_canopy_at(std::span<const GreatOakSite> sites, glm::vec2 world);

} // namespace dfn::world
