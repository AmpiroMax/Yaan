/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 16:47:51
Module: engine/world
File: engine/world/sources/Worldgen.cpp

Responsibility:
- Worldgen v2 orchestration (Rule 13.1: same seed, byte-identical output):
  world context (P2 hydrology + P4 sites over the P1 macro field), per-chunk
  generation — final heights quantized in the shared WORLDGEN_MAX_HEIGHT
  range, P3 surface arrays, P4 entity records, P5 scatter instances.

Key items:
- WorldGenRng (SplitMix64), build_world_context, terrain_height,
  surface_point, generate_chunk, generate_world (file output still deferred).

Dependencies:
- Uses: Worldgen.h, WorldgenMacro/Hydrology/Sites/Scatter, WorldgenNoise,
  generated constants.
- Used by: dfn_world, worldgen tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DETERMINISM IS NON-NEGOTIABLE (Rule 13.1): all randomness flows from mix64
  streams; no std::rand, no platform-dependent paths.
- All chunks share ONE quantization range (offset 0, scale
  WORLDGEN_MAX_HEIGHT/65535) so shared edge samples decode identically across
  neighbors — exact-stitch guarantee of the HeightFieldView contract. Do not
  "optimize" to per-chunk min/max without a group sync (it would break edge
  equality). Range raised 31.5 -> 64 m at stage 3b (lead-acked, sync note).
- Heights/surface/classification are pure functions of (params, world_xz) —
  chunk-independent by construction. Keep it that way.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — value-noise gentle hills, SplitMix64 rng;
  generate_world returns a deferred-IO error until stage 3.
- 09:08:2026 - 11:05:22: Stage 3b — worldgen v2: P1 stamps via WorldgenMacro
  (octaves from dfn::config, local table deleted), P2 hydrology, P3 surface
  arrays, P4 site records, P5 scatter; WORLDGEN_MAX_HEIGHT quantization.
- 09:08:2026 - 13:12:19: Stage 3b amendments: grid-pass generate_chunk (water/heights once per node, slope from the grid, analytic border) — bit-identical to surface_point, ~5x fewer field evals; equality pinned by test.
- 09:08:2026 - 16:30:44: Representation swap: generate_chunk builds the voxel volume from the heightmap it just wrote, extracts the surface, and drops the volume.
- 09:08:2026 - 16:47:51: P7: carves passed to the volume build.
*/

#include "engine/world/sources/Worldgen.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenNoise.h"
#include "engine/world/sources/VoxelMesh.h"
#include "engine/world/sources/VoxelVolume.h"
#include "engine/world/sources/WorldgenScatter.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dfn::world {

namespace {

constexpr uint32_t RESOLUTION = static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION);
constexpr float STEP_M = static_cast<float>(config::HEIGHTMAP_STEP);
constexpr float CHUNK_SIZE_M = static_cast<float>(config::CHUNK_SIZE);
constexpr float MAX_HEIGHT_M = static_cast<float>(config::WORLDGEN_MAX_HEIGHT);
constexpr float SLOPE_GRASS = static_cast<float>(config::SLOPE_GRASS_MAX);
constexpr float SLOPE_ROCK = static_cast<float>(config::SLOPE_ROCK_MIN);
constexpr float SAND_DIST = static_cast<float>(config::SHORE_SAND_DIST);
constexpr float SAND_HEIGHT = static_cast<float>(config::SHORE_SAND_HEIGHT);

} // namespace

// --- WorldGenRng --------------------------------------------------------------

