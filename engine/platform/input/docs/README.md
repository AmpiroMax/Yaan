<!--
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 00:16:00
-->
<!--
UPD:
- 09:08:2026 - 00:16:00: Stage-1 state: interface only, no backends yet.
-->

# engine/platform/input

## Responsibility

Platform input contract (Rule 0): device-level keyboard/mouse polling for
first-person control — per-frame key state with edge detection, mouse delta,
cursor capture. GLFW is hidden behind it; backends arrive in stage 2.

## Key types

- `dfn::platform::IInput` (`interfaces/IInput.h`) — pure virtual contract:
  `update` (once per frame after `IWindow::poll_events`), `is_down` /
  `was_pressed` / `was_released` for `Key` and `MouseButton`, `mouse_position`,
  `mouse_delta`, `scroll_delta`, `set_cursor_captured`/`is_cursor_captured`.
- `dfn::platform::Key`, `dfn::platform::MouseButton` — engine-owned stable
  codes (never backend scancodes); append-only enums, safe to serialize by the
  future rebinding layer (Q58).

## Usage example

```cpp
input.update(); // after window.poll_events()
const glm::vec2 look = input.mouse_delta();          // first-person look
const bool forward = input.is_down(dfn::platform::Key::W);
if (input.was_pressed(dfn::platform::Key::ESCAPE)) {
    input.set_cursor_captured(false);                // open menu
}
```

## Dependencies

- Uses: C++ stdlib, glm (Rule 2). Backends (stage 2): `sources/glfw/`,
  `sources/null/` (nothing pressed, zero deltas — Rule 3).
- Used by: `engine/app`, controller/gameplay systems (as a parameter, Rule 9),
  `engine/editor`, tests.

## Current state (stage 1)

Interface only — no `sources/` yet (contracts-only stage, Q38/Q51). Action
mapping and rebinding are a later engine-layer module on top of these enums;
gamepad methods will be added additively via group sync (Rule 26).
