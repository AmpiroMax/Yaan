/*
Module: engine/app
File: engine/app/sources/DoorAim.cpp

Responsibility:
- Геометрия прицела двери: ближайшая точка полотна (радиус) и пересечение луча
  взгляда с прямоугольником створки (взгляд).

Dependencies:
- Uses: DoorAim.h, glm, <cmath>.
- Used by: AppInterior.cpp, tests/app/DoorAimTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона lead владеет этим файлом.
*/

#include "engine/app/sources/DoorAim.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace dfn::app {

glm::vec3 door_aim_tangent(const glm::vec3& normal) {
    // ПОВОРОТ НА 90° ВОКРУГ ВЕРТИКАЛИ, и он обязан совпасть с тем, во что
    // угол door_leaf_yaw переводит местный +X коробки: (cos a, 0, -sin a) при
    // sin a = n.x, cos a = n.z даёт ровно (n.z, 0, -n.x).
    const glm::vec3 t{normal.z, 0.0f, -normal.x};
    const float len = glm::length(t);
    if (len < 1e-4f) {
        // Нормаль смотрит вертикально — у полотна это дефект геометрии, а не
        // законное состояние. Отвечаем осью X: вердикт станет ложным по
        // прямоугольнику, а не непредсказуемым по NaN.
        return {1.0f, 0.0f, 0.0f};
    }
    return t / len;
}

float door_leaf_yaw(const glm::vec3& normal) {
    return std::atan2(normal.x, normal.z);
}

DoorAimHit door_aim(const DoorAim& aim, const glm::vec3& eye, const glm::vec3& dir) {
    DoorAimHit hit;
    const glm::vec3 d = eye - aim.at;

    if (!aim.leaf) {
        // ПЕРЕХОД БЕЗ ПОЛОТНА: конус вместо прямоугольника. Радиус меряется до
        // самой точки — другой поверхности у неё нет.
        const float dist = glm::length(d);
        hit.distance_m = dist;
        hit.in_reach = dist <= aim.reach_m;
        if (dist > 1e-4f) {
            const float c = glm::dot(dir, -d / dist);
            hit.looking = c >= DOOR_POINT_CONE_COS;
            hit.t_m = hit.looking ? dist : 0.0f;
        } else {
            // Глаз В САМОЙ точке перехода: направления «на неё» не существует,
            // и молчаливое «да» здесь было бы тем же дефектом, что коробка
            // вокруг головы. Взгляда нет.
            hit.looking = false;
        }
        hit.ok = hit.in_reach && hit.looking;
        return hit;
    }

    const glm::vec3 n = aim.normal;
    const glm::vec3 t = door_aim_tangent(n);

    // --- РАДИУС: до БЛИЖАЙШЕЙ ТОЧКИ ПОЛОТНА, а не до его середины ------------
    // Разница не косметическая: у створки в два метра ростом середина отстоит
    // от глаза на полметра дальше, чем сама створка, и радиус, отмеренный до
    // середины, у высокой двери молча длиннее, чем у низкой. Ближайшая точка
    // прямоугольника — это проекция глаза на него с зажимом в габарит.
    const float u = glm::dot(d, t);
    const float v = d.y;
    const float w = glm::dot(d, n);
    const float cu = std::clamp(u, -aim.half_w, aim.half_w);
    const float cv = std::clamp(v, -aim.half_h, aim.half_h);
    const glm::vec3 nearest = aim.at + t * cu + glm::vec3{0.0f, cv, 0.0f};
    hit.distance_m = glm::length(eye - nearest);
    hit.in_reach = hit.distance_m <= aim.reach_m;

    // --- ВЗГЛЯД: луч из глаза пересекает прямоугольник створки ---------------
    // Плоскость полотна: dot(p - at, n) = 0. Луч p = eye + dir * t_m.
    const float denom = glm::dot(dir, n);
    if (std::fabs(denom) > 1e-4f) {
        const float t_m = -w / denom;
        // t_m <= 0 — полотно ПОЗАДИ глаза (или ровно в нём). Смотреть на дверь
        // спиной нельзя, и «стою в проёме, гляжу вдоль стены» — это тоже не
        // взгляд на створку: декоративная дверь в коллайдер не входит, в её
        // проёме стоят, и без этой строки прицел там горел бы всегда.
        if (t_m > 0.0f) {
            const glm::vec3 p = eye + dir * t_m;
            const glm::vec3 pd = p - aim.at;
            hit.u = glm::dot(pd, t);
            hit.v = pd.y;
            hit.looking = std::fabs(hit.u) <= aim.half_w + DOOR_AIM_PAD_M
                       && std::fabs(hit.v) <= aim.half_h + DOOR_AIM_PAD_M;
            hit.t_m = t_m;
        }
    }
    hit.ok = hit.in_reach && hit.looking;
    return hit;
}

} // namespace dfn::app
