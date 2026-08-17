/*
Created: 17:08:2026 - 16:58:13
Last updated: 17:08:2026 - 16:58:13
Module: tests
File: tests/core/SceneHouseRuleTests.cpp

Responsibility:
- The BUILDING rules of the scene judge (HOUSES.md §8), each with the red hand
  it exists to reject and the green control that proves it rejects the right
  thing: WallTwoJoints, JointCapacity, DeckOnJoints, RoofSeat — plus the seam
  that decides WHO these rules judge at all (a member of a building, not a
  sample on a shelf).

Key items:
- kit_world(): a synthetic SceneWorld whose extents follow the kit's real
  conventions (panel along +X, deck as a slab, joint by name alone).
- The §8 red/green pairs. The one that matters most is the CAPACITY pair: two
  panels on one facet at one station is a defect, the same two panels on the
  same facet at two storeys is a house — the first pass over houses/demo
  reported 38 of the second as the first.

Dependencies:
- Uses: engine/world (Scene, SceneHouseRules through check_scene), doctest.
- Used by: ctest (core_scene_house_rules).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- BOTH HANDS COME OUT OF ONE BINARY AND DIFFER BY ONE PARAMETER (Rule 30). In
  this file the parameter is usually a single number in a loop — a storey
  height, a post count, a metre of offset — so that «the rule fires» and «the
  rule does not fire» cannot be two different scenes with two different bugs.
- A RULE THAT NEVER WENT RED IS NOT CHECKED. Every SceneRule value added by
  the 17.08 order has a red arm here; if you add a fifth, add its red arm in
  the same commit, not in the next one.
*/
/*
UPD:
- 17:08:2026 - 16:58:13: Создан. Живые красные руки на четыре правила заказа 17.08 и на
  шов «группа»: правила постройки судят ЧЛЕНА ПОСТРОЙКИ, одиночный образец
  витрины судится землёй (и контрфакт: снятая группа не глушит судью, а
  меняет его — висящий скат немедленно краснеет как OnGround).
*/

#include "engine/world/sources/Scene.h"

#include <cmath>
#include <doctest/doctest.h>
#include <string>
#include <vector>

using namespace dfn::world;

namespace {

constexpr float STOREY_M = 2.75f; ///< what top_of() reports for a wall panel
constexpr float DECK_T_M = 0.10f; ///< настил
constexpr float SLEEPER_R_M = 0.175f; ///< d35 across-flats -> r_in

float flat_ground(void*, glm::vec2) { return 0.0f; }

bool extent_of(void*, const std::string&, float& r, float& b) {
    r = 2.0f;
    b = 0.0f;
    return true;
}

/// How far a part rises above its own origin. Only two answers matter to the
/// rules under test: a wall is a storey tall, a deck is its own thickness.
bool top_of(void*, const std::string& name, float& t) {
    if (name.rfind("floor-", 0) == 0) {
        t = DECK_T_M;
        return true;
    }
    t = STOREY_M;
    return true;
}

/// The kit's footprints. A wall runs 4 m along local +X and is 0.25 m thick;
/// a deck is a 4 x 4 m slab from its origin corner (starts AT THE CORNER —
/// the kit convention this project has already paid for once).
bool box_of(void*, const std::string& name, glm::vec2& lo, glm::vec2& hi) {
    if (name.rfind("wall-stub-", 0) == 0) {
        // A degenerate module: an angle and no span. Nothing in the shelf is
        // shaped like this, and that is the point — it is the ONLY way both
        // ends of one panel can land inside one post, which is exactly what
        // WallTwoJoints exists to name.
        lo = {0.0f, 0.0f};
        hi = {0.10f, 0.25f};
        return true;
    }
    if (name.rfind("wall-", 0) == 0) {
        lo = {0.0f, 0.0f};
        hi = {4.0f, 0.25f};
        return true;
    }
    if (name.rfind("floor-", 0) == 0) {
        lo = {0.0f, 0.0f};
        hi = {4.0f, 4.0f};
        return true;
    }
    lo = {-0.25f, -0.25f};
    hi = {0.25f, 0.25f};
    return true;
}

SceneWorld kit_world() {
    SceneWorld w;
    w.ground_at = &flat_ground;
    w.object_extent = &extent_of;
    w.object_top = &top_of;
    w.object_box = &box_of;
    return w;
}

Placement place(const std::string& obj, glm::vec3 pos, float yaw,
                const std::string& group) {
    Placement p;
    p.object = obj;
    p.position = pos;
    p.yaw = yaw;
    p.group = group;
    return p;
}

constexpr const char* J_N4 = "joint-timber-d50-n4-h13-w03";
constexpr const char* J_NR = "joint-timber-d50-nr-h13-w03";
constexpr const char* PANEL = "wall-timber-16x1x13-w03";
/// Начинается с `wall-`, иначе правило его вообще не увидит: судья узнаёт
/// модуль стены ПО ИМЕНИ, и «wall_stub-» проходил бы мимо всех правил разом —
/// первый прогон этого теста поймал ровно это.
constexpr const char* STUB = "wall-stub-timber-1x1x13-w03";
constexpr const char* DECK = "floor-board-16x16x1-w03";
/// sleeper-<mat>-d<cm>-n<N>-<L>u-w<NN>: 4 m of lying joint, round, so that the
/// facet-angle rule never speaks in a test about seating.
constexpr const char* SLEEPER = "sleeper-timber-d35-nr-16u-w03";
/// roof-<mat>-<run>x<depth>x<rise>: run 3 m, depth 4 m, rise 3 m — a 45 deg
/// slope whose lower and upper edges are PARALLEL (the family that makes
/// arches and canopies, HOUSES.md §8).
constexpr const char* SLOPE = "roof-thatch-12x16x12-w03";

/// A wall panel with its MID-THICKNESS PLANE through (x, z) — §3.2.
Placement wall(float x, float y, float z, float yaw, const std::string& group,
               const char* obj = PANEL) {
    const float half_t = 0.125f;
    return place(obj, {x - half_t * std::sin(yaw), y, z - half_t * std::cos(yaw)},
                 yaw, group);
}

/// A lying joint whose AXIS passes through (x, y, z): the part beds on its
/// underside, so the origin sits r_in below the axis.
Placement sleeper(float x, float y, float z, float yaw, const std::string& group) {
    return place(SLEEPER, {x, y - SLEEPER_R_M, z}, yaw, group);
}

int count_rule(const std::vector<SceneFinding>& fs, SceneRule r) {
    int n = 0;
    for (const auto& f : fs) {
        if (f.rule == r) {
            ++n;
        }
    }
    return n;
}

} // namespace

