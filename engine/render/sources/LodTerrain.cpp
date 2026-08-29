/*
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

#include "engine/render/sources/LodTerrain.h"

#include "engine/render/sources/TerrainMesher.h"

#include <algorithm>
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

LodRect LodTerrain::clip_region_of(const LodNode& node, const LodRect& rect) {
    if (rect.empty()) {
        return LodRect{};
    }
    const float size = lod_node_size_m(node.level);
    const float x0 = static_cast<float>(node.x) * size;
    const float z0 = static_cast<float>(node.z) * size;
    LodRect r;
    r.min = {std::max(x0, rect.min.x), std::max(z0, rect.min.y)};
    r.max = {std::min(x0 + size, rect.max.x), std::min(z0 + size, rect.max.y)};
    if (r.empty()) {
        return LodRect{}; // normalize every empty intersection to one value
    }
    return r;
}

void LodTerrain::update(const glm::vec3& eye, float dt_seconds) {
    pending_with_stale_.clear();
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

    // pending() = residency's undelivered nodes + STALE CLIPS: a resident,
    // still-selected node whose mesh was clipped for a rectangle that has
    // since moved. It keeps drawing the old mesh (never a hole) while the
    // ferry re-uploads it — core holds the field until release, so the
    // re-ship needs no new request. Deselected nodes are left alone: they are
    // fading out over ground the incoming selection already covers.
    pending_with_stale_.assign(residency_.pending().begin(),
                               residency_.pending().end());
    for (const LodNode& node : selection) {
        const auto it = meshes_.find(key_of(node));
        if (it == meshes_.end()) {
            continue; // not resident: already in pending via residency
        }
        const LodRect want = clip_region_of(node, resident_);
        const LodRect& have = it->second.clipped;
        if (want.min != have.min || want.max != have.max) {
            pending_with_stale_.push_back(node);
        }
    }
}

std::span<const LodNode> LodTerrain::to_load() const {
    return residency_.to_load();
}

std::span<const LodNode> LodTerrain::to_release() const {
    return residency_.to_release();
}

std::span<const LodNode> LodTerrain::pending() const {
    return pending_with_stale_;
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
    // Straddle clip: the part of this node inside the resident rectangle is
    // chunk ground and is not meshed (see select_lod_nodes — the selection
    // accepts straddling nodes at their distance-correct level on the promise
    // that the overlap is removed HERE).
    const LodRect clipped = clip_region_of(node, resident_);
    if (!clipped.empty()) {
        options.clip_min = resident_.min;
        options.clip_max = resident_.max;
    }

    options.path_classes = path_classes_; // одна упаковка альфы на весь кадр
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
    res.clipped = clipped;
    // Bounds over INDEXED vertices only: a clipped mesh keeps its full vertex
    // grid (grid indexing is part of the mesher's contract) but draws only
    // the emitted cells, and a cull box inflated by never-drawn vertices
    // would keep off-screen nodes alive.
    for (const uint32_t idx : mesh.indices) {
        res.bounds.expand(mesh.vertices[idx].position);
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
                        platform::TextureHandle atlas,
                        platform::TextureHandle aux,
                        platform::TextureHandle aux2,
                        platform::TextureHandle aux3) const {
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
        // The relief sheet the chunk ground shades with — the coarse ring
        // must not go flat at the cross-fade seam (see the header).
        params.aux_texture = aux;
        params.aux2_texture = aux2; // путевой атлас — тот же, что у чанков
        params.aux3_texture = aux3; // и маска троп — кромка одна на весь кадр
        // A COARSE STAND-IN NEVER CASTS INTO A CARRIED LIGHT'S CUBE. These
        // nodes are built from the heightfield WITHOUT the carves, so inside
        // a tunnel their geometry is solid rock through the corridor's own
        // air; drawn into a torch's shadow faces they put "an occluder at
        // centimetres" in every direction and the torch lights nothing at
        // all (measured: floor at 2.79 m from a sconce read 0 of 255, whole
        // frame). A torch's radius is metres, and within metres of a flame
        // the FINE world is resident by definition — this flag can only
        // remove occluders that do not exist.
        params.casts_in_point_shadows = false;
        renderer.submit(platform::MeshHandle{it->second.mesh_id}, program, identity,
                        atlas, params);
        ++last_draw_count_;
    }
    return last_draw_count_;
}

} // namespace dfn::render
