/*
Created: 09:08:2026 - 00:45:00
Last updated: 27:08:2026 - 14:00:00
Module: engine/platform/window
File: engine/platform/window/sources/glfw/GlfwWindow.cpp

Responsibility:
- GlfwWindow implementation + create_glfw_window factory. The only window file
  that includes GLFW headers (Rule 1).

Key items:
- GlfwWindow methods; create_glfw_window(); native handle per OS.

Dependencies:
- Uses: GLFW 3.4 (glfw3.h, glfw3native.h), GlfwWindow.h, CreateGlfwWindow.h.
- Used by: dfn_platform_window target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- macOS is the tested path this stage; Windows branches compile-clean only.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation (macOS Cocoa handle,
  Win32 branch compiling-clean).
- 17:08:2026 - 16:27:55: set_fullscreen — своя частота монитора, возврат в запомненную рамку.
- 17:08:2026 - 19:17:13: content_size() через glfwGetWindowSize — GLFW сообщает позицию курсора именно в этих единицах.
- 18:08:2026 - 00:24:58: focus() — реализация нового пункта контракта IWindow.
- 27:08:2026 - 14:00:00: set_size(): в полном экране не делает ничего и
  не запоминает просьбу; в оконном режиме обновляет и запомненную коробку,
  иначе возврат из полного экрана прыгал бы в размер, выбранный полчаса назад.
*/

#include "engine/platform/window/sources/glfw/GlfwWindow.h"

#include "engine/platform/window/sources/glfw/CreateGlfwWindow.h"

#include <GLFW/glfw3.h>
#if defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#elif defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <cassert>
#include <memory>

namespace dfn::platform {

namespace {
// glfwInit/glfwTerminate are process-global; one live GlfwWindow at a time.
bool g_glfw_window_alive = false;
} // namespace

GlfwWindow::~GlfwWindow() {
    shutdown();
}

bool GlfwWindow::init(const WindowInitParams& params) {
    assert(!g_glfw_window_alive && "only one GlfwWindow may exist at a time");
    if (glfwInit() != GLFW_TRUE) {
        return false;
    }
    glfw_initialized_ = true;

    // bgfx owns the graphics API; GLFW must not create a GL context.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, params.resizable ? GLFW_TRUE : GLFW_FALSE);

    GLFWmonitor* monitor = params.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    window_ = glfwCreateWindow(static_cast<int>(params.width),
                               static_cast<int>(params.height),
                               params.title.c_str(), monitor, nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        glfw_initialized_ = false;
        return false;
    }
    g_glfw_window_alive = true;
    last_size_ = framebuffer_size();
    return true;
}

void GlfwWindow::set_fullscreen(bool on) {
    if (window_ == nullptr || on == is_fullscreen()) {
        return;
    }
    if (on) {
        // Remember the windowed placement BEFORE leaving it — see the fields'
        // comment. Position, not just size: a window restored to the right size
        // in the wrong corner still reads as broken.
        glfwGetWindowPos(window_, &windowed_x_, &windowed_y_);
        glfwGetWindowSize(window_, &windowed_w_, &windowed_h_);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor == nullptr) {
            return;
        }
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode == nullptr) {
            return;
        }
        // The monitor's OWN refresh rate, not a number we picked: anything else
        // makes the display resample and the whole frame judder.
        glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height,
                             mode->refreshRate);
        return;
    }
    // Returning to a window with no remembered box would put it at 0,0 with
    // monitor size. If we never stored one (fullscreen from birth), fall back
    // to a readable window rather than to zeros.
    if (windowed_w_ <= 0 || windowed_h_ <= 0) {
        windowed_x_ = 100;
        windowed_y_ = 100;
        windowed_w_ = 1280;
        windowed_h_ = 720;
    }
    glfwSetWindowMonitor(window_, nullptr, windowed_x_, windowed_y_,
                         windowed_w_, windowed_h_, GLFW_DONT_CARE);
}

void GlfwWindow::set_size(uint32_t width, uint32_t height) {
    // В ПОЛНОМ ЭКРАНЕ РАЗМЕР ЗАДАЁТ МОНИТОР. Просьба не отвергается ошибкой и
    // не откладывается: она просто не про этот режим, и запомнить её тоже
    // нельзя — окно, которое при выходе из полного экрана прыгнет в размер,
    // заказанный полчаса назад, читается как поломка.
    if (window_ == nullptr || width == 0 || height == 0 || is_fullscreen()) {
        return;
    }
    glfwSetWindowSize(window_, static_cast<int>(width), static_cast<int>(height));
    // И ЗАПОМИНАЕМ КАК ОКОННУЮ КОРОБКУ: без этого возврат из полного экрана
    // восстановил бы РАНЬШЕ выбранный размер, а не тот, который игрок только
    // что поставил.
    glfwGetWindowPos(window_, &windowed_x_, &windowed_y_);
    glfwGetWindowSize(window_, &windowed_w_, &windowed_h_);
}

bool GlfwWindow::is_fullscreen() const {
    return window_ != nullptr && glfwGetWindowMonitor(window_) != nullptr;
}

void GlfwWindow::shutdown() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        g_glfw_window_alive = false;
    }
    if (glfw_initialized_) {
        glfwTerminate();
        glfw_initialized_ = false;
    }
}

void GlfwWindow::poll_events() {
    glfwPollEvents();
}

bool GlfwWindow::should_close() const {
    return window_ == nullptr || glfwWindowShouldClose(window_) == GLFW_TRUE;
}

void GlfwWindow::request_close() {
    if (window_ != nullptr) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

void GlfwWindow::focus() {
    if (window_ != nullptr) {
        glfwFocusWindow(window_);
    }
}

void* GlfwWindow::native_handle() const {
    if (window_ == nullptr) {
        return nullptr;
    }
#if defined(__APPLE__)
    return static_cast<void*>(glfwGetCocoaWindow(window_)); // NSWindow*
#elif defined(_WIN32)
    return static_cast<void*>(glfwGetWin32Window(window_)); // HWND
#else
    return nullptr; // other platforms arrive with a later sync
#endif
}

glm::uvec2 GlfwWindow::framebuffer_size() const {
    if (window_ == nullptr) {
        return {0, 0};
    }
    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
}

glm::uvec2 GlfwWindow::content_size() const {
    if (window_ == nullptr) {
        return {0, 0};
    }
    // GLFW's "window size" IS the logical unit the cursor callbacks report in,
    // which is the whole reason this exists: the editor's interface has to put
    // the pointer and the panels in one coordinate system, and on a Retina
    // display the framebuffer is twice this.
    int w = 0;
    int h = 0;
    glfwGetWindowSize(window_, &w, &h);
    return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
}

bool GlfwWindow::consume_resize() {
    const glm::uvec2 now = framebuffer_size();
    if (now != last_size_) {
        last_size_ = now;
        return true;
    }
    return false;
}

std::unique_ptr<IWindow> create_glfw_window() {
    return std::make_unique<GlfwWindow>();
}

} // namespace dfn::platform
