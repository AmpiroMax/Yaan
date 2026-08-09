/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 11:57:20
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
  fs_terrain.sc): R = sand, G = rock, B = water-bed, A = grass<->dirt dryness.
  With a SurfaceFieldView the weights come from core's design-truth data
  (surface_class + dist_to_water per LANDSCAPE §3.3/§4); without one they fall
  back to slope-only rock. Pure function of the inputs — deterministic,
  unit-tested.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this a pure function: no GPU calls, no ECS access.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial contract + implementation.
- 09:08:2026 - 11:57:20: Stage 3b — SurfaceFieldView overload; vertex color
  re-purposed from tint to splat weights (shader contract updated in step).
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

} // namespace dfn::render
