/*
Created: 16:08:2026 - 20:16:09
Last updated: 22:08:2026 - 20:10:00
Module: tests
File: tests/core/SceneTests.cpp

Responsibility:
- The composition tool's rules: that a .scene survives the round trip, and
  that every rule GOES RED on the defect it was written for.

Dependencies:
- Uses: doctest, dfn_world (Scene).
- Used by: ctest (test_scene).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- EVERY RULE IS TESTED FROM BOTH SIDES (Rule 30): a clean scene passes it and a
  planted defect fails it. A rule that cannot go red guards nothing, and this
  file is the only place that proves each one can. The defects planted here are
  the ones the tool was asked for: an object hovering over its ground, one
  reaching past the map edge, two standing inside each other, a name the
  registry does not have.
- The world here is a FLAT PLANE and two invented objects on purpose: the rules
  are what is under test, not the generator. The tool's own run against the
  real world is the acceptance, and it lives in the commit message.
*/
/*
UPD:
- 16:08:2026 - 20:16:09: Создан вместе с инструментом композиции.
- 16:08:2026 - 21:08:52: Правила групп с обеих сторон: деталь на детали законна, она же на 0.6 м
  выше — нет (и число названо), опора требует стоять НАД, а не рядом, опора не
  ходит между группами, без крюка object_top всё остаётся как было (правило 26),
  члены одной группы пересекаться могут — а контроль в разных группах ловится.
  Плюс врезка в склон: постройке можно, дереву нельзя, и висеть нельзя никому.
- 17:08:2026 - 03:09:30: спавн необязателен, и ОТСУТСТВИЕ читается как отсутствие: (0,0,0) по
  умолчанию, выглядящий как авторский выбор, телепортировал бы каждую карту без
  спавна в угол мира.
- 17:08:2026 - 10:53:33: лампы [light] в круговороте, и погашенная (радиус 0) возвращается
  погашенной: читатель, «услужливо» вернувший ей яркость, переспорил бы
  композитора.
- 17:08:2026 - 12:33:08: оба новых правила с обеих сторон: дерево НА тропе, дерево РЯДОМ с
  тропой (ловится кроной, а не стволом — иначе правило мерило бы начало),
  контроль вдали, контроль без поля троп; дерево в доме, деталь дома в своём же
  доме (не находка), контроль рядом, и то же самое в обратном порядке записи —
  правило не должно зависеть от того, кого поставили первым.
- 20:08:2026 - 15:30:00: [house] в круговороте; кривое pos — отказ со строкой.
- 22:08:2026 - 16:20:00: раунд-трип [air] + контроль «без записи air.set == false».
- 22:08:2026 - 20:10:00: cloud в раунд-трипе [air] + контроль «без ключа — не задана».
*/

#include "engine/world/sources/Scene.h"

#include <doctest/doctest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

using dfn::world::check_scene;
using dfn::world::Placement;
using dfn::world::SceneDoc;
using dfn::world::SceneFinding;
using dfn::world::SceneLimits;
using dfn::world::SceneRule;
using dfn::world::SceneWorld;
using dfn::world::describe;
using dfn::world::fix_scene_ground;
using dfn::world::read_scene;
using dfn::world::write_scene;

namespace {

constexpr float GROUND_Y = 10.0f;

float flat_ground(void*, glm::vec2) { return GROUND_Y; }

/// Two objects: a 3 m tree and a 20 m giant. Anything else is unknown, which
/// is what exercises the KnownObject rule.
bool two_objects(void*, const std::string& name, float& radius, float& bottom) {
    if (name == "tree") {
        radius = 3.0f;
        bottom = -0.5f;
        return true;
    }
    if (name == "giant") {
        radius = 20.0f;
        bottom = -1.0f;
        return true;
    }
    return false;
}

/// The kit half of the fixture: a 4 m beam, 1 m wide, 2 m tall. Height matters
/// here in a way it never did for the tree rules — it is what the NEXT part
/// rests on.
bool object_top(void*, const std::string& name, float& top) {
    if (name == "tree") {
        top = 8.0f;
        return true;
    }
    if (name == "giant") {
        top = 40.0f;
        return true;
    }
    if (name == "beam") {
        top = 2.0f;
        return true;
    }
    return false;
}

bool three_objects(void* ctx, const std::string& name, float& radius, float& bottom) {
    if (name == "beam") {
        radius = 2.0f;
        bottom = 0.0f;
        return true;
    }
    return two_objects(ctx, name, radius, bottom);
}

[[nodiscard]] SceneWorld test_world() {
    SceneWorld w;
    w.ground_at = &flat_ground;
    w.object_extent = &three_objects;
    w.object_top = &object_top;
    return w;
}

/// The world as it was BEFORE parts could rest on parts. Kept so the stacking
/// tests can prove the old behaviour is what you still get without the new
/// hook — a contract that grew, not one that changed (Rule 26).
[[nodiscard]] SceneWorld world_without_tops() {
    SceneWorld w = test_world();
    w.object_top = nullptr;
    return w;
}

[[nodiscard]] Placement at(std::string object, float x, float y, float z) {
    Placement p;
    p.object = std::move(object);
    p.position = {x, y, z};
    return p;
}

[[nodiscard]] bool has(const std::vector<SceneFinding>& found, SceneRule rule) {
    return std::any_of(found.begin(), found.end(),
                       [rule](const SceneFinding& f) { return f.rule == rule; });
}

} // namespace

