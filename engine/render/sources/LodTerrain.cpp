/*
Created: 09:08:2026 - 22:12:57
Last updated: 09:08:2026 - 22:39:28
Module: engine/render
File: engine/render/sources/LodTerrain.cpp

Responsibility:
- LodTerrain implementation: node mesh residency, skirted coarse meshing, and
  the faded submission pass.

Key items:
- LodTerrain::update / upload / drop / draw.

Dependencies:
- Uses: LodTerrain.h, TerrainLod, TerrainMesher, IRenderer.
- Used by: dfn_render.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The residency rules (no draw before resident, no release before faded out)
  live in LodResidency and must not be re-implemented or short-circuited here.
*/
/*
UPD:
- 09:08:2026 - 22:12:57: Created with the LOD drawing half.
- 09:08:2026 - 22:39:28: pending() forwarded.
*/

#include "engine/render/sources/LodTerrain.h"

#include "engine/render/sources/TerrainMesher.h"

#include <utility>

namespace dfn::render {

uint64_t LodTerrain::key_of(const LodNode& node) {
    // Node coords are world-grid indices at their level and comfortably fit
    // 24 bits each even at 10x10 km (level 0 spans +/- 8 nodes per km).
    const auto x = static_cast<uint64_t>(static_cast<uint32_t>(node.x));
    const auto z = static_cast<uint64_t>(static_cast<uint32_t>(node.z));
    return (static_cast<uint64_t>(node.level) << 56) ^ (x << 28) ^ z;
}

void LodTerrain::set_world_bounds(glm::vec2 min_xz, glm::vec2 max_xz) {
    world_min_ = min_xz;
    world_max_ = max_xz;
}

void LodTerrain::set_resident_rect(glm::vec2 min_xz, glm::vec2 max_xz) {
    resident_.min = min_xz;
    resident_.max = max_xz;
}

void LodTerrain::update(const glm::vec3& eye, float dt_seconds) {
    if (!enabled_ || world_max_.x <= world_min_.x || world_max_.y <= world_min_.y) {
        // Disabled: run the residency with an EMPTY selection rather than
        // skipping it, so everything already resident fades out and is offered
        // for release instead of being stranded on the GPU.
        residency_.update({}, dt_seconds);
        selected_count_ = 0;
        return;
    }
    const std::vector<LodNode> selection =
        select_lod_nodes(eye, world_min_, world_max_, resident_);
    selected_count_ = selection.size();
    residency_.update(selection, dt_seconds);
}

std::span<const LodNode> LodTerrain::to_load() const {
    return residency_.to_load();
}

std::span<const LodNode> LodTerrain::to_release() const {
    return residency_.to_release();
}

std::span<const LodNode> LodTerrain::pending() const {
    return residency_.pending();
}

std::span<const LodDraw> LodTerrain::residency_draws() const {
    return residency_.to_draw();
}

void LodTerrain::upload(platform::IRenderer& renderer, const LodNode& node,
                        const math::HeightFieldView& field,
                        const math::SurfaceFieldView* surface) {
    // The skirt is sized from the field, not guessed: the worst adjacent step
    // along this node's own border bounds how far a differently-tessellated
    // neighbour's edge can fall away from ours.
    TerrainMeshOptions options;
    options.skirt_depth_m =
        lod_skirt_depth_m(node.level, terrain_border_max_step_m(field));

    const TerrainMeshData mesh = build_terrain_mesh(field, surface, options);
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        return; // malformed view: leave whatever was resident alone
    }
    const platform::MeshHandle handle = renderer.create_mesh(mesh.vertices, mesh.indices);
    if (!handle.valid()) {
        return;
    }

    NodeRes res;
    res.mesh_id = handle.id;
    for (const platform::Vertex& v : mesh.vertices) {
        res.bounds.expand(v.position);
    }

    const uint64_t key = key_of(node);
    if (const auto it = meshes_.find(key); it != meshes_.end()) {
        // Replacement, not a new node: destroy the old mesh but do NOT touch
        // the fade. A node core rebuilt is the same ground, and restarting its
        // dissolve would show a hole for the fade duration.
        renderer.destroy_mesh(platform::MeshHandle{it->second.mesh_id});
        it->second = res;
    } else {
        meshes_.emplace(key, res);
    }
    residency_.mark_resident(node);
}

void LodTerrain::drop(platform::IRenderer& renderer, const LodNode& node) {
    const auto it = meshes_.find(key_of(node));
    if (it == meshes_.end()) {
        return;
    }
    renderer.destroy_mesh(platform::MeshHandle{it->second.mesh_id});
    meshes_.erase(it);
}

void LodTerrain::destroy_all(platform::IRenderer& renderer) {
    for (const auto& [key, res] : meshes_) {
        renderer.destroy_mesh(platform::MeshHandle{res.mesh_id});
    }
    meshes_.clear();
}

size_t LodTerrain::draw(platform::IRenderer& renderer, const math::Frustum& frustum,
                        platform::ProgramHandle program,
                        platform::TextureHandle atlas) const {
    last_draw_count_ = 0;
    if (program.id == 0) {
        return 0;
    }
    const glm::mat4 identity(1.0f);
    for (const LodDraw& draw_node : residency_.to_draw()) {
        const auto it = meshes_.find(key_of(draw_node.node));
        if (it == meshes_.end()) {
            continue; // selected and faded, but the mesh went away — never draw blind
        }
        if (it->second.bounds.valid() && !frustum.visible(it->second.bounds)) {
            continue;
        }
        platform::DrawParams params;
        params.fade = draw_node.fade;
        renderer.submit(platform::MeshHandle{it->second.mesh_id}, program, identity,
                        atlas, params);
        ++last_draw_count_;
    }
    return last_draw_count_;
}

} // namespace dfn::render
