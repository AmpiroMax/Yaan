/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 11:57:20
Module: engine/render
File: engine/render/sources/TerrainMesher.cpp

Responsibility:
- build_terrain_mesh implementation: positions, central-difference normals,
  chunk-spanning UVs, per-vertex splat weights (surface-class driven when a
  SurfaceFieldView is supplied).

Key items:
- build_terrain_mesh(); splat weight helpers.

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
- 09:08:2026 - 11:57:20: Stage 3b — vertex RGB re-purposed to splat weights
  (R sand / G rock / B water-bed) from core's SurfaceFieldView; the old
  height-band tint is gone (fs_terrain owns the palette now).
*/

#include "engine/render/sources/TerrainMesher.h"

#include "engine/render/sources/ProcTexture.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cstdint>

namespace dfn::render {

namespace {

// Dryness mottling: world-space patch scale and seed (visual, not worldgen).
constexpr float DRY_PATCH_SCALE = 1.0f / 37.0f; // patches ~ tens of meters
constexpr uint32_t DRY_NOISE_SEED = 0x7E22u;

// Splat weight of the GrassRockBlend class (§4 priority 3): mid value so the
// shader's dither band straddles it between grass and rock.
constexpr float BLEND_CLASS_ROCK_W = 0.5f;

uint32_t pack_weights(float sand, float rock, float bed, float dry) {
    const auto r = static_cast<uint32_t>(glm::clamp(sand, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto g = static_cast<uint32_t>(glm::clamp(rock, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto b = static_cast<uint32_t>(glm::clamp(bed, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto a = static_cast<uint32_t>(glm::clamp(dry, 0.0f, 1.0f) * 255.0f + 0.5f);
    return (a << 24) | (b << 16) | (g << 8) | r; // 0xAABBGGRR (frozen Vertex)
}

} // namespace

TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field) {
    return build_terrain_mesh(field, nullptr);
}

TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field,
                                   const math::SurfaceFieldView* surface) {
    TerrainMeshData mesh;
    const uint32_t res = field.resolution;
    if (res < 2 || field.heights.size() < static_cast<size_t>(res) * res) {
        return mesh; // malformed view: empty mesh, never UB
    }
    // A surface view is used only when it matches the height grid exactly
    // (same chunk, same resolution — the agreed contract).
    const size_t samples = static_cast<size_t>(res) * res;
    if (surface != nullptr
        && (surface->resolution != res || surface->surface_class.size() < samples
            || surface->dist_to_water.size() < samples)) {
        surface = nullptr;
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

            const glm::vec3 wpos{field.origin.x + static_cast<float>(x) * field.step,
                                 h,
                                 field.origin.y + static_cast<float>(z) * field.step};

            // Splat weights (shader contract, see header). Surface data is the
            // design truth (LANDSCAPE §4 priority: sand > rock > blend > grass;
            // water bed under water); slope-driven rock is added in-shader.
            float sand_w = 0.0f;
            float rock_w = 0.0f;
            float bed_w = 0.0f;
            if (surface != nullptr) {
                const size_t idx = static_cast<size_t>(z) * res + x;
                switch (static_cast<math::SurfaceClass>(surface->surface_class[idx])) {
                case math::SurfaceClass::Sand: sand_w = 1.0f; break;
                case math::SurfaceClass::Rock: rock_w = 1.0f; break;
                case math::SurfaceClass::GrassRockBlend:
                    rock_w = BLEND_CLASS_ROCK_W;
                    break;
                case math::SurfaceClass::WaterBed: bed_w = 1.0f; break;
                case math::SurfaceClass::Grass: break;
                }
            }

            // Grass<->dirt mottling for the shader splat: two-octave world-space
            // value noise, remapped so mid-values stay grass (alpha = dryness).
            const glm::vec2 dry_p{wpos.x * DRY_PATCH_SCALE, wpos.z * DRY_PATCH_SCALE};
            const float dry_noise =
                0.65f * value_noise01(dry_p, DRY_NOISE_SEED)
                + 0.35f * value_noise01(dry_p * 2.7f, DRY_NOISE_SEED ^ 0x5Au);
            // Threshold raised at stage 3b: from above, 0.55 painted ~1/3 of
            // the meadows brown (tour overview read) — dirt is an accent.
            const float dryness = glm::smoothstep(0.63f, 0.80f, dry_noise);

            platform::Vertex& v = mesh.vertices[static_cast<size_t>(z) * res + x];
            v.position = wpos;
            v.normal = normal;
            v.uv = {static_cast<float>(x) * field.step * inv_span,
                    static_cast<float>(z) * field.step * inv_span};
            v.color_rgba = pack_weights(sand_w, rock_w, bed_w, dryness);
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