// ---------------------------------------------------------------------------
// WallTwoJoints — «каждый модуль стены присоединён к ДВУМ шарнирам-столбам».
// ---------------------------------------------------------------------------

TEST_CASE("WallTwoJoints: both ends in ONE post is a pin, not a wall") {
    // ONE PARAMETER between the hands: the module's span. The stub is 0.10 m
    // long, so both its ends sit inside a d50 post and JointSeat is happy
    // TWICE — which is the whole reason this rule had to exist separately.
    for (const char* obj : {STUB, PANEL}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(J_N4, {100.0f, 0.0f, 100.0f}, 0.0f, "house"));
        doc.placements.push_back(wall(100.0f, 0.0f, 100.0f, 0.0f, "house", obj));
        doc.placements.push_back(place(J_N4, {104.0f, 0.0f, 100.0f}, 0.0f, "house"));
        const auto found = check_scene(doc, kit_world());
        CAPTURE(obj);
        const bool degenerate = obj == STUB;
        CHECK(count_rule(found, SceneRule::WallTwoJoints) == (degenerate ? 1 : 0));
        // And the seat rule says nothing either way on the stub: both its
        // ends really are inside a post. Без отдельного правила дефект
        // прошёл бы зелёным.
        CHECK(count_rule(found, SceneRule::JointSeat) == 0);
    }
}

// ---------------------------------------------------------------------------
// JointCapacity — сколько граней, столько панелей. И ЛИМИТ СТАНЦИОННЫЙ.
// ---------------------------------------------------------------------------

TEST_CASE("JointCapacity: one facet, one panel — AT A STATION, not over the whole post") {
    // THE PAIR THAT COST 38 FALSE FINDINGS. One parameter: the second panel's
    // height. At 0 it shares the first panel's storey and the facet is truly
    // taken twice; at 2.75 it is the second storey of a two-storey house,
    // which every carpenter builds and the first version of this rule called
    // a defect thirty-eight times.
    for (const float second_y : {0.0f, STOREY_M}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(J_N4, {100.0f, 0.0f, 100.0f}, 0.0f, "house"));
        doc.placements.push_back(place(J_N4, {104.0f, 0.0f, 100.0f}, 0.0f, "house"));
        doc.placements.push_back(wall(100.0f, 0.0f, 100.0f, 0.0f, "house"));
        doc.placements.push_back(wall(100.0f, second_y, 100.0f, 0.0f, "house"));
        const auto found = check_scene(doc, kit_world());
        CAPTURE(second_y);
        const bool same_station = second_y < 0.1f;
        CHECK(count_rule(found, SceneRule::JointCapacity) == (same_station ? 2 : 0));
    }
}

