/*
Module: engine/app
File: engine/app/sources/ThirdPersonRig.cpp

Responsibility:
- Реализация трёх предложений заголовка: угол ввода, ограниченный разворот и
  перепись осей в систему тела.

Dependencies:
- Uses: ThirdPersonRig.h, glm, std.
- Used by: engine/app (App), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Чисто и детерминированно: те же входы — те же выходы, часов нет.
*/

#include "engine/app/sources/ThirdPersonRig.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

namespace dfn::app {

float shortest_arc(float from, float to) {
    const float two_pi = glm::two_pi<float>();
    float d = std::fmod(to - from + glm::pi<float>(), two_pi);
    if (d < 0.0f) {
        d += two_pi;
    }
    return d - glm::pi<float>();
}

float move_yaw_from_axes(const glm::vec2& axes) {
    return std::atan2(axes.x, axes.y);
}

float turn_body_toward(float body_yaw, float target_yaw, float dt, float rate_rad_s) {
    if (dt <= 0.0f || rate_rad_s <= 0.0f) {
        return body_yaw;
    }
    const float need = shortest_arc(body_yaw, target_yaw);
    const float most = rate_rad_s * dt;
    return body_yaw + std::clamp(need, -most, most);
}

glm::vec2 axes_in_body_frame(const glm::vec2& axes, float body_yaw, float want_yaw) {
    const float len = glm::length(axes);
    if (len <= 0.0f) {
        return glm::vec2{0.0f};
    }
    const float d = shortest_arc(body_yaw, want_yaw);
    return glm::vec2{std::sin(d), std::cos(d)} * len;
}

ThirdPersonStep third_person_step(float body_yaw, float cam_yaw, const glm::vec2& axes,
                                  float dt, float rate_rad_s) {
    ThirdPersonStep out;
    out.body_yaw = body_yaw;
    out.want_yaw = body_yaw;
    if (glm::length(axes) < MOVE_AXES_DEADZONE) {
        // СТОЯ ТЕЛО НЕ ПОВОРАЧИВАЕТСЯ ЗА КАМЕРОЙ — заказ владельца, пункт 4.
        // Бит-в-бит тождество, а не «доворот на ноль»: контрольная рука
        // приёмки («мышь крутила, рыск не изменился») держится ровно этим.
        return out;
    }
    out.turning = true;
    out.want_yaw = cam_yaw + move_yaw_from_axes(axes);
    out.body_yaw = turn_body_toward(body_yaw, out.want_yaw, dt, rate_rad_s);
    // ОСИ ПЕРЕПИСЫВАЮТСЯ ОТ НОВОГО РЫСКА, а не от старого: симуляция применит
    // их в том же тике, в котором получит рыск, и рассогласование на один шаг
    // разворота — это ровно тот дрейф, который делает прямую дугой.
    out.move_axes = axes_in_body_frame(axes, out.body_yaw, out.want_yaw);
    return out;
}

} // namespace dfn::app
