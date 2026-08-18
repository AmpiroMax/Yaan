/*
Created: 18:08:2026 - 16:59:18
Last updated: 18:08:2026 - 16:59:18
Module: tests/app
File: tests/app/ActionRoutesTests.cpp

Responsibility:
- Holds the DISPATCH table total, unambiguous and closed against the chat
  window. Controls.h says which key an action answers to and ControlsTests
  holds that side; this holds the other one -- WHO answers, and whether
  anything can answer while the player is typing.

Dependencies:
- Uses: engine/app AppActions + Controls, doctest.
- Used by: ctest (app_controls).

Notes:
- WHY A SOURCE-READING CASE IS IN HERE, and it is not laziness. Two of the
  three claims this suite makes are about a table, and a table can be read.
  The third -- "there is exactly ONE place a key reaches the app" -- is a claim
  about App.cpp, which owns a window and cannot be instantiated by any test in
  this project. The choice was between checking it by reading the file and not
  checking it at all, and the defect it guards is the one the whole layer
  exists to end: a nineteenth handler written inline in run(), with its own
  hand-written `!chat_typing &&` in front of it, which no instrument can see.
  Reading the file is a weak instrument. It is not a missing one.
- THE COUNTERFACTUAL FOR EACH CASE IS CHEAP AND WAS RUN (Rule 30): flip one
  row's gate to TypingIgnores (case 3 reds), point two rows at one method with
  one arg (case 2 reds), swap two rows (case 1 reds), put an action_pressed()
  call back into App.cpp (case 4 reds). Numbers are in the commit message.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 18:08:2026 - 16:59:18: Создан. Слой 1 разбора App.cpp: рукав на таблицу диспетчеризации.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/AppActions.h"
#include "engine/app/sources/Controls.h"

#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>

using dfn::app::Action;
using dfn::app::ActionRoute;
using dfn::app::Gate;

namespace {

std::string read_file(const char* path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// How many times `needle` occurs in `hay`. Counting rather than testing for
// presence, because "App.cpp mentions action_pressed once" (its own
// definition) and "App.cpp dispatches keys again" differ only by a number.
std::size_t count_of(const std::string& hay, const std::string& needle) {
    std::size_t n = 0;
    for (std::size_t at = hay.find(needle); at != std::string::npos;
         at = hay.find(needle, at + needle.size())) {
        ++n;
    }
    return n;
}

} // namespace

TEST_CASE("every action has a route, and the routes are in Action order") {
    const auto routes = dfn::app::action_routes();

    // TOTAL. This is what makes "did someone add a key and forget to say what
    // it does" a question the build answers: dispatch_actions() walks THIS
    // table, so an Action with no row is an Action that never fires.
    REQUIRE(routes.size() == static_cast<std::size_t>(Action::Count));

    // IN ORDER, the sneakier claim: route_for() indexes by the Action's value,
    // so a table that is complete but misordered hands every key someone
    // else's handler. Nothing about that looks wrong until a key is pressed.
    for (std::size_t i = 0; i < routes.size(); ++i) {
        CAPTURE(i);
        CHECK(static_cast<std::size_t>(routes[i].action) == i);
        CHECK(&dfn::app::route_for(static_cast<Action>(i)) == &routes[i]);
    }

    // AND THE TWO TABLES DESCRIBE THE SAME SET. A row in one and not the other
    // is either a key nobody handles or a handler nobody can reach.
    CHECK(routes.size() == dfn::app::control_bindings().size());
}

TEST_CASE("no two actions share one handler call") {
    const auto routes = dfn::app::action_routes();
    std::set<std::pair<std::string, int>> seen;

    for (const ActionRoute& r : routes) {
        CAPTURE(static_cast<int>(r.action));
        REQUIRE(r.handler != nullptr);
        const std::string name = r.handler;
        CHECK_FALSE(name.empty());
        // The name is bound to a method by a switch in AppInput.cpp, and the
        // convention is what lets the source case below find it.
        CHECK(name.rfind("on_", 0) == 0);

        // UNAMBIGUOUS ON THE PAIR, not on the name. The five tool keys are one
        // method and five numbers on purpose -- the key names a POSITION on the
        // tool bar and the toolbox owns what lives there (docs/AUDIT_EDITOR_
        // TOOLS.md) -- so judging on the name alone would forbid the one shape
        // this table is meant to allow, and judging on nothing would permit two
        // actions that silently do the same thing.
        const auto key = std::make_pair(name, r.arg);
        CHECK(seen.insert(key).second);
    }
}

TEST_CASE("nothing reaches the app through an open chat window") {
    // THE DEFECT THIS REPLACES was eighteen copies of `!chat_typing &&`,
    // written by hand in front of eighteen handlers spread over 350 lines of
    // run(). Eighteen correct copies are not an invariant, they are a habit --
    // and the cost of the nineteenth is that typing a message toggles third
    // person, drops snapshots and steers the camera. The physical keys DO
    // still fire while the window is up (text_input() collects codepoints
    // beside them, it does not consume them), so this is a real decision and
    // not a formality.
    //
    // Written as "the exceptions are none" rather than "the field exists", so
    // an action that opts out has to argue for itself in a diff.
    int escapes = 0;
    for (const ActionRoute& r : dfn::app::action_routes()) {
        CAPTURE(static_cast<int>(r.action));
        CHECK(r.gate == Gate::TypingEats);
        if (r.gate != Gate::TypingEats) {
            ++escapes;
        }
    }
    CHECK(escapes == 0);
}

TEST_CASE("there is exactly one place a key reaches the app") {
    // SEE THE HEADER NOTE for why this reads files. Short version: the claim
    // is about App.cpp, App.cpp owns a window, and the alternative to a weak
    // instrument here is no instrument at all.
    const std::string app = read_file("engine/app/sources/App.cpp");
    const std::string input = read_file("engine/app/sources/AppInput.cpp");
    REQUIRE_FALSE(app.empty());  // wrong working directory would silently pass
    REQUIRE_FALSE(input.empty());

    // App.cpp may DEFINE action_pressed and must not CALL it: every key edge
    // now goes through dispatch_actions(), which is the single walk.
    CHECK(count_of(app, "action_pressed(") == 1);
    CHECK(count_of(app, "bool App::action_pressed(Action action) const") == 1);
    CHECK(count_of(app, "dispatch_actions(") == 1);

    // ...and the one walk really is in the file that holds the handlers.
    CHECK(count_of(input, "bool App::dispatch_actions(bool chat_typing)") == 1);
    CHECK(count_of(input, "action_pressed(r.action)") == 1);

    // EVERY NAME IN THE TABLE IS A METHOD THAT EXISTS, and every one of them is
    // dispatched. A row naming a method nobody wrote would not compile; a row
    // whose `case` label was forgotten WOULD compile and would simply never
    // fire, which is the failure this pair catches.
    std::set<std::string> handlers;
    for (const ActionRoute& r : dfn::app::action_routes()) {
        handlers.insert(r.handler);
    }
    for (const std::string& h : handlers) {
        CAPTURE(h);
        CHECK(count_of(input, "void App::" + h + "(") == 1);
        CHECK(count_of(input, h + "(") >= 2); // the definition and the call
    }
}
