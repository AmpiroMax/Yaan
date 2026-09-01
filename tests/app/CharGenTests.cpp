/*
Module: tests
File: tests/app/CharGenTests.cpp

Responsibility:
- ЭКРАН СОЗДАНИЯ ПЕРСОНАЖА: то, что кадром не доказать. Виджет-ползунок
  (точка -> значение и обратно, полосы, шаг), раскладка (все строки в кадре, и
  указатель попадает туда же, куда смотрит глаз), кадрирование (рост ВИДЕН, а
  крупный план держит голову), тело (пара create/destroy на видеокарте) и два
  файла «Готово» — пресет и выпечка.

Key items:
- Ползунок: значение зажато полосой ЦЕЛИ на всех трёх входах.
- Раскладка: ни одна строка не уходит за кадр ни на 320x180, ни на 1920x1080.
- Рост: множитель кадра равен отношению ростов — иначе ползунок работает, а
  увидеть его нельзя (так и было в первой версии).
- Тело: 60 движений ручки -> один живой меш, release -> ноль.
- Выпечка: секции MORF нет, рост тот, что просили, ДВЕ выпечки одного пресета
  побайтово равны.

Dependencies:
- Uses: doctest, engine/app (CharGen, CharGenBody, UiSlider), нулевой
  бэкенд рендера, engine/anim Rig.
- Used by: ctest (app_chargen).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ТЕЛО БЕРЁТСЯ НАСТОЯЩЕЕ (assets/objects/characters/HumanBase.dfo), а не
  выдуманное: предмет проверки — «экран показывает то тело, которым игрок
  пойдёт в мир», и на фикстуре из двух треугольников это утверждение
  бессодержательно (правило 30). Выпечка пишется во ВРЕМЕННЫЙ каталог: набор,
  который пишет в assets/, меняет то, что прочтёт следующий прогон игры.
*/

#include "engine/app/sources/CharGen.h"
#include "engine/app/sources/CharGenBody.h"
#include "engine/app/sources/UiSlider.h"

#include "engine/anim/sources/Rig.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/FirstPersonCamera.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using namespace dfn;

namespace {

namespace fs = std::filesystem;

/// ТА ЖЕ СТРОКА, ЧТО У ЭКРАНА И У МИРА, а не четвёртая её копия: набор,
/// назвавший путь своими буквами, проверяет тело, которое в игру может и не
/// попасть (правило 32).
const char* BODY_PATH = app::CHARGEN_SOURCE_BODY;

[[nodiscard]] bool body_present() {
    std::error_code ec;
    return fs::exists(BODY_PATH, ec);
}

/// Экран, собранный ТЕМ ЖЕ описанием, что и в игре: строки телосложения
/// приходят снаружи, категории и глаголы — из chargen_describe().
[[nodiscard]] app::CharGenScreen screen_with(std::size_t morph_count) {
    app::CharGenScreen s;
    std::vector<app::CharGenRow> rows;
    for (std::size_t i = 0; i < morph_count; ++i) {
        app::CharGenRow r;
        r.kind = app::CharGenRowKind::Slider;
        r.name = "belly" + std::to_string(i);
        r.lo = -1.0f;
        r.hi = 1.0f;
        rows.push_back(std::move(r));
    }
    app::CharGenRow h;
    h.kind = app::CharGenRowKind::Slider;
    h.name = app::CHARGEN_HEIGHT_KEY;
    h.lo = app::CHARGEN_HEIGHT_MIN_M;
    h.hi = app::CHARGEN_HEIGHT_MAX_M;
    h.value = app::CHARGEN_BODY_HEIGHT_M;
    h.metres = true;
    rows.push_back(std::move(h));
    s.set_categories(app::chargen_describe(std::move(rows)));
    return s;
}

[[nodiscard]] std::string read_bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

/// Габарит потока SKIN файла — им меряется рост выпеченного тела.
[[nodiscard]] float dfo_height(const fs::path& p) {
    const auto obj = render::read_object(p);
    if (!obj || obj->skin.vertices.empty()) {
        return -1.0f;
    }
    float lo = 1e9f;
    float hi = -1e9f;
    for (const platform::SkinnedVertex& v : obj->skin.vertices) {
        lo = std::min(lo, v.position.y);
        hi = std::max(hi, v.position.y);
    }
    return hi - lo;
}

} // namespace

// --- ВИДЖЕТ -----------------------------------------------------------------

