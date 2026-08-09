/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 00:42:03
Module: engine/world
File: engine/world/sources/Worldgen.cpp

Responsibility:
- Stage-2 deterministic worldgen: gentle-hills heightmaps from seeded value
  noise (Rule 13.1 — same seed, byte-identical output), SplitMix64 RNG streams.

Key items:
- WorldGenRng (SplitMix64), generate_chunk (value-noise fBm), generate_world
  (file output deferred to stage 3).

Dependencies:
- Uses: Worldgen.h, generated constants.
- Used by: dfn_world, worldgen determinism test.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM IS NON-NEGOTIABLE (Rule 13.1): all randomness flows from mix64 of
  (seed, lattice coords, octave); no std::rand, no platform-dependent paths.
- All chunks share ONE quantization range (offset 0, scale AMPLITUDE/65535) so
  shared edge samples decode identically across neighbors — exact-stitch
  guarantee of the HeightFieldView contract. Do not "optimize" to per-chunk
  min/max without a group sync (it would break edge equality).
- Terrain shape numbers below are worldgen-internal algorithm constants,
  flagged for NUMBERS.md at the next sync; do not add gameplay constants here.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — value-noise gentle hills, SplitMix64 rng;
  generate_world returns a deferred-IO error until stage 3.
*/

#include "engine/world/sources/Worldgen.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {

constexpr uint32_t RESOLUTION = static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION);
constexpr float STEP_M = static_cast<float>(config::HEIGHTMAP_STEP);
constexpr float CHUNK_SIZE_M = static_cast<float>(config::CHUNK_SIZE);

// Gentle-hills shape (worldgen-internal; candidates for NUMBERS.md at the next
// sync). Octaves: (lattice cell size in meters, amplitude in meters).
struct Octave {
    float cell_size;
    float amplitude;
};
constexpr Octave OCTAVES[] = {
    {512.0f, 24.0f}, // broad rolling hills
    {128.0f, 6.0f},  // mid-scale variation
    {32.0f, 1.5f},   // fine detail
};
// Sum of amplitudes — the fixed quantization range shared by ALL chunks
// (offset 0), guaranteeing exact decoded-height equality on shared edges.
constexpr float MAX_HEIGHT_M = 24.0f + 6.0f + 1.5f;

// SplitMix64 finalizer — the single mixing primitive of all worldgen hashing.
constexpr uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

// Deterministic lattice value in [0, 1) for integer lattice point (gx, gz).
float lattice_value(uint64_t seed, uint32_t octave, int64_t gx, int64_t gz) {
    uint64_t h = mix64(seed ^ (0xA24BAED4963EE407ull + octave));
    h = mix64(h ^ static_cast<uint64_t>(gx));
    h = mix64(h ^ static_cast<uint64_t>(gz));
    return static_cast<float>(h >> 40) * (1.0f / 16777216.0f); // top 24 bits
}

float smoothstep01(float t) { return t * t * (3.0f - 2.0f * t); }

// Bilinear value noise in [0, 1) at world position, on a lattice of cell_size.
float value_noise(uint64_t seed, uint32_t octave, float cell_size, glm::vec2 world) {
    const float cx = world.x / cell_size;
    const float cz = world.y / cell_size;
    const int64_t gx = static_cast<int64_t>(std::floor(cx));
    const int64_t gz = static_cast<int64_t>(std::floor(cz));
    const float tx = smoothstep01(cx - static_cast<float>(gx));
    const float tz = smoothstep01(cz - static_cast<float>(gz));

    const float v00 = lattice_value(seed, octave, gx, gz);
    const float v10 = lattice_value(seed, octave, gx + 1, gz);
    const float v01 = lattice_value(seed, octave, gx, gz + 1);
    const float v11 = lattice_value(seed, octave, gx + 1, gz + 1);
    const float v0 = v00 + (v10 - v00) * tx;
    const float v1 = v01 + (v11 - v01) * tx;
    return v0 + (v1 - v0) * tz;
}

// Terrain height in meters at a world position. Position-based (not
// chunk-based), so neighboring chunks sample identical values on shared edges.
float terrain_height(uint64_t seed, glm::vec2 world) {
    float h = 0.0f;
    uint32_t octave = 0;
    for (const Octave& o : OCTAVES) {
        h += value_noise(seed, octave, o.cell_size, world) * o.amplitude;
        ++octave;
    }
    return h;
}

} // namespace

// --- WorldGenRng --------------------------------------------------------------

WorldGenRng WorldGenRng::for_chunk(uint64_t seed, ChunkCoord coord, uint32_t pass_tag) {
    uint64_t s = mix64(seed ^ 0x8BADF00D5EEDC0DEull);
    s = mix64(s ^ static_cast<uint64_t>(static_cast<uint32_t>(coord.x)));
    s = mix64(s ^ static_cast<uint64_t>(static_cast<uint32_t>(coord.z)));
    s = mix64(s ^ pass_tag);
    return WorldGenRng{s};
}

uint64_t WorldGenRng::next_u64() {
    state += 0x9E3779B97F4A7C15ull;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

float WorldGenRng::next_float01() {
    return static_cast<float>(next_u64() >> 40) * (1.0f / 16777216.0f);
}

uint32_t WorldGenRng::next_range(uint32_t min, uint32_t max) {
    const uint64_t span = static_cast<uint64_t>(max) - min + 1;
    // Rejection sampling: no modulo bias.
    const uint64_t limit = UINT64_MAX - (UINT64_MAX % span);
    uint64_t v = next_u64();
    while (v >= limit) {
        v = next_u64();
    }
    return min + static_cast<uint32_t>(v % span);
}

// --- Generation ----------------------------------------------------------------

Chunk generate_chunk(const WorldGenParams& params, ChunkCoord coord) {
    Chunk chunk;
    chunk.coord = coord;

    Heightmap& hm = chunk.heightmap;
    hm.height_offset = 0.0f;
    hm.height_scale = MAX_HEIGHT_M / 65535.0f; // shared by all chunks (edge equality)
    hm.samples.resize(static_cast<std::size_t>(RESOLUTION) * RESOLUTION);

    const glm::vec2 origin{static_cast<float>(coord.x) * CHUNK_SIZE_M,
                           static_cast<float>(coord.z) * CHUNK_SIZE_M};
    for (uint32_t z = 0; z < RESOLUTION; ++z) {
        for (uint32_t x = 0; x < RESOLUTION; ++x) {
            const glm::vec2 world = origin + glm::vec2{static_cast<float>(x) * STEP_M,
                                                       static_cast<float>(z) * STEP_M};
            const float h = std::clamp(terrain_height(params.seed, world), 0.0f, MAX_HEIGHT_M);
            const float raw = std::round(h / MAX_HEIGHT_M * 65535.0f);
            hm.samples[static_cast<std::size_t>(z) * RESOLUTION + x] =
                static_cast<uint16_t>(raw);
        }
    }

    // Stage 2: terrain only — no generated entities yet (props/NPC spawns are a
    // later worldgen pass; the streaming batch path is exercised regardless).
    return chunk;
}

WorldGenResult generate_world(const WorldGenParams& params,
                              const std::filesystem::path& out_file) {
    (void)params;
    (void)out_file;
    // World file IO is deferred to stage 3 (lead directive: stage 2 streams from
    // the in-memory generator; WorldFormat stays headers-only).
    return WorldGenResult{false,
                          "generate_world: .dfw output arrives in stage 3; stage 2 uses "
                          "ChunkManager::open_generated (in-memory chunks)"};
}

} // namespace dfn::world
