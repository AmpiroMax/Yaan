/*
Module: tests
File: tests/core/CoarseLodTests.cpp

Responsibility:
- The LOD streaming half: coarse node identity and geometry, the EXACT seam
  between a coarse node and a chunk where their lattices coincide (with the two
  counterfactual builders that must fail it), and ChunkManager's async node
  residency — budgeted delivery, no eviction behind render's back, release.

Dependencies:
- Uses: doctest, dfn_world (CoarseTerrain, ChunkManager, Worldgen), dfn_core.
- Used by: ctest (test_coarse_lod).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 30: the seam case ships with the two builders it exists to REJECT (a
  continuous-field sampler and a per-node quantization range), and both are run
  and required to FAIL. A green seam test with no control is a description.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/world/sources/ChunkManager.h"
#include "engine/world/sources/CoarseTerrain.h"
#include "engine/world/sources/Worldgen.h"

#include <algorithm>
#include <cmath>
#include <doctest/doctest.h>
#include <vector>

using dfn::world::ChunkCoord;
using dfn::world::ChunkManager;
using dfn::world::ChunkStreamingParams;
using dfn::world::CoarseNode;
using dfn::world::WorldGenParams;

namespace {

constexpr float CHUNK_SIZE_M = static_cast<float>(dfn::config::CHUNK_SIZE);
constexpr float CHUNK_STEP_M = static_cast<float>(dfn::config::HEIGHTMAP_STEP);
constexpr uint32_t CHUNK_RES = static_cast<uint32_t>(dfn::config::HEIGHTMAP_RESOLUTION);
constexpr uint32_t NODE_RES = dfn::world::COARSE_NODE_RESOLUTION;

/// The 2x2 km testbed the world is being taken to (8x8 chunks).
[[nodiscard]] WorldGenParams testbed_params() {
    return WorldGenParams{1, {0, 0}, {7, 7}};
}

} // namespace

TEST_CASE("coarse node geometry sits on the fixed world grid") {
    using dfn::world::coarse_node_origin_m;
    using dfn::world::coarse_node_size_m;
    using dfn::world::coarse_voxel_size_m;

    // The ladder: 128 voxels per node at every level, so the node's metre size
    // is the voxel size scaled by a constant.
    const float sizes[6] = {128.0f, 512.0f, 1024.0f, 2048.0f, 4096.0f, 8192.0f};
    for (uint8_t l = 0; l < 6; ++l) {
        CHECK(coarse_node_size_m(l) == doctest::Approx(sizes[l]));
        CHECK(coarse_node_size_m(l)
              == doctest::Approx(coarse_voxel_size_m(l) * 128.0f));
        // A node spans exactly (resolution - 1) steps: the 129th sample is the
        // shared edge row, which belongs to the neighbour as well.
        CHECK(coarse_voxel_size_m(l) * static_cast<float>(NODE_RES - 1)
              == doctest::Approx(coarse_node_size_m(l)));
    }

    // World position = coord * node size, and the numbering is rooted at world
    // zero rather than at the world's corner — that is what lets the world grow
    // from 2x2 to 10x10 km without renumbering a single cached node.
    CHECK(coarse_node_origin_m({2, 3, -1}).x == doctest::Approx(3.0f * 1024.0f));
    CHECK(coarse_node_origin_m({2, 3, -1}).y == doctest::Approx(-1024.0f));

    // Identity is exact and collision-free, INCLUDING negative coordinates —
    // the control for the key packing, where a sign-extended coordinate would
    // flood the level field and alias two different nodes onto one node.
    std::vector<uint64_t> keys;
    for (uint8_t l = 0; l < 6; ++l) {
        for (int32_t x = -3; x <= 3; ++x) {
            for (int32_t z = -3; z <= 3; ++z) {
                keys.push_back(dfn::world::coarse_node_key({l, x, z}));
            }
        }
    }
    const std::size_t total = keys.size();
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    CHECK(keys.size() == total);
}

TEST_CASE("a coarse sample equals the chunk sample where the lattices meet") {
    const WorldGenParams params = testbed_params();
    const dfn::world::WorldGenContext ctx = dfn::world::build_world_context(params);

    // Chunk (4, 4) sits under the massif's flank — the part of the world where
    // a seam would actually be seen, and where the field is steep enough that
    // an inexact builder cannot pass by accident (Rule 30a: a case that CAN
    // pass, on ground that CAN fail).
    const ChunkCoord coord{4, 4};
    const dfn::world::Chunk chunk = dfn::world::generate_chunk(ctx, coord);
    const glm::vec2 chunk_origin{static_cast<float>(coord.x) * CHUNK_SIZE_M,
                                 static_cast<float>(coord.z) * CHUNK_SIZE_M};

    std::size_t compared = 0;
    std::size_t mismatched = 0;
    std::size_t control_continuous = 0; // sampling the field without quantizing
    std::size_t control_local_range = 0; // quantizing against a per-node range

    // Levels 0..3 (1 / 4 / 8 / 16 m). From level 1 up every sample lands on the
    // chunk's 2 m lattice; level 0 is finer than the chunk, so every OTHER
    // sample does, and the rest are skipped.
    for (uint8_t level = 0; level <= 3; ++level) {
        const float voxel = dfn::world::coarse_voxel_size_m(level);
        const float node_size = dfn::world::coarse_node_size_m(level);
        const CoarseNode node{level,
                              static_cast<int32_t>(std::floor(chunk_origin.x / node_size)),
                              static_cast<int32_t>(std::floor(chunk_origin.y / node_size))};
        const dfn::world::CoarseNodeData data = dfn::world::build_coarse_node(ctx, node);
        REQUIRE(data.complete());
        const glm::vec2 origin = dfn::world::coarse_node_origin_m(node);

        // The counterfactual range: what a per-node min/max quantization would
        // have produced for this node.
        float lo = 1.0e30f;
        float hi = -1.0e30f;
        for (uint16_t raw : data.heights) {
            const float h = dfn::world::dequantize_height(raw);
            lo = std::min(lo, h);
            hi = std::max(hi, h);
        }
        const float local_scale = (hi - lo) / 65535.0f;

        for (uint32_t z = 0; z < NODE_RES; ++z) {
            for (uint32_t x = 0; x < NODE_RES; ++x) {
                const glm::vec2 world = origin
                                      + glm::vec2{static_cast<float>(x) * voxel,
                                                  static_cast<float>(z) * voxel};
                const glm::vec2 rel = world - chunk_origin;
                const float fx = rel.x / CHUNK_STEP_M;
                const float fz = rel.y / CHUNK_STEP_M;
                if (fx < 0.0f || fz < 0.0f || fx > static_cast<float>(CHUNK_RES - 1)
                    || fz > static_cast<float>(CHUNK_RES - 1)) {
                    continue;
                }
                const auto ix = static_cast<uint32_t>(std::lround(fx));
                const auto iz = static_cast<uint32_t>(std::lround(fz));
                if (std::abs(static_cast<float>(ix) - fx) > 1.0e-4f
                    || std::abs(static_cast<float>(iz) - fz) > 1.0e-4f) {
                    continue; // not a shared lattice point
                }

                const float chunk_h = chunk.heightmap.height_at(ix, iz);
                const std::size_t i = static_cast<std::size_t>(z) * NODE_RES + x;
                const float coarse_h = dfn::world::dequantize_height(data.heights[i]);
                ++compared;
                if (coarse_h != chunk_h) {
                    ++mismatched;
                }

                // CONTROL A — sample the continuous field instead of the
                // quantized one. This is the builder the flag warned about and
                // it is what produced the 0.30 m chunk border step; here it is
                // required to DISAGREE, so the assertion above is known to
                // discriminate rather than merely to hold.
                if (dfn::world::terrain_height(ctx, world) != chunk_h) {
                    ++control_continuous;
                }
                // CONTROL B — quantize against this node's own min/max range,
                // the tempting "better precision" variant. Same requirement.
                if (local_scale > 0.0f) {
                    const float raw =
                        std::round((chunk_h - lo) / local_scale);
                    const float decoded =
                        lo + std::clamp(raw, 0.0f, 65535.0f) * local_scale;
                    if (decoded != chunk_h) {
                        ++control_local_range;
                    }
                }
            }
        }
    }

    // Non-vacuous: three levels of overlap over a 256 m chunk is tens of
    // thousands of shared points, and a test that compared none would pass.
    MESSAGE("shared lattice points compared: " << compared);
    REQUIRE(compared > 8000);
    CHECK(mismatched == 0);

    // Both controls must FAIL the assertion the real builder passes.
    MESSAGE("control A (continuous field) mismatches: " << control_continuous);
    MESSAGE("control B (per-node range) mismatches: " << control_local_range);
    CHECK(control_continuous > 0);
    CHECK(control_local_range > 0);
}

TEST_CASE("a coarse surface class equals the chunk's at a shared point") {
    const WorldGenParams params = testbed_params();
    const dfn::world::WorldGenContext ctx = dfn::world::build_world_context(params);
    const ChunkCoord coord{4, 4};
    const dfn::world::Chunk chunk = dfn::world::generate_chunk(ctx, coord);
    const glm::vec2 chunk_origin{static_cast<float>(coord.x) * CHUNK_SIZE_M,
                                 static_cast<float>(coord.z) * CHUNK_SIZE_M};

    // Level 2 (8 m): every sample is a shared lattice point. If the coarse
    // builder took its slope from its OWN 8 m spacing instead of the shared
    // +-2 m, the class would differ here — and render's cross-fade would swap
    // the material as well as the shape.
    const uint8_t level = 2;
    const float voxel = dfn::world::coarse_voxel_size_m(level);
    const float node_size = dfn::world::coarse_node_size_m(level);
    const CoarseNode node{level,
                          static_cast<int32_t>(std::floor(chunk_origin.x / node_size)),
                          static_cast<int32_t>(std::floor(chunk_origin.y / node_size))};
    const dfn::world::CoarseNodeData data = dfn::world::build_coarse_node(ctx, node);
    const glm::vec2 origin = dfn::world::coarse_node_origin_m(node);

    std::size_t compared = 0;
    std::size_t class_mismatch = 0;
    std::size_t water_mismatch = 0;
    for (uint32_t z = 0; z < NODE_RES; ++z) {
        for (uint32_t x = 0; x < NODE_RES; ++x) {
            const glm::vec2 world = origin + glm::vec2{static_cast<float>(x) * voxel,
                                                       static_cast<float>(z) * voxel};
            const glm::vec2 rel = world - chunk_origin;
            const float fx = rel.x / CHUNK_STEP_M;
            const float fz = rel.y / CHUNK_STEP_M;
            if (fx < 0.0f || fz < 0.0f || fx > static_cast<float>(CHUNK_RES - 1)
                || fz > static_cast<float>(CHUNK_RES - 1)) {
                continue;
            }
            const auto ix = static_cast<uint32_t>(std::lround(fx));
            const auto iz = static_cast<uint32_t>(std::lround(fz));
            const std::size_t ci = static_cast<std::size_t>(iz) * CHUNK_RES + ix;
            const std::size_t ni = static_cast<std::size_t>(z) * NODE_RES + x;
            ++compared;
            if (chunk.surface.surface_class[ci] != data.surface_class[ni]) {
                ++class_mismatch;
            }
            if (chunk.surface.water_surface[ci] != data.water_surface[ni]) {
                ++water_mismatch;
            }
        }
    }
    MESSAGE("surface samples compared: " << compared);
    REQUIRE(compared > 1000);
    CHECK(class_mismatch == 0);
    CHECK(water_mismatch == 0);
}

TEST_CASE("two nodes of the same level share their edge row exactly") {
    const dfn::world::WorldGenContext ctx =
        dfn::world::build_world_context(testbed_params());
    // Neighbours in x at level 2: the right node's column 0 is the left node's
    // column 128. Anything less than exact equality is a hairline of sky.
    const dfn::world::CoarseNodeData left = dfn::world::build_coarse_node(ctx, {2, 0, 1});
    const dfn::world::CoarseNodeData right = dfn::world::build_coarse_node(ctx, {2, 1, 1});
    std::size_t differing = 0;
    for (uint32_t z = 0; z < NODE_RES; ++z) {
        const std::size_t li = static_cast<std::size_t>(z) * NODE_RES + (NODE_RES - 1);
        const std::size_t ri = static_cast<std::size_t>(z) * NODE_RES;
        if (left.heights[li] != right.heights[ri]) {
            ++differing;
        }
    }
    CHECK(differing == 0);
    // Control: a row that is NOT shared must differ somewhere, or the check
    // above would pass on a node of constant height.
    std::size_t unshared_differing = 0;
    for (uint32_t z = 0; z < NODE_RES; ++z) {
        const std::size_t li = static_cast<std::size_t>(z) * NODE_RES + (NODE_RES - 2);
        const std::size_t ri = static_cast<std::size_t>(z) * NODE_RES;
        if (left.heights[li] != right.heights[ri]) {
            ++unshared_differing;
        }
    }
    CHECK(unshared_differing > 0);
}

namespace {

/// Drives ChunkManager without caring about chunk entities.
struct StreamFixture {
    dfn::ecs::World ecs;
    dfn::events::EventBus bus;
    ChunkManager chunks;

    explicit StreamFixture(WorldGenParams gen = testbed_params()) {
        chunks.open_generated(gen, ChunkStreamingParams{1, 2});
    }

    void updates(const glm::vec3& focus, int n) {
        for (int i = 0; i < n; ++i) {
            chunks.update(focus, ecs, bus);
        }
    }

    /// Runs until the chunk ring stops growing, so the coarse pass (which only
    /// spends an update no chunk wanted) is free to run.
    void settle_chunks(const glm::vec3& focus) {
        for (int i = 0; i < 256; ++i) {
            const std::size_t before = chunks.loaded_chunks().size();
            chunks.update(focus, ecs, bus);
            if (chunks.loaded_chunks().size() == before) {
                return;
            }
        }
    }
};

} // namespace

TEST_CASE("world_bounds_xz reports the generated extent, not a configured one") {
    ChunkManager fresh;
    // Nothing open: an empty rectangle, which is how render's descent spells
    // "no world" and is the control for the case below.
    CHECK(fresh.world_bounds_xz() == glm::vec4{0.0f});

    StreamFixture f;
    const glm::vec4 b = f.chunks.world_bounds_xz();
    CHECK(b.x == doctest::Approx(0.0f));
    CHECK(b.y == doctest::Approx(0.0f));
    CHECK(b.z == doctest::Approx(2048.0f)); // 8 chunks x 256 m = the 2x2 km world
    CHECK(b.w == doctest::Approx(2048.0f));

    // A DIFFERENT extent must produce a different answer — otherwise this is
    // reading a constant and calling it a measurement.
    StreamFixture small{WorldGenParams{1, {0, 0}, {3, 3}}};
    CHECK(small.chunks.world_bounds_xz().z == doctest::Approx(1024.0f));
}

TEST_CASE("a coarse node streams in asynchronously and past 600 m") {
    StreamFixture f;
    const glm::vec3 focus{128.0f, 0.0f, 128.0f};
    f.settle_chunks(focus);

    // A level-2 node (1024 m of ground at 8 m samples) in the far corner of the
    // 2x2 km world: its nearest edge is ~1267 m from the focus, which is well
    // past both the 512 m chunk ring and the 600 m the far terrain has to reach.
    const CoarseNode far_node{2, 1, 1};
    const float nearest =
        std::sqrt(2.0f) * (1024.0f - 128.0f); // corner-to-corner on xz
    REQUIRE(nearest > 600.0f);

    // A node nobody asked for is absent — the control for every "it arrived"
    // check below.
    CHECK_FALSE(f.chunks.coarse_heightfield(far_node).has_value());

    f.chunks.request_coarse_nodes(std::span<const CoarseNode>{&far_node, 1});
    CHECK(f.chunks.coarse_pending_count() == 1);

    // ASYNC: a request is not a delivery. One update cannot finish a 129-row
    // node under the row budget, and render's ferry is written against exactly
    // this (it retries its pending set rather than its one-shot load list).
    f.updates(focus, 1);
    CHECK_FALSE(f.chunks.coarse_heightfield(far_node).has_value());
    CHECK(f.chunks.coarse_resident_count() == 0);

    int updates_to_deliver = 1;
    for (; updates_to_deliver < 400; ++updates_to_deliver) {
        f.updates(focus, 1);
        if (f.chunks.coarse_heightfield(far_node)) {
            break;
        }
    }
    MESSAGE("updates to deliver one node: " << updates_to_deliver);
    CHECK(updates_to_deliver > 1); // genuinely budgeted, not "async" in name
    REQUIRE(f.chunks.coarse_heightfield(far_node).has_value());
    CHECK(f.chunks.coarse_pending_count() == 0);
    CHECK(f.chunks.coarse_resident_count() == 1);

    // The view IS a HeightFieldView on the agreed grid.
    const auto view = *f.chunks.coarse_heightfield(far_node);
    CHECK(view.resolution == NODE_RES);
    CHECK(view.step == doctest::Approx(8.0f));
    CHECK(view.origin.x == doctest::Approx(1024.0f));
    CHECK(view.origin.y == doctest::Approx(1024.0f));
    CHECK(view.heights.size() == static_cast<std::size_t>(NODE_RES) * NODE_RES);
    CHECK(view.height_offset == doctest::Approx(dfn::world::HEIGHT_QUANT_OFFSET));
    CHECK(view.height_scale == doctest::Approx(dfn::world::HEIGHT_QUANT_SCALE));
    // Real ground, not zeros: a node of zeros would satisfy every check above.
    float max_h = 0.0f;
    for (uint32_t i = 0; i < view.resolution * view.resolution; ++i) {
        max_h = std::max(max_h, view.height_at(i % view.resolution, i / view.resolution));
    }
    CHECK(max_h > 1.0f);

    // The surface field ships WITH the geometry, on the same grid.
    const auto surface = f.chunks.coarse_surfacefield(far_node);
    REQUIRE(surface.has_value());
    CHECK(surface->resolution == view.resolution);
    CHECK(surface->step == doctest::Approx(view.step));
    CHECK(surface->origin == view.origin);
    CHECK(surface->surface_class.size() == view.heights.size());

    // A NODE BUILT IN SLICES IS THE NODE BUILT IN ONE GO. The streaming path
    // fills COARSE_NODE_ROW_BUDGET rows per update and the reference builds all
    // 129 at once; if those ever diverge, far terrain would depend on how busy
    // the frame was, which is the least debuggable class of difference there is.
    const dfn::world::WorldGenContext ctx =
        dfn::world::build_world_context(testbed_params());
    const dfn::world::CoarseNodeData reference =
        dfn::world::build_coarse_node(ctx, far_node);
    REQUIRE(reference.heights.size() == view.heights.size());
    std::size_t slice_mismatch = 0;
    for (std::size_t i = 0; i < reference.heights.size(); ++i) {
        if (reference.heights[i] != view.heights[i]) {
            ++slice_mismatch;
        }
        if (reference.surface_class[i] != surface->surface_class[i]) {
            ++slice_mismatch;
        }
    }
    CHECK(slice_mismatch == 0);

    // NO EVICTION BEHIND RENDER'S BACK: a hundred further updates, and a focus
    // that walks to the far side of the world, must not take the node away.
    f.updates(glm::vec3{1900.0f, 0.0f, 1900.0f}, 100);
    CHECK(f.chunks.coarse_heightfield(far_node).has_value());

    // Release is the only thing that frees it.
    f.chunks.release_coarse_node(far_node);
    CHECK(f.chunks.coarse_resident_count() == 0);
    CHECK_FALSE(f.chunks.coarse_heightfield(far_node).has_value());
    CHECK_FALSE(f.chunks.coarse_surfacefield(far_node).has_value());
}

TEST_CASE("requests are idempotent and cancellable before delivery") {
    StreamFixture f;
    const glm::vec3 focus{128.0f, 0.0f, 128.0f};
    f.settle_chunks(focus);

    const CoarseNode nodes[3] = {{2, 1, 1}, {2, 0, 1}, {2, 1, 0}};
    f.chunks.request_coarse_nodes(nodes);
    CHECK(f.chunks.coarse_pending_count() == 3);
    // Render passes its standing pending set every frame; asking twice must not
    // queue the same ground twice (and must never rebuild a delivered node
    // under a view render is drawing).
    f.chunks.request_coarse_nodes(nodes);
    CHECK(f.chunks.coarse_pending_count() == 3);

    // Cancel one that is still queued (nearest-first has started {2,0,1}).
    f.updates(focus, 2);
    f.chunks.release_coarse_node(nodes[2]);
    CHECK(f.chunks.coarse_pending_count() == 2);
    f.updates(focus, 400);
    CHECK(f.chunks.coarse_resident_count() == 2);
    CHECK(f.chunks.coarse_pending_count() == 0);
    CHECK_FALSE(f.chunks.coarse_heightfield(nodes[2]).has_value());
    for (int i = 0; i < 2; ++i) {
        CHECK(f.chunks.coarse_heightfield(nodes[i]).has_value());
    }
}

TEST_CASE("a coarse build never competes with a chunk admission") {
    // The policy that keeps two budgets out of one frame: an update that
    // admitted a chunk spends nothing on coarse terrain. It is measured rather
    // than asserted in prose — from a COLD start, delivery cannot happen before
    // (updates that admitted a chunk) + (rows / row budget) have passed, and if
    // the coarse pass ran alongside chunk loading it would land earlier.
    StreamFixture f;
    const glm::vec3 focus{128.0f, 0.0f, 128.0f};
    const CoarseNode node{2, 1, 1};
    f.chunks.request_coarse_nodes(std::span<const CoarseNode>{&node, 1});

    std::size_t before = f.chunks.loaded_chunks().size();
    int chunk_admitting_updates = 0;
    int updates = 0;
    for (; updates < 400; ++updates) {
        f.chunks.update(focus, f.ecs, f.bus);
        const std::size_t now = f.chunks.loaded_chunks().size();
        if (now != before) {
            ++chunk_admitting_updates;
        }
        before = now;
        if (f.chunks.coarse_heightfield(node)) {
            break;
        }
    }
    REQUIRE(chunk_admitting_updates > 0); // the case is not vacuous
    REQUIRE(f.chunks.coarse_heightfield(node).has_value());

    const int build_updates =
        static_cast<int>((NODE_RES + dfn::world::COARSE_NODE_ROW_BUDGET - 1)
                         / dfn::world::COARSE_NODE_ROW_BUDGET);
    MESSAGE("cold-start updates to first coarse node: " << updates + 1
            << " (chunk admissions " << chunk_admitting_updates
            << ", build updates " << build_updates << ")");
    CHECK(updates + 1 >= chunk_admitting_updates + build_updates);
}
