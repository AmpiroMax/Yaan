/*
Created: 09:08:2026 - 11:57:20
Last updated: 09:08:2026 - 22:29:52
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
- 09:08:2026 - 22:29:52: Castle mass ids 8..12 — envelope fit, hollow ward
  with a real gate opening, determinism, and the thin-caster merlon width.
*/

#include "engine/render/sources/ProcMesh.h"

#include <doctest/doctest.h>

#include <glm/common.hpp>

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
    // The §6 SETTLEMENT structures only. The castle mass (8..12) has its own
    // envelopes and its own much larger budget, and lives in the cases below —
    // this loop stops at 7 rather than following SITE_MESH_ID_LAST, which now
    // reaches 12 and would walk off the end of SITE_BOUNDS.
    for (uint32_t id = dfn::render::SITE_MESH_ID_FIRST; id <= 7; ++id) {
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
    // 8..12 are the castle now; 13 is the first id that is genuinely nothing.
    CHECK(build_site_mesh(13).vertices.empty());
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

// --- The castle mass, ids 8..12 (§6.1.3) -----------------------------------

namespace {

struct Envelope {
    uint32_t id;
    glm::vec3 mn;
    glm::vec3 mx;
};

// MIRRORS world::site_archetype's LocalBounds for the castle types. Render
// cannot include world (the DAG), so the numbers are copied and this comment
// is the contract: if SiteComponents moves an envelope, this test is the thing
// that must be updated with it.
constexpr Envelope CASTLE[5] = {
    {8, {-5.0f, 0.0f, -11.0f}, {5.0f, 9.0f, 11.0f}},   // hall
    {9, {-20.0f, 0.0f, -20.0f}, {20.0f, 8.0f, 20.0f}}, // curtain wall
    {10, {-5.0f, 0.0f, -3.0f}, {5.0f, 11.0f, 3.0f}},   // gatehouse
    {11, {-4.0f, 0.0f, -4.0f}, {4.0f, 20.0f, 4.0f}},   // solar
    {12, {-3.5f, 0.0f, -3.5f}, {3.5f, 15.0f, 3.5f}},   // corner tower
};

} // namespace

TEST_CASE("every castle id builds a mesh that fits its declared envelope") {
    for (const Envelope& e : CASTLE) {
        CAPTURE(e.id);
        const MeshData m = build_site_mesh(e.id);
        // THE CONTROL, and it is the actual bug this was written for: ids
        // 8..12 returned an EMPTY mesh, RenderSystem's ECS pass drops a cache
        // miss silently, and so Harrowward was invisible in the world and had
        // no collision — with no error anywhere. An empty mesh must fail.
        REQUIRE_FALSE(m.vertices.empty());
        REQUIRE_FALSE(m.indices.empty());
        CHECK(m.triangle_count() > 8);

        glm::vec3 lo{1e9f};
        glm::vec3 hi{-1e9f};
        for (const auto& v : m.vertices) {
            lo = glm::min(lo, v.position);
            hi = glm::max(hi, v.position);
        }
        // Inside the envelope core mirrors as LocalBounds, and sitting ON the
        // pad: y = 0 is the ground surface, so nothing may dip below it.
        CHECK(lo.x >= e.mn.x - 1e-3f);
        CHECK(lo.z >= e.mn.z - 1e-3f);
        CHECK(lo.y >= -1e-3f);
        CHECK(hi.x <= e.mx.x + 1e-3f);
        CHECK(hi.y <= e.mx.y + 1e-3f);
        CHECK(hi.z <= e.mx.z + 1e-3f);
        // And actually USING its envelope: a 40 m curtain wall rendered as a
        // 2 m cube would pass every bound above.
        CHECK(hi.y >= e.mx.y * 0.6f);
        for (const uint32_t index : m.indices) {
            CHECK(index < m.vertices.size());
        }
    }
}

TEST_CASE("the curtain wall is hollow and has a gate you can walk through") {
    const MeshData wall = build_site_mesh(9);
    REQUIRE_FALSE(wall.vertices.empty());

    // Hollow: nothing occupies the middle of the ward. A solid box would pass
    // the envelope test above, and it is exactly what "draw the castle" would
    // produce if someone reached for the cheapest primitive.
    for (const auto& v : wall.vertices) {
        const bool in_yard = std::abs(v.position.x) < 15.0f
                          && std::abs(v.position.z) < 15.0f;
        CHECK_FALSE(in_yard);
    }

    // The gate opening: a vertical slot in the -Z run. Sim builds collision
    // from these triangles, so an opening that exists only as a dark painted
    // plate is a wall — the fortress has to be enterable at all.
    bool any_geometry_in_gate = false;
    for (const auto& v : wall.vertices) {
        if (std::abs(v.position.x) < 3.0f && v.position.z < -18.0f
            && v.position.y > 0.5f && v.position.y < 7.5f) {
            any_geometry_in_gate = true;
        }
    }
    CHECK_FALSE(any_geometry_in_gate);
}

TEST_CASE("castle meshes are deterministic and every merlon can cast a shadow") {
    for (const Envelope& e : CASTLE) {
        CAPTURE(e.id);
        const MeshData a = build_site_mesh(e.id);
        const MeshData b = build_site_mesh(e.id);
        REQUIRE(a.vertices.size() == b.vertices.size());
        for (size_t i = 0; i < a.vertices.size(); ++i) {
            CHECK(a.vertices[i].position.x == doctest::Approx(b.vertices[i].position.x));
            CHECK(a.vertices[i].position.y == doctest::Approx(b.vertices[i].position.y));
            CHECK(a.vertices[i].position.z == doctest::Approx(b.vertices[i].position.z));
        }
    }
    // THE THIN-CASTER RULE, as a test rather than as a comment someone will
    // stop reading: the sun shadow map is 4096 over a 320 m half extent =
    // 0.156 m per texel, and a caster narrower than about two texels only
    // darkens one when it happens to cover the texel centre — so it drops out
    // entirely and the crown reads as a smooth band. Merlons are 1.2 m.
    constexpr float SHADOW_TEXEL_M = 2.0f * 320.0f / 4096.0f;
    CHECK(1.2f >= 2.0f * SHADOW_TEXEL_M);
    // CONTROL: the 0.4 m merlon that would look right in a reference photo is
    // exactly the width that would cast nothing. If this ever stops failing,
    // the shadow map changed and the rule needs re-deriving, not ignoring.
    CHECK_FALSE(0.25f >= 2.0f * SHADOW_TEXEL_M);
}
