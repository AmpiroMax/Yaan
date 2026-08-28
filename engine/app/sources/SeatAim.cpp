/*
Created: 28:08:2026 - 11:52:10
Last updated: 28:08:2026 - 11:52:10
Module: engine/app
File: engine/app/sources/SeatAim.cpp

Responsibility:
- Геометрия прицела мебели: ближайшая точка габарита (радиус) и пересечение
  луча взгляда с этим же габаритом (взгляд).

Dependencies:
- Uses: SeatAim.h, glm, <cmath>.
- Used by: AppSeats.cpp, tests/app/SeatAimTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона lead владеет этим файлом.
*/
/*
UPD:
- 28:08:2026 - 11:52:10: Создан вместе с заголовком.
*/

#include "engine/app/sources/SeatAim.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace dfn::app {

glm::vec3 seat_aim_local(const SeatAim& aim, const glm::vec3& world) {
    // ОБРАТНЫЙ ПОВОРОТ ТОЙ ЖЕ КОНВЕНЦИИ, ЧТО У СЦЕН: местный +X уходит в
    // (cos, 0, -sin), местный +Z — в (sin, 0, cos). Значит местные координаты
    // берутся проекцией на те же две оси.
    const glm::vec3 d = world - aim.centre;
    const float c = std::cos(aim.yaw);
    const float s = std::sin(aim.yaw);
    return {d.x * c - d.z * s, d.y, d.x * s + d.z * c};
}

SeatAimHit seat_aim(const SeatAim& aim, const glm::vec3& eye, const glm::vec3& dir) {
    SeatAimHit hit;
    const glm::vec3 h = glm::max(aim.half, glm::vec3{0.01f});
    const glm::vec3 pad = h + glm::vec3{SEAT_AIM_PAD_M};

    const glm::vec3 e = seat_aim_local(aim, eye);
    // Направление в местных осях — тот же поворот, но без переноса.
    const float c = std::cos(aim.yaw);
    const float s = std::sin(aim.yaw);
    const glm::vec3 d{dir.x * c - dir.z * s, dir.y, dir.x * s + dir.z * c};

    // --- РАДИУС: ДО БЛИЖАЙШЕЙ ТОЧКИ ГАБАРИТА -------------------------------
    // Не до середины: у кровати 1.15x2.05 середина отстоит от изголовья на
    // метр, и радиус, отмеренный до неё, значил бы у разных предметов разное.
    // ...И ОТ СТОЙКИ, А НЕ ОТ ГЛАЗА: по вертикали человек занимает отрезок от
    // глаза до подошвы, и до лавки 0.45 он дотягивается, не отходя (см.
    // stand_m в заголовке). По X и Z — обычная ближайшая точка коробки.
    const float feet_y = e.y - std::max(0.0f, aim.stand_m);
    const float y_lo = std::min(e.y, feet_y);
    const float y_hi = std::max(e.y, feet_y);
    // Точка СТОЙКИ, ближайшая к габариту по высоте: середина коробки (местный
    // 0) зажатая в отрезок. В паре с зажимом в габарит строкой ниже это ровно
    // расстояние между двумя отрезками — ноль, когда они перекрываются.
    const glm::vec3 column{e.x, std::clamp(0.0f, y_lo, y_hi), e.z};
    const glm::vec3 nearest = glm::clamp(column, -h, h);
    hit.distance_m = glm::length(column - nearest);
    hit.in_reach = hit.distance_m <= aim.reach_m;

    // --- ВЗГЛЯД: ПЛИТОЧНЫЙ ТЕСТ ЛУЧА ПРОТИВ КОРОБКИ -------------------------
    float t_near = -1.0e30f;
    float t_far = 1.0e30f;
    for (int a = 0; a < 3; ++a) {
        const float o = e[a];
        const float dv = d[a];
        if (std::fabs(dv) < 1.0e-6f) {
            if (o < -pad[a] || o > pad[a]) {
                t_near = 1.0f;   // луч параллелен плите и вне её — мимо
                t_far = -1.0f;
                break;
            }
            continue;
        }
        float t0 = (-pad[a] - o) / dv;
        float t1 = (pad[a] - o) / dv;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        t_near = std::max(t_near, t0);
        t_far = std::min(t_far, t1);
    }
    if (t_near <= t_far && t_far >= 0.0f) {
        hit.looking = true;
        // ВХОД, А НЕ ВЫХОД: t_near < 0 значит «глаз уже внутри», и тогда
        // расстояние до цели вдоль луча ноль — это честнее, чем отрицательное.
        hit.t_m = std::max(0.0f, t_near);
    }
    hit.ok = hit.in_reach && hit.looking;
    return hit;
}

} // namespace dfn::app
