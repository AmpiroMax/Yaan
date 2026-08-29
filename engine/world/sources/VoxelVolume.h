/*
Module: engine/world
File: engine/world/sources/VoxelVolume.h

Responsibility:
- The voxel representation of one chunk: a quantized signed-distance field
  plus a material per voxel, built from the chunk's OWN heightmap and surface
  classes (docs/design/VOXEL_ARCHITECTURE.md, "3D IN ONE STAGE" variant).

Key items:
- VoxelVolume: slab-limited SDF + material grid with sampling helpers.
- build_voxel_volume(): heightmap -> volume (the representation swap).

Dependencies:
- Uses: Chunk.h, core/math/VoxelField.h, config.
- Used by: VoxelMesh (extraction), ChunkManager, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- SIGNED DISTANCE, NOT BOOLEAN FILL. Boolean occupancy stair-steps at 1 m and
  would turn our hills into terraces; the distance value is what lets the
  extractor reconstruct the surface sub-voxel. This was the decisive spike
  finding — do not "simplify" it to a solid flag.
- The volume is built from the chunk's decoded heightmap, NOT by re-sampling
  the macro field per voxel (measured 166 ms vs 11 ms). It also makes the
  extracted surface the SAME surface the heightfield describes, which is what
  makes the swap visually neutral, and preserves edge stitching because
  neighbours share their edge samples exactly.
- The world is NOT destructible (user decision): there is no edit path, no
  re-meshing at runtime. Volumes are generated, extracted once, and dropped.
*/

#pragma once

#include "engine/core/math/sources/VoxelField.h"
#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/TestbedLayout.h"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace dfn::world {

/// One chunk's voxel field. Only the vertical SLAB the surface passes through
/// is stored (plus room for carved volumes), which is the difference between
/// 2.4 MB and 8.2 MB per chunk at 1 m.
struct VoxelVolume {
    float voxel = 0.0f;           ///< VOXEL_SIZE, meters
    float band = 0.0f;            ///< VOXEL_SDF_BAND: the range int8 covers
    glm::vec2 origin{0.0f};       ///< world x/z of sample (0,0)
    float y0 = 0.0f;              ///< world Y of the slab's bottom layer
    int32_t nx = 0, ny = 0, nz = 0;
    std::vector<int8_t> sdf;      ///< quantized signed distance; < 0 is solid
    std::vector<uint8_t> material; ///< math::VoxelMaterial
    /// Per column (x,z): the y range where the field actually varies. Outside
    /// it the value is saturated (-127 solid below, +127 air above), so no
    /// isosurface can cross there and both the builder and the extractor skip
    /// it. This is what keeps a 40-layer slab from costing 40 layers of work.
    std::vector<int32_t> band_lo;
    std::vector<int32_t> band_hi;
    /// Per column: the surface height and the skin material there. Extraction
    /// needs these to give a vertex the material of ITS OWN position, rather
    /// than inheriting one from whichever solid corner happened to be nearest
    /// — on a slope that corner belongs to an uphill column and reads as deep
    /// soil, which painted contour-following dirt stripes across open meadow.
    std::vector<float> column_surface;
    std::vector<uint8_t> column_skin;

    [[nodiscard]] std::size_t column(int32_t x, int32_t z) const {
        return static_cast<std::size_t>(z) * nx + x;
    }

    /// Y IS CONTIGUOUS, deliberately: every consumer walks a column's band
    /// top-to-bottom, so the whole band lands in one cache line run. The
    /// obvious (z,y,x) layout strides y by nx and measured 13x slower in
    /// extraction — this is not a micro-optimisation, it is the difference
    /// between 157 ms and 12 ms per chunk.
    [[nodiscard]] std::size_t index(int32_t x, int32_t y, int32_t z) const {
        return (static_cast<std::size_t>(z) * nx + x) * ny + y;
    }
    /// Signed distance in meters at a grid node (negative = inside solid).
    [[nodiscard]] float distance_at(int32_t x, int32_t y, int32_t z) const {
        return static_cast<float>(sdf[index(x, y, z)]) * (band / 127.0f);
    }
    [[nodiscard]] bool solid_at(int32_t x, int32_t y, int32_t z) const {
        return sdf[index(x, y, z)] < 0;
    }
    [[nodiscard]] glm::vec3 world_at(int32_t x, int32_t y, int32_t z) const {
        return glm::vec3{origin.x + static_cast<float>(x) * voxel,
                         y0 + static_cast<float>(y) * voxel,
                         origin.y + static_cast<float>(z) * voxel};
    }
};

/// Samples terrain height at a world position for the ONE node column past the
/// chunk's own heightmap (see the seam note below). Must return the same value
/// the neighbouring chunk stores, so callers quantize it the same way.
using BorderHeightSampler = std::function<float(glm::vec2)>;

/// Builds the volume for `chunk` (which must already carry its heightmap and
/// surface data). Deterministic.
///
/// SEAM: the volume spans ONE CELL BEYOND the chunk in +x and +z. Surface nets
/// puts a vertex inside each cell and connects vertices of adjacent cells, so
/// without that extra cell the quad strip between two chunks belongs to
/// neither and every chunk border becomes a 1 m crack. With it, the strip is
/// emitted exactly once — by the chunk on the -x/-z side — and the duplicated
/// node plane carries identical values, so the meshes coincide rather than
/// overlap. `border_height` supplies the heights for that extra column.
/// `layout` supplies the P7 carves subtracted from the terrain (pass a layout
/// with no carves to get bare terrain).
/// `extra_carves` are DERIVED corridors (the §6.2 entrance adits), which do
/// not live in the layout because they are generated from measured relief.
[[nodiscard]] VoxelVolume build_voxel_volume(const Chunk& chunk,
                                             const BorderHeightSampler& border_height,
                                             const TestbedLayout& layout,
                                             std::span<const CarveCorridor> extra_carves = {});

} // namespace dfn::world
