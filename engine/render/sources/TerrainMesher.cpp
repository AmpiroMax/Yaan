/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 11:01:00
Module: engine/render
File: engine/render/sources/TerrainMesher.cpp

Responsibility:
- build_terrain_mesh implementation: positions, central-difference normals,
  chunk-spanning UVs, height/slope ground tint.

Key items:
- build_terrain_mesh(); tint helpers.

Dependencies:
- Uses: TerrainMesher.h, glm.
- Used by: dfn_render target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic pure function; covered by tests/render/TerrainMesherTests.cpp.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
- 09:08:2026 - 11:01:00: Stage 3 — vertex alpha now carries the grass<->dirt
  "dryness" mottling (world-space value noise, continuous across chunks);
  slope tint thresholds aligned with the shader splat band (Materials.h).
*/

#include "engine/render/sources/TerrainMesher.h"

#include "engine/render/sources/ProcTexture.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cstdint>

namespace dfn::render {

namespace {

// Look-dev ground palette (not gameplay constants): linear-ish RGB in 0..1.
constexpr glm::vec3 GRASS_LOW{0.33f, 0.45f, 0.22f};   // valley grass
constexpr glm::vec3 GRASS_HIGH{0.48f, 0.44f, 0.26f};  // dry upland grass
constexpr glm::vec3 ROCK{0.42f, 0.40f, 0.38f};        // steep slopes
constexpr float HEIGHT_BAND_M = 60.0f; // grass fades to dry over this rise
constexpr float SLOPE_ROCK_START = 0.07f; // slope (1 - normal.y) where rock begins
                                          // (aligned with Materials.h splat band)

// Dryness mottling: world-space patch scale and seed (visual, not worldgen).
constexpr float DRY_PATCH_SCALE = 1.0f / 37.0f; // patches ~ tens of meters
constexpr uint32_t DRY_NOISE_SEED = 0x7E22u;

uint32_t pack_rgba(const glm::vec3& c, float a) {
    const auto r = static_cast<uint32_t>(glm::clamp(c.r, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto g = static_cast<uint32_t>(glm::clamp(c.g, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto b = static_cast<uint32_t>(glm::clamp(c.b, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto av = static_cast<uint32_t>(glm::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f);
    return (av << 24) | (b << 16) | (g << 8) | r; // 0xAABBGGRR (frozen Vertex)
}

} // namespace

TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field) {
    TerrainMeshData mesh;
    const uint32_t res = field.resolution;
    if (res < 2 || field.heights.size() < static_cast<size_t>(res) * res) {
        return mesh; // malformed view: empty mesh, never UB
    }

    mesh.vertices.resize(static_cast<size_t>(res) * res);
    const float inv_span = 1.0f / (field.step * static_cast<float>(res - 1));

    for (uint32_t z = 0; z < res; ++z) {
        for (uint32_t x = 0; x < res; ++x) {
            const float h = field.height_at(x, z);

            // Central differences with edge clamping (meters per meter).
            const uint32_t xl = x > 0 ? x - 1 : x;
            const uint32_t xr = x < res - 1 ? x + 1 : x;
            const uint32_t zu = z > 0 ? z - 1 : z;
            const uint32_t zd = z < res - 1 ? z + 1 : z;
            const float dx_span = static_cast<float>(xr - xl) * field.step;
            const float dz_span = static_cast<float>(zd - zu) * field.step;
            const float dhdx = (field.height_at(xr, z) - field.height_at(xl, z)) / dx_span;
            const float dhdz = (field.height_at(x, zd) - field.height_at(x, zu)) / dz_span;
            const glm::vec3 normal = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));

            // Ground tint: height band grass, slope-driven rock (see header).
            const float band = glm::clamp((h - field.height_offset) / HEIGHT_BAND_M,
                                          0.0f, 1.0f);
            const float slope = 1.0f - normal.y;
            const float rockness = glm::clamp(
                (slope - SLOPE_ROCK_START) / (1.0f - SLOPE_ROCK_START), 0.0f, 1.0f);
            const glm::vec3 tint = glm::mix(glm::mix(GRASS_LOW, GRASS_HIGH, band),
                                            ROCK, rockness);

            const glm::vec3 wpos{field.origin.x + static_cast<float>(x) * field.step,
                                 h,
                                 field.origin.y + static_cast<float>(z) * field.step};
            // Grass<->dirt mottling for the shader splat: two-octave world-space
            // value noise, remapped so mid-values stay grass (alpha = dryness).
            const glm::vec2 dry_p{wpos.x * DRY_PATCH_SCALE, wpos.z * DRY_PATCH_SCALE};
            const float dry_noise =
                0.65f * value_noise01(dry_p, DRY_NOISE_SEED)
                + 0.35f * value_noise01(dry_p * 2.7f, DRY_NOISE_SEED ^ 0x5Au);
            const float dryness = glm::smoothstep(0.55f, 0.75f, dry_noise);

            platform::Vertex& v = mesh.vertices[static_cast<size_t>(z) * res + x];
            v.position = wpos;
            v.normal = normal;
            v.uv = {static_cast<float>(x) * field.step * inv_span,
                    static_cast<float>(z) * field.step * inv_span};
            v.color_rgba = pack_rgba(tint, dryness);
        }
    }

    mesh.indices.reserve(static_cast<size_t>(res - 1) * (res - 1) * 6);
    for (uint32_t z = 0; z + 1 < res; ++z) {
        for (uint32_t x = 0; x + 1 < res; ++x) {
            const uint32_t i00 = z * res + x;
            const uint32_t i10 = i00 + 1;
            const uint32_t i01 = i00 + res;
            const uint32_t i11 = i01 + 1;
            // CCW seen from +Y (culling is off this stage; kept consistent
            // so stage 3 can enable it without re-meshing).
            mesh.indices.insert(mesh.indices.end(), {i00, i11, i10, i00, i01, i11});
        }
    }
    return mesh;
}

} // namespace dfn::render
