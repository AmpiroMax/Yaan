/*
Module: tests/app
File: tests/app/DoorAimTests.cpp

Responsibility:
- ПРИЦЕЛ ДВЕРИ = РАДИУС + ВЗГЛЯД, и обе половины проверяются вместе со своими
  отвергнутыми образцами (правило 30). Отвергнутый образец здесь НАСТОЯЩИЙ, а
  не синтетический: это поза, в которой прежний прицел зажигал подсказку
  «Выйти» без всякого взгляда, — глаз вошедшего в 0.50 м от створки, смотрящий
  в противоположную стену (крит владельца 28.08).

Key items:
- «смотрю в полотно» — горит; «смотрю на стену рядом» — молчит.
- Голова ВНУТРИ прежней коробки прицела, взгляд от двери — молчит.
- Радиус: та же поза дальше 1.2 м от полотна — молчит.
- Переход без полотна: конус вместо прямоугольника, обе руки.

Dependencies:
- Uses: doctest, engine/app/sources/DoorAim.h. Ни App, ни окна, ни физики.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Каждое утверждение здесь ходит парой с образцом, который обязан его
  провалить: утверждение, которое ничто не проваливает, — это описание.
*/

#include <doctest/doctest.h>

#include <cmath>

#include "engine/app/sources/DoorAim.h"

