/*
Created: 09:08:2026 - 11:57:20
Last updated: 09:08:2026 - 20:05:00
Module: engine/render
File: engine/render/sources/ProcMesh.h

Responsibility:
- Procedural placeholder meshes (stage 3b): flora/stone scatter meshes per the
  LANDSCAPE.md §5 species briefs and structure meshes per the §6 silhouette
  codes, mapped to the lead-blessed placeholder RenderMesh ids 1..7. Pure,
  deterministic, GPU-free — real content arrives via data files later (Rule 5).

Key items:
- MeshData: shared CPU mesh buffer (also used by WaterMesher/ScatterBatcher).
- build_scatter_mesh(species): oak/pine/birch/bush/stone canonical meshes.
- build_site_mesh(mesh_id): dwelling=1, trader=2, tavern=3, barn=4, shrine=5,
  dungeon_entrance=6, tower_ruin=7 (id table blessed by the lead; worldgen
  attaches the same ids via SiteComponents.h — render never includes world).

Dependencies:
- Uses: engine/core/math (ScatterSpecies), engine/platform/render (Vertex), glm.
- Used by: RenderSystem (site mesh registry), ScatterBatcher, tests.

Notes:
- Model space: y = 0 at the ground/pad surface, +X east / +Z south like the
  world; scatter meshes are built at the species' NOMINAL size (§5), instance
  scale is applied by the batcher. Site meshes fit the §6 footprint x height
  boxes that core mirrors in SiteComponents (LocalBounds).
- Dimensions/colors cite LANDSCAPE.md §5/§6 briefs (design zone owns the
  values). They are placeholder-asset geometry, not gameplay constants; the
  §5/§6 numbers flagged "предложение" stay on the NUMBERS.md migration list.
- Flat-shaded (hard-edged low-poly read at low res): faces do not share
  vertices. Tri budgets per §5/§6 are asserted in ProcMeshTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep building pure and deterministic (fixed seeds); no GPU, no ECS access.
- Mesh ids 1..7 are a cross-zone agreement — never renumber unilaterally.
*/
/*
UPD:
- 09:08:2026 - 11:57:20: Stage 3b — initial placeholder mesh catalog.
- 09:08:2026 - 20:05:00: pack/tri/quad exposed for the flora agent's ProcFlora
  (same directory, its own files) instead of being duplicated there.
*/

#pragma once

#include "engine/core/math/sources/SurfaceField.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <vector>

namespace dfn::render {

/// CPU-side mesh ready for IRenderer::create_mesh. Shared by the procedural
/// mesh builders (props, scatter batches, water bodies).
struct MeshData {
    std::vector<platform::Vertex> vertices;
    std::vector<uint32_t> indices;
    [[nodiscard]] size_t triangle_count() const { return indices.size() / 3; }
};

/// Packs a linear 0..1 colour into the frozen Vertex's 0xAABBGGRR field.
[[nodiscard]] uint32_t pack(const glm::vec3& color);

/// Flat-shaded triangle: the normal comes from the winding (CCW seen from
/// outside), which is what gives the hard-edged low-poly read.
void tri(MeshData& m, glm::vec3 a, glm::vec3 b, glm::vec3 c, uint32_t color);

/// Two triangles a-b-c and a-c-d, same winding rule.
void quad(MeshData& m, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
          uint32_t color);

/// Appends `src` transformed by yaw (radians, around +Y), uniform scale and
/// translation into `dst`. Normals are rotated (scale is uniform).
void append_transformed(MeshData& dst, const MeshData& src, glm::vec3 translation,
                        float yaw, float scale);

/// Canonical model-space mesh for a scatter species at nominal size (§5
/// silhouettes: oak = ball on a stump, pine = stacked cones, birch = pale slim
/// trunk + small crown, bush = hemisphere lump, stone = faceted boulder).
[[nodiscard]] MeshData build_scatter_mesh(math::ScatterSpecies species);

/// Placeholder structure mesh for a blessed RenderMesh id (1..7, §6 silhouette
/// codes: gable dwelling, porch trader, two-storey L tavern, tall-roof barn,
/// spired shrine, dark portal dungeon entrance, broken-cylinder tower ruin).
/// Returns an empty mesh for unknown ids.
[[nodiscard]] MeshData build_site_mesh(uint32_t mesh_id);

/// Blessed placeholder mesh-id range (see SiteComponents.h on the world side).
inline constexpr uint32_t SITE_MESH_ID_FIRST = 1;
inline constexpr uint32_t SITE_MESH_ID_LAST = 7;

} // namespace dfn::render
