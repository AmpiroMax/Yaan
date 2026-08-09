/*
Created: 09:08:2026 - 16:00:00
Last updated: 09:08:2026 - 17:36:42
Module: engine/world
File: engine/world/sources/VoxelVolume.cpp

Responsibility:
- Builds a chunk's quantized SDF + material grid from its decoded heightmap
  and P3 surface classes.

Key items:
- build_voxel_volume.

Dependencies:
- Uses: VoxelVolume.h, config.
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Heights come from the chunk's own heightmap, bilinearly interpolated to the
  voxel grid. Never re-sample the macro field here (15x slower, and it would
  let the voxel surface drift from the heightfield the rest of the engine
  still queries).
- Deterministic: fixed loop order, integer quantization, no rng.
*/
/*
UPD:
- 09:08:2026 - 16:00:00: Created — volume construction for the 3D stage.
- 09:08:2026 - 16:30:44: Representation swap: volume built from the chunk's own heightmap; border nodes emulate the NEIGHBOUR's heightmap (quantized 2 m lattice + bilinear) rather than sampling the continuous field, which disagreed by up to 0.30 m mid-cell and showed as a seam.
- 09:08:2026 - 16:47:51: P7: slab extended to reach carved volumes (they sit below the surface band), per-column band widened over carves, CSG subtraction d = max(terrain, -carve).
- 09:08:2026 - 17:36:42: §6.2: derived corridors included in the band widening and the CSG subtraction.
*/

#include "engine/world/sources/VoxelVolume.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenCarve.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {

constexpr float VOXEL = static_cast<float>(config::VOXEL_SIZE);
constexpr float BAND = static_cast<float>(config::VOXEL_SDF_BAND);
constexpr float CHUNK_SIZE_M = static_cast<float>(config::CHUNK_SIZE);
constexpr uint32_t RES = static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION);
constexpr float STEP_M = static_cast<float>(config::HEIGHTMAP_STEP);
// Depth bands below the surface (meters): the surface class skins the top,
// then soil, then bedrock. Carved cave walls read as Dirt/Rock accordingly.
constexpr float SKIN_DEPTH = 0.4f;
constexpr float SOIL_DEPTH = 2.5f;

/// Quantizes a height exactly as the heightmap encoder does, so a border node
/// sampled analytically equals the neighbour's stored sample bit for bit.
float quantize_like_heightmap(float h) {
    constexpr float MAX_H = static_cast<float>(config::WORLDGEN_MAX_HEIGHT);
    const float raw = std::round(std::clamp(h, 0.0f, MAX_H) / MAX_H * 65535.0f);
    return raw * (MAX_H / 65535.0f);
}

/// Height for a node PAST this chunk's heightmap. It must equal what the
/// neighbour computes there, and the neighbour interpolates its own quantized
/// heightmap — so this samples the analytic field on the 2 m heightmap lattice,
/// quantizes each sample, and interpolates. Sampling the continuous field
/// directly would be "more accurate" and WRONG: it disagrees with the
/// neighbour by up to 0.3 m mid-cell, which is exactly a visible seam.
float border_node_height(const BorderHeightSampler& sample, glm::vec2 world_pos) {
    const float gx = std::floor(world_pos.x / STEP_M) * STEP_M;
    const float gz = std::floor(world_pos.y / STEP_M) * STEP_M;
    const float tx = (world_pos.x - gx) / STEP_M;
    const float tz = (world_pos.y - gz) / STEP_M;
    const float h00 = quantize_like_heightmap(sample({gx, gz}));
    const float h10 = quantize_like_heightmap(sample({gx + STEP_M, gz}));
    const float h01 = quantize_like_heightmap(sample({gx, gz + STEP_M}));
    const float h11 = quantize_like_heightmap(sample({gx + STEP_M, gz + STEP_M}));
    const float a = h00 + (h10 - h00) * tx;
    const float b = h01 + (h11 - h01) * tx;
    return a + (b - a) * tz;
}

/// Bilinear height at a voxel node from the chunk's 2 m heightmap. Edge nodes
/// land exactly on heightmap samples, so neighbouring chunks agree.
float height_at_node(const Heightmap& hm, int32_t vx, int32_t vz) {
    const float fx = std::clamp(static_cast<float>(vx) * VOXEL / STEP_M, 0.0f,
                                static_cast<float>(RES - 1));
    const float fz = std::clamp(static_cast<float>(vz) * VOXEL / STEP_M, 0.0f,
                                static_cast<float>(RES - 1));
    const uint32_t x0 = static_cast<uint32_t>(fx);
    const uint32_t z0 = static_cast<uint32_t>(fz);
    const uint32_t x1 = std::min(x0 + 1, RES - 1);
    const uint32_t z1 = std::min(z0 + 1, RES - 1);
    const float tx = fx - static_cast<float>(x0);
    const float tz = fz - static_cast<float>(z0);
    const float h00 = hm.height_at(x0, z0);
    const float h10 = hm.height_at(x1, z0);
    const float h01 = hm.height_at(x0, z1);
    const float h11 = hm.height_at(x1, z1);
    const float a = h00 + (h10 - h00) * tx;
    const float b = h01 + (h11 - h01) * tx;
    return a + (b - a) * tz;
}

