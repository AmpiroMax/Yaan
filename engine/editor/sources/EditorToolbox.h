/*
Module: engine/editor
File: engine/editor/sources/EditorToolbox.h

Responsibility:
- THE ONE PLACE THAT KNOWS WHICH TOOL IS IN HAND. Holds the tools, holds the
  single active pointer, routes the click to it and to nobody else, keeps the
  common reach ceiling, and owns the pointer/camera mode the user asked for on
  R. Nothing else in the program may hold that answer.

Key items:
- EditorToolbox::add(): a tool joins the bar by existing, not by being listed
  in a switch.
- click_icon() / click_settings(): the two halves of the user's double button,
  and they are DIFFERENT VERBS — the settings triangle never changes what is in
  hand («если у меня выбран один инструмент, я кликаю на настройки другого,
  инструмент не меняется в руках»).
- update(): one entry point per frame. Press, drag and release are derived HERE
  from the button state, so no caller can invent a second edge convention.
- reach ceiling: «я не должен уметь за 1000 км что-то строить» — the general
  parameter that lives under the gear, above every tool.
- pointer_mode(): R, «почти как в vim».

WHY THE EXCLUSIVITY IS STRUCTURAL AND NOT A CONVENTION. There is one
`active_` index. `update()` dispatches to `active()` and there is no loop over
tools anywhere in this class that could deliver a press to a second one. A tool
cannot subscribe to the button; the button belongs to whoever `active_` names.
The old arrangement had two owners (the placing hand on was_pressed, the brush
on is_down) and a THIRD condition arming one of them («или открыт список
объектов»), which is why one click both dug and built.

WHY on_deselected LIVES IN ONE FUNCTION. select() is the only writer of
`active_`, and it always calls on_deselected on the way out. The defect it
prevents is already in this repo's history: the ghost mesh was cleared at the
END of a function with three early returns, so putting the tool down left the
part hanging in the world.

Dependencies:
- Uses: EditorTool.h, std. NO ImGui, NO renderer, NO App: this class must be
  instantiable in a test with no window, because every property worth having
  here is invisible in a screenshot (Rule 3, Rule 27).
- Used by: EditorUi (owns one), EditorToolbar (draws it), engine/app (fills the
  tools and the world hooks), tests/app/EditorToolboxTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DO NOT add a second way to change the active tool. If a caller needs one, it
  needs click_icon(), which is the user's own gesture.
*/

#pragma once

#include "engine/editor/sources/EditorTool.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace dfn::app {

/// «Ничего в руке» — a real state, not the absence of one. The user asked for
/// it in the same breath as the tools: «если я кликну на иконку выбранного уже
/// инструмента, выбор сбросится... я буду просто бегать по игре». It replaces
/// the old `Look` tool, which was a tool that did nothing — a chip on the bar
/// that had to be selected in order to select nothing.
inline constexpr std::size_t NO_TOOL = static_cast<std::size_t>(-1);

/// THE COMMON CEILING ON REACH, in metres, and it is COMMON on purpose (user,
/// 18.08: «Добавь это как общий параметр вне какого-либо инструмента, он общий
/// для всех»). A per-tool number would be five numbers to keep sane and five
/// places to forget; a tool may still be shorter-armed than this, never longer.
inline constexpr float EDITOR_REACH_DEFAULT_M = 30.0f;
inline constexpr float EDITOR_REACH_MIN_M = 2.0f;
inline constexpr float EDITOR_REACH_MAX_M = 80.0f; ///< the aim ray's own march

/// What one frame of holding (or not holding) the button did. Returned rather
/// than printed: a test asserts on it, and the badge could report it.
struct ToolTickReport {
    bool pressed = false;
    bool dragged = false;
    bool released = false;
    /// The click was inside a tool's hands but OUT OF REACH. Separate from
    /// "nothing happened" because it is the user's complaint, and a tool that
    /// silently ignores a distant click is indistinguishable from a broken one.
    bool out_of_reach = false;
    /// The pointer was on a panel, or the editor was in pointer mode.
    bool blocked = false;
};

class EditorToolbox {
public:
    EditorToolbox() = default;
    EditorToolbox(const EditorToolbox&) = delete;
    EditorToolbox& operator=(const EditorToolbox&) = delete;

    // -- the shelf ------------------------------------------------------------

