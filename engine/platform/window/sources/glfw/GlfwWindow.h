/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
Module: engine/platform/window
File: engine/platform/window/sources/glfw/GlfwWindow.h

Responsibility:
- GLFW IWindow backend: real OS window, event pump, native handle for bgfx.

Key items:
- GlfwWindow: IWindow implementation; glfw_handle() for the sibling glfw input
  backend (backend-internal API, never used outside platform sources).

Dependencies:
- Uses: IWindow interface; GLFWwindow by forward declaration ONLY (Rule 1 —
  GLFW headers appear solely in the .cpp).
- Used by: engine/app via create_glfw_window, GlfwInput (via glfw_handle).

Notes:
- Owns glfwInit/glfwTerminate: exactly one GlfwWindow may exist at a time
  (asserted). Single-window engine — a second window is a stage-later sync.
- Callback policy (agreed within the zone): GlfwWindow claims NO glfw callbacks
  and NOT the window user pointer; those belong to GlfwInput. Resize/close are
  polled.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep GLFW includes out of this header (forward declaration only).
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
*/

#pragma once

#include "engine/platform/window/interfaces/IWindow.h"

struct GLFWwindow; // GLFW's C handle — safe to forward-declare (Rule 1)

namespace dfn::platform {

class GlfwWindow final : public IWindow {
public:
    GlfwWindow() = default;
    ~GlfwWindow() override;
    GlfwWindow(const GlfwWindow&) = delete;
    GlfwWindow& operator=(const GlfwWindow&) = delete;

    [[nodiscard]] bool init(const WindowInitParams& params) override;
    void shutdown() override;
    void poll_events() override;
    [[nodiscard]] bool should_close() const override;
    void request_close() override;
    [[nodiscard]] void* native_handle() const override; // NSWindow* / HWND
    [[nodiscard]] glm::uvec2 framebuffer_size() const override;
    [[nodiscard]] bool consume_resize() override;

    /// Backend-internal: the raw GLFW handle for the sibling glfw input backend.
    /// Never exposed through IWindow; platform sources only (Rule 1).
    [[nodiscard]] GLFWwindow* glfw_handle() const { return window_; }

private:
    GLFWwindow* window_ = nullptr;
    glm::uvec2 last_size_{0, 0}; // framebuffer size at the last consume_resize
    bool glfw_initialized_ = false;
};

} // namespace dfn::platform
