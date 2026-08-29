/*
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
///   HEIGHTMAP_STEP = 1 m, resolution is 257 and sample x = 256 of chunk (cx, cz)
///   equals sample x = 0 of chunk (cx + 1, cz) — meshes stitch without cracks.
/// - THE STEP EQUALS VOXEL_SIZE, AND THAT IS A CONTRACT, NOT A COINCIDENCE.
///   The ground the engine draws and collides is a voxel surface on the
///   VOXEL_SIZE lattice, built by sampling THIS field. A step coarser than the
///   voxel puts nodes BETWEEN samples, and whatever the reconstruction filter
///   writes there is relief nobody generated. Measured on seed-1 while the step
///   was 2 m: 74.8 % of a chunk's 66 049 voxel nodes fell between samples, and
///   the invented height stood up to 6.57 m from the generator's own ground
///   (mean 5-7 cm). The requirement is that the step DIVIDE VOXEL_SIZE; equal is
///   the cheapest way to satisfy it, and tests/core/HeightLatticeTests.cpp is
///   the arm that holds it.
/// - Lifetime: valid from the chunk's ChunkLoaded event until AFTER its
///   ChunkUnloaded event has been dispatched (consumers release GPU/physics
///   resources in the unload handler, before the memory is freed).
struct HeightFieldView {
    glm::ivec2 chunk_coord{0, 0};      ///< Chunk grid coordinate.
    glm::vec2 origin{0.0f, 0.0f};      ///< World-space (x, z) of sample (0, 0), meters.
    uint32_t resolution = 0;           ///< Samples per side (HEIGHTMAP_RESOLUTION = 257).
    float step = 0.0f;                 ///< Meters between samples (HEIGHTMAP_STEP = 1.0).
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
