/*
Module: engine/render
File: engine/render/sources/ProcTexture.cpp

Responsibility:
- Procedural texture implementation: integer-hash periodic value noise, periodic
  cellular (Worley) fields, per-kind recipes (grass, rock, sand, dirt, water,
  and the four §8.1 path surfaces), ramp quantization, atlas assembly.

Key items:
- tileable_fbm(); tileable_cells(); tileable_cell_id();
  generate_proc_texture(); generate_terrain_atlas(); generate_path_atlas().

Dependencies:
- Uses: ProcTexture.h, glm.
- Used by: dfn_render target; tests/render/ProcTextureTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic pure functions only: integer hashes, no float trig seeding, no
  std::rand. Byte-identical output across platforms is a test invariant.
*/

#include "engine/render/sources/ProcTexture.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace dfn::render {

namespace {

// lowbias32 (Chris Wellons) — well-mixed 32-bit integer hash.
uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

// Lattice value in [0,1] at wrapped integer coordinates.
float lattice01(int x, int y, glm::ivec2 period, uint32_t seed) {
    const auto px = static_cast<uint32_t>(((x % period.x) + period.x) % period.x);
    const auto py = static_cast<uint32_t>(((y % period.y) + period.y) % period.y);
    const uint32_t h = hash_u32(px * 0x9E3779B9U ^ hash_u32(py ^ seed * 0x85EBCA6BU));
    return static_cast<float>(h) * (1.0f / 4294967295.0f);
}

float smooth5(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); // quintic fade
}

// One octave of periodic value noise, uv in tile domain (wraps at 1).
float periodic_value_noise(glm::vec2 uv, glm::ivec2 period, uint32_t seed) {
    const glm::vec2 p{uv.x * static_cast<float>(period.x),
                      uv.y * static_cast<float>(period.y)};
    const int x0 = static_cast<int>(std::floor(p.x));
    const int y0 = static_cast<int>(std::floor(p.y));
    const float fx = smooth5(p.x - static_cast<float>(x0));
    const float fy = smooth5(p.y - static_cast<float>(y0));
    const float v00 = lattice01(x0, y0, period, seed);
    const float v10 = lattice01(x0 + 1, y0, period, seed);
    const float v01 = lattice01(x0, y0 + 1, period, seed);
    const float v11 = lattice01(x0 + 1, y0 + 1, period, seed);
    const float a = v00 + (v10 - v00) * fx;
    const float b = v01 + (v11 - v01) * fx;
    return a + (b - a) * fy;
}

// One traversal of the periodic cellular field: the two nearest feature points
// and the id of the nearest cell. `period` cells per axis; feature points are
// hashed from the WRAPPED cell coordinate (lattice01 wraps), so the field tiles
// exactly while the distances are measured in unwrapped cell units.
struct CellField {
    float d1 = 1e9f; // distance to the nearest feature point, cell units
    float d2 = 1e9f; // to the second nearest
    float id = 0.0f; // stable per-cell value in [0,1)
};

CellField cell_field(glm::vec2 uv, glm::ivec2 period, uint32_t seed, float jitter) {
    period = glm::max(period, glm::ivec2{1, 1});
    jitter = glm::clamp(jitter, 0.0f, 1.0f);
    const glm::vec2 p{uv.x * static_cast<float>(period.x),
                      uv.y * static_cast<float>(period.y)};
    const int x0 = static_cast<int>(std::floor(p.x));
    const int y0 = static_cast<int>(std::floor(p.y));
    CellField out;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int cx = x0 + dx;
            const int cy = y0 + dy;
            const float jx = lattice01(cx, cy, period, seed);
            const float jy = lattice01(cx, cy, period, seed ^ 0x1B873593U);
            const glm::vec2 f{static_cast<float>(cx) + 0.5f + (jx - 0.5f) * jitter,
                              static_cast<float>(cy) + 0.5f + (jy - 0.5f) * jitter};
            const glm::vec2 delta = p - f;
            const float d = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (d < out.d1) {
                out.d2 = out.d1;
                out.d1 = d;
                out.id = lattice01(cx, cy, period, seed ^ 0xCC9E2D51U);
            } else if (d < out.d2) {
                out.d2 = d;
            }
        }
    }
    return out;
}

