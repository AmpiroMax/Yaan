/*
Created: 13:08:2026 - 19:44:00
Last updated: 13:08:2026 - 20:05:00
Module: tests/app
File: tests/app/MenuTests.cpp

Responsibility:
- Proves the settings page behaves like a settings page rather than like a list
  that happens to draw: every row lands on a LEGAL value, every exit saves, and
  the "needs a restart" warning fires for the rows that need one and stays
  silent for the rows that do not.

Dependencies:
- Uses: engine/app Menu (model only -- no canvas, no window), doctest.
- Used by: ctest.

Notes:
- EVERY CASE SHIPS ITS CONTROL (Rule 30), and here the controls are the ones
  that would catch the plausible bug rather than the impossible one:
  * the restart warning's control is a LIVE setting (head_bob). A warning that
    fires for everything is the same as no warning, and it is the easy bug.
  * the ladder's control is a row that is NOT a value (Back): adjust() there
    must do nothing, or "Enter cycles the value" would eat the exit.
  * the calibration page's control is entering it from the ROOT: a return that
    always goes back to settings would pass the settings case and strand
    everyone who came from the start screen.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone ui owns this file.
*/
/*
UPD:
- 13:08:2026 - 19:44:00: Created with the settings page.
- 13:08:2026 - 19:50:00: Случай про паузу: настройки достижимы с неё и возвращают
  в неё, контроль — вход с корня.
- 13:08:2026 - 20:05:00: Метки времени приведены к часам — были написаны вперёд.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/Menu.h"

using dfn::app::MenuAction;
using dfn::app::MenuModel;
using dfn::app::MenuPage;
using dfn::app::MenuSettings;

namespace {

// The page as the app hands it over: what the game is running with.
MenuModel launched() {
    MenuModel m;
    MenuSettings s;
    s.internal_w = 640;
    s.internal_h = 360;
    s.msaa = 4;
    s.palette = false;
    s.head_bob = 1.0f;
    m.set_settings(s);
    m.open(MenuPage::Settings);
    return m;
}

// Rows, in the order the page draws them.
constexpr int ROW_RESOLUTION = 0;
constexpr int ROW_MSAA = 1;
constexpr int ROW_PALETTE = 2;
constexpr int ROW_HEAD_BOB = 3;
constexpr int ROW_BRIGHTNESS = 4;
constexpr int ROW_BACK = 5;

void select(MenuModel& m, int row) {
    m.open(MenuPage::Settings); // selection resets to 0
    for (int i = 0; i < row; ++i) {
        m.move(1);
    }
}

bool legal_resolution(const MenuSettings& s) {
    return (s.internal_w == 320 && s.internal_h == 180)
        || (s.internal_w == 640 && s.internal_h == 360)
        || (s.internal_w == 960 && s.internal_h == 540)
        || (s.internal_w == 1280 && s.internal_h == 720);
}

} // namespace

TEST_CASE("settings rows land only on legal values, and wrap") {
    MenuModel m = launched();
    select(m, ROW_RESOLUTION);
    for (int i = 0; i < 9; ++i) { // more presses than rungs: the wrap is the point
        m.adjust(+1);
        CHECK(legal_resolution(m.settings()));
    }
    // Four rungs, so four presses return to where it started.
    const MenuSettings before = m.settings();
    for (int i = 0; i < 4; ++i) {
        m.adjust(+1);
    }
    CHECK(m.settings().internal_w == before.internal_w);
    CHECK(m.settings().internal_h == before.internal_h);

    select(m, ROW_MSAA);
    for (int i = 0; i < 9; ++i) {
        m.adjust(-1); // backwards too: a ladder with one working direction is half a ladder
        const uint32_t v = m.settings().msaa;
        CHECK((v == 0 || v == 2 || v == 4 || v == 8));
    }
}

TEST_CASE("a hand-edited settings.cfg value lands on the nearest legal rung") {
    MenuModel m;
    MenuSettings s;
    s.msaa = 3;         // nobody offers 3; a text editor does
    s.head_bob = 0.77f; // nor 0.77
    m.set_settings(s);
    m.open(MenuPage::Settings);

    select(m, ROW_MSAA);
    m.adjust(+1);
    const uint32_t v = m.settings().msaa;
    CHECK((v == 0 || v == 2 || v == 4 || v == 8));

    select(m, ROW_HEAD_BOB);
    m.adjust(+1);
    const float b = m.settings().head_bob;
    CHECK(b >= 0.0f);
    CHECK(b <= 2.0f);
    // On a quarter-step grid: 0, 0.5, 1, 1.5, 2 are all multiples of 0.5.
    CHECK(doctest::Approx(b * 2.0f).epsilon(1e-4) == static_cast<float>(static_cast<int>(b * 2.0f + 0.5f)));
}

TEST_CASE("the restart warning fires for the renderer's rows and not for the live one") {
    MenuModel m = launched();
    CHECK_FALSE(m.needs_restart()); // nothing turned yet

    select(m, ROW_HEAD_BOB);
    m.adjust(+1);
    CHECK(m.settings().head_bob != 1.0f);
    CHECK_FALSE(m.needs_restart()); // THE CONTROL: a live setting must stay silent

    select(m, ROW_RESOLUTION);
    m.adjust(+1);
    CHECK(m.needs_restart());

    // ...and it goes quiet again when the row is turned back to what launched.
    for (int i = 0; i < 3; ++i) {
        m.adjust(+1);
    }
    CHECK_FALSE(m.needs_restart());
}

TEST_CASE("every exit from the settings page saves") {
    MenuModel m = launched();
    select(m, ROW_BACK);
    CHECK(m.activate() == MenuAction::SettingsDone);
    CHECK(m.page() == MenuPage::Root);

    m = launched();
    CHECK(m.back() == MenuAction::SettingsDone); // Escape, from any row
    CHECK(m.page() == MenuPage::Root);
}

TEST_CASE("Enter on a value row is the same verb as right, and Back is not a value") {
    MenuModel m = launched();
    select(m, ROW_PALETTE);
    const bool before = m.settings().palette;
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.settings().palette != before);
    CHECK(m.page() == MenuPage::Settings); // still here: Enter turned a dial

    // THE CONTROL: adjust() on a row that is not a value must be a no-op, or
    // "Enter cycles" would have eaten the exit.
    select(m, ROW_BACK);
    const MenuSettings snapshot = m.settings();
    m.adjust(+1);
    CHECK(m.settings().internal_w == snapshot.internal_w);
    CHECK(m.settings().msaa == snapshot.msaa);
    CHECK(m.settings().palette == snapshot.palette);
    CHECK(m.settings().head_bob == snapshot.head_bob);
}

TEST_CASE("the calibration page returns to whichever page opened it") {
    MenuModel m = launched();
    select(m, ROW_BRIGHTNESS);
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.page() == MenuPage::Calibrate);
    CHECK(m.back() == MenuAction::CalibrationDone);
    CHECK(m.page() == MenuPage::Settings);

    // THE CONTROL: opened from anywhere else, it goes back to the root.
    MenuModel r;
    r.open(MenuPage::Calibrate);
    CHECK(r.activate() == MenuAction::CalibrationDone);
    CHECK(r.page() == MenuPage::Root);
}

TEST_CASE("the brightness dial cannot leave its range from either side") {
    MenuModel m;
    m.set_black_floor(999.0f);
    CHECK(m.black_floor() == doctest::Approx(dfn::app::black_floor_max()));
    m.set_black_floor(-1.0f);
    CHECK(m.black_floor() == doctest::Approx(0.0f));

    // ...including through the page's own keys: up is brighter, and holding it
    // stops at the ceiling rather than running past it.
    m.open(MenuPage::Calibrate);
    for (int i = 0; i < 100; ++i) {
        m.move(-1);
    }
    CHECK(m.black_floor() == doctest::Approx(dfn::app::black_floor_max()));
    for (int i = 0; i < 100; ++i) {
        m.move(+1);
    }
    CHECK(m.black_floor() == doctest::Approx(0.0f));
}

TEST_CASE("settings are reachable from pause, and come back to pause") {
    MenuModel m;
    m.set_settings(MenuSettings{});
    m.open(MenuPage::Pause);
    CHECK(m.item_count() == 3); // resume, settings, quit
    m.move(1);
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.page() == MenuPage::Settings);
    CHECK(m.back() == MenuAction::SettingsDone);
    CHECK(m.page() == MenuPage::Pause); // NOT the start screen: the world is still there

    // And the pause page's other two rows still do what they did.
    m.open(MenuPage::Pause);
    CHECK(m.activate() == MenuAction::Resume);
    m.move(2);
    CHECK(m.activate() == MenuAction::Quit);

    // THE CONTROL: entered from the root, it still returns to the root.
    MenuModel r;
    r.set_settings(MenuSettings{});
    r.open(MenuPage::Root);
    r.move(1);
    CHECK(r.activate() == MenuAction::None);
    CHECK(r.page() == MenuPage::Settings);
    CHECK(r.back() == MenuAction::SettingsDone);
    CHECK(r.page() == MenuPage::Root);
}
