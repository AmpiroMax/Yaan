/*
Created: 09:08:2026 - 11:57:20
Last updated: 09:08:2026 - 11:57:20
Module: tests
File: tests/render/WaterMesherTests.cpp

Responsibility:
- Unit tests for per-body water meshes: lake plane height/extent, river ribbon
  structure, descending surface preservation, per-station width, degenerate
  inputs.

Key items:
- doctest cases over build_lake_mesh / build_river_mesh.

Dependencies:
- Uses: doctest, engine/render WaterMesher.
- Used by: ctest (render_water_mesher).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 11:57:20: Stage 3b — initial tests.
*/

#include "engine/render/sources/WaterMesher.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using dfn::math::LakePlane;
using dfn::math::RiverStation;
using dfn::render::build_lake_mesh;
using dfn::render::build_river_mesh;
using dfn::render::MeshData;

TEST_CASE("lake mesh is a flat fan at surface height covering the extent") {
    LakePlane lake;
    lake.center = {230.0f, 520.0f};
    lake.half_extent = {45.0f, 70.0f};
    lake.surface_height = 15.0f;
    const MeshData mesh = build_lake_mesh(lake, 24.0f, 2.0f, 32);
    REQUIRE(mesh.vertices.size() == 33); // center + rim
    REQUIRE(mesh.indices.size() == 32 * 3);
    float max_dx = 0.0f;
    float max_dz = 0.0f;
    for (const auto& v : mesh.vertices) {
        CHECK(v.position.y == doctest::Approx(15.0f));
        CHECK(v.normal.y == doctest::Approx(1.0f));
        max_dx = std::max(max_dx, std::abs(v.position.x - 230.0f));
        max_dz = std::max(max_dz, std::abs(v.position.z - 520.0f));
    }
    CHECK(max_dx == doctest::Approx(47.0f)); // half_extent + margin
    CHECK(max_dz == doctest::Approx(72.0f));
}

TEST_CASE("river ribbon keeps station heights and widths") {
    // Descending surface (core invariant) with growing width.
    const std::vector<RiverStation> stations{
        {{700.0f, 300.0f}, 30.0f, 2.0f},
        {{696.0f, 304.0f}, 29.5f, 2.5f},
        {{692.0f, 308.0f}, 29.5f, 3.0f},
        {{688.0f, 312.0f}, 28.0f, 3.5f},
    };
    const float margin = 1.0f;
    const MeshData mesh = build_river_mesh(stations, 24.0f, margin);
    REQUIRE(mesh.vertices.size() == stations.size() * 2);
    REQUIRE(mesh.indices.size() == (stations.size() - 1) * 6);
    for (size_t i = 0; i < stations.size(); ++i) {
        const auto& left = mesh.vertices[i * 2];
        const auto& right = mesh.vertices[i * 2 + 1];
        // Height = station surface on both rims (water descends downstream).
        CHECK(left.position.y == doctest::Approx(stations[i].surface_height));
        CHECK(right.position.y == doctest::Approx(stations[i].surface_height));
        const float dx = left.position.x - right.position.x;
        const float dz = left.position.z - right.position.z;
        CHECK(std::sqrt(dx * dx + dz * dz)
              == doctest::Approx(2.0f * (stations[i].half_width + margin)));
    }
    // Rim heights never increase along the ribbon (monotonic preserved).
    for (size_t i = 2; i < mesh.vertices.size(); ++i) {
        CHECK(mesh.vertices[i].position.y <= mesh.vertices[i - 2].position.y + 1e-4f);
    }
}

TEST_CASE("degenerate water inputs return empty meshes") {
    CHECK(build_lake_mesh({}, 24.0f, 2.0f, 2).vertices.empty()); // < 3 segments
    CHECK(build_lake_mesh({}, 0.0f, 2.0f).vertices.empty());     // bad uv tile
    const std::vector<RiverStation> one{{{0.0f, 0.0f}, 1.0f, 1.0f}};
    CHECK(build_river_mesh(one, 24.0f, 1.0f).vertices.empty());
    CHECK(build_river_mesh({}, 24.0f, 1.0f).vertices.empty());
}
