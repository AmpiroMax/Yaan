/*
Created: 14:08:2026 - 16:11:00
Last updated: 18:08:2026 - 23:20:00
Module: engine/app
File: engine/app/sources/EditorCamera.cpp

Responsibility:
- EditorCamera integration: mouse-look, wheel-driven fly speed, and 6DOF
  translation from the key state, all over one render frame. See the header.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/
/*
UPD:
- 14:08:2026 - 16:11:00: Created.
- 18:08:2026 - 16:28:24: СКОРОСТЬ ПОЛЁТА УЕХАЛА С КОЛЕСА НА СКОБКИ (решение 18.08: «ускорение
  передвижения и уменьшение скорости перенесём на [ — замедлить, ] — ускорить»).
  Довод не про удобство клавиш: колесо освобождено под ДАЛЬНОСТЬ, которая
  меняется в работе постоянно, а скорость полёта выставляется раз и надолго.
  Шаг остался геометрическим и по прежней причине — от края до края одинаковое
  число нажатий.
- 18:08:2026 - 23:20:00: Спуск на Shift вместо Q, и Shift НЕ опускает, пока он модификатор при Cmd.
*/

#include "engine/app/sources/EditorCamera.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/platform/input/interfaces/IInput.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace dfn::app {

void EditorCamera::reset(const glm::vec3& eye, float yaw, float pitch) {
    position_ = eye;
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -static_cast<float>(config::CAMERA_PITCH_LIMIT),
                        static_cast<float>(config::CAMERA_PITCH_LIMIT));
    speed_ = static_cast<float>(config::EDITOR_CAM_SPEED_DEFAULT);
}

void EditorCamera::set_pose(const glm::vec3& eye, float yaw, float pitch) {
    position_ = eye;
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -static_cast<float>(config::CAMERA_PITCH_LIMIT),
                        static_cast<float>(config::CAMERA_PITCH_LIMIT));
}

void EditorCamera::update(const platform::IInput& input, float dt) {
    using platform::Key;

    // MOUSE-LOOK, the same sign as the sim controller and the third-person
    // orbit (Rule 35, one convention). mouse_delta is zero when the cursor is
    // not captured, so an unattended run simply does not turn -- no branch on
    // capture state, which is the app's business and not this class's.
    const float sens = static_cast<float>(config::MOUSE_SENSITIVITY);
    const glm::vec2 md = input.mouse_delta();
    yaw_ += md.x * sens;
    const float limit = static_cast<float>(config::CAMERA_PITCH_LIMIT);
    pitch_ = std::clamp(pitch_ - md.y * sens, -limit, limit);

    // СКОРОСТЬ ПОЛЁТА — НА СКОБКАХ, А НЕ НА КОЛЕСЕ (решение пользователя 18.08:
    // «ускорение передвижения и уменьшение скорости перенесём на [ — замедлить,
    // ] — ускорить»). Колесо освобождено под дальность взаимодействия — то, что
    // меняется в работе постоянно, а скорость полёта выставляется раз и надолго.
    //
    // Шаг тот же геометрический и по той же причине: одно нажатие умножает, а не
    // прибавляет, поэтому от края до края одинаковое число нажатий. Линейный шаг
    // полз бы у верхней границы и прыгал у нижней.
    const auto step_speed = [this](float notches) {
        speed_ *= std::pow(static_cast<float>(config::EDITOR_CAM_SPEED_WHEEL_STEP),
                           notches);
        speed_ = std::clamp(speed_, static_cast<float>(config::EDITOR_CAM_SPEED_MIN),
                            static_cast<float>(config::EDITOR_CAM_SPEED_MAX));
    };
    if (input.was_pressed(Key::RIGHT_BRACKET)) {
        step_speed(1.0f);
    }
    if (input.was_pressed(Key::LEFT_BRACKET)) {
        step_speed(-1.0f);
    }

    // BASIS. Forward carries pitch so W flies where the eye looks (a 6DOF fly,
    // not a walk); right is the horizontal perpendicular so strafing never
    // rolls; up is world up so Space/Ctrl are always vertical regardless of
    // where the eye points.
    const float cp = std::cos(pitch_);
    const glm::vec3 forward{std::sin(yaw_) * cp, std::sin(pitch_), -std::cos(yaw_) * cp};
    const glm::vec3 world_up{0.0f, 1.0f, 0.0f};
    const glm::vec3 right{std::cos(yaw_), 0.0f, std::sin(yaw_)};

    glm::vec3 move{0.0f};
    if (input.is_down(Key::W)) { move += forward; }
    if (input.is_down(Key::S)) { move -= forward; }
    if (input.is_down(Key::D)) { move += right; }
    if (input.is_down(Key::A)) { move -= right; }
    // Up: E or Space. Down: Q or Ctrl. Two bindings for one axis so both the
    // "Q/E" and the "Space/Ctrl" hands the user named reach it.
    if (input.is_down(Key::E) || input.is_down(Key::SPACE)) { move += world_up; }
    if (input.is_down(Key::Q) || input.is_down(Key::LEFT_CONTROL)
        || input.is_down(Key::RIGHT_CONTROL)) {
        move -= world_up;
    }

    const float len = glm::length(move);
    if (len > 0.0f) {
        position_ += (move / len) * speed_ * dt;
    }
}

components::CameraPose EditorCamera::pose() const {
    components::CameraPose p{};
    p.position = position_;
    p.yaw = yaw_;
    p.pitch = pitch_;
    p.fov_scale = 1.0f;
    return p;
}

} // namespace dfn::app