    /// Adds a tool and returns its index. Order is the order of the bar and the
    /// order of the number keys, so "press 3" and "the third button" are one
    /// thing.
    std::size_t add(std::unique_ptr<IEditorTool> tool);
    [[nodiscard]] std::size_t count() const { return tools_.size(); }
    [[nodiscard]] IEditorTool* at(std::size_t index) const;
    /// Index of the tool whose identity().id matches, or NO_TOOL.
    [[nodiscard]] std::size_t index_of(const char* id) const;
    void clear();

    // -- what is in hand ------------------------------------------------------

    [[nodiscard]] IEditorTool* active() const { return at(active_); }
    [[nodiscard]] std::size_t active_index() const { return active_; }

    /// THE USER'S GESTURE ON THE SQUARE: pick this tool, or — if it is already
    /// in hand — put it down. Both directions go through select().
    void click_icon(std::size_t index, ToolWorld& world);

    /// Put whatever is in hand down. The only other caller of select().
    void deselect(ToolWorld& world) { select(NO_TOOL, world); }

    // -- the settings triangle ------------------------------------------------

    /// THE USER'S GESTURE ON THE TRIANGLE: open this tool's settings, or close
    /// them. IT NEVER CHANGES WHAT IS IN HAND, and that is the whole point:
    /// «в меню настройки я настраиваю текущий инструмент... Но я не выбирал
    /// этот инструмент только настроил.»
    void click_settings(std::size_t index);
    [[nodiscard]] std::size_t settings_index() const { return settings_; }

    /// The gear: the parameters that belong to no tool (the reach ceiling).
    void click_gear();
    [[nodiscard]] bool common_settings_open() const { return common_settings_; }

    /// Anything open at all — what the settings window's visibility follows,
    /// and what ESC closes.
    [[nodiscard]] bool settings_open() const {
        return settings_ != NO_TOOL || common_settings_;
    }
    /// Returns whether anything was actually closed, so ESC can fall through to
    /// the pause menu when there was nothing to close.
    bool close_settings();

    // -- the pointer mode (R) -------------------------------------------------

    /// FALSE = the mouse belongs to the camera, TRUE = to the interface. Starts
    /// FALSE, which is the user's own wording: «изначально мышка к камере
    /// привязана, чтобы войти в режим, когда я могу выбирать инструменты и
    /// процесс, надо нажать на R и также нажать R чтобы выйти».
    [[nodiscard]] bool pointer_mode() const { return pointer_mode_; }
    /// Toggles. Deliberately takes nothing and asks nothing: the key must work
    /// whatever is open, which is exactly what broke when the cursor was freed
    /// by a guess about where the pointer hovered.
    void toggle_pointer_mode() { pointer_mode_ = !pointer_mode_; }

    // -- reach ----------------------------------------------------------------

    [[nodiscard]] float reach_ceiling_m() const { return reach_ceiling_m_; }
    void set_reach_ceiling_m(float metres);
    /// The reach that is actually in force: the smaller of the ceiling and the
    /// active tool's own arm. Zero when nothing is in hand.
    [[nodiscard]] float active_reach_m() const;
    [[nodiscard]] bool in_reach(const ToolAim& aim) const;

    // -- the frame ------------------------------------------------------------

    /// ONE ENTRY POINT. Press, drag and release are derived here from
    /// `button_down`, so there is no second edge convention to disagree with.
    ToolTickReport update(const ToolAim& aim, float dt_s, bool button_down,
                          ToolWorld& world);

    /// What the world should draw for the tool in hand. All false when the
    /// hand is empty — which is «весь UI дополнительный для этого пропадет»,
    /// expressed once.
    [[nodiscard]] ToolPreview preview(const ToolAim& aim) const;

    /// What the badge under the crosshair says. Empty key when nothing is held.
    [[nodiscard]] ToolStatus status(const ToolAim& aim) const;

private:
    /// THE ONLY WRITER OF active_. on_deselected is called from here and from
    /// nowhere else in the program.
    void select(std::size_t index, ToolWorld& world);

    std::vector<std::unique_ptr<IEditorTool>> tools_;
    std::size_t active_ = NO_TOOL;
    std::size_t settings_ = NO_TOOL;
    bool common_settings_ = false;
    bool pointer_mode_ = false;
    float reach_ceiling_m_ = EDITOR_REACH_DEFAULT_M;

    /// Whether the current press belongs to the active tool. A press that
    /// started blocked (on a panel, or out of reach) stays blocked until the
    /// button is let go: dragging a slider off the panel's edge must not start
    /// digging halfway through the drag.
    bool holding_ = false;
    bool was_down_ = false;
};

} // namespace dfn::app
