/*
Module: tests/app
File: tests/app/ThirdPersonRigTests.cpp

Responsibility:
- ПРИЁМКА СВОБОДНОГО ОБЛЁТА (заказ владельца 31.08, пункт 4). Три утверждения,
  которые кадр предъявить не может: стоя рыск тела НЕ МЕНЯЕТСЯ от мыши; при
  движении тело идёт к направлению «камера + ввод» с ограниченной скоростью; и
  мировое направление шага равно этому направлению В ЛЮБОЙ МОМЕНТ доворота, а
  не только в его конце.

Dependencies:
- Uses: engine/app/sources/ThirdPersonRig.h, generated constants, doctest.
- Used by: ctest (app_third_person).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- У каждой вилки — КОНТРОЛЬНАЯ рука (правило 30): утверждение «рыск не
  изменился» без руки, в которой он обязан измениться, неотличимо от
  утверждения «функция ничего не делает».
*/

#include <doctest/doctest.h>

#include "engine/app/sources/ThirdPersonRig.h"
#include "engine/core/config/sources/Constants.h"

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

namespace {

using dfn::app::axes_in_body_frame;
using dfn::app::move_yaw_from_axes;
using dfn::app::shortest_arc;
using dfn::app::third_person_step;
using dfn::app::ThirdPersonStep;
using dfn::app::turn_body_toward;

constexpr float RATE = static_cast<float>(dfn::config::BODY_TURN_RATE);
constexpr float DT = 1.0f / 60.0f;

/// КУДА НА САМОМ ДЕЛЕ ПОЙДЁТ ТЕЛО, посчитанное КАК В СИМУЛЯЦИИ и не иначе:
/// forward = (sin yaw, -cos yaw), right = (cos yaw, sin yaw), шаг =
/// right*x + forward*y (PlayerMovement.cpp). Написано здесь ещё раз нарочно —
/// прибор, зовущий ту же функцию, что и предмет, меряет согласие функции с
/// собой (правило 46).
[[nodiscard]] float world_move_yaw(float body_yaw, const glm::vec2& axes) {
    const glm::vec2 forward{std::sin(body_yaw), -std::cos(body_yaw)};
    const glm::vec2 right{std::cos(body_yaw), std::sin(body_yaw)};
    const glm::vec2 dir = right * axes.x + forward * axes.y;
    return std::atan2(dir.x, -dir.y);
}

} // namespace

TEST_CASE("standing_still_the_mouse_moves_the_camera_and_not_the_man") {
    // ПЕРВАЯ ПОЛОВИНА ЗАКАЗА, дословно: «стоя персонаж НЕ поворачивается за
    // камерой». Проверяется на ПОЛНОМ КРУГЕ азимутов, а не на одном: азимут,
    // совпавший с рыском тела, дал бы ноль по построению.
    const float body = 0.7f;
    for (int i = 0; i < 36; ++i) {
        const float cam = glm::two_pi<float>() * float(i) / 36.0f;
        CAPTURE(cam);
        const ThirdPersonStep s = third_person_step(body, cam, {0.0f, 0.0f}, DT, RATE);
        CHECK(s.body_yaw == body); // БИТ-В-БИТ, а не «почти»
        CHECK(s.move_axes.x == 0.0f);
        CHECK(s.move_axes.y == 0.0f);
        CHECK_FALSE(s.turning);
    }
    // КОНТРОЛЬНАЯ РУКА: тот же вызов с нажатым «вперёд» ОБЯЗАН развернуть
    // тело. Без неё «рыск не изменился» неотличимо от «функция пустая».
    const ThirdPersonStep moved =
        third_person_step(body, body + glm::pi<float>(), {0.0f, 1.0f}, DT, RATE);
    CHECK(moved.body_yaw != body);
    CHECK(moved.turning);
    // И МЁРТВАЯ ЗОНА — ПРО ВЕЛИЧИНУ, А НЕ ПРО НОЛЬ: полунажатый стик это
    // движение, дрожь стика — нет.
    CHECK(third_person_step(body, 0.0f, {0.0f, 0.5f}, DT, RATE).turning);
    CHECK_FALSE(third_person_step(body, 0.0f, {0.0f, 0.01f}, DT, RATE).turning);
}

