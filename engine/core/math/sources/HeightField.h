/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/core/math
File: engine/core/math/sources/HeightField.h

Responsibility:
- The agreed cross-zone heightfield view type: engine/world fills it from chunk
  data; engine/render meshes it; engine/physics feeds it to terrain collision.
  Lives in core because world, render and physics are DAG siblings (Rule 1) and
  all three may include core.

Key items:
- HeightFieldView: non-owning POD view over one chunk's raw height samples.

Dependencies:
- Uses: glm, <span>, <cstdint>.
- Used by: world::ChunkManager (producer), engine/render terrain mesher and
  engine/physics terrain collision (consumers).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- BOUNDARY CONTRACT agreed core<->render<->sim (Rule 26, stage 1); field set,
  layout and height formula are FROZEN for the stage — changes only via group sync.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — HeightFieldView agreed with render
  (meshing) and sim (terrain collision); formula height = offset + raw * scale.
*/

#pragma once

#include <cstdint>
#include <glm/vec2.hpp>
#include <span>

namespace dfn::math {

/// Non-owning view over one terrain chunk's height samples.
///
/// Conventions (agreed, frozen for stage 1):
/// - Right-handed, Y up, +X east, +Z south.
/// - Row-major, x fastest: sample (x, z) is heights[z * resolution + x].
/// - height_m = height_offset + heights[i] * height_scale, where height_scale is
///   METERS PER RAW UNIT (worldgen precomputes (chunk_max - chunk_min) / 65535;
///   no divide at runtime).
/// - Edge rows are shared with neighbors: with CHUNK_SIZE = 256 m and
///   HEIGHTMAP_STEP = 2 m, resolution is 129 and sample x = 128 of chunk (cx, cz)
///   equals sample x = 0 of chunk (cx + 1, cz) — meshes stitch without cracks.
/// - Lifetime: valid from the chunk's ChunkLoaded event until AFTER its
///   ChunkUnloaded event has been dispatched (consumers release GPU/physics
///   resources in the unload handler, before the memory is freed).
struct HeightFieldView {
    glm::ivec2 chunk_coord{0, 0};      ///< Chunk grid coordinate.
    glm::vec2 origin{0.0f, 0.0f};      ///< World-space (x, z) of sample (0, 0), meters.
    uint32_t resolution = 0;           ///< Samples per side (HEIGHTMAP_RESOLUTION = 129).
    float step = 0.0f;                 ///< Meters between samples (HEIGHTMAP_STEP = 2.0).
    std::span<const uint16_t> heights; ///< resolution * resolution raw samples.
    float height_scale = 0.0f;         ///< Meters per raw unit.
    float height_offset = 0.0f;        ///< Meters (chunk minimum height).

    /// Decoded height in meters at integer sample (x, z). Bounds unchecked.
    [[nodiscard]] float height_at(uint32_t x, uint32_t z) const {
        return height_offset
             + static_cast<float>(heights[z * resolution + x]) * height_scale;
    }
};

} // namespace dfn::math
