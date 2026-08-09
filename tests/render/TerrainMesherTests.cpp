/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 14:11:37
Module: tests
File: tests/render/TerrainMesherTests.cpp

Responsibility:
- Unit tests for build_terrain_mesh: counts, height decode, crack-free chunk
  borders (shared edge rows), normals on a known slope, malformed input.

Key items:
- make_field helper; doctest cases.

Dependencies:
- Uses: doctest, engine/render TerrainMesher, math::HeightFieldView.
- Used by: ctest (render_terrain_mesher).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial tests.
- 09:08:2026 - 11:08:00: Stage 3 — vertex alpha now carries the grass/dirt
  dryness (world-continuous), no longer forced opaque; test updated to check
  determinism + cross-chunk alpha continuity instead.
- 09:08:2026 - 11:57:20: Stage 3b — surface-field splat weight channels
  (R sand / G rock / B bed) + mismatched-grid fallback.
- 09:08:2026 - 14:11:37: Dryness channel removed (design ruling): alpha is
  reserved-opaque, checked in the determinism case.
*/

#include "engine/render/sources/TerrainMesher.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/TerrainLod.h"

#include <doctest/doctest.h>

#include <cmath>
#include <functional>
#include <vector>

namespace {

using dfn::math::HeightFieldView;
using dfn::render::build_terrain_mesh;
using dfn::render::TerrainMeshData;

constexpr uint32_t RES = 5;
constexpr float STEP = 2.0f;
constexpr float SCALE = 0.01f; // meters per raw unit
constexpr float OFFSET = 10.0f;

struct FieldData {
    std::vector<uint16_t> heights;
    HeightFieldView view;
};

/// Builds a field for chunk (cx, 0); `raw` maps WORLD sample coords to raw
/// heights so neighbor chunks share edge samples exactly (the contract).
FieldData make_field(int cx, const std::function<uint16_t(uint32_t, uint32_t)>& raw) {
    FieldData f;
    f.heights.resize(RES * RES);
    const uint32_t base_x = static_cast<uint32_t>(cx) * (RES - 1);
    for (uint32_t z = 0; z < RES; ++z) {
        for (uint32_t x = 0; x < RES; ++x) {
            f.heights[z * RES + x] = raw(base_x + x, z);
        }
    }
    f.view.chunk_coord = {cx, 0};
    f.view.origin = {static_cast<float>(cx) * STEP * static_cast<float>(RES - 1), 0.0f};
    f.view.resolution = RES;
    f.view.step = STEP;
    f.view.heights = f.heights;
    f.view.height_scale = SCALE;
    f.view.height_offset = OFFSET;
    return f;
}

} // namespace

TEST_CASE("terrain mesh has expected vertex and index counts") {
    const FieldData f = make_field(0, [](uint32_t, uint32_t) { return uint16_t{0}; });
    const TerrainMeshData mesh = build_terrain_mesh(f.view);
    CHECK(mesh.vertices.size() == RES * RES);
    CHECK(mesh.indices.size() == (RES - 1) * (RES - 1) * 6);
    for (const uint32_t index : mesh.indices) {
        CHECK(index < mesh.vertices.size());
    }
}

TEST_CASE("terrain mesh decodes heights and spans the chunk") {
    const FieldData f =
        make_field(0, [](uint32_t x, uint32_t) { return static_cast<uint16_t>(x * 100); });
    const TerrainMeshData mesh = build_terrain_mesh(f.view);

    // height = offset + raw * scale (frozen decode formula).
    CHECK(mesh.vertices[0].position.y == doctest::Approx(OFFSET));
    CHECK(mesh.vertices[RES - 1].position.y == doctest::Approx(OFFSET + 400 * SCALE));

    // Positions span origin .. origin + (res-1)*step. UVs are WORLD-referenced:
    // one uv unit is one CHUNK_SIZE of ground wherever the mesh sits, so the
    // material tiles at a fixed physical size on a 256 m chunk and on an 8 km
    // LOD node alike. This toy field spans 8 m, hence 8/256.
    const auto& first = mesh.vertices.front();
    const auto& last = mesh.vertices.back();
    const float chunk = static_cast<float>(dfn::config::CHUNK_SIZE);
    CHECK(first.position.x == doctest::Approx(0.0f));
    CHECK(last.position.x == doctest::Approx(STEP * (RES - 1)));
    CHECK(last.position.z == doctest::Approx(STEP * (RES - 1)));
    CHECK(first.uv.x == doctest::Approx(0.0f));
    CHECK(last.uv.x == doctest::Approx(STEP * (RES - 1) / chunk));
    CHECK(last.uv.y == doctest::Approx(STEP * (RES - 1) / chunk));
}

