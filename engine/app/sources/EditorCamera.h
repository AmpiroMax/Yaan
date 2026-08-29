/*
Module: engine/app
File: engine/app/sources/EditorCamera.h

Responsibility:
- The free (6DOF) camera of AppMode::Editor: a flying eye the app drives
  DIRECTLY from live input, detached from the player body and from physics
  (it passes through walls). It never ticks the simulation and never reads the
  clock -- the app hands it the render-frame dt and reads a CameraPose back.

Key items:
- EditorCamera: reset() seeds pose from a player eye for a seamless toggle;
  update() advances the fly from IInput over one render frame (WASD + up/down +
  mouse-look, wheel = fly speed); pose() yields the CameraPose the app feeds to
  FirstPersonCamera (prev == curr, since the app owns the pose outright and
  there is nothing to interpolate between fixed steps).

Dependencies:
- Uses: engine/platform/input (IInput), engine/core/components (CameraPose),
  engine/core/config (generated NUMBERS constants), glm.
- Used by: engine/app (App) only.

Notes:
- Sim's angle convention, shared with FirstPersonCamera and the sim controller
  (Rule 35, one convention): yaw 0 looks toward -Z, positive yaw turns right
  (clockwise from above); positive pitch looks up; forward =
  (sin y cos p, sin p, -cos y cos p). Kept identical so a toggle in and out of
  the body does not flip the view.
- Numbers are strings in docs/NUMBERS.md (Rule 14): EDITOR_CAM_SPEED_* and the
  reused MOUSE_SENSITIVITY / CAMERA_PITCH_LIMIT. No literal fly constant here.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
- No cursor or window calls here: whether the pointer is captured is the app's
  decision (an unattended run must not grab it), and this class must work the
  same whether it is or not. КТО ДЕРЖИТ МЫШЬ — решает App, и решает
  editor_camera_takes_mouse ниже; сюда доходит уже принятое решение.
*/

#pragma once

#include "engine/core/components/sources/Components.h"

#include <glm/vec3.hpp>

namespace dfn::platform {
class IInput;
}

namespace dfn::app {

/// КОМУ ДОСТАЁТСЯ МЫШЬ В РЕДАКТОРЕ — одним выражением, чтобы у него была
/// проверка. Три захода подряд «камера не крутится» разбирались за игрой, а не
/// прибором: решение жило вложенными if внутри кадрового цикла, куда тест не
/// дотягивается. Значения: редактор ли сейчас, набирает ли человек текст,
/// отдан ли курсор интерфейсу клавишей R. Камера смотрит ТОЛЬКО когда
/// редактор открыт, текст не набирается и курсор не отдан.
[[nodiscard]] constexpr bool editor_camera_takes_mouse(bool editor, bool chat_typing,
                                                       bool cursor_free) {
    return editor && !chat_typing && !cursor_free;
}

class EditorCamera {
public:
    // Seed the fly from a player eye (position + look), so entering the editor
    // from the body -- and returning -- is seamless. Resets the fly speed to
    // EDITOR_CAM_SPEED_DEFAULT: a fresh session of flying starts controllable.
    void reset(const glm::vec3& eye, float yaw, float pitch);

    // Place the eye at an explicit pose without touching the speed. Used by the
    // tooling door (DFN_EDITOR_CAM) so an acceptance frame can be taken from any
    // vantage without a hand on the keyboard.
    void set_pose(const glm::vec3& eye, float yaw, float pitch);

    // Advance one RENDER frame from live input. dt is the wall-clock frame
    // delta (metres per second are real seconds, Rule 14). Reads WASD, up/down
    // (E/Space up, Q/Ctrl down), mouse-look, and the wheel (fly speed). Touches
    // neither the world nor the cursor.
    void update(const platform::IInput& input, float dt);

    [[nodiscard]] components::CameraPose pose() const;

    [[nodiscard]] const glm::vec3& position() const { return position_; }
    [[nodiscard]] float yaw() const { return yaw_; }
    [[nodiscard]] float pitch() const { return pitch_; }
    [[nodiscard]] float speed() const { return speed_; }

private:
    glm::vec3 position_{0.0f};
    float yaw_ = 0.0f;   // radians, sim convention (0 = -Z, + = clockwise)
    float pitch_ = 0.0f; // radians, + = up
    float speed_ = 0.0f; // m/s, seeded on reset() from EDITOR_CAM_SPEED_DEFAULT
};

} // namespace dfn::app