TEST_CASE("JointCapacity: five panels at one station on a square post, four is the cap") {
    // One parameter: how many panels meet. Four at 90 deg is a crossing every
    // town has; the fifth has no facet left to sit on.
    for (const int count : {4, 5}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(J_N4, {100.0f, 0.0f, 100.0f}, 0.0f, "house"));
        for (int k = 0; k < count; ++k) {
            // The fifth doubles up on facet 0 — there is nowhere else for it.
            const float yaw = 1.5707963f * static_cast<float>(k % 4);
            doc.placements.push_back(wall(100.0f, 0.0f, 100.0f, yaw, "house"));
            doc.placements.push_back(place(J_N4,
                                           {100.0f + 4.0f * std::cos(yaw), 0.0f,
                                            100.0f - 4.0f * std::sin(yaw)},
                                           0.0f, "house"));
        }
        const auto found = check_scene(doc, kit_world());
        CAPTURE(count);
        CHECK((count_rule(found, SceneRule::JointCapacity) > 0) == (count == 5));
    }
}

TEST_CASE("JointCapacity: a ROUND joint has no facets to run out of") {
    // The green control for the rule's own premise (§4): the limit is not a
    // budget somebody chose, it is the count of facets — so a drum has none.
    // Same five panels as the red arm above, one parameter changed: the post.
    for (const char* joint : {J_N4, J_NR}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(joint, {100.0f, 0.0f, 100.0f}, 0.0f, "house"));
        for (int k = 0; k < 5; ++k) {
            const float yaw = 1.5707963f * static_cast<float>(k % 4);
            doc.placements.push_back(wall(100.0f, 0.0f, 100.0f, yaw, "house"));
            doc.placements.push_back(place(joint,
                                           {100.0f + 4.0f * std::cos(yaw), 0.0f,
                                            100.0f - 4.0f * std::sin(yaw)},
                                           0.0f, "house"));
        }
        const auto found = check_scene(doc, kit_world());
        CAPTURE(joint);
        CHECK((count_rule(found, SceneRule::JointCapacity) > 0) == (joint == J_N4));
    }
}

// ---------------------------------------------------------------------------
// DeckOnJoints — пол и потолок не висят в пространстве.
// ---------------------------------------------------------------------------

TEST_CASE("DeckOnJoints: a deck on ONE joist hangs, on two it is let in") {
    // One parameter: how many joists cross the deck. The deck is 4 x 4 m from
    // its origin corner; the joists run along +X under z = 100.5 and z = 103.5.
    for (const int joists : {1, 2}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(DECK, {100.0f, 3.0f, 100.0f}, 0.0f, "house"));
        for (int k = 0; k < joists; ++k) {
            doc.placements.push_back(sleeper(99.0f, 3.05f,
                                             100.5f + 3.0f * static_cast<float>(k),
                                             0.0f, "house"));
        }
        const auto found = check_scene(doc, kit_world());
        CAPTURE(joists);
        CHECK(count_rule(found, SceneRule::DeckOnJoints) == (joists == 1 ? 1 : 0));
    }
}

TEST_CASE("DeckOnJoints: two joists under ONE half hold it like two legs under a table") {
    // One parameter: where the second joist runs. Both under the near edge —
    // the deck's own centre is past them and the rest of it hangs. The
    // COUNT is two in both arms, so what fires is the geometry, not the tally.
    for (const float second_z : {100.7f, 103.5f}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(DECK, {100.0f, 3.0f, 100.0f}, 0.0f, "house"));
        doc.placements.push_back(sleeper(99.0f, 3.05f, 100.5f, 0.0f, "house"));
        doc.placements.push_back(sleeper(99.0f, 3.05f, second_z, 0.0f, "house"));
        const auto found = check_scene(doc, kit_world());
        CAPTURE(second_z);
        const bool lopsided = second_z < 101.0f;
        CHECK(count_rule(found, SceneRule::DeckOnJoints) == (lopsided ? 1 : 0));
    }
}

TEST_CASE("DeckOnJoints: five joists exceed the cap of four") {
    // «Шарниров от 2 до 4.» A plank floor is nailed to a dozen lags; a PANEL
    // is framed on its sides, and this rule is about panels.
    for (const int joists : {4, 5}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(DECK, {100.0f, 3.0f, 100.0f}, 0.0f, "house"));
        for (int k = 0; k < joists; ++k) {
            doc.placements.push_back(sleeper(99.0f, 3.05f,
                                             100.2f + 0.9f * static_cast<float>(k),
                                             0.0f, "house"));
        }
        const auto found = check_scene(doc, kit_world());
        CAPTURE(joists);
        CHECK(count_rule(found, SceneRule::DeckOnJoints) == (joists == 5 ? 1 : 0));
    }
}

