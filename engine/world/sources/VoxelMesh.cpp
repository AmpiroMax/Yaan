/*
Created: 09:08:2026 - 16:00:00
Last updated: 10:08:2026 - 21:32:35
Module: engine/world
File: engine/world/sources/VoxelMesh.cpp

Responsibility:
- Surface nets implementation: cell vertex placement from edge crossings,
  normals from the SDF gradient, material from the solid side, quad emission
  across sign-changing edges.

Key items:
- extract_surface_nets, VoxelMeshData::view.

Dependencies:
- Uses: VoxelMesh.h.
- Used by: dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Winding: faces are emitted counter-clockwise seen from the AIR side, so a
  cave ceiling faces down into the passage and the open ground faces up.
  Getting this wrong makes caves invisible from inside — check it there, not
  on open terrain where both windings look plausible.
- Deterministic traversal (see the header notice).
*/
/*
UPD:
- 09:08:2026 - 16:00:00: Created — surface nets extraction.
- 09:08:2026 - 16:30:44: Representation swap: surface nets with gradient normals and per-vertex material; scans only the active band; volume fields hoisted out of the inner loops.
- 09:08:2026 - 19:55:17: STRIPES FIX (user-visible): a surface vertex's material now comes from its own depth below the interpolated local surface, not from the nearest solid corner. That corner's material was computed against ITS column's surface, so on any slope an uphill corner carried deep-soil Dirt even where the isosurface grazes the top — drawing dirt in contour-following bands across open meadow. Dry open ground away from water now carries 19 Dirt vertices out of 1.08M upward-facing (0.002%), all of them river-bed-class columns.
- 10:08:2026 - 21:27:14: Recorded the unmeasured premise under render's
  0.375%% visible-share figure for the Rule 39 blend fix: it assumes these
  gradient normals match worldgen's analytic slope, and nobody has checked.
- 10:08:2026 - 21:32:35: Closed the normals premise. The extracted normals
  are NOT systematically flatter than the analytic slope (51.8%% flatter, mean
  slightly steeper); they are noisier. 46.1%% of blend vertices change under the
  Rule 39 fix, so render's 0.375%% ground-pixel figure is a floor — but vertices
  are not pixels, and the converting measurement is render's instrument.
*/

#include "engine/world/sources/VoxelMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