TEST_CASE("adjacent chunk meshes stitch without cracks (shared edge rows)") {
    const auto raw = [](uint32_t x, uint32_t z) {
        return static_cast<uint16_t>(137 * x + 291 * z);
    };
    const FieldData a = make_field(0, raw);
    const FieldData b = make_field(1, raw);
    const TerrainMeshData ma = build_terrain_mesh(a.view);
    const TerrainMeshData mb = build_terrain_mesh(b.view);

    for (uint32_t z = 0; z < RES; ++z) {
        const auto& va = ma.vertices[z * RES + (RES - 1)]; // a's east edge
        const auto& vb = mb.vertices[z * RES + 0];         // b's west edge
        CHECK(va.position.x == doctest::Approx(vb.position.x));
        CHECK(va.position.y == doctest::Approx(vb.position.y));
        CHECK(va.position.z == doctest::Approx(vb.position.z));
    }
}

TEST_CASE("normals: flat field points up, constant slope tilts against it") {
    const FieldData flat = make_field(0, [](uint32_t, uint32_t) { return uint16_t{500}; });
    const TerrainMeshData mesh_flat = build_terrain_mesh(flat.view);
    for (const auto& v : mesh_flat.vertices) {
        CHECK(v.normal.y == doctest::Approx(1.0f));
    }

    // Rising toward +x: raw 0,1000,2000,... -> dh/dx = 1000*SCALE/STEP = 5 m/m.
    const FieldData slope =
        make_field(0, [](uint32_t x, uint32_t) { return static_cast<uint16_t>(x * 1000); });
    const TerrainMeshData mesh_slope = build_terrain_mesh(slope.view);
    const auto& v = mesh_slope.vertices[2 * RES + 2]; // interior vertex
    const float dhdx = 1000.0f * SCALE / STEP;
    const float expected_len = std::sqrt(1.0f + dhdx * dhdx);
    CHECK(v.normal.x == doctest::Approx(-dhdx / expected_len));
    CHECK(v.normal.y == doctest::Approx(1.0f / expected_len));
    CHECK(v.normal.z == doctest::Approx(0.0f));
}

TEST_CASE("terrain mesh is deterministic and alpha is reserved-opaque") {
    const auto raw = [](uint32_t x, uint32_t z) {
        return static_cast<uint16_t>(53 * x * x + 7 * z);
    };
    const FieldData f = make_field(0, raw);
    const TerrainMeshData m1 = build_terrain_mesh(f.view);
    const TerrainMeshData m2 = build_terrain_mesh(f.view);
    REQUIRE(m1.vertices.size() == m2.vertices.size());
    for (size_t i = 0; i < m1.vertices.size(); ++i) {
        CHECK(m1.vertices[i].color_rgba == m2.vertices[i].color_rgba);
        // The dryness/dirt channel is gone (design ruling: splat keys off
        // core's surface_class only); alpha is reserved at 255.
        CHECK((m1.vertices[i].color_rgba >> 24) == 0xFFu);
    }
}

TEST_CASE("surface field drives the splat weight channels") {
    const FieldData f = make_field(0, [](uint32_t, uint32_t) { return uint16_t{100}; });

    // Surface data: sample 0 sand, 1 rock, 2 blend, 3 water bed, rest grass.
    std::vector<uint8_t> classes(RES * RES,
                                 static_cast<uint8_t>(dfn::math::SurfaceClass::Grass));
    classes[0] = static_cast<uint8_t>(dfn::math::SurfaceClass::Sand);
    classes[1] = static_cast<uint8_t>(dfn::math::SurfaceClass::Rock);
    classes[2] = static_cast<uint8_t>(dfn::math::SurfaceClass::GrassRockBlend);
    classes[3] = static_cast<uint8_t>(dfn::math::SurfaceClass::WaterBed);
    std::vector<float> dist(RES * RES, 100.0f);
    std::vector<float> water(RES * RES, dfn::math::NO_WATER);

    dfn::math::SurfaceFieldView surface;
    surface.chunk_coord = f.view.chunk_coord;
    surface.origin = f.view.origin;
    surface.resolution = RES;
    surface.step = STEP;
    surface.dist_to_water = dist;
    surface.water_surface = water;
    surface.surface_class = classes;

    const TerrainMeshData mesh = build_terrain_mesh(f.view, &surface);
    REQUIRE(mesh.vertices.size() == RES * RES);
    // Channel contract (fs_terrain): R = sand, G = rock, B = water bed.
    const auto red = [](uint32_t c) { return c & 0xFFu; };
    const auto green = [](uint32_t c) { return (c >> 8) & 0xFFu; };
    const auto blue = [](uint32_t c) { return (c >> 16) & 0xFFu; };
    CHECK(red(mesh.vertices[0].color_rgba) == 255);
    CHECK(green(mesh.vertices[0].color_rgba) == 0);
    CHECK(green(mesh.vertices[1].color_rgba) == 255);
    CHECK(green(mesh.vertices[2].color_rgba) == 128); // blend class = mid rock
    CHECK(blue(mesh.vertices[3].color_rgba) == 255);
    CHECK(red(mesh.vertices[4].color_rgba) == 0); // grass: no sand/rock/bed
    CHECK(green(mesh.vertices[4].color_rgba) == 0);
    CHECK(blue(mesh.vertices[4].color_rgba) == 0);

    // A mismatched surface grid is ignored (falls back to slope-only).
    surface.resolution = RES - 1;
    const TerrainMeshData fallback = build_terrain_mesh(f.view, &surface);
    CHECK(red(fallback.vertices[0].color_rgba) == 0);
    CHECK(green(fallback.vertices[1].color_rgba) == 0);
}

