<!--
Created: 09:08:2026 - 00:16:00
Last updated: 14:08:2026 - 16:59:44
-->
<!--
UPD:
- 09:08:2026 - 00:16:00: Stage-1 state: interface only, no backends yet.
- 09:08:2026 - 00:50:00: Stage 2 — glfw + null backends implemented.
- 14:08:2026 - 16:59:44: Added text_input() — per-frame Unicode codepoint stream
  for live text entry (tool B28 chat overlay); GLFW char callback, null empty.
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
  `mouse_delta`, `scroll_delta`, `set_cursor_captured`/`is_cursor_captured`,
  `text_input` (per-frame Unicode codepoints — the live-typing channel, layout/
  IME aware, defaulted to an empty stream so the addition is purely additive).
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

## Current state (stage 2)

Implemented: `sources/glfw/` (GlfwInput — snapshot polling with edge
detection, raw mouse motion when captured, scroll via callback; owns the GLFW
window user pointer per the zone-internal callback policy) and
`sources/null/`. Factories: `sources/glfw/CreateGlfwInput.h` (requires a
GlfwWindow), `sources/null/CreateNullInput.h`. Target: `dfn_platform_input`.
Action mapping and rebinding remain a later engine-layer module on top of
these enums; gamepad methods will be added additively via group sync
(Rule 26).

Live text entry: `text_input()` returns the Unicode codepoints entered during
the frame just closed by `update()`. GlfwInput feeds a `glfwSetCharCallback`
accumulator (finished codepoints, already through the OS layout/IME — no manual
UTF-8 parsing) and snapshots it in `update()`, mirroring the scroll-callback
policy. NullInput returns an empty stream. Door-probe: run with
`DFN_TEXT_INPUT_LOG=1` to echo every accepted codepoint to stderr.
