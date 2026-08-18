/*
Created: 18:08:2026 - 16:59:18
Last updated: 18:08:2026 - 23:20:00
Module: engine/app
File: engine/app/sources/AppActions.cpp

Responsibility:
- The dispatch table itself. See AppActions.h for why it is a table and why it
  holds a name instead of a member pointer.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 18:08:2026 - 16:59:18: Создан вместе с заголовком — двадцать одна строка «действие →
  метод App», собранная из восемнадцати обработчиков, лежавших в run().
- 18:08:2026 - 18:58:40: Маршрут AxisLock -> on_axis_lock.
- 18:08:2026 - 20:26:30: Маршрут DeleteSelected -> on_delete_selected.
- 18:08:2026 - 23:20:00: Маршруты инструментов 6..9.
*/

#include "engine/app/sources/AppActions.h"

#include <array>

namespace dfn::app {
namespace {

// ORDER IS Action's ORDER, and the test holds it to that for the same reason
// Controls.cpp is held to it: route_for() indexes by the Action's value, so a
// complete but MISORDERED table hands every key someone else's handler -- and
// that looks entirely normal until you press something.
constexpr std::array<ActionRoute, static_cast<size_t>(Action::Count)> ROUTES{{
    {Action::ThirdPerson, Gate::TypingEats, "on_third_person"},
    {Action::DebugReadout, Gate::TypingEats, "on_debug_readout"},
    {Action::StateCapture, Gate::TypingEats, "on_state_capture"},
    {Action::Wireframe, Gate::TypingEats, "on_wireframe"},
    {Action::Screenshot, Gate::TypingEats, "on_screenshot"},
    {Action::ToggleBody, Gate::TypingEats, "on_toggle_body"},
    {Action::TrajectoryRecord, Gate::TypingEats, "on_trajectory_record"},
    {Action::TrajectoryReplay, Gate::TypingEats, "on_trajectory_replay"},
    {Action::ChatWindow, Gate::TypingEats, "on_chat_window"},
    {Action::QuickRemark, Gate::TypingEats, "on_quick_remark"},
    {Action::Map, Gate::TypingEats, "on_map"},
    {Action::MenuPause, Gate::TypingEats, "on_menu_pause"},
    {Action::Fullscreen, Gate::TypingEats, "on_fullscreen"},
    {Action::CursorToggle, Gate::TypingEats, "on_cursor_toggle"},
    {Action::BuildMenu, Gate::TypingEats, "on_build_menu"},
    {Action::BuildRotate, Gate::TypingEats, "on_build_rotate"},
    // ПЯТЬ КЛАВИШ — ОДИН МЕТОД И ПЯТЬ НОМЕРОВ. Имени инструмента здесь нет:
    // клавиша называет НОМЕР фишки на полосе, а какой это инструмент, знает
    // ящик (docs/AUDIT_EDITOR_TOOLS.md). Поэтому однозначность судится по ПАРЕ
    // «метод + номер», а не по имени метода.
    {Action::ToolHeight, Gate::TypingEats, "on_tool_pick", 0},
    {Action::ToolPaint, Gate::TypingEats, "on_tool_pick", 1},
    {Action::ToolSelect, Gate::TypingEats, "on_tool_pick", 2},
    {Action::ToolPlace, Gate::TypingEats, "on_tool_pick", 3},
    {Action::ToolLook, Gate::TypingEats, "on_tool_pick", 4},
    {Action::Tool6, Gate::TypingEats, "on_tool_pick", 5},
    {Action::Tool7, Gate::TypingEats, "on_tool_pick", 6},
    {Action::Tool8, Gate::TypingEats, "on_tool_pick", 7},
    {Action::Tool9, Gate::TypingEats, "on_tool_pick", 8},
    // ОТМЕНА И ПОВТОР — ОДНА строка: отличает их модификатор, а не клавиша.
    {Action::Undo, Gate::TypingEats, "on_undo_redo", 0},
    {Action::AxisLock, Gate::TypingEats, "on_axis_lock", 0},
    {Action::DeleteSelected, Gate::TypingEats, "on_delete_selected", 0},
}};

} // namespace

std::span<const ActionRoute> action_routes() { return ROUTES; }

const ActionRoute& route_for(Action action) {
    // Clamped rather than reported, like binding_for(): a caller passing
    // Action::Count is a programming error, and returning a valid row keeps a
    // mis-call from dispatching a WILD handler, which is the worse outcome.
    const size_t i = static_cast<size_t>(action);
    return ROUTES[i < ROUTES.size() ? i : 0];
}

} // namespace dfn::app
