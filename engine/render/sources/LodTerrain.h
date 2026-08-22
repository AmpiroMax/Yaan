/*
Created: 09:08:2026 - 22:12:57
Last updated: 22:08:2026 - 23:49:20
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
  a coarse node IS a HeightFieldView — COARSE_NODE_RESOLUTION samples
  (LOD_NODE_VOXELS + 1 = 129), step = the level's voxel size, origin = node
  world origin — so nothing here needs a second mesh
  format, and the splat, the atlas and the shader are the chunk path's.
  `coarse_heightfield` may return nullopt for several frames after a request
  (core admits nodes under a budget), and core never evicts behind our back:
  a node stays resident until release_coarse_node. THAT 129 IS THE NODE
  LATTICE AND ONLY THE NODE LATTICE: a chunk is HEIGHTMAP_RESOLUTION = 257
  since 18:08:2026, and the two used to be the same number, so a bare "129"
  here now reads as a stale chunk figure to anyone who does not know which
  constant it came from.
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
- 10:08:2026 - 01:47:53: Straddle-ring fix, drawing half. A node overlapping
  the resident rectangle is meshed WITHOUT the overlapped cells
  (TerrainMeshOptions::clip_*), and the clip a mesh was built with is
  remembered: when the rectangle moves, every resident node whose clipped
  region changed re-enters pending() while its old mesh keeps drawing, so the
  ferry re-ships it through the ordinary upload path (core keeps node fields
  until release) and the swap keeps the fade — no hole, no pop, no app change.
- 10:08:2026 - 20:01:43: The "coarse nodes are past the shadow volume" claim in
  draw() now carries its MEASURED margin (512 m worst against a 320 m volume,
  1.6x) instead of standing as an assertion, because it is the premise that
  decides whether a cross-fading node can double-cast into the sun shadow map.
- 18:08:2026 - 12:06:07: Disambiguation only, no behaviour: the seam note's
  "129 samples" is spelled COARSE_NODE_RESOLUTION (= LOD_NODE_VOXELS + 1) and
  says out loud that it is the node lattice. HEIGHTMAP_RESOLUTION moved
  129 -> 257 (NUMBERS), so the two lattices no longer share a number, and a
  reader who assumed this 129 tracked the chunk would now be wrong about the
  seam with core — the exact kind of silent divergence this note exists to
  prevent.
- 22:08:2026 - 15:40:00: draw() принимает aux-лист нормалей — кольцо обязано шейдить рельеф
  как чанковая земля, иначе шов LOD мигает плоскостью; заметка о марже
  тени обновлена под полуохват 160 (маржа выросла до 3.2x).
- 23:08:2026 - 00:30:00: set_path_classes — кольцо пакует альфу тем же правилом, что чанки:
- 22:08:2026 - 23:49:20: draw(...aux3) — маска троп кольцу, тем же листом, что чанкам.
  разные упаковки при одном дозовом разборе читались бы мусорными классами.
*/

#pragma once

#include "engine/core/math/sources/Aabb.h"
#include "engine/render/sources/TerrainMesher.h"
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
    /// Selected but not yet delivered — PLUS resident nodes whose mesh was
    /// clipped for a rectangle that has since moved (they keep drawing their
    /// old mesh while the ferry re-ships them). The ferry retries core's
    /// `coarse_heightfield` against THIS every frame, not against to_load():
    /// core admits nodes under a budget, so a node is announced once and
    /// arrives several frames later; and a re-clip needs no announcement at
    /// all because core still holds the field.
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
    ///
    /// THAT CLAIM NOW CARRIES ITS MARGIN, because it is the premise a shadow
    /// artefact hunt had to check rather than accept (Rule 34). Measured in a
    /// running build: the streamed rectangle is CHUNK_LOAD_RADIUS chunks each
    /// way of the focus chunk, so its nearest edge is 2*CHUNK_SIZE = 512 m
    /// from the eye at worst (eye at its chunk's far corner) and 768 m at
    /// best, against SHADOW_HALF_EXTENT_M = 160 m (was 320 until 22.08; the
    /// soft-edge halving GREW this margin to 3.2x at the worst eye position).
    /// The margin is what decides whether a cross-fading node can put a
    /// second version of the same ground into the sun shadow map: shrink
    /// CHUNK_LOAD_RADIUS, or grow the shadow volume past 512 m for a second
    /// cascade, and coarse geometry enters the map. The backend's
    /// SHADOW_CASTER_MIN_FADE gate makes that safe in advance rather than
    /// after the frame that shows it.
    ///
    /// `aux` is the terrain normal atlas (DrawParams::aux_texture), passed so
    /// the coarse ring shades its relief exactly like the chunk ground it
    /// cross-fades with — a ring that went flat at the LOD seam would flash
    /// the seam every re-selection.
    size_t draw(platform::IRenderer& renderer, const math::Frustum& frustum,
                platform::ProgramHandle program, platform::TextureHandle atlas,
                platform::TextureHandle aux = {},
                platform::TextureHandle aux2 = {},
                platform::TextureHandle aux3 = {}) const;

    /// Поле классов полотна (TerrainMesher.h). Кольцо обязано паковать альфу
    /// ТЕМ ЖЕ правилом, что чанковая земля: шейдер разбирает её одним дозовым
    /// правилом на весь кадр, и старая 8-битная упаковка на кольце при новой
    /// на чанках читалась бы мусорными классами. Указатель живёт у владельца
    /// (RenderSystem); nullptr = прежняя упаковка.
    void set_path_classes(const PathClassField* classes) { path_classes_ = classes; }

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
        /// The clipped region this mesh was built with: intersection of the
        /// node footprint and the resident rectangle AT UPLOAD TIME (empty =
        /// unclipped). Compared against the current rectangle each update to
        /// decide whether the mesh is stale and must be re-shipped.
        LodRect clipped{};
    };

    /// Node id -> 64-bit key. Exact integer identity, never a float compare.
    [[nodiscard]] static uint64_t key_of(const LodNode& node);

    /// Intersection of a node's footprint with `rect`, empty-normalized.
    [[nodiscard]] static LodRect clip_region_of(const LodNode& node,
                                                const LodRect& rect);

    LodResidency residency_;
    std::unordered_map<uint64_t, NodeRes> meshes_;
    std::vector<LodNode> pending_with_stale_;
    glm::vec2 world_min_{0.0f};
    glm::vec2 world_max_{0.0f};
    LodRect resident_{};
    bool enabled_ = false;
    size_t selected_count_ = 0;
    mutable size_t last_draw_count_ = 0;
    const PathClassField* path_classes_ = nullptr;
};

} // namespace dfn::render