TEST_CASE("scene: a clean composition produces no findings") {
    SceneDoc doc;
    doc.map = "trees/gallery";
    doc.world_span_m = 256.0f;
    doc.placements = {at("tree", 60.0f, GROUND_Y, 60.0f),
                      at("tree", 90.0f, GROUND_Y, 60.0f),
                      at("giant", 160.0f, GROUND_Y, 160.0f)};
    const auto found = check_scene(doc, test_world());
    for (const SceneFinding& f : found) {
        INFO(describe(f));
    }
    CHECK(found.empty());
}

TEST_CASE("scene: every rule goes red on the defect it was written for") {
    const SceneWorld world = test_world();

    SUBCASE("a hovering object is caught, and by how much") {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements = {at("tree", 60.0f, GROUND_Y + 0.42f, 60.0f)};
        const auto found = check_scene(doc, world);
        REQUIRE(found.size() == 1);
        CHECK(found[0].rule == SceneRule::OnGround);
        // The NUMBER is the point: "hovers" is an opinion, "hovers by 0.42 m"
        // is something a composer can act on.
        CHECK(found[0].amount_m == doctest::Approx(0.42f));
    }

    SUBCASE("a buried object is caught with the opposite sign") {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements = {at("tree", 60.0f, GROUND_Y - 1.5f, 60.0f)};
        const auto found = check_scene(doc, world);
        REQUIRE(found.size() == 1);
        CHECK(found[0].rule == SceneRule::OnGround);
        CHECK(found[0].amount_m == doctest::Approx(-1.5f));
    }

    SUBCASE("the bounds rule measures the OBJECT, not its origin") {
        // The giant's ORIGIN is comfortably inside a 256 m map; its 20 m
        // footprint is not. An origin-only rule would pass this, which is the
        // whole reason the extent is measured.
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements = {at("giant", 245.0f, GROUND_Y, 128.0f)};
        const auto found = check_scene(doc, world);
        CHECK(has(found, SceneRule::InsideBounds));

        // CONTROL: the same object, moved in by its own radius, is legal —
        // so the rule is not simply refusing everything near an edge.
        doc.placements = {at("giant", 128.0f, GROUND_Y, 128.0f)};
        CHECK_FALSE(has(check_scene(doc, world), SceneRule::InsideBounds));
    }

    SUBCASE("two trunks in one hole are caught; mingling crowns are not") {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements = {at("tree", 100.0f, GROUND_Y, 100.0f),
                          at("tree", 101.0f, GROUND_Y, 100.0f)};
        CHECK(has(check_scene(doc, world), SceneRule::NoOverlap));

        // CONTROL: 5.6 m apart, two 3 m radii, half a metre of slack — crowns
        // touch and that is a wood, not a defect.
        doc.placements = {at("tree", 100.0f, GROUND_Y, 100.0f),
                          at("tree", 105.6f, GROUND_Y, 100.0f)};
        CHECK_FALSE(has(check_scene(doc, world), SceneRule::NoOverlap));
    }

    SUBCASE("a name the registry does not have is caught") {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements = {at("no-such-object", 60.0f, GROUND_Y, 60.0f)};
        const auto found = check_scene(doc, world);
        REQUIRE(found.size() == 1);
        CHECK(found[0].rule == SceneRule::KnownObject);
    }
}

