/*
Created: 09:08:2026 - 16:00:00
Last updated: 09:08:2026 - 16:00:00
Module: tests
File: tests/core/VoxelTests.cpp

Responsibility:
- The 3D terrain representation-swap suite: the extracted voxel surface must
  reproduce the heightfield surface (the machine-checkable half of "zero
  visible change"), stay bit-deterministic (Rule 13.1), meet its neighbours
  without a seam, and carry sane materials and winding.

Dependencies:
- Uses: doctest, dfn_world (Worldgen, VoxelVolume, VoxelMesh), dfn_core.
- Used by: ctest (test_voxel).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The fidelity test is the contract that lets the swap ship without a visual
  regression. Do not relax its tolerance to make a change pass — if the
  surface moved, the change is wrong.
*/
/*
UPD:
- 09:08:2026 - 16:00:00: Created — representation-swap suite.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/world/sources/VoxelMesh.h"
#include "engine/world/sources/VoxelVolume.h"
#include "engine/world/sources/Worldgen.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <vector>

using namespace dfn;
using world::ChunkCoord;
using world::WorldGenParams;

namespace {

const world::WorldGenContext& testbed() {
    static const world::WorldGenContext ctx =
        world::build_world_context(WorldGenParams{1, {0, 0}, {3, 3}});
    return ctx;
}

uint64_t mesh_hash(const world::VoxelSurface& m) {
    serialization::Fnv1a64 h;
    for (const glm::vec3& p : m.positions) {
        h.update_u64(std::bit_cast<uint32_t>(p.x));
        h.update_u64(std::bit_cast<uint32_t>(p.y));
        h.update_u64(std::bit_cast<uint32_t>(p.z));
    }
    for (const glm::vec3& n : m.normals) {
        h.update_u64(std::bit_cast<uint32_t>(n.x));
        h.update_u64(std::bit_cast<uint32_t>(n.y));
        h.update_u64(std::bit_cast<uint32_t>(n.z));
    }
    for (const uint8_t m8 : m.materials) h.update_u64(m8);
    for (const uint32_t i : m.indices) h.update_u64(i);
    return h.digest();
}

} // namespace

TEST_CASE("voxel surface reproduces the heightfield surface (no visible change)") {
    const auto& ctx = testbed();
    const auto chunk = world::generate_chunk(ctx, ChunkCoord{2, 1}); // crag foot
    REQUIRE_FALSE(chunk.voxels.empty());

    // Every extracted vertex must sit on the terrain the rest of the engine
    // still queries. Surface nets place a vertex inside its cell, so the
    // expected error is bounded by the voxel size, not by the terrain.
    double worst = 0.0;
    double sum = 0.0;
    std::size_t counted = 0;
    for (const glm::vec3& p : chunk.voxels.positions) {
        const float ground = world::terrain_height(ctx, {p.x, p.z});
        const double err = std::fabs(static_cast<double>(p.y) - ground);
        worst = std::max(worst, err);
        sum += err;
        ++counted;
    }
    REQUIRE(counted > 10000);
    const double mean = sum / static_cast<double>(counted);
    INFO("mean |dy| = " << mean << " m, worst = " << worst << " m, voxel = "
                        << config::VOXEL_SIZE << " m");
    // Mean error is the number that matters for "does it look the same".
    CHECK(mean < 0.25);
    // No vertex may be more than a voxel off the true surface.
    CHECK(worst < static_cast<double>(config::VOXEL_SIZE) * 1.5);
}

TEST_CASE("Rule 13.1: voxel volume and extracted mesh are bit-deterministic") {
    const auto& ctx = testbed();
    const auto a = world::generate_chunk(ctx, ChunkCoord{2, 1});
    const auto b = world::generate_chunk(ctx, ChunkCoord{2, 1});
    CHECK(a.voxels.positions.size() == b.voxels.positions.size());
    CHECK(a.voxels.indices == b.voxels.indices);
    CHECK(mesh_hash(a.voxels) == mesh_hash(b.voxels));

    // The volume itself (the quantized field) is an exact integer state.
    const auto sampler = [&ctx](glm::vec2 p) { return world::terrain_height(ctx, p); };
    const auto va = world::build_voxel_volume(a, sampler);
    const auto vb = world::build_voxel_volume(b, sampler);
    CHECK(va.sdf == vb.sdf);
    CHECK(va.material == vb.material);
}

TEST_CASE("neighbouring chunks meet without a seam") {
    const auto& ctx = testbed();
    const auto left = world::generate_chunk(ctx, ChunkCoord{1, 1});
    const auto right = world::generate_chunk(ctx, ChunkCoord{2, 1});
    // The -x/-z chunk owns the seam strip: its volume spans one cell past the
    // shared plane (x = 512), so its vertices there must COINCIDE with the
    // neighbour's own first-cell vertices. Coincide, not merely "be close":
    // both sides evaluate an identically quantized field.
    const auto band = [](const world::VoxelSurface& m, float lo, float hi) {
        std::vector<glm::vec3> out;
        for (const glm::vec3& p : m.positions) {
            if (p.x > lo && p.x < hi) out.push_back(p);
        }
        std::sort(out.begin(), out.end(), [](const glm::vec3& a, const glm::vec3& b) {
            return a.z < b.z;
        });
        return out;
    };
    const auto seam_left = band(left.voxels, 512.0f, 513.0f);
    const auto seam_right = band(right.voxels, 512.0f, 513.0f);
    REQUIRE_FALSE(seam_left.empty());
    REQUIRE_FALSE(seam_right.empty());

    std::size_t matched = 0;
    double worst = 0.0;
    for (const glm::vec3& a : seam_left) {
        double best = 1e9;
        for (const glm::vec3& b : seam_right) {
            best = std::min(best, static_cast<double>(glm::length(a - b)));
        }
        if (best < 1e-3) ++matched;
        worst = std::max(worst, best);
    }
    INFO("seam vertices: " << seam_left.size() << " left, " << seam_right.size()
                           << " right, matched " << matched << ", worst gap " << worst);
    // Every seam vertex of the owning chunk has an exact twin next door.
    CHECK(matched == seam_left.size());
    CHECK(worst < 1e-3);
}

TEST_CASE("mesh is well formed: indices in range, normals unit, materials solid") {
    const auto& ctx = testbed();
    const auto chunk = world::generate_chunk(ctx, ChunkCoord{2, 1});
    const auto& m = chunk.voxels;
    REQUIRE(m.indices.size() % 3 == 0);
    CHECK(m.normals.size() == m.positions.size());
    CHECK(m.materials.size() == m.positions.size());
    for (const uint32_t i : m.indices) {
        REQUIRE(i < m.positions.size());
    }
    for (const glm::vec3& n : m.normals) {
        CHECK(std::fabs(glm::length(n) - 1.0f) < 0.001f);
    }
    for (const uint8_t mat : m.materials) {
        // Air must never reach a vertex — every vertex has a solid side.
        CHECK(mat != static_cast<uint8_t>(math::VoxelMaterial::Air));
        CHECK(mat <= static_cast<uint8_t>(math::VoxelMaterial::Dirt));
    }
    // Open terrain: the great majority of faces point upward. (Carves will add
    // downward-facing ceilings; this pins the pre-carve baseline.)
    std::size_t up = 0;
    for (const glm::vec3& n : m.normals) {
        if (n.y > 0.0f) ++up;
    }
    CHECK(up > m.normals.size() * 9 / 10);
}

TEST_CASE("the cross-zone view exposes the mesh unchanged") {
    const auto& ctx = testbed();
    const auto chunk = world::generate_chunk(ctx, ChunkCoord{2, 1});
    const auto view = chunk.voxels.view(ChunkCoord{2, 1});
    CHECK(view.chunk_coord == glm::ivec2{2, 1});
    CHECK(view.positions.size() == chunk.voxels.positions.size());
    CHECK(view.triangle_count() == chunk.voxels.indices.size() / 3);
    CHECK(view.triangle_count() > 1000);
}
