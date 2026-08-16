/*
Created: 16:08:2026 - 20:16:09
Last updated: 16:08:2026 - 20:16:09
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

[[nodiscard]] SceneWorld test_world() {
    SceneWorld w;
    w.ground_at = &flat_ground;
    w.object_extent = &two_objects;
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
