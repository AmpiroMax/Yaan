/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 14:11:37
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
