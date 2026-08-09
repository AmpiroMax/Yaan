/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 16:51:22
Module: engine/physics
File: engine/physics/sources/TerrainCollision.cpp

Responsibility:
- Implements both terrain collision paths: VoxelMeshView -> mesh body (the
  voxel world) and HeightFieldView -> heightfield-derived body (legacy; the
  agreed core<->sim boundary: raw uint16 decoded here, backend gets meters).

Key items:
- create_terrain_mesh_body(): world-space triangle hand-off, LAYER_STATIC.
- create_terrain_body(): decode via the frozen height formula, fill TerrainDesc.

Dependencies:
- Uses: TerrainCollision.h, CollisionLayers.h.
- Used by: chunk-load handlers, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep the decode identical to HeightFieldView::height_at (frozen contract).
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial implementation.
- 09:08:2026 - 16:51:22: Added create_terrain_mesh_body (voxel terrain).
*/

#include "engine/physics/sources/TerrainCollision.h"

#include "engine/physics/sources/CollisionLayers.h"

namespace dfn::physics {

platform::PhysicsBodyHandle create_terrain_mesh_body(platform::IPhysics& physics,
                                                     const math::VoxelMeshView& mesh,
                                                     uint64_t user_data) {
    // The extracted mesh is already world-space float triangles, so this is a
    // pure hand-off: no decode, no scratch, no per-chunk copy on our side.
    platform::TerrainMeshDesc desc;
    desc.positions = mesh.positions;
    desc.indices = mesh.indices;
    desc.layer = LAYER_STATIC;
    desc.user_data = user_data;
    return physics.create_terrain_mesh(desc);
}

platform::PhysicsBodyHandle create_terrain_body(platform::IPhysics& physics,
                                                const math::HeightFieldView& view,
                                                uint64_t user_data,
                                                std::vector<float>& scratch) {
    const size_t sample_count = static_cast<size_t>(view.resolution) * view.resolution;
    if (view.resolution < 2 || view.step <= 0.0f || view.heights.size() < sample_count) {
        return {};
    }

    scratch.clear();
    scratch.reserve(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        // Frozen formula (HeightFieldView contract): offset + raw * scale.
        scratch.push_back(view.height_offset +
                          static_cast<float>(view.heights[i]) * view.height_scale);
    }

    platform::TerrainDesc desc;
    desc.origin = {view.origin.x, 0.0f, view.origin.y}; // heights are absolute meters
    desc.sample_count_x = view.resolution;
    desc.sample_count_z = view.resolution;
    desc.sample_spacing = view.step;
    desc.heights = scratch;
    desc.layer = LAYER_STATIC;
    desc.user_data = user_data;
    return physics.create_terrain(desc);
}

} // namespace dfn::physics
