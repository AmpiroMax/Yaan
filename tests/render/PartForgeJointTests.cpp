/*
Created: 17:08:2026 - 12:39:52
Last updated: 17:08:2026 - 13:54:03
Module: tests
File: tests/render/PartForgeJointTests.cpp

Responsibility:
- The connector family's contract (HOUSES.md §3-4): joints are CLOSED volumes
  with outward winding, their facets are true planes at the across-flats
  radius in the orientation the judge measures against, a sleeper offers a
  flat bed and a flat seat, and every name states the working properties.

Key items:
- meshtest::half_edge_defects(): manifold-closedness meter with its failing control.
- meshtest::signed_volume(): outward-winding meter.

Dependencies:
- Uses: engine/render (PartForge), doctest.
- Used by: ctest (render_part_forge_joints).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 30: every meter here ships with the case it exists to reject — the
  open tube for closedness, the inverted prism for winding.
- Explicit bounds, never Approx().epsilon() — most quantities here are
  differences whose correct value is zero (Rule 40).
*/
/*
UPD:
- 17:08:2026 - 12:39:52: Создан вместе с семьёй соединителей.
- 17:08:2026 - 13:23:29: измерители вынесены в MeshMeters.h (второй потребитель —
  тесты крыш; копия на файл — правило 39 в миниатюре).
- 17:08:2026 - 13:54:03: счёт каталога под ряд высот стен (HOUSES.md §6): стойки 384 -> 960
  (h11/12/13/14/16), перевязки 8 -> 20 (h6/11/12/13/14).
*/

#include "engine/render/sources/PartForge.h"

#include "MeshMeters.h"

#include <cmath>
#include <doctest/doctest.h>
#include <set>
#include <string>

using namespace dfn::render;

namespace {

PartParams joint_params(int facets, int d_cm, int h_u, PartMaterial mat,
                        int variant = 0) {
    PartParams p;
    p.kind = PartKind::JointPost;
    p.material = mat;
    p.facets = facets;
    p.diameter_cm = d_cm;
    p.length_u = h_u;
    p.variant = variant;
    p.wear = 0.3f;
    p.name = part_name(p);
    p.seed = 12345;
    return p;
}

} // namespace

TEST_CASE("the closedness meter rejects an open tube (Rule 30 control)") {
    // Four walls of a box, NO caps: the exact defect the meter exists to
    // catch. 8 boundary edges (4 top + 4 bottom) must be reported.
    MeshData tube;
    const glm::vec3 v[8] = {
        {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1},
        {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1},
    };
    quad(tube, v[0], v[1], v[5], v[4], 0xffffffff);
    quad(tube, v[1], v[2], v[6], v[5], 0xffffffff);
    quad(tube, v[2], v[3], v[7], v[6], 0xffffffff);
    quad(tube, v[3], v[0], v[4], v[7], 0xffffffff);
    CHECK(meshtest::half_edge_defects(tube) == 8);
}

TEST_CASE("every joint shape is a sealed hull with outward winding") {
    for (const int facets : {4, 6, 8, 0}) {
        for (const int d : {35, 100}) {
            const RegistryObject obj = forge_part(joint_params(facets, d, 11,
                                                               PartMaterial::Stone));
            CAPTURE(facets);
            CAPTURE(d);
            CHECK(meshtest::half_edge_defects(obj.wood) == 0);
            // Volume of an across-flats-d prism of height 2.75 m: bounded by
            // the inscribed cylinder from below-ish and the box from above.
            const double d_m = d * 0.01;
            const double vol = meshtest::signed_volume(obj.wood);
            CHECK(vol > 0.5 * d_m * d_m * 2.75 * 0.5);
            CHECK(vol < d_m * d_m * 2.75 * 1.01);
        }
    }
    // The winding meter's own control: the same prism inverted must read
    // NEGATIVE, or the meter cannot tell outward from inward.
    RegistryObject obj = forge_part(joint_params(4, 50, 11, PartMaterial::Timber));
    MeshData flipped = obj.wood;
    for (std::size_t i = 0; i + 2 < flipped.indices.size(); i += 3) {
        std::swap(flipped.indices[i], flipped.indices[i + 1]);
    }
    CHECK(meshtest::signed_volume(flipped) < 0.0);
}

TEST_CASE("facet contract: a plane at the across-flats radius, first normal at +X") {
    // The judge (engine/world) measures panel seating against THIS orientation
    // convention; if it moves, move both.
    for (const int facets : {4, 6, 8}) {
        const float r_in = 0.25f;
        const RegistryObject obj = forge_part(joint_params(facets, 50, 11,
                                                           PartMaterial::Timber));
        CAPTURE(facets);
        float max_x = -1e9f;
        int on_facet = 0;
        for (const auto& v : obj.wood.vertices) {
            max_x = std::max(max_x, v.position.x);
            if (v.position.x > r_in - 1e-4f) {
                ++on_facet;
            }
        }
        // No vertex may stand PROUD of the facet plane (a proud vertex is the
        // arris the panel would ride over)...
        CHECK(max_x < r_in + 1e-4f);
        // ...and the facet is really THERE: both vertex columns of facet 0
        // sit exactly on x = r_in, over every band ring.
        CHECK(on_facet >= 8);
    }
    // The round joint: every ring vertex on the d/2 circle — no facet
    // anywhere, which IS its rule (any angle).
    const RegistryObject round_j = forge_part(joint_params(0, 50, 11,
                                                           PartMaterial::Timber));
    for (const auto& v : round_j.wood.vertices) {
        const float r = std::sqrt(v.position.x * v.position.x
                                  + v.position.z * v.position.z);
        const bool on_axis = r < 1e-4f; // cap-fan centres
        if (!on_axis) {
            CHECK(r > 0.25f - 1e-3f);
            CHECK(r < 0.25f + 1e-3f);
        }
    }
}

