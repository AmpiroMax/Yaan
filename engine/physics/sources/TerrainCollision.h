/*
Module: engine/physics
File: engine/physics/sources/TerrainCollision.h

Responsibility:
- Bridges world terrain data to physics. Voxel path: math::VoxelMeshView ->
  one static mesh body per chunk (tunnels/overhangs). Heightmap path (legacy):
  math::HeightFieldView -> heightfield-derived body; the uint16 -> float meters
  conversion lives HERE, per the stage-1 boundary agreement with core.

Key items:
- create_terrain_mesh_body(): THE voxel-world terrain call, one body per chunk.
- create_terrain_body(): heightmap decode + create_terrain (no overhangs).

Dependencies:
- Uses: core math (HeightFieldView), platform physics interface, CollisionLayers.
- Used by: chunk-load handlers (app/world side), jolt-backed tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Height formula is the frozen HeightFieldView contract:
  height_m = height_offset + raw * height_scale. Never reimplement elsewhere.
*/

#pragma once

#include <vector>

#include "engine/core/math/sources/HeightField.h"
#include "engine/core/math/sources/VoxelField.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::physics {

/// THE terrain collision call for the voxel world: builds one static body per
/// chunk from the extracted surface mesh, on LAYER_STATIC. Represents tunnels,
/// caves and overhangs — a heightfield-derived body cannot, which is why the
/// player used to walk over the crag instead of through it.
/// `user_data` carries the chunk terrain entity's id bits.
/// Returns an INVALID handle when the chunk has no triangles (all air or all
/// rock) — that means "no body needed", not an error; callers simply skip it.
[[nodiscard]] platform::PhysicsBodyHandle create_terrain_mesh_body(
    platform::IPhysics& physics, const math::VoxelMeshView& mesh,
    uint64_t user_data);

/// Heightmap terrain collision (pre-voxel worlds and tests). Decodes `view`
/// into float meters (scratch is reused between calls to avoid per-chunk
/// allocations) and creates one static body on LAYER_STATIC.
/// CANNOT represent overhangs — voxel terrain must use create_terrain_mesh_body.
/// Returns an invalid handle if the view is malformed.
[[nodiscard]] platform::PhysicsBodyHandle create_terrain_body(
    platform::IPhysics& physics, const math::HeightFieldView& view,
    uint64_t user_data, std::vector<float>& scratch);

} // namespace dfn::physics
