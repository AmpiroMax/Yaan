/*
Module: engine/app
File: engine/app/sources/GrabDrive.cpp

Responsibility:
- Реализация GrabDrive.h: счётчик удержания, пружина с потолком силы, выпадение
  и бросок.

Dependencies:
- Uses: GrabDrive.h, glm, стандартная библиотека.
- Used by: AppProps.cpp, рукава app_grab_drive и sim_loose_props.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона big-grab владеет этим файлом.
- ЭТО ЕДИНСТВЕННОЕ МЕСТО, ГДЕ ЖИВЁТ АРИФМЕТИКА ХВАТА. И игра, и беспилотный
  замер зовут ЭТИ функции: две копии пружины разошлись бы, и приёмка мерила бы
  не то, во что играют.
*/

#include "engine/app/sources/GrabDrive.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace dfn::app {

GrabPress grab_press(GrabHold& hold, bool down, float dt, const GrabTuning& t) {
    if (!down) {
        hold.held_s = 0.0f;
        hold.consumed = false;
        return GrabPress::None;
    }
    hold.held_s += dt;
    if (hold.held_s < t.hold_s) {
        return GrabPress::Holding;
    }
    if (hold.consumed) {
        return GrabPress::Holding; // порог уже засчитан; второй раз не берём
    }
    hold.consumed = true;
    return GrabPress::Long;
}

glm::vec3 grab_velocity(const glm::vec3& position, const glm::vec3& velocity,
                        const glm::vec3& target, float mass_kg, float dt,
                        const GrabTuning& t) {
    const glm::vec3 to_target = target - position;
    // ЖЕЛАЕМАЯ СКОРОСТЬ — «выбрать расстояние за reach_time_s», ограниченная
    // потолком скорости. Потолок стоит ДО ограничения ускорением, а не после:
    // иначе тяжёлое тело сперва получало бы недостижимую цель, а потом её
    // урезали, и вес выражался бы дважды.
    glm::vec3 wanted = to_target / std::max(t.reach_time_s, 1e-3f);
    const float wanted_speed = glm::length(wanted);
    if (wanted_speed > t.max_speed) {
        wanted *= t.max_speed / wanted_speed;
    }
    // ВЕС ЖИВЁТ ЗДЕСЬ: потолок силы, делённый на массу, — это потолок
    // УСКОРЕНИЯ, и за один тик скорость может измениться не больше чем на
    // a_max * dt. Кувшин (2 кг) получает 175 м/с² и догоняет руку мгновенно;
    // мешок (46 кг) — 7.6 м/с², и его видно тащат.
    const float a_max = t.max_force_n / std::max(mass_kg, 1e-3f);
    glm::vec3 delta_v = wanted - velocity;
    const float delta_len = glm::length(delta_v);
    const float allowed = a_max * dt;
    if (delta_len > allowed && delta_len > 1e-6f) {
        delta_v *= allowed / delta_len;
    }
    return velocity + delta_v;
}

bool grab_slipped(float distance_m, float dt, float& lag_s, const GrabTuning& t) {
    if (distance_m <= t.slip_m) {
        lag_s = 0.0f;
        return false;
    }
    lag_s += dt;
    return lag_s >= t.slip_time_s;
}

float throw_speed(float mass_kg, const GrabTuning& t) {
    return std::min(t.throw_impulse / std::max(mass_kg, 1e-3f), t.throw_max_speed);
}

} // namespace dfn::app
