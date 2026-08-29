/*
Module: engine/world
File: engine/world/sources/VoxelMesh.h

Responsibility:
- Surface extraction from a VoxelVolume: surface nets (one vertex per
  sign-changing cell, quads across shared edges), producing the owned mesh
  behind the cross-zone math::VoxelMeshView.

Key items:
- VoxelMeshData: owned extraction output; view() yields the handoff type.
- extract_surface_nets(): volume -> mesh.

Dependencies:
- Uses: VoxelVolume.h, core/math/VoxelField.h.
- Used by: Worldgen (per chunk), ChunkManager, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Surface nets, deliberately, not marching cubes: MC emits sliver triangles
  that make degenerate physics contact manifolds, and it cannot hold a sharp
  edge either. Dual contouring (QEF vertex placement) is the documented
  upgrade path and changes ONLY where the vertex sits — the traversal and the
  quad emission below stay valid.
- DETERMINISM (Rule 13.1): vertex indices come from a fixed traversal order.
  If this is ever threaded, threads must write pre-assigned index ranges —
  appending in completion order would break the determinism contract.
*/

#pragma once

#include "engine/core/math/sources/VoxelField.h"
#include "engine/world/sources/VoxelVolume.h"

#include <vector>

namespace dfn::world {

/// Owned extraction output for one chunk. Parallel arrays, world-space
/// positions; the volume it came from is discarded (the world is not
/// destructible, so nothing needs the field afterwards).
struct VoxelMeshData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint8_t> materials;
    std::vector<uint32_t> indices;

    [[nodiscard]] math::VoxelMeshView view(ChunkCoord coord) const;
    [[nodiscard]] bool empty() const { return indices.empty(); }
};

/// Extracts the isosurface at distance 0. Deterministic.
[[nodiscard]] VoxelMeshData extract_surface_nets(const VoxelVolume& volume);

} // namespace dfn::world
