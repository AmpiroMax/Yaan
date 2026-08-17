/*
Created: 17:08:2026 - 12:49:26
Last updated: 17:08:2026 - 12:49:26
Module: tests
File: tests/core/SceneJointRuleTests.cpp

Responsibility:
- The connector rules of the scene judge (HOUSES.md §5): a panel end lives
  inside its joint post (JointSeat) and a panel's angle lands on a facet
  (JointAngle) — each with the red hand it exists to reject and the green
  control that proves it rejects the right thing.

Key items:
- flat_world(): a synthetic SceneWorld — flat ground, table-driven extents.
- The §5 red/green pairs: панели встык; 45 deg on n4 vs 45 deg on n8/round;
  turned square post; too-fine facet.

Dependencies:
- Uses: engine/world (Scene), doctest.
- Used by: ctest (core_scene_joint_rules).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 30 shapes this file: every assertion of "the rule fires" sits next to
  the assertion that the corrected scene passes THE SAME rule. A red hand
  without its green twin proves only that the rule fires on something.
*/
/*
UPD:
- 17:08:2026 - 12:49:26: Создан вместе с правилами JointSeat/JointAngle.
*/

#include "engine/world/sources/Scene.h"

#include <cmath>
#include <cstring>
#include <doctest/doctest.h>
#include <string>
#include <vector>

using namespace dfn::world;

namespace {

/// Table-driven world: flat ground at 0, every object known, panels 4 m long
/// and 0.25 m thick along local +X (the kit's convention), joints judged by
/// name alone (their extent is irrelevant to the rules under test).
float flat_ground(void*, glm::vec2) { return 0.0f; }

bool extent_of(void*, const std::string&, float& r, float& b) {
    r = 2.0f;
    b = 0.0f;
    return true;
}

bool top_of(void*, const std::string&, float& t) {
    t = 2.75f;
    return true;
}

bool box_of(void*, const std::string& name, glm::vec2& lo, glm::vec2& hi) {
    if (name.rfind("wall-", 0) == 0) {
        lo = {0.0f, 0.0f};
        hi = {4.0f, 0.25f};
        return true;
    }
    // Joints and everything else: a small square about the origin.
    lo = {-0.25f, -0.25f};
    hi = {0.25f, 0.25f};
    return true;
}

SceneWorld flat_world() {
    SceneWorld w;
    w.ground_at = &flat_ground;
    w.object_extent = &extent_of;
    w.object_top = &top_of;
    w.object_box = &box_of;
    return w;
}

Placement place(const std::string& obj, float x, float z, float yaw,
                const std::string& group) {
    Placement p;
    p.object = obj;
    p.position = {x, 0.0f, z};
    p.yaw = yaw;
    p.group = group;
    return p;
}

constexpr const char* J_N4 = "joint-timber-d50-n4-h11-w03";
/// The octagon must come from the d75 row to seat a 0.25 m panel: its facet
/// is 2 * r_in * tan(22.5) = 0.414 * d, so d50 offers 0.207 m — NARROWER
/// than the panel, a finding at every angle (the too-fine test below pins
/// that). The §5 green arm is honest only on a diameter that can seat it.
constexpr const char* J_N8 = "joint-timber-d75-n8-h11-w03";
constexpr const char* J_NR = "joint-timber-d50-nr-h11-w03";
constexpr const char* J_FINE = "joint-timber-d35-n8-h11-w03"; // facet 14.5 cm
constexpr const char* PANEL = "wall-timber-16x1x11-w03";

/// Places a panel with its MID-THICKNESS PLANE through (x, z) — the
/// centre-to-centre convention of §3.2. A kit panel's local origin sits on
/// its z = 0 FACE, so the composer (and this helper) backs it off by T/2
/// along the turned lateral axis; a panel placed face-on-axis hangs a corner
/// exactly T/2 + T/2 = T from the axis and fails the seat rule honestly.
Placement place_panel(float x, float z, float yaw, const std::string& group) {
    const float half_t = 0.125f;
    Placement p = place(PANEL, x - half_t * std::sin(yaw),
                        z - half_t * std::cos(yaw), yaw, group);
    return p;
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

TEST_CASE("JointSeat red hand: панели встык, ни одной стойки (the farmhouse defect)") {
    SceneDoc doc;
    doc.world_span_m = 256.0f;
    doc.placements.push_back(place_panel(100.0f, 100.0f, 0.0f, "house"));
    doc.placements.push_back(place_panel(104.0f, 100.0f, 0.0f, "house"));
    const auto found = check_scene(doc, flat_world());
    // Two panels, two bare ends each.
    CHECK(count_rule(found, SceneRule::JointSeat) == 4);
}

TEST_CASE("JointSeat green: the same wall with posts at both ends of each panel") {
    SceneDoc doc;
    doc.world_span_m = 256.0f;
    // Panels measured centre-to-centre: origin ON the post axis (§3.2).
    doc.placements.push_back(place(J_N4, 100.0f, 100.0f, 0.0f, "house"));
    doc.placements.push_back(place_panel(100.0f, 100.0f, 0.0f, "house"));
    doc.placements.push_back(place(J_N4, 104.0f, 100.0f, 0.0f, "house"));
    doc.placements.push_back(place_panel(104.0f, 100.0f, 0.0f, "house"));
    doc.placements.push_back(place(J_N4, 108.0f, 100.0f, 0.0f, "house"));
    const auto found = check_scene(doc, flat_world());
    CHECK(count_rule(found, SceneRule::JointSeat) == 0);
    CHECK(count_rule(found, SceneRule::JointAngle) == 0);
}

TEST_CASE("JointSeat measures the CORNERS: an end off-axis fails once the corner leaves") {
    // Post slid ALONG the panel's axis, so the end centre misses it by e and
    // the corner sits at sqrt(e^2 + (T/2)^2) — §5's own formula. e = 0.15:
    // corner at 0.195 < 0.23 allowed, still seated. e = 0.25: corner at
    // 0.280, out by 0.05. The pair pins the rule to the corner formula, not
    // to a designated distance.
    for (const float off : {0.15f, 0.25f}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(J_N4, 100.0f + off, 100.0f, 0.0f, "house"));
        doc.placements.push_back(place_panel(100.0f, 100.0f, 0.0f, "house"));
        doc.placements.push_back(place(J_N4, 104.0f + off, 100.0f, 0.0f, "house"));
        const auto found = check_scene(doc, flat_world());
        CAPTURE(off);
        CHECK(count_rule(found, SceneRule::JointSeat) == (off > 0.2f ? 2 : 0));
    }
}

TEST_CASE("a neighbour's post ties nothing: joints bind only their own group") {
    SceneDoc doc;
    doc.world_span_m = 256.0f;
    doc.placements.push_back(place_panel(100.0f, 100.0f, 0.0f, "house-a"));
    doc.placements.push_back(place(J_N4, 100.0f, 100.0f, 0.0f, "house-b"));
    doc.placements.push_back(place(J_N4, 104.0f, 100.0f, 0.0f, "house-b"));
    const auto found = check_scene(doc, flat_world());
    CHECK(count_rule(found, SceneRule::JointSeat) == 2);
}

TEST_CASE("JointAngle: 45 deg on a SQUARE post is the defect, 45 deg on n8 and round is not") {
    // HOUSES.md §5's own red/green pair, verbatim: the pair proves the rule
    // catches «косой угол НА ЭТОЙ стойке», not «косой угол».
    const float yaw45 = 0.7853982f;
    for (const char* joint : {J_N4, J_N8, J_NR}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(joint, 100.0f, 100.0f, 0.0f, "house"));
        doc.placements.push_back(place_panel(100.0f, 100.0f, yaw45, "house"));
        // The far post sits where the turned panel actually ends.
        const float fx = 100.0f + 4.0f * std::cos(yaw45);
        const float fz = 100.0f - 4.0f * std::sin(yaw45);
        doc.placements.push_back(place(joint, fx, fz, 0.0f, "house"));
        const auto found = check_scene(doc, flat_world());
        CAPTURE(joint);
        const bool square = std::strcmp(joint, J_N4) == 0;
        CHECK(count_rule(found, SceneRule::JointAngle) == (square ? 2 : 0));
        CHECK(count_rule(found, SceneRule::JointSeat) == 0);
    }
}

