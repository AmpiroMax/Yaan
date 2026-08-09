/*
Created: 09:08:2026 - 11:57:20
Last updated: 09:08:2026 - 11:57:20
Module: engine/render
File: engine/render/sources/ScatterBatcher.cpp

Responsibility:
- build_scatter_batches implementation: species mesh cache, world-space
  baking, micro tile assignment and bounding radii.

Key items:
- build_scatter_batches().

Dependencies:
- Uses: ScatterBatcher.h, ProcMesh.
- Used by: dfn_render target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic pure function; covered by ScatterBatcherTests.
*/
/*
UPD:
- 09:08:2026 - 11:57:20: Stage 3b — initial implementation.
*/

#include "engine/render/sources/ScatterBatcher.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace dfn::render {

namespace {

// Sink fraction of the instance scale: hides the downhill gap under a mesh
// placed at the sample-point terrain height on sloped ground.
constexpr float GROUND_SINK_FRAC = 0.12f;

// Conservative horizontal footprint radius (m) of each species' nominal mesh,
// used for micro tile bounding circles (kept in sync with ProcMesh geometry).
float species_radius(math::ScatterSpecies species) {
    switch (species) {
    case math::ScatterSpecies::OakTree: return 4.0f;
    case math::ScatterSpecies::PineTree: return 2.5f;
    case math::ScatterSpecies::BirchTree: return 2.0f;
    case math::ScatterSpecies::Bush: return 1.0f;
    case math::ScatterSpecies::Stone: return 0.5f;
    }
    return 1.0f;
}

bool is_tree(math::ScatterSpecies species) {
    return species == math::ScatterSpecies::OakTree
        || species == math::ScatterSpecies::PineTree
        || species == math::ScatterSpecies::BirchTree;
}

} // namespace

ScatterBatches build_scatter_batches(std::span<const math::ScatterInstance> instances,
                                     glm::vec2 chunk_origin, float chunk_size,
                                     uint32_t micro_tiles_per_axis) {
    ScatterBatches out;
    if (instances.empty() || micro_tiles_per_axis == 0 || chunk_size <= 0.0f) {
        return out;
    }

    // Species meshes built once per call (cheap; caching across calls is the
    // caller's option — batches dominate the cost anyway).
    std::array<MeshData, 5> species_mesh;
    std::array<bool, 5> built{};

    const auto mesh_of = [&](math::ScatterSpecies s) -> const MeshData& {
        const auto i = static_cast<size_t>(s);
        if (!built[i]) {
            species_mesh[i] = build_scatter_mesh(s);
            built[i] = true;
        }
        return species_mesh[i];
    };

    const float tile_size = chunk_size / static_cast<float>(micro_tiles_per_axis);
    const uint32_t n = micro_tiles_per_axis;
    struct TileScratch {
        MeshData mesh;
        float radius = 0.0f;
    };
    std::vector<TileScratch> tiles(static_cast<size_t>(n) * n);

    for (const math::ScatterInstance& inst : instances) {
        const MeshData& src = mesh_of(inst.species);
        if (src.vertices.empty()) {
            continue;
        }
        const glm::vec3 pos{inst.position.x,
                            inst.position.y - GROUND_SINK_FRAC * inst.scale,
                            inst.position.z};
        if (is_tree(inst.species)) {
            append_transformed(out.trees, src, pos, inst.yaw, inst.scale);
            continue;
        }
        // Micro: clamp the tile index so border instances never fall outside.
        const auto tx = static_cast<uint32_t>(std::clamp(
            static_cast<int>((inst.position.x - chunk_origin.x) / tile_size), 0,
            static_cast<int>(n) - 1));
        const auto tz = static_cast<uint32_t>(std::clamp(
            static_cast<int>((inst.position.z - chunk_origin.y) / tile_size), 0,
            static_cast<int>(n) - 1));
        TileScratch& tile = tiles[static_cast<size_t>(tz) * n + tx];
        append_transformed(tile.mesh, src, pos, inst.yaw, inst.scale);
        const glm::vec2 tile_center{
            chunk_origin.x + (static_cast<float>(tx) + 0.5f) * tile_size,
            chunk_origin.y + (static_cast<float>(tz) + 0.5f) * tile_size};
        const float reach =
            glm::length(glm::vec2{inst.position.x, inst.position.z} - tile_center)
            + species_radius(inst.species) * inst.scale;
        tile.radius = std::max(tile.radius, reach);
    }

    for (uint32_t tz = 0; tz < n; ++tz) {
        for (uint32_t tx = 0; tx < n; ++tx) {
            TileScratch& tile = tiles[static_cast<size_t>(tz) * n + tx];
            if (tile.mesh.vertices.empty()) {
                continue;
            }
            MicroTile micro;
            micro.center_xz = {
                chunk_origin.x + (static_cast<float>(tx) + 0.5f) * tile_size,
                chunk_origin.y + (static_cast<float>(tz) + 0.5f) * tile_size};
            micro.radius_m = tile.radius;
            micro.mesh = std::move(tile.mesh);
            out.micro.push_back(std::move(micro));
        }
    }
    return out;
}

} // namespace dfn::render
