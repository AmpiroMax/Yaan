/*
Created: 18:08:2026 - 13:08:07
Last updated: 18:08:2026 - 13:08:07
Module: tests/app
File: tests/app/EditorStrokeTests.cpp

Responsibility:
- ЗЕМЛЯ ДВИГАЕТСЯ, ПОКА ВЕДЁШЬ КИСТЬ. Заказ 18.08: «хочу изменение ландшафта от
  инструмента высоты в реальном времени, сейчас высота меняется только когда я
  мышь отпущу, а мне так непонятно что происходит».

Key claims (и почему их не сделает ни один кадр):
- за ОДИН штрих земля показана БОЛЬШЕ ОДНОГО РАЗА. Кадр показывает одно
  состояние и на вопрос «сколько раз» ответить не может в принципе.
- КОНТРОЛЬ к этому: короткий штрих длиной в один кадр показывает её РОВНО
  ОДИН раз. Без него утверждение прошло бы и на коде, который копит всё до
  отпускания (и который здесь стоит третьей рукой и КРАСНЕЕТ).
- ЦЕНА НАЗВАНА: доля времени штриха, ушедшая на перестройку, не выше 1/(1+4).
  Отзывчивость, купленная за кадр, — это то, чего лид просил не делать молча.

Dependencies:
- Uses: doctest, engine/editor (EditorBrush: StrokeRefresh). Ни окна, ни ImGui,
  ни рендера — здесь одни решения (правило 3).
- Used by: рукав app_editor_brush.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЦЕНА ПЕРЕСТРОЙКИ ЗДЕСЬ — ИЗМЕРЕННАЯ, А НЕ ВЫДУМАННАЯ: 0.196 с это среднее по
  12 прогонам на живом кольце 3x3 (сид 123, шаг земли 1 м). Меняется цена —
  меняется и это число, иначе проверка перестанет описывать работу.
*/
/*
UPD:
- 18:08:2026 - 13:08:07: Создан — показ земли во время штриха (заказ 18.08).
*/

#include <doctest/doctest.h>

#include "engine/core/config/sources/Constants.h"
#include "engine/editor/sources/EditorBrush.h"

#include <cstdio>

using dfn::app::StrokeRefresh;

namespace {

/// ИЗМЕРЕНО, а не назначено: перестройка одного чанка под кистью, среднее по 12
/// прогонам (135.4 лучшая, 259.9 худшая).
constexpr float REBUILD_S = 0.196f;
constexpr float FRAME_S = 1.0f / 60.0f;

/// Один штрих длиной `seconds` ПО ЧАСАМ НА СТЕНЕ, при 60 кадрах в секунду, и
/// каждый кадр кисть кусает землю.
///
/// ЦЕНА ПОКАЗА ВХОДИТ В ЧАСЫ, и это не мелочь: перестройка идёт ВНУТРИ кадра,
/// поэтому кадр, в котором показали землю, длится не 16 мс, а 16 + 196. Модель,
/// которая этого не считает, показала бы долю вдвое меньше настоящей — то есть
/// соврала бы ровно про то число, ради которого лид просил замер.
StrokeRefresh run_stroke(float seconds, float rebuild_cost_s) {
    StrokeRefresh r;
    float wall = 0.0f;
    while (wall < seconds) {
        float dt = FRAME_S;
        if (r.step(true, dt)) {
            r.note_cost(rebuild_cost_s);
            dt += rebuild_cost_s;
        }
        wall += dt;
    }
    return r;
}

} // namespace

TEST_CASE("штрих: земля показывается несколько раз, пока кнопка зажата") {
    constexpr float HELD_S = 2.0f;
    const StrokeRefresh r = run_stroke(HELD_S, REBUILD_S);
    const double duty = 100.0 * static_cast<double>(r.spent_s)
                      / static_cast<double>(HELD_S);
    std::printf("[штрих] %.1f с удержания при цене %.3f с: показов %d, на них "
                "ушло %.2f с (%.0f %% штриха)\n",
                static_cast<double>(HELD_S), static_cast<double>(REBUILD_S),
                r.pushes, static_cast<double>(r.spent_s), duty);
    // ГЛАВНОЕ УТВЕРЖДЕНИЕ. Больше одного — это и есть «в реальном времени» на
    // перестройке, которая стоит двенадцать кадров.
    CHECK(r.pushes > 1);
    // И ЦЕНА, НАЗВАННАЯ ЧИСЛОМ. Доля, которую съедают показы, обязана держаться
    // около 1/(1+RATIO) = 20 %; допуск — один показ, потому что первый идёт
    // немедленно и приходится на штрих, который ещё не начал набирать время.
    const float budget = HELD_S / (1.0f + static_cast<float>(dfn::config::REFRESH_COST_RATIO)) + REBUILD_S;
    CHECK(r.spent_s <= budget);
}

