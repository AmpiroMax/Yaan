/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 16:30:44
Module: engine/world
File: engine/world/sources/Chunk.cpp

Responsibility:
- Chunk helpers: world-position to chunk mapping, HeightFieldView construction,
  bilinear height sampling.

Key items:
- chunk_at_position, Heightmap::view, Heightmap::sample_world.

Dependencies:
- Uses: Chunk.h, generated constants.
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- view() must stay byte-consistent with the frozen math::HeightFieldView
  contract (row-major x fastest, origin = sample (0,0) world x/z).
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — implementation.
- 09:08:2026 - 11:05:22: Stage 3b — SurfaceData::view (SurfaceFieldView per
  the render agreement).
- 09:08:2026 - 16:30:44: Representation swap: VoxelSurface::view.
*/

#include "engine/world/sources/Chunk.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {
constexpr float CHUNK_SIZE_M = static_cast<float>(config::CHUNK_SIZE);
constexpr uint32_t RESOLUTION = static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION);
constexpr float STEP_M = static_cast<float>(config::HEIGHTMAP_STEP);
} // namespace

ChunkCoord chunk_at_position(glm::vec2 world_xz) {
    return ChunkCoord{static_cast<int32_t>(std::floor(world_xz.x / CHUNK_SIZE_M)),
                      static_cast<int32_t>(std::floor(world_xz.y / CHUNK_SIZE_M))};
}

math::HeightFieldView Heightmap::view(ChunkCoord coord) const {
    math::HeightFieldView v;
    v.chunk_coord = glm::ivec2{coord.x, coord.z};
    v.origin = glm::vec2{static_cast<float>(coord.x) * CHUNK_SIZE_M,
                         static_cast<float>(coord.z) * CHUNK_SIZE_M};
    v.resolution = RESOLUTION;
    v.step = STEP_M;
    v.heights = std::span<const uint16_t>{samples.data(), samples.size()};
    v.height_scale = height_scale;
    v.height_offset = height_offset;
    return v;
}

math::SurfaceFieldView SurfaceData::view(ChunkCoord coord) const {
    math::SurfaceFieldView v;
    v.chunk_coord = glm::ivec2{coord.x, coord.z};
    v.origin = glm::vec2{static_cast<float>(coord.x) * CHUNK_SIZE_M,
                         static_cast<float>(coord.z) * CHUNK_SIZE_M};
    v.resolution = RESOLUTION;
    v.step = STEP_M;
    v.dist_to_water = std::span<const float>{dist_to_water.data(), dist_to_water.size()};
    v.water_surface = std::span<const float>{water_surface.data(), water_surface.size()};
    v.surface_class = std::span<const uint8_t>{surface_class.data(), surface_class.size()};
    return v;
}

math::VoxelMeshView VoxelSurface::view(ChunkCoord coord) const {
    math::VoxelMeshView v;
    v.chunk_coord = glm::ivec2{coord.x, coord.z};
    v.positions = positions;
    v.normals = normals;
    v.materials = materials;
    v.indices = indices;
    return v;
}

float Heightmap::sample_world(ChunkCoord coord, glm::vec2 world_xz) const {
    const glm::vec2 origin{static_cast<float>(coord.x) * CHUNK_SIZE_M,
                           static_cast<float>(coord.z) * CHUNK_SIZE_M};
    const float max_index = static_cast<float>(RESOLUTION - 1);
    const float fx = std::clamp((world_xz.x - origin.x) / STEP_M, 0.0f, max_index);
    const float fz = std::clamp((world_xz.y - origin.y) / STEP_M, 0.0f, max_index);

    const uint32_t x0 = static_cast<uint32_t>(fx);
    const uint32_t z0 = static_cast<uint32_t>(fz);
    const uint32_t x1 = std::min(x0 + 1, RESOLUTION - 1);
    const uint32_t z1 = std::min(z0 + 1, RESOLUTION - 1);
    const float tx = fx - static_cast<float>(x0);
    const float tz = fz - static_cast<float>(z0);

    const float h00 = height_at(x0, z0);
    const float h10 = height_at(x1, z0);
    const float h01 = height_at(x0, z1);
    const float h11 = height_at(x1, z1);
    const float h0 = h00 + (h10 - h00) * tx;
    const float h1 = h01 + (h11 - h01) * tx;
    return h0 + (h1 - h0) * tz;
}

} // namespace dfn::world
