/*
Created: 09:08:2026 - 00:45:00
Last updated: 18:08:2026 - 00:24:58
Module: engine/platform/window
File: engine/platform/window/sources/null/NullWindow.h

Responsibility:
- Headless IWindow backend (Rule 3): a runnable mode, not a stub. Reports the
  requested size, never resizes, closes only via request_close.

Key items:
- NullWindow: full IWindow implementation without any OS window.

Dependencies:
- Uses: IWindow interface only.
- Used by: headless tests, CI tour smoke runs, engine/app (null mode).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Must stay dependency-free; behavior must satisfy every IWindow postcondition.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
- 17:08:2026 - 16:27:55: полный экран — тихий отказ: нечего показывать, нечего разворачивать.
- 18:08:2026 - 00:24:58: focus() — реализация нового пункта контракта IWindow.
*/

#pragma once

#include "engine/platform/window/interfaces/IWindow.h"

namespace dfn::platform {

class NullWindow final : public IWindow {
public:
    [[nodiscard]] bool init(const WindowInitParams& params) override;
    void shutdown() override;
    void poll_events() override;
    [[nodiscard]] bool should_close() const override;
    void request_close() override;
    void focus() override;
    [[nodiscard]] void* native_handle() const override; // always nullptr
    [[nodiscard]] glm::uvec2 framebuffer_size() const override;
    [[nodiscard]] bool consume_resize() override; // always false (never resizes)
    // NOTHING ON SCREEN, NOTHING TO MAKE FULL. Silently accepting the request
    // and reporting false is the honest answer for a headless window; refusing
    // loudly would make every headless test print a warning it cannot act on.
    void set_fullscreen(bool /*on*/) override {}
    [[nodiscard]] bool is_fullscreen() const override { return false; }

private:
    glm::uvec2 size_{0, 0};
    bool close_requested_ = false;
};

} // namespace dfn::platform
