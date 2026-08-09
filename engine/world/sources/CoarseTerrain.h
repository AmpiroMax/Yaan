/*
Created: 09:08:2026 - 23:49:27
Last updated: 09:08:2026 - 23:52:50
Module: engine/world
File: engine/world/sources/CoarseTerrain.h

Responsibility:
- The STREAMING half of terrain level of detail: the coarse quadtree node — its
  identity on a fixed world grid, its sample lattice, and the builder that
  fills one from the same field, with the same quantization, that the chunk
  builder uses. Render decides WHICH nodes exist; this decides what they hold.

Key items:
- CoarseNode (level + node coords), the ladder (voxel size per level), node
  geometry helpers.
- CoarseNodeData: owned heights + surface arrays, with the two cross-zone views.
- build_coarse_rows(): incremental fill, so a node costs a slice of an update
  instead of a whole one.

Dependencies:
- Uses: Chunk.h (the shared quantization), Worldgen.h (the field), core math.
- Used by: ChunkManager (residency + budget), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- A COARSE NODE IS A HeightFieldView. 129 samples with the shared edge row,
  step = the level's voxel size, origin = node coord * node size. That is a
  frozen cross-zone agreement with render (Rule 26) — do not add a second mesh
  format, and do not change the ladder without render.
- Sample the SAME QUANTIZED field the chunk builder samples. The heights are
  bit-identical wherever the two lattices coincide only because both call
  quantize_height() on the same pure terrain_height(); anything that samples
  "the continuous field instead" reintroduces the 0.30 m border step the chunk
  seam already paid for, at 64 m scale.
*/
/*
UPD:
- 09:08:2026 - 23:49:27: Created — coarse node identity, ladder, incremental
  builder (core's half of the LOD contract with render).
- 09:08:2026 - 23:49:27: The ladder now reads dfn::config (the lead landed the
  eight NUMBERS rows); core's local copy deleted. COARSE_NODE_ROW_BUDGET
  derived from measurement and requested as the last row.
- 09:08:2026 - 23:52:50: COARSE_NODE_ROW_BUDGET reads dfn::config too — the lead landed the row.
*/

#pragma once

#include "engine/core/config/sources/Constants.h"
#include "engine/core/math/sources/HeightField.h"
#include "engine/core/math/sources/SurfaceField.h"
#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/Worldgen.h"

#include <cstdint>
#include <glm/vec2.hpp>
#include <vector>

namespace dfn::world {

/// LOD ladder — READ FROM NUMBERS.md, not held here. Render selects the nodes
/// and this zone builds them, so the ladder is a number two zones must agree
/// on and therefore belongs to neither (Rule 35); the rows landed as
/// LOD_LEVEL_COUNT / LOD_NODE_VOXELS / LOD_VOXEL_SIZE_L0..L5.
/// tests/core/LodSeamTests.cpp links dfn_render and pins the two tables equal,
/// so a zone quietly keeping its own copy again breaks a build rather than a
/// horizon.
inline constexpr uint32_t COARSE_LEVEL_COUNT =
    static_cast<uint32_t>(config::LOD_LEVEL_COUNT);
inline constexpr float COARSE_VOXEL_SIZE_M[COARSE_LEVEL_COUNT] = {
    static_cast<float>(config::LOD_VOXEL_SIZE_L0),
    static_cast<float>(config::LOD_VOXEL_SIZE_L1),
    static_cast<float>(config::LOD_VOXEL_SIZE_L2),
    static_cast<float>(config::LOD_VOXEL_SIZE_L3),
    static_cast<float>(config::LOD_VOXEL_SIZE_L4),
    static_cast<float>(config::LOD_VOXEL_SIZE_L5)};

/// Node side in VOXELS, constant across levels — this is what keeps the
/// triangle budget per node constant while the node's metre size grows.
inline constexpr uint32_t COARSE_NODE_VOXELS =
    static_cast<uint32_t>(config::LOD_NODE_VOXELS);

/// Samples per side: the voxel count plus the SHARED EDGE ROW, exactly as a
/// chunk is 128 cells / 129 samples. The shared row is what lets two nodes of
/// the same level stitch without a crack.
inline constexpr uint32_t COARSE_NODE_RESOLUTION = COARSE_NODE_VOXELS + 1;

/// One coarse quadtree node. `x`/`z` are node coordinates AT THAT LEVEL, so
/// world origin = coord * node size and identity is exact integer comparison.
/// Mirrors render::LodNode field for field; the app ferry converts between the
/// two (siblings in the DAG cannot include each other, Rule 1).
///
/// The grid is FIXED IN WORLD SPACE and not relative to the generated extent:
/// growing the world from 2x2 km to 10x10 km must renumber nothing that is
/// already cached, which it cannot do if the origin of numbering moves.
struct CoarseNode {
    uint8_t level = 0;
    int32_t x = 0;
    int32_t z = 0;

