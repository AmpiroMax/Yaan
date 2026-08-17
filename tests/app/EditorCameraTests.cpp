/*
Created: 17:08:2026 - 23:52:25
Last updated: 18:08:2026 - 00:24:58
Module: tests
File: tests/app/EditorCameraTests.cpp

Responsibility:
- ДОХОДИТ ЛИ МЫШЬ ДО КАМЕРЫ РЕДАКТОРА. Ровно тот вопрос, который три захода
  подряд разбирал человек за игрой («камера при редактировании всё ещё не
  двигается»), потому что прибора на него не было ни одного. Две половины
  ответа проверяются отдельно: КОМУ достаётся мышь (гейт) и ЧТО камера делает
  с полученным смещением (поворот).

Key items:
- editor_camera_takes_mouse: полная таблица из восьми сочетаний. Гейт живёт
  выражением в EditorCamera.h, и App.cpp зовёт ЕГО же — вложенные if внутри
  кадрового цикла тест не достаёт, и потому отказ там был невидим.
- LookInput: IInput, который отдаёт заданное смещение и ничего больше.
- Поворот: ненулевое смещение ОБЯЗАНО менять рыск; нулевое — обязано не менять.
- Знак: мышь вправо увеличивает рыск, мышь вниз уменьшает тангаж. Одна
  договорённость с симом и с орбитой третьего лица (правило 35).
- Тангаж упирается в предел и не переваливает через макушку.

Dependencies:
- Uses: doctest, EditorCamera.cpp, constants.
- Used by: ctest (app_editor_camera).

AI Agents Notice (must follow):
- Правило 30: проверка обязана уметь краснеть. Здесь она краснеет, если
  смещение перестанет доходить, если гейт начнёт пропускать при отданном
  курсоре, и если знак развернётся.
*/
/*
UPD:
- 18:08:2026 - 00:07:07: Создан. Живой прогон (DFN_EDITOR=1
  DFN_OPEN_MAP=houses/demo DFN_CAM_TRACE=1) показал 167 кадров с ненулевым
  смещением, и на каждом рыск менялся — то есть путь исправен. Рукав заведён
  не поэтому, а вопреки: пока проверки не было, ТОТ ЖЕ ответ приходилось
  добывать запуском игры, и три захода подряд его добывал пользователь.
  Контрфакт при заведении: закоротил mouse_delta нулём — 7 утверждений
  покраснели; вернул — 18 из 18 зелёные.
- 18:08:2026 - 00:24:58: LookInput отвечает на новый пункт контракта IInput (place_cursor).
*/

#include <doctest/doctest.h>

#include <cmath>

#include "engine/app/sources/EditorCamera.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/platform/input/interfaces/IInput.h"

namespace {

using dfn::app::EditorCamera;
using dfn::app::editor_camera_takes_mouse;
namespace platform = dfn::platform;
namespace config = dfn::config;

constexpr float EPS = 1e-5f;

/// Ввод, у которого есть только смещение мыши: клавиш нет, колеса нет. Так
/// поворот измеряется отдельно от полёта — иначе W, зажатый по недосмотру,
/// подвинул бы точку и вопрос «повернулась ли камера» стал бы двумя вопросами.
class LookInput final : public platform::IInput {
public:
    glm::vec2 delta{0.0f};
    glm::vec2 wheel{0.0f};

    void update() override {}
    bool is_down(platform::Key) const override { return false; }
    bool was_pressed(platform::Key) const override { return false; }
    bool was_released(platform::Key) const override { return false; }
    bool is_down(platform::MouseButton) const override { return false; }
    bool was_pressed(platform::MouseButton) const override { return false; }
    bool was_released(platform::MouseButton) const override { return false; }
    glm::vec2 mouse_position() const override { return {0.0f, 0.0f}; }
    glm::vec2 mouse_delta() const override { return delta; }
    glm::vec2 scroll_delta() const override { return wheel; }
    void set_cursor_captured(bool) override {}
    bool is_cursor_captured() const override { return true; }
    void place_cursor(const glm::vec2&) override {}
};

} // namespace

