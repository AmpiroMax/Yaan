/*
Created: 09:08:2026 - 11:57:20
Last updated: 09:08:2026 - 11:57:20
Module: engine/render
File: engine/render/sources/ScatterBatcher.h

Responsibility:
- Bakes a chunk's ScatterInstance span (core P5 data, not entities) into
  batched CPU meshes: one tree batch per chunk (oak/pine/birch — always drawn)
  and micro tiles (bush/stone) sized for GRASS_VIEW_DISTANCE camera culling.

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
*/

#pragma once

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
struct ScatterBatches {
    MeshData trees;
    std::vector<MicroTile> micro;
};

/// Bakes `instances` into batches. `chunk_origin`/`chunk_size` define the tile
/// grid (`micro_tiles_per_axis`^2 tiles); instances are assigned to tiles by
/// x/z. Tile radius covers the actual baked geometry, so culling by
/// `distance(eye, center) < view_distance + radius` never pops visible props.
[[nodiscard]] ScatterBatches
build_scatter_batches(std::span<const math::ScatterInstance> instances,
                      glm::vec2 chunk_origin, float chunk_size,
                      uint32_t micro_tiles_per_axis = 4);

} // namespace dfn::render
