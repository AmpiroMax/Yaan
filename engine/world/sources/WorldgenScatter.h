/*
Created: 09:08:2026 - 11:05:22
Last updated: 09:08:2026 - 13:12:19
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
*/

#pragma once

#include "engine/core/math/sources/SurfaceField.h"
#include "engine/world/sources/TestbedLayout.h"
#include "engine/world/sources/WorldgenHydrology.h"
#include "engine/world/sources/WorldgenSites.h"

#include <vector>

namespace dfn::world {

/// True if `world` is inside an oak or pine forest-mass region (layout shapes;
/// terrain suitability is checked per instance, not here).
[[nodiscard]] bool in_forest_mass(const TestbedLayout& layout, glm::vec2 world);

/// Occlusion canopy height above terrain at `world` for the canopy-aware C1
/// raycast (§1.1: the occlusion heightfield is terrain PLUS canopy): the
/// species' §5 max height inside a forest mass, 0 in clearings, on the crag
/// treeless band and outside masses. `terrain_h` = terrain height there.
[[nodiscard]] float canopy_height_at(uint64_t seed, const TestbedLayout& layout,
                                     glm::vec2 world, float terrain_h);

/// All scatter instances whose positions fall inside [chunk_min, chunk_max)
/// (world meters, half-open so chunk borders never duplicate instances).
[[nodiscard]] std::vector<math::ScatterInstance> build_scatter(
    uint64_t seed, const TestbedLayout& layout, const HydrologyData& hydro,
    const SitesData& sites, glm::vec2 chunk_min, glm::vec2 chunk_max);

} // namespace dfn::world
