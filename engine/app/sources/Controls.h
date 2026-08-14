/*
Created: 14:08:2026 - 19:22:10
Last updated: 14:08:2026 - 19:22:10
Module: engine/app
File: engine/app/sources/Controls.h

Responsibility:
- THE key binding table: every discrete action the app dispatches on a key
  edge, the key(s) that trigger it, where it applies, and the localization key
  describing it. One table, read by BOTH the input handlers and the controls
  screen.

Key items:
- Action: the closed set of key-dispatched actions.
- Binding / control_bindings() / binding_for(): the table and its lookups.
- MovementRow / movement_rows(): the continuous fly-mode inputs, described.

Dependencies:
- Uses: engine/platform/input (Key only).
- Used by: App (dispatch), Menu (the controls page), tests/app.

Notes:
- WHY A TABLE AND NOT A DRAWN LIST. The user asked to be able to LOOK at the
  controls, and a screen that lists them is easy; a screen that lists them
  CORRECTLY a month from now is not. A hand-written list drifts the first time
  somebody binds a key in App.cpp and does not think about a menu page -- and
  it drifts SILENTLY, because a wrong help screen looks exactly like a right
  one. So the list is not a copy of the bindings, it IS the bindings: App asks
  this table for the key belonging to an Action instead of naming a key
  literal, which means a new key cannot be dispatched without a row here, and
  a row here cannot exist without a description. The test then only has to
  check that the table is total and unambiguous -- see tests/app/ControlsTests.
- KEY NAMES ARE NOT PROSE (the same reading of Rule 5 the debug readout uses):
  "F3", "Tab", "1" are what is physically printed on the key and are read the
  same in every language, so key_name() returns them directly. Everything that
  is a SENTENCE -- what the key does, and the names of the movement inputs --
  goes through localization like all other user-facing text.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ADDING A KEY? Add its Action here and give it a row. App.cpp must dispatch it
  through action_pressed(), never through a Key literal -- a literal is how the
  screen starts lying.
*/
/*
UPD:
- 14:08:2026 - 19:22:10: Создан. Экран управления (просьба пользователя: «я должен
  уметь посмотреть на это в настройках управления»), заведённый так, чтобы
  список не мог разъехаться с кодом: обработчики спрашивают клавишу ПО
  ДЕЙСТВИЮ, поэтому новая клавиша без строки в таблице просто не диспатчится.
*/

#pragma once

#include "engine/platform/input/interfaces/IInput.h"

#include <cstdint>
#include <span>

namespace dfn::app {

// EVERY ACTION THE APP DISPATCHES ON A KEY EDGE. Closed set on purpose: it is
// what makes "did anyone add a key without telling the screen" a question with
// an answer. Continuous input (walking, looking, the fly camera) is NOT here --
// it is not an edge, it is polled, and it is described by movement_rows().
enum class Action : uint8_t {
    ThirdPerson = 0,   // 1
    DebugReadout,      // 2 / F3
    StateCapture,      // 3 / F2
    Wireframe,         // 4 / F4
    Screenshot,        // 5
    ToggleBody,        // Tab
    TrajectoryRecord,  // R
    TrajectoryReplay,  // P
    ChatWindow,        // /
    QuickRemark,       // Enter
    Map,               // M
    MenuPause,         // Esc
    Count,
};

// Where an action applies. It is part of the table rather than a comment
// because two actions MAY share a key when their scopes do not overlap, and a
// test that did not know the scopes would have to either miss that or forbid
// it.
enum class Scope : uint8_t {
    Anywhere,    // both modes
    EditorOnly,  // the free-camera viewer
    PlayingOnly, // in the body
};

struct Binding {
    Action action;
    platform::Key key;
    // The second key for the same action, or UNKNOWN. The aliases are real
    // history, not decoration: F2/F3/F4 appear in frames and recipes already
    // archived, so moving them silently would make every recipe on disk wrong.
    platform::Key alias;
    const char* what;  // localization key: what the action DOES
    Scope scope;
};

// The table, indexed so that control_bindings()[i].action == Action(i).
[[nodiscard]] std::span<const Binding> control_bindings();
[[nodiscard]] const Binding& binding_for(Action action);

// The physical label on the key ("1", "F3", "Tab", "Esc"). ASCII, not
// translated -- see the header note. Returns "?" for a key with no label,
// which is loud rather than blank.
[[nodiscard]] const char* key_name(platform::Key key);

// THE FLY CAMERA'S CONTINUOUS INPUTS. Polled every frame rather than dispatched
// on an edge, so they carry no Action and cannot be checked against a handler --
// they are documentation, and the table says so instead of pretending otherwise.
struct MovementRow {
    const char* keys;  // localization key for the key names (e.g. "WASD")
    const char* what;  // localization key for what they do
};
[[nodiscard]] std::span<const MovementRow> movement_rows();

// WHERE THE CONTROLS PAGE'S ROWS LAND, computed rather than drawn, so the
// question "does the list fit on the screen" has an answer a test can read.
//
// IT EXISTS BECAUSE THE FIRST VERSION DID NOT FIT AND NOTHING SAID SO. The page
// was written with a row pitch that tightened as the frame got shorter, which
// looked like it handled small screens and did not: the pitch cannot go below
// the glyph height without the rows printing into each other, so at 320x180 the
// last two rows ran off the bottom and the footer landed on top of a row. The
// frame showed it immediately; no assertion could, because the layout only
// existed inside the draw. Now the arithmetic is out here and tests/app checks
// it at every resolution the settings page offers.
//
// The two droppable parts are dropped in order of what costs least: the footer
// hint (Escape works whether or not it is advertised), then the section
// headings (they group rows that are already visually grouped). The ROWS are
// never dropped -- a controls screen missing controls is worse than a dense one.
struct ControlsLayout {
    int row_h = 0;        // pitch between rows
    int title_y = 0;
    int first_y = 0;      // top of the first drawn line
    int bottom = 0;       // y just past the last drawn line
    int line_count = 0;   // rows + whatever headings survived
    bool headings = true;
    bool footer = true;
    bool fits = true;     // false = it overflowed anyway, and the page says so
};
[[nodiscard]] ControlsLayout controls_layout(int width_px, int height_px);

} // namespace dfn::app