TEST_CASE("the_body_turns_toward_the_camera_at_a_bounded_rate") {
    // ВТОРАЯ ПОЛОВИНА: «ПЛАВНО, ограниченная скорость поворота». Худший случай
    // — полкруга: нажать «назад», когда камера смотрит вперёд.
    const float cam = 0.0f;
    float body = glm::pi<float>(); // ровно спиной к тому, куда попросят идти
    const float want = cam;        // «вперёд» = азимут камеры
    int steps = 0;
    float worst_step = 0.0f;
    while (std::abs(shortest_arc(body, want)) > 1.0e-4f && steps < 10000) {
        const ThirdPersonStep s = third_person_step(body, cam, {0.0f, 1.0f}, DT, RATE);
        worst_step = std::max(worst_step, std::abs(shortest_arc(body, s.body_yaw)));
        body = s.body_yaw;
        ++steps;
    }
    const float seconds = float(steps) * DT;
    CAPTURE(worst_step);
    CAPTURE(seconds);
    MESSAGE("разворот на полкруга: " << seconds << " с, худший шаг "
                                     << worst_step * 57.29578f << " град за кадр");
    // НИ ОДИН КАДР НЕ ПЕРЕПРЫГИВАЕТ ПОТОЛОК. Допуск — одна миллионная, а не
    // проценты: это арифметика, а не измерение.
    CHECK(worst_step <= RATE * DT + 1.0e-6f);
    // И ВРЕМЯ РАЗВОРОТА — ТО, ЧТО НАПИСАНО В ПАСПОРТЕ СТРОКИ (NUMBERS.md):
    // не дольше ОДНОГО ШАГА ХОДЬБЫ (иначе тело идёт целый шаг боком) и не
    // быстрее ОДНОГО ШАГА БЕГА (иначе на бегу ноги перекрещиваются в кадре).
    const float walk_step_s =
        static_cast<float>(dfn::config::STEP_LENGTH_BASE
                           + dfn::config::STEP_LENGTH_PER_MPS * dfn::config::WALK_SPEED)
        / static_cast<float>(dfn::config::WALK_SPEED);
    const float run_step_s =
        static_cast<float>(dfn::config::STEP_LENGTH_BASE
                           + dfn::config::STEP_LENGTH_PER_MPS * dfn::config::RUN_SPEED)
        / static_cast<float>(dfn::config::RUN_SPEED);
    CAPTURE(walk_step_s);
    CAPTURE(run_step_s);
    CHECK(seconds <= walk_step_s);
    CHECK(seconds >= run_step_s - DT);
    // И ЗАКАЗАННАЯ ВИЛКА 360-540 град/с — она названа владельцем, и строка
    // обязана лежать внутри неё, а не рядом.
    const float deg_s = RATE * 57.29578f;
    CAPTURE(deg_s);
    CHECK(deg_s >= 360.0f);
    CHECK(deg_s <= 540.0f);
    // КОНТРОЛЬНАЯ РУКА ПОТОЛКА: без ограничения (потолок в сто раз выше) тот
    // же разворот кончается за ОДИН кадр. Иначе «шаг не больше потолка» может
    // держаться просто потому, что доворачивать было нечего.
    const ThirdPersonStep instant = third_person_step(glm::pi<float>(), cam,
                                                      {0.0f, 1.0f}, DT, 100.0f * RATE);
    CHECK(std::abs(shortest_arc(instant.body_yaw, want)) < 1.0e-4f);
}

