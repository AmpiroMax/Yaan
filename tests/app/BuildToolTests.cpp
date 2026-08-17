/*
Created: 17:08:2026 - 19:20:00
Last updated: 17:08:2026 - 19:20:04
Module: tests/app
File: tests/app/BuildToolTests.cpp

Responsibility:
- Holds the build hand to its one promise: the colour the builder sees is the
  JUDGE'S answer about HIS ghost, and nobody else's.

Dependencies:
- Uses: engine/app/sources/BuildTool, engine/world (Scene), doctest.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE ARMS DIFFER BY ONE THING. A test where the allowed and the refused case
  differ in two ways proves neither (Rule 30).
*/
/*
UPD:
- 17:08:2026 - 19:20:00: Создан — зелёное/красное руки строителя.
- 17:08:2026 - 19:20:04: граница обхода перечисления — ПОСЛЕДНЕЕ значение, а не названное. Обход,
  кончавшийся на предпоследнем, пропустил два новых правила судьи.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/BuildTool.h"

using namespace dfn;
using namespace dfn::app;

TEST_CASE("the ghost is coloured by ITS OWN findings, not the scene's") {
    // THE DEFECT THIS GUARDS AGAINST is the one that makes a build tool
    // unusable rather than merely wrong: a composition with an old problem
    // three houses away paints every new ghost red, and the builder can place
    // nothing until somebody cleans a map he did not come to clean.
    std::vector<world::SceneFinding> findings;
    world::SceneFinding other;
    other.rule = world::SceneRule::OffPath;
    other.placement_index = 7; // somebody else's problem
    other.object = "tree-oak";
    findings.push_back(other);

    const BuildVerdict clean = verdict_from_findings(findings, /*candidate=*/12);
    CHECK(clean.allowed);
    CHECK(clean.reason.empty());

    // ONE THING CHANGES: the finding now names the ghost.
    findings[0].placement_index = 12;
    const BuildVerdict red = verdict_from_findings(findings, 12);
    CHECK_FALSE(red.allowed);
    CHECK(red.reason == "build.no.path");
}

TEST_CASE("every rule the judge can raise has a sentence for the builder") {
    // A verdict with no reason is a red square with no explanation, which
    // teaches the builder to move the mouse until it goes green — the exact
    // opposite of what the colour is for. Walking the enum here means a rule
    // added to the judge without a sentence fails in CI, not on his screen.
        // THE BOUND IS THE LAST VALUE, NOT A NAMED ONE. Written as RoofSeat, this
    // loop stopped one short the day the judge grew StairSeat and StairHeadroom
    // — and an enum walk that ends before the end is exactly the instrument
    // that cannot fail. The two new rules fell through to "build.no.other" and
    // the ghost went red WITHOUT A REASON on every stair, which is the failure
    // this case exists to prevent. Found by the palette agent, not by this test.
    constexpr uint8_t LAST = static_cast<uint8_t>(world::SceneRule::StairHeadroom);
    for (uint8_t r = 0; r <= LAST; ++r) {
        std::vector<world::SceneFinding> f(1);
        f[0].rule = static_cast<world::SceneRule>(r);
        f[0].placement_index = 0;
        const BuildVerdict v = verdict_from_findings(f, 0);
        CHECK_FALSE(v.allowed);
        REQUIRE_FALSE(v.reason.empty());
        // Not the fallback: every rule must be named, not swept into "other".
        CHECK(v.reason != "build.no.other");
    }
}

TEST_CASE("the grid catches x and z and leaves the ground alone") {
    const glm::vec3 snapped = snap_to_grid({12.34f, 25.51f, -7.60f});
    CHECK(snapped.x == doctest::Approx(12.25f));
    CHECK(snapped.z == doctest::Approx(-7.50f));
    // HEIGHT IS NOT SNAPPED, and this is the arm that says so out loud: a part
    // pushed to the nearest 25 cm would hover or sink on sloping ground, and
    // the judge would then call the builder's own tool wrong.
    CHECK(snapped.y == doctest::Approx(25.51f));
}

TEST_CASE("the palette comes from the shelf, and a missing shelf is empty, not a crash") {
    // A shelf that is not there yet (first run, before the bake) must give an
    // empty menu rather than take the editor down with it.
    const std::vector<BuildGroup> none = build_palette("assets/objects/does-not-exist");
    CHECK(none.empty());

    const std::vector<BuildGroup> parts = build_palette("assets/objects/parts");
    if (parts.empty()) {
        MESSAGE("полка деталей не испечена — пропускаю рукав каталога");
        return;
    }
    // Grouped by the family the kit spells into the name, and the far LOD form
    // is not a second thing to place.
    bool saw_wall = false;
    for (const BuildGroup& g : parts) {
        CHECK_FALSE(g.names.empty());
        if (g.title == "wall") {
            saw_wall = true;
        }
        for (const std::string& n : g.names) {
            CHECK(n.find("-far") == std::string::npos);
            CHECK(n.rfind(g.title, 0) == 0); // the family really is the prefix
        }
    }
    CHECK(saw_wall);
}
