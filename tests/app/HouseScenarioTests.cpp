/*
Created: 17:08:2026 - 22:01:29
Last updated: 17:08:2026 - 22:01:29
Module: tests/app
File: tests/app/HouseScenarioTests.cpp

Responsibility:
- THE SLEEVE THAT BUILDS A HOUSE. It replays the demo's own houses through the
  BUILD HAND'S JUDGE, one part at a time, and reports the verdict at every
  step — which is the question the user actually asked (17.08.2026): «сделай
  сценарий, где дом строится в редакторе, как те, что в демке, убедись, что
  список правил полный и позволяет это сделать».

Dependencies:
- Uses: engine/world (read_scene, check_scene — the judge itself, never a
  second copy of the rules), engine/render (measure_object — the same ruler the
  ghost and the panel use), engine/app BuildTool (verdict_from_findings — the
  same translation the builder reads), doctest.
- Used by: ctest (app_house_scenario).

WHY A SLEEVE AND NOT A SCREENSHOT. A frame of a finished house proves that a
house EXISTS; it says nothing about whether a person could have built it. The
question is about the SEQUENCE — whether every intermediate state is one the
judge allows — and an intermediate state is exactly what a photograph of the
result cannot contain.

THE GROUND MODEL IS CONTROLLED, NOT ASSUMED. The demo stands on a [pad] at
25.5 m, so the ground under all three houses is that constant. The first case
below checks the finished house against it and requires ZERO findings — the
same number tools/check_scene reports for the whole map. A sleeve whose ground
disagreed with the map's would refuse legal steps for a reason that is about
the sleeve, and every later number would be noise.

WHAT IT FOUND, and it is worth more than the tool it was written to test: the
ROOF CANNOT BE BUILT ONE PART AT A TIME. Ridge, slope and gable form a CYCLE —
the ridge is refused by OnGround until the gables carry it, and the gables are
refused by RoofSeat until the ridge carries them. The rules are consistent for
a FINISHED scene and unsatisfiable for a growing one. That is held here as a
named expectation rather than fixed in silence (see the third case).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NEVER answer "allowed" from a rule written here. Every verdict in this file
  comes out of world::check_scene, exactly as it does in the editor.
- IF THE THIRD CASE GOES RED, THE FINDING WAS FIXED. Do not "repair" the test
  to match: read the note above it, confirm the roof now builds part by part,
  and write the new number down.
*/
/*
UPD:
- 17:08:2026 - 22:01:29: Создан — рукав постройки дома БЕЗ ОКНА (заказ 17.08 п.3).
  Держит три числа: дом демки судится начисто (контроль модели земли), БЕЗ
  ГРУППЫ рука получает 21 отказ из 46 (поэтому у руки появилось поле
  постройки), и с группой по одной детали встают 38 из 46 — остальные восемь
  это КОЛЬЦО конёк-фронтон, находка, а не недоделка.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/BuildTool.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/world/sources/Scene.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace dfn;

namespace {

// THE MAP'S OWN SHELVES AND THE MAP'S OWN GROUND. Both are read from what the
// map declares (assets/maps/houses/demo.map: objects = parts;signs, stand
// Gallery with a [pad] at 25.5), never guessed — a sleeve judging a different
// world than the game does is a sleeve measuring itself.
constexpr float PAD_HEIGHT_M = 25.5f;

struct Shelf {
    std::vector<std::filesystem::path> dirs;
    std::map<std::string, render::ObjectExtent> extents;
};

const render::ObjectExtent* extent_of(void* ctx, const std::string& name) {
    auto* s = static_cast<Shelf*>(ctx);
    if (const auto it = s->extents.find(name); it != s->extents.end()) {
        return &it->second;
    }
    for (const auto& dir : s->dirs) {
        if (auto obj = render::read_object(dir / (name + ".dfo"))) {
            return &s->extents.emplace(name, render::measure_object(*obj)).first->second;
        }
    }
    return nullptr;
}

float ground_at(void*, glm::vec2) { return PAD_HEIGHT_M; }

bool object_extent(void* c, const std::string& n, float& radius, float& bottom) {
    const render::ObjectExtent* e = extent_of(c, n);
    if (e == nullptr) {
        return false;
    }
    radius = e->radius;
    bottom = e->bottom;
    return true;
}
bool object_top(void* c, const std::string& n, float& top) {
    const render::ObjectExtent* e = extent_of(c, n);
    if (e == nullptr) {
        return false;
    }
    top = e->top;
    return true;
}
bool object_box(void* c, const std::string& n, glm::vec2& lo, glm::vec2& hi) {
    const render::ObjectExtent* e = extent_of(c, n);
    if (e == nullptr) {
        return false;
    }
    lo = e->lo;
    hi = e->hi;
    return true;
}
bool object_box_solid(void* c, const std::string& n, glm::vec2& lo, glm::vec2& hi) {
    const render::ObjectExtent* e = extent_of(c, n);
    if (e == nullptr) {
        return false;
    }
    lo = e->slo;
    hi = e->shi;
    return true;
}
bool object_solid(void* c, const std::string& n) {
    const render::ObjectExtent* e = extent_of(c, n);
    return e == nullptr || e->solid;
}

struct Bench {
    world::SceneDoc source;
    Shelf shelf;
    world::SceneWorld world;

    Bench() {
        std::string error;
        REQUIRE_MESSAGE(world::read_scene("assets/scenes/demo.scene", source, error),
                        error);
        shelf.dirs.emplace_back("assets/objects/parts");
        shelf.dirs.emplace_back("assets/objects/signs");
        world.ground_at = &ground_at;
        world.object_extent = &object_extent;
        world.object_top = &object_top;
        world.object_box = &object_box;
        world.object_box_solid = &object_box_solid;
        world.object_solid = &object_solid;
        world.ctx = &shelf;
    }

    [[nodiscard]] std::vector<world::Placement> group(const std::string& name) const {
        std::vector<world::Placement> out;
        for (const world::Placement& p : source.placements) {
            if (p.group == name) {
                out.push_back(p);
            }
        }
        return out;
    }
};

// THE BUILD HAND'S OWN VERDICT, and it is the hand's arithmetic rather than a
// paraphrase of it: judge the composition, judge it again with the candidate
// appended, and blame the candidate for whatever the judge started saying.
// App::update_build_tool() does exactly this, and it does it because asking
// "is there a finding with my index" is blind — NoOverlap hangs its finding on
// the EARLIER of a pair and the candidate is always appended last.
[[nodiscard]] app::BuildVerdict hand_verdict(const world::SceneDoc& doc,
                                             const world::Placement& candidate,
                                             const world::SceneWorld& w) {
    const std::vector<world::SceneFinding> before = world::check_scene(doc, w);
    world::SceneDoc probe = doc;
    probe.placements.push_back(candidate);
    const std::vector<world::SceneFinding> after = world::check_scene(probe, w);
    if (after.size() <= before.size()) {
        return {true, {}};
    }
    std::vector<world::SceneFinding> blame;
    blame.push_back(after[before.size()]);
    blame.back().placement_index = 0;
    return app::verdict_from_findings(blame, 0);
}

// Places what it can, defers what it cannot, and comes back — which is what a
// builder does when a part is refused for want of the thing that holds it.
struct BuildRun {
    int placed = 0;
    int rounds = 0;
    std::vector<world::Placement> stuck;
    std::vector<std::string> stuck_reasons;
    world::SceneDoc doc;
};

[[nodiscard]] BuildRun build_by_hand(const Bench& bench,
                                     std::vector<world::Placement> parts,
                                     bool with_group) {
    BuildRun run;
    run.doc.pads = bench.source.pads;
    if (!with_group) {
        for (world::Placement& p : parts) {
            p.group.clear();
        }
    }
    while (!parts.empty() && run.rounds < 12) {
        ++run.rounds;
        std::vector<world::Placement> left;
        std::vector<std::string> why;
        int this_round = 0;
        for (const world::Placement& part : parts) {
            const app::BuildVerdict v = hand_verdict(run.doc, part, bench.world);
            if (v.allowed) {
                run.doc.placements.push_back(part);
                ++run.placed;
                ++this_round;
            } else {
                left.push_back(part);
                why.push_back(v.reason);
            }
        }
        if (this_round == 0) {
            run.stuck = std::move(left);
            run.stuck_reasons = std::move(why);
            break;
        }
        parts = std::move(left);
    }
    return run;
}

} // namespace

TEST_CASE("the sleeve judges the same world the map does") {
    // THE CONTROL FOR EVERY NUMBER BELOW. If the finished house is not clean
    // under this ground, the sleeve is measuring its own ground model and each
    // later refusal is noise. tools/check_scene reports 0 findings for the
    // whole demo map (docs/HOUSES.md §9.5); this is that same zero, restricted
    // to one house and reached through the same judge.
    Bench bench;
    for (const char* name : {"house-log", "house-frame", "house-stone", "stairwell"}) {
        world::SceneDoc whole;
        whole.pads = bench.source.pads;
        whole.placements = bench.group(name);
        REQUIRE(whole.placements.size() > 10);
        const std::vector<world::SceneFinding> f = world::check_scene(whole, bench.world);
        INFO("group ", name, " findings ", f.size());
        CHECK(f.empty());
    }
}

TEST_CASE("a hand that cannot say what it is building cannot build a house") {
    // THE RED HAND FOR THE GROUP FIELD, and the reason App's build hand grew
    // one. A group changes two rules (Scene.h): members may intersect, and a
    // member may rest on ANOTHER MEMBER instead of on the terrain. Take it
    // away and every part above the footings is judged as a lone object that
    // must stand on the earth — so a wall on posts, a rafter and a door all
    // become "hovering", and the joint rules never get a word in.
    //
    // Both arms are one function with one flag changed.
    Bench bench;
    const std::vector<world::Placement> parts = bench.group("house-log");
    REQUIRE(parts.size() == 46);

    int refused_alone = 0;
    world::SceneDoc alone;
    alone.pads = bench.source.pads;
    for (world::Placement p : parts) {
        p.group.clear();
        if (!hand_verdict(alone, p, bench.world).allowed) {
            ++refused_alone;
        }
        alone.placements.push_back(p);
    }

    int refused_grouped = 0;
    world::SceneDoc grouped;
    grouped.pads = bench.source.pads;
    for (const world::Placement& p : parts) {
        if (!hand_verdict(grouped, p, bench.world).allowed) {
            ++refused_grouped;
        }
        grouped.placements.push_back(p);
    }

    // MEASURED, in the demo's own file order: 21 refusals without a group
    // against 2 with one. The two are the ridge, and they are the next case.
    CHECK(refused_alone == 21);
    CHECK(refused_grouped == 2);
    CHECK(refused_alone > refused_grouped * 5);
}

TEST_CASE("the house builds part by part, and the roof is where the rules run out") {
    // WHAT THE USER ASKED FOR, answered with a number per house: how much of a
    // demo house can be laid down one part at a time, every step judged?
    //
    // Everything except the roof cap. Foundation, posts, panels between TWO
    // posts, floor beams and boards, door, stairs — all of it lands, because
    // each part has something under it by the time it goes down.
    //
    // THE ROOF IS A CYCLE, and this is the finding this suite exists to carry:
    //   * the ridge (sleeper at 30.5) is refused by ON-GROUND — "hovers above
    //     wall-log-timber-16x1x13-blind-w05 (+1.75 m)" — until the gables
    //     stand under it;
    //   * the gables are refused by ROOF-SEAT — "apex hangs on nothing: no
    //     lying joint of this group crosses it AT ALL" — until the ridge lies
    //     in them;
    //   * the slopes are refused by ROOF-SEAT for the same missing ridge.
    // Each rule is right about the finished house and the three together are
    // unsatisfiable while it is growing. The judge is consistent for a scene
    // and incomplete for a SEQUENCE, and that distinction had never been
    // measured before this sleeve.
    //
    // NOT FIXED HERE ON PURPOSE. A rule that forbids a legal step is worth
    // more as a number than as a silent patch, and the fix belongs to the
    // judge's zone: the honest shape is probably §8.2's — a gable that stands
    // on the wall plate beneath it is SEATED, and only its apex overhang needs
    // the ridge.
    Bench bench;

    struct Expect {
        const char* group;
        int total;
        int placed;
    };
    // Measured 17.08.2026, this tree, this shelf.
    const Expect table[] = {
        {"house-log", 46, 38},
        {"house-frame", 66, 55},
        {"house-stone", 106, 95},
        // THE STAIRWELL GOES IN WHOLE, and it is the half of the answer that
        // says yes: deck with a declared hole, its lying joints and the flight
        // through it all place one at a time. The mechanic the user named
        // («лестница с проёмом») is buildable by hand today.
        {"stairwell", 13, 13},
    };
    for (const Expect& e : table) {
        const std::vector<world::Placement> parts = bench.group(e.group);
        REQUIRE(static_cast<int>(parts.size()) == e.total);
        const BuildRun run = build_by_hand(bench, parts, /*with_group=*/true);
        INFO("group ", e.group, ": placed ", run.placed, " of ", e.total, " in ",
             run.rounds, " rounds");
        CHECK(run.placed == e.placed);
        // WHATEVER WENT DOWN IS A LEGAL SCENE, every step of the way — the
        // point of the whole exercise. A run that placed 38 parts into a
        // composition the judge dislikes would have proved nothing.
        CHECK(world::check_scene(run.doc, bench.world).empty());
        for (const std::string& reason : run.stuck_reasons) {
            // The only two refusals the cycle can produce. A THIRD reason
            // appearing here is a new defect wearing the finding's clothes.
            CHECK((reason == "build.no.ground" || reason == "build.no.roof"));
        }
    }
}
