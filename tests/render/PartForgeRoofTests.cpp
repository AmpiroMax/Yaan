/*
Module: tests
File: tests/render/PartForgeRoofTests.cpp

Responsibility:
- The roof variants' contract: every covering builds closed geometry with
  outward winding, the тёс underdeck stays on the slope, the hip narrows
  toward its apex (and the half-hip keeps its top edge), the vent stays
  inside its base square, names and catalogue counts hold.

Key items:
- shape assertions per covering; the hip taper meter with its half-hip pair.

Dependencies:
- Uses: engine/render (PartForge), MeshMeters.h, doctest.
- Used by: ctest (render_part_forge_roofs).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The hip taper is measured as a PAIR (triangle vs полувальма): one number
  could pass a hip that never narrows; the pair pins the variant to the
  geometry it names (Rule 30's discriminating form).
*/

#include "engine/render/sources/PartForge.h"

#include "MeshMeters.h"

#include <algorithm>
#include <doctest/doctest.h>

using namespace dfn::render;

namespace {

PartParams roof_params(PartKind kind, PartMaterial mat, int len, int run, int rise,
                       int variant = 0) {
    PartParams p;
    p.kind = kind;
    p.material = mat;
    p.length_u = len;
    p.width_u = run;
    p.height_u = rise;
    p.variant = variant;
    p.wear = 0.5f;
    p.name = part_name(p);
    p.seed = 4242;
    return p;
}

/// Mesh z-extent measured over vertices whose slope parameter sits in the
/// top fifth of the rise — how wide the hip still is near its apex.
float top_width(const RegistryObject& obj, float rise) {
    float lo = 1e9f;
    float hi = -1e9f;
    for (const auto& v : meshtest::solid_of(obj).vertices) {
        if (v.position.y > rise * 0.8f) {
            lo = std::min(lo, v.position.z);
            hi = std::max(hi, v.position.z);
        }
    }
    return hi > lo ? hi - lo : 0.0f;
}

} // namespace

TEST_CASE("every covering builds closed, outward geometry") {
    for (const PartMaterial mat : {PartMaterial::Thatch, PartMaterial::Shingle,
                                   PartMaterial::Timber, PartMaterial::Tile,
                                   PartMaterial::Turf}) {
        const RegistryObject obj = forge_part(
            roof_params(PartKind::RoofSlope, mat, 12, 8, 8));
        CAPTURE(static_cast<int>(mat));
        REQUIRE(!meshtest::solid_of(obj).indices.empty());
        CHECK(meshtest::half_edge_defects(meshtest::solid_of(obj)) == 0);
        CHECK(meshtest::signed_volume(meshtest::solid_of(obj)) > 0.0);
    }
}

TEST_CASE("тёс underdeck sits ON the slope, not past the eaves") {
    // The first cut of the board roof centred its underdeck at the eaves
    // corner, hanging half of it into the room below. Nothing may reach more
    // than the fringe-length past the eaves line (x < 0) or above the ridge.
    const RegistryObject obj = forge_part(
        roof_params(PartKind::RoofSlope, PartMaterial::Timber, 12, 8, 8));
    float min_x = 1e9f;
    float max_y = -1e9f;
    for (const auto& v : meshtest::solid_of(obj).vertices) {
        min_x = std::min(min_x, v.position.x);
        max_y = std::max(max_y, v.position.y);
    }
    CHECK(min_x > -0.6f);
    CHECK(max_y < 2.0f + 0.6f); // rise 2 m + ridge beam + deck headroom
}

TEST_CASE("the hip narrows to its apex; the half-hip keeps its top edge (the pair)") {
    const float rise = 2.0f;
    const RegistryObject tri = forge_part(
        roof_params(PartKind::RoofHip, PartMaterial::Shingle, 12, 8, 8, 0));
    const RegistryObject polu = forge_part(
        roof_params(PartKind::RoofHip, PartMaterial::Shingle, 12, 8, 8, 1));
    const float depth = 3.0f;
    const float w_tri = top_width(tri, rise);
    const float w_polu = top_width(polu, rise);
    // The triangle's top course is a sliver of the eaves width; the half-hip
    // keeps its authored 45% (plus course overlap slack). The PAIR is the
    // assertion: a hip that never narrowed would pass either single bound.
    CHECK(w_tri < depth * 0.45f);
    CHECK(w_polu > depth * 0.40f);
    CHECK(w_polu < depth * 0.75f);
    CHECK(w_tri < w_polu);
    CHECK(meshtest::half_edge_defects(meshtest::solid_of(tri)) == 0);
}

TEST_CASE("the smoke vent stays inside its base square and above it") {
    const RegistryObject obj = forge_part(
        roof_params(PartKind::SmokeVent, PartMaterial::Timber, 2, 1, 1));
    glm::vec3 lo{1e9f};
    glm::vec3 hi{-1e9f};
    for (const auto& v : meshtest::solid_of(obj).vertices) {
        lo = glm::min(lo, v.position);
        hi = glm::max(hi, v.position);
    }
    // Base side 0.5: the cap overhangs 25%, nothing more; origin at base
    // centre so a composer can set it astride a ridge by ridge coordinates.
    CHECK(lo.x > -0.40f);
    CHECK(hi.x < 0.40f);
    CHECK(lo.y > -0.05f);
    CHECK(hi.y > 0.5f);
}

TEST_CASE("roof names: the half-hip spells its variant, the vent its base") {
    CHECK(roof_params(PartKind::RoofHip, PartMaterial::Tile, 12, 8, 6, 1).name
          == "roofhip-tile-12x8x6-polu-w05");
    CHECK(roof_params(PartKind::RoofHip, PartMaterial::Tile, 12, 8, 6, 0).name
          == "roofhip-tile-12x8x6-w05");
    CHECK(roof_params(PartKind::SmokeVent, PartMaterial::TimberDark, 3, 1, 1).name
          == "smokevent-dark-3u-w05");
    // The shipped coverings keep their exact old names (509 references).
    CHECK(roof_params(PartKind::RoofSlope, PartMaterial::Thatch, 12, 8, 8).name
          == "roof-thatch-12x8x8-w05");
}

TEST_CASE("the catalogue carries the roof variants") {
    const auto cat = kit_catalogue();
    int new_slopes = 0;
    int hips = 0;
    int vents = 0;
    for (const auto& p : cat) {
        if (p.kind == PartKind::RoofSlope
            && (p.material == PartMaterial::Timber || p.material == PartMaterial::Tile
                || p.material == PartMaterial::Turf || p.height_u == 4)) {
            ++new_slopes;
        }
        if (p.kind == PartKind::RoofHip) ++hips;
        if (p.kind == PartKind::SmokeVent) ++vents;
    }
    // 36 покрытий на старых уклонах + 15 пологих (все пять покрытий);
    // вальмы 2 варианта x 3 покрытия x 3 глубины x 2 уклона; дымники 2x2.
    CHECK(new_slopes == 51);
    CHECK(hips == 36);
    CHECK(vents == 4);
}
