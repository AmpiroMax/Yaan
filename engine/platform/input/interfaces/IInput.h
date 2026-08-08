/*
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 00:16:00
Module: engine/platform/input
File: engine/platform/input/interfaces/IInput.h

Responsibility:
- The platform input contract (Rule 0): keyboard/mouse polling for first-person
  control — per-frame key/button state, mouse delta, cursor capture. GLFW lives
  only behind it.

Key items:
- Key / MouseButton: engine-owned device codes (never backend scancodes).
- IInput: update, is_down / was_pressed / was_released, mouse position/delta,
  scroll, cursor capture.

Dependencies:
- Uses: C++ stdlib, glm (Rule 2). Nothing else.
- Used by: engine/app (per-frame update), gameplay/controller systems (as a
  parameter, Rule 9), engine/editor, tests (null backend).

Notes:
- Device level ONLY. Action mapping ("move_forward", rebinding, save/load of
  bindings — Q58) is a later engine-layer module built ON TOP of these enums;
  because Key/MouseButton are engine-owned values, rebinding needs no change
  here. Gamepad arrives the same way: new Gamepad* methods added via group sync
  (Rule 26) — additive, nothing existing breaks.
- Polling model: the app calls update() once per frame AFTER IWindow::poll_events;
  was_pressed/was_released are edge flags valid until the next update().
- Mouse delta is reported in pixels, sign convention: +x right, +y down; the
  camera layer applies sensitivity and inversion (degrees/radians never appear
  here — deltas are raw device units).
- Backends (stage 2): sources/glfw/ (real), sources/null/ (headless; nothing
  pressed, zero deltas, capture calls succeed silently — Rule 3).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Public contract, frozen for the stage (Rule 26): changes only via group sync.
- Do not add GLFW types, includes, or scancode assumptions to this header.
*/
/*
UPD:
- 09:08:2026 - 00:16:00: Initial stage-1 contract (render zone).
*/

#pragma once

#include <cstdint>
#include <glm/vec2.hpp>

namespace dfn::platform {

// Engine-owned key codes. Values are stable (serialized later by the rebinding
// layer, Q58) — append new keys at the end, never renumber.
enum class Key : uint16_t {
    UNKNOWN = 0,
    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // Digits (top row)
    NUM_0, NUM_1, NUM_2, NUM_3, NUM_4, NUM_5, NUM_6, NUM_7, NUM_8, NUM_9,
    // Control & navigation
    ESCAPE, ENTER, TAB, BACKSPACE, SPACE,
    LEFT, RIGHT, UP, DOWN,
    LEFT_SHIFT, RIGHT_SHIFT, LEFT_CONTROL, RIGHT_CONTROL, LEFT_ALT, RIGHT_ALT,
    LEFT_SUPER, RIGHT_SUPER,
    INSERT, DELETE, HOME, END, PAGE_UP, PAGE_DOWN,
    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    // Punctuation commonly bound in games
    GRAVE, MINUS, EQUAL, LEFT_BRACKET, RIGHT_BRACKET,
    SEMICOLON, APOSTROPHE, COMMA, PERIOD, SLASH, BACKSLASH,

    COUNT // keep last; not a key
};

enum class MouseButton : uint8_t {
    LEFT = 0,   // attack (Q10)
    RIGHT,      // block (Q10)
    MIDDLE,

    COUNT // keep last; not a button
};

class IInput {
public:
    virtual ~IInput() = default;

    // Snapshots device state for the frame. Call exactly once per frame, after
    // IWindow::poll_events(). Edge queries below refer to this snapshot.
    virtual void update() = 0;

    // Keyboard -----------------------------------------------------------------
    [[nodiscard]] virtual bool is_down(Key key) const = 0;
    [[nodiscard]] virtual bool was_pressed(Key key) const = 0;  // up -> down this frame
    [[nodiscard]] virtual bool was_released(Key key) const = 0; // down -> up this frame

    // Mouse --------------------------------------------------------------------
    [[nodiscard]] virtual bool is_down(MouseButton button) const = 0;
    [[nodiscard]] virtual bool was_pressed(MouseButton button) const = 0;
    [[nodiscard]] virtual bool was_released(MouseButton button) const = 0;

    // Cursor position in window pixels, origin top-left. Meaningful only while
    // the cursor is NOT captured (UI/editor use).
    [[nodiscard]] virtual glm::vec2 mouse_position() const = 0;

    // Movement since the previous update(), pixels, +x right / +y down.
    // The first-person look source; valid in both captured and free modes.
    [[nodiscard]] virtual glm::vec2 mouse_delta() const = 0;

    // Scroll steps since the previous update() (+y away from the user).
    [[nodiscard]] virtual glm::vec2 scroll_delta() const = 0;

    // Cursor capture -----------------------------------------------------------
    // Captured = cursor hidden and locked to the window; raw deltas keep flowing
    // (first-person mode). Uncaptured = normal OS cursor (menus, editor).
    virtual void set_cursor_captured(bool captured) = 0;
    [[nodiscard]] virtual bool is_cursor_captured() const = 0;
};

} // namespace dfn::platform