TEST_CASE("ползунок: точка и значение — обратные друг другу") {
    const app::SliderTrack track{100, 50, 200, 5, 9};
    // Три опорные точки: оба конца и середина. Края важнее середины — именно
    // там жила бы ошибка «жёлоб считается по нарисованной линии».
    CHECK(app::slider_value_at(track, 100, -1.0f, 1.0f) == doctest::Approx(-1.0f));
    CHECK(app::slider_value_at(track, 300, -1.0f, 1.0f) == doctest::Approx(1.0f));
    CHECK(app::slider_value_at(track, 200, -1.0f, 1.0f) == doctest::Approx(0.0f));
    CHECK(app::slider_handle_x(track, -1.0f, -1.0f, 1.0f) == 100);
    CHECK(app::slider_handle_x(track, 1.0f, -1.0f, 1.0f) == 300);
    CHECK(app::slider_handle_x(track, 0.0f, -1.0f, 1.0f) == 200);
}

TEST_CASE("ползунок: точка ЗА полосой зажимается, а не вылетает") {
    const app::SliderTrack track{100, 50, 200, 5, 9};
    CHECK(app::slider_value_at(track, -500, 0.0f, 0.55f) == doctest::Approx(0.0f));
    CHECK(app::slider_value_at(track, 5000, 0.0f, 0.55f) == doctest::Approx(0.55f));
}

TEST_CASE("ползунок: вырожденная полоса не делит на ноль") {
    const app::SliderTrack track{0, 0, 100, 3, 7};
    CHECK(app::slider_value_at(track, 50, 0.4f, 0.4f) == doctest::Approx(0.4f));
    CHECK(app::slider_handle_x(track, 0.4f, 0.4f, 0.4f) == 0);
    CHECK(app::slider_key_step(0.4f, 0.4f, false) == doctest::Approx(0.0f));
}

TEST_CASE("ползунок: мелкий шаг ровно вдесятеро меньше крупного") {
    const float coarse = app::slider_key_step(0.0f, 0.55f, false);
    const float fine = app::slider_key_step(0.0f, 0.55f, true);
    CHECK(coarse == doctest::Approx(0.55f / app::SLIDER_COARSE_STEPS));
    CHECK(fine == doctest::Approx(coarse / 10.0f));
}

// --- РАСКЛАДКА --------------------------------------------------------------

TEST_CASE("раскладка: ни одна строка не уходит за кадр") {
    // ОБА КОНЦА ЛЕСТНИЦЫ СЕТОК. Ретро-сетка — та, где строки не помещаются
    // первой, и ровно там страница настроек однажды уронила последнюю строку
    // на четыре пикселя ниже кадра: невидимую И НЕНАЖИМАЕМУЮ.
    for (const auto [w, h] : {std::pair{320, 180}, std::pair{1280, 720},
                              std::pair{1920, 1080}}) {
        const app::CharGenScreen s = screen_with(11);
        const app::CharGenLayout L = app::chargen_layout(w, h, s.row_count());
        CHECK(L.row_y(0) > L.title_y);
        CHECK(L.row_y(s.row_count() - 1) < h);
        CHECK(L.step >= 1);
        // Колонка кончается ЛЕВЕЕ фигуры: иначе текст ложился бы на тело.
        CHECK(L.panel_right < static_cast<int>(
                                  static_cast<float>(w) * app::CHARGEN_FIGURE_X_FRAC));
        CHECK(L.track_x > L.label_x);
        CHECK(L.track_x + L.track_w <= L.value_right);
    }
}

TEST_CASE("вкладка под указателем — та же, что нарисована") {
    app::CharGenScreen s = screen_with(11);
    const int w = 1920;
    const int h = 1080;
    const app::CharGenLayout L = app::chargen_layout(w, h, s.row_count());
    const int span = L.panel_right - L.label_x;
    const int each = span / 2;
    CHECK(s.tab_at(w, h, L.label_x + each / 2, L.tabs_y + 2) == 0);
    CHECK(s.tab_at(w, h, L.label_x + each + each / 2, L.tabs_y + 2) == 1);
    // Ниже полосы вкладок — «ни на какой»: иначе щелчок по первой строке
    // молча переключал бы категорию.
    CHECK(s.tab_at(w, h, L.label_x + each / 2, L.row_y(0)) == s.categories().size());
    // И щелчок по вкладке ручку не берёт, а вкладку меняет.
    CHECK(s.press(w, h, L.label_x + each + each / 2, L.tabs_y + 2) == s.row_count());
    CHECK(s.category() == 1);
}

