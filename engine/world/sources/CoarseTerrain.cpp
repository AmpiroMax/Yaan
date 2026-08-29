/*
Module: engine/world
File: engine/world/sources/CoarseTerrain.cpp

Responsibility:
- Coarse LOD node geometry helpers and the incremental node builder.

Key items:
- coarse_voxel_size_m / coarse_node_size_m / coarse_node_origin_m / key.
- CoarseNodeData views, begin_coarse_node, build_coarse_rows.

Dependencies:
- Uses: CoarseTerrain.h, Worldgen (terrain_height / terrain_slope /
  classify_surface), Chunk.h (quantize_height), WorldgenMacro/Hydrology.
- Used by: ChunkManager, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every height goes through quantize_height() and every class through
  classify_surface() with the POSITION-PURE slope. Both are the reason a coarse
  sample equals a chunk sample exactly where the lattices meet; a local
  shortcut here is a seam at 64 m scale.
*/

#include "engine/world/sources/CoarseTerrain.h"

#include "engine/world/sources/WorldgenHydrology.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenSites.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {

constexpr uint32_t RES = COARSE_NODE_RESOLUTION;
constexpr std::size_t SAMPLE_COUNT = static_cast<std::size_t>(RES) * RES;

[[nodiscard]] uint8_t clamp_level(uint8_t level) {
    return level < COARSE_LEVEL_COUNT ? level : static_cast<uint8_t>(COARSE_LEVEL_COUNT - 1);
}

} // namespace

float coarse_voxel_size_m(uint8_t level) {
    return COARSE_VOXEL_SIZE_M[clamp_level(level)];
}

float coarse_node_size_m(uint8_t level) {
    return coarse_voxel_size_m(level) * static_cast<float>(COARSE_NODE_VOXELS);
}

glm::vec2 coarse_node_origin_m(const CoarseNode& node) {
    const float size = coarse_node_size_m(node.level);
    return {static_cast<float>(node.x) * size, static_cast<float>(node.z) * size};
}

uint64_t coarse_node_key(const CoarseNode& node) {
    // 8 bits level | 28 bits x | 28 bits z. Both coordinate fields are masked,
    // so a NEGATIVE coordinate stays inside its own field instead of flooding
    // the level bits and aliasing two different nodes onto one key — the world
    // grid is signed and the origin of numbering is world zero, not the world's
    // corner, so negative node coords are ordinary.
    constexpr uint64_t FIELD = 0x0FFFFFFFull;
    return (static_cast<uint64_t>(node.level) << 56)
         | ((static_cast<uint64_t>(static_cast<uint32_t>(node.x)) & FIELD) << 28)
         | (static_cast<uint64_t>(static_cast<uint32_t>(node.z)) & FIELD);
}

math::HeightFieldView CoarseNodeData::height_view() const {
    math::HeightFieldView view;
    view.chunk_coord = {node.x, node.z};
    view.origin = coarse_node_origin_m(node);
    view.resolution = RES;
    view.step = coarse_voxel_size_m(node.level);
    view.heights = heights;
    view.height_scale = HEIGHT_QUANT_SCALE;
    view.height_offset = HEIGHT_QUANT_OFFSET;
    return view;
}

math::SurfaceFieldView CoarseNodeData::surface_view() const {
    math::SurfaceFieldView view;
    view.chunk_coord = {node.x, node.z};
    view.origin = coarse_node_origin_m(node);
    view.resolution = RES;
    view.step = coarse_voxel_size_m(node.level);
    view.dist_to_water = dist_to_water;
    view.water_surface = water_surface;
    view.surface_class = surface_class;
    return view;
}

CoarseNodeData begin_coarse_node(const CoarseNode& node) {
    CoarseNodeData data;
    data.node = node;
    data.node.level = clamp_level(node.level);
    data.heights.assign(SAMPLE_COUNT, 0);
    data.dist_to_water.assign(SAMPLE_COUNT, 0.0f);
    data.water_surface.assign(SAMPLE_COUNT, math::NO_WATER);
    data.surface_class.assign(SAMPLE_COUNT, static_cast<uint8_t>(math::SurfaceClass::Grass));
    data.rows_done = 0;
    return data;
}

uint32_t build_coarse_rows(const WorldGenContext& ctx, CoarseNodeData& data,
                           uint32_t row_count) {
    if (data.complete() || row_count == 0) {
        return 0;
    }
    const uint32_t first = data.rows_done;
    const uint32_t last = std::min(RES, first + row_count);
    const glm::vec2 origin = coarse_node_origin_m(data.node);
    const float step = coarse_voxel_size_m(data.node.level);
    const TestbedLayout& layout = ctx.params.layout;

    for (uint32_t z = first; z < last; ++z) {
        for (uint32_t x = 0; x < RES; ++x) {
            const glm::vec2 world = origin
                                  + glm::vec2{static_cast<float>(x) * step,
                                              static_cast<float>(z) * step};
            const std::size_t i = static_cast<std::size_t>(z) * RES + x;

            // THE PASS STACK, VIA ITS ONE DEFINITION (compose_passes). This
            // used to open-code "water -> entrance works -> pads -> clamp" and
            // assert in a comment that terrain_height() was the same chain. It
            // was, until the forest stand's branch (LF-8 erosion, then the path
            // flatten) landed in terrain_height and this copy was not told —
            // at which point the coarse nodes were building a DIFFERENT
            // TERRAIN from the chunks they have to meet, and the exact-seam
            // contract (a coarse sample equals a chunk sample bit for bit
            // wherever the lattices coincide) was quietly false on that stand.
            // A comment claiming two things are the same is not a mechanism
            // that makes them the same; calling one function is.
            const float macro = macro_height(ctx.params.seed, layout, world);
            const WaterSample water = water_at(ctx.hydrology, layout, world, macro);
            const float h = compose_passes(ctx, world, macro, water);

            data.heights[i] = quantize_height(h);

            const bool covered =
                water.water_surface != math::NO_WATER && h < water.water_surface;
            data.dist_to_water[i] = water.dist_to_water;
            data.water_surface[i] = covered ? water.water_surface : math::NO_WATER;
            // POSITION-PURE slope (+-HEIGHTMAP_STEP), not the node's own
            // spacing: see terrain_slope's comment — a per-level slope would
            // make render's cross-fade change the material as well as the
            // geometry.
            data.surface_class[i] = static_cast<uint8_t>(
                classify_surface(layout, world, h, water, terrain_slope(ctx, world),
                                 &ctx.params.composed_relief));
        }
    }
    data.rows_done = last;
    // NOTE for whoever looks for a border-step measurement here: there is not
    // one, on purpose. Render sizes each node's skirt from its own
    // terrain_border_max_step_m() over the view it was handed, so a second
    // measurement on this side would be a second copy of one number with
    // nobody reading it (Rule 35 in its cheapest form: do not create the
    // second copy at all).
    return last - first;
}

CoarseNodeData build_coarse_node(const WorldGenContext& ctx, const CoarseNode& node) {
    CoarseNodeData data = begin_coarse_node(node);
    while (!data.complete()) {
        build_coarse_rows(ctx, data, RES);
    }
    return data;
}

} // namespace dfn::world
