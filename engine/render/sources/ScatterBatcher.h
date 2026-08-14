/*
Created: 09:08:2026 - 11:57:20
Last updated: 14:08:2026 - 19:34:00
Module: engine/render
File: engine/render/sources/ScatterBatcher.h

Responsibility:
- Bakes a chunk's ScatterInstance span (core P5 data, not entities) into
  batched CPU meshes: one opaque tree batch and one alpha-cutout FOLIAGE batch
  per chunk (oak/pine/birch — always drawn) and micro tiles (bush/stone) sized
  for GRASS_VIEW_DISTANCE camera culling.

Key items:
- ScatterBatches / MicroTile; build_scatter_batches().

Dependencies:
- Uses: ProcMesh (species meshes, append_transformed), engine/core/math
  (ScatterInstance), glm.
- Used by: RenderSystem::upload_scatter, tests/render/ScatterBatcherTests.cpp.

Notes:
- Batching (not instancing) keeps the frozen IRenderer contract: one
  create_mesh + one submit per batch instead of thousands of per-instance
  submits. Instancing lands via a contract sync only if profiling demands
  (recorded in the spec's boundary agreement).
- Instances are baked in world space (positions arrive world-space with y =
  terrain height); meshes are submitted with the identity transform like
  terrain. A small sink (fraction of scale) hides downhill-edge float on
  slopes.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure function of its inputs — deterministic, GPU-free, unit-tested.
*/
/*
UPD:
- 09:08:2026 - 11:57:20: Stage 3b — initial scatter batching.
- 09:08:2026 - 20:21:13: Second stream: ScatterBatches::foliage (alpha-cutout
  leaf cards). EDITED BY THE FLORA AGENT under an explicit lead-granted Rule 25
  exception while render's zone was unowned; wiring only, no material change.
- 14:08:2026 - 19:34:00: FloraLod у выпечки — параметр, плюс flora_lod_for_distance и flora_lod_forced (рез ведущего). Уровень есть параметр ВЫПЕЧКИ, а не отрисовки, потому что партия — один слитый меш: выбирать покадрово значило бы покадрово сливать заново.
*/

#pragma once

#include "engine/render/sources/ProcFlora.h" // FloraLod: the bake's detail level
#include "engine/render/sources/ProcMesh.h"

#include <span>
#include <vector>

namespace dfn::render {

/// One micro-scatter tile: bushes/stones baked together, drawn only when the
/// eye is within the micro view distance of its bounding circle.
struct MicroTile {
    glm::vec2 center_xz{0.0f}; ///< world meters
    float radius_m = 0.0f;     ///< bounding circle of the baked instances
    MeshData mesh;
};

/// Per-chunk scatter batches: trees always drawn, micro tiles distance-culled.
///
/// `trees` and `foliage` are TWO STREAMS ON PURPOSE and merging them would be a
/// bug that looks like an optimisation. On the "prop" program a vertex's colour
/// is its ALBEDO; on the "foliage" program the same four bytes are WIND DATA
/// (sway weight, instance phase, per-card value jitter, sky visibility) and the
/// albedo comes from the leaf mask atlas instead. Same bytes, different
/// meaning, therefore different draws — and the foliage draw additionally needs
/// the mask bound so its shadow caster can punch the holes through the depth
/// map.
struct ScatterBatches {
    MeshData trees;   ///< trunks, branches, cone tiers -> "prop"
    MeshData foliage; ///< alpha-cutout leaf cards -> "foliage" + leaf atlas
    std::vector<MicroTile> micro;
};

/// The detail level a chunk `distance_m` away should be baked at, given the
/// level it is baked at NOW (`current`). Pure, so the banding is testable
/// without a renderer, a window or a world.
///
/// The hysteresis (FLORA_LOD_HYSTERESIS_M) is not polish. A band edge is a
/// circle drawn through ordinary walking ground; without overlap a player
/// standing on one would push the chunk across it and back every frame and pay
/// a full re-bake each time, turning a cost saving into the worst hitch in the
/// game. A chunk therefore only DROPS detail once it is a hysteresis past the
/// band, and only REGAINS it once it is a hysteresis inside.
[[nodiscard]] FloraLod flora_lod_for_distance(float distance_m, FloraLod current);

/// True when DFN_FLORA_FORCE_LOD pins the whole world to one detail level.
/// The distance banding must not run in that case: forcing is the CONTROL arm
/// of the banding's own measurement, and a banding pass re-baking chunks whose
/// bytes cannot change would burn frames proving nothing.
[[nodiscard]] bool flora_lod_forced();

/// Bakes `instances` into batches. `chunk_origin`/`chunk_size` define the tile
/// grid (`micro_tiles_per_axis`^2 tiles); instances are assigned to tiles by
/// x/z. Tile radius covers the actual baked geometry, so culling by
/// `distance(eye, center) < view_distance + radius` never pops visible props.
///
/// `lod` is the detail level every FLORA instance in this batch is built at.
/// It is a parameter of the BAKE and not of the draw because the batch is one
/// merged mesh: choosing per frame would mean re-merging per frame. The caller
/// (RenderSystem) picks it from the chunk's distance and re-bakes the chunk
/// when it crosses a band — see FLORA_LOD_REDUCED_M / FLORA_LOD_SILHOUETTE_M.
[[nodiscard]] ScatterBatches
build_scatter_batches(std::span<const math::ScatterInstance> instances,
                      glm::vec2 chunk_origin, float chunk_size,
                      uint32_t micro_tiles_per_axis = 4,
                      FloraLod lod = FloraLod::Full);

} // namespace dfn::render
