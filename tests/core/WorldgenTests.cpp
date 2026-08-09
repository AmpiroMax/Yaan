/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 11:05:22
Module: tests
File: tests/core/WorldgenTests.cpp

Responsibility:
- THE worldgen determinism test (Rule 13.1, non-negotiable): same seed produces
  identical output (FNV-1a 64 over heights, surface arrays, entities and
  scatter), different seeds differ; chunk independence; exact shared-edge
  stitching at the WORLDGEN_MAX_HEIGHT quantization range; RNG stream
  stability.

Dependencies:
- Uses: doctest, dfn_world (Worldgen), dfn_core (ContentHash).
- Used by: ctest (test_worldgen_determinism).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- This suite guards Rule 13.1 from the first worldgen commit. Never weaken it.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — initial determinism suite.
- 09:08:2026 - 11:05:22: Stage 3b — hash covers v2 outputs (surface arrays,
  entities, scatter); edge stitching re-pinned at the 64 m shared range
  (hash values changed with worldgen v2 — expected, structure unchanged).
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/world/sources/Worldgen.h"

#include <bit>
#include <doctest/doctest.h>

using dfn::serialization::Fnv1a64;
using dfn::world::ChunkCoord;
using dfn::world::WorldGenParams;
using dfn::world::WorldGenRng;

namespace {

constexpr uint32_t RESOLUTION = static_cast<uint32_t>(dfn::config::HEIGHTMAP_RESOLUTION);

void hash_chunk(Fnv1a64& hash, const dfn::world::Chunk& chunk) {
    REQUIRE(chunk.heightmap.samples.size()
            == static_cast<std::size_t>(RESOLUTION) * RESOLUTION);
    REQUIRE(chunk.surface.dist_to_water.size() == chunk.heightmap.samples.size());
    REQUIRE(chunk.surface.water_surface.size() == chunk.heightmap.samples.size());
    REQUIRE(chunk.surface.surface_class.size() == chunk.heightmap.samples.size());
    for (const uint16_t s : chunk.heightmap.samples) {
        hash.update_u64(s);
    }
    for (std::size_t i = 0; i < chunk.surface.dist_to_water.size(); ++i) {
        hash.update_u64(std::bit_cast<uint32_t>(chunk.surface.dist_to_water[i]));
        hash.update_u64(std::bit_cast<uint32_t>(chunk.surface.water_surface[i]));
        hash.update_u64(chunk.surface.surface_class[i]);
    }
    for (const auto& e : chunk.entities) {
        hash.update_u64(e.world_id);
        hash.update_u64(e.archetype);
        hash.update_u64(std::bit_cast<uint32_t>(e.position_xz.x));
        hash.update_u64(std::bit_cast<uint32_t>(e.position_xz.y));
        hash.update_u64(std::bit_cast<uint32_t>(e.yaw));
    }
    for (const auto& s : chunk.scatter) {
        hash.update_u64(std::bit_cast<uint32_t>(s.position.x));
        hash.update_u64(std::bit_cast<uint32_t>(s.position.y));
        hash.update_u64(std::bit_cast<uint32_t>(s.position.z));
        hash.update_u64(std::bit_cast<uint32_t>(s.yaw));
        hash.update_u64(std::bit_cast<uint32_t>(s.scale));
        hash.update_u64(static_cast<uint8_t>(s.species));
    }
}

// FNV-1a 64 digest of every v2 output of a 3x3 chunk region, row-major over
// chunks (fixed order = comparable digests).
uint64_t region_hash(uint64_t seed) {
    const WorldGenParams params{seed, ChunkCoord{-1, -1}, ChunkCoord{1, 1}};
    const auto ctx = dfn::world::build_world_context(params);
    Fnv1a64 hash;
    for (int32_t cz = -1; cz <= 1; ++cz) {
        for (int32_t cx = -1; cx <= 1; ++cx) {
            hash_chunk(hash, dfn::world::generate_chunk(ctx, ChunkCoord{cx, cz}));
        }
    }
    return hash.digest();
}

} // namespace

TEST_CASE("Rule 13.1: same seed -> byte-identical worlds, twice") {
    const uint64_t first = region_hash(0xDA66E12FA11ull);
    const uint64_t second = region_hash(0xDA66E12FA11ull);
    CHECK(first == second);
}

TEST_CASE("Rule 13.1: different seeds -> different worlds") {
    CHECK(region_hash(1) != region_hash(2));
    CHECK(region_hash(0) != region_hash(1));
}