TEST_CASE("malformed heightfield views yield an empty mesh, never UB") {
    HeightFieldView bad;
    bad.resolution = 1; // too small to mesh
    CHECK(build_terrain_mesh(bad).vertices.empty());

    std::vector<uint16_t> short_data(3, 0);
    bad.resolution = RES;
    bad.step = STEP;
    bad.heights = short_data; // fewer than res^2 samples
    CHECK(build_terrain_mesh(bad).vertices.empty());
}

// --- LOD support: world-referenced UVs, border measurement, skirts ---------

namespace {

/// A field that is a real CHUNK: 129 samples at 2 m = exactly CHUNK_SIZE,
/// origin on the chunk grid. `cx`/`cz` are chunk coords.
struct ChunkField {
    std::vector<uint16_t> heights;
    HeightFieldView view;
};
ChunkField make_chunk(int cx, int cz,
                      const std::function<uint16_t(uint32_t, uint32_t)>& raw) {
    ChunkField f;
    const uint32_t res = static_cast<uint32_t>(dfn::config::HEIGHTMAP_RESOLUTION);
    const float step = static_cast<float>(dfn::config::HEIGHTMAP_STEP);
    f.heights.resize(static_cast<size_t>(res) * res);
    for (uint32_t z = 0; z < res; ++z) {
        for (uint32_t x = 0; x < res; ++x) {
            f.heights[static_cast<size_t>(z) * res + x] = raw(x, z);
        }
    }
    f.view.chunk_coord = {cx, cz};
    f.view.origin = {static_cast<float>(cx) * step * static_cast<float>(res - 1),
                     static_cast<float>(cz) * step * static_cast<float>(res - 1)};
    f.view.resolution = res;
    f.view.step = step;
    f.view.heights = f.heights;
    f.view.height_scale = SCALE;
    f.view.height_offset = OFFSET;
    return f;
}

/// A coarse LOD node delivered as a heightfield (the agreed seam with core):
/// 129 samples at the level's voxel size, origin = node coords * node size.
ChunkField make_node(uint8_t level, int nx, int nz,
                     const std::function<uint16_t(uint32_t, uint32_t)>& raw) {
    ChunkField f;
    const uint32_t res = dfn::render::LOD_NODE_VOXELS + 1;
    const float step = dfn::render::LOD_VOXEL_SIZE_M[level];
    const float size = dfn::render::lod_node_size_m(level);
    f.heights.resize(static_cast<size_t>(res) * res);
    for (uint32_t z = 0; z < res; ++z) {
        for (uint32_t x = 0; x < res; ++x) {
            f.heights[static_cast<size_t>(z) * res + x] = raw(x, z);
        }
    }
    f.view.chunk_coord = {nx, nz};
    f.view.origin = {static_cast<float>(nx) * size, static_cast<float>(nz) * size};
    f.view.resolution = res;
    f.view.step = step;
    f.view.heights = f.heights;
    f.view.height_scale = SCALE;
    f.view.height_offset = OFFSET;
    return f;
}

} // namespace

