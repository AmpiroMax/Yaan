/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
Module: engine/platform/window
File: engine/platform/window/sources/null/NullWindow.cpp

Responsibility:
- NullWindow implementation + create_null_window factory.

Key items:
- NullWindow methods; create_null_window().

Dependencies:
- Uses: NullWindow.h, CreateNullWindow.h.
- Used by: dfn_platform_window target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
*/

#include "engine/platform/window/sources/null/NullWindow.h"

#include "engine/platform/window/sources/null/CreateNullWindow.h"

#include <memory>

namespace dfn::platform {

bool NullWindow::init(const WindowInitParams& params) {
    size_ = {params.width, params.height};
    close_requested_ = false;
    return true;
}

void NullWindow::shutdown() {}

void NullWindow::poll_events() {}

bool NullWindow::should_close() const {
    return close_requested_;
}

void NullWindow::request_close() {
    close_requested_ = true;
}

void* NullWindow::native_handle() const {
    return nullptr; // the null renderer accepts this (agreed contract)
}

glm::uvec2 NullWindow::framebuffer_size() const {
    return size_; // logical == physical in headless mode
}

bool NullWindow::consume_resize() {
    return false;
}

std::unique_ptr<IWindow> create_null_window() {
    return std::make_unique<NullWindow>();
}

} // namespace dfn::platform
