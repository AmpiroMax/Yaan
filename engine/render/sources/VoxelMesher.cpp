/*
Created: 09:08:2026 - 19:40:00
Last updated: 10:08:2026 - 21:13:39
Module: engine/render
File: engine/render/sources/VoxelMesher.cpp

Responsibility:
- build_voxel_terrain_mesh: VoxelMeshView -> renderer vertices/indices with the
  splat-weight, UV and alpha conventions the terrain shader expects.

Key items:
- build_voxel_terrain_mesh; the VoxelMaterial -> splat weight mapping.

Dependencies:
- Uses: VoxelMesher.h, generated Constants.h (CHUNK_SIZE for UV scale), glm.
- Used by: RenderSystem::upload_terrain_voxel, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep the weight packing identical to TerrainMesher's: both feed the same
  shader, and a divergence would show as two different-looking terrains.
*/
/*
UPD:
- 09:08:2026 - 19:40:00: Created with the voxel render path.
- 10:08:2026 - 21:13:39: Rule 39 fix, render half. GrassRockBlend was drawing as
  plain grass (rock weight 0.0) while the heightfield path drew the same
  ground at 0.5, on the same chunk-load branch. The private switch and
  pack_voxel_weights() are gone; one table in Materials.h serves both meshers.
  26136 of 1183258 voxel vertices on the seed-1 testbed (2.21%, core's
  measurement) carried the material and drew wrong.
*/

#include "engine/render/sources/VoxelMesher.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/Materials.h"

#include <glm/common.hpp>

namespace dfn::render {

TerrainMeshData build_voxel_terrain_mesh(const math::VoxelMeshView& mesh) {
    TerrainMeshData out;
    const size_t count = mesh.positions.size();
    if (count == 0 || mesh.indices.empty()) {
        return out; // solid or empty chunk — normal, not an error
    }

    out.vertices.resize(count);
    // UVs are world-space so the atlas tiles continuously across chunk borders
    // and across the seam where a carve meets open ground.
    const float inv_span = 1.0f / static_cast<float>(config::CHUNK_SIZE);
    for (size_t i = 0; i < count; ++i) {
        platform::Vertex& v = out.vertices[i];
        v.position = mesh.positions[i];
        v.normal = i < mesh.normals.size() ? mesh.normals[i] : glm::vec3{0.0f, 1.0f, 0.0f};
        v.uv = {v.position.x * inv_span, v.position.z * inv_span};

        // ONE table, shared with TerrainMesher (Materials.h). This file used to
        // own a second switch, over VoxelMaterial where TerrainMesher switched
        // over SurfaceClass. Both were exhaustive within their own enum — which
        // is precisely why -Wswitch could not see that they disagreed about the
        // blend class, and why it drew as plain grass here (Rule 39).
        SplatWeights w;
        if (i < mesh.materials.size()) {
            w = splat_weights_of(static_cast<math::VoxelMaterial>(mesh.materials[i]));
        }
        // Alpha is sky visibility; 255 (open sky) until core supplies the
        // channel, which VoxelMeshView::sky_visibility now carries.
        v.color_rgba = pack_splat(w, 255);
    }

    out.indices.assign(mesh.indices.begin(), mesh.indices.end());
    return out;
}

} // namespace dfn::render
