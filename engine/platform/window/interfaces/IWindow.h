/*
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 00:16:00
Module: engine/platform/window
File: engine/platform/window/interfaces/IWindow.h

Responsibility:
- The platform window contract (Rule 0): lifecycle, OS event pump, native handle
  for the renderer, framebuffer size and resize/close signalling. GLFW lives only
  behind it.

Key items:
- IWindow: init/shutdown, poll_events, should_close, native_handle,
  framebuffer_size, consume_resize.
- WindowInitParams: logical size, title, fullscreen/resizable flags.

Dependencies:
- Uses: C++ stdlib, glm (Rule 2). Nothing else.
- Used by: engine/app (owns the window, feeds RendererInitParams), engine/render
  (tour shutdown via request_close), tests (null backend).

Notes:
- Polling model, no callbacks: the app calls poll_events() once per frame, then
  reads state. Keeps the contract trivially implementable by the null backend and
  by any windowing library (Rule 4).
- Backends (stage 2): sources/glfw/ (real), sources/null/ (headless; runnable
  mode per Rule 3 — never a crash, sensible inert values).
- native_handle() returns the OS-level handle RendererInitParams expects
  (NSWindow* on macOS, HWND on Windows); null backend returns nullptr and the
  null renderer accepts that.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Public contract, frozen for the stage (Rule 26): changes only via group sync.
- Do not add GLFW types, includes, or assumptions to this header.
*/
/*
UPD:
- 09:08:2026 - 00:16:00: Initial stage-1 contract (render zone).
*/

#pragma once

#include <cstdint>
#include <glm/vec2.hpp>
#include <string>

namespace dfn::platform {

struct WindowInitParams {
    uint32_t width = 0;        // logical size, pixels (framebuffer may differ on HiDPI)
    uint32_t height = 0;
    std::string title;         // window title; caller resolves it via localization (Rule 5)
    bool fullscreen = false;
    bool resizable = true;
};

class IWindow {
public:
    virtual ~IWindow() = default;

    // Lifecycle ----------------------------------------------------------------
    [[nodiscard]] virtual bool init(const WindowInitParams& params) = 0;
    virtual void shutdown() = 0;

    // Pumps OS events. Call exactly once per frame, before IInput::update().
    virtual void poll_events() = 0;

    // Close signalling ---------------------------------------------------------
    // True once the user (or request_close) asked to close; the app owns the exit.
    [[nodiscard]] virtual bool should_close() const = 0;
    virtual void request_close() = 0; // e.g. the tour after its last screenshot

    // Renderer handoff ---------------------------------------------------------
    // OS-level handle for RendererInitParams::native_window_handle.
    // NSWindow* on macOS, HWND on Windows; nullptr from the null backend.
    [[nodiscard]] virtual void* native_handle() const = 0;

    // Framebuffer size in physical pixels (HiDPI-aware). Feeds RendererInitParams
    // and IRenderer::resize.
    [[nodiscard]] virtual glm::uvec2 framebuffer_size() const = 0;

    // Returns true if the framebuffer size changed since the previous call and
    // clears the flag (one consumer: the app, which forwards to IRenderer::resize).
    [[nodiscard]] virtual bool consume_resize() = 0;
};

} // namespace dfn::platform
