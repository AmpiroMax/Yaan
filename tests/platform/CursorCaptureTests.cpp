/*
Created: 18:08:2026 - 00:25:00
Last updated: 18:08:2026 - 00:25:00
Module: tests
File: tests/platform/CursorCaptureTests.cpp

Responsibility:
- ЗАХВАТ КУРСОРА НЕ СМЕЕТ СЪЕДАТЬ СМЕЩЕНИЕ МЫШИ. Единственный рукав во всём
  дереве, который поднимает НАСТОЯЩЕЕ окно GLFW, и поднимает его по одной
  причине: отказ, который он держит, невидим для любого другого прибора.
  App держит захват УТВЕРЖДЕНИЕМ («пока человек в редакторе — курсор мой»), то
  есть зовёт set_cursor_captured(true) каждым кадром; платформа считала каждый
  такой вызов СОБЫТИЕМ и сбрасывала «предыдущее положение известно». Смещение
  выходило нулевым всегда, камера не поворачивалась вовсе, и пользователь три
  захода подряд писал «в режиме редактора не работает камера».

Key items:
- Рука двигает курсор САМА, через IInput::place_cursor: прав системы на это не
  нужно, и «мышь не двигали» перестаёт быть объяснением нуля. Заголовков третьей
  стороны здесь нет — правила 2 и 23 держат их внутри бэкендов, ворота DAG это
  проверяют, и потому фокус окна и постановка указателя стали операциями
  контракта, а не вызовами GLFW из рукава.
- КОНТРОЛЬНАЯ РУКА обязательна: тот же цикл, но захват запрошен ОДИН раз. Без
  неё зелёный результат неотличим от стенда, который просто ничего не мерит, —
  первая версия этого стенда именно такой и была (окно скрыто и без фокуса,
  все три руки дали ноль).

Dependencies:
- Uses: doctest, dfn_platform_input (glfw), dfn_platform_window.
- Used by: ctest (platform_cursor_capture).

AI Agents Notice (must follow):
- ЕМУ НУЖЕН ДИСПЛЕЙ. Окно не поднялось — рукав говорит об этом вслух и выходит
  зелёным, а не притворяется, будто проверил. Правило 3 про НУЛЕВОЙ бэкенд:
  здесь проверяется сам бэкенд GLFW, и подменять его нечем.
- Окно поднимается ВИДИМЫМ и с фокусом намеренно. Скрытое окно не получает
  положения курсора, и стенд молча перестаёт мерить.
*/
/*
UPD:
- 18:08:2026 - 00:25:00: Создан. Три руки одного стенда: захват один раз — 39
  кадров со смещением из 40; захват каждым кадром БЕЗ правки — 0 из 40; с
  правкой — 39 из 40. Отказ воспроизведён числом до правки и снят после.
*/

#include <doctest/doctest.h>

#include "engine/platform/input/interfaces/IInput.h"
#include "engine/platform/input/sources/glfw/CreateGlfwInput.h"
#include "engine/platform/window/interfaces/IWindow.h"
#include "engine/platform/window/sources/glfw/CreateGlfwWindow.h"

#include <cmath>
#include <memory>

namespace {

namespace platform = dfn::platform;

/// Окно и ввод на время одной проверки. Окно ВИДИМОЕ и с фокусом: скрытое не
/// получает положения курсора, и стенд молча перестаёт мерить.
struct Rig {
    std::unique_ptr<platform::IWindow> window;
    std::unique_ptr<platform::IInput> input;
    bool ready = false;

    Rig() {
        platform::WindowInitParams params;
        params.width = 640;
        params.height = 480;
        params.title = "cursor capture probe";
        window = platform::create_glfw_window();
        if (window == nullptr || !window->init(params)) {
            return;
        }
        input = platform::create_glfw_input(*window);
        if (input == nullptr) {
            return;
        }
        window->poll_events();
        ready = true;
    }

    ~Rig() {
        input.reset();
        if (window != nullptr) {
            window->shutdown();
        }
    }

    /// Сколько кадров из `frames` принесли ненулевое смещение, если каждый кадр
    /// курсор сдвигать на `step` пикселей. `hold` — просить ли захват КАЖДЫМ
    /// кадром (так делает App в редакторе) или только один раз на входе.
    int moved_frames(bool hold, int frames, double step) {
        // Ни одного заголовка третьей стороны здесь нет и быть не может:
        // правила 2 и 23 держат их внутри бэкендов, и ворота DAG это ловят.
        // Поэтому и фокус, и постановка указателя — операции контракта.
        window->focus();
        window->poll_events();

        input->set_cursor_captured(true); // вход в редактор
        double x = 100.0;
        int moved = 0;
        for (int i = 0; i < frames; ++i) {
            if (hold) {
                input->set_cursor_captured(true);
            }
            x += step;
            input->place_cursor(glm::vec2{static_cast<float>(x), 100.0f});
            window->poll_events();
            input->update();
            const glm::vec2 d = input->mouse_delta();
            if (std::fabs(d.x) > 0.0f || std::fabs(d.y) > 0.0f) {
                ++moved;
            }
        }
        return moved;
    }
};

} // namespace

TEST_CASE("захват курсора не съедает смещение мыши") {
    Rig rig;
    if (!rig.ready) {
        MESSAGE("нет дисплея — рукав пропущен, а не пройден");
        return;
    }

    constexpr int FRAMES = 40;
    constexpr double STEP = 10.0;

    // КОНТРОЛЬНАЯ РУКА. Захват запрошен один раз — так ведёт себя всё, кроме
    // редактора. Если ноль придёт ЗДЕСЬ, значит стенд не мерит, и молчание
    // второй руки ничего не доказывает.
    const int once = rig.moved_frames(/*hold=*/false, FRAMES, STEP);
    REQUIRE_MESSAGE(once >= FRAMES - 1,
                    "контрольная рука не мерит: смещения не доходят даже при "
                    "однократном запросе захвата");

    // РАБОЧАЯ РУКА. Захват запрошен КАЖДЫМ кадром — ровно так его держит App,
    // пока человек в редакторе. Смещения обязаны доходить так же.
    const int hold = rig.moved_frames(/*hold=*/true, FRAMES, STEP);
    CHECK_MESSAGE(hold >= FRAMES - 1,
                  "повторный запрос захвата съедает смещение: камера редактора "
                  "не повернётся ни на градус");
}
