/*
Created: 09:08:2026 - 00:42:03
Last updated: 10:08:2026 - 02:59:28
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
- 09:08:2026 - 17:36:42: §6.2: entrance works applied between hydrology and pads; derived adits passed to the voxel build.
- 09:08:2026 - 19:55:17: Barrow re-siting (design ruling): swing_barrow_into_couloir searches the arc for a re-entrant fold at the same radius and rigidly rotates site, passage and chamber together. On Ravenscar it finds nothing — the stamp is a smooth radial cone with no angular structure — so the barrow stays authored and its mouth test stays red. Design's high-shoulder fallback was implemented, MEASURED and then removed: it broke story's hard constraint (mouth visible from 26 of 39 Vaelmere standpoints) and put the lifted chamber through the crag tunnel (10 stations with no floor).
- 10:08:2026 - 02:29:54: build_world_context derives daylight portals (open_daylight_portals) after the couloir swing, against the pre-P4 sampler (macro + water carve) — same layout copy every consumer reads, so the extended corridor is one fact.
- 10:08:2026 - 02:59:28: Stand selector (§8.1): build_world_context branches for StandId::Forest — empty hydrology (a waterless stand's VALID P2, ok=true), empty sites; the stand's own passes land with the erosion/path commits. Testbed path untouched.
*/

#include "engine/world/sources/Worldgen.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenCarve.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenNoise.h"
#include "engine/world/sources/VoxelMesh.h"
#include "engine/world/sources/VoxelVolume.h"
#include "engine/world/sources/WorldgenScatter.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
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

namespace {

/// §7.1 coordinates are stamps against a SPECIFIC terrain state: anything sited
/// on a landmark's slopes carries an implicit dependency on that landmark's
/// relief. Raising Ravenscar 52 -> 115 m buried the Backbarrow under 20-44 m of
/// its own mountain. Design's ruling: the barrow does not move OUTWARD (that
/// drags the castle with it and cascades five ways), it moves AROUND — into a
/// couloir, one of the re-entrant folds a ridged stamp produces between its
/// buttress ridges, where ground at the SAME radius is still near valley level.
/// A rigid rotation about the crag centre carries the authored passage and
/// chamber with the site, so the barrow keeps its designed shape.
void swing_barrow_into_couloir(TestbedLayout& layout, uint64_t seed,
                               const HydrologyData& hydro) {
    const int si = layout.carves.barrow_site_index;
    if (si < 0 || si >= static_cast<int>(std::size(layout.sites))) {
        return;
    }
    const glm::vec2 cc = layout.crag.center;
    const glm::vec2 rel = layout.sites[si].position - cc;
    const float radius = glm::length(rel);
    if (radius < 1.0f || radius > layout.crag.radius) {
        return; // not sited on the massif
    }
    const auto ground = [&](glm::vec2 q) {
        return carve_height(hydro, layout, q, macro_height(seed, layout, q));
    };
    // Valley reference along this bearing, just outside the stamp.
    const glm::vec2 dir = rel / radius;
    const float valley = ground(cc + dir * (layout.crag.radius + 40.0f));
    const float threshold = valley + 8.0f;
    if (ground(layout.sites[si].position) <= threshold) {
        return; // already clear; nothing to swing
    }
    // Search the arc outward from the authored bearing so the FIRST hit is the
    // nearest one — the barrow moves as little as the mountain allows.
    const float base = std::atan2(rel.y, rel.x);
    float found = 0.0f;
    bool ok = false;
    for (float dev = 2.0f * 0.0174532925f; dev <= 30.0f * 0.0174532925f && !ok;
         dev += 2.0f * 0.0174532925f) {
        for (const float sign : {1.0f, -1.0f}) {
            const float ang = base + sign * dev;
            const glm::vec2 probe = cc + glm::vec2{std::cos(ang), std::sin(ang)} * radius;
            if (ground(probe) <= threshold) {
                found = sign * dev;
                ok = true;
                break;
            }
        }
    }
    if (!ok) {
        // NO COULOIR EXISTS on this massif, and design's high-shoulder
        // fallback is NOT taken, because measuring it showed it breaks two
        // things rather than one:
        //   (a) story's hard constraint — a mouth lifted to ~39 m is visible
        //       from 26 of 39 Vaelmere standpoints, and MQ1 depends on the
        //       player FINDING the grave rather than seeing it from town;
        //   (b) the lifted chamber intersects the crag switchback tunnel,
        //       leaving 10 stations of ascent with no floor underfoot.
        // The couloir search itself is correct and stays: Ravenscar simply has
        // no angular structure to search — its stamp is a smooth radial cone
        // (measured: identical lobe ratio at every height), so re-entrant folds
        // do not exist yet. Once the pending crag SHAPE ruling adds angular
        // ridge modulation, couloirs appear and this finds one. Until then the
        // barrow stays where it was authored and its mouth test stays red,
        // which is the honest state.
        return;
    }
    // Rigid rotation about the crag centre: site, passage and chamber together.
    const float cs = std::cos(found);
    const float sn = std::sin(found);
    const auto swing = [&](glm::vec2 q) {
        const glm::vec2 r = q - cc;
        return cc + glm::vec2{r.x * cs - r.y * sn, r.x * sn + r.y * cs};
    };
    layout.sites[si].position = swing(layout.sites[si].position);
    CarveCorridor& passage = layout.carves.barrow_passage;
    for (int i = 0; i < passage.point_count; ++i) {
        const glm::vec2 xz = swing({passage.points[i].x, passage.points[i].z});
        passage.points[i].x = xz.x;
        passage.points[i].z = xz.y;
    }
    CarveChamber& ch = layout.carves.barrow_chamber;
    const glm::vec2 cxz = swing({ch.center.x, ch.center.z});
    ch.center.x = cxz.x;
    ch.center.z = cxz.y;
}

} // namespace