// Cell corner offsets, indexed as bit0=x, bit1=y, bit2=z.
constexpr std::array<glm::ivec3, 8> CORNER = {
    glm::ivec3{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
    {0, 0, 1},           {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
// The 12 cell edges as corner index pairs.
constexpr std::array<std::array<int, 2>, 12> EDGE = {
    std::array<int, 2>{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                      {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

constexpr int32_t NO_VERTEX = -1;
// Same depth bands the volume builder uses, so a vertex and the voxels around
// it cannot disagree about what they are made of.
constexpr float SKIN_DEPTH_M = 0.4f;
constexpr float SOIL_DEPTH_M = 2.5f;

} // namespace

math::VoxelMeshView VoxelMeshData::view(ChunkCoord coord) const {
    math::VoxelMeshView v;
    v.chunk_coord = glm::ivec2{coord.x, coord.z};
    v.positions = positions;
    v.normals = normals;
    v.materials = materials;
    v.indices = indices;
    return v;
}

VoxelMeshData extract_surface_nets(const VoxelVolume& volume) {
    VoxelMeshData mesh;
    if (volume.nx < 2 || volume.ny < 2 || volume.nz < 2) {
        return mesh;
    }
    // Hoist everything the inner loops read. `volume` is a reference and the
    // loops write to mesh vectors, so the compiler must assume those writes
    // may alias its fields: without this it reloads nx/ny/band and re-does the
    // band/127 DIVISION on every sample. Measured 156 ms -> 12 ms per chunk.
    const int32_t nx = volume.nx;
    const int32_t ny = volume.ny;
    const int32_t nz = volume.nz;
    const int8_t* const raw = volume.sdf.data();
    const float scale = volume.band / 127.0f;
    const auto idx = [nx, ny](int32_t x, int32_t y, int32_t z) {
        return (static_cast<std::size_t>(z) * nx + x) * ny + y;
    };
    const auto dist = [raw, scale, &idx](int32_t x, int32_t y, int32_t z) {
        return static_cast<float>(raw[idx(x, y, z)]) * scale;
    };
    const auto is_solid = [raw, &idx](int32_t x, int32_t y, int32_t z) {
        return raw[idx(x, y, z)] < 0;
    };

    std::vector<int32_t> cell_vertex(
        static_cast<std::size_t>(nx) * ny * nz, NO_VERTEX);

    // The y range a cell can possibly straddle: the union of its four
    // columns' active bands. Outside it every corner is saturated to the same
    // sign, so no crossing exists and scanning it is pure waste (measured: the
    // difference between 300 ms and 45 ms per chunk).
    const auto cell_y_range = [&volume](int32_t x, int32_t z) {
        int32_t lo = volume.ny;
        int32_t hi = -1;
        for (int32_t dz = 0; dz <= 1; ++dz) {
            for (int32_t dx = 0; dx <= 1; ++dx) {
                const std::size_t c = volume.column(x + dx, z + dz);
                lo = std::min(lo, volume.band_lo[c]);
                hi = std::max(hi, volume.band_hi[c]);
            }
        }
        return std::pair<int32_t, int32_t>{lo, hi};
    };

    // --- Pass 1: one vertex per cell straddling the isosurface --------------
    for (int32_t z = 0; z + 1 < nz; ++z) {
        for (int32_t x = 0; x + 1 < nx; ++x) {
            const auto [ylo, yhi] = cell_y_range(x, z);
            for (int32_t y = std::max(0, ylo - 1); y <= std::min(yhi, ny - 2); ++y) {
                std::array<float, 8> s{};
                bool has_solid = false;
                bool has_air = false;
                for (int c = 0; c < 8; ++c) {
                    s[static_cast<std::size_t>(c)] =
                        dist(x + CORNER[static_cast<std::size_t>(c)].x,
                             y + CORNER[static_cast<std::size_t>(c)].y,
                             z + CORNER[static_cast<std::size_t>(c)].z);
                    (s[static_cast<std::size_t>(c)] < 0.0f ? has_solid : has_air) = true;
                }
                if (!has_solid || !has_air) {
                    continue;
                }
                // Vertex = mean of the edge crossings (surface nets). This is
                // what recovers a smooth surface from 1 m samples.
                glm::vec3 acc{0.0f};
                int crossings = 0;
                for (const auto& e : EDGE) {
                    const float a = s[static_cast<std::size_t>(e[0])];
                    const float b = s[static_cast<std::size_t>(e[1])];
                    if ((a < 0.0f) == (b < 0.0f)) {
                        continue;
                    }
                    const float t = a / (a - b);
                    const glm::vec3 pa{CORNER[static_cast<std::size_t>(e[0])]};
                    const glm::vec3 pb{CORNER[static_cast<std::size_t>(e[1])]};
                    acc += pa + (pb - pa) * t;
                    ++crossings;
                }
                acc /= static_cast<float>(crossings);

                // Normal from the SDF gradient (central differences, clamped
                // at the slab border).
                const auto grad = [&](int32_t ax, int32_t ay, int32_t az) {
                    const auto d = [&](int32_t px, int32_t py, int32_t pz) {
                        return dist(std::clamp(px, 0, nx - 1), std::clamp(py, 0, ny - 1),
                                    std::clamp(pz, 0, nz - 1));
                    };
                    return glm::vec3{d(ax + 1, ay, az) - d(ax - 1, ay, az),
                                     d(ax, ay + 1, az) - d(ax, ay - 1, az),
                                     d(ax, ay, az + 1) - d(ax, ay, az - 1)};
                };
                glm::vec3 n = grad(x, y, z);
                const float len = glm::length(n);
                n = len > 1e-6f ? n / len : glm::vec3{0.0f, 1.0f, 0.0f};

                // Material from the vertex's OWN depth below the surface of
                // the column it stands in. Inheriting it from the nearest solid
                // corner was wrong on every slope: that corner's material was
                // computed against ITS column's surface, so an uphill corner
                // carried deep-soil Dirt even where the isosurface grazes the
                // top — which drew dirt in contour-following stripes across
                // open meadow. Depth ~0 gives the skin (grass/rock/sand); only
                // genuinely buried surfaces, i.e. cave walls, go to soil/rock.
                const glm::vec3 vpos = volume.world_at(x, y, z) + acc * volume.voxel;
                // BILINEAR surface lookup, not nearest column. A surface vertex
                // sits between columns, and on a slope the nearest column's
                // surface is up to half a voxel of rise away — at 32 degrees
                // that is 0.3 m, enough to push a grass vertex past the 0.4 m
                // skin band into Dirt. Rounding left 2.1% of open ground still
                // striped; interpolating removes the discretisation entirely.
                const float fx = std::clamp((vpos.x - volume.origin.x) / volume.voxel,
                                            0.0f, static_cast<float>(nx - 1));
                const float fz = std::clamp((vpos.z - volume.origin.y) / volume.voxel,
                                            0.0f, static_cast<float>(nz - 1));
                const int32_t x0 = std::min(static_cast<int32_t>(fx), nx - 2);
                const int32_t z0 = std::min(static_cast<int32_t>(fz), nz - 2);
                const float tx = fx - static_cast<float>(x0);
                const float tz = fz - static_cast<float>(z0);
                const auto surf_at = [&](int32_t sx, int32_t sz) {
                    return volume.column_surface[static_cast<std::size_t>(sz) * nx + sx];
                };
                const float s0 = surf_at(x0, z0) + (surf_at(x0 + 1, z0) - surf_at(x0, z0)) * tx;
                const float s1 =
                    surf_at(x0, z0 + 1) + (surf_at(x0 + 1, z0 + 1) - surf_at(x0, z0 + 1)) * tx;
                const std::size_t col =
                    static_cast<std::size_t>(tz < 0.5f ? z0 : z0 + 1) * nx
                    + (tx < 0.5f ? x0 : x0 + 1);
                const float depth = (s0 + (s1 - s0) * tz) - vpos.y;
                uint8_t mat;
                if (depth < SKIN_DEPTH_M) {
                    mat = volume.column_skin[col];
                } else if (depth < SOIL_DEPTH_M) {
                    mat = static_cast<uint8_t>(math::VoxelMaterial::Dirt);
                } else {
                    mat = static_cast<uint8_t>(math::VoxelMaterial::Rock);
                }

                cell_vertex[idx(x, y, z)] = static_cast<int32_t>(mesh.positions.size());
                mesh.positions.push_back(vpos);
                // MEASURED 10.08.2026, closing the premise this note used to
                // record as open. `n` is the SDF gradient; the shader's slope
                // measure is 1-cos(angle), which is exactly 1-n.y, so the two
                // are directly comparable. Over the 26136 blend vertices of the
                // seed-1 testbed, against terrain_slope() at the same points:
                //
                //   1-n.y     mean 0.1875  p10 0.1205  p50 0.1893  p90 0.2516
                //   analytic  mean 0.1796  p10 0.1334  p50 0.1802  p90 0.2290
                //
                // THE "FLATTER NORMALS" HYPOTHESIS IS WRONG. Surface nets do
                // not bias these toward flat: the voxel normal reads flatter on
                // 51.8% of vertices and steeper on the rest — a coin flip — and
                // the mean is slightly STEEPER, not flatter. What the extraction
                // actually adds is SCATTER (p10 lower and p90 higher than the
                // analytic), which is a different defect from a bias and would
                // not have been found by looking for the one we expected.
                //
                // The consequence still runs the other way from the estimate.
                // Against the shader ramp (START 0.1322, END 0.2352): 12.8% of
                // blend vertices sit below START, where the shader adds NO rock
                // and the surface class is the only source; 14.9% sit above END,
                // where it is already saturated. The shader's own term is
                // already >= 0.5 on 53.9%, so the fix changes 46.1% of blend
                // vertices — far more than the ~17% implied by the 0.375%
                // ground-pixel estimate. Mean rock-weight delta 0.1652; the
                // median vertex changes by 0, the affected ones by the full 0.5.
                //
                // WHAT THIS DOES NOT SETTLE, and the reason it is not a licence
                // to quote a new percentage: THESE ARE VERTICES, NOT PIXELS. A
                // vertex on a crag covers a different screen area from one on a
                // valley floor, so 46.1% of vertices does not convert to 46.1%
                // of anything visible. This measures the OBJECT; the claim
                // 0.375% makes is about the VIEW, and Rule 41 says an
                // instrument on one cannot settle the other. What it does
                // establish is that 0.375% is a FLOOR, and by a wide enough
                // margin (~2.7x on the vertex share alone) that retiring
                // BLEND_CLASS_ROCK_W on the strength of it would be premature.
                // The number that closes it is a pixel-space measurement and
                // that instrument lives in render.
                mesh.normals.push_back(n);
                mesh.materials.push_back(mat);
            }
        }
    }

    // --- Pass 2: a quad per sign-changing edge, from the 4 cells sharing it --
    const auto quad = [&](int32_t a, int32_t b, int32_t c, int32_t d, bool flip) {
        if (a == NO_VERTEX || b == NO_VERTEX || c == NO_VERTEX || d == NO_VERTEX) {
            return;
        }
        const auto ua = static_cast<uint32_t>(a);
        const auto ub = static_cast<uint32_t>(b);
        const auto uc = static_cast<uint32_t>(c);
        const auto ud = static_cast<uint32_t>(d);
        if (flip) {
            mesh.indices.insert(mesh.indices.end(), {ua, uc, ub, ua, ud, uc});
        } else {
            mesh.indices.insert(mesh.indices.end(), {ua, ub, uc, ua, uc, ud});
        }
    };
    for (int32_t z = 1; z + 1 < nz; ++z) {
        for (int32_t x = 1; x + 1 < nx; ++x) {
            // Widened by one cell on each side because this pass reads the
            // neighbouring cells' vertices as well as its own.
            const auto [ylo, yhi] = cell_y_range(x, z);
            for (int32_t y = std::max(1, ylo - 2); y <= std::min(yhi + 1, ny - 2); ++y) {
                const bool solid = is_solid(x, y, z);
                if (solid != is_solid(x + 1, y, z)) {
                    quad(cell_vertex[idx(x, y - 1, z - 1)], cell_vertex[idx(x, y, z - 1)],
                         cell_vertex[idx(x, y, z)], cell_vertex[idx(x, y - 1, z)], solid);
                }
                if (solid != is_solid(x, y + 1, z)) {
                    quad(cell_vertex[idx(x - 1, y, z - 1)], cell_vertex[idx(x, y, z - 1)],
                         cell_vertex[idx(x, y, z)], cell_vertex[idx(x - 1, y, z)], !solid);
                }
                if (solid != is_solid(x, y, z + 1)) {
                    quad(cell_vertex[idx(x - 1, y - 1, z)], cell_vertex[idx(x, y - 1, z)],
                         cell_vertex[idx(x, y, z)], cell_vertex[idx(x - 1, y, z)], !solid);
                }
            }
        }
    }
    return mesh;
}

} // namespace dfn::world
