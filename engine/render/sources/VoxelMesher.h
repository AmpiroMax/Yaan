/*
Created: 09:08:2026 - 19:38:00
Last updated: 23:08:2026 - 00:30:00
Module: engine/render
File: engine/render/sources/VoxelMesher.h

Responsibility:
- Turns core's VoxelMeshView (the true world surface, including carves —
  tunnels, cave mouths, overhangs) into the renderer's Vertex/index buffers
  with the same splat-weight and UV conventions the heightfield mesher uses,
  so both feed the identical "terrain" shader.

Key items:
- build_voxel_terrain_mesh(VoxelMeshView): pure, deterministic, GPU-free.

Dependencies:
- Uses: core math (VoxelMeshView, VoxelMaterial), TerrainMesher (TerrainMeshData
  and the shared weight packing), IRenderer Vertex, glm.
- Used by: RenderSystem::upload_terrain_voxel; tests.

Notes:
- WHY THIS EXISTS: terrain was drawn from the HEIGHTFIELD while the voxel mesh
  went only to physics. A heightfield is a function of (x, z) and literally
  cannot represent a ceiling, so inside the crag tunnel and the barrows there
  was nothing to draw — a player who walked in "saw the map from the inside".
  That was never a lighting bug; the geometry was never submitted.
- Material -> splat weights mirrors TerrainMesher's SurfaceClass mapping so
  the two sources cannot drift apart visually. Dirt is the carved cave wall
  material and maps to the water-bed (dark) channel, which is the closest
  thing the current atlas has to bare earth.
- Vertex ALPHA carries sky visibility once core supplies it (agreed: ambient
  and moonlight are multiplied by it, so sealed volumes receive no sky light).
  VoxelMeshView has no colour channel, so core exposes it as its own span;
  until it exists this writes 255 (fully open) and the interior is lit exactly
  as it is today — visible, but not yet dark.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure: no GPU, no ECS. Keep the weight/UV conventions in step with
  TerrainMesher — they share a shader.
*/
/*
UPD:
- 09:08:2026 - 19:38:00: Created — render finally draws the voxel surface, so
  carved interiors exist on screen (lead-confirmed user bug: walkable but
  invisible barrow).
- 17:08:2026 - 11:53:47: build_voxel_terrain_mesh принимает поле поверхности (необязательное).
- 17:08:2026 - 11:54:29: build_voxel_terrain_mesh принимает поле поверхности (необязательное).
- 23:08:2026 - 00:30:00: параметр path_classes — та же упаковка, что у TerrainMesher.
*/

#pragma once

#include "engine/core/math/sources/SurfaceField.h"
#include "engine/core/math/sources/VoxelField.h"
#include "engine/render/sources/TerrainMesher.h"

namespace dfn::render {

/// Builds the drawable mesh for one voxel chunk. Empty view -> empty mesh
/// (an entirely solid or entirely empty chunk is normal, not an error).
/// `surface` is OPTIONAL and carries the ground's own properties — today the
/// §8.1 path wear, sampled per vertex from the chunk's field. It is passed
/// because THE DRAWN GROUND IS THIS MESH: a path painted anywhere else would
/// be a second surface over this one, which is exactly the ribbon that used to
/// hover (user, 17.08: «тропинки должны быть свойством земли»).
/// `path_classes` — поле материалов полотна (TerrainMesher.h); nullptr =
/// прежняя 8-битная упаковка износа бит-в-бит. Кодировка обязана быть одной
/// на всех мешерах кадра.
[[nodiscard]] TerrainMeshData build_voxel_terrain_mesh(
    const math::VoxelMeshView& mesh,
    const math::SurfaceFieldView* surface = nullptr,
    const PathClassField* path_classes = nullptr);

} // namespace dfn::render