// Color ramp: t in [0,1] quantized to `shades` levels across the stops.
glm::vec3 ramp_quantized(const glm::vec3* stops, int stop_count, float t, int shades) {
    t = glm::clamp(t, 0.0f, 1.0f);
    const float q = std::floor(t * static_cast<float>(shades)) /
                    static_cast<float>(shades - 1);
    const float tq = glm::clamp(q, 0.0f, 1.0f);
    const float span = tq * static_cast<float>(stop_count - 1);
    const int i = std::min(static_cast<int>(span), stop_count - 2);
    return glm::mix(stops[i], stops[i + 1], span - static_cast<float>(i));
}

uint32_t pack_rgba8(const glm::vec3& c) {
    const auto r = static_cast<uint32_t>(glm::clamp(c.r, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto g = static_cast<uint32_t>(glm::clamp(c.g, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto b = static_cast<uint32_t>(glm::clamp(c.b, 0.0f, 1.0f) * 255.0f + 0.5f);
    return 0xFF000000U | (b << 16) | (g << 8) | r; // RGBA8 little-endian bytes
}

// --- Per-kind field recipes (t in [0,1]) + look-dev ramps --------------------

constexpr glm::vec3 GRASS_STOPS[] = {
    {0.13f, 0.23f, 0.09f}, {0.26f, 0.38f, 0.14f}, {0.42f, 0.47f, 0.20f}};
constexpr glm::vec3 ROCK_STOPS[] = {
    {0.20f, 0.19f, 0.19f}, {0.38f, 0.37f, 0.36f}, {0.55f, 0.53f, 0.50f}};
constexpr glm::vec3 SAND_STOPS[] = {
    {0.56f, 0.47f, 0.31f}, {0.68f, 0.58f, 0.40f}, {0.78f, 0.69f, 0.51f}};
constexpr glm::vec3 DIRT_STOPS[] = {
    {0.23f, 0.16f, 0.10f}, {0.36f, 0.26f, 0.16f}, {0.48f, 0.38f, 0.25f}};
constexpr glm::vec3 WATER_STOPS[] = {
    {0.10f, 0.20f, 0.24f}, {0.16f, 0.30f, 0.34f}, {0.30f, 0.44f, 0.46f}};

// --- §8.1 path surfaces -----------------------------------------------------
//
// PACKED_EARTH is deliberately LIGHTER than DIRT (which is river-bed mud). A
// road is compacted and dusty and the forest floor around it is dark; at
// 640x360 a road reads as a PALE LINE THROUGH DARK GROUND long before any of
// its texture is resolvable, and that silhouette is the whole feature. Making
// the road the same value as the ground and relying on grain would be the
// PALETTE SIGNAL STRENGTH rule's textbook failure.
constexpr glm::vec3 COBBLE_STOPS[] = {
    {0.15f, 0.14f, 0.12f}, {0.43f, 0.40f, 0.35f}, {0.67f, 0.63f, 0.55f}};
constexpr glm::vec3 PATH_EARTH_STOPS[] = {
    {0.30f, 0.24f, 0.17f}, {0.46f, 0.38f, 0.28f}, {0.62f, 0.54f, 0.41f}};
constexpr glm::vec3 TRAIL_STOPS[] = {
    {0.24f, 0.20f, 0.13f}, {0.38f, 0.33f, 0.21f}, {0.52f, 0.46f, 0.31f}};
constexpr glm::vec3 SLAB_STOPS[] = {
    {0.14f, 0.14f, 0.15f}, {0.37f, 0.38f, 0.38f}, {0.60f, 0.61f, 0.60f}};

// THE PRE-RAMP SCALAR of the four terrain cells, extracted so the normal
// atlas differentiates the SAME field the albedo quantizes — one groove, two
// projections (the PartsAtlas contract). NOT a general height model: only the
// terrain splat kinds live here; the path/water cells shade shapes (stone id,
// bevel) whose height is not their albedo t.
float terrain_field_t(ProcTextureKind kind, glm::vec2 uv, uint32_t seed) {
    switch (kind) {
    case ProcTextureKind::GRASS: {
        // Meadow mottling + fine blade grain.
        const float patches = tileable_fbm(uv, {4, 4}, seed, 3);
        const float blades = tileable_fbm(uv, {24, 24}, seed ^ 0x51u, 2);
        return 0.55f * patches + 0.45f * blades;
    }
    case ProcTextureKind::ROCK: {
        // Grey mottling with darker crack lines (ridged high-frequency noise).
        const float base = tileable_fbm(uv, {5, 5}, seed, 4);
        const float ridge =
            1.0f - std::fabs(2.0f * tileable_fbm(uv, {9, 9}, seed ^ 0x33u, 2) - 1.0f);
        float t = 0.30f + 0.55f * base;
        if (ridge > 0.86f) {
            t -= 0.35f; // crack
        }
        return t;
    }
    case ProcTextureKind::SAND: {
        // Fine grain + faint ripple bands (integer cycle count keeps it
        // tileable; ripple kept subtle — strong bands moire at a distance).
        const float grain = tileable_fbm(uv, {32, 32}, seed ^ 0x77u, 2);
        const float warp = tileable_fbm(uv, {4, 4}, seed, 2);
        const float ripple =
            0.5f + 0.5f * std::sin((uv.y * 5.0f + warp * 0.35f) * 6.2831853f);
        return 0.62f * grain + 0.18f * ripple + 0.20f * warp;
    }
    case ProcTextureKind::DIRT: {
        const float clods = tileable_fbm(uv, {6, 6}, seed, 3);
        const float grit = tileable_fbm(uv, {28, 28}, seed ^ 0x99u, 2);
        return 0.65f * clods + 0.35f * grit;
    }
    default:
        return 0.5f; // flat: kinds outside the terrain splat carry no field
    }
}

glm::vec3 shade_texel(ProcTextureKind kind, glm::vec2 uv, uint32_t seed) {
    switch (kind) {
    case ProcTextureKind::GRASS:
        return ramp_quantized(GRASS_STOPS, 3, terrain_field_t(kind, uv, seed), 6);
    case ProcTextureKind::ROCK:
        return ramp_quantized(ROCK_STOPS, 3, terrain_field_t(kind, uv, seed), 7);
    case ProcTextureKind::SAND:
        return ramp_quantized(SAND_STOPS, 3, terrain_field_t(kind, uv, seed), 5);
    case ProcTextureKind::DIRT:
        return ramp_quantized(DIRT_STOPS, 3, terrain_field_t(kind, uv, seed), 6);
    case ProcTextureKind::WATER: {
        // Horizontally streaked waves (anisotropic period).
        const float streaks = tileable_fbm(uv, {10, 3}, seed, 3);
        const float chop = tileable_fbm(uv, {20, 8}, seed ^ 0xAAu, 2);
        const float t = 0.7f * streaks + 0.3f * chop;
        return ramp_quantized(WATER_STOPS, 3, t, 5);
    }
    case ProcTextureKind::COBBLE: {
        // Set stones: the JOINT is the feature. d2-d1 is ~0 on a cell border
        // and rises into the stone, so it is simultaneously the joint mask and
        // the dome shading. Value comes PER STONE from the cell id — a paved
        // surface is a mosaic of flat plates, and sampling noise inside a stone
        // would dissolve the plate back into gravel.
        const CellField c = cell_field(uv, {9, 9}, seed, 0.80f);
        const float joint = c.d2 - c.d1;
        const float dome = glm::clamp(joint / 0.30f, 0.0f, 1.0f);
        const float grain = tileable_fbm(uv, {40, 40}, seed ^ 0x5Au, 2);
        float t = (0.34f + 0.52f * c.id) * (0.60f + 0.40f * dome)
                + 0.10f * (grain - 0.5f);
        // ШОВ — КАНАВКА С РАСТВОРОМ, А НЕ ПРОПАСТЬ (владелец 24.08: «около
        // источника света чёрные границы на полу»). Жёсткий порог со сбросом
        // −0.45 ронял шов в самый низ рампы (альбедо 0.15 против 0.5-0.67 у
        // камня, контраст 4x), и у близкого огня обводка каждого камня
        // выворачивалась в уголь. Мягкий профиль по joint + глубина раствора
        // −0.26: шов темнее камня, но остаётся кладкой, а не трещиной в мир.
        const float mortar =
            1.0f - glm::clamp((joint - 0.035f) / 0.04f, 0.0f, 1.0f);
        t -= 0.26f * mortar;
        // 24 ступени вместо 6: палитра выключена 15.08, квантовать мостовую
        // лестницей больше не для чего — ступени сами рисовали контуры.
        return ramp_quantized(COBBLE_STOPS, 3, t, 24);
    }
    case ProcTextureKind::PACKED_EARTH: {
        // Compacted, smoothed, with pebbles worked to the surface.
        const float base = tileable_fbm(uv, {5, 5}, seed, 3);
        const float grit = tileable_fbm(uv, {36, 36}, seed ^ 0x21u, 2);
        const CellField peb = cell_field(uv, {15, 15}, seed ^ 0x8Du, 0.95f);
        float t = 0.28f + 0.46f * base + 0.18f * grit;
        if (peb.d1 < 0.17f && peb.id > 0.55f) {
            t += 0.24f; // a pebble catching the light
        }
        return ramp_quantized(PATH_EARTH_STOPS, 3, t, 6);
    }
    case ProcTextureKind::SCUFFED: {
        // The hint-path: thin earth with grass surviving IN it. The survivors
        // are a THRESHOLD, not a blend — at 640x360 a blend of earth and green
        // is mud, and a threshold is tufts.
        const float earth = tileable_fbm(uv, {6, 6}, seed, 3);
        const float tuft = tileable_fbm(uv, {17, 17}, seed ^ 0x3Cu, 2);
        // Threshold read off the dumped tile, not guessed: at 0.615 the cell
        // was ~40% green, which fights the WORN CENTRE this cell is supposed to
        // be — the trail's grass belongs at the margins, where the wear
        // gradient puts it, not in the tread.
        if (tuft > 0.680f) {
            return ramp_quantized(GRASS_STOPS, 3, 0.25f + 0.55f * tuft, 5) * 0.88f;
        }
        return ramp_quantized(TRAIL_STOPS, 3, 0.30f + 0.52f * earth, 5);
    }
    case ProcTextureKind::CUT_SLAB: {
        // Cut stone in running bond — an analytic grid rather than a cellular
        // field, because a step tread is rectangular by definition and Worley
        // cells cannot be made rectangular without giving up the tiling.
        constexpr glm::ivec2 BOND{5, 10}; // both wrap; y EVEN so the half-row
                                          // offset is consistent across the seam
        const float py = uv.y * static_cast<float>(BOND.y);
        const int row = static_cast<int>(std::floor(py));
        const float offset = (row & 1) != 0 ? 0.5f : 0.0f;
        const float px = uv.x * static_cast<float>(BOND.x) + offset;
        const int col = static_cast<int>(std::floor(px));
        const float fx = px - static_cast<float>(col);
        const float fy = py - static_cast<float>(row);
        const float dj = std::min(std::min(fx, 1.0f - fx), std::min(fy, 1.0f - fy));
        const float id = lattice01(col, row, BOND, seed ^ 0x6Bu);
        const float grain = tileable_fbm(uv, {30, 30}, seed ^ 0x6Bu, 2);
        const float bevel = glm::clamp(dj / 0.10f, 0.0f, 1.0f);
        float t = (0.36f + 0.44f * id) * (0.74f + 0.26f * bevel)
                + 0.08f * (grain - 0.5f);
        if (dj < 0.035f) {
            t -= 0.42f; // the joint
        }
        return ramp_quantized(SLAB_STOPS, 3, t, 6);
    }
    }
    return {1.0f, 0.0f, 1.0f}; // unreachable: loud magenta if it ever is
}

void write_tile(std::vector<uint8_t>& out, uint32_t out_side, uint32_t x0,
                uint32_t y0, const ProcTextureDesc& desc) {
    const float inv = 1.0f / static_cast<float>(desc.size);
    for (uint32_t y = 0; y < desc.size; ++y) {
        for (uint32_t x = 0; x < desc.size; ++x) {
            const glm::vec2 uv{(static_cast<float>(x) + 0.5f) * inv,
                               (static_cast<float>(y) + 0.5f) * inv};
            const uint32_t rgba = pack_rgba8(shade_texel(desc.kind, uv, desc.seed));
            const size_t o =
                (static_cast<size_t>(y0 + y) * out_side + (x0 + x)) * 4;
            out[o + 0] = static_cast<uint8_t>(rgba & 0xFF);
            out[o + 1] = static_cast<uint8_t>((rgba >> 8) & 0xFF);
            out[o + 2] = static_cast<uint8_t>((rgba >> 16) & 0xFF);
            out[o + 3] = static_cast<uint8_t>((rgba >> 24) & 0xFF);
        }
    }
}

} // namespace

float value_noise01(glm::vec2 p, uint32_t seed) {
    // Wrap far outside any world extent: 1<<20 lattice cells (~1000 km at 1 m).
    constexpr glm::ivec2 HUGE_PERIOD{1 << 20, 1 << 20};
    const glm::vec2 uv{p.x / static_cast<float>(HUGE_PERIOD.x),
                       p.y / static_cast<float>(HUGE_PERIOD.y)};
    return periodic_value_noise(uv, HUGE_PERIOD, seed);
}

float tileable_fbm(glm::vec2 uv, glm::ivec2 period, uint32_t seed, int octaves) {
    float sum = 0.0f;
    float amplitude = 0.5f;
    float total = 0.0f;
    glm::ivec2 p = period;
    for (int o = 0; o < octaves; ++o) {
        sum += amplitude * periodic_value_noise(uv, p, seed + static_cast<uint32_t>(o));
        total += amplitude;
        amplitude *= 0.5f;
        p *= 2;
    }
    return total > 0.0f ? sum / total : 0.0f;
}

std::vector<uint8_t> generate_proc_texture(const ProcTextureDesc& desc) {
    std::vector<uint8_t> pixels;
    if (desc.size == 0) {
        return pixels;
    }
    pixels.resize(static_cast<size_t>(desc.size) * desc.size * 4);
    write_tile(pixels, desc.size, 0, 0, desc);
    return pixels;
}

float tileable_cells(glm::vec2 uv, glm::ivec2 period, uint32_t seed, float jitter) {
    const CellField c = cell_field(uv, period, seed, jitter);
    return c.d2 - c.d1;
}

float tileable_cell_id(glm::vec2 uv, glm::ivec2 period, uint32_t seed, float jitter) {
    return cell_field(uv, period, seed, jitter).id;
}

float lattice_hash01(glm::ivec2 cell, glm::ivec2 period, uint32_t seed) {
    // THE LATTICE ITSELF, undecorated. Every other primitive in this file
    // INTERPOLATES between these points, and interpolation is exactly what
    // removes the energy at the tile's Nyquist: a field built from fbm is
    // smooth at the texel however many octaves it carries, so a texture built
    // only from it is flat at the PIXEL. That is the measured diagnosis of
    // MATERIALS.md §0.1, and this is the primitive that answers it.
    return lattice01(cell.x, cell.y, glm::max(period, glm::ivec2{1, 1}), seed);
}

std::vector<uint8_t> generate_terrain_atlas(uint32_t cell_size, uint32_t seed) {
    std::vector<uint8_t> pixels;
    if (cell_size == 0) {
        return pixels;
    }
    const uint32_t side = cell_size * 2;
    pixels.resize(static_cast<size_t>(side) * side * 4);
    // Layout contract with fs_terrain.sc (see header).
    const ProcTextureKind cells[] = {ProcTextureKind::GRASS, ProcTextureKind::ROCK,
                                     ProcTextureKind::SAND, ProcTextureKind::DIRT};
    for (uint32_t i = 0; i < 4; ++i) {
        ProcTextureDesc desc;
        desc.kind = cells[i];
        desc.size = cell_size;
        desc.seed = seed;
        write_tile(pixels, side, (i & 1u) * cell_size, (i >> 1u) * cell_size, desc);
    }
    return pixels;
}

std::vector<uint8_t> generate_terrain_normal_atlas(uint32_t cell_size,
                                                   uint32_t seed) {
    std::vector<uint8_t> pixels;
    if (cell_size == 0) {
        return pixels;
    }
    const uint32_t side = cell_size * 2;
    pixels.resize(static_cast<size_t>(side) * side * 4);
    // Layout mirrors generate_terrain_atlas — the shader picks the SAME cell
    // for both sheets, so the layouts drifting apart would tilt grass with
    // rock's cracks. One repeat spans 8 m of ground (32 tiles per 256 m chunk,
    // LOOKDEV_TERRAIN_TILES_PER_CHUNK).
    constexpr float SPAN_M = 8.0f;
    const float texel_m = SPAN_M / static_cast<float>(cell_size);
    // Relief per kind, metres over the full 0..1 field swing. Deliberately
    // 2-3x over honest physics, the PartsAtlas precedent (its own header:
    // a groove read from 3 m without AO has to lie). Rock carries the most:
    // its cracks are the feature the eye reads slope from.
    const auto relief_m = [](ProcTextureKind k) {
        switch (k) {
        case ProcTextureKind::GRASS: return 0.070f;
        case ProcTextureKind::ROCK:  return 0.110f;
        case ProcTextureKind::SAND:  return 0.030f;
        case ProcTextureKind::DIRT:  return 0.085f;
        default:                     return 0.0f;
        }
    };
    const ProcTextureKind cells[] = {ProcTextureKind::GRASS, ProcTextureKind::ROCK,
                                     ProcTextureKind::SAND, ProcTextureKind::DIRT};
    std::vector<float> height(static_cast<size_t>(cell_size) * cell_size, 0.0f);
    const float inv = 1.0f / static_cast<float>(cell_size);
    for (uint32_t i = 0; i < 4; ++i) {
        const ProcTextureKind kind = cells[i];
        const float relief = relief_m(kind);
        for (uint32_t y = 0; y < cell_size; ++y) {
            for (uint32_t x = 0; x < cell_size; ++x) {
                const glm::vec2 uv{(static_cast<float>(x) + 0.5f) * inv,
                                   (static_cast<float>(y) + 0.5f) * inv};
                height[static_cast<size_t>(y) * cell_size + x] =
                    terrain_field_t(kind, uv, seed);
            }
        }
        const uint32_t x0 = (i & 1u) * cell_size;
        const uint32_t y0 = (i >> 1u) * cell_size;
        // Central differences of the same field the albedo shaded with; the
        // cells tile, so the differences wrap.
        const auto at = [&](int x, int y) {
            x = (x + static_cast<int>(cell_size)) % static_cast<int>(cell_size);
            y = (y + static_cast<int>(cell_size)) % static_cast<int>(cell_size);
            return height[static_cast<size_t>(y) * cell_size
                          + static_cast<size_t>(x)];
        };
        for (uint32_t y = 0; y < cell_size; ++y) {
            for (uint32_t x = 0; x < cell_size; ++x) {
                const int xi = static_cast<int>(x);
                const int yi = static_cast<int>(y);
                const float dhdx = (at(xi + 1, yi) - at(xi - 1, yi)) * 0.5f * relief;
                const float dhdy = (at(xi, yi + 1) - at(xi, yi - 1)) * 0.5f * relief;
                const glm::vec3 n = glm::normalize(
                    glm::vec3{-dhdx / texel_m, -dhdy / texel_m, 1.0f});
                const size_t o = (static_cast<size_t>(y0 + y) * side + (x0 + x)) * 4;
                const auto to_byte = [](float v) {
                    return static_cast<uint8_t>(
                        std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f));
                };
                pixels[o + 0] = to_byte(n.x * 0.5f + 0.5f);
                pixels[o + 1] = to_byte(n.y * 0.5f + 0.5f);
                pixels[o + 2] = to_byte(n.z * 0.5f + 0.5f);
                pixels[o + 3] = 255u;
            }
        }
    }
    return pixels;
}

std::vector<uint8_t> generate_path_atlas(uint32_t cell_size, uint32_t seed) {
    std::vector<uint8_t> pixels;
    if (cell_size == 0) {
        return pixels;
    }
    const uint32_t side = cell_size * 2;
    pixels.resize(static_cast<size_t>(side) * side * 4);
    // THE INDEX IS core's PathClass ORDINAL (Cobble 0, Dirt 1, FaintTrail 2,
    // StoneSteps 3). See the ordinal warning in ProcTexture.h.
    const ProcTextureKind cells[] = {ProcTextureKind::COBBLE,
                                     ProcTextureKind::PACKED_EARTH,
                                     ProcTextureKind::SCUFFED,
                                     ProcTextureKind::CUT_SLAB};
    for (uint32_t i = 0; i < 4; ++i) {
        ProcTextureDesc desc;
        desc.kind = cells[i];
        desc.size = cell_size;
        desc.seed = seed;
        write_tile(pixels, side, (i & 1u) * cell_size, (i >> 1u) * cell_size, desc);
    }
    return pixels;
}

} // namespace dfn::render