WorldGenContext build_world_context(const WorldGenParams& params) {
    WorldGenContext ctx;
    ctx.params = params;
    const glm::vec2 domain_min{static_cast<float>(params.min_chunk.x) * CHUNK_SIZE_M,
                               static_cast<float>(params.min_chunk.z) * CHUNK_SIZE_M};
    const glm::vec2 domain_max{static_cast<float>(params.max_chunk.x + 1) * CHUNK_SIZE_M,
                               static_cast<float>(params.max_chunk.z + 1) * CHUNK_SIZE_M};
    if (params.layout.stand == StandId::Forest) {
        // §8.1: the forest stand declares NO water landform (LF-3/LF-6 absent
        // from its composition), so P2 stays empty — water_at then passes
        // heights through and reports far-field distance everywhere. ok=true
        // because an empty hydrology is this stand's VALID hydrology, not a
        // failed trace. P4 sites stay empty too: the stand's goals belong to
        // the §8.1 path network (built in the stand passes below), not to the
        // testbed site table.
        ctx.hydrology.ok = true;
        return ctx;
    }
    ctx.hydrology = build_hydrology(params.seed, params.layout, domain_min, domain_max);
    // Re-validate placements that sit on the L0's slopes BEFORE anything is
    // sited against them (design's durable rule: re-validation is part of a
    // landmark change, not a follow-up).
    swing_barrow_into_couloir(ctx.params.layout, params.seed, ctx.hydrology);
    // Derive the daylight ends of flagged carve corridors against the terrain
    // that actually ships (macro + water carve — the same pre-P4 sampler the
    // carve mouths use). The §2.8 massif re-buried the surveyed tunnel exit;
    // an endpoint that must stand in open air is derived, never re-surveyed.
    {
        const uint64_t seed = params.seed;
        const TestbedLayout& lay = ctx.params.layout;
        const HydrologyData& hydro = ctx.hydrology;
        open_daylight_portals(ctx.params.layout, [&](glm::vec2 p) {
            return carve_height(hydro, lay, p, macro_height(seed, lay, p));
        });
    }
    ctx.sites = build_sites(params.seed, ctx.params.layout, ctx.hydrology);
    return ctx;
}