/// Surface class at a voxel node, nearest-sample (materials are categorical —
/// interpolating them would invent classes that do not exist).
math::VoxelMaterial surface_material(const SurfaceData& surface, int32_t vx, int32_t vz) {
    const uint32_t sx = std::min(
        static_cast<uint32_t>(std::lround(static_cast<float>(vx) * VOXEL / STEP_M)), RES - 1);
    const uint32_t sz = std::min(
        static_cast<uint32_t>(std::lround(static_cast<float>(vz) * VOXEL / STEP_M)), RES - 1);
    const auto cls = static_cast<math::SurfaceClass>(
        surface.surface_class[static_cast<std::size_t>(sz) * RES + sx]);
    switch (cls) {
    case math::SurfaceClass::Rock:
        return math::VoxelMaterial::Rock;
    case math::SurfaceClass::Sand:
        return math::VoxelMaterial::Sand;
    case math::SurfaceClass::WaterBed:
        return math::VoxelMaterial::Dirt; // river and lake beds
    case math::SurfaceClass::GrassRockBlend:
    case math::SurfaceClass::Grass:
    default:
        return math::VoxelMaterial::Grass;
    }
}

} // namespace

VoxelVolume build_voxel_volume(const Chunk& chunk, const BorderHeightSampler& border_height,
                               const TestbedLayout& layout,
                               std::span<const CarveCorridor> extra_carves) {
    VoxelVolume v;
    v.voxel = VOXEL;
    v.band = BAND;
    v.origin = glm::vec2{static_cast<float>(chunk.coord.x) * CHUNK_SIZE_M,
                         static_cast<float>(chunk.coord.z) * CHUNK_SIZE_M};
    // +1 for the shared node plane, +1 more for the seam cell (see header).
    v.nx = static_cast<int32_t>(CHUNK_SIZE_M / VOXEL) + 2;
    v.nz = v.nx;
    const int32_t own_nodes = static_cast<int32_t>(CHUNK_SIZE_M / VOXEL) + 1;

    // Node heights first: they set the slab and are needed per column anyway.
    std::vector<float> heights(static_cast<std::size_t>(v.nx) * v.nz);
    float hmin = 1e9f;
    float hmax = -1e9f;
    for (int32_t z = 0; z < v.nz; ++z) {
        for (int32_t x = 0; x < v.nx; ++x) {
            // Inside the chunk: interpolate its own heightmap. The one node
            // past it: sample analytically and quantize identically, which is
            // exactly what the neighbour stores there.
            const bool own = x < own_nodes && z < own_nodes;
            const float h =
                own ? height_at_node(chunk.heightmap, x, z)
                    : border_node_height(border_height,
                                         v.origin
                                             + glm::vec2{static_cast<float>(x) * VOXEL,
                                                         static_cast<float>(z) * VOXEL});
            heights[static_cast<std::size_t>(z) * v.nx + x] = h;
            hmin = std::min(hmin, h);
            hmax = std::max(hmax, h);
        }
    }

    // Slab: the vertical range the surface passes through, plus band margin.
    // Snapped to the voxel lattice in WORLD space so neighbouring chunks share
    // node planes exactly (their meshes must meet without a seam).
    float slab_lo = hmin - BAND - VOXEL;
    float slab_hi = hmax + BAND + VOXEL;
    // Carved volumes live BELOW the surface band; the slab has to reach them
    // or the tunnel simply would not exist in the field.
    const bool carving = has_carves(layout) || !extra_carves.empty();
    if (carving) {
        for (int32_t z = 0; z < v.nz; ++z) {
            for (int32_t x = 0; x < v.nx; ++x) {
                const auto [clo, chi] = carve_column_range(
                    layout, extra_carves,
                    v.origin + glm::vec2{static_cast<float>(x) * VOXEL,
                                         static_cast<float>(z) * VOXEL});
                if (clo > chi) {
                    continue;
                }
                slab_lo = std::min(slab_lo, clo - BAND - VOXEL);
                slab_hi = std::max(slab_hi, chi + BAND + VOXEL);
            }
        }
    }
    const float lo = std::floor(slab_lo / VOXEL) * VOXEL;
    const float hi = std::ceil(slab_hi / VOXEL) * VOXEL;
    v.y0 = lo;
    v.ny = static_cast<int32_t>(std::lround((hi - lo) / VOXEL)) + 1;

    const std::size_t count = static_cast<std::size_t>(v.nx) * v.nz * v.ny;
    v.sdf.assign(count, 0);
    v.material.assign(count, static_cast<uint8_t>(math::VoxelMaterial::Air));
    v.band_lo.assign(static_cast<std::size_t>(v.nx) * v.nz, 0);
    v.band_hi.assign(static_cast<std::size_t>(v.nx) * v.nz, 0);
    v.column_surface.assign(static_cast<std::size_t>(v.nx) * v.nz, 0.0f);
    v.column_skin.assign(static_cast<std::size_t>(v.nx) * v.nz,
                         static_cast<uint8_t>(math::VoxelMaterial::Grass));

    constexpr int8_t SOLID_SAT = -127;
    constexpr int8_t AIR_SAT = 127;
    for (int32_t z = 0; z < v.nz; ++z) {
        for (int32_t x = 0; x < v.nx; ++x) {
            const float surface = heights[static_cast<std::size_t>(z) * v.nx + x];
            const math::VoxelMaterial skin = surface_material(
                chunk.surface, std::min(x, own_nodes - 1), std::min(z, own_nodes - 1));

            // Only the band around this column's surface varies; everything
            // below is saturated solid rock, everything above saturated air.
            int32_t lo = std::clamp(
                static_cast<int32_t>(std::floor((surface - BAND - v.y0) / VOXEL)) - 1, 0,
                v.ny - 1);
            int32_t hi = std::clamp(
                static_cast<int32_t>(std::ceil((surface + BAND - v.y0) / VOXEL)) + 1, 0,
                v.ny - 1);
            const glm::vec2 column_xz =
                v.origin + glm::vec2{static_cast<float>(x) * VOXEL,
                                     static_cast<float>(z) * VOXEL};
            bool carved_column = false;
            if (carving) {
                const auto [clo, chi] = carve_column_range(layout, extra_carves, column_xz);
                if (clo <= chi) {
                    carved_column = true;
                    lo = std::min(lo, std::clamp(static_cast<int32_t>(
                                                     std::floor((clo - BAND - v.y0) / VOXEL)),
                                                 0, v.ny - 1));
                    hi = std::max(hi, std::clamp(static_cast<int32_t>(
                                                     std::ceil((chi + BAND - v.y0) / VOXEL)),
                                                 0, v.ny - 1));
                }
            }
            v.band_lo[v.column(x, z)] = lo;
            v.band_hi[v.column(x, z)] = hi;
            v.column_surface[v.column(x, z)] = surface;
            v.column_skin[v.column(x, z)] = static_cast<uint8_t>(skin);

            // Saturated runs are contiguous in this layout, so they fill.
            const std::size_t base = v.index(x, 0, z);
            std::fill(v.sdf.begin() + static_cast<std::ptrdiff_t>(base),
                      v.sdf.begin() + static_cast<std::ptrdiff_t>(base + lo), SOLID_SAT);
            std::fill(v.material.begin() + static_cast<std::ptrdiff_t>(base),
                      v.material.begin() + static_cast<std::ptrdiff_t>(base + lo),
                      static_cast<uint8_t>(math::VoxelMaterial::Rock));
            for (int32_t y = lo; y <= hi; ++y) {
                const float wy = v.y0 + static_cast<float>(y) * VOXEL;
                float d = wy - surface; // negative below the surface
                if (carved_column) {
                    // CSG subtraction: air wins over rock.
                    d = std::max(d, -carve_distance(layout, extra_carves,
                                                    {column_xz.x, wy, column_xz.y}));
                }
                const std::size_t i = v.index(x, y, z);
                v.sdf[i] = static_cast<int8_t>(
                    std::lround(std::clamp(d / BAND, -1.0f, 1.0f) * 127.0f));
                if (d < 0.0f) {
                    const float depth = -d;
                    v.material[i] = static_cast<uint8_t>(
                        depth < SKIN_DEPTH  ? skin
                        : depth < SOIL_DEPTH ? math::VoxelMaterial::Dirt
                                             : math::VoxelMaterial::Rock);
                }
            }
            std::fill(v.sdf.begin() + static_cast<std::ptrdiff_t>(base + hi + 1),
                      v.sdf.begin() + static_cast<std::ptrdiff_t>(base + v.ny), AIR_SAT);
        }
    }
    return v;
}

} // namespace dfn::world