TEST_CASE("штрих: короткий штрих в один кадр меняет землю ровно один раз") {
    // КОНТРОЛЬ, БЕЗ КОТОРОГО ПЕРВЫЙ СЛУЧАЙ НИЧЕГО НЕ ЗНАЧИТ. Код, который
    // показывает землю КАЖДЫЙ кадр без разбора, прошёл бы первый случай и
    // провалил бы этот — на нём приложение встало бы намертво.
    StrokeRefresh r;
    CHECK(r.step(true, FRAME_S)); // первый мазок виден сразу, а не через паузу
    r.note_cost(REBUILD_S);
    CHECK_FALSE(r.step(false, FRAME_S)); // отпустили: показывать больше нечего
    CHECK(r.pushes == 1);
}

TEST_CASE("штрих: рука прежнего поведения — ни одного показа до отпускания") {
    // ТРЕТЬЯ РУКА, ОБЯЗАННАЯ КРАСНЕТЬ (правило 30) — И ЭТО РОВНО ТОТ КОД,
    // КОТОРЫЙ БЫЛ ДО СЕГОДНЯ: пауза длиннее штриха, поэтому за всё удержание
    // земля не показывается ни разу, а единственное изменение приходит из
    // finish_stroke в момент отпускания. Итог штриха — ОДНО изменение, и
    // жалоба «непонятно, что происходит» описывает именно его.
    StrokeRefresh old;
    old.wait_s = 1.0e9f; // «показывать только в конце», выраженное числом
    for (int i = 0; i < 120; ++i) {
        CHECK_FALSE(old.step(true, FRAME_S));
    }
    std::printf("[штрих] рука «до отпускания»: показов за 2 с удержания %d\n",
                old.pushes);
    CHECK(old.pushes == 0);
    const StrokeRefresh live = run_stroke(2.0f, REBUILD_S);
    CHECK(live.pushes > old.pushes + 1); // +1 — тот самый показ на отпускании
}

TEST_CASE("штрих: пауза следует за ценой, а не за числом в коде") {
    // ДЕШЁВАЯ перестройка (локальная правка, если её когда-нибудь сделают) —
    // показ чаще; ДОРОГАЯ — реже. Оба конца упираются в свои пределы, и это
    // тоже проверяется: без пола показ на даровой перестройке шёл бы каждый
    // кадр, без потолка дорогая машина вернулась бы к «скачку на отпускании».
    StrokeRefresh cheap;
    cheap.note_cost(0.002f);
    CHECK(cheap.wait_s == doctest::Approx(static_cast<float>(dfn::config::REFRESH_MIN_PERIOD_S)));
    StrokeRefresh dear;
    dear.note_cost(1.5f);
    CHECK(dear.wait_s == doctest::Approx(static_cast<float>(dfn::config::REFRESH_MAX_PERIOD_S)));
    StrokeRefresh measured;
    measured.note_cost(REBUILD_S);
    CHECK(measured.wait_s
          == doctest::Approx(REBUILD_S * static_cast<float>(dfn::config::REFRESH_COST_RATIO))
                 .epsilon(0.001));

    const StrokeRefresh a = run_stroke(1.0f, 0.01f);
    const StrokeRefresh b = run_stroke(1.0f, REBUILD_S);
    std::printf("[штрих] 1.0 с: дешёвая перестройка %d показов, измеренная %d\n",
                a.pushes, b.pushes);
    CHECK(a.pushes > b.pushes);
}

TEST_CASE("штрих: мазок, который ничего не изменил, ничего и не показывает") {
    // Кисть, наведённая за край мира, не двигает ни отсчёта. Показ на этом
    // месте перестроил бы чанк ради нуля — и, что хуже, счётчик «сколько раз
    // земля менялась» перестал бы значить то, что написано на нём.
    StrokeRefresh r;
    for (int i = 0; i < 120; ++i) {
        CHECK_FALSE(r.step(false, FRAME_S));
    }
    CHECK(r.pushes == 0);
    CHECK(r.spent_s == 0.0f);
}
