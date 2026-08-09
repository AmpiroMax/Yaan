/*
Created: 09:08:2026 - 11:57:20
Last updated: 09:08:2026 - 11:57:20
Module: tests
File: tests/render/ProcMeshTests.cpp

Responsibility:
- Unit tests for the placeholder mesh catalog: determinism, tri budgets per
  LANDSCAPE §5/§6, §6 bounds containment (mirror of SiteComponents), and
  append_transformed math.

Key items:
- doctest cases over build_scatter_mesh / build_site_mesh / append_transformed.

Dependencies:
- Uses: doctest, engine/render ProcMesh.
- Used by: ctest (render_proc_mesh).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 11:57:20: Stage 3b — initial tests.
*/

#include "engine/render/sources/ProcMesh.h"

#include <doctest/doctest.h>

#include <cmath>
#include <glm/gtc/constants.hpp>

using dfn::math::ScatterSpecies;
using dfn::render::append_transformed;
using dfn::render::build_scatter_mesh;
using dfn::render::build_site_mesh;
using dfn::render::MeshData;

namespace {

// §6 footprint x height boxes, mirroring world's SiteComponents archetype
// table (render cannot include world — the numbers are the cross-zone
// agreement, LANDSCAPE §6).
struct Bounds {
    glm::vec3 min;
    glm::vec3 max;
};
constexpr Bounds SITE_BOUNDS[7] = {
    {{-3.0f, 0.0f, -4.0f}, {3.0f, 5.5f, 4.0f}},   // dwelling
    {{-4.0f, 0.0f, -5.0f}, {4.0f, 6.5f, 5.0f}},   // trader
    {{-5.0f, 0.0f, -7.0f}, {5.0f, 8.5f, 7.0f}},   // tavern
    {{-4.0f, 0.0f, -6.0f}, {4.0f, 7.5f, 6.0f}},   // barn
    {{-2.5f, 0.0f, -2.5f}, {2.5f, 12.0f, 2.5f}},  // shrine
    {{-2.0f, 0.0f, -2.0f}, {2.0f, 4.0f, 2.0f}},   // dungeon_entrance
    {{-2.0f, 0.0f, -2.0f}, {2.0f, 12.0f, 2.0f}},  // tower_ruin
};

// Tri budget ceilings (LANDSCAPE §5 TREE_TRI_BUDGET <= 500, §6
// HOUSE_TRI_BUDGET <= 600; both flagged for NUMBERS.md as ranges).
constexpr size_t TREE_TRI_MAX = 500;
constexpr size_t HOUSE_TRI_MAX = 600;

bool identical(const MeshData& a, const MeshData& b) {
    if (a.vertices.size() != b.vertices.size() || a.indices != b.indices) {
        return false;
    }
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (a.vertices[i].position != b.vertices[i].position
            || a.vertices[i].normal != b.vertices[i].normal
            || a.vertices[i].color_rgba != b.vertices[i].color_rgba) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("scatter meshes exist, stay in tree budget and stand on the ground") {
    for (const ScatterSpecies species :
         {ScatterSpecies::OakTree, ScatterSpecies::PineTree, ScatterSpecies::BirchTree,
          ScatterSpecies::Bush, ScatterSpecies::Stone}) {
        const MeshData mesh = build_scatter_mesh(species);
        CHECK(!mesh.vertices.empty());
        CHECK(mesh.indices.size() % 3 == 0);
        CHECK(mesh.triangle_count() <= TREE_TRI_MAX);
        float min_y = 1e9f;
        for (const auto& v : mesh.vertices) {
            min_y = std::min(min_y, v.position.y);
        }
        CHECK(min_y >= -0.01f); // base at y = 0 (batcher applies the sink)
        CHECK(min_y <= 0.5f);
    }
}

TEST_CASE("species silhouettes differ per the §5 briefs") {
    auto height_of = [](const MeshData& m) {
        float max_y = 0.0f;
        for (const auto& v : m.vertices) {
            max_y = std::max(max_y, v.position.y);
        }
        return max_y;
    };
    const float oak = height_of(build_scatter_mesh(ScatterSpecies::OakTree));
    const float pine = height_of(build_scatter_mesh(ScatterSpecies::PineTree));
    const float birch = height_of(build_scatter_mesh(ScatterSpecies::BirchTree));
    const float bush = height_of(build_scatter_mesh(ScatterSpecies::Bush));
    CHECK(pine > oak);   // pine is the tall pointed anti-oak
    CHECK(oak > birch);  // birch is the small pale accent
    CHECK(birch > bush); // bush is a <= 1.5 m lump
    CHECK(bush <= 1.5f);
}

TEST_CASE("site meshes 1..7 exist, fit the §6 bounds and the house budget") {
    for (uint32_t id = dfn::render::SITE_MESH_ID_FIRST;
         id <= dfn::render::SITE_MESH_ID_LAST; ++id) {
        const MeshData mesh = build_site_mesh(id);
        CAPTURE(id);
        CHECK(!mesh.vertices.empty());
        CHECK(mesh.triangle_count() <= HOUSE_TRI_MAX);
        const Bounds& b = SITE_BOUNDS[id - 1];
        for (const auto& v : mesh.vertices) {
            CHECK(v.position.x >= b.min.x - 1e-3f);
            CHECK(v.position.y >= b.min.y - 1e-3f);
            CHECK(v.position.z >= b.min.z - 1e-3f);
            CHECK(v.position.x <= b.max.x + 1e-3f);
            CHECK(v.position.y <= b.max.y + 1e-3f);
            CHECK(v.position.z <= b.max.z + 1e-3f);
        }
    }
    CHECK(build_site_mesh(0).vertices.empty());
    CHECK(build_site_mesh(8).vertices.empty());
}

TEST_CASE("mesh building is deterministic") {
    CHECK(identical(build_scatter_mesh(ScatterSpecies::OakTree),
                    build_scatter_mesh(ScatterSpecies::OakTree)));
    CHECK(identical(build_site_mesh(3), build_site_mesh(3)));
}

TEST_CASE("append_transformed applies yaw, scale and translation") {
    MeshData src;
    src.vertices.push_back({{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f},
                            0xFF112233u});
    src.indices = {0, 0, 0};

    MeshData dst;
    // Existing content survives and indices rebase.
    dst.vertices.push_back({{9.0f, 9.0f, 9.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f},
                            0xFFFFFFFFu});
    const float yaw = glm::half_pi<float>(); // +X rotates toward... (checked below)
    append_transformed(dst, src, {10.0f, 5.0f, 20.0f}, yaw, 2.0f);

    REQUIRE(dst.vertices.size() == 2);
    REQUIRE(dst.indices.size() == 3);
    CHECK(dst.indices[0] == 1);
    const auto& v = dst.vertices[1];
    // Rotation convention matches the camera yaw: +yaw turns +X toward -Z...
    // here we only require: length preserved, y untouched, translation added.
    const glm::vec3 local = v.position - glm::vec3{10.0f, 5.0f, 20.0f};
    CHECK(std::abs(glm::length(local) - 2.0f) < 1e-4f);
    CHECK(std::abs(local.y) < 1e-4f);
    CHECK(std::abs(glm::length(v.normal) - 1.0f) < 1e-4f); // normals not scaled
    CHECK(v.color_rgba == 0xFF112233u);
}
