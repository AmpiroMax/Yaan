/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 22:01:04
Module: engine/render
File: engine/render/sources/TerrainMesher.h

Responsibility:
- Terrain meshing: turns a chunk's HeightFieldView (agreed core<->render
  contract) into the frozen IRenderer Vertex/index arrays, crack-free across
  chunk borders (shared edge rows).

Key items:
- TerrainMeshData; build_terrain_mesh().

Dependencies:
- Uses: engine/core/math (HeightFieldView), engine/platform/render (Vertex).
- Used by: RenderSystem::upload_terrain, tests (deterministic, GPU-free).

Notes:
- Vertex color carries SPLAT WEIGHTS since stage 3b (contract with
  fs_terrain.sc): R = sand, G = rock, B = water-bed, A = reserved (255).
  With a SurfaceFieldView the weights come from core's surface_class ONLY —
  the design truth (LANDSCAPE §3.3/§4); render never re-derives material
  bands from raw dist/height fields (design ruling, feature-requests batch —
  the removed dryness/dirt band painted 60 m brown washes over Grass).
  Without a surface view weights fall back to slope-only rock. Pure function
  of the inputs — deterministic, unit-tested.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this a pure function: no GPU calls, no ECS access.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial contract + implementation.
- 09:08:2026 - 11:57:20: Stage 3b — SurfaceFieldView overload; vertex color
  re-purposed from tint to splat weights (shader contract updated in step).
- 09:08:2026 - 14:11:37: Dryness/dirt channel removed (design ruling): splat
  keys off core's surface_class only; alpha reserved.
- 09:08:2026 - 22:01:04: LOD support. (1) UVs are WORLD-REFERENCED (world xz /
  CHUNK_SIZE) instead of 0..1 across the field. For a field whose origin sits
  on the 128 m node grid this is identical to the old formula — the difference
  is a whole number of tile repeats — but under the old formula a 1..8 km LOD
  node stretched one texture set across the entire node. A test pins the
  equality rather than asserting it in prose. (2) TerrainMeshOptions::
  skirt_depth_m appends a vertical apron to the four borders, which is what
  hides the T-junction crack between two adjacent LOD levels.
*/

#pragma once

#include "engine/core/math/sources/HeightField.h"
#include "engine/core/math/sources/SurfaceField.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <vector>

namespace dfn::render {

/// CPU-side terrain mesh for one chunk, ready for IRenderer::create_mesh.
struct TerrainMeshData {
    std::vector<platform::Vertex> vertices; // resolution^2, row-major like the field
    std::vector<uint32_t> indices;          // 6 * (resolution-1)^2
};

/// Triangulates `field` (world-space positions from origin/step, heights via
/// the agreed decode formula). Normals by central differences; UVs span 0..1
/// across the chunk; vertex colors encode the splat weights (see Notes).
/// Neighbor chunks share edge samples by contract, so meshes stitch without
/// cracks. `surface` (same chunk's SurfaceFieldView, may be nullptr) supplies
/// design-truth sand/rock/water-bed weights; nullptr keeps slope-only rock.
[[nodiscard]] TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field,
                                                 const math::SurfaceFieldView* surface);

/// Stage-2 compatible form: slope-only splat weights (no surface data).
[[nodiscard]] TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field);

/// Extra meshing choices. Everything here defaults to the chunk behaviour, so
/// the two calls above are exactly `build_terrain_mesh(field, surface, {})`.
struct TerrainMeshOptions {
    /// Metres of vertical apron hung from the four border edges, 0 = none.
    /// A skirt exists ONLY to hide the T-junction crack where this mesh meets
    /// a neighbour meshed on a different lattice — it is never visible ground,
    /// so it is deliberately allowed to be too deep rather than too shallow.
    /// Derive it with lod_skirt_depth_m(); chunks share a sample lattice with
    /// their neighbours by contract and need none.
    float skirt_depth_m = 0.0f;
};

/// Full form. Skirt vertices are appended AFTER the resolution^2 grid vertices,
/// so `vertices[z * resolution + x]` keeps addressing the surface and existing
/// callers that index the grid are unaffected.
[[nodiscard]] TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field,
                                                 const math::SurfaceFieldView* surface,
                                                 const TerrainMeshOptions& options);

/// The largest height difference between two ADJACENT samples along the four
/// border rows of `field`, in metres. This is the measurement that sizes a
/// skirt (see lod_skirt_depth_m) — measured from the field the node was built
/// from, never assumed.
[[nodiscard]] float terrain_border_max_step_m(const math::HeightFieldView& field);

} // namespace dfn::render