TEST_CASE("раскладка: указатель попадает в ту же строку, на которую смотрит глаз") {
    app::CharGenScreen s = screen_with(11);
    const int w = 1920;
    const int h = 1080;
    const app::CharGenLayout L = app::chargen_layout(w, h, s.row_count());
    for (std::size_t i = 0; i < s.row_count(); ++i) {
        CHECK(s.row_at(w, h, L.track_x + 4, L.row_y(i)) == i);
    }
    // Правее панели — «ни на чём»: наведение на фигуру не двигает выбор.
    CHECK(s.row_at(w, h, w - 10, L.row_y(0)) == s.row_count());
    CHECK(s.over_figure(w, h, w - 10));
    CHECK_FALSE(s.over_figure(w, h, L.track_x));
}

// --- МОДЕЛЬ ЭКРАНА ----------------------------------------------------------

TEST_CASE("описание: две категории, телосложение первое, глаголы под каждой") {
    app::CharGenScreen s = screen_with(11);
    REQUIRE(s.categories().size() == 2);
    CHECK(s.categories()[0].key == "chargen.tab.body");
    CHECK(s.categories()[1].key == "chargen.tab.identity");
    CHECK(s.category() == 0);
    // Вкладка телосложения: 11 целей + рост + три глагола.
    REQUIRE(s.row_count() == 15);
    CHECK(s.row_kind(0) == app::CharGenRowKind::Slider);
    CHECK(s.row_kind(11) == app::CharGenRowKind::Slider); // рост
    CHECK(s.row_kind(12) == app::CharGenRowKind::Button); // сброс
    CHECK(s.row_kind(14) == app::CharGenRowKind::Button); // назад
    REQUIRE(s.find(app::CHARGEN_HEIGHT_KEY) != nullptr);
    CHECK(s.find(app::CHARGEN_HEIGHT_KEY)->metres);
    // Вкладка имени: одно поле ввода и те же три глагола.
    s.set_category(1);
    REQUIRE(s.row_count() == 4);
    CHECK(s.row_kind(0) == app::CharGenRowKind::Text);
    CHECK(s.row_kind(1) == app::CharGenRowKind::Button);
    // ГЛАГОЛЫ ЕСТЬ НА ОБЕИХ ВКЛАДКАХ. «Готово», спрятанное внутрь одной,
    // означало бы, что кнопка выхода зависит от того, где стоял игрок.
    CHECK(s.row_at_index(3)->action == app::CharGenAction::Back);
}

TEST_CASE("вкладки: переключение по кругу, и выбор возвращается в начало") {
    app::CharGenScreen s = screen_with(11);
    s.set_selection(9);
    s.cycle_category(+1);
    CHECK(s.category() == 1);
    CHECK(s.selection() == 0); // номер строки на другой вкладке — другая строка
    s.cycle_category(+1);
    CHECK(s.category() == 0);
    s.cycle_category(-1);
    CHECK(s.category() == 1);
    // Значение, набранное на одной вкладке, ВИДНО с другой: пресет собирается
    // со всего экрана, а не с открытой страницы.
    s.set_category(0);
    CHECK(s.set_value("belly3", 0.5f));
    s.set_category(1);
    REQUIRE(s.find("belly3") != nullptr);
    CHECK(s.find("belly3")->value == doctest::Approx(0.5f));
}

TEST_CASE("переключатель вариантов листает по кругу и НЕ трогает соседей") {
    // ВИД СТРОКИ, ЗАВЕДЁННЫЙ ПОД ДИЗАЙН-СЕССИЮ (причёски, цвета,
    // происхождение). Проверяется здесь, а не «когда понадобится»: каркас,
    // объявленный готовым и ни разу не прогнанный, — это обещание, а не код.
    app::CharGenScreen s;
    std::vector<app::CharGenCategory> cats(1);
    cats[0].key = "chargen.tab.body";
    app::CharGenRow opt;
    opt.kind = app::CharGenRowKind::Option;
    opt.name = "hair";
    opt.label_key = "chargen.name";
    opt.choices = {"a", "b", "c"};
    cats[0].rows.push_back(opt);
    app::CharGenRow slider;
    slider.kind = app::CharGenRowKind::Slider;
    slider.name = "belly";
    slider.lo = -1.0f;
    slider.hi = 1.0f;
    cats[0].rows.push_back(slider);
    s.set_categories(std::move(cats));

    s.set_selection(0);
    CHECK(s.adjust(+1, false) == 0);
    CHECK(s.find("hair")->choice == 1);
    CHECK(s.adjust(+1, false) == 0);
    CHECK(s.adjust(+1, false) == 0);
    CHECK(s.find("hair")->choice == 0); // по кругу
    CHECK(s.adjust(-1, false) == 0);
    CHECK(s.find("hair")->choice == 2);
    CHECK(s.find("belly")->value == doctest::Approx(0.0f));
    // Сброс возвращает переключатель к первому варианту, а имя не трогает.
    s.reset_rows();
    CHECK(s.find("hair")->choice == 0);
}

