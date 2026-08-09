/*
Created: 09:08:2026 - 10:48:00
Last updated: 09:08:2026 - 10:48:00
Module: engine/render
File: engine/render/sources/ProcTexture.cpp

Responsibility:
- Procedural texture implementation: integer-hash periodic value noise, per-kind
  recipes (grass, rock, sand, dirt, water), ramp quantization, atlas assembly.

Key items:
- tileable_fbm(); generate_proc_texture(); generate_terrain_atlas().

Dependencies:
- Uses: ProcTexture.h, glm.
- Used by: dfn_render target; tests/render/ProcTextureTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic pure functions only: integer hashes, no float trig seeding, no
  std::rand. Byte-identical output across platforms is a test invariant.
*/
/*
UPD:
- 09:08:2026 - 10:48:00: Stage 3 — initial implementation.
*/

#include "engine/render/sources/ProcTexture.h"

#include <glm/common.hpp>
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

glm::vec3 shade_texel(ProcTextureKind kind, glm::vec2 uv, uint32_t seed) {
    switch (kind) {
    case ProcTextureKind::GRASS: {
        // Meadow mottling + fine blade grain.
        const float patches = tileable_fbm(uv, {4, 4}, seed, 3);
        const float blades = tileable_fbm(uv, {24, 24}, seed ^ 0x51u, 2);
        const float t = 0.55f * patches + 0.45f * blades;
        return ramp_quantized(GRASS_STOPS, 3, t, 6);
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
        return ramp_quantized(ROCK_STOPS, 3, t, 7);
    }
    case ProcTextureKind::SAND: {
        // Fine grain + faint ripple bands (integer cycle count keeps it
        // tileable; ripple kept subtle — strong bands moire at a distance).
        const float grain = tileable_fbm(uv, {32, 32}, seed ^ 0x77u, 2);
        const float warp = tileable_fbm(uv, {4, 4}, seed, 2);
        const float ripple =
            0.5f + 0.5f * std::sin((uv.y * 5.0f + warp * 0.35f) * 6.2831853f);
        const float t = 0.62f * grain + 0.18f * ripple + 0.20f * warp;
        return ramp_quantized(SAND_STOPS, 3, t, 5);
    }
    case ProcTextureKind::DIRT: {
        const float clods = tileable_fbm(uv, {6, 6}, seed, 3);
        const float grit = tileable_fbm(uv, {28, 28}, seed ^ 0x99u, 2);
        const float t = 0.65f * clods + 0.35f * grit;
        return ramp_quantized(DIRT_STOPS, 3, t, 6);
    }
    case ProcTextureKind::WATER: {
        // Horizontally streaked waves (anisotropic period).
        const float streaks = tileable_fbm(uv, {10, 3}, seed, 3);
        const float chop = tileable_fbm(uv, {20, 8}, seed ^ 0xAAu, 2);
        const float t = 0.7f * streaks + 0.3f * chop;
        return ramp_quantized(WATER_STOPS, 3, t, 5);
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

} // namespace dfn::render