TEST_CASE("chunk generation is independent of generation order and context reuse") {
    const WorldGenParams params{42, ChunkCoord{-8, -8}, ChunkCoord{8, 8}};
    // Generate (3, -2) cold, then after generating unrelated chunks — identical.
    const auto direct = dfn::world::generate_chunk(params, ChunkCoord{3, -2});
    (void)dfn::world::generate_chunk(params, ChunkCoord{0, 0});
    (void)dfn::world::generate_chunk(params, ChunkCoord{-5, 7});
    const auto again = dfn::world::generate_chunk(params, ChunkCoord{3, -2});
    CHECK(direct.heightmap.samples == again.heightmap.samples);
    CHECK(direct.heightmap.height_scale == doctest::Approx(again.heightmap.height_scale));
    CHECK(direct.heightmap.height_offset == doctest::Approx(again.heightmap.height_offset));

    // A shared prebuilt context produces the same bytes as the throwaway path.
    const auto ctx = dfn::world::build_world_context(params);
    const auto via_ctx = dfn::world::generate_chunk(ctx, ChunkCoord{3, -2});
    CHECK(direct.heightmap.samples == via_ctx.heightmap.samples);
    CHECK(direct.surface.surface_class == via_ctx.surface.surface_class);
    CHECK(direct.scatter.size() == via_ctx.scatter.size());
}

TEST_CASE("shared chunk edges stitch exactly at the shared 64 m range") {
    const WorldGenParams params{7, ChunkCoord{-2, -2}, ChunkCoord{2, 2}};
    const auto ctx = dfn::world::build_world_context(params);
    const auto left = dfn::world::generate_chunk(ctx, ChunkCoord{0, 0});
    const auto right = dfn::world::generate_chunk(ctx, ChunkCoord{1, 0});
    const auto below = dfn::world::generate_chunk(ctx, ChunkCoord{0, 1});

    for (uint32_t z = 0; z < RESOLUTION; ++z) {
        // Column x = 128 of (0,0) == column x = 0 of (1,0) — raw equality.
        const std::size_t a = static_cast<std::size_t>(z) * RESOLUTION + (RESOLUTION - 1);
        const std::size_t b = static_cast<std::size_t>(z) * RESOLUTION + 0;
        CHECK(left.heightmap.samples[a] == right.heightmap.samples[b]);
        // Surface outputs are position-based too — identical on shared edges.
        CHECK(left.surface.surface_class[a] == right.surface.surface_class[b]);
        CHECK(left.surface.water_surface[a] == right.surface.water_surface[b]);
        CHECK(left.surface.dist_to_water[a] == right.surface.dist_to_water[b]);
    }
    for (uint32_t x = 0; x < RESOLUTION; ++x) {
        // Row z = 128 of (0,0) == row z = 0 of (0,1).
        const std::size_t a = static_cast<std::size_t>(RESOLUTION - 1) * RESOLUTION + x;
        CHECK(left.heightmap.samples[a] == below.heightmap.samples[x]);
        CHECK(left.surface.surface_class[a] == below.surface.surface_class[x]);
    }
    // Same quantization everywhere (edge-equality precondition): offset 0,
    // scale = WORLDGEN_MAX_HEIGHT / 65535 on every chunk.
    const float expected_scale =
        static_cast<float>(dfn::config::WORLDGEN_MAX_HEIGHT) / 65535.0f;
    CHECK(left.heightmap.height_offset == 0.0f);
    CHECK(right.heightmap.height_offset == 0.0f);
    CHECK(left.heightmap.height_scale == doctest::Approx(expected_scale));
    CHECK(right.heightmap.height_scale == doctest::Approx(expected_scale));
}

TEST_CASE("WorldGenRng streams are stable and well-behaved") {
    auto rng_a = WorldGenRng::for_chunk(99, ChunkCoord{4, -4}, 1);
    auto rng_b = WorldGenRng::for_chunk(99, ChunkCoord{4, -4}, 1);
    for (int i = 0; i < 100; ++i) {
        CHECK(rng_a.next_u64() == rng_b.next_u64());
    }
    // Different pass tag -> different stream.
    auto rng_c = WorldGenRng::for_chunk(99, ChunkCoord{4, -4}, 2);
    CHECK(rng_c.next_u64() != WorldGenRng::for_chunk(99, ChunkCoord{4, -4}, 1).next_u64());

    auto rng = WorldGenRng::for_chunk(5, ChunkCoord{0, 0}, 0);
    for (int i = 0; i < 1000; ++i) {
        const float f = rng.next_float01();
        CHECK(f >= 0.0f);
        CHECK(f < 1.0f);
        const uint32_t r = rng.next_range(3, 7);
        CHECK(r >= 3);
        CHECK(r <= 7);
    }
}

TEST_CASE("generate_world reports deferred file IO") {
    const auto result =
        dfn::world::generate_world(WorldGenParams{1, {0, 0}, {0, 0}}, "/tmp/unused.dfw");
    CHECK_FALSE(result.ok);
    CHECK_FALSE(result.error.empty());
}