TEST_CASE("выбор ходит по кругу") {
    app::CharGenScreen s = screen_with(11);
    s.set_selection(0);
    s.move(-1);
    CHECK(s.selection() == s.row_count() - 1);
    s.move(1);
    CHECK(s.selection() == 0);
}

TEST_CASE("стрелка не выпускает вес за полосу цели") {
    app::CharGenScreen s = screen_with(1);
    s.set_selection(0);
    for (int i = 0; i < 1000; ++i) {
        (void)s.adjust(+1, false);
    }
    CHECK(s.find("belly0")->value == doctest::Approx(1.0f));
    for (int i = 0; i < 1000; ++i) {
        (void)s.adjust(-1, false);
    }
    CHECK(s.find("belly0")->value == doctest::Approx(-1.0f));
    // На строке-глаголе стрелка не делает НИЧЕГО и говорит об этом номером.
    s.set_selection(s.row_count() - 1);
    CHECK(s.adjust(+1, false) == s.row_count());
}

TEST_CASE("сброс: ползунки в ноль, рост в канон, имя НЕ трогается") {
    app::CharGenScreen s = screen_with(2);
    CHECK(s.set_value("belly0", 0.8f));
    CHECK(s.set_value(app::CHARGEN_HEIGHT_KEY, app::CHARGEN_HEIGHT_MAX_M));
    s.set_name("Гуннар");
    s.reset_rows();
    CHECK(s.find("belly0")->value == doctest::Approx(0.0f));
    CHECK(s.find(app::CHARGEN_HEIGHT_KEY)->value
          == doctest::Approx(app::CHARGEN_BODY_HEIGHT_M));
    CHECK(s.name() == "Гуннар");
}

TEST_CASE("имя: кириллица считается ЗНАКАМИ, а стирается по знаку") {
    app::CharGenScreen s = screen_with(1);
    s.set_category(1);  // вкладка имени
    s.set_selection(0);
    REQUIRE(s.text_focused());
    // «Ярл» — три знака, шесть байт. Предел, посчитанный в байтах, обрезал бы
    // русское имя вдвое раньше латинского.
    s.feed_text({0x042F, 0x0440, 0x043B});
    CHECK(s.name() == "Ярл");
    s.backspace();
    CHECK(s.name() == "Яр");
    // Управляющие знаки не имя.
    s.feed_text({0x000A, 0x0009});
    CHECK(s.name() == "Яр");
    // Предел длины держится.
    for (int i = 0; i < 100; ++i) {
        s.feed_text({0x0430});
    }
    std::size_t chars = 0;
    for (const char c : s.name()) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++chars;
        }
    }
    CHECK(chars == app::CHARGEN_NAME_MAX_CHARS);
    // Буквы идут в имя ТОЛЬКО пока курсор на его строке.
    s.set_selection(s.row_count() - 1); // «Назад» — не поле ввода
    const std::string before = s.name();
    s.feed_text({0x0431});
    CHECK(s.name() == before);
}

TEST_CASE("мышь: щелчок по полосе ставит значение и берёт ручку") {
    app::CharGenScreen s = screen_with(3);
    const int w = 1920;
    const int h = 1080;
    const app::CharGenLayout L = app::chargen_layout(w, h, s.row_count());
    const app::SliderTrack track = L.track_of(1);
    const std::size_t grabbed = s.press(w, h, track.x + track.w, track.y);
    CHECK(grabbed == 1);
    CHECK(s.dragging());
    CHECK(s.find("belly1")->value == doctest::Approx(1.0f));
    CHECK(s.drag(w, h, track.x));
    CHECK(s.find("belly1")->value == doctest::Approx(-1.0f));
    s.release();
    CHECK_FALSE(s.dragging());
    // Нажатие по глаголу ручку НЕ берёт.
    CHECK(s.press(w, h, L.label_x + 2, L.row_y(s.row_count() - 1))
          == s.row_count());
}

// --- КАМЕРА -----------------------------------------------------------------

TEST_CASE("облёт и приближение зажаты границами") {
    app::CharGenView v;
    app::chargen_orbit(v, 1e6f, 1e6f, 1.0f);
    CHECK(v.yaw == doctest::Approx(app::CHARGEN_YAW_LIMIT));
    CHECK(v.pitch == doctest::Approx(app::CHARGEN_PITCH_LIMIT));
    app::chargen_orbit(v, -1e6f, -1e6f, 1.0f);
    CHECK(v.yaw == doctest::Approx(-app::CHARGEN_YAW_LIMIT));
    CHECK(v.pitch == doctest::Approx(-app::CHARGEN_PITCH_LIMIT));
    app::chargen_zoom(v, 1e4f);
    CHECK(v.zoom == doctest::Approx(1.0f));
    app::chargen_zoom(v, -1e4f);
    CHECK(v.zoom == doctest::Approx(0.0f));
}

