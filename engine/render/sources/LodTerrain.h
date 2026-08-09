/*
Created: 09:08:2026 - 22:12:57
Last updated: 09:08:2026 - 22:39:28
Module: engine/render
File: engine/render/sources/LodTerrain.h

Responsibility:
- The DRAWING half of terrain LOD: owns the GPU meshes for coarse quadtree
  nodes, turns each frame's selection into submissions with a cross-fade, and
  publishes the load/release lists the app ferries to core.

Key items:
- LodTerrain: set_world_bounds / set_resident_rect / update / to_load /
  to_release / upload / drop / draw.

Dependencies:
- Uses: TerrainLod (pure policy), TerrainMesher, engine/core/math
  (HeightFieldView, SurfaceFieldView, Frustum, Aabb), IRenderer.
- Used by: RenderSystem (which forwards the app-facing calls).

Notes:
- The SEAM WITH CORE (agreed in session 09:08:2026, both halves confirmed):
  a coarse node IS a HeightFieldView — 129 samples, step = the level's voxel
  size, origin = node world origin — so nothing here needs a second mesh
  format, and the splat, the atlas and the shader are the chunk path's.
  `coarse_heightfield` may return nullopt for several frames after a request
  (core admits nodes under a budget), and core never evicts behind our back:
  a node stays resident until release_coarse_node.
- WHY A NODE CANNOT POP: a node is not drawn until its mesh exists
  (mark_resident), and it is not released until it is BOTH deselected AND
  faded fully out. Both halves are in LodResidency; this file only obeys them.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- No ECS, no wall clock: dt arrives as a parameter. The only impure thing here
  is IRenderer, and it is always a parameter, never stored (Rule 9).
*/
/*
UPD:
- 09:08:2026 - 22:12:57: Created — the render half of LOD that actually draws:
  node mesh residency, skirted coarse meshes, frustum culling and the
  screen-door cross-fade through DrawParams::fade.
- 09:08:2026 - 22:39:28: pending() forwarded for the app ferry.
*/

#pragma once

#include "engine/core/math/sources/Aabb.h"
#include "engine/core/math/sources/Frustum.h"
#include "engine/core/math/sources/HeightField.h"
#include "engine/core/math/sources/SurfaceField.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/render/sources/TerrainLod.h"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace dfn::render {

/// Owns the coarse-node meshes and draws them. One instance lives in
/// RenderSystem; the app drives it through RenderSystem's forwarding calls.
class LodTerrain {
public:
    /// The generated world's extent on xz, in metres. Read from core's
    /// world_bounds_xz() rather than from generated config: config describes
    /// the CONFIGURED extent and what exists is what the generator was run
    /// for, and those two have already diverged once.
    void set_world_bounds(glm::vec2 min_xz, glm::vec2 max_xz);

    /// The ground core currently streams at full chunk detail. Coarse nodes
    /// are excluded from it — see select_lod_nodes for why that is a
    /// correctness requirement and not an optimisation.
    void set_resident_rect(glm::vec2 min_xz, glm::vec2 max_xz);

    /// Off by default: until the app ferries node meshes there is nothing to
    /// draw, and a selection that asks for meshes nobody delivers would grow
    /// a to_load list forever.
    void set_enabled(bool enabled) { enabled_ = enabled; }
    [[nodiscard]] bool enabled() const { return enabled_; }

    /// Selects nodes for `eye` and advances the fades. Call once per frame
    /// before draw(); the app then ferries to_load()/to_release() to core.
    void update(const glm::vec3& eye, float dt_seconds);

    /// Nodes whose mesh must be requested from core this frame.
    [[nodiscard]] std::span<const LodNode> to_load() const;
    /// Nodes whose mesh core may free: deselected AND fully faded out.
    [[nodiscard]] std::span<const LodNode> to_release() const;
    /// Selected but not yet delivered. The ferry retries core's
    /// `coarse_heightfield` against THIS every frame, not against to_load():
    /// core admits nodes under a budget, so a node is announced once and
    /// arrives several frames later.
    [[nodiscard]] std::span<const LodNode> pending() const;

    /// Meshes the node and hands it to the GPU. Idempotent per node: a
    /// re-upload replaces the previous mesh and keeps the node's fade, so a
    /// node core rebuilds does not restart its dissolve. `surface` may be
    /// nullptr (slope-only splat, the agreed first-cut fallback).
    void upload(platform::IRenderer& renderer, const LodNode& node,
                const math::HeightFieldView& field,
                const math::SurfaceFieldView* surface);

    /// Destroys the node's mesh. Safe for a node that was never uploaded.
    void drop(platform::IRenderer& renderer, const LodNode& node);

    /// Releases every mesh (shutdown).
    void destroy_all(platform::IRenderer& renderer);

    /// Submits every drawable node that survives the frustum, each with its
    /// fade in DrawParams. Returns the number of draws issued.
    ///
    /// The cull here is a PLAIN frustum test, unlike RenderSystem's
    /// visible_or_casting: a coarse node lies outside the streamed rectangle
    /// by construction, i.e. further from the eye than the chunk ring, which
    /// is already past the sun shadow map's half extent. There is no
    /// off-screen shadow to preserve, so keeping off-screen nodes would buy
    /// nothing at all.
    size_t draw(platform::IRenderer& renderer, const math::Frustum& frustum,
                platform::ProgramHandle program, platform::TextureHandle atlas) const;

    /// What the last update() decided to draw, with each node's fade. Exposed
    /// because a draw COUNT cannot see a wrong fade: a renderer that ignored
    /// DrawParams::fade would still issue the right number of draws and pop.
    [[nodiscard]] std::span<const LodDraw> residency_draws() const;

    [[nodiscard]] size_t resident_count() const { return meshes_.size(); }
    [[nodiscard]] size_t selected_count() const { return selected_count_; }
    /// Draws issued by the last draw() call — the number worth watching when
    /// the world grows from 2x2 to 10x10 km.
    [[nodiscard]] size_t last_draw_count() const { return last_draw_count_; }

private:
    struct NodeRes {
        uint32_t mesh_id = 0;
        math::Aabb bounds{};
    };

    /// Node id -> 64-bit key. Exact integer identity, never a float compare.
    [[nodiscard]] static uint64_t key_of(const LodNode& node);

    LodResidency residency_;
    std::unordered_map<uint64_t, NodeRes> meshes_;
    glm::vec2 world_min_{0.0f};
    glm::vec2 world_max_{0.0f};
    LodRect resident_{};
    bool enabled_ = false;
    size_t selected_count_ = 0;
    mutable size_t last_draw_count_ = 0;
};

} // namespace dfn::render