TEST_CASE("кому достаётся мышь: полная таблица гейта") {
    // Смотрим камерой ровно в одном случае из восьми.
    CHECK(editor_camera_takes_mouse(true, false, false));

    // Не редактор — не наша мышь ни при каких остальных значениях.
    CHECK_FALSE(editor_camera_takes_mouse(false, false, false));
    CHECK_FALSE(editor_camera_takes_mouse(false, false, true));
    CHECK_FALSE(editor_camera_takes_mouse(false, true, false));
    CHECK_FALSE(editor_camera_takes_mouse(false, true, true));

    // Набирается текст — W это буква, а не полёт.
    CHECK_FALSE(editor_camera_takes_mouse(true, true, false));
    CHECK_FALSE(editor_camera_takes_mouse(true, true, true));

    // Курсор отдан интерфейсу клавишей R — человек указывает, а не смотрит.
    CHECK_FALSE(editor_camera_takes_mouse(true, false, true));
}

TEST_CASE("смещение мыши доходит до камеры") {
    EditorCamera cam;
    cam.reset(glm::vec3{0.0f}, 0.0f, 0.0f);

    LookInput input;

    SUBCASE("нулевое смещение не двигает взгляд") {
        input.delta = {0.0f, 0.0f};
        cam.update(input, 1.0f / 60.0f);
        CHECK(std::fabs(cam.yaw()) < EPS);
        CHECK(std::fabs(cam.pitch()) < EPS);
    }

    SUBCASE("ненулевое смещение ОБЯЗАНО повернуть") {
        // Ровно тот отказ, который ловим: смещение пришло, рыск не изменился.
        input.delta = {10.0f, 0.0f};
        cam.update(input, 1.0f / 60.0f);
        CHECK(std::fabs(cam.yaw()) > EPS);
    }

    SUBCASE("знак: мышь вправо увеличивает рыск") {
        input.delta = {10.0f, 0.0f};
        cam.update(input, 1.0f / 60.0f);
        const float right = cam.yaw();
        CHECK(right > 0.0f);
        CHECK(right == doctest::Approx(10.0f * static_cast<float>(config::MOUSE_SENSITIVITY)));

        cam.reset(glm::vec3{0.0f}, 0.0f, 0.0f);
        input.delta = {-10.0f, 0.0f};
        cam.update(input, 1.0f / 60.0f);
        CHECK(cam.yaw() < 0.0f);
    }

    SUBCASE("знак: мышь вниз опускает взгляд") {
        input.delta = {0.0f, 10.0f};
        cam.update(input, 1.0f / 60.0f);
        CHECK(cam.pitch() < 0.0f);
    }

    SUBCASE("смещения накапливаются кадр за кадром") {
        input.delta = {4.0f, 0.0f};
        cam.update(input, 1.0f / 60.0f);
        const float after_one = cam.yaw();
        cam.update(input, 1.0f / 60.0f);
        CHECK(cam.yaw() == doctest::Approx(2.0f * after_one));
    }
}

TEST_CASE("тангаж упирается в предел и не переваливает") {
    EditorCamera cam;
    cam.reset(glm::vec3{0.0f}, 0.0f, 0.0f);

    LookInput input;
    const float limit = static_cast<float>(config::CAMERA_PITCH_LIMIT);

    input.delta = {0.0f, -1000.0f}; // вверх, заведомо больше предела
    for (int i = 0; i < 8; ++i) {
        cam.update(input, 1.0f / 60.0f);
    }
    CHECK(cam.pitch() == doctest::Approx(limit));

    input.delta = {0.0f, 1000.0f}; // вниз
    for (int i = 0; i < 16; ++i) {
        cam.update(input, 1.0f / 60.0f);
    }
    CHECK(cam.pitch() == doctest::Approx(-limit));
}