TEST_CASE("рост ВИДЕН: множитель кадра равен отношению ростов") {
    // ПРОВАЛ, КОТОРЫЙ ЭТО ЛОВИТ, БЫЛ НАСТОЯЩИМ. Первая версия делила на
    // габарит, УЖЕ умноженный на масштаб роста; множитель сокращался, и
    // фигура ростом 1.84 занимала в кадре ровно столько же, сколько фигура
    // ростом 1.66. Ползунок работал, а увидеть его было нельзя.
    render::FirstPersonCamera camera;
    camera.set_projection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    const glm::vec3 lo{-0.3f, 0.0f, -0.2f};
    const glm::vec3 hi{0.3f, 1.75f, 0.2f};
    const app::CharGenView view;
    const glm::mat4 low =
        app::chargen_in_camera(camera, lo, hi,
                               app::chargen_height_scale(app::CHARGEN_HEIGHT_MIN_M),
                               view);
    const glm::mat4 high =
        app::chargen_in_camera(camera, lo, hi,
                               app::chargen_height_scale(app::CHARGEN_HEIGHT_MAX_M),
                               view);
    const float k_low = glm::length(glm::vec3(low[1]));
    const float k_high = glm::length(glm::vec3(high[1]));
    CHECK(k_high / k_low
          == doctest::Approx(app::CHARGEN_HEIGHT_MAX_M / app::CHARGEN_HEIGHT_MIN_M)
                 .epsilon(0.001));
}

TEST_CASE("крупный план держит ГОЛОВУ, а общий — середину фигуры") {
    const glm::vec3 lo{-0.3f, 0.0f, -0.2f};
    const glm::vec3 hi{0.3f, 1.75f, 0.2f};
    const glm::vec3 body = app::chargen_pivot(lo, hi, 0.0f);
    const glm::vec3 face = app::chargen_pivot(lo, hi, 1.0f);
    CHECK(body.y == doctest::Approx(0.875f));
    CHECK(face.y > 1.55f);   // выше плеч (0.812 роста) — это уже шея и голова
    CHECK(face.y < hi.y);    // и не выше макушки
}

// --- ТЕЛО НА ВИДЕОКАРТЕ -----------------------------------------------------

TEST_CASE("тело: движение ручки — это ПАРА создания и уничтожения меша") {
    // УТЕЧКА НЕ ВИДНА НИ НА ОДНОМ СНИМКЕ ЭКРАНА, КОГДА-ЛИБО СНЯТОМ С ЭКРАНА
    // СОЗДАНИЯ. Единственный прибор, который её видит, — счётчик живых
    // буферов нулевого бэкенда.
    if (!body_present()) {
        MESSAGE("HumanBase.dfo нет в дереве — набор пропущен");
        return;
    }
    platform::NullRenderer renderer;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(renderer, rig, BODY_PATH));
    CHECK(body.ready());
    // КОНТРОЛЬ: пока тело показано, живой меш ОБЯЗАН быть. Без этой строки
    // утверждение держалось бы и для экрана, который не залил ничего.
    CHECK(renderer.live_meshes() == 1);
    REQUIRE_FALSE(body.morphs().empty());

    for (int i = 0; i < 60; ++i) {
        const std::size_t slot = static_cast<std::size_t>(i) % body.morphs().size();
        const render::MorphTarget& t = body.morphs()[slot];
        (void)body.set_weight(slot, (i % 2 == 0) ? t.hi : t.lo);
        REQUIRE(body.apply(renderer));
        REQUIRE(renderer.live_meshes() == 1);
    }
    CHECK(body.uploads() == 61);
    CHECK(body.drops() == 60);
    body.release(renderer);
    CHECK(renderer.live_meshes() == 0);
    CHECK(body.uploads() == body.drops());
}