namespace {

using dfn::app::DoorAim;
using dfn::app::DoorAimHit;

/// СТВОРКА ЛОКАЦИИ ВАЙТРАНА x100z64, ЧИСЛАМИ ИЗ ФАЙЛА: полотно у z = 7.7,
/// комната по −z от него, потому наружная нормаль (0,0,1). Ширина 1.1 м,
/// высота 2.0 м — габарит дверного полотна рецепта.
[[nodiscard]] DoorAim leaf() {
    DoorAim a;
    a.at = {7.5f, 2.72f, 7.7f};
    a.normal = {0.0f, 0.0f, 1.0f};
    a.half_w = 0.55f;
    a.half_h = 1.00f;
    a.reach_m = dfn::app::DOOR_REACH_M;
    a.leaf = true;
    return a;
}

/// Взгляд по конвенции PlayerMovement: рыск 0 — в −Z, положительный — по
/// часовой сверху; положительный тангаж — вверх.
[[nodiscard]] glm::vec3 look(float yaw, float pitch = 0.0f) {
    const float cp = std::cos(pitch);
    return {std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
}

/// ГЛАЗ ВОШЕДШЕГО: точка входа локации (7.5, 1.82, 7.2) плюс рост глаза 1.7.
constexpr glm::vec3 EYE_INSIDE{7.5f, 3.52f, 7.2f};

} // namespace

TEST_CASE("прицел двери: смотрю в полотно — горит, смотрю на стену рядом — нет") {
    const DoorAim a = leaf();
    // ДВЕРЬ ПОЗАДИ ВОШЕДШЕГО: она по +Z от него, значит рыск 180°.
    const DoorAimHit at_door = dfn::app::door_aim(a, EYE_INSIDE, look(3.14159265f));
    CHECK(at_door.in_reach);
    CHECK(at_door.looking);
    CHECK(at_door.ok);
    // Радиус мерится до БЛИЖАЙШЕЙ точки полотна: глаз в 0.50 м от плоскости и
    // ровно против середины по горизонтали, по высоте — внутри габарита.
    // ЯВНЫЕ ГРАНИЦЫ, НЕ Approx().epsilon(): правило 40 — допуск doctest'а
    // раздут множителем (1 + |x|), и на метрах зазора он значит не то, что
    // читается.
    CHECK(at_door.distance_m > 0.49f);
    CHECK(at_door.distance_m < 0.51f);

    // ОТВЕРГНУТЫЙ ОБРАЗЕЦ 1: стена в полуметре вбок. Тот же глаз, тот же
    // радиус, повёрнута только голова — то есть провалить это может ТОЛЬКО
    // проверка взгляда.
    const float side_yaw = 3.14159265f
                         - std::atan2(a.half_w + 0.5f, 0.5f); // 1.05 м вбок, 0.5 вперёд
    const DoorAimHit at_wall = dfn::app::door_aim(a, EYE_INSIDE, look(side_yaw));
    CHECK(at_wall.in_reach); // радиус тот же самый
    CHECK_FALSE(at_wall.looking);
    CHECK_FALSE(at_wall.ok);
}

TEST_CASE("прицел двери: голова внутри прежней коробки, взгляд от двери — молчит") {
    // ЭТО И ЕСТЬ ДЕФЕКТ, НА КОТОРЫЙ ПОЖАЛОВАЛСЯ ВЛАДЕЛЕЦ. Прежняя коробка
    // прицела стояла кубом 1.10x1.10x1.10 вокруг точки перехода (7.5, 2.72,
    // 7.7) и накрывала глаз вошедшего (7.5, 3.52, 7.2): |dx|=0, |dy|=0.80,
    // |dz|=0.50 — внутри по всем трём осям. Луч, начатый внутри выпуклого
    // тела, Jolt засчитывает попаданием на нулевой доле, и подсказка «Выйти»
    // горела под любым углом.
    const glm::vec3 d = EYE_INSIDE - leaf().at;
    REQUIRE(std::fabs(d.x) < 1.10f);
    REQUIRE(std::fabs(d.y) < 1.10f);
    REQUIRE(std::fabs(d.z) < 1.10f);

    // Взгляд ровно в противоположную стену (рыск 0 — в −Z, дверь по +Z).
    const DoorAimHit away = dfn::app::door_aim(leaf(), EYE_INSIDE, look(0.0f));
    CHECK(away.in_reach);
    CHECK_FALSE(away.looking);
    CHECK_FALSE(away.ok);
    CHECK(away.t_m == 0.0f); // луч в плоскость не приходит вовсе

    // И вбок, вдоль стены: полотно не позади, а РЯДОМ — луч идёт параллельно
    // плоскости и не пересекает её вовсе.
    const DoorAimHit along = dfn::app::door_aim(leaf(), EYE_INSIDE, look(1.5707963f));
    CHECK_FALSE(along.ok);
}

TEST_CASE("прицел двери: радиус — та же поза дальше 1.2 м молчит") {
    const DoorAim a = leaf();
    const glm::vec3 to_door = look(3.14159265f);
    // Отходим по нормали, взгляд НЕ меняется: провалить это может только
    // радиус (обратная пара к проверке взгляда выше).
    const DoorAimHit near_eye = dfn::app::door_aim(a, {7.5f, 3.52f, 6.60f}, to_door);
    CHECK(near_eye.looking);
    CHECK(near_eye.in_reach); // 1.10 м — внутри 1.20
    CHECK(near_eye.ok);

    const DoorAimHit far_eye = dfn::app::door_aim(a, {7.5f, 3.52f, 6.40f}, to_door);
    CHECK(far_eye.looking); // смотрит по-прежнему точно в дверь
    CHECK_FALSE(far_eye.in_reach); // 1.30 м
    CHECK_FALSE(far_eye.ok);

    // ПОРОГ СТОИТ ВЫШЕ ХУДШЕЙ ИЗМЕРЕННОЙ ТОЧКИ ВХОДА, а не под неё подогнан:
    // живой обход 44 локаций обоих городов даёт от глаза вошедшего до полотна
    // обратной двери 0.80 м во всех 44 (запас 0.40 м).
    CHECK(dfn::app::DOOR_REACH_M > 0.80f);
    // ...и НИЖЕ прежнего радиуса 1.6 м, иначе владелец не заметил бы разницы.
    CHECK(dfn::app::DOOR_REACH_M < 1.6f);
}

TEST_CASE("прицел двери: высокая створка не удлиняет руку") {
    // РАДИУС ДО БЛИЖАЙШЕЙ ТОЧКИ, А НЕ ДО СЕРЕДИНЫ. У створки вдвое выше
    // середина уезжает от глаза, и радиус, отмеренный до неё, у высокой двери
    // молча длиннее. Контроль — тот же глаз, те же 0.5 м до плоскости.
    DoorAim tall = leaf();
    tall.half_h = 2.0f;
    tall.at.y = 3.72f; // середина уехала на метр вверх
    const DoorAimHit h = dfn::app::door_aim(tall, EYE_INSIDE, look(3.14159265f));
    CHECK(h.distance_m > 0.49f);
    CHECK(h.distance_m < 0.51f);
    CHECK(h.ok);
}

TEST_CASE("переход без полотна: конус, обе руки") {
    DoorAim point;
    point.at = {0.0f, 1.7f, 0.0f};
    point.reach_m = dfn::app::DOOR_REACH_M;
    point.leaf = false;

    // Глаз в метре по +Z от точки: смотреть на неё — значит смотреть в −Z, то
    // есть рыск 0 (конвенция PlayerMovement).
    const glm::vec3 eye{0.0f, 1.7f, 1.0f};
    CHECK(dfn::app::door_aim(point, eye, look(0.0f)).ok);          // смотрю на неё
    CHECK_FALSE(dfn::app::door_aim(point, eye, look(3.14159265f)).ok); // прочь
    CHECK_FALSE(dfn::app::door_aim(point, eye, look(1.5707963f)).ok);  // вбок
    // Радиус: два метра — не дотянуться, как бы точно ни целился.
    CHECK_FALSE(dfn::app::door_aim(point, {0.0f, 1.7f, 2.0f}, look(0.0f)).ok);
    // ГЛАЗ В САМОЙ ТОЧКЕ — не «да», а «нет»: направления на неё не существует.
    CHECK_FALSE(dfn::app::door_aim(point, point.at, look(0.0f)).ok);
}

TEST_CASE("коробка прицела и вердикт — одни и те же четыре числа") {
    // Поворот коробки и касательная вердикта обязаны согласоваться: местный +Z
    // уходит в нормаль, местный +X — в касательную. Расхождение здесь означало
    // бы, что тело стоит поперёк того прямоугольника, по которому судят.
    for (const float yaw : {0.0f, 0.7f, 1.5707963f, 2.4f, -1.1f}) {
        const glm::vec3 n{std::sin(yaw), 0.0f, std::cos(yaw)};
        const float a = dfn::app::door_leaf_yaw(n);
        const glm::vec3 local_x{std::cos(a), 0.0f, -std::sin(a)};
        const glm::vec3 local_z{std::sin(a), 0.0f, std::cos(a)};
        const glm::vec3 t = dfn::app::door_aim_tangent(n);
        CHECK(std::fabs(local_z.x - n.x) < 1e-4f);
        CHECK(std::fabs(local_z.z - n.z) < 1e-4f);
        CHECK(std::fabs(local_x.x - t.x) < 1e-4f);
        CHECK(std::fabs(local_x.z - t.z) < 1e-4f);
    }
}