float terrain_height(const WorldGenContext& ctx, glm::vec2 world) {
    const float macro = macro_height(ctx.params.seed, ctx.params.layout, world);
    const float carved = carve_height(ctx.hydrology, ctx.params.layout, world, macro);
    const float worked = entrance_works_height(ctx.sites, world, carved);
    const float padded = pads_height(ctx.sites, world, worked);
    return std::clamp(padded, 0.0f, MAX_HEIGHT_M);
}

float terrain_slope(const WorldGenContext& ctx, glm::vec2 world) {
    // Central differences of the FINAL height field (position-based — identical
    // on shared chunk edges even though neighbors lie outside the chunk being
    // generated, and identical at every LOD level for the same reason).
    const float hx = terrain_height(ctx, {world.x + STEP_M, world.y})
                   - terrain_height(ctx, {world.x - STEP_M, world.y});
    const float hz = terrain_height(ctx, {world.x, world.y + STEP_M})
                   - terrain_height(ctx, {world.x, world.y - STEP_M});
    return std::atan(std::sqrt(hx * hx + hz * hz) / (2.0f * STEP_M));
}

math::SurfaceClass classify_surface(const TestbedLayout& layout, glm::vec2 world,
                                    float height, const WaterSample& water,
                                    float slope_rad) {
    // Priority rules per LANDSCAPE §4 (first match wins).
    const bool covered = water.water_surface != math::NO_WATER && height < water.water_surface;
    if (covered) {
        return math::SurfaceClass::WaterBed;
    }
    if (water.dist_to_water <= SAND_DIST && water.near_level != math::NO_WATER
        && height - water.near_level <= SAND_HEIGHT) {
        return math::SurfaceClass::Sand;
    }
    if (slope_rad >= SLOPE_ROCK
        || (crag_distance(layout, world) < layout.crag.radius
            && height >= layout.crag.rockline)) {
        return math::SurfaceClass::Rock;
    }
    if (slope_rad >= SLOPE_GRASS) {
        return math::SurfaceClass::GrassRockBlend;
    }
    return math::SurfaceClass::Grass;
}

SurfacePoint surface_point(const WorldGenContext& ctx, glm::vec2 world) {
    const TestbedLayout& layout = ctx.params.layout;
    const float macro = macro_height(ctx.params.seed, layout, world);
    const WaterSample water = water_at(ctx.hydrology, layout, world, macro);
    const float h = std::clamp(
        pads_height(ctx.sites, world, entrance_works_height(ctx.sites, world, water.height)),
        0.0f, MAX_HEIGHT_M);

    SurfacePoint out;
    out.height = h;
    out.dist_to_water = water.dist_to_water;
    const bool covered = water.water_surface != math::NO_WATER && h < water.water_surface;
    out.water_surface = covered ? water.water_surface : math::NO_WATER;
    out.surface_class = classify_surface(layout, world, h, water, terrain_slope(ctx, world));
    return out;
}

// --- Generation ----------------------------------------------------------------

Chunk generate_chunk(const WorldGenContext& ctx, ChunkCoord coord) {
    Chunk chunk;
    chunk.coord = coord;

    Heightmap& hm = chunk.heightmap;
    // The SHARED quantization (Chunk.h): every chunk and every coarse LOD node
    // decode with the same offset/scale, which is what makes a sample two grids
    // share bit-identical rather than merely close.
    hm.height_offset = HEIGHT_QUANT_OFFSET;
    hm.height_scale = HEIGHT_QUANT_SCALE;
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
            final_h[i] = std::clamp(
                pads_height(ctx.sites, world,
                            entrance_works_height(ctx.sites, world, water[i].height)),
                0.0f, MAX_HEIGHT_M);
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
            hm.samples[i] = quantize_height(h);

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

            chunk.surface.surface_class[i] =
                static_cast<uint8_t>(classify_surface(layout, world, h, w, slope));
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
        std::vector<CarveCorridor> derived;
        for (const EntranceWorks& w : ctx.sites.entrances) {
            if (w.valid && w.adit.point_count > 1) {
                derived.push_back(w.adit);
            }
        }
        const VoxelVolume volume = build_voxel_volume(
            chunk, [&ctx](glm::vec2 p) { return terrain_height(ctx, p); },
            ctx.params.layout, derived);
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
