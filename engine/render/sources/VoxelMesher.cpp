/*
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

#include "engine/render/sources/VoxelMesher.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/Materials.h"

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::render {

TerrainMeshData build_voxel_terrain_mesh(const math::VoxelMeshView& mesh,
                                        const math::SurfaceFieldView* surface,
                                        const PathClassField* path_classes) {
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
        // ALPHA CARRIES THE PATH WEAR, and 255 means "no path" so a world
        // without a network produces byte-identical meshes. The channel was
        // documented as sky visibility and written as a constant 255 ever
        // since — reserved and never spent, which is the room a path needs.
        //
        // SAMPLED FROM THE CHUNK'S OWN FIELD at this vertex's x/z: the ground
        // IS the path, rather than wearing one. The ribbon mesh that used to
        // be laid over the terrain could hover wherever the two disagreed, and
        // no amount of tuning fixes a second surface — only not having one.
        uint8_t path_a = 255;
        if (surface != nullptr && !surface->path_wear.empty() && surface->step > 0.0f) {
            const glm::vec2 local = glm::vec2{v.position.x, v.position.z} - surface->origin;
            const int gx = static_cast<int>(std::lround(local.x / surface->step));
            const int gz = static_cast<int>(std::lround(local.y / surface->step));
            const int res = static_cast<int>(surface->resolution);
            if (gx >= 0 && gz >= 0 && gx < res && gz < res) {
                const std::size_t idx = static_cast<std::size_t>(gz) * surface->resolution
                                      + static_cast<std::size_t>(gx);
                if (idx < surface->path_wear.size()) {
                    const float wear = std::clamp(surface->path_wear[idx], 0.0f, 1.0f);
                    // С полем классов альфа несёт материал полотна (контракт
                    // pack_path_alpha, TerrainMesher.h); без него — прежние
                    // 8 бит износа бит-в-бит.
                    path_a = path_classes != nullptr
                        ? pack_path_alpha(wear,
                              wear > 0.0f
                                  ? path_classes->class_at(
                                        {v.position.x, v.position.z})
                                  : 1u)
                        : static_cast<uint8_t>(
                              std::lround((1.0f - wear) * 255.0f));
                }
            }
        }
        v.color_rgba = pack_splat(w, path_a);
    }

    out.indices.assign(mesh.indices.begin(), mesh.indices.end());
    // SAY IT ONCE per run: how much of the drawn ground came out trodden. "The
    // path is a property of the ground" is a claim; "3184 of 41000 vertices
    // carry wear" is the fact that settles whether it reached the mesh at all.
    if (surface != nullptr && !surface->path_wear.empty()) {
        static bool announced = false;
        if (!announced) {
            std::size_t worn = 0;
            for (const platform::Vertex& vv : out.vertices) {
                if ((vv.color_rgba >> 24) != 0xFFu) {
                    ++worn;
                }
            }
            if (worn > 0) {
                announced = true;
                std::fprintf(stderr, "[paths] %zu of %zu drawn vertices carry wear\n",
                             worn, out.vertices.size());
            }
        }
    }
    return out;
}

} // namespace dfn::render
