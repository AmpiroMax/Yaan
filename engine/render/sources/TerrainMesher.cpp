/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 14:11:37
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
- 09:08:2026 - 14:11:37: Splat fix (design ruling, feature-requests batch): the
  render-side "dryness" dirt mottling is REMOVED — it painted large red-brown
  washes over ground core classifies as Grass (04_hamlet_approach probe:
  whole sightline Grass, yet the frame read as a 60+ m brown flat). LANDSCAPE
  §4 has no dirt material (dirt/path is a FUTURE road pass); the splat now
  keys off core's surface_class ONLY. Vertex alpha is reserved (255).
*/

#include "engine/render/sources/TerrainMesher.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cstdint>

namespace dfn::render {

namespace {

// Splat weight of the GrassRockBlend class (§4 priority 3): mid value so the
// shader's dither band straddles it between grass and rock.
constexpr float BLEND_CLASS_ROCK_W = 0.5f;

uint32_t pack_weights(float sand, float rock, float bed) {
    const auto r = static_cast<uint32_t>(glm::clamp(sand, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto g = static_cast<uint32_t>(glm::clamp(rock, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto b = static_cast<uint32_t>(glm::clamp(bed, 0.0f, 1.0f) * 255.0f + 0.5f);
    return 0xFF000000u | (b << 16) | (g << 8) | r; // 0xAABBGGRR; A reserved
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

            platform::Vertex& v = mesh.vertices[static_cast<size_t>(z) * res + x];
            v.position = wpos;
            v.normal = normal;
            v.uv = {static_cast<float>(x) * field.step * inv_span,
                    static_cast<float>(z) * field.step * inv_span};
            v.color_rgba = pack_weights(sand_w, rock_w, bed_w);
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
