/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
Module: engine/platform/input
File: engine/platform/input/sources/null/NullInput.h

Responsibility:
- Headless IInput backend (Rule 3): nothing pressed, zero deltas; capture
  state is remembered so is_cursor_captured stays consistent.

Key items:
- NullInput: full IInput implementation without any device.

Dependencies:
- Uses: IInput interface only.
- Used by: headless tests, CI tour smoke runs, engine/app (null mode).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
*/

#pragma once

#include "engine/platform/input/interfaces/IInput.h"

namespace dfn::platform {

class NullInput final : public IInput {
public:
    void update() override {}
    [[nodiscard]] bool is_down(Key) const override { return false; }
    [[nodiscard]] bool was_pressed(Key) const override { return false; }
    [[nodiscard]] bool was_released(Key) const override { return false; }
    [[nodiscard]] bool is_down(MouseButton) const override { return false; }
    [[nodiscard]] bool was_pressed(MouseButton) const override { return false; }
    [[nodiscard]] bool was_released(MouseButton) const override { return false; }
    [[nodiscard]] glm::vec2 mouse_position() const override { return {0.0f, 0.0f}; }
    [[nodiscard]] glm::vec2 mouse_delta() const override { return {0.0f, 0.0f}; }
    [[nodiscard]] glm::vec2 scroll_delta() const override { return {0.0f, 0.0f}; }
    void set_cursor_captured(bool captured) override { captured_ = captured; }
    [[nodiscard]] bool is_cursor_captured() const override { return captured_; }

private:
    bool captured_ = false;
};

} // namespace dfn::platform
