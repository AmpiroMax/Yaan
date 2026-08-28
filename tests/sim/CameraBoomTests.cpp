/*
Created: 27:08:2026 - 11:56:36
Last updated: 28:08:2026 - 18:50:00
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
- 28:08:2026 - 18:50:00: рукав насеста (коммит ed8bf25): начало стрелы над телом, лежащим на
  мебели, потолок тангажа, нулевая доля бит-в-бит и та самая находка —
  щуп от глаза лежащего идёт ВНИЗ, в матрас, — предъявленная числом.
*/

#include <doctest/doctest.h>

#include "engine/gameplay/sources/CameraBoom.h"

#include <cmath>

#include <glm/vec3.hpp>

using dfn::gameplay::CameraBoomDesc;
using dfn::gameplay::CameraBoomState;
using dfn::gameplay::camera_boom_aim;
using dfn::gameplay::camera_boom_free_length;
using dfn::gameplay::camera_boom_perch;
using dfn::gameplay::camera_boom_step;
using dfn::gameplay::POSTURE_BOOM_PITCH_MAX;
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

TEST_CASE("в позе стрела начинается НАД ТЕЛОМ, а не в глазу у матраса") {
    // ЧИСЛА ЛОКАЦИИ ПРИЁМКИ (x90z90, кровать реестра furn-bed): пол комнаты на
    // 0, матрас на 0.50, ВЕРХ ПРЕДМЕТА — столбики изголовья — на 1.14 (замер
    // из assets/houses/INDEX.txt), глаз лежащего на 0.751. Насест считает app
    // по габариту предмета: 1.14 + радиус щупа 0.25 + отступ 0.05 = 1.44.
    const glm::vec3 eye_lie{2.0f, 0.751f, 5.0f};
    const glm::vec3 perch{2.0f, 1.44f, 5.30f}; // над серединой кровати
    const float looking_up = 1.0f;             // лежащий смотрит в потолок

    // НУЛЕВАЯ ДОЛЯ — ПРЕЖНЯЯ КАМЕРА БИТ-В-БИТ. Без этого плеча правка поз
    // тихо переехала бы всему третьему лицу (Rule 48).
    const auto none = camera_boom_perch(eye_lie, looking_up, perch, POSTURE_BOOM_PITCH_MAX, 0.0f);
    CHECK(none.origin.x == doctest::Approx(eye_lie.x));
    CHECK(none.origin.y == doctest::Approx(eye_lie.y));
    CHECK(none.origin.z == doctest::Approx(eye_lie.z));
    CHECK(none.pitch == doctest::Approx(looking_up));

    // И ВОТ ТА САМАЯ НАХОДКА, ИЗ-ЗА КОТОРОЙ ЭТА ФУНКЦИЯ ЕСТЬ: щуп от глаза
    // лежащего идёт ВНИЗ (потому что «назад по взгляду», а взгляд в потолок),
    // то есть в матрас. Отрицательное плечо предъявляется здесь числом.
    const CameraBoomDesc d;
    CHECK(camera_boom_aim(0.0f, none.pitch, d).direction.y < 0.0f);

    // ПОЛНАЯ ДОЛЯ: начало над предметом, и стрела идёт ВВЕРХ-НАЗАД.
    const auto full = camera_boom_perch(eye_lie, looking_up, perch, POSTURE_BOOM_PITCH_MAX, 1.0f);
    CHECK(full.origin.y == doctest::Approx(1.44f));
    CHECK(full.origin.z == doctest::Approx(5.30f));
    CHECK(full.pitch == doctest::Approx(POSTURE_BOOM_PITCH_MAX));
    CHECK(camera_boom_aim(0.0f, full.pitch, d).direction.y > 0.0f);
    // Начало поднялось ВЫШЕ СТОЛБИКОВ на радиус щупа с отступом — сфера щупа
    // начинает свободной от самой кровати, а это и было причиной схлопывания.
    CHECK(full.origin.y - 1.14f >= d.probe_radius + d.margin);

    // ...И У СИДЯЩЕГО НИЧЕГО НЕ ПОДНИМАЕТСЯ. Его глаз (1.26 над полом) и так
    // выше верха лавки (0.45 + 0.30 = 0.75), а поднимать камеру там незачем:
    // кадр сидящего снят до этой волны и обязан остаться прежним по высоте.
    const glm::vec3 eye_sit{2.0f, 1.261f, 5.0f};
    const glm::vec3 bench{2.0f, 0.75f, 5.10f};
    // ...И ТАНГАЖ СИДЯЩЕМУ НЕ ТРОГАЮТ: app выдаёт ему потолком предел камеры,
    // то есть ничего. Проверяется тем же вызовом с тем же смыслом — «потолок,
    // который не связывает».
    const auto sit = camera_boom_perch(eye_sit, -0.3f, bench, 1.5f, 1.0f);
    CHECK(sit.origin.y == doctest::Approx(eye_sit.y));
    CHECK(sit.pitch == doctest::Approx(-0.3f));

    // ПОТОЛОК — ИМЕННО ПОТОЛОК: смотреть НИЖЕ (то есть подниматься выше над
    // телом) он не мешает. Иначе это был бы фиксированный ракурс, а не обвод.
    CHECK(camera_boom_perch(eye_lie, -0.9f, perch, POSTURE_BOOM_PITCH_MAX, 1.0f).pitch
          == doctest::Approx(-0.9f));

    // Середина — между концами: вход в позу не дёргает камеру.
    const auto half = camera_boom_perch(eye_lie, looking_up, perch, POSTURE_BOOM_PITCH_MAX, 0.5f);
    CHECK(half.origin.y > eye_lie.y);
    CHECK(half.origin.y < full.origin.y);
    CHECK(half.pitch < looking_up);
    CHECK(half.pitch > full.pitch);
}
