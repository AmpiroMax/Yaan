/*
Created: 09:08:2026 - 11:05:22
Last updated: 10:08:2026 - 11:51:23
Module: engine/world
File: engine/world/sources/WorldgenScatter.h

Responsibility:
- Worldgen v2 pass P5 (LANDSCAPE.md §2.2, §5, §7.1): deterministic meso
  scatter — forest masses (oak/pine/birch per the layout regions), clearings,
  bushes on forest edges, loose stones and outcrop clusters, the watchpoint
  cluster — as per-chunk instance lists (render decides how to draw).

Key items:
- build_scatter(): instances whose positions fall inside one chunk.
- in_forest_mass(): the P5 forest mask (shared with validation).

Dependencies:
- Uses: TestbedLayout.h, WorldgenHydrology.h, WorldgenSites.h,
  core/math/SurfaceField.h (ScatterInstance), config.
- Used by: Worldgen.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM (Rule 13.1): all placement runs on world-space jittered lattices
  keyed by lattice-cell coords (LANDSCAPE §2 cross-border trick) — a chunk
  computes any instance whose cell touches it and keeps those inside its
  half-open bounds; neighbors agree without communication.
- Scatter is DATA, not entities: instances never enter the ECS or the entity
  records (Rule 11 friendliness; render consumes via ChunkManager).
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — P5 scatter.
- 09:08:2026 - 13:12:19: Stage 3b amendments: canopy_height_at exposed for the canopy-aware C1 raycast (§1.1).
- 10:08:2026 - 11:51:23: §5.10 THE FOREST FLOOR gets a consumer. build_scatter
  takes the stand's erosion grid and path network — not optional and not
  branched on, since an empty grid samples 0 and an empty network flattens by
  0, so the testbed runs the identical path (Rule 32). Without them every
  instance on the forest stand stood at its PRE-EROSION height. Also exports
  in_forest_interior / in_open_ground: a per-hectare density is per hectare of
  ITS OWN ground, and the acceptance must divide by the area the placement
  multiplied by.
*/

#pragma once

#include "engine/core/math/sources/SurfaceField.h"
#include "engine/world/sources/TestbedLayout.h"
#include "engine/world/sources/WorldgenHydrology.h"
#include "engine/world/sources/WorldgenErosion.h"
#include "engine/world/sources/WorldgenPaths.h"
#include "engine/world/sources/WorldgenSites.h"

#include <vector>

namespace dfn::world {

/// True if `world` is inside an oak or pine forest-mass region (layout shapes;
/// terrain suitability is checked per instance, not here).
[[nodiscard]] bool in_forest_mass(const TestbedLayout& layout, glm::vec2 world);

/// The two §5.10 placement DOMAINS, exported because a density per hectare is
/// a density per hectare OF ITS OWN GROUND, and the acceptance must divide by
/// the same area the placement multiplied by.
///
/// This is not a convenience: measuring SNAG_DENSITY_OPEN over the whole stand
/// read 0.029/ha against a declared 0.25-0.5 and looked like a nine-fold
/// placement bug. It was the denominator — on the §8.1 stand the oak mass
/// covers everything, so "open ground" is the clearings alone. One predicate,
/// two callers, no second definition to drift.
[[nodiscard]] bool in_forest_interior(uint64_t seed, const TestbedLayout& layout,
                                      glm::vec2 world);
/// The complement plus the clearings inside the mass — a pale snag in a glade
/// is exactly the landmark case the split exists for.
[[nodiscard]] bool in_open_ground(uint64_t seed, const TestbedLayout& layout, glm::vec2 world);

/// Occlusion canopy height above terrain at `world` for the canopy-aware C1
/// raycast (§1.1: the occlusion heightfield is terrain PLUS canopy): the
/// species' §5 max height inside a forest mass, 0 in clearings, on the crag
/// treeless band and outside masses. `terrain_h` = terrain height there.
[[nodiscard]] float canopy_height_at(uint64_t seed, const TestbedLayout& layout,
                                     glm::vec2 world, float terrain_h);

/// All scatter instances whose positions fall inside [chunk_min, chunk_max)
/// (world meters, half-open so chunk borders never duplicate instances).
/// `erosion` and `paths` are the §8.1 stand's own passes. THEY ARE NOT
/// OPTIONAL AND THEY ARE NOT BRANCHED ON: an empty ErosionGrid samples 0 and an
/// empty PathNetwork flattens by 0 and reports "far from any path", so the
/// testbed goes through the identical code and produces identical scatter
/// (Rule 32). They are here because without them every instance on the forest
/// stand stood at its PRE-EROSION height and dead wood could land on the tread.
[[nodiscard]] std::vector<math::ScatterInstance> build_scatter(
    uint64_t seed, const TestbedLayout& layout, const HydrologyData& hydro,
    const SitesData& sites, const ErosionGrid& erosion, const PathNetwork& paths,
    glm::vec2 chunk_min, glm::vec2 chunk_max);

} // namespace dfn::world