WorldGenRng WorldGenRng::for_chunk(uint64_t seed, ChunkCoord coord, uint32_t pass_tag) {
    uint64_t s = noise::mix64(seed ^ 0x8BADF00D5EEDC0DEull);
    s = noise::mix64(s ^ static_cast<uint64_t>(static_cast<uint32_t>(coord.x)));
    s = noise::mix64(s ^ static_cast<uint64_t>(static_cast<uint32_t>(coord.z)));
    s = noise::mix64(s ^ pass_tag);
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

// --- World-level context -------------------------------------------------------

WorldGenContext build_world_context(const WorldGenParams& params) {
    WorldGenContext ctx;
    ctx.params = params;
    const glm::vec2 domain_min{static_cast<float>(params.min_chunk.x) * CHUNK_SIZE_M,
                               static_cast<float>(params.min_chunk.z) * CHUNK_SIZE_M};
    const glm::vec2 domain_max{static_cast<float>(params.max_chunk.x + 1) * CHUNK_SIZE_M,
                               static_cast<float>(params.max_chunk.z + 1) * CHUNK_SIZE_M};
    ctx.hydrology = build_hydrology(params.seed, params.layout, domain_min, domain_max);
    ctx.sites = build_sites(params.seed, params.layout, ctx.hydrology);
    return ctx;
}

float terrain_height(const WorldGenContext& ctx, glm::vec2 world) {
    const float macro = macro_height(ctx.params.seed, ctx.params.layout, world);
    const float carved = carve_height(ctx.hydrology, ctx.params.layout, world, macro);
    const float padded = pads_height(ctx.sites, world, carved);
    return std::clamp(padded, 0.0f, MAX_HEIGHT_M);
}

SurfacePoint surface_point(const WorldGenContext& ctx, glm::vec2 world) {
    const TestbedLayout& layout = ctx.params.layout;
    const float macro = macro_height(ctx.params.seed, layout, world);
    const WaterSample water = water_at(ctx.hydrology, layout, world, macro);
    const float h = std::clamp(pads_height(ctx.sites, world, water.height), 0.0f, MAX_HEIGHT_M);

    SurfacePoint out;
    out.height = h;
    out.dist_to_water = water.dist_to_water;
    const bool covered = water.water_surface != math::NO_WATER && h < water.water_surface;
    out.water_surface = covered ? water.water_surface : math::NO_WATER;

    // Slope from central differences of the FINAL height field (position-based
    // — identical on shared chunk edges even though neighbors lie outside the
    // chunk being generated).
    const float hx = terrain_height(ctx, {world.x + STEP_M, world.y})
                   - terrain_height(ctx, {world.x - STEP_M, world.y});
    const float hz = terrain_height(ctx, {world.x, world.y + STEP_M})
                   - terrain_height(ctx, {world.x, world.y - STEP_M});
    const float slope = std::atan(std::sqrt(hx * hx + hz * hz) / (2.0f * STEP_M));

    // Priority rules per LANDSCAPE §4 (first match wins).
    if (covered) {
        out.surface_class = math::SurfaceClass::WaterBed;
    } else if (water.dist_to_water <= SAND_DIST && water.near_level != math::NO_WATER
               && h - water.near_level <= SAND_HEIGHT) {
        out.surface_class = math::SurfaceClass::Sand;
    } else if (slope >= SLOPE_ROCK
               || (crag_distance(layout, world) < layout.crag.radius
                   && h >= layout.crag.rockline)) {
        out.surface_class = math::SurfaceClass::Rock;
    } else if (slope >= SLOPE_GRASS) {
        out.surface_class = math::SurfaceClass::GrassRockBlend;
    } else {
        out.surface_class = math::SurfaceClass::Grass;
    }
    return out;
}

// --- Generation ----------------------------------------------------------------

Chunk generate_chunk(const WorldGenContext& ctx, ChunkCoord coord) {
    Chunk chunk;
    chunk.coord = coord;

    Heightmap& hm = chunk.heightmap;
    hm.height_offset = 0.0f;
    hm.height_scale = MAX_HEIGHT_M / 65535.0f; // shared by all chunks (edge equality)
    const std::size_t sample_count = static_cast<std::size_t>(RESOLUTION) * RESOLUTION;
    hm.samples.resize(sample_count);
    chunk.surface.dist_to_water.resize(sample_count);
    chunk.surface.water_surface.resize(sample_count);
    chunk.surface.surface_class.resize(sample_count);

    const glm::vec2 origin{static_cast<float>(coord.x) * CHUNK_SIZE_M,
                           static_cast<float>(coord.z) * CHUNK_SIZE_M};
    const auto world_at = [&](uint32_t x, uint32_t z) {
        return origin
             + glm::vec2{static_cast<float>(x) * STEP_M, static_cast<float>(z) * STEP_M};
    };
    const TestbedLayout& layout = ctx.params.layout;

    // Grid-pass generation (bit-identical to per-sample surface_point calls —
    // every value below is the same pure position-based function, evaluated
    // once instead of five times per sample):
    // pass A: water samples (carve heights) + final heights per grid node.
    std::vector<WaterSample> water(sample_count);
    std::vector<float> final_h(sample_count);
    for (uint32_t z = 0; z < RESOLUTION; ++z) {
        for (uint32_t x = 0; x < RESOLUTION; ++x) {
            const glm::vec2 world = world_at(x, z);
            const std::size_t i = static_cast<std::size_t>(z) * RESOLUTION + x;
            water[i] = water_at(ctx.hydrology, layout, world,
                                macro_height(ctx.params.seed, layout, world));
            final_h[i] = std::clamp(pads_height(ctx.sites, world, water[i].height), 0.0f,
                                    MAX_HEIGHT_M);
        }
    }
    // pass B: quantize + classify. Slope uses the grid where the +-STEP
    // neighbor is inside the chunk and the analytic field on the border —
    // identical floats either way (position-based), so shared edges agree.
    for (uint32_t z = 0; z < RESOLUTION; ++z) {
        for (uint32_t x = 0; x < RESOLUTION; ++x) {
            const glm::vec2 world = world_at(x, z);
            const std::size_t i = static_cast<std::size_t>(z) * RESOLUTION + x;
            const float h = final_h[i];
            const float raw = std::round(h / MAX_HEIGHT_M * 65535.0f);
            hm.samples[i] = static_cast<uint16_t>(std::clamp(raw, 0.0f, 65535.0f));

            const auto h_at = [&](int32_t nx, int32_t nz) {
                if (nx >= 0 && nz >= 0 && nx < static_cast<int32_t>(RESOLUTION)
                    && nz < static_cast<int32_t>(RESOLUTION)) {
                    return final_h[static_cast<std::size_t>(nz) * RESOLUTION
                                   + static_cast<std::size_t>(nx)];
                }
                return terrain_height(ctx, {world.x + static_cast<float>(nx - static_cast<int32_t>(x)) * STEP_M,
                                            world.y + static_cast<float>(nz - static_cast<int32_t>(z)) * STEP_M});
            };
            const int32_t ix = static_cast<int32_t>(x);
            const int32_t iz = static_cast<int32_t>(z);
            const float hx = h_at(ix + 1, iz) - h_at(ix - 1, iz);
            const float hz = h_at(ix, iz + 1) - h_at(ix, iz - 1);
            const float slope = std::atan(std::sqrt(hx * hx + hz * hz) / (2.0f * STEP_M));

            const WaterSample& w = water[i];
            const bool covered = w.water_surface != math::NO_WATER && h < w.water_surface;
            chunk.surface.dist_to_water[i] = w.dist_to_water;
            chunk.surface.water_surface[i] = covered ? w.water_surface : math::NO_WATER;

            math::SurfaceClass cls = math::SurfaceClass::Grass;
            if (covered) {
                cls = math::SurfaceClass::WaterBed;
            } else if (w.dist_to_water <= SAND_DIST && w.near_level != math::NO_WATER
                       && h - w.near_level <= SAND_HEIGHT) {
                cls = math::SurfaceClass::Sand;
            } else if (slope >= SLOPE_ROCK
                       || (crag_distance(layout, world) < layout.crag.radius
                           && h >= layout.crag.rockline)) {
                cls = math::SurfaceClass::Rock;
            } else if (slope >= SLOPE_GRASS) {
                cls = math::SurfaceClass::GrassRockBlend;
            }
            chunk.surface.surface_class[i] = static_cast<uint8_t>(cls);
        }
    }

    // P4 entities whose position falls inside this chunk (half-open bounds —
    // no duplicates across neighbors). Order preserved => deterministic ids.
    const glm::vec2 chunk_max = origin + glm::vec2{CHUNK_SIZE_M, CHUNK_SIZE_M};
    for (const GeneratedEntityRecord& rec : ctx.sites.entities) {
        if (rec.position_xz.x >= origin.x && rec.position_xz.x < chunk_max.x
            && rec.position_xz.y >= origin.y && rec.position_xz.y < chunk_max.y) {
            chunk.entities.push_back(rec);
        }
    }

    // P5 scatter instances for this chunk.
    chunk.scatter = build_scatter(ctx.params.seed, ctx.params.layout, ctx.hydrology,
                                  ctx.sites, origin, chunk_max);

    // 3D terrain: build the voxel volume from the heightmap just written, then
    // extract its surface and DROP the volume — the world is not destructible,
    // so only the geometry stays resident.
    {
        const VoxelVolume volume = build_voxel_volume(
            chunk, [&ctx](glm::vec2 p) { return terrain_height(ctx, p); },
            ctx.params.layout);
        VoxelMeshData mesh = extract_surface_nets(volume);
        chunk.voxels.positions = std::move(mesh.positions);
        chunk.voxels.normals = std::move(mesh.normals);
        chunk.voxels.materials = std::move(mesh.materials);
        chunk.voxels.indices = std::move(mesh.indices);
    }
    return chunk;
}

Chunk generate_chunk(const WorldGenParams& params, ChunkCoord coord) {
    return generate_chunk(build_world_context(params), coord);
}

WorldGenResult generate_world(const WorldGenParams& params,
                              const std::filesystem::path& out_file) {
    (void)params;
    (void)out_file;
    // World file IO is deferred (lead directive: streaming uses the in-memory
    // generator via ChunkManager::open_generated; WorldFormat stays headers-only).
    return WorldGenResult{false,
                          "generate_world: .dfw output arrives with world file IO; use "
                          "ChunkManager::open_generated (in-memory chunks)"};
}

} // namespace dfn::world