TEST_CASE("scene: a built thing stands on itself") {
    // The house case the kit exists for: a beam on the ground, a second beam
    // resting on the first. Both are members of one group.
    const auto beam = [](float x, float y, const char* group) {
        Placement p = at("beam", x, y, 100.0f);
        p.group = group;
        return p;
    };

    SUBCASE("a part resting on another part is legal") {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements = {beam(100.0f, GROUND_Y, "house"),
                          beam(100.0f, GROUND_Y + 2.0f, "house")};
        const auto found = check_scene(doc, test_world());
        for (const SceneFinding& f : found) {
            INFO(describe(f));
        }
        CHECK(found.empty());
    }

    SUBCASE("...and one resting on NOTHING is still caught, by how much") {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements = {beam(100.0f, GROUND_Y, "house"),
                          beam(100.0f, GROUND_Y + 2.6f, "house")};
        const auto found = check_scene(doc, test_world());
        REQUIRE(found.size() == 1);
        CHECK(found[0].rule == SceneRule::OnGround);
        CHECK(found[0].amount_m == doctest::Approx(0.6f)); // above the 2 m beam
    }

    SUBCASE("support needs the parts to be OVER each other, not merely near") {
        // Same heights, but 20 m apart: nothing holds the upper beam up. This
        // is the rule the whole feature is for — «висящие в воздухе тропинки».
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements = {beam(100.0f, GROUND_Y, "house"),
                          beam(120.0f, GROUND_Y + 2.0f, "house")};
        CHECK(has(check_scene(doc, test_world()), SceneRule::OnGround));
    }

    SUBCASE("a building may be dug into its slope; a tree may not") {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        // A footing 0.3 m into the hill: how a wall meets a slope.
        doc.placements = {beam(100.0f, GROUND_Y - 0.3f, "house")};
        CHECK_FALSE(has(check_scene(doc, test_world()), SceneRule::OnGround));
        // CONTROL 1: the same beam standing loose is a beam sunk in the mud.
        doc.placements[0].group.clear();
        CHECK(has(check_scene(doc, test_world()), SceneRule::OnGround));
        // CONTROL 2: the licence is for burying only. A house part hovering by
        // the same 0.3 m is still a defect, because nothing holds it up.
        doc.placements = {beam(100.0f, GROUND_Y + 0.3f, "house")};
        CHECK(has(check_scene(doc, test_world()), SceneRule::OnGround));
    }

    SUBCASE("support does not cross group boundaries") {
        // Two different buildings: one may not lean on the other's roof by
        // accident, or a composer would never learn that he mis-typed a group.
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements = {beam(100.0f, GROUND_Y, "barn"),
                          beam(100.0f, GROUND_Y + 2.0f, "house")};
        CHECK(has(check_scene(doc, test_world()), SceneRule::OnGround));
    }

    SUBCASE("without the object_top hook, nothing rests on anything") {
        // The old contract, unchanged: a caller that never supplied heights
        // gets exactly the behaviour it had before groups existed.
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements = {beam(100.0f, GROUND_Y, "house"),
                          beam(100.0f, GROUND_Y + 2.0f, "house")};
        CHECK(has(check_scene(doc, world_without_tops()), SceneRule::OnGround));
    }

    SUBCASE("members of one group may interpenetrate; two buildings may not") {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements = {beam(100.0f, GROUND_Y, "house"),
                          beam(100.5f, GROUND_Y, "house")};
        CHECK_FALSE(has(check_scene(doc, test_world()), SceneRule::NoOverlap));

        // CONTROL: the same two beams in DIFFERENT groups are two things in
        // one place, which is the defect the rule was written for.
        doc.placements[1].group = "barn";
        CHECK(has(check_scene(doc, test_world()), SceneRule::NoOverlap));
    }
}

namespace {
/// A single straight path down x = 100: worn half-width 1 m, so the trodden
/// surface is 99..101 and the clearance is measured outward from its edge.
bool one_path(void*, glm::vec2 p, float& metres) {
    metres = std::fabs(p.x - 100.0f) - 1.0f;
    return true;
}
} // namespace

