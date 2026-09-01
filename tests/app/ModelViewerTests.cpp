/*
Module: tests
File: tests/app/ModelViewerTests.cpp

Responsibility:
- THE VIEWING STAND'S MODEL, held to what a frame cannot show: that the list is
  actually gathered from all three sources of this tree, that walking it wraps
  and jumps by shelf, that the framing distance really contains the bound, and
  that the display scale never quietly rescales something already in metres.

Key items:
- The scan over the REAL tree (Rule 30: a real shelf, not a fixture).
- The framing check, done GEOMETRICALLY: the bound's corners are projected and
  must land inside the frustum. A distance compared against a magic number
  would only be testing the number.

Dependencies:
- Uses: doctest, engine/app ModelViewer.
- Used by: ctest (app_model_viewer).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE COUNTS ARE FLOORS, NOT EQUALITIES. Waves add models to these folders
  daily; an exact count would go red for someone else's success, and a suite
  that cries wolf is a suite people stop reading.
*/

#include "engine/app/sources/ModelViewer.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

#include <cmath>
#include <filesystem>
#include <set>
#include <string>

using dfn::app::scan_viewer_items;
using dfn::app::ViewerItem;
using dfn::app::ViewerRoots;
using dfn::app::ViewerSource;

namespace {

/// Does the whole bound sit inside the view cone when the eye stands `dist`
/// away, looking at the bound's middle? Answered by projecting the eight
/// corners, which is the property the fit is FOR — not by comparing the
/// returned metres against a hand-written expectation.
[[nodiscard]] bool bound_fits(const glm::vec3& lo, const glm::vec3& hi, float dist,
                              float fov_y, float aspect) {
    const glm::vec3 centre = 0.5f * (lo + hi);
    // Straight-on, along -Z: the fit uses the bounding SPHERE, so the direction
    // it is checked from cannot change the answer, and this is the direction
    // whose arithmetic is readable.
    const glm::vec3 eye = centre + glm::vec3{0.0f, 0.0f, dist};
    const float half_v = 0.5f * fov_y;
    const float half_h = std::atan(std::tan(half_v) * aspect);
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 c{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y,
                          (i & 4) ? hi.z : lo.z};
        const glm::vec3 v = c - eye;
        const float depth = -v.z;
        if (depth <= 0.0f) {
            return false; // behind the eye: not framed, whatever the maths says
        }
        if (std::fabs(v.y) > depth * std::tan(half_v)) {
            return false;
        }
        if (std::fabs(v.x) > depth * std::tan(half_h)) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("the list is gathered from all three sources of the real tree") {
    const auto items = scan_viewer_items(ViewerRoots{});
    const auto tally = dfn::app::viewer_tally(items);

    // FLOORS TAKEN WELL BELOW WHAT IS THERE (2587 / 4 / 19 on the day this was
    // written). They exist to catch a scan that stopped working, not to pin a
    // tree that grows every day.
    CHECK(tally.shelf >= 500);
    CHECK(tally.character >= 3);
    CHECK(tally.downloaded >= 10);
    CHECK(tally.total() >= 520);

    // Заказ пункт 5: «список источников собирается (>= N позиций)». N здесь —
    // сумма трёх полов выше, и она проверяется как ОДНО утверждение о полном
    // списке, а не как три о его кусках.
    CHECK(static_cast<int>(items.size()) == tally.total());
}

TEST_CASE("what the scan must never list") {
    const auto items = scan_viewer_items(ViewerRoots{});
    for (const ViewerItem& i : items) {
        CHECK_FALSE(i.name.empty());
        CHECK_FALSE(i.category.empty());
        CHECK_FALSE(i.path.empty());
        CHECK_FALSE(i.origin.empty());
        // A `-far` twin is the same exhibit made cheaper. The gallery already
        // paid for showing both once.
        const bool far_form = i.name.size() > 4
                              && i.name.compare(i.name.size() - 4, 4, "-far") == 0;
        CHECK_FALSE(far_form);
        // OUR OWN CACHE IS NOT A SOURCE. Without the guard the second run of
        // the stand lists every converted figure twice — once as the file that
        // was fetched, once as the .dfo we made of it.
        CHECK(i.path.find("/.cache/") == std::string::npos);
    }
}

TEST_CASE("the order is stable and a category is contiguous") {
    const auto items = scan_viewer_items(ViewerRoots{});
    REQUIRE(items.size() > 2);
    // Sorted by (source, category, name): a walk with the arrow keys must
    // never leave a category and come back to it, or the Shift jump would land
    // in the middle of a shelf it already visited.
    std::set<std::pair<int, std::string>> seen;
    std::pair<int, std::string> current{-1, {}};
    for (const ViewerItem& i : items) {
        const std::pair<int, std::string> key{static_cast<int>(i.source), i.category};
        if (key != current) {
            CHECK(seen.insert(key).second); // never revisited
            current = key;
        }
    }
    // And two scans of one tree agree, which is what makes DFN_VIEWER_ITEM and
    // the acceptance frames reproducible at all.
    const auto again = scan_viewer_items(ViewerRoots{});
    REQUIRE(again.size() == items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        CHECK(again[i].path == items[i].path);
    }
}

TEST_CASE("walking the list wraps, and Shift jumps a whole shelf") {
    const auto items = scan_viewer_items(ViewerRoots{});
    REQUIRE(items.size() > 3);
    const int n = static_cast<int>(items.size());

    // WRAPS. The first draft clamped, and the last of 2610 models then had no
    // way back to the first except 2609 presses.
    CHECK(dfn::app::viewer_step_index(items, n - 1, +1, false) == 0);
    CHECK(dfn::app::viewer_step_index(items, 0, -1, false) == n - 1);
    CHECK(dfn::app::viewer_step_index(items, 5, +1, false) == 6);

    // BY CATEGORY: the landing place is the FIRST line of another category,
    // both ways. Backwards it must not stop on the LAST part of the previous
    // shelf — that reads as one step back, not as a jump.
    const int fwd = dfn::app::viewer_step_index(items, 0, +1, true);
    CHECK(items[static_cast<std::size_t>(fwd)].category != items[0].category);
    if (fwd > 0) {
        CHECK(items[static_cast<std::size_t>(fwd - 1)].category != items[static_cast<std::size_t>(fwd)].category);
    }
    const int back = dfn::app::viewer_step_index(items, fwd, -1, true);
    const std::string landed = items[static_cast<std::size_t>(back)].category;
    if (back > 0) {
        CHECK(items[static_cast<std::size_t>(back - 1)].category != landed);
    }
}

TEST_CASE("a named model is found, and a wrong name is not silently substituted") {
    const auto items = scan_viewer_items(ViewerRoots{});
    REQUIRE(!items.empty());
    const std::string name = items[items.size() / 2].name;
    const int at = dfn::app::viewer_find_item(items, name);
    REQUIRE(at >= 0);
    CHECK(items[static_cast<std::size_t>(at)].name == name);
    // THE MISS IS -1, NOT 0. A door that showed model zero for a typo would
    // hand back an acceptance frame that is plausible and not the one asked
    // for, which is the failure a named door exists to avoid.
    CHECK(dfn::app::viewer_find_item(items, "нет-такой-модели-нигде") == -1);
    CHECK(dfn::app::viewer_find_item(items, "") == -1);
}

TEST_CASE("the display scale is decided by the SOURCE, not by the size") {
    using S = ViewerSource;
    // OUR OWN PIPELINE IS ALWAYS 1.0, whatever the size. This arm is the one
    // that went red in the real frame first: a size band drew the 17.6 m oak of
    // the tree shelf at the height of a man, because the rule had no way to know
    // that the forge already works in metres.
    CHECK(dfn::app::viewer_display_scale(S::Shelf, {-0.3f, 0.0f, -0.2f},
                                         {0.3f, 1.8f, 0.2f}) == doctest::Approx(1.0f));
    CHECK(dfn::app::viewer_display_scale(S::Shelf, {-9.0f, 0.0f, -9.0f},
                                         {9.0f, 17.6f, 9.0f}) == doctest::Approx(1.0f));
    CHECK(dfn::app::viewer_display_scale(S::Shelf, {-30.0f, 0.0f, -30.0f},
                                         {30.0f, 60.0f, 30.0f}) == doctest::Approx(1.0f));
    // The importer bakes metres and facing, so a character is 1.0 too.
    CHECK(dfn::app::viewer_display_scale(S::Character, {-0.3f, 0.0f, -0.2f},
                                         {0.3f, 1.75f, 0.2f}) == doctest::Approx(1.0f));

    // A DOWNLOAD IS NORMALISED, because its units are unknown by definition.
    // This tree's own gallery figures measure 31..46 along their tallest side —
    // millimetres — and the factor is a quotient of two measurements, never a
    // guessed millimetre constant.
    const float mini = dfn::app::viewer_display_scale(S::Downloaded, {-20.0f, 0.0f, -20.0f},
                                                      {20.0f, 62.0f, 20.0f});
    CHECK(mini < 1.0f);
    CHECK(62.0f * mini == doctest::Approx(dfn::app::VIEWER_SCALE_TARGET_M));
    const float speck = dfn::app::viewer_display_scale(S::Downloaded, {0.0f, 0.0f, 0.0f},
                                                       {0.003f, 0.004f, 0.003f});
    CHECK(speck > 1.0f);
    CHECK(0.004f * speck == doctest::Approx(dfn::app::VIEWER_SCALE_TARGET_M));
    // An empty bound scales by nothing rather than by infinity.
    CHECK(dfn::app::viewer_display_scale(S::Downloaded, glm::vec3{0.0f}, glm::vec3{0.0f})
          == doctest::Approx(1.0f));
}

TEST_CASE("the fit distance really contains the bound") {
    const float fov = 1.309f; // config::CAMERA_FOV_Y
    struct Case {
        glm::vec3 lo;
        glm::vec3 hi;
    };
    const Case cases[] = {
        {{-0.3f, 0.0f, -0.2f}, {0.3f, 1.8f, 0.2f}},   // a figure
        {{-4.0f, 0.0f, -4.0f}, {4.0f, 11.5f, 4.0f}},  // a tree
        {{-0.05f, 0.0f, -0.05f}, {0.05f, 0.08f, 0.05f}}, // a cup
        {{-0.9f, 0.0f, -0.2f}, {0.9f, 0.25f, 0.2f}},  // a long low beam
    };
    for (const float aspect : {16.0f / 9.0f, 1.0f, 9.0f / 16.0f}) {
        for (const Case& c : cases) {
            const float d = dfn::app::viewer_fit_distance(c.lo, c.hi, fov, aspect);
            CHECK(d > 0.0f);
            CHECK(bound_fits(c.lo, c.hi, d, fov, aspect));
            // AND THE CONTROL ARM: at a fifth of that distance it must NOT fit,
            // or the check above would pass for any number at all (Rule 30 —
            // an instrument that cannot fail is not an instrument).
            CHECK_FALSE(bound_fits(c.lo, c.hi, d * 0.2f, fov, aspect));
        }
    }
    // A bigger model stands further back. Monotone, which is the one property
    // a single measurement cannot show.
    const float small = dfn::app::viewer_fit_distance(glm::vec3{0.0f}, {0.2f, 0.2f, 0.2f}, fov, 1.7f);
    const float big = dfn::app::viewer_fit_distance(glm::vec3{0.0f}, {2.0f, 2.0f, 2.0f}, fov, 1.7f);
    CHECK(big > small * 5.0f);
}

TEST_CASE("the orbiting eye looks where it stands") {
    dfn::app::ViewerView v = dfn::app::viewer_reset(ViewerSource::Shelf,
                                                    {-0.3f, 0.0f, -0.3f},
                                                    {0.3f, 1.8f, 0.3f}, 1.309f, 1.7f);
    const glm::vec3 target{40.0f, 26.4f, 40.0f};
    for (int i = 0; i < 8; ++i) {
        v.orbit_yaw = static_cast<float>(i) * 0.7f;
        v.orbit_pitch = -0.4f + static_cast<float>(i) * 0.1f;
        const glm::vec3 eye = dfn::app::viewer_eye(target, v);
        // The distance is the distance, at every azimuth.
        CHECK(glm::length(eye - target) == doctest::Approx(v.dist_m).epsilon(1e-4));
        // And the frame's own look vector for (yaw, pitch) points from the eye
        // AT the target — the property that makes the pose handed to
        // FirstPersonCamera frame the model rather than the sky.
        const float cp = std::cos(v.orbit_pitch);
        const glm::vec3 look{std::sin(v.orbit_yaw) * cp, std::sin(v.orbit_pitch),
                             -std::cos(v.orbit_yaw) * cp};
        const glm::vec3 to = glm::normalize(target - eye);
        CHECK(glm::dot(look, to) == doctest::Approx(1.0f).epsilon(1e-4));
    }
}

TEST_CASE("the zoom stays inside the model's own band and the reset returns") {
    const glm::vec3 lo{-0.3f, 0.0f, -0.3f};
    const glm::vec3 hi{0.3f, 1.8f, 0.3f};
    dfn::app::ViewerView v = dfn::app::viewer_reset(ViewerSource::Shelf, lo, hi,
                                                    1.309f, 1.7f);
    const float fit = v.fit_dist_m;
    CHECK(v.dist_m == doctest::Approx(fit));

    for (int i = 0; i < 200; ++i) {
        dfn::app::viewer_zoom(v, +1.0f);
    }
    CHECK(v.dist_m >= fit * dfn::app::VIEWER_ZOOM_MIN_FACTOR - 1e-4f);
    CHECK(v.dist_m < fit); // the wheel did move it
    for (int i = 0; i < 400; ++i) {
        dfn::app::viewer_zoom(v, -1.0f);
    }
    CHECK(v.dist_m <= fit * dfn::app::VIEWER_ZOOM_MAX_FACTOR + 1e-4f);

    // R returns exactly the portrait pose, whatever the hand did to it.
    dfn::app::viewer_orbit(v, 400.0f, -220.0f, 0.002f);
    const dfn::app::ViewerView back = dfn::app::viewer_reset(ViewerSource::Shelf, lo, hi,
                                                             1.309f, 1.7f);
    CHECK(back.dist_m == doctest::Approx(fit));
    CHECK(back.orbit_yaw == doctest::Approx(dfn::app::VIEWER_START_YAW));
    CHECK(back.orbit_pitch == doctest::Approx(dfn::app::VIEWER_START_PITCH));
    CHECK(back.model_yaw == doctest::Approx(0.0f));
}

TEST_CASE("the orbit never reaches the pole") {
    dfn::app::ViewerView v{};
    for (int i = 0; i < 500; ++i) {
        dfn::app::viewer_orbit(v, 0.0f, 100.0f, 0.01f);
    }
    CHECK(v.orbit_pitch > -1.5708f);
    for (int i = 0; i < 1000; ++i) {
        dfn::app::viewer_orbit(v, 0.0f, -100.0f, 0.01f);
    }
    CHECK(v.orbit_pitch < 1.5708f);
}

TEST_CASE("the target is the middle of what stands on the pad") {
    const glm::vec3 pad{40.0f, 25.5f, 40.0f};
    // Modelled from the feet: lo.y == 0.
    const glm::vec3 t1 = dfn::app::viewer_target(pad, {-0.3f, 0.0f, -0.3f},
                                                 {0.3f, 1.8f, 0.3f}, 1.0f);
    CHECK(t1.y == doctest::Approx(25.5f + 0.9f));
    // Modelled from the middle: lo.y == -0.9. The eye must land in the SAME
    // place — the model is put on the pad by its own bottom either way.
    const glm::vec3 t2 = dfn::app::viewer_target(pad, {-0.3f, -0.9f, -0.3f},
                                                 {0.3f, 0.9f, 0.3f}, 1.0f);
    CHECK(t2.y == doctest::Approx(t1.y));
    CHECK(t2.x == doctest::Approx(pad.x));
    CHECK(t2.z == doctest::Approx(pad.z));
}
