/*
Created: 09:08:2026 - 11:57:20
Last updated: 09:08:2026 - 20:21:13
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
- 09:08:2026 - 19:46:00: Flora generator integrated: trees build per-instance
  geometry via build_flora_mesh (variant by position, shape from
  analyse_neighbourhood), appended at scale 1.0 because maturity is
  already inside the mesh, and with NO ground sink now that they have a
  root flare. species_radius updated to flora's measured envelopes.
- 09:08:2026 - 19:54:00: birch footprint 2.4 -> 3.1 m (flora's crown fix gave
  the birch a real 6.1 m crown; the old radius was measured off a bald tree).
- 09:08:2026 - 20:21:13: Trees bake into TWO streams via flora's append_flora
  (opaque wood + alpha-cutout leaf cards). EDITED BY THE FLORA AGENT under an
  explicit lead-granted Rule 25 exception while render's zone was unowned;
  wiring only, no material or shader change.
*/

#include "engine/render/sources/ScatterBatcher.h"

#include <algorithm>
#include "engine/render/sources/ProcFlora.h"

#include <array>
#include <cmath>

namespace dfn::render {

namespace {

// Sink fraction of the instance scale: hides the downhill gap under a mesh
// placed at the sample-point terrain height on sloped ground. TREES NO LONGER
// SINK AT ALL: the flora generator gives them a root flare reaching ~1 m below
// the model origin, which covers far more ground drop than this ever did, and
// sinking a tree only makes it shorter — the opposite of the point of the 4x
// height stage. Kept for bushes and stones, which have no flare.
constexpr float GROUND_SINK_FRAC = 0.12f;

// Conservative horizontal footprint radius (m) of each species' nominal mesh,
// used for micro tile bounding circles. Values from the flora agent's measured
// envelopes (their radial-clip fix): the old numbers were the pre-4x trees and
// under-covered by ~2x, which would have culled tiles while their geometry was
// still on screen.
float species_radius(math::ScatterSpecies species) {
    switch (species) {
    case math::ScatterSpecies::OakTree: return 7.3f;
    case math::ScatterSpecies::PineTree: return 3.8f;
    case math::ScatterSpecies::BirchTree: return 3.1f;
    // Core has one Bush species; flora may map it to either bush size, so take
    // the larger — an over-large bounding circle costs a few extra draws, an
    // under-large one pops geometry out while it is still visible.
    case math::ScatterSpecies::Bush: return 2.0f;
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

    // Per-instance shape (crowding lean, crown shyness, maturity, understory)
    // is derived from the neighbourhood once, up front — it needs every
    // instance to see its neighbours, so it cannot be done inside the loop.
    const std::vector<FloraShape> shapes =
        analyse_neighbourhood(instances, instances.size());

    for (size_t i = 0; i < instances.size(); ++i) {
        const math::ScatterInstance& inst = instances[i];
        if (is_tree(inst.species)) {
            const FloraSpecies fs = flora_species_of(inst.species);
            const uint32_t variant =
                flora_variant_for({inst.position.x, inst.position.z});
            // Scale 1.0: the generator has ALREADY applied FloraShape::maturity
            // (which is inst.scale) to the height. Passing inst.scale here too
            // would square it — a 1.25 giant becomes 1.56 and still looks
            // plausible, which is exactly why it would survive review.
            // Trees do not sink: they stand on their root flare.
            append_flora(out.trees, out.foliage, fs, variant, shapes[i],
                         FloraLod::Full, inst.position, inst.yaw);
            continue;
        }
        const MeshData& src = mesh_of(inst.species);
        if (src.vertices.empty()) {
            continue;
        }
        const glm::vec3 pos{inst.position.x,
                            inst.position.y - GROUND_SINK_FRAC * inst.scale,
                            inst.position.z};
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