TEST_CASE("тело экрана — ТОТ ЖЕ ФАЙЛ, что грузит мир, и он СКИНИРОВАННЫЙ") {
    // ЧТО ЗДЕСЬ ПРОВЕРЯЕТСЯ И ПОЧЕМУ ЭТО НЕ ОЧЕВИДНО. Владелец 01.09 увидел на
    // экране создания фигуру, которую назвал «сегментной болванкой», и спросил,
    // не рисуется ли там запасное тело из пятнадцати коробок вместо
    // скинированной модели. Ответ «нет, тот же файл» стоит ровно столько,
    // сколько стоит прибор, которым он получен: экран и мир называют ОДНУ
    // строку (CHARGEN_SOURCE_BODY), и то, что по ней лежит, — это поток SKIN с
    // тысячами треугольников и секцией MORF, а не коробки.
    //
    // ГРАНИЦА ВЗЯТА НЕ С ПОТОЛКА: тело из коробок (anim::build_body_segment_mesh,
    // рука двери DFN_BODY_BOXES) — это пятнадцать параллелепипедов, то есть
    // 15 x 12 = 180 треугольников. Тысяча отделяет одно от другого с запасом в
    // пять раз и не привязывает набор к точному числу вершин модели, которое
    // меняет каждая правка импортёра.
    if (!body_present()) {
        MESSAGE("HumanBase.dfo нет в дереве — набор пропущен");
        return;
    }
    const auto obj = render::read_object(fs::path(app::CHARGEN_SOURCE_BODY));
    REQUIRE(obj);
    CHECK_FALSE(obj->skin.empty());
    CHECK(obj->skin.indices.size() / 3 > 1000);
    CHECK_FALSE(obj->morphs.empty());
    CHECK_FALSE(obj->skeleton.empty());

    // ХЭШ: «один файл» — утверждение о байтах, и оно проверяется байтами.
    // Ноль значил бы «файла нет», и тогда равенство двух нулей ничего бы не
    // доказывало — поэтому ноль отвергается отдельной строкой.
    const std::uint64_t hash =
        app::chargen_body_hash(fs::path(app::CHARGEN_SOURCE_BODY));
    CHECK(hash != 0);
    CHECK(hash == app::chargen_body_hash(fs::path(BODY_PATH)));

    // И ТО, ЧТО ЭКРАН ДЕЙСТВИТЕЛЬНО ЗАЛИЛ ИМЕННО ЭТУ ГЕОМЕТРИЮ, а не «тоже
    // что-то»: число треугольников экрана равно числу треугольников файла.
    platform::NullRenderer renderer;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(renderer, rig, app::CHARGEN_SOURCE_BODY));
    CHECK(body.triangles() == obj->skin.indices.size() / 3);
    CHECK(body.morphs().size() == obj->morphs.size());
    body.release(renderer);
}

TEST_CASE("MORF-бленд ЖИВОЙ: вес двигает вершины, а не только число на экране") {
    // ПОЛЗУНОК, КОТОРЫЙ КРУТИТСЯ И НЕ ЛЕПИТ, — это худший из отказов экрана
    // создания: он выглядит рабочим. Прибор — ГАБАРИТ рест-позы: он считается
    // после бленда и скиннинга, то есть меряет то самое, что уходит на
    // видеокарту.
    if (!body_present()) {
        return;
    }
    platform::NullRenderer renderer;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(renderer, rig, app::CHARGEN_SOURCE_BODY));
    const glm::vec3 lo0 = body.lo();
    const glm::vec3 hi0 = body.hi();
    int moved = 0;
    for (std::size_t i = 0; i < body.morphs().size(); ++i) {
        const render::MorphTarget& t = body.morphs()[i];
        app::CharGenBody one;
        REQUIRE(one.load(renderer, rig, app::CHARGEN_SOURCE_BODY));
        // КРАЙ ПОЛОСЫ, А НЕ СЕРЕДИНА: у половины целей нейтраль стоит НА краю
        // (belly [0, 0.45], age [0, 0.55]), и «сдвинуть в середину» для них
        // значит сдвинуть меньше, чем позволяет цель.
        const float far_end = (std::fabs(t.hi) > std::fabs(t.lo)) ? t.hi : t.lo;
        REQUIRE(one.set_weight(i, far_end));
        REQUIRE(one.apply(renderer));
        if (glm::length(one.lo() - lo0) + glm::length(one.hi() - hi0) > 1e-4f) {
            ++moved;
        }
        one.release(renderer);
    }
    // НЕ «ХОТЬ ОДНА»: цель, которая не двигает габарит, ещё может двигать
    // вершины внутри силуэта (мускулатура), поэтому порог — большинство, а не
    // все. Ноль сдвинувших значил бы, что бленда нет вовсе.
    CHECK(moved >= static_cast<int>(body.morphs().size()) / 2);
    body.release(renderer);
}

