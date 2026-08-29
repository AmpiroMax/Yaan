/*
Module: tests
File: tests/core/HeightLatticeTests.cpp

Responsibility:
- THE STORAGE LATTICE AGAINST THE GEOMETRY LATTICE. The drawn and collided
  ground is a voxel surface on a VOXEL_SIZE lattice, built by sampling the
  chunk heightmap, which lives on a HEIGHTMAP_STEP lattice. This suite is the
  arm that measures what the first lattice INVENTS when it is finer than the
  second, and refuses the pair of numbers that would let it invent again.

Key items:
- "the storage lattice spans the chunk exactly": the invariant nothing guarded.
- "the drawn ground invents no relief": the acceptance, on real seed-1 terrain,
  with the coarsened-lattice control that must measure invention.
- "a voxel node lands on a stored sample": the structural half of the same fact.

Dependencies:
- Uses: engine/world (build_world_context, generate_chunk, terrain_height),
  engine/core/config.
- Used by: ctest target test_height_lattice.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE INSTRUMENT REUSES Heightmap::sample_world RATHER THAN COPYING THE
  BILINEAR. VoxelVolume.cpp's height_at_node is the same filter over the same
  data; a private copy here would be a second truth about the drawn ground and
  would drift the day either one is retuned (Rule 39).
- Every acceptance in this file has a control that must FAIL (Rule 30). The
  control is not a reverted constant — it is the same instrument pointed at a
  deliberately coarsened lattice, so it runs on every green build.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/Worldgen.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <doctest/doctest.h>
#include <vector>

using dfn::world::Chunk;
using dfn::world::ChunkCoord;
using dfn::world::Heightmap;
using dfn::world::WorldGenParams;

namespace {

constexpr float CHUNK_SIZE_M = static_cast<float>(dfn::config::CHUNK_SIZE);
constexpr float STEP_M = static_cast<float>(dfn::config::HEIGHTMAP_STEP);
constexpr uint32_t RES = static_cast<uint32_t>(dfn::config::HEIGHTMAP_RESOLUTION);
constexpr float VOXEL_M = static_cast<float>(dfn::config::VOXEL_SIZE);

/// The height quantum: one raw uint16 unit of the SHARED quantization. No
/// sample can be closer to the true field than half of this, so it is the
/// noise floor of every measurement below and NOT a tolerance anyone chose.
constexpr float QUANT_M = static_cast<float>(dfn::config::WORLDGEN_MAX_HEIGHT) / 65535.0f;

/// The seed-1 testbed, the world every other measurement in this repo is on.
[[nodiscard]] WorldGenParams testbed_params() {
    return WorldGenParams{1, {0, 0}, {7, 7}};
}

/// What the instrument reports: how far the ground the engine DRAWS at a voxel
/// node stands from the ground the generator actually describes there.
struct Invention {
    uint32_t nodes = 0;      ///< voxel nodes measured
    uint32_t off_sample = 0; ///< of those, nodes that do NOT land on a stored sample
    float worst_m = 0.0f;
    double mean_m = 0.0;
};

/// Measures one chunk. `hm` is the stored heightmap the drawn surface is built
/// from; the reference is the generator's own continuous field.
[[nodiscard]] Invention measure(const dfn::world::WorldGenContext& ctx, ChunkCoord coord,
                                const Heightmap& hm) {
    const glm::vec2 origin{static_cast<float>(coord.x) * CHUNK_SIZE_M,
                           static_cast<float>(coord.z) * CHUNK_SIZE_M};
    // The voxel node plane of one chunk, exactly as VoxelVolume builds it.
    const int32_t nodes = static_cast<int32_t>(CHUNK_SIZE_M / VOXEL_M) + 1;
    Invention out;
    double sum = 0.0;
    for (int32_t j = 0; j < nodes; ++j) {
        for (int32_t i = 0; i < nodes; ++i) {
            const glm::vec2 p = origin + glm::vec2{static_cast<float>(i) * VOXEL_M,
                                                   static_cast<float>(j) * VOXEL_M};
            const float drawn = hm.sample_world(coord, p);
            const float truth = dfn::world::terrain_height(ctx, p);
            const float err = std::fabs(drawn - truth);
            out.worst_m = std::max(out.worst_m, err);
            sum += static_cast<double>(err);
            ++out.nodes;
            // "Lands on a stored sample" is a statement about the LATTICES, not
            // about this chunk's data: the node's offset from the origin has to
            // be a whole number of storage steps ON BOTH AXES. Checking only x
            // undercounts badly — at a 2 m step it reports 49.8 % of nodes
            // interpolated when the true figure is 74.8 %, because a node is
            // invented when EITHER index is off the lattice, not only when x is.
            const auto off = [](int32_t n) {
                const float in_steps = static_cast<float>(n) * VOXEL_M / STEP_M;
                return std::fabs(in_steps - std::round(in_steps)) > 1.0e-4f;
            };
            if (off(i) || off(j)) {
                ++out.off_sample;
            }
        }
    }
    out.mean_m = sum / static_cast<double>(out.nodes);
    return out;
}

/// THE CONTROL, and it needs no constant reverted: the same chunk's heightmap
/// with every ODD sample replaced by the average of its neighbours — that is
/// precisely what a lattice one power of two coarser would have stored, and
/// what the bilinear would have had to invent back. If the instrument above
/// cannot see the difference between this and the real thing, it cannot see
/// anything, and its acceptance has quietly stopped being able to fail.
[[nodiscard]] Heightmap coarsened(const Heightmap& hm) {
    Heightmap out = hm;
    for (uint32_t z = 0; z < RES; ++z) {
        for (uint32_t x = 0; x < RES; ++x) {
            if ((x % 2 == 0) && (z % 2 == 0)) continue;
            const uint32_t x0 = (x / 2) * 2;
            const uint32_t z0 = (z / 2) * 2;
            const uint32_t x1 = std::min(x0 + 2, RES - 1);
            const uint32_t z1 = std::min(z0 + 2, RES - 1);
            const float tx = (x == x0) ? 0.0f : 0.5f;
            const float tz = (z == z0) ? 0.0f : 0.5f;
            const float a = static_cast<float>(hm.samples[z0 * RES + x0])
                          + (static_cast<float>(hm.samples[z0 * RES + x1])
                             - static_cast<float>(hm.samples[z0 * RES + x0])) * tx;
            const float b = static_cast<float>(hm.samples[z1 * RES + x0])
                          + (static_cast<float>(hm.samples[z1 * RES + x1])
                             - static_cast<float>(hm.samples[z1 * RES + x0])) * tx;
            out.samples[z * RES + x] =
                static_cast<uint16_t>(std::lround(a + (b - a) * tz));
        }
    }
    return out;
}

} // namespace

TEST_CASE("the storage lattice spans the chunk exactly") {
    // NOTHING GUARDED THIS. CHUNK_SIZE, HEIGHTMAP_RESOLUTION and HEIGHTMAP_STEP
    // are three independent rows of NUMBERS.md that must satisfy one equation,
    // and until this line existed the only statement of it was prose in
    // HeightField.h. An edit to any one of them alone produces a heightmap that
    // covers the wrong ground — which shows up as a seam at every chunk border,
    // a long way from the row that caused it.
    CHECK(STEP_M * static_cast<float>(RES - 1) == doctest::Approx(CHUNK_SIZE_M));

    // The control: the equation is not vacuous — a resolution one off breaks it.
    CHECK_FALSE(STEP_M * static_cast<float>(RES) == doctest::Approx(CHUNK_SIZE_M));
}

TEST_CASE("every voxel node lands on a stored height sample") {
    // THE STRUCTURAL HALF OF THE FIX. The drawn and collided ground is a voxel
    // surface on a VOXEL_SIZE lattice; a node between two stored samples gets a
    // height NOBODY GENERATED, filled in by the bilinear filter. With storage
    // at 2 m and voxels at 1 m, three of every four nodes were such a node.
    //
    // The requirement is therefore not "finer is nicer": it is that the storage
    // step DIVIDE the voxel size, so the reconstruction filter has nothing left
    // to invent. Equal is the cheapest way to satisfy it.
    CHECK(STEP_M <= VOXEL_M);
    const float ratio = VOXEL_M / STEP_M;
    CHECK(std::fabs(ratio - std::round(ratio)) < 1.0e-6f);
}

TEST_CASE("the drawn ground invents no relief the generator did not describe") {
    const auto ctx = dfn::world::build_world_context(testbed_params());
    // Four chunks off the world corner, where the testbed has its real relief
    // rather than the flat approach: the invention this measures is a property
    // of slope, and flat ground cannot exhibit it (the flat-chunk lesson from
    // the quad-diagonal audit, JoltPhysicsTests).
    const ChunkCoord coords[4] = {{3, 3}, {3, 4}, {4, 3}, {4, 4}};

    float worst_shipped = 0.0f;
    float worst_control = 0.0f;
    uint32_t off_sample_total = 0;
    uint64_t nodes_total = 0;

    for (const ChunkCoord c : coords) {
        const Chunk chunk = dfn::world::generate_chunk(ctx, c);
        const Invention shipped = measure(ctx, c, chunk.heightmap);
        const Invention control = measure(ctx, c, coarsened(chunk.heightmap));
        MESSAGE("chunk (" << c.x << "," << c.z << "): " << shipped.nodes
                          << " voxel nodes, " << shipped.off_sample
                          << " off-sample | shipped worst " << shipped.worst_m
                          << " m mean " << shipped.mean_m << " m | coarsened control worst "
                          << control.worst_m << " m mean " << control.mean_m << " m");
        worst_shipped = std::max(worst_shipped, shipped.worst_m);
        worst_control = std::max(worst_control, control.worst_m);
        off_sample_total += shipped.off_sample;
        nodes_total += shipped.nodes;
    }
    REQUIRE(nodes_total > 200000);

    // THE ACCEPTANCE. Not a fitted tolerance: the stored sample is the true
    // height ROUNDED to the shared quantization, so half a quantum is the
    // closest any storage can stand to the field, and two quanta is that floor
    // with room for the float arithmetic of the round trip. A drawn surface
    // that stays inside it is carrying the generator's own relief and nothing
    // of its own invention.
    CHECK(off_sample_total == 0);
    CHECK(worst_shipped < 2.0f * QUANT_M);

    // THE CONTROL MUST MEASURE INVENTION, and by a margin the acceptance could
    // never be mistaken for. Two orders of magnitude between them is the
    // instrument saying it can resolve the thing it is asked to refuse.
    CHECK(worst_control > 100.0f * worst_shipped);
    CHECK(worst_control > 0.5f);
}