// ---------------------------------------------------------------------------
// RoofSeat — скат низом на один горизонтальный шарнир, верхом на другой.
// ---------------------------------------------------------------------------

TEST_CASE("RoofSeat: a slope with no ridge purlin under its upper edge") {
    // THE 22 FINDINGS ON houses/demo, in miniature. One parameter: whether the
    // ridge purlin exists. Both arms keep the eaves purlin, so what fires is
    // the missing ridge and nothing else.
    for (const bool ridge : {false, true}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(J_N4, {100.0f, 0.0f, 100.0f}, 0.0f, "house"));
        doc.placements.push_back(place(J_N4, {100.0f, 0.0f, 104.0f}, 0.0f, "house"));
        // The slope's origin is its LOWER edge corner; +X runs up the pitch.
        doc.placements.push_back(place(SLOPE, {100.0f, 3.0f, 100.0f}, 0.0f, "house"));
        doc.placements.push_back(sleeper(100.0f, 3.0f, 100.0f, -1.5707963f, "house"));
        if (ridge) {
            doc.placements.push_back(sleeper(103.0f, 6.0f, 100.0f, -1.5707963f,
                                             "house"));
        }
        const auto found = check_scene(doc, kit_world());
        CAPTURE(ridge);
        CHECK(count_rule(found, SceneRule::RoofSeat) == (ridge ? 0 : 1));
    }
}

TEST_CASE("RoofSeat: КОЗЫРЁК — a lower edge is free only OUTSIDE the group's posts") {
    // The licence the user granted is not «низ свободен» but «низ снаружи».
    // One parameter: where the group's posts stand. When they stand under the
    // eaves, the same unseated edge is a defect; when the eaves reach past
    // them, it is a canopy. Ridge purlin present in both arms.
    for (const float post_z : {100.0f, 108.0f}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(J_N4, {100.0f, 0.0f, post_z}, 0.0f, "house"));
        doc.placements.push_back(place(J_N4, {100.0f, 0.0f, post_z + 4.0f}, 0.0f,
                                       "house"));
        doc.placements.push_back(place(SLOPE, {100.0f, 3.0f, 100.0f}, 0.0f, "house"));
        doc.placements.push_back(sleeper(103.0f, 6.0f, 100.0f, -1.5707963f, "house"));
        const auto found = check_scene(doc, kit_world());
        CAPTURE(post_z);
        const bool over_own_posts = post_z < 104.0f;
        CHECK(count_rule(found, SceneRule::RoofSeat) == (over_own_posts ? 1 : 0));
    }
}

// ---------------------------------------------------------------------------
// ШОВ: КОГО эти правила судят вообще.
// ---------------------------------------------------------------------------

TEST_CASE("the building rules judge a MEMBER OF A BUILDING, and the seam is not a loophole") {
    // Arm 1: the same slope, no group — a sample on the showcase shelf. The
    // building rules say nothing (the 9 findings on houses/showcase were all
    // of these), and the slope is judged BY THE GROUND instead.
    // Arm 2: the same groupless slope lifted to ridge height. The judge does
    // not go quiet — OnGround names it, because dropping the group hands the
    // part back to the terrain rather than to nobody.
    for (const float y : {0.0f, 6.0f}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(SLOPE, {100.0f, y, 100.0f}, 0.0f, ""));
        const auto found = check_scene(doc, kit_world());
        CAPTURE(y);
        CHECK(count_rule(found, SceneRule::RoofSeat) == 0);
        CHECK(count_rule(found, SceneRule::OnGround) == (y > 0.1f ? 1 : 0));
    }
}

TEST_CASE("describe names the building rules for the report") {
    SceneFinding f;
    f.object = PANEL;
    f.detail = "both ends seat in the SAME joint post";
    f.rule = SceneRule::WallTwoJoints;
    CHECK(describe(f).find("wall-two-joints") != std::string::npos);
    f.rule = SceneRule::JointCapacity;
    CHECK(describe(f).find("joint-capacity") != std::string::npos);
    f.rule = SceneRule::DeckOnJoints;
    CHECK(describe(f).find("deck-on-joints") != std::string::npos);
    f.rule = SceneRule::RoofSeat;
    CHECK(describe(f).find("roof-seat") != std::string::npos);
}
