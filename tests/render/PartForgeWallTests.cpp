/*
Created: 17:08:2026 - 13:09:29
Last updated: 17:08:2026 - 13:54:03
Module: tests
File: tests/render/PartForgeWallTests.cpp

Responsibility:
- The wall variants' contract: every style at every opening is SOLID to the
  panel instrument (no daylight straight through) — except the door opening,
  where daylight is the DESIGN and its absence would mean a walled-up door.
  Plus determinism and the name grammar the judge and composer parse.

Key items:
- solidity sweep over style x opening via check_panel_solid (the ONE panel
  instrument, same as dfn_assemble --require-solid and the judge's --solid).

Dependencies:
- Uses: engine/render (PartForge), engine/world (check_panel_solid), doctest.
- Used by: ctest (render_part_forge_walls).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 30: the door case IS the control of the solidity sweep — the same
  instrument that must read 0 on nine styles must read >0 there, or it is
  not measuring daylight at all.
*/
/*
UPD:
- 17:08:2026 - 13:09:29: Создан — волна вариантов стен.
- 17:08:2026 - 13:54:03: счёт стилевых панелей 174 -> 696: те же 174 конструкции на четырёх
  высотах ряда 11/12/13/14u (HOUSES.md §6).
*/

#include "engine/render/sources/PartForge.h"
#include "engine/world/sources/Scene.h"

#include <doctest/doctest.h>
#include <string>
#include <vector>

using namespace dfn::render;

namespace {

PartParams wall_params(int variant, int opening, PartMaterial mat) {
    PartParams p;
    p.kind = PartKind::WallPanel;
    p.material = mat;
    p.variant = variant;
    p.opening = opening;
    p.length_u = 16;
    p.width_u = 1;
    p.height_u = 11;
    p.wear = 0.5f;
    p.name = part_name(p);
    p.seed = 777;
    return p;
}

dfn::world::SolidReport solidity(const RegistryObject& obj) {
    std::vector<glm::vec3> pos;
    pos.reserve(obj.wood.vertices.size());
    for (const auto& v : obj.wood.vertices) {
        pos.push_back(v.position);
    }
    // 2 cm grid: the openings under test are metre-scale, and the full-kit
    // 1 cm sweep over ten styles is a minute of test time nobody rereads.
    return dfn::world::check_panel_solid(pos, obj.wood.indices, 0.02f);
}

PartMaterial default_mat(int variant) {
    switch (variant) {
    case 2:
    case 3:
    case 4: return PartMaterial::Plaster;
    case 7:
    case 8:
    case 10: return PartMaterial::Stone;
    case 9: return PartMaterial::Brick;
    default: return PartMaterial::Timber;
    }
}

} // namespace

TEST_CASE("every wall style is solid blind and with windows; the door leaks by design") {
    for (int variant = 1; variant <= 10; ++variant) {
        const PartMaterial mat = default_mat(variant);
        for (const int opening : {0, 1, 3}) {
            const RegistryObject obj = forge_part(wall_params(variant, opening, mat));
            REQUIRE(!obj.wood.indices.empty());
            const auto r = solidity(obj);
            CAPTURE(variant);
            CAPTURE(opening);
            if (opening == 3) {
                // The door hole is the sweep's own control (Rule 30): the
                // instrument that reads 0 above must read daylight here.
                CHECK(r.rays_through > 0);
            } else {
                CHECK(r.rays_through == 0);
            }
        }
    }
}

TEST_CASE("window panes are sealed inserts, not holes (два окна тоже)") {
    const RegistryObject obj = forge_part(wall_params(9, 2, PartMaterial::Brick));
    const auto r = solidity(obj);
    CHECK(r.rays_through == 0);
}

TEST_CASE("styled wall names spell construction and opening for the shelf") {
    CHECK(wall_params(3, 2, PartMaterial::Clay).name == "wall-framex-clay-16x1x11-win2-w05");
    CHECK(wall_params(1, 0, PartMaterial::Timber).name == "wall-log-timber-16x1x11-blind-w05");
    CHECK(wall_params(10, 3, PartMaterial::Stone).name == "wall-combo-stone-16x1x11-door-w05");
    // The legacy plain bay keeps its exact old name: 508 shipped parts and
    // the farmhouse scene reference it by these strings.
    PartParams legacy;
    legacy.kind = PartKind::WallPanel;
    legacy.material = PartMaterial::Timber;
    legacy.length_u = 8;
    legacy.width_u = 1;
    legacy.height_u = 10;
    legacy.wear = 0.8f;
    CHECK(part_name(legacy) == "wall-timber-8x1x10-w08");
}

TEST_CASE("determinism: same styled params, same bytes") {
    const PartParams p = wall_params(7, 1, PartMaterial::Stone);
    CHECK(object_content_hash(forge_part(p)) == object_content_hash(forge_part(p)));
}

TEST_CASE("the catalogue carries the ten styles and stays name-unique") {
    const auto cat = kit_catalogue();
    int styled = 0;
    for (const auto& p : cat) {
        if (p.kind == PartKind::WallPanel && p.variant != 0) {
            ++styled;
        }
    }
    // (18 сруб + 72 фахверк + 48 обшивка + 9 тёсаный + 6 бут + 12 кирпич +
    // 9 комбо = 174 конструкции, §7 п.4) x 4 высоты стены (11 legacy +
    // ряд жилья 12/13/14, HOUSES.md §6).
    CHECK(styled == 696);
}
