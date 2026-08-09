/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
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
*/

#include "engine/render/sources/TerrainMesher.h"

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

    // Positions span origin .. origin + (res-1)*step; UVs span 0..1.
    const auto& first = mesh.vertices.front();
    const auto& last = mesh.vertices.back();
    CHECK(first.position.x == doctest::Approx(0.0f));
    CHECK(last.position.x == doctest::Approx(STEP * (RES - 1)));
    CHECK(last.position.z == doctest::Approx(STEP * (RES - 1)));
    CHECK(first.uv.x == doctest::Approx(0.0f));
    CHECK(last.uv.x == doctest::Approx(1.0f));
    CHECK(last.uv.y == doctest::Approx(1.0f));
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

TEST_CASE("terrain mesh is deterministic and colors are opaque") {
    const auto raw = [](uint32_t x, uint32_t z) {
        return static_cast<uint16_t>(53 * x * x + 7 * z);
    };
    const FieldData f = make_field(0, raw);
    const TerrainMeshData m1 = build_terrain_mesh(f.view);
    const TerrainMeshData m2 = build_terrain_mesh(f.view);
    REQUIRE(m1.vertices.size() == m2.vertices.size());
    for (size_t i = 0; i < m1.vertices.size(); ++i) {
        CHECK(m1.vertices[i].color_rgba == m2.vertices[i].color_rgba);
        CHECK((m1.vertices[i].color_rgba & 0xFF000000u) == 0xFF000000u); // alpha 255
    }
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