    [[nodiscard]] constexpr bool operator==(const CoarseNode& o) const {
        return level == o.level && x == o.x && z == o.z;
    }
    [[nodiscard]] constexpr bool operator!=(const CoarseNode& o) const { return !(*this == o); }
};

/// Sample rows of the node under construction advanced per ChunkManager
/// update — NUMBERS.md `COARSE_NODE_ROW_BUDGET`, the same reason
/// CHUNK_LOAD_BUDGET is a row and not a literal: it is the knob between "far
/// terrain appears" and "the frame hitches", and the last time that knob was
/// invisible it was a multi-second freeze.
///
/// DERIVED, measured on the seed-1 testbed: a row of 129 samples costs 0.087 ms
/// (a whole node is 11.3 ms). update() runs once per SIM TICK, so a budget of
/// N rows costs N * 0.087 ms every tick. Capping far terrain at a tenth of one
/// core at the tick rate gives 0.1 / SIM_TICK_RATE = 1.67 ms, i.e. 19 rows;
/// 16 is that rounded down to a divisor of the node's 128 cells, and measures
/// 1.39 ms per tick (8.4% of one core). A node lands in 9 ticks.
inline constexpr uint32_t COARSE_NODE_ROW_BUDGET =
    static_cast<uint32_t>(config::COARSE_NODE_ROW_BUDGET);

/// Distance between samples of a node at this level, in metres.
[[nodiscard]] float coarse_voxel_size_m(uint8_t level);

/// Node side length in metres (= voxel size * COARSE_NODE_VOXELS).
[[nodiscard]] float coarse_node_size_m(uint8_t level);

/// World-space (x, z) of the node's sample (0, 0).
[[nodiscard]] glm::vec2 coarse_node_origin_m(const CoarseNode& node);

/// A 64-bit key for a node id — exact integer identity, never a float compare.
[[nodiscard]] uint64_t coarse_node_key(const CoarseNode& node);

/// One built (or partially built) coarse node: owned samples plus the two
/// cross-zone views over them.
struct CoarseNodeData {
    CoarseNode node;
    std::vector<uint16_t> heights;      ///< COARSE_NODE_RESOLUTION^2, row-major
    std::vector<float> dist_to_water;
    std::vector<float> water_surface;
    std::vector<uint8_t> surface_class;
    /// Rows filled so far. The node is complete at COARSE_NODE_RESOLUTION.
    uint32_t rows_done = 0;

    [[nodiscard]] bool complete() const { return rows_done >= COARSE_NODE_RESOLUTION; }

    /// The agreed cross-zone views. `chunk_coord` carries the NODE coordinates
    /// (there is no chunk here); consumers key off the node id they asked for.
    [[nodiscard]] math::HeightFieldView height_view() const;
    [[nodiscard]] math::SurfaceFieldView surface_view() const;
};

/// Allocates the arrays for `node` and resets it to zero rows done.
[[nodiscard]] CoarseNodeData begin_coarse_node(const CoarseNode& node);

/// Fills up to `row_count` further sample rows of `data` from the generated
/// field, advancing rows_done. Returns the number of rows actually filled (0
/// when the node is already complete).
///
/// INCREMENTAL ON PURPOSE. A node is 129x129 samples and each sample costs a
/// height plus the four neighbour heights the position-pure slope needs, so a
/// whole node is several times a chunk. Admitting one per update would be the
/// multi-second freeze again wearing a different hat; a row is ~0.4% of it.
uint32_t build_coarse_rows(const WorldGenContext& ctx, CoarseNodeData& data,
                           uint32_t row_count);

/// Builds a whole node in one call. For tests and tools — the streaming path
/// uses build_coarse_rows.
[[nodiscard]] CoarseNodeData build_coarse_node(const WorldGenContext& ctx,
                                               const CoarseNode& node);

} // namespace dfn::world