TEST_CASE("the_step_goes_where_the_camera_and_the_keys_say_from_the_first_frame") {
    // ТРЕТЬЕ УТВЕРЖДЕНИЕ, И ОНО ЖЕ САМОЕ ЛЁГКОЕ ПОТЕРЯТЬ: пока тело
    // доворачивается, идти оно обязано УЖЕ туда, куда попросили. Иначе прямая,
    // набранная на клавиатуре, рисуется дугой — ровно то, что игрок читает как
    // «управление вязкое».
    struct Key {
        glm::vec2 axes;
        float want_offset_deg;
        const char* label;
    };
    const Key keys[] = {
        {{0.0f, 1.0f}, 0.0f, "вперёд = от камеры"},
        {{0.0f, -1.0f}, 180.0f, "назад"},
        {{1.0f, 0.0f}, 90.0f, "вправо (страйф)"},
        {{-1.0f, 0.0f}, -90.0f, "влево"},
        {{1.0f, 1.0f}, 45.0f, "вперёд-вправо"},
    };
    for (const Key& k : keys) {
        CAPTURE(std::string(k.label));
        for (int i = 0; i < 12; ++i) {
            const float cam = glm::two_pi<float>() * float(i) / 12.0f;
            // РЫСК ТЕЛА БЕРЁТСЯ ПОПЕРЁК ВСЕГО КРУГА, включая тот, что уже
            // совпал с целью: утверждение обязано держаться и на первом кадре
            // разворота, и на последнем.
            for (int j = 0; j < 12; ++j) {
                const float body = glm::two_pi<float>() * float(j) / 12.0f;
                const ThirdPersonStep s =
                    third_person_step(body, cam, k.axes, DT, RATE);
                const float want = cam + k.want_offset_deg / 57.29578f;
                CAPTURE(cam);
                CAPTURE(body);
                CHECK(std::abs(shortest_arc(s.want_yaw, want)) < 1.0e-4f);
                // И ШАГ ИДЁТ ТУДА ЖЕ, посчитанный формулой СИМУЛЯЦИИ.
                CHECK(std::abs(shortest_arc(world_move_yaw(s.body_yaw, s.move_axes),
                                            want))
                      < 1.0e-4f);
                // ДЛИНА ОСЕЙ СОХРАНЕНА: sim нормирует сама и только сверх
                // единицы, поэтому нормировка здесь сделала бы полунажатый
                // стик полным.
                CHECK(glm::length(s.move_axes)
                      == doctest::Approx(glm::length(k.axes)).epsilon(1.0e-5));
            }
        }
    }
    // КОНТРОЛЬНАЯ РУКА: оси, НЕ переписанные в систему тела (то есть прежнее
    // «иду туда, куда смотрю телом»), обязаны промахнуться мимо цели — и на
    // полкруга, когда тело ещё не довернулось.
    const float cam = 0.0f;
    const float body = glm::pi<float>();
    const ThirdPersonStep s = third_person_step(body, cam, {0.0f, 1.0f}, DT, RATE);
    const float naive = world_move_yaw(s.body_yaw, glm::vec2{0.0f, 1.0f});
    CHECK(std::abs(shortest_arc(naive, cam)) > 3.0f);
}

TEST_CASE("the_two_helpers_say_what_their_names_say") {
    // МЕЛОЧЬ, КОТОРАЯ ЛОМАЕТ ВСЁ ОСТАЛЬНОЕ МОЛЧА: знак угла ввода. atan2(x, y)
    // против atan2(y, x) — одна перестановка, и «вправо» становится «вперёд».
    CHECK(move_yaw_from_axes({0.0f, 1.0f}) == doctest::Approx(0.0f));
    CHECK(move_yaw_from_axes({1.0f, 0.0f})
          == doctest::Approx(glm::half_pi<float>()));
    CHECK(move_yaw_from_axes({0.0f, -1.0f}) == doctest::Approx(glm::pi<float>()));
    // Кратчайшая дуга обязана идти через ноль, а не длинным путём.
    CHECK(shortest_arc(0.1f, glm::two_pi<float>() - 0.1f)
          == doctest::Approx(-0.2f).epsilon(1.0e-4));
    CHECK(turn_body_toward(0.0f, 1.0f, 0.0f, RATE) == 0.0f); // dt = 0 — тождество
    CHECK(axes_in_body_frame({0.0f, 0.0f}, 0.0f, 1.0f) == glm::vec2{0.0f});
}