TEST_CASE("the angle is measured against the POST'S facets, not the world axes") {
    // A square post itself turned 30 deg: a panel at 0 deg now sits BETWEEN
    // facets (worst case for n4 is 45 deg; 30 exceeds the derived tolerance
    // atan(0.125/0.25) = 26.6 deg) — and a panel at the same 30 deg is flush.
    const float yaw30 = 0.5235988f;
    for (const float panel_yaw : {0.0f, yaw30}) {
        SceneDoc doc;
        doc.world_span_m = 256.0f;
        doc.placements.push_back(place(J_N4, 100.0f, 100.0f, yaw30, "house"));
        doc.placements.push_back(place_panel(100.0f, 100.0f, panel_yaw, "house"));
        const float fx = 100.0f + 4.0f * std::cos(panel_yaw);
        const float fz = 100.0f - 4.0f * std::sin(panel_yaw);
        doc.placements.push_back(place(J_N4, fx, fz, yaw30, "house"));
        const auto found = check_scene(doc, flat_world());
        CAPTURE(panel_yaw);
        const bool flush = panel_yaw > 0.1f;
        CHECK(count_rule(found, SceneRule::JointAngle) == (flush ? 0 : 2));
    }
}

TEST_CASE("a facet narrower than the panel fails at EVERY angle") {
    // d35 n8: facet 2 * 0.175 * tan(22.5) = 0.145 m against a 0.25 m panel.
    // Perfectly aligned — and still a finding, because no yaw can seat it.
    SceneDoc doc;
    doc.world_span_m = 256.0f;
    doc.placements.push_back(place(J_FINE, 100.0f, 100.0f, 0.0f, "house"));
    doc.placements.push_back(place_panel(100.0f, 100.0f, 0.0f, "house"));
    doc.placements.push_back(place(J_FINE, 104.0f, 100.0f, 0.0f, "house"));
    const auto found = check_scene(doc, flat_world());
    CHECK(count_rule(found, SceneRule::JointAngle) == 2);
    // The green twin: the same panel on d50 n4 passes (the green test above),
    // so what fires here is the PAIRING, not the rule being trigger-happy.
}

TEST_CASE("describe names the connector rules for the report") {
    SceneFinding f;
    f.rule = SceneRule::JointSeat;
    f.object = PANEL;
    f.detail = "near end: no joint post in this group at all";
    CHECK(describe(f).find("joint-seat") != std::string::npos);
    f.rule = SceneRule::JointAngle;
    CHECK(describe(f).find("joint-angle") != std::string::npos);
}