TEST_CASE("тело: вес зажимается ПОЛОСОЙ ЦЕЛИ из файла") {
    if (!body_present()) {
        return;
    }
    platform::NullRenderer renderer;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(renderer, rig, BODY_PATH));
    const render::MorphTarget& t = body.morphs()[0];
    (void)body.set_weight(0, 1e6f);
    CHECK(body.weights().weights[0] == doctest::Approx(t.hi));
    (void)body.set_weight(0, -1e6f);
    CHECK(body.weights().weights[0] == doctest::Approx(t.lo));
    body.release(renderer);
}

// --- ПУТЬ ИГРОКА ЧИСТ -------------------------------------------------------

TEST_CASE("диагностика редактора не печатается на пути игрока") {
    // ДЕФЕКТ, КОТОРЫЙ ЭТО ДЕРЖИТ. Интерфейс редактора поднимается в App::init —
    // раньше, чем решено, игра этот запуск или редактор, — и печатал двенадцать
    // строк («шрифт: …», «атлас шрифта …», восемь «знак U+…»). Владелец 01.09
    // открыл главное меню, нажал «Создание персонажа» и получил их в терминал
    // рядом с игрой. Родня того же дефекта, что панели редактора, остававшиеся
    // на экране при выходе в меню: и там, и здесь редакторское живёт выше
    // решения о режиме, и лечится это ОДНОЙ точкой, а не «убрать эту строку».
    //
    // ПОЧЕМУ НАБОР ЧИТАЕТ ИСХОДНИК, А НЕ ЗАПУСКАЕТ ИГРУ. Печать происходит
    // внутри инициализации ImGui на живом бэкенде bgfx: под нулевым рендером
    // она не выполняется вовсе, то есть «молчит» здесь было бы правдой по
    // причине, не имеющей отношения к починке (правило 30b). Прибор, который
    // видит настоящий предмет, — это то, что КАЖДАЯ такая печать стоит за
    // выключателем; его и меряем.
    struct Guarded {
        const char* file;
        const char* literal;
        const char* guard;
    };
    static const Guarded WATCHED[] = {
        {"engine/editor/sources/EditorUi.cpp", "[editor-ui] знак U+", "diagnostics()"},
        {"engine/editor/sources/EditorUi.cpp", "[editor-ui] шрифт: ", "diagnostics()"},
        {"engine/editor/sources/EditorUi.cpp", "[editor-ui] знаки домешаны",
         "diagnostics()"},
        {"engine/platform/render/sources/bgfx/ImGuiBackend.cpp",
         "[imgui] атлас шрифта", "g_diagnostics"},
    };
    for (const Guarded& g : WATCHED) {
        const std::string text = read_bytes(g.file);
        if (text.empty()) {
            MESSAGE("не прочитан: " << g.file);
            continue;
        }
        const std::size_t at = text.find(g.literal);
        REQUIRE_MESSAGE(at != std::string::npos,
                        "строка исчезла из " << g.file << ": " << g.literal);
        // ВЫКЛЮЧАТЕЛЬ ОБЯЗАН СТОЯТЬ ВЫШЕ ПЕЧАТИ И БЛИЗКО. Восемьсот знаков —
        // это около двадцати строк кода: дальше него условие принадлежит уже
        // не этой печати, и «нашлось где-то в файле» перестало бы что-либо
        // значить.
        const std::size_t from = at > 800 ? at - 800 : 0;
        const std::string before = text.substr(from, at - from);
        CHECK_MESSAGE(before.find(g.guard) != std::string::npos,
                      "печать без выключателя в " << g.file << ": " << g.literal);
    }
}

// --- ПРЕСЕТ И ВЫПЕЧКА -------------------------------------------------------

TEST_CASE("пресет: имя, рост и ползунки переживают запись и чтение") {
    const fs::path out = fs::temp_directory_path() / "dfn_chargen_preset.json";
    app::CharGenPreset p;
    p.name = "Гуннар \"Рыжий\"";
    p.height_m = 1.712f;
    p.sliders = {{"belly", 0.31f}, {"hips", -0.75f}, {"weight", 0.0f}};
    REQUIRE(app::write_chargen_preset(out, p));
    app::CharGenPreset back;
    REQUIRE(app::read_chargen_preset(out, back));
    CHECK(back.name == p.name);
    CHECK(back.height_m == doctest::Approx(p.height_m));
    REQUIRE(back.sliders.size() == p.sliders.size());
    for (std::size_t i = 0; i < p.sliders.size(); ++i) {
        CHECK(back.sliders[i].first == p.sliders[i].first);
        CHECK(back.sliders[i].second == doctest::Approx(p.sliders[i].second));
    }
    // ПОРЯДОК ПОЛЕЙ — ЧАСТЬ ФОРМАТА: читатель шага 1 (tools/morph_tool.cpp)
    // ищет "sliders" и разбирает пары до конца файла, поэтому рост и имя
    // обязаны стоять РАНЬШЕ. Проверяется текстом, а не обещанием.
    const std::string text = read_bytes(out);
    CHECK(text.find("\"height_m\"") < text.find("\"sliders\""));
    CHECK(text.find("\"name\"") < text.find("\"sliders\""));
    std::error_code ec;
    fs::remove(out, ec);
}

