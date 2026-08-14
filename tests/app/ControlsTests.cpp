/*
Created: 14:08:2026 - 19:22:10
Last updated: 14:08:2026 - 19:22:10
Module: tests/app
File: tests/app/ControlsTests.cpp

Responsibility:
- Holds the key binding table TOTAL and UNAMBIGUOUS, which is what makes the
  controls screen a mechanism instead of a paragraph of documentation that goes
  stale.

Dependencies:
- Uses: engine/app Controls + Localization, doctest.
- Used by: ctest.

Notes:
- WHAT THIS SUITE CAN AND CANNOT DO, said plainly because the difference is the
  whole value. It cannot read App.cpp and check that every handler is listed --
  App.cpp owns a window and is not testable. What it does instead is hold the
  invariant that makes that check unnecessary: App dispatches through
  action_pressed(Action), so a handler MUST name an Action, an Action MUST have
  a row (the totality case below), and a row MUST carry a description that
  resolves (the localization case). A key added without a row does not draw
  wrong -- it does not fire at all, which is the loudest failure available and
  the one a human notices in the first second of using it.
- THE LOCALIZATION CASE IS NOT A FORMALITY. A missing key resolves to
  "?<0x...>?" rather than to empty, so an untranslated row would draw as noise
  on the very screen the user opens to find out what a key does.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 14:08:2026 - 19:22:10: Создан вместе с таблицей привязок — экран управления
  (просьба пользователя) и защита от его расхождения с кодом.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/Controls.h"
#include "engine/app/sources/Localization.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/BitmapFont.h"

#include <set>
#include <string>
#include <string_view>

using dfn::app::Action;
using dfn::app::Binding;
using dfn::app::Scope;

namespace {

bool table_loaded() {
    static const bool ok = dfn::app::load_localization(
        "games/daggerfall_n/assets/localization/ru.txt");
    return ok;
}

std::string_view text(const char* key) {
    return dfn::app::localized(dfn::serialization::fnv1a64(key));
}

// Two scopes can share a key only when they can never both be listening.
bool scopes_overlap(Scope a, Scope b) {
    if (a == Scope::Anywhere || b == Scope::Anywhere) {
        return true;
    }
    return a == b;
}

} // namespace

TEST_CASE("the binding table is total and in Action order") {
    const auto rows = dfn::app::control_bindings();

    // TOTAL: one row per Action, no gaps. This is the case that turns "did
    // someone add a key and forget the screen" into a question the build
    // answers -- an Action with no row cannot be dispatched.
    REQUIRE(rows.size() == static_cast<size_t>(Action::Count));

    // IN ORDER, which is a separate claim and the sneakier one: binding_for()
    // indexes the table by the Action's value, so a table that is complete but
    // MISORDERED hands every caller someone else's key. On screen that looks
    // completely normal -- the list is full, every row has a key and a
    // description -- and the game simply responds to the wrong buttons.
    for (size_t i = 0; i < rows.size(); ++i) {
        CAPTURE(i);
        CHECK(static_cast<size_t>(rows[i].action) == i);
        CHECK(&dfn::app::binding_for(static_cast<Action>(i)) == &rows[i]);
    }
}

TEST_CASE("no two actions answer to the same key in the same place") {
    const auto rows = dfn::app::control_bindings();

    for (size_t i = 0; i < rows.size(); ++i) {
        for (size_t j = i + 1; j < rows.size(); ++j) {
            if (!scopes_overlap(rows[i].scope, rows[j].scope)) {
                continue; // they are never both listening
            }
            CAPTURE(i);
            CAPTURE(j);
            CHECK(rows[i].key != rows[j].key);
            // Aliases collide just as hard as primaries: F3 firing two actions
            // is the same defect as 2 firing two actions.
            if (rows[i].alias != dfn::platform::Key::UNKNOWN) {
                CHECK(rows[i].alias != rows[j].key);
                CHECK(rows[i].alias != rows[j].alias);
            }
            if (rows[j].alias != dfn::platform::Key::UNKNOWN) {
                CHECK(rows[j].alias != rows[i].key);
            }
        }
    }
}

TEST_CASE("every row can be drawn: a name for the key, a sentence for the deed") {
    REQUIRE(table_loaded());

    for (const Binding& b : dfn::app::control_bindings()) {
        CAPTURE(static_cast<int>(b.action));

        // The key's printed label. "?" is the table's own "I have no name for
        // this key" marker, and a screen that says "? — Каркас" is useless.
        const std::string name = dfn::app::key_name(b.key);
        CHECK(name != "?");
        CHECK_FALSE(name.empty());
        if (b.alias != dfn::platform::Key::UNKNOWN) {
            CHECK(std::string(dfn::app::key_name(b.alias)) != "?");
        }

        // The description. A miss resolves to "?<0x...>?" rather than to empty,
        // so it would draw as noise on the one screen whose entire job is to
        // explain -- checked by the marker, not by comparing to a literal
        // Russian string, which would be the Rule 5 violation this module
        // exists to prevent.
        const std::string_view what = text(b.what);
        CHECK_FALSE(what.empty());
        CHECK(what.front() != '?');
    }

    // The fly camera's rows are documentation, but they are documentation the
    // same screen draws, so they answer to the same standard.
    for (const auto& m : dfn::app::movement_rows()) {
        CHECK(text(m.keys).front() != '?');
        CHECK(text(m.what).front() != '?');
    }
}

TEST_CASE("the keys the user named are the keys the table binds") {
    // THE CONTROL FOR ALL THREE CASES ABOVE, and it is the one that keeps them
    // from being self-referential. Everything else here asks whether the table
    // is CONSISTENT -- a table that bound every action to the letter Z would
    // pass totality, pass ordering, and pass localization. This asks whether it
    // is RIGHT, against the list the user actually gave, and it is written out
    // by hand on purpose: a control derived from the thing under test is not a
    // control.
    using K = dfn::platform::Key;
    const auto& b = dfn::app::binding_for(Action::ThirdPerson);
    CHECK(b.key == K::NUM_1);
    CHECK(dfn::app::binding_for(Action::DebugReadout).key == K::NUM_2);
    CHECK(dfn::app::binding_for(Action::DebugReadout).alias == K::F3);
    CHECK(dfn::app::binding_for(Action::StateCapture).key == K::NUM_3);
    CHECK(dfn::app::binding_for(Action::StateCapture).alias == K::F2);
    CHECK(dfn::app::binding_for(Action::Wireframe).key == K::NUM_4);
    CHECK(dfn::app::binding_for(Action::Wireframe).alias == K::F4);
    CHECK(dfn::app::binding_for(Action::Screenshot).key == K::NUM_5);
    CHECK(dfn::app::binding_for(Action::ToggleBody).key == K::TAB);
    CHECK(dfn::app::binding_for(Action::TrajectoryRecord).key == K::R);
    CHECK(dfn::app::binding_for(Action::TrajectoryReplay).key == K::P);
    CHECK(dfn::app::binding_for(Action::ChatWindow).key == K::SLASH);
    CHECK(dfn::app::binding_for(Action::QuickRemark).key == K::ENTER);
    CHECK(dfn::app::binding_for(Action::MenuPause).key == K::ESCAPE);

    // The two that are NOT everywhere, because the screen says so and a wrong
    // scope note is a wrong instruction: recording a trajectory is an editor
    // tool, and third person is something you can only leave a body into.
    CHECK(dfn::app::binding_for(Action::TrajectoryRecord).scope == Scope::EditorOnly);
    CHECK(dfn::app::binding_for(Action::TrajectoryReplay).scope == Scope::EditorOnly);
    CHECK(dfn::app::binding_for(Action::ThirdPerson).scope == Scope::PlayingOnly);
}

TEST_CASE("the whole list fits on every screen the settings page offers") {
    // THIS CASE EXISTS BECAUSE A FRAME CAUGHT WHAT THE SUITE DID NOT. The first
    // controls page tightened its row pitch as the frame got shorter, which
    // looked like it handled small screens; it does not, because the pitch
    // cannot go below the glyph height without rows printing into each other.
    // At 320x180 the last two rows ran off the bottom edge and the footer hint
    // landed on top of a row. Every assertion here was green at the time,
    // because the layout lived inside the draw where nothing could read it.
    struct Res {
        int w;
        int h;
    };
    for (const Res r : {Res{1280, 720}, Res{960, 540}, Res{640, 360}, Res{320, 180}}) {
        CAPTURE(r.w);
        CAPTURE(r.h);
        const auto L = dfn::app::controls_layout(r.w, r.h);

        // The block ends inside the frame, and it says so itself.
        CHECK(L.fits);
        CHECK(L.bottom <= r.h);
        CHECK(L.first_y > L.title_y);

        // EVERY ROW IS DRAWN. The layout may drop its headings and its footer
        // when the frame is short -- those are chrome -- but a controls screen
        // that quietly omits controls would be worse than a cramped one, and
        // "it fits now" is exactly what dropping rows would achieve.
        const int rows = static_cast<int>(dfn::app::control_bindings().size()
                                          + dfn::app::movement_rows().size());
        CHECK(L.line_count >= rows);

        // Rows must not print into each other: the pitch is at least the ink.
        CHECK(L.row_h >= dfn::render::FONT_INK_H);
    }

    // THE CONTROL, and it is what stops "fits" from being a rubber stamp: on a
    // frame that genuinely cannot hold the list, it must report false rather
    // than shrink the rows into an unreadable stack and call it success.
    const auto tiny = dfn::app::controls_layout(320, 60);
    CHECK_FALSE(tiny.fits);
    CHECK(tiny.row_h >= dfn::render::FONT_INK_H); // still refuses to overlap

    // ...and the generous frame keeps the chrome the cramped one gives up,
    // which proves the two arrangements are really two.
    const auto roomy = dfn::app::controls_layout(1280, 720);
    CHECK(roomy.headings);
    CHECK(roomy.footer);
}
