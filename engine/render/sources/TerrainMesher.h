/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
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
- Ground tint: per-vertex color from height band (grass -> dry brown) and
  slope (steep -> rock grey); the terrain shader applies directional lambert
  on top. Pure function of the input view — deterministic, unit-tested.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this a pure function: no GPU calls, no ECS access.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial contract + implementation.
*/

#pragma once

#include "engine/core/math/sources/HeightField.h"
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
/// across the chunk; vertex colors encode the ground tint. Neighbor chunks
/// share edge samples by contract, so their meshes stitch without cracks.
[[nodiscard]] TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field);

} // namespace dfn::render
