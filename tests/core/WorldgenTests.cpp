/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 00:42:03
Module: tests
File: tests/core/WorldgenTests.cpp

Responsibility:
- THE worldgen determinism test (Rule 13.1, non-negotiable): same seed produces
  identical heights (FNV-1a 64 over all raw samples), different seeds differ;
  chunk independence; exact shared-edge stitching; RNG stream stability.

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
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/world/sources/Worldgen.h"

#include <doctest/doctest.h>

using dfn::serialization::Fnv1a64;
using dfn::world::ChunkCoord;
using dfn::world::WorldGenParams;
using dfn::world::WorldGenRng;

namespace {

constexpr uint32_t RESOLUTION = static_cast<uint32_t>(dfn::config::HEIGHTMAP_RESOLUTION);

// FNV-1a 64 digest of every raw height sample of a 3x3 chunk region,
// row-major over chunks and samples (fixed order = comparable digests).
uint64_t region_height_hash(uint64_t seed) {
    const WorldGenParams params{seed, ChunkCoord{-1, -1}, ChunkCoord{1, 1}};
    Fnv1a64 hash;
    for (int32_t cz = -1; cz <= 1; ++cz) {
        for (int32_t cx = -1; cx <= 1; ++cx) {
            const auto chunk = dfn::world::generate_chunk(params, ChunkCoord{cx, cz});
            REQUIRE(chunk.heightmap.samples.size()
                    == static_cast<std::size_t>(RESOLUTION) * RESOLUTION);
            for (const uint16_t s : chunk.heightmap.samples) {
                hash.update_u64(s);
            }
        }
    }
    return hash.digest();
}

} // namespace

TEST_CASE("Rule 13.1: same seed -> byte-identical heights, twice") {
    const uint64_t first = region_height_hash(0xDA66E12FA11ull);
    const uint64_t second = region_height_hash(0xDA66E12FA11ull);
    CHECK(first == second);
}

TEST_CASE("Rule 13.1: different seeds -> different worlds") {
    CHECK(region_height_hash(1) != region_height_hash(2));
    CHECK(region_height_hash(0) != region_height_hash(1));
}

TEST_CASE("chunk generation is independent of generation order") {
    const WorldGenParams params{42, ChunkCoord{-8, -8}, ChunkCoord{8, 8}};
    // Generate (3, -2) cold, then after generating unrelated chunks — identical.
    const auto direct = dfn::world::generate_chunk(params, ChunkCoord{3, -2});
    (void)dfn::world::generate_chunk(params, ChunkCoord{0, 0});
    (void)dfn::world::generate_chunk(params, ChunkCoord{-5, 7});
    const auto again = dfn::world::generate_chunk(params, ChunkCoord{3, -2});
    CHECK(direct.heightmap.samples == again.heightmap.samples);
    CHECK(direct.heightmap.height_scale == doctest::Approx(again.heightmap.height_scale));
    CHECK(direct.heightmap.height_offset == doctest::Approx(again.heightmap.height_offset));
}

TEST_CASE("shared chunk edges stitch exactly (HeightFieldView contract)") {
    const WorldGenParams params{7, ChunkCoord{-2, -2}, ChunkCoord{2, 2}};
    const auto left = dfn::world::generate_chunk(params, ChunkCoord{0, 0});
    const auto right = dfn::world::generate_chunk(params, ChunkCoord{1, 0});
    const auto below = dfn::world::generate_chunk(params, ChunkCoord{0, 1});

    for (uint32_t z = 0; z < RESOLUTION; ++z) {
        // Column x = 128 of (0,0) == column x = 0 of (1,0) — raw equality.
        CHECK(left.heightmap.samples[z * RESOLUTION + (RESOLUTION - 1)]
              == right.heightmap.samples[z * RESOLUTION + 0]);
    }
    for (uint32_t x = 0; x < RESOLUTION; ++x) {
        // Row z = 128 of (0,0) == row z = 0 of (0,1).
        CHECK(left.heightmap.samples[(RESOLUTION - 1) * RESOLUTION + x]
              == below.heightmap.samples[0 * RESOLUTION + x]);
    }
    // Same quantization everywhere (edge-equality precondition).
    CHECK(left.heightmap.height_scale == doctest::Approx(right.heightmap.height_scale));
    CHECK(left.heightmap.height_offset == doctest::Approx(right.heightmap.height_offset));
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

TEST_CASE("generate_world reports deferred file IO (stage 2)") {
    const auto result =
        dfn::world::generate_world(WorldGenParams{1, {0, 0}, {0, 0}}, "/tmp/unused.dfw");
    CHECK_FALSE(result.ok);
    CHECK_FALSE(result.error.empty());
}
