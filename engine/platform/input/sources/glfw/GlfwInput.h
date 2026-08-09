/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
Module: engine/platform/input
File: engine/platform/input/sources/glfw/GlfwInput.h

Responsibility:
- GLFW IInput backend: per-frame keyboard/mouse snapshots with edge detection,
  mouse delta, scroll (via callback), cursor capture with raw motion.

Key items:
- GlfwInput: IInput implementation over a GLFWwindow owned by GlfwWindow.

Dependencies:
- Uses: IInput interface; GLFWwindow by forward declaration ONLY (Rule 1).
- Used by: engine/app via create_glfw_input.

Notes:
- Callback policy (agreed within the zone): GlfwInput owns the GLFW window user
  pointer and the scroll callback; GlfwWindow polls and claims neither.
- Key/button state is snapshotted in update() via glfwGetKey/glfwGetMouseButton
  over the engine Key table; edges are curr vs prev snapshot.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep GLFW includes out of this header (forward declaration only).
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
*/

#pragma once

#include "engine/platform/input/interfaces/IInput.h"

#include <array>
#include <bitset>
#include <cstddef>

struct GLFWwindow; // GLFW's C handle — safe to forward-declare (Rule 1)

namespace dfn::platform {

class GlfwInput final : public IInput {
public:
    /// `window` must outlive this object (it is GlfwWindow's handle).
    explicit GlfwInput(GLFWwindow* window);
    ~GlfwInput() override;
    GlfwInput(const GlfwInput&) = delete;
    GlfwInput& operator=(const GlfwInput&) = delete;

    void update() override;

    [[nodiscard]] bool is_down(Key key) const override;
    [[nodiscard]] bool was_pressed(Key key) const override;
    [[nodiscard]] bool was_released(Key key) const override;

    [[nodiscard]] bool is_down(MouseButton button) const override;
    [[nodiscard]] bool was_pressed(MouseButton button) const override;
    [[nodiscard]] bool was_released(MouseButton button) const override;

    [[nodiscard]] glm::vec2 mouse_position() const override;
    [[nodiscard]] glm::vec2 mouse_delta() const override;
    [[nodiscard]] glm::vec2 scroll_delta() const override;

    void set_cursor_captured(bool captured) override;
    [[nodiscard]] bool is_cursor_captured() const override;

private:
    static constexpr size_t KEY_COUNT = static_cast<size_t>(Key::COUNT);
    static constexpr size_t BUTTON_COUNT = static_cast<size_t>(MouseButton::COUNT);

    GLFWwindow* window_ = nullptr;
    std::bitset<KEY_COUNT> keys_curr_;
    std::bitset<KEY_COUNT> keys_prev_;
    std::bitset<BUTTON_COUNT> buttons_curr_;
    std::bitset<BUTTON_COUNT> buttons_prev_;
    glm::vec2 mouse_pos_{0.0f, 0.0f};
    glm::vec2 mouse_delta_{0.0f, 0.0f};
    glm::vec2 scroll_delta_{0.0f, 0.0f};    // snapshot for the current frame
    glm::vec2 scroll_accum_{0.0f, 0.0f};    // fed by the GLFW scroll callback
    bool captured_ = false;
    bool have_prev_pos_ = false;

    friend void glfw_input_scroll_callback(GLFWwindow*, double, double);
};

} // namespace dfn::platform
