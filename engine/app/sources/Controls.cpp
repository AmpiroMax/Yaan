/*
Created: 14:08:2026 - 19:22:10
Last updated: 17:08:2026 - 16:27:55
Module: engine/app
File: engine/app/sources/Controls.cpp

Responsibility:
- The binding table itself. See the header for why it is a table.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 14:08:2026 - 19:22:10: Создан вместе с заголовком — таблица привязок, из которой
  и диспатчатся клавиши, и рисуется экран управления.
- 17:08:2026 - 16:27:55: F11 в таблице и в key_name (тест поймал отсутствие подписи).
*/

#include "engine/app/sources/Controls.h"

#include "engine/render/sources/BitmapFont.h"

#include <algorithm>
#include <array>

namespace dfn::app {
namespace {

using K = platform::Key;

// THE TABLE. Order is Action's order, and the test holds it to that: an
// out-of-order row would make binding_for() return someone else's key, which
// is the one bug in here that would look completely normal on screen.
//
// The order the ROWS are in is also the order the screen draws, so it is
// grouped the way the user learns them: what you look at, then what you record,
// then where you are, then how you leave.
constexpr std::array<Binding, static_cast<size_t>(Action::Count)> TABLE{{
    {Action::ThirdPerson, K::NUM_1, K::UNKNOWN, "controls.third_person", Scope::PlayingOnly},
    {Action::DebugReadout, K::NUM_2, K::F3, "controls.debug_readout", Scope::Anywhere},
    {Action::StateCapture, K::NUM_3, K::F2, "controls.state_capture", Scope::Anywhere},
    {Action::Wireframe, K::NUM_4, K::F4, "controls.wireframe", Scope::Anywhere},
    {Action::Screenshot, K::NUM_5, K::UNKNOWN, "controls.screenshot", Scope::Anywhere},
    {Action::ToggleBody, K::TAB, K::UNKNOWN, "controls.toggle_body", Scope::Anywhere},
    {Action::TrajectoryRecord, K::R, K::UNKNOWN, "controls.traj_record", Scope::EditorOnly},
    {Action::TrajectoryReplay, K::P, K::UNKNOWN, "controls.traj_replay", Scope::EditorOnly},
    {Action::ChatWindow, K::SLASH, K::UNKNOWN, "controls.chat", Scope::Anywhere},
    {Action::QuickRemark, K::ENTER, K::UNKNOWN, "controls.quick_remark", Scope::Anywhere},
    {Action::Map, K::M, K::UNKNOWN, "controls.map", Scope::Anywhere},
    {Action::MenuPause, K::ESCAPE, K::UNKNOWN, "controls.menu", Scope::Anywhere},
    {Action::Fullscreen, K::F11, K::UNKNOWN, "controls.fullscreen", Scope::Anywhere},
}};

// THE FLY CAMERA'S CONTINUOUS INPUTS, described rather than dispatched.
// READ OFF EditorCamera, NOT off the request that asked for this screen. The
// task described the fly controls as "WASD+QE, Space/Ctrl", which reads as two
// separate pairs; EditorCamera.h says what the code does -- E/Space are BOTH
// up and Q/Ctrl are BOTH down. A help screen copied from the description
// instead of the source is the exact failure this file exists to prevent, so
// it would have been an unusually poor place to introduce one.
constexpr std::array<MovementRow, 5> MOVEMENT{{
    {"controls.fly.move.keys", "controls.fly.move"},
    {"controls.fly.up.keys", "controls.fly.up"},
    {"controls.fly.down.keys", "controls.fly.down"},
    {"controls.fly.look.keys", "controls.fly.look"},
    {"controls.fly.speed.keys", "controls.fly.speed"},
}};

} // namespace

std::span<const Binding> control_bindings() { return TABLE; }

const Binding& binding_for(Action action) {
    // The caller passing Action::Count would be a programming error, not user
    // input, so it is clamped rather than reported: returning a valid row keeps
    // a mis-call from dispatching a WILD key, which is the worse outcome.
    const size_t i = static_cast<size_t>(action);
    return TABLE[i < TABLE.size() ? i : 0];
}

std::span<const MovementRow> movement_rows() { return MOVEMENT; }

ControlsLayout controls_layout(int width_px, int height_px) {
    (void)width_px; // the rows are two columns; only the HEIGHT is contended
    const int rows = static_cast<int>(TABLE.size() + MOVEMENT.size());

    ControlsLayout L;
    L.title_y = height_px / 12;

    // Tried in order, most generous first, taking the first arrangement whose
    // block ends above the bottom edge. Written as a loop over the two things
    // that may be given up rather than as nested ifs: the order of sacrifice is
    // then a list one can read and reorder, not a shape one has to infer.
    const bool wants[3][2] = {{true, true}, {true, false}, {false, false}};
    for (const auto& want : wants) {
        L.headings = want[0];
        L.footer = want[1];
        L.line_count = rows + (L.headings ? 2 : 0);
        L.first_y = L.title_y + render::FONT_CELL_H + (L.headings ? 6 : 4);
        const int floor_y =
            height_px - (L.footer ? render::FONT_CELL_H * 2 + 4 : 2);
        const int room = floor_y - L.first_y;
        // The pitch never goes below the glyph height: rows that overlap are
        // not a denser list, they are an unreadable one.
        L.row_h = std::max(render::FONT_INK_H,
                           std::min(render::FONT_CELL_H + 1,
                                    room / std::max(L.line_count, 1)));
        L.bottom = L.first_y + L.line_count * L.row_h;
        L.fits = L.bottom <= floor_y;
        if (L.fits) {
            break;
        }
    }
    return L;
}

const char* key_name(platform::Key key) {
    switch (key) {
    case K::NUM_1: return "1";
    case K::NUM_2: return "2";
    case K::NUM_3: return "3";
    case K::NUM_4: return "4";
    case K::NUM_5: return "5";
    case K::F2: return "F2";
    case K::F3: return "F3";
    case K::F4: return "F4";
    case K::F11: return "F11";
    case K::TAB: return "Tab";
    case K::ENTER: return "Enter";
    case K::ESCAPE: return "Esc";
    case K::SLASH: return "/";
    case K::M: return "M";
    case K::P: return "P";
    case K::R: return "R";
    default: return "?"; // loud, not blank: a nameless key is a table bug
    }
}

} // namespace dfn::app
