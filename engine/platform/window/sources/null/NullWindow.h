/*
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
    /// РАЗМЕР ПРИНИМАЕТСЯ, И ЭТО НЕ ПРИТВОРСТВО: у безголового окна размер —
    /// единственное, что у него вообще есть (framebuffer_size его отдаёт), и
    /// прогон, попросивший другой кадр, обязан его получить. Отказ здесь сделал
    /// бы счётный прогон слепым к настройке, которую он же и проверяет.
    void set_size(uint32_t width, uint32_t height) override {
        if (width > 0 && height > 0) {
            size_ = {width, height};
        }
    }

private:
    glm::uvec2 size_{0, 0};
    bool close_requested_ = false;
};

} // namespace dfn::platform