TEST_CASE("world-referenced UVs are a no-op for chunks and a fix for nodes") {
    const auto flat = [](uint32_t, uint32_t) { return uint16_t{0}; };
    const float tiles = 32.0f; // terrain_tiles_per_chunk, the shader's multiplier

    // A chunk far from the origin. The OLD formula was (offset in the
    // field)/(field span); the new one is world/CHUNK_SIZE. They differ by
    // exactly the chunk coordinate — a WHOLE number of texture repeats once
    // multiplied by tiles_per_chunk — so the sampler sees the identical
    // texel. That is the claim; here it is measured rather than asserted.
    const ChunkField c = make_chunk(3, 2, flat);
    const TerrainMeshData chunk_mesh = build_terrain_mesh(c.view);
    const uint32_t res = c.view.resolution;
    for (uint32_t i : {0u, res / 2, res - 1}) {
        const auto& v = chunk_mesh.vertices[static_cast<size_t>(i) * res + i];
        const float old_u = static_cast<float>(i) / static_cast<float>(res - 1);
        const float delta_repeats = (v.uv.x - old_u) * tiles;
        CHECK(delta_repeats == doctest::Approx(std::round(delta_repeats)));
    }

    // CONTROL — the case the change exists for, and the case that proves the
    // check above is not vacuous. A level-3 node spans 2048 m. Under the OLD
    // formula its uv still ran 0..1, i.e. 32 texture repeats stretched over
    // 2048 m = one 64 m tile, against 8 m on a chunk: the SAME material at
    // eight times the size, which is a visible seam at every node border.
    // World-referenced UVs give it 2048/256 = 8 uv units, i.e. 256 repeats.
    const ChunkField n = make_node(3, 1, 1, flat);
    const TerrainMeshData node_mesh = build_terrain_mesh(n.view);
    const uint32_t nres = n.view.resolution;
    const float span_u = node_mesh.vertices[nres - 1].uv.x
                       - node_mesh.vertices[0].uv.x;
    CHECK(span_u == doctest::Approx(8.0f));
    CHECK(span_u > 1.5f); // the old formula would give exactly 1.0
}

TEST_CASE("border step is measured on the border, not over the whole field") {
    // A field that is flat on all four borders and has a 40 m spike dead
    // centre. A skirt sized from the WHOLE field would be 40 m deep on ground
    // that needs none — the measurement has to be the border's.
    const uint32_t mid = static_cast<uint32_t>(dfn::config::HEIGHTMAP_RESOLUTION) / 2;
    const ChunkField f = make_chunk(0, 0, [&](uint32_t x, uint32_t z) {
        return (x == mid && z == mid) ? uint16_t{4000} : uint16_t{0};
    });
    CHECK(dfn::render::terrain_border_max_step_m(f.view) == doctest::Approx(0.0f));

    // And it DOES see a step that lies on a border: a ramp along +x. The two
    // z-borders each step 300 raw units per sample; the two x-borders are
    // constant along their own run, so the answer is the ramp's step.
    const ChunkField ramp = make_chunk(0, 0, [](uint32_t x, uint32_t) {
        return static_cast<uint16_t>(x * 300);
    });
    CHECK(dfn::render::terrain_border_max_step_m(ramp.view)
          == doctest::Approx(300.0f * SCALE));
}

TEST_CASE("a skirt hangs down from the border and leaves the surface alone") {
    const auto raw = [](uint32_t x, uint32_t z) {
        return static_cast<uint16_t>(137 * x + 291 * z);
    };
    const ChunkField f = make_chunk(0, 0, raw);
    const TerrainMeshData plain = build_terrain_mesh(f.view, nullptr, {});

    dfn::render::TerrainMeshOptions options;
    options.skirt_depth_m = 5.0f;
    const TerrainMeshData skirted = build_terrain_mesh(f.view, nullptr, options);

    const uint32_t res = f.view.resolution;
    const size_t grid = static_cast<size_t>(res) * res;

    // CONTROL: with no skirt the mesh is exactly the surface grid. If skirt
    // geometry ever leaked into the default path, distant terrain would grow
    // an apron everywhere and chunks would gain hidden triangles for nothing.
    CHECK(plain.vertices.size() == grid);

    // The surface is untouched: the first `grid` vertices are identical, so
    // every existing caller that indexes [z * res + x] still addresses ground.
    REQUIRE(skirted.vertices.size() == grid + static_cast<size_t>(res) * 4);
    for (size_t i = 0; i < grid; ++i) {
        CHECK(skirted.vertices[i].position.y
              == doctest::Approx(plain.vertices[i].position.y));
    }
    // Every skirt vertex is exactly the skirt depth BELOW some border vertex.
    // A skirt that went UP would be visible geometry standing in the air.
    for (size_t i = grid; i < skirted.vertices.size(); ++i) {
        const glm::vec3 p = skirted.vertices[i].position;
        bool found = false;
        for (uint32_t k = 0; k < res && !found; ++k) {
            const std::array<size_t, 4> border{
                k, static_cast<size_t>(res - 1) * res + k,
                static_cast<size_t>(k) * res,
                static_cast<size_t>(k) * res + (res - 1)};
            for (size_t b : border) {
                const glm::vec3 q = skirted.vertices[b].position;
                if (std::fabs(q.x - p.x) < 1e-3f && std::fabs(q.z - p.z) < 1e-3f
                    && std::fabs((q.y - options.skirt_depth_m) - p.y) < 1e-3f) {
                    found = true;
                    break;
                }
            }
        }
        CHECK(found);
    }
    // Indices stay in range and the skirt adds exactly 2 triangles per span.
    CHECK(skirted.indices.size()
          == plain.indices.size() + static_cast<size_t>(res - 1) * 4 * 6);
    for (const uint32_t index : skirted.indices) {
        CHECK(index < skirted.vertices.size());
    }
}