TEST_CASE("a 4-facet sleeper has a flat bed at y=0 and a flat seat at y=d") {
    PartParams p;
    p.kind = PartKind::Sleeper;
    p.material = PartMaterial::Timber;
    p.facets = 4;
    p.diameter_cm = 50;
    p.length_u = 16;
    p.wear = 0.3f;
    p.name = part_name(p);
    const RegistryObject obj = forge_part(p);
    CHECK(meshtest::half_edge_defects(obj.wood) == 0);
    CHECK(meshtest::signed_volume(obj.wood) > 0.0); // outward, not an inside-out log
    float lo = 1e9f;
    float hi = -1e9f;
    int at_bed = 0;
    int at_seat = 0;
    for (const auto& v : obj.wood.vertices) {
        lo = std::min(lo, v.position.y);
        hi = std::max(hi, v.position.y);
        if (std::fabs(v.position.y) < 1e-4f) ++at_bed;
        if (std::fabs(v.position.y - 0.50f) < 1e-4f) ++at_seat;
    }
    // Origin at the UNDERSIDE (the stacking convention): bed exactly at 0,
    // seat exactly at the across-flats diameter.
    CHECK(lo > -1e-4f);
    CHECK(hi < 0.50f + 1e-4f);
    CHECK(at_bed >= 4);
    CHECK(at_seat >= 4);
}

TEST_CASE("names carry the working properties the composer filters by") {
    CHECK(part_name(joint_params(4, 50, 11, PartMaterial::Timber))
          == "joint-timber-d50-n4-h11-w03");
    CHECK(part_name(joint_params(0, 100, 16, PartMaterial::Stone, 1))
          == "joint-stone-d100-nr-h16-cap-w03");
    PartParams s;
    s.kind = PartKind::Sleeper;
    s.material = PartMaterial::TimberDark;
    s.facets = 0;
    s.diameter_cm = 35;
    s.length_u = 12;
    s.wear = 0.8f;
    CHECK(part_name(s) == "sleeper-dark-d35-nr-12u-w08");
}

TEST_CASE("determinism: same params, same bytes (Rule 13.1 for objects)") {
    const PartParams p = joint_params(6, 75, 16, PartMaterial::Brick, 1);
    const RegistryObject a = forge_part(p);
    const RegistryObject b = forge_part(p);
    CHECK(object_content_hash(a) == object_content_hash(b));
    CHECK(a.wood.vertices.size() == b.wood.vertices.size());
}

TEST_CASE("the catalogue grew by rule and every name is unique") {
    const auto cat = kit_catalogue();
    int joints = 0;
    int sleepers = 0;
    int corners = 0;
    std::set<std::string> names;
    for (const PartParams& p : cat) {
        CHECK(names.insert(p.name).second); // a duplicate name is two files fighting
        if (p.kind == PartKind::JointPost) ++joints;
        if (p.kind == PartKind::Sleeper) ++sleepers;
        if (p.kind == PartKind::LogCorner) ++corners;
    }
    // The family rows as designed: 4 shapes x 4 diameters x (2 woods x 5
    // heights + 2 masonry x 5 heights x 2 capitals) x 2 wears = 320 + 640
    // (heights are the wall row 11/12/13/14 plus tall 16 — HOUSES.md §6);
    // sleepers 2 x 2 x 3 x 3 x 2 = 72; corners 5 heights x 2 woods x 2 wears.
    CHECK(joints == 960);
    CHECK(sleepers == 72);
    CHECK(corners == 20);
}

TEST_CASE("log corner: stubs run out past BOTH walls at the panel's course rhythm") {
    PartParams p;
    p.kind = PartKind::LogCorner;
    p.material = PartMaterial::Timber;
    p.length_u = 11;
    p.wear = 0.3f;
    const RegistryObject obj = forge_part(p);
    glm::vec3 lo{1e9f};
    glm::vec3 hi{-1e9f};
    for (const auto& v : obj.wood.vertices) {
        lo = glm::min(lo, v.position);
        hi = glm::max(hi, v.position);
    }
    // выпуск «в обло»: 0.30 m past the corner axis on both walls' far sides
    // (wobble-free at bar ends, so the bound is tight).
    CHECK(lo.x < -0.28f);
    CHECK(lo.z < -0.28f);
    CHECK(hi.x > 0.48f);
    CHECK(hi.z > 0.48f);
    // 12 courses of 0.23 fill a 2.75 m wall's height.
    CHECK(hi.y > 2.5f);
    CHECK(hi.y < 2.9f);
}
