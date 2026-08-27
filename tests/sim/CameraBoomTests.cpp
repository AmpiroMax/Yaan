/*
Created: 27:08:2026 - 11:56:36
Last updated: 27:08:2026 - 11:56:36
Module: tests/sim
File: tests/sim/CameraBoomTests.cpp

Responsibility:
- Стрела камеры третьего лица (engine/gameplay/sources/CameraBoom.h): куда она
  смотрит, как читает ответ щупа и почему сглаживание НЕСИММЕТРИЧНО.

Notes:
- Ни одного физического стенда: арифметика стрелы отделена от единственного
  вызова sphere_cast ровно для того, чтобы её можно было проверить числами.
  Живой замер (щуп против оболочки дома) лежит в docs/acceptance.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 27:08:2026 - 11:56:36: Заведён вместе со стрелой (заказ владельца 27.08).
*/

#include <doctest/doctest.h>

#include "engine/gameplay/sources/CameraBoom.h"

#include <cmath>

using dfn::gameplay::CameraBoomDesc;
using dfn::gameplay::CameraBoomState;
using dfn::gameplay::camera_boom_aim;
using dfn::gameplay::camera_boom_free_length;
using dfn::gameplay::camera_boom_step;
using dfn::platform::RayHit;

TEST_CASE("стрела смотрит НАЗАД и ВВЕРХ, и её длина зависит от тангажа") {
    const CameraBoomDesc d;
    // Рыск 0 -- взгляд на -Z (соглашение углов заморожено, FirstPersonCamera).
    // Значит камера стоит на +Z позади и выше.
    const auto level = camera_boom_aim(0.0f, 0.0f, d);
    CHECK(level.direction.z > 0.9f);
    CHECK(level.direction.y > 0.0f);
    CHECK(level.reach == doctest::Approx(std::sqrt(3.2f * 3.2f + 0.55f * 0.55f)).epsilon(1e-4));

    // ТА САМАЯ ПРИЧИНА, ПО КОТОРОЙ ДЛИНА ВОЗВРАЩАЕТСЯ ВМЕСТЕ С НАПРАВЛЕНИЕМ:
    // при взгляде отвесно вниз отвод назад и подъём складываются в одну
    // сторону, и стрела становится длиннее на весь подъём. Функция, отдающая
    // только направление, заставила бы вызывающего считать длину самому.
    const auto down = camera_boom_aim(0.0f, -1.5707963f, d);
    CHECK(down.reach == doctest::Approx(3.2f + 0.55f).epsilon(1e-3));
    CHECK(down.reach > level.reach);

    // Направление единичное на всех тангажах, иначе камера уезжала бы дальше
    // измеренного щупом.
    for (const float p : {-1.2f, -0.6f, 0.0f, 0.6f, 1.2f}) {
        const auto a = camera_boom_aim(0.7f, p, d);
        const float len = std::sqrt(a.direction.x * a.direction.x
                                    + a.direction.y * a.direction.y
                                    + a.direction.z * a.direction.z);
        CHECK(len == doctest::Approx(1.0f).epsilon(1e-4));
    }
}

TEST_CASE("промах щупа даёт полную длину, касание -- расстояние минус отступ") {
    const CameraBoomDesc d;
    const float reach = camera_boom_aim(0.0f, 0.0f, d).reach;

    RayHit miss;
    CHECK(camera_boom_free_length(miss, reach, d) == doctest::Approx(reach));

    RayHit near_wall;
    near_wall.hit = true;
    near_wall.distance = 1.20f;
    CHECK(camera_boom_free_length(near_wall, reach, d)
          == doctest::Approx(1.20f - d.margin));

    // ПРИЖАТИЕ К ГОЛОВЕ -- ЗАКОННЫЙ ИСХОД, а не сбой: владелец назвал именно
    // его («приближением камеры к голове персонажа»). Отрицательной длины при
    // этом не бывает -- камера не имеет права оказаться ВПЕРЕДИ головы.
    RayHit touching;
    touching.hit = true;
    touching.distance = 0.0f;
    CHECK(camera_boom_free_length(touching, reach, d) == doctest::Approx(0.0f));

    // И касание ДАЛЬШЕ полной длины не удлиняет стрелу.
    RayHit far_hit;
    far_hit.hit = true;
    far_hit.distance = reach + 5.0f;
    CHECK(camera_boom_free_length(far_hit, reach, d) == doctest::Approx(reach));
}

TEST_CASE("сглаживание НЕСИММЕТРИЧНО: укорочение мгновенно, выпуск постепенный") {
    const CameraBoomDesc d;
    const float reach = camera_boom_aim(0.0f, 0.0f, d).reach;

    CameraBoomState s;
    // Первый кадр вида встаёт на свободную длину без взлёта.
    CHECK(camera_boom_step(s, reach, 1.0f / 60.0f, d) == doctest::Approx(reach));

    // СТЕНА ПОЯВИЛАСЬ -- НИ ОДНОГО КАДРА СНАРУЖИ. Это и есть приёмочное
    // требование заказа: «protrusion сквозь стену недопустим ни в одном
    // кадре», а любая инерция на сокращении есть такой кадр.
    CHECK(camera_boom_step(s, 0.80f, 1.0f / 60.0f, d) == doctest::Approx(0.80f));

    // ...И ОБРАТНО СТРЕЛА ИДЁТ ПОСТЕПЕННО. Контроль, без которого проверка
    // выше проходит и у бесхитростного «length = free_length», у которого
    // возврат такой же мгновенный и камера дёргается.
    const float one = camera_boom_step(s, reach, 1.0f / 60.0f, d);
    CHECK(one > 0.80f);
    CHECK(one < reach);

    // Экспонента доходит до цели, а не застревает: за секунду при темпе 10 1/с
    // остаток меньше сотой доли метра.
    CameraBoomState slow;
    slow.length = 0.80f;
    float v = 0.80f;
    for (int i = 0; i < 60; ++i) {
        v = camera_boom_step(slow, reach, 1.0f / 60.0f, d);
    }
    CHECK(v == doctest::Approx(reach).epsilon(0.01));

    // И ОДИН И ТОТ ЖЕ ОТРЕЗОК ВРЕМЕНИ, ПРОЙДЕННЫЙ РАЗНЫМ ЧИСЛОМ КАДРОВ, ДАЁТ
    // ОДНУ ДЛИНУ: возврат, написанный как «прибавить долю разности за кадр»,
    // на 120 кадрах в секунду шёл бы вдвое быстрее, чем на 60.
    CameraBoomState a;
    a.length = 0.80f;
    CameraBoomState b;
    b.length = 0.80f;
    float va = 0.0f;
    float vb = 0.0f;
    for (int i = 0; i < 30; ++i) {
        va = camera_boom_step(a, reach, 1.0f / 30.0f, d);
    }
    for (int i = 0; i < 60; ++i) {
        vb = camera_boom_step(b, reach, 1.0f / 60.0f, d);
    }
    CHECK(va == doctest::Approx(vb).epsilon(1e-3));
}
