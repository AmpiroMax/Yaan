/*
Created: 09:08:2026 - 20:58:00
Last updated: 09:08:2026 - 20:58:00
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
- 09:08:2026 - 20:58:00: Created with the voxel render path.
*/

#include "engine/render/sources/VoxelMesher.h"

#include "engine/core/config/sources/Constants.h"

#include <glm/common.hpp>

namespace dfn::render {

namespace {

// Mirrors TerrainMesher's packing: R sand / G rock / B water-bed, alpha = sky
// visibility (255 = open sky until core supplies the real channel).
uint32_t pack_voxel_weights(float sand, float rock, float bed, uint8_t sky) {
    const auto r = static_cast<uint32_t>(glm::clamp(sand, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto g = static_cast<uint32_t>(glm::clamp(rock, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto b = static_cast<uint32_t>(glm::clamp(bed, 0.0f, 1.0f) * 255.0f + 0.5f);
    return (static_cast<uint32_t>(sky) << 24) | (b << 16) | (g << 8) | r; // 0xAABBGGRR
}

} // namespace

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

        float sand_w = 0.0f;
        float rock_w = 0.0f;
        float bed_w = 0.0f;
        if (i < mesh.materials.size()) {
            switch (static_cast<math::VoxelMaterial>(mesh.materials[i])) {
            case math::VoxelMaterial::Sand: sand_w = 1.0f; break;
            case math::VoxelMaterial::Rock: rock_w = 1.0f; break;
            // Dirt is sub-surface fill AND carved cave wall: the darkest of the
            // atlas cells is the closest thing we have to bare earth, and a
            // tunnel wall reading as grass would be worse than reading as mud.
            case math::VoxelMaterial::Dirt: bed_w = 1.0f; break;
            case math::VoxelMaterial::Grass:
            case math::VoxelMaterial::Air: break;
            }
        }
        v.color_rgba = pack_voxel_weights(sand_w, rock_w, bed_w, 255);
    }

    out.indices.assign(mesh.indices.begin(), mesh.indices.end());
    return out;
}

} // namespace dfn::render
