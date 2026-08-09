/*
Created: 09:08:2026 - 00:45:00
Last updated: 10:08:2026 - 01:47:53
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
- 09:08:2026 - 22:01:04: World-referenced UVs + border skirts (LOD).
- 10:08:2026 - 01:47:53: Clip rectangle (options.clip_*): cells wholly inside
  the chunk-streamed rect are skipped and skirts follow the emitted region's
  whole boundary. The no-clip path is untouched and bit-identical.
*/

#include "engine/render/sources/TerrainMesher.h"

#include "engine/core/config/sources/Constants.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
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
    return build_terrain_mesh(field, nullptr, {});
}

TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field,
                                   const math::SurfaceFieldView* surface) {
    return build_terrain_mesh(field, surface, {});
}

float terrain_border_max_step_m(const math::HeightFieldView& field) {
    const uint32_t res = field.resolution;
    if (res < 2 || field.heights.size() < static_cast<size_t>(res) * res) {
        return 0.0f;
    }
    float worst = 0.0f;
    for (uint32_t i = 0; i + 1 < res; ++i) {
        // The four border rows, each walked as adjacent pairs.
        worst = std::max(worst, std::fabs(field.height_at(i + 1, 0)
                                          - field.height_at(i, 0)));
        worst = std::max(worst, std::fabs(field.height_at(i + 1, res - 1)
                                          - field.height_at(i, res - 1)));
        worst = std::max(worst, std::fabs(field.height_at(0, i + 1)
                                          - field.height_at(0, i)));
        worst = std::max(worst, std::fabs(field.height_at(res - 1, i + 1)
                                          - field.height_at(res - 1, i)));
    }
    return worst;
}

TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field,
                                   const math::SurfaceFieldView* surface,
                                   const TerrainMeshOptions& options) {
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
    // UVs are WORLD-referenced: one unit of uv is one CHUNK_SIZE of ground, so
    // the material tiles at the same physical size on a 256 m chunk and on an
    // 8 km LOD node. The old formula (offset within the field / field span)
    // agrees with this exactly whenever the origin is a whole number of
    // CHUNK_SIZE/tiles apart, which every chunk and every node origin is — and
    // disagrees catastrophically for a node, which was the bug this fixes.
    const float inv_tile = 1.0f / static_cast<float>(config::CHUNK_SIZE);

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
            v.uv = {wpos.x * inv_tile, wpos.z * inv_tile};
            v.color_rgba = pack_weights(sand_w, rock_w, bed_w);
        }
    }

    // Clip rectangle (straddle-ring fix): cells wholly inside the rect belong
    // to chunk-streamed ground and are not emitted. `cell_emitted` is only
    // materialized when a clip is active, so the ordinary path allocates and
    // emits exactly what it always did.
    const bool clip_active = options.clip_max.x > options.clip_min.x
                          && options.clip_max.y > options.clip_min.y;
    const auto cell_clipped = [&](uint32_t x, uint32_t z) {
        if (!clip_active) {
            return false;
        }
        const float cx0 = field.origin.x + static_cast<float>(x) * field.step;
        const float cz0 = field.origin.y + static_cast<float>(z) * field.step;
        return cx0 >= options.clip_min.x && cx0 + field.step <= options.clip_max.x
            && cz0 >= options.clip_min.y && cz0 + field.step <= options.clip_max.y;
    };

    mesh.indices.reserve(static_cast<size_t>(res - 1) * (res - 1) * 6);
    for (uint32_t z = 0; z + 1 < res; ++z) {
        for (uint32_t x = 0; x + 1 < res; ++x) {
            if (cell_clipped(x, z)) {
                continue;
            }
            const uint32_t i00 = z * res + x;
            const uint32_t i10 = i00 + 1;
            const uint32_t i01 = i00 + res;
            const uint32_t i11 = i01 + 1;
            // CCW seen from +Y (culling is off this stage; kept consistent
            // so stage 3 can enable it without re-meshing).
            mesh.indices.insert(mesh.indices.end(), {i00, i11, i10, i00, i01, i11});
        }
    }

    // Skirt: an apron dropped from each border vertex. It is NOT ground and is
    // never seen as a surface — it exists so that the pixel-wide gap where two
    // differently-tessellated meshes meet shows terrain instead of sky. Skirt
    // vertices copy their parent's normal, colour and uv, so a skirt that IS
    // momentarily visible at a crack shades like the ground it stands in
    // rather than as a black band.
    //
    // With a clip active the skirt follows the EMITTED REGION'S boundary
    // instead of the four grid borders: every edge between an emitted cell and
    // a missing one (clipped or off-grid) gets an apron, because the cut line
    // against chunk ground is a lattice seam exactly like the outer border.
    if (clip_active && options.skirt_depth_m > 0.0f) {
        const auto add_edge_skirt = [&](uint32_t p0, uint32_t p1) {
            const auto s0 = static_cast<uint32_t>(mesh.vertices.size());
            platform::Vertex v0 = mesh.vertices[p0];
            platform::Vertex v1 = mesh.vertices[p1];
            v0.position.y -= options.skirt_depth_m;
            v1.position.y -= options.skirt_depth_m;
            mesh.vertices.push_back(v0);
            mesh.vertices.push_back(v1);
            mesh.indices.insert(mesh.indices.end(),
                                {p0, s0, s0 + 1, p0, s0 + 1, p1});
        };
        for (uint32_t z = 0; z + 1 < res; ++z) {
            for (uint32_t x = 0; x + 1 < res; ++x) {
                if (cell_clipped(x, z)) {
                    continue;
                }
                const uint32_t i00 = z * res + x;
                // Neighbour missing (off-grid or clipped) -> this edge is a
                // boundary and hangs an apron.
                if (z == 0 || cell_clipped(x, z - 1)) {
                    add_edge_skirt(i00, i00 + 1);
                }
                if (z + 2 >= res || cell_clipped(x, z + 1)) {
                    add_edge_skirt(i00 + res, i00 + res + 1);
                }
                if (x == 0 || cell_clipped(x - 1, z)) {
                    add_edge_skirt(i00, i00 + res);
                }
                if (x + 2 >= res || cell_clipped(x + 1, z)) {
                    add_edge_skirt(i00 + 1, i00 + res + 1);
                }
            }
        }
    } else if (options.skirt_depth_m > 0.0f) {
        const auto grid_count = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.reserve(mesh.vertices.size() + static_cast<size_t>(res) * 4);
        mesh.indices.reserve(mesh.indices.size() + static_cast<size_t>(res - 1) * 4 * 6);

        // Each border walked in order; `parent` indexes the surface grid.
        const auto add_border = [&](uint32_t border) {
            const uint32_t base = grid_count + border * res;
            for (uint32_t i = 0; i < res; ++i) {
                uint32_t px = 0;
                uint32_t pz = 0;
                switch (border) {
                case 0: px = i;       pz = 0;       break; // -z edge
                case 1: px = i;       pz = res - 1; break; // +z edge
                case 2: px = 0;       pz = i;       break; // -x edge
                default: px = res - 1; pz = i;      break; // +x edge
                }
                platform::Vertex v = mesh.vertices[static_cast<size_t>(pz) * res + px];
                v.position.y -= options.skirt_depth_m;
                mesh.vertices.push_back(v);
            }
            for (uint32_t i = 0; i + 1 < res; ++i) {
                uint32_t p0 = 0;
                uint32_t p1 = 0;
                switch (border) {
                case 0: p0 = i;                            p1 = i + 1; break;
                case 1: p0 = (res - 1) * res + i;          p1 = p0 + 1; break;
                case 2: p0 = i * res;                      p1 = p0 + res; break;
                default: p0 = i * res + (res - 1);         p1 = p0 + res; break;
                }
                const uint32_t s0 = base + i;
                const uint32_t s1 = base + i + 1;
                // Two triangles per span. Backface culling is off for terrain,
                // so the winding is chosen for consistency with the surface
                // rather than to be load-bearing.
                mesh.indices.insert(mesh.indices.end(), {p0, s0, s1, p0, s1, p1});
            }
        };
        for (uint32_t border = 0; border < 4; ++border) {
            add_border(border);
        }
    }
    return mesh;
}

} // namespace dfn::render
