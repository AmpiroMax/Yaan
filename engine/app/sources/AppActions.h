/*
Module: engine/app
File: engine/app/sources/AppActions.h

Responsibility:
- THE DISPATCH TABLE: one row per Action, naming the App method that performs
  it and whether the chat window eats the key. Controls.h says which KEY an
  action answers to; this says WHO answers, and it is the only place that
  knows.

Key items:
- Gate: does the open chat window swallow this key.
- ActionRoute / action_routes() / route_for(): the table and its lookups.

Dependencies:
- Uses: engine/app/sources/Controls.h (Action only). Nothing else -- no App,
  no window, which is the whole reason it is a separate translation unit.
- Used by: AppInput.cpp (dispatch), tests/app/ActionRoutesTests.cpp.

Notes:
- WHY THE HANDLER IS A NAME AND NOT A POINTER. A table of `void (App::*)()`
  would be prettier and would drag App.cpp into every target that links it --
  and App.cpp owns a window, so the table would become untestable in exactly
  the way this refactor exists to end. The name is bound to the method by a
  switch in AppInput.cpp with no `default:` label, so the compiler, not a
  reader, is what makes the binding total.
- WHY A GATE FIELD AT ALL WHEN EVERY ROW SAYS THE SAME THING TODAY. Because
  before this table the same word was written EIGHTEEN TIMES by hand as
  `!chat_typing &&` in front of eighteen handlers scattered through run(), and
  the nineteenth is the one nobody writes. The claim "typing a message cannot
  toggle third person, drop snapshots or steer the camera" was, until now, a
  claim about eighteen separate expressions that no instrument could read.
  Here it is one column, and tests/app/ActionRoutesTests.cpp reads it.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ADDING A KEY? Controls.h gets the binding, this gets the route, AppInput.cpp
  gets the method. Miss any one of the three and it does not compile or does
  not dispatch -- never "works but is undocumented".
*/

#pragma once

#include "engine/app/sources/Controls.h"

#include <cstdint>
#include <span>

namespace dfn::app {

// DOES THE OPEN CHAT WINDOW SWALLOW THIS KEY. While the window is up the
// physical keys still fire was_pressed() -- text_input() collects codepoints
// beside them, it does not consume them -- so every gameplay key has to be
// held back explicitly or typing "hi" toggles the readout and drops a capture.
enum class Gate : uint8_t {
    // Not delivered while the chat window has the keyboard. This is what every
    // row is today, and the test says so, so a row that opts out has to argue
    // for itself in a diff rather than slip in as one missing `!chat_typing`.
    TypingEats = 0,
    // Delivered even while typing. Nothing uses it; it exists so that the
    // claim above is a MEASUREMENT of the table rather than a property of a
    // type with one value.
    TypingIgnores,
};

// ONE ROW PER ACTION. `handler` is the App method's name and `arg` is what it
// is called with -- the five tool keys are one method and five numbers, which
// is why uniqueness is judged on the PAIR and not on the name alone.
struct ActionRoute {
    Action action;
    Gate gate;
    const char* handler; // "on_fullscreen", ... — defined in AppInput.cpp
    int arg = 0;         // the tool index for on_tool_pick; 0 elsewhere
};

// The table, indexed so that action_routes()[i].action == Action(i).
[[nodiscard]] std::span<const ActionRoute> action_routes();
[[nodiscard]] const ActionRoute& route_for(Action action);

} // namespace dfn::app