TEST_CASE("выпечка: MORF снята, рост тот, что просили, два прогона побайтово равны") {
    if (!body_present()) {
        return;
    }
    platform::NullRenderer renderer;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(renderer, rig, BODY_PATH));
    // ГАБАРИТ ПОТОКА SKIN — ЭТО НЕ РОСТ ФИГУРЫ, и это стоит сказать числом,
    // потому что выглядит одинаково. Вершины .dfo лежат в пространстве
    // ПРИВЯЗКИ (T-поза с разведёнными руками): их размах по вертикали
    // 1.805 м. Рост, который меряет судья пропорций, — это РЕСТ-поза,
    // 1.750 м. Отношение двух ВЫПЕЧЕК проверяет масштаб, не путая их.
    const float bind_h = dfo_height(BODY_PATH);
    CHECK(bind_h > 1.7f);
    CHECK(bind_h < 1.9f);

    const std::size_t slot = 0;
    (void)body.set_weight(slot, body.morphs()[slot].hi);

    const fs::path canon = fs::temp_directory_path() / "dfn_chargen_canon.dfo";
    const fs::path a = fs::temp_directory_path() / "dfn_chargen_a.dfo";
    const fs::path b = fs::temp_directory_path() / "dfn_chargen_b.dfo";
    CHECK_FALSE(body.set_height_m(app::CHARGEN_BODY_HEIGHT_M)); // уже канон
    REQUIRE(body.bake(canon));
    CHECK(body.set_height_m(app::CHARGEN_HEIGHT_MAX_M));
    REQUIRE(body.bake(a));
    REQUIRE(body.bake(b));

    // МАСШТАБ РАВНОМЕРЕН: отношение габаритов двух выпечек ОДНИХ И ТЕХ ЖЕ
    // ползунков равно отношению ростов, и ничему другому.
    CHECK(dfo_height(a) / dfo_height(canon)
          == doctest::Approx(app::CHARGEN_HEIGHT_MAX_M / app::CHARGEN_BODY_HEIGHT_M)
                 .epsilon(0.001));

    const auto baked = render::read_object(a);
    REQUIRE(baked);
    // СЕКЦИИ MORF НЕТ — схема Creation Kit: мир грузит обычного персонажа.
    CHECK(baked->morphs.empty());
    CHECK_FALSE(baked->skin.empty());
    CHECK(baked->skeleton.size() > 0);
    // ДВЕ ВЫПЕЧКИ ОДНОГО СОСТОЯНИЯ — ОДИН ФАЙЛ. Пресет это только числа, а
    // бленд детерминирован; расхождение здесь означало бы, что «персонаж
    // воспроизводим» держится на погоде.
    CHECK(read_bytes(a) == read_bytes(b));

    // И РОСТ ЗАЖАТ КАНОНОМ на обоих концах.
    CHECK_FALSE(body.set_height_m(app::CHARGEN_HEIGHT_MAX_M + 5.0f));
    CHECK(body.height_m() == doctest::Approx(app::CHARGEN_HEIGHT_MAX_M));
    CHECK(app::chargen_height_scale(0.5f)
          == doctest::Approx(app::CHARGEN_HEIGHT_MIN_M / app::CHARGEN_BODY_HEIGHT_M));

    body.release(renderer);
    std::error_code ec;
    fs::remove(a, ec);
    fs::remove(b, ec);
    fs::remove(canon, ec);
}

TEST_CASE("пресет старше тела: неизвестный ползунок ПРОПУСКАЕТСЯ, а не рушит экран") {
    if (!body_present()) {
        return;
    }
    platform::NullRenderer renderer;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(renderer, rig, BODY_PATH));
    app::CharGenPreset p;
    p.height_m = 1.70f;
    p.sliders = {{"нет-такой-цели", 0.5f}, {body.morphs()[0].name, body.morphs()[0].hi}};
    body.apply_preset(p);
    CHECK(body.height_m() == doctest::Approx(1.70f));
    CHECK(body.weights().weights[0] == doctest::Approx(body.morphs()[0].hi));
    body.release(renderer);
}
