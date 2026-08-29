/*
Module: tests/sim
File: tests/sim/CollisionCostTests.cpp

Responsibility:
- SIZE THE COLLISION MESH COST, which is the dominant term left in chunk
  streaming. CHUNK_LOAD_BUDGET's own row prices a chunk at ~83 ms and splits it
  14.5 ms core generation + ~68 ms this zone's Jolt MeshShape build: 82% of a
  streaming hitch is built here, and the row's stated direction for cheapening
  it is a COARSER COLLISION MESH rather than a smaller budget.
- This measures what a coarser mesh would actually buy, so that decision is
  taken against a curve instead of an intuition.

Key items:
- Reports triangles/chunk and ms/chunk for the real generated world.
- Reports MeshShape build time against triangle count at 1/1, 1/2, 1/4, 1/8.

Dependencies:
- Uses: world ChunkManager (real chunks, never a synthetic mesh), jolt backend,
  engine/physics helper, doctest.

Notes:
- The decimation here is INDEX TRUNCATION, which is NOT geometrically valid
  terrain — it is valid for timing, because Jolt's tree build cost is a
  function of triangle COUNT and their distribution, not of whether the surface
  is watertight. Stated rather than glossed: this probe answers "what does
  count cost", not "is a decimated mesh walkable". The second question is the
  fix's job and needs the tunnel-walk suite, not this file.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- This is a MEASUREMENT, not a gate: it asserts only that the shapes build.
  A wall-clock threshold here would go red on a busy machine and get weakened,
  which is the failure Rule 38 describes.
*/

#include <doctest/doctest.h>

#include <chrono>
#include <vector>

#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/world/sources/ChunkManager.h"

using namespace dfn;

TEST_CASE("collision mesh cost: what a coarser mesh would buy" * doctest::skip(false)) {
    auto physics = platform::create_jolt_physics();
    REQUIRE(physics->init());

    ecs::World ecs;
    events::EventBus bus;
    world::ChunkManager chunks;
    chunks.open_generated(world::WorldGenParams{1, {0, 0}, {3, 3}},
                          world::ChunkStreamingParams{1, 2});
    const glm::vec3 focus{128.0f, 0.0f, 128.0f};
    for (int i = 0; i < 128; ++i) {
        const std::size_t before = chunks.loaded_chunks().size();
        chunks.update(focus, ecs, bus);
        if (chunks.loaded_chunks().size() == before && before > 0) {
            break;
        }
    }
    REQUIRE(!chunks.loaded_chunks().empty());

    // One real chunk is enough for a scaling curve; the full-world per-chunk
    // average already lives in sim_tunnel_walk.
    const world::ChunkCoord coord = chunks.loaded_chunks().front();
    const auto mesh = chunks.voxel_mesh(coord);
    REQUIRE(mesh.has_value());
    const size_t full_tris = mesh->indices.size() / 3;
    MESSAGE("chunk (" << coord.x << "," << coord.z << "): " << mesh->positions.size()
                      << " vertices, " << full_tris << " triangles");

    for (const size_t divisor : {size_t{1}, size_t{2}, size_t{4}, size_t{8}}) {
        std::vector<uint32_t> idx;
        idx.reserve(mesh->indices.size() / divisor + 3);
        for (size_t t = 0; t * divisor < full_tris; ++t) {
            const size_t src = t * divisor * 3;
            idx.push_back(mesh->indices[src]);
            idx.push_back(mesh->indices[src + 1]);
            idx.push_back(mesh->indices[src + 2]);
        }
        platform::TerrainMeshDesc desc;
        desc.positions = mesh->positions;
        desc.indices = idx;
        desc.layer = dfn::physics::LAYER_STATIC;
        const auto start = std::chrono::steady_clock::now();
        const auto body = physics->create_terrain_mesh(desc);
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - start)
                              .count();
        CHECK(body.valid());
        MESSAGE("  1/" << divisor << " of the triangles (" << idx.size() / 3
                       << "): " << ms << " ms");
        physics->destroy_body(body);
    }
    physics->shutdown();
}