TEST_CASE("scene: nothing stands on a path") {
    SceneWorld w = test_world();
    w.path_clearance = &one_path;

    SceneDoc doc;
    doc.world_span_m = 256.0f;

    SUBCASE("a tree ON the tread is caught") {
        doc.placements = {at("tree", 100.0f, GROUND_Y, 60.0f)};
        const auto found = check_scene(doc, w);
        CHECK(has(found, SceneRule::OffPath));
    }

    SUBCASE("a tree BESIDE the tread is caught by its CROWN, not its trunk") {
        // Trunk at x = 103, two metres clear of the worn edge at 101 — but the
        // tree's footprint is 3 m of radius, so its crown is over the road.
        // Measuring the origin alone would pass this, which is the whole
        // reason the rule probes the footprint.
        doc.placements = {at("tree", 103.0f, GROUND_Y, 60.0f)};
        CHECK(has(check_scene(doc, w), SceneRule::OffPath));
    }

    SUBCASE("CONTROL: the same tree far enough away is legal") {
        doc.placements = {at("tree", 110.0f, GROUND_Y, 60.0f)};
        CHECK_FALSE(has(check_scene(doc, w), SceneRule::OffPath));
    }

    SUBCASE("CONTROL: with no path field the rule cannot fire at all") {
        doc.placements = {at("tree", 100.0f, GROUND_Y, 60.0f)};
        CHECK_FALSE(has(check_scene(doc, test_world()), SceneRule::OffPath));
    }
}

TEST_CASE("scene: nothing stands inside a building it is not part of") {
    SceneWorld w = test_world();
    SceneDoc doc;
    doc.world_span_m = 256.0f;

    const auto beam_at = [](float x, float z, const char* group) {
        Placement p = at("beam", x, GROUND_Y, z);
        p.group = group;
        return p;
    };

    SUBCASE("a tree in the middle of the house is caught") {
        // The user's own pair of cases, and they are ONE rule: «нельзя дерево в
        // доме ставить / дом поверх дерева ставить».
        doc.placements = {beam_at(100.0f, 100.0f, "house"),
                          beam_at(100.0f, 102.0f, "house"),
                          at("tree", 100.5f, GROUND_Y, 101.0f)};
        const auto found = check_scene(doc, w);
        CHECK(has(found, SceneRule::OutsideBuildings));
    }

    SUBCASE("a member of the house is NOT intruding on its own house") {
        doc.placements = {beam_at(100.0f, 100.0f, "house"),
                          beam_at(100.5f, 100.0f, "house")};
        CHECK_FALSE(has(check_scene(doc, w), SceneRule::OutsideBuildings));
    }

    SUBCASE("CONTROL: a tree beside the house is legal") {
        doc.placements = {beam_at(100.0f, 100.0f, "house"),
                          at("tree", 112.0f, GROUND_Y, 100.0f)};
        CHECK_FALSE(has(check_scene(doc, w), SceneRule::OutsideBuildings));
    }

    SUBCASE("and it reads the other way round: a house over a tree") {
        // Same geometry, opposite order of authoring — the finding must still
        // appear, or the rule would depend on who was placed first.
        doc.placements = {at("tree", 100.5f, GROUND_Y, 101.0f),
                          beam_at(100.0f, 100.0f, "house"),
                          beam_at(100.0f, 102.0f, "house")};
        CHECK(has(check_scene(doc, w), SceneRule::OutsideBuildings));
    }
}

TEST_CASE("scene: --fix never touches a built thing") {
    // A house's upper storey is hovering BY DESIGN as far as the terrain is
    // concerned. A fixer that sat it down would demolish the house.
    SceneDoc doc;
    doc.world_span_m = 256.0f;
    doc.placements = {at("tree", 60.0f, GROUND_Y + 3.0f, 60.0f),
                      at("beam", 100.0f, GROUND_Y + 2.0f, 100.0f)};
    doc.placements[1].group = "house";
    const std::size_t moved = fix_scene_ground(doc, test_world());
    CHECK(moved == 1); // the loose tree, and only it
    CHECK(doc.placements[0].position.y == doctest::Approx(GROUND_Y));
    CHECK(doc.placements[1].position.y == doctest::Approx(GROUND_Y + 2.0f));
}

TEST_CASE("scene: --fix sits objects down and changes nothing else") {
    SceneDoc doc;
    doc.world_span_m = 256.0f;
    doc.placements = {at("tree", 60.0f, GROUND_Y + 3.0f, 60.0f),
                      at("tree", 90.0f, GROUND_Y, 90.0f)};
    doc.placements[0].yaw = 1.25f;
    doc.placements[0].note = "the one the user asked about";

    const std::size_t moved = fix_scene_ground(doc, test_world());
    CHECK(moved == 1); // the one that was wrong, and only it
    CHECK(doc.placements[0].position.y == doctest::Approx(GROUND_Y));
    // Everything a composer authored survives the repair untouched: a fixer
    // that quietly re-yawed or dropped a note would be worse than the hover.
    CHECK(doc.placements[0].yaw == doctest::Approx(1.25f));
    CHECK(doc.placements[0].note == "the one the user asked about");
    CHECK(doc.placements[0].position.x == doctest::Approx(60.0f));
    CHECK(check_scene(doc, test_world()).empty());
}

TEST_CASE("scene: the spawn is optional, and absent means absent") {
    const auto path = std::filesystem::temp_directory_path() / "dfn_scene_spawn.scene";
    SceneDoc doc;
    doc.map = "trees/glade";
    doc.world_span_m = 256.0f;
    doc.placements = {at("tree", 60.0f, GROUND_Y, 60.0f)};

    // A scene that says nothing about the spawn must READ BACK saying nothing:
    // a default-constructed (0,0,0) that looked like an authored choice would
    // teleport every map without one into the corner of the world.
    REQUIRE(write_scene(doc, path));
    SceneDoc back;
    std::string error;
    REQUIRE(read_scene(path, back, error));
    CHECK_FALSE(back.has_spawn);

    doc.has_spawn = true;
    doc.spawn = {56.0f, 0.0f, 157.0f};
    doc.spawn_yaw = 1.19f;
    REQUIRE(write_scene(doc, path));
    SceneDoc with;
    REQUIRE(read_scene(path, with, error));
    REQUIRE(with.has_spawn);
    CHECK(with.spawn.x == doctest::Approx(56.0f));
    CHECK(with.spawn.z == doctest::Approx(157.0f));
    CHECK(with.spawn_yaw == doctest::Approx(1.19f));

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("scene: lamps survive the round trip and an unlit one stays unlit") {
    const auto path = std::filesystem::temp_directory_path() / "dfn_scene_light.scene";
    SceneDoc doc;
    doc.map = "trees/glade";
    doc.world_span_m = 256.0f;
    doc.placements = {at("tree", 60.0f, GROUND_Y, 60.0f)};
    doc.lights.push_back({{128.0f, 27.4f, 96.0f}, {1.0f, 0.85f, 0.55f}, 6.0f, false,
                          "фонарь у каменной тропы"});
    // radius 0 means OFF, and it must come back as off rather than as a
    // default-bright lamp: a composer turning one down to nothing is making a
    // decision, and a reader that "helpfully" restored it would overrule him.
    doc.lights.push_back({{10.0f, 1.0f, 10.0f}, {1.0f, 1.0f, 1.0f}, 0.0f, false, ""});
    doc.lights.push_back({{20.0f, 2.0f, 20.0f}, {0.4f, 0.9f, 1.0f}, 12.0f, true, ""});
    REQUIRE(write_scene(doc, path));

    SceneDoc back;
    std::string error;
    REQUIRE(read_scene(path, back, error));
    REQUIRE(back.lights.size() == 3);
    CHECK(back.lights[0].position.y == doctest::Approx(27.4f));
    CHECK(back.lights[0].color.g == doctest::Approx(0.85f));
    CHECK(back.lights[0].radius_m == doctest::Approx(6.0f));
    CHECK_FALSE(back.lights[0].casts_shadow);
    CHECK(back.lights[0].note == "фонарь у каменной тропы");
    CHECK(back.lights[1].radius_m == doctest::Approx(0.0f));
    CHECK(back.lights[2].casts_shadow);
    // The placements are untouched by the new section: a reader that lost them
    // while gaining lamps would be a very expensive trade.
    REQUIRE(back.placements.size() == 1);
    CHECK(back.placements[0].object == "tree");

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("scene: [air] survives the round trip and its absence means absence") {
    const auto path = std::filesystem::temp_directory_path() / "dfn_scene_air.scene";
    SceneDoc doc;
    doc.map = "houses/whiterun";
    doc.world_span_m = 256.0f;
    doc.air.set = true;
    doc.air.fog_start_m = 110.0f;
    doc.air.fog_end_m = 380.0f;
    doc.air.cloud_cover = 0.25f;
    REQUIRE(write_scene(doc, path));

    SceneDoc back;
    std::string error;
    REQUIRE(read_scene(path, back, error));
    REQUIRE(back.air.set);
    CHECK(back.air.fog_start_m == doctest::Approx(110.0f));
    CHECK(back.air.fog_end_m == doctest::Approx(380.0f));
    CHECK(back.air.cloud_cover == doctest::Approx(0.25f));

    // Облачность необязательна и внутри [air]: без ключа `cloud` возвращается
    // «не задана» (-1), а не нулевое чистое небо.
    doc.air.cloud_cover = -1.0f;
    REQUIRE(write_scene(doc, path));
    SceneDoc no_cloud;
    REQUIRE(read_scene(path, no_cloud, error));
    REQUIRE(no_cloud.air.set);
    CHECK(no_cloud.air.cloud_cover < 0.0f);

    // Контрольная рука: сцена БЕЗ [air] обязана вернуться с air.set == false —
    // карта без записи живёт на глобальных константах, а не на воздухе
    // предыдущей карты или на нулевом тумане.
    doc.air = {};
    REQUIRE(write_scene(doc, path));
    SceneDoc plain;
    REQUIRE(read_scene(path, plain, error));
    CHECK_FALSE(plain.air.set);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("scene: the file survives the round trip, notes and all") {
    const auto path = std::filesystem::temp_directory_path() / "dfn_scene_roundtrip.scene";
    SceneDoc doc;
    doc.map = "trees/gallery";
    doc.world_span_m = 256.0f;
    doc.placements = {at("tree", 12.5f, 3.25f, -7.75f), at("giant", 100.0f, 0.0f, 100.0f)};
    doc.placements[0].yaw = 0.75f;
    doc.placements[0].scale = 1.4f;
    doc.placements[0].note = "why it stands here";
    REQUIRE(write_scene(doc, path));

    SceneDoc back;
    std::string error;
    REQUIRE(read_scene(path, back, error));
    CHECK(back.map == doc.map);
    CHECK(back.world_span_m == doctest::Approx(doc.world_span_m));
    REQUIRE(back.placements.size() == doc.placements.size());
    CHECK(back.placements[0].object == "tree");
    CHECK(back.placements[0].position.x == doctest::Approx(12.5f));
    CHECK(back.placements[0].position.z == doctest::Approx(-7.75f));
    CHECK(back.placements[0].yaw == doctest::Approx(0.75f));
    CHECK(back.placements[0].scale == doctest::Approx(1.4f));
    CHECK(back.placements[0].note == "why it stands here");

    // A MALFORMED NUMBER IS AN ERROR WITH A LINE, never a silent zero: an
    // object quietly placed at the origin looks like a composition decision.
    {
        std::ofstream f(path, std::ios::trunc);
        f << "map = x\n[place]\nobject = tree\nyaw = north\n";
    }
    SceneDoc bad;
    std::string bad_error;
    CHECK_FALSE(read_scene(path, bad, bad_error));
    CHECK(bad_error.find("line 4") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("scene: [house] survives the round trip and a wrong pos is refused") {
    // Готовые постройки (20.08): секция [house] — регистрация домов кузницы.
    const auto path = std::filesystem::temp_directory_path() / "dfn_scene_house.scene";
    SceneDoc doc;
    doc.map = "houses/build";
    doc.world_span_m = 256.0f;
    dfn::world::ScenePlacedHouse h;
    h.file = "assets/houses/u-house.dfh";
    h.position = {90.0f, 25.5f, 100.0f};
    h.yaw = 1.57f;
    h.note = "П-образный на южной полке";
    doc.houses.push_back(h);
    REQUIRE(write_scene(doc, path));

    SceneDoc back;
    std::string error;
    REQUIRE(read_scene(path, back, error));
    REQUIRE(back.houses.size() == 1);
    CHECK(back.houses[0].file == h.file);
    CHECK(back.houses[0].position.x == doctest::Approx(90.0f));
    CHECK(back.houses[0].position.y == doctest::Approx(25.5f));
    CHECK(back.houses[0].position.z == doctest::Approx(100.0f));
    CHECK(back.houses[0].yaw == doctest::Approx(1.57f));
    CHECK(back.houses[0].note == h.note);

    // Кривое pos — отказ со строкой, не молчаливый ноль (контрольное плечо).
    {
        std::ofstream f(path, std::ios::trunc);
        f << "map = x\n[house]\nfile = a.dfh\npos = there\n";
    }
    SceneDoc bad;
    std::string bad_error;
    CHECK_FALSE(read_scene(path, bad, bad_error));
    CHECK(bad_error.find("line 4") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}
