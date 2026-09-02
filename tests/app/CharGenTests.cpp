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
#include "engine/app/sources/CharacterParts.h"
#include "engine/render/sources/MorphFollow.h"
#include "engine/app/sources/UiSlider.h"

#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/FirstPersonCamera.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/RenderSystem.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <span>
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

/// ЕСТЬ ЛИ У ТЕЛА СЕКЦИЯ MORF. Отдельный вопрос от «есть ли файл», и он
/// законный: тело БЕЗ секции — это выпеченный персонаж (или тело на середине
/// перепечки целей), показать его можно, а крутить нечего. Наборы, которые
/// без ползунков бессодержательны, пропускаются ВСЛУХ; без этой проверки они
/// падали в SIGSEGV на `morphs()[0]` — то есть отвечали «сломано» там, где
/// правильный ответ «нечего мерить».
[[nodiscard]] bool body_has_morphs() {
    if (!body_present()) {
        return false;
    }
    const auto obj = render::read_object(fs::path(BODY_PATH));
    return obj && !obj->morphs.empty();
}

/// Экран, собранный ТЕМ ЖЕ описанием, что и в игре: строки телосложения
/// приходят снаружи, категории и глаголы — из chargen_describe().
/// ИМЕНА ЦЕЛЕЙ — НАСТОЯЩИЕ, В ТОМ ЖЕ ПОРЯДКЕ, В КОТОРОМ ИХ РАСКЛАДЫВАЕТ
/// ОПИСАНИЕ ПО РАЗДЕЛАМ. Выдуманные («belly0», «belly1») набор проходил, а
/// проверял при этом ХВОСТ описания — ветку «цель, о разделе которой никто не
/// сказал», — то есть ровно не то, что видит игрок.
constexpr const char* KNOBS[] = {"weight",   "muscle",     "belly",
                                 "torso-depth", "buttocks", "hips",
                                 "age",      "shoulders",  "deltoid",
                                 "arm-length", "leg-length"};
/// НОМЕРА ВКЛАДОК В ОПИСАНИИ. Названы один раз: их знают все наборы ниже, а
/// «вкладка номер 1» в десяти местах разошлась бы с описанием на первой же
/// новой вкладке.
constexpr std::size_t TAB_ORIGIN = 0;
constexpr std::size_t TAB_BODY = 1;
constexpr std::size_t TAB_NAME = 5;

[[nodiscard]] app::CharGenScreen screen_with(std::size_t morph_count) {
    app::CharGenScreen s;
    std::vector<app::CharGenRow> rows;
    for (std::size_t i = 0; i < morph_count && i < std::size(KNOBS); ++i) {
        app::CharGenRow r;
        r.kind = app::CharGenRowKind::Slider;
        r.name = KNOBS[i];
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
    // ОПИСАНИЕ БЕЗ НАРОДОВ — ЗАКОННОЕ СОСТОЯНИЕ, и набор нарочно гоняет
    // именно его: экран обязан жить на дереве без ассетов народов (вкладка
    // происхождения тогда серая), а сами народы проверяет свой рукав
    // (app_peoples).
    s.set_categories(app::chargen_describe(std::move(rows), {}));
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
        const app::CharGenLayout L = s.layout(w, h);
        CHECK(L.row_y(0) > L.title_y);
        // ПОСЛЕДНЯЯ СТРОКА КОЛОНКИ — НЕ ПОСЛЕДНЯЯ СТРОКА ЭКРАНА: глаголы
        // уехали в подвал, и меряется теперь то, что и должно, — что колонка
        // кончается ВЫШЕ подвала, а подвал и подсказка стоят в кадре.
        CHECK(L.row_y(s.rows().size() - 1) < L.footer_rule_y);
        CHECK(L.verbs_y < L.hint_y);
        CHECK(L.hint_y < h);
        CHECK(L.item_px >= 1);
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
    const app::CharGenLayout L = s.layout(w, h);
    const std::vector<app::CharGenTabBox> boxes =
        app::chargen_tab_boxes(L, s.categories());
    REQUIRE(boxes.size() == s.categories().size());
    // ЯЩИК И НАДПИСЬ — ОДНА АРИФМЕТИКА: спрашиваем середину каждого ящика и
    // требуем ту же вкладку. ВЫКЛЮЧЕННЫЕ («Лицо», «Волосы», «Цвета») обязаны
    // НЕ отвечать: серая надпись, съевшая клик, — худший вид заглушки.
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const int mid = boxes[i].x + boxes[i].w / 2;
        const std::size_t hit = s.tab_at(w, h, mid, L.tabs_y + 2);
        CAPTURE(i);
        if (s.categories()[i].enabled) {
            CHECK(hit == i);
        } else {
            CHECK(hit == s.categories().size());
        }
    }
    // Ниже полосы вкладок — «ни на какой»: иначе щелчок по первой строке
    // молча переключал бы категорию.
    CHECK(s.tab_at(w, h, boxes[0].x + 1, L.row_y(0)) == s.categories().size());
    // И щелчок по живой вкладке ручку не берёт, а вкладку меняет.
    const std::size_t body_tab = 1;
    REQUIRE(s.categories()[body_tab].enabled);
    CHECK(s.press(w, h, boxes[body_tab].x + boxes[body_tab].w / 2, L.tabs_y + 2)
          == s.row_count());
    CHECK(s.category() == body_tab);
}

TEST_CASE("раскладка: указатель попадает в ту же строку, на которую смотрит глаз") {
    app::CharGenScreen s = screen_with(11);
    const int w = 1920;
    const int h = 1080;
    const app::CharGenLayout L = s.layout(w, h);
    for (std::size_t i = 0; i < s.rows().size(); ++i) {
        CHECK(s.row_at(w, h, L.track_x + 4, L.row_y(i)) == i);
    }
    // ГЛАГОЛЫ ПОДВАЛА — ПО СВОИМ ЯЩИКАМ, и это тот же вопрос «строка под
    // указателем»: они все на одной высоте, и спрашивать про них «в какой
    // строке колонки лежит y» значило бы всегда получать первый.
    const std::vector<app::CharGenTabBox> vb = app::chargen_verb_boxes(L, s.verbs());
    for (std::size_t v = 0; v < vb.size(); ++v) {
        CAPTURE(v);
        CHECK(s.row_at(w, h, vb[v].x + vb[v].w / 2, L.verbs_y + 1)
              == s.rows().size() + v);
    }
    // Правее панели — «ни на чём»: наведение на фигуру не двигает выбор.
    CHECK(s.row_at(w, h, w - 10, L.row_y(0)) == s.row_count());
    CHECK(s.over_figure(w, h, w - 10));
    CHECK_FALSE(s.over_figure(w, h, L.track_x));
}

// --- МОДЕЛЬ ЭКРАНА ----------------------------------------------------------

TEST_CASE("описание: шесть вкладок, три из них честные заглушки") {
    app::CharGenScreen s = screen_with(11);
    REQUIRE(s.categories().size() == 6);
    // ПОРЯДОК ВКЛАДОК — ЭТО ПОРЯДОК ПРИНЯТИЯ РЕШЕНИЙ (CHARGEN_UI.md, Р7), от
    // крупного к мелкому. Он проверяется, а не подразумевается: перестановка
    // здесь — это перестановка того, о чём игрока спрашивают первым.
    CHECK(s.categories()[TAB_ORIGIN].key == "chargen.tab.origin");
    CHECK(s.categories()[TAB_BODY].key == "chargen.tab.body");
    CHECK(s.categories()[2].key == "chargen.tab.face");
    CHECK(s.categories()[3].key == "chargen.tab.hair");
    CHECK(s.categories()[4].key == "chargen.tab.colours");
    CHECK(s.categories()[TAB_NAME].key == "chargen.tab.name");
    // ЛИЦО, ВОЛОСЫ И ЦВЕТА — СЕРЫЕ: фазы не начаты (лицу нужна голова, цветам
    // — зона материалов). Игрок видит карту дороги, а не пустоту.
    CHECK_FALSE(s.categories()[2].enabled);
    CHECK_FALSE(s.categories()[3].enabled);
    CHECK_FALSE(s.categories()[4].enabled);
    CHECK(s.categories()[TAB_BODY].enabled);
    CHECK(s.categories()[TAB_NAME].enabled);
    // ПРОИСХОЖДЕНИЕ БЕЗ НАРОДОВ — ТОЖЕ ЗАГЛУШКА, и экран открывается НЕ на
    // ней: открыться на серой вкладке значило бы показать пустую колонку и ни
    // одной причины.
    CHECK_FALSE(s.categories()[TAB_ORIGIN].enabled);
    CHECK(s.category() == TAB_BODY);

    // Вкладка тела: 11 целей + рост + шесть глаголов подвала.
    REQUIRE(s.verbs().size() == 6);
    REQUIRE(s.row_count() == 18);
    CHECK(s.row_kind(0) == app::CharGenRowKind::Slider);
    CHECK(s.row_kind(11) == app::CharGenRowKind::Slider); // рост
    CHECK(s.row_kind(12) == app::CharGenRowKind::Button); // сброс
    REQUIRE(s.find(app::CHARGEN_HEIGHT_KEY) != nullptr);
    CHECK(s.find(app::CHARGEN_HEIGHT_KEY)->metres);
    // РАЗДЕЛЫ СТОЯТ ТАМ, ГДЕ НАЗВАНЫ, и рост — последний: он единственная
    // ручка с человеческой единицей, и прятать её в середину списка значило
    // бы, что игрок ищет свой рост среди безразмерных весов.
    CHECK(s.rows().front().group_key == "chargen.group.trunk");
    CHECK(s.rows().back().name == app::CHARGEN_HEIGHT_KEY);
    CHECK(s.rows().back().group_key == "chargen.group.height");
    // ЗАРУБКА РОСТА — ПОЛАЯ, остальных — сплошная. Полосу роста держит
    // константа, а не судья: он масштабу безразличен по построению, и без
    // этой разницы правило «зарубка = измеренная нейтраль» на одной строке
    // тихо врало бы (CHARGEN_UI.md, Р3).
    CHECK_FALSE(s.rows().back().marks.measured);
    CHECK(s.rows().back().marks.has_notch);
    CHECK(s.rows().front().marks.measured);
    // СЛОВЕСНАЯ ПАРА ПО КРАЯМ — у каждой дорожки, и ключ строится из имени
    // цели: новая цель заводит свою пару в локализации, а не в коде.
    CHECK(s.rows().front().lo_word_key == "morph.edge.weight.lo");
    CHECK(s.rows().front().hi_word_key == "morph.edge.weight.hi");

    // Вкладка имени: одно поле ввода и те же шесть глаголов.
    s.set_category(TAB_NAME);
    REQUIRE(s.row_count() == 7);
    CHECK(s.row_kind(0) == app::CharGenRowKind::Text);
    // ГЛАГОЛЫ ЕСТЬ НА ВСЕХ ВКЛАДКАХ. «Готово», спрятанное внутрь одной,
    // означало бы, что кнопка выхода зависит от того, где стоял игрок.
    CHECK(s.row_at_index(5)->action == app::CharGenAction::Back);
    CHECK(s.row_at_index(6)->action == app::CharGenAction::Done);
    // «ПРЕСЕТЫ» — ЧЕСТНЫЙ СЕРЫЙ ХВОСТ: библиотеки пресетов в дереве нет,
    // единственные пресеты — это типажи народа. Выключенная строка не
    // активируется и не берёт на себя выбор.
    const app::CharGenRow* presets = s.row_at_index(3);
    REQUIRE(presets != nullptr);
    CHECK(presets->action == app::CharGenAction::Presets);
    CHECK_FALSE(presets->enabled);
    s.set_selection(3);
    CHECK(s.activate() == app::CharGenAction::None);
}

TEST_CASE("описание с народами: вкладка происхождения оживает") {
    // ВКЛАДКА ЖИВА РОВНО ПОКА В ДЕРЕВЕ ЕСТЬ НАРОДЫ. Народ здесь выдуман
    // нарочно: предмет — «описание принимает народы», а сами четыре народа
    // проверяет свой рукав (app_peoples) на настоящих файлах.
    app::People folk;
    folk.id = "проба";
    folk.name_key = "chargen.people.venedy";
    folk.blurb_key = "chargen.people.venedy.blurb";
    folk.naming.rule_key = "chargen.naming.venedy";
    app::PeopleArchetype a;
    a.id = "one";
    a.name_key = "chargen.archetype.venedy.plowman";
    a.frequency = 100.0f;
    folk.archetypes.push_back(a);
    const std::vector<app::People> peoples{folk};

    std::vector<app::CharGenRow> rows;
    app::CharGenRow r;
    r.kind = app::CharGenRowKind::Slider;
    r.name = "weight";
    r.lo = -1.0f;
    r.hi = 1.0f;
    rows.push_back(std::move(r));

    app::CharGenScreen s;
    s.set_categories(app::chargen_describe(std::move(rows), peoples));
    REQUIRE(s.categories().size() == 6);
    CHECK(s.categories()[TAB_ORIGIN].enabled);
    // И ЭКРАН ОТКРЫВАЕТСЯ НА НЕЙ: порядок вкладок есть порядок решений.
    CHECK(s.category() == TAB_ORIGIN);
    REQUIRE(s.find(app::CHARGEN_PEOPLE_ROW) != nullptr);
    REQUIRE(s.find(app::CHARGEN_ARCHETYPE_ROW) != nullptr);
    REQUIRE(s.find(app::CHARGEN_SEX_ROW) != nullptr);
    CHECK(s.find(app::CHARGEN_PEOPLE_ROW)->choices.size() == 1);
    CHECK(s.find(app::CHARGEN_ARCHETYPE_ROW)->choices.size() == 1);
    // ПОЛ ЧЕСТНО ОГРАНИЧЕН ИМЕНЕМ, и это НАПИСАНО СЛОВАМИ, а не спрятано
    // отсутствием строки (CHARGEN_UI.md, Р9).
    CHECK(s.find(app::CHARGEN_SEX_ROW)->note_key == "chargen.sex.note");
    // «СЛУЧАЙНОЕ ИМЯ» СТОИТ НА ВКЛАДКЕ ИМЕНИ, а не в подвале: подвальные
    // глаголы общие для всех вкладок, а бросок имени принадлежит имени.
    s.set_category(TAB_NAME);
    REQUIRE(s.rows().size() == 2);
    CHECK(s.rows()[1].action == app::CharGenAction::RollName);
}

TEST_CASE("вкладки: листание ПЕРЕПРЫГИВАЕТ серые, и выбор возвращается в начало") {
    app::CharGenScreen s = screen_with(11);
    s.set_selection(9);
    // ЖИВЫХ ВКЛАДОК ДВЕ (тело и имя), между ними три серых. Tab, упирающийся
    // в «Лицо», читался бы как сломанный Tab, а не как ненаписанная фаза.
    CHECK(s.category() == TAB_BODY);
    s.cycle_category(+1);
    CHECK(s.category() == TAB_NAME);
    CHECK(s.selection() == 0); // номер строки на другой вкладке — другая строка
    s.cycle_category(+1);
    CHECK(s.category() == TAB_BODY);
    s.cycle_category(-1);
    CHECK(s.category() == TAB_NAME);
    // И ПРЯМОЙ ПЕРЕХОД НА СЕРУЮ ОТКАЗЫВАЕТ, а не тихо соглашается.
    s.set_category(2);
    CHECK(s.category() == TAB_NAME);
    s.set_category(TAB_BODY);
    CHECK(s.category() == TAB_BODY);
    // Значение, набранное на одной вкладке, ВИДНО с другой: пресет собирается
    // со всего экрана, а не с открытой страницы.
    s.set_category(TAB_BODY);
    CHECK(s.set_value("torso-depth", 0.5f));
    s.set_category(TAB_NAME);
    REQUIRE(s.find("torso-depth") != nullptr);
    CHECK(s.find("torso-depth")->value == doctest::Approx(0.5f));
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
    CHECK(s.find("weight")->value == doctest::Approx(1.0f));
    for (int i = 0; i < 1000; ++i) {
        (void)s.adjust(-1, false);
    }
    CHECK(s.find("weight")->value == doctest::Approx(-1.0f));
    // На строке-глаголе стрелка не делает НИЧЕГО и говорит об этом номером.
    s.set_selection(s.row_count() - 1);
    CHECK(s.adjust(+1, false) == s.row_count());
}

TEST_CASE("сброс: ползунки в ноль, рост в канон, имя НЕ трогается") {
    app::CharGenScreen s = screen_with(2);
    CHECK(s.set_value("weight", 0.8f));
    CHECK(s.set_value(app::CHARGEN_HEIGHT_KEY, app::CHARGEN_HEIGHT_MAX_M));
    s.set_name("Гуннар");
    s.reset_rows();
    CHECK(s.find("weight")->value == doctest::Approx(0.0f));
    CHECK(s.find(app::CHARGEN_HEIGHT_KEY)->value
          == doctest::Approx(app::CHARGEN_BODY_HEIGHT_M));
    CHECK(s.name() == "Гуннар");
}

TEST_CASE("имя: кириллица считается ЗНАКАМИ, а стирается по знаку") {
    app::CharGenScreen s = screen_with(1);
    s.set_category(TAB_NAME);
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
    const app::CharGenLayout L = s.layout(w, h);
    const app::SliderTrack track = L.track_of(1);
    const std::size_t grabbed = s.press(w, h, track.x + track.w, track.y);
    CHECK(grabbed == 1);
    CHECK(s.dragging());
    CHECK(s.find("muscle")->value == doctest::Approx(1.0f));
    CHECK(s.drag(w, h, track.x));
    CHECK(s.find("muscle")->value == doctest::Approx(-1.0f));
    s.release();
    CHECK_FALSE(s.dragging());
    // Нажатие по глаголу ручку НЕ берёт.
    const std::vector<app::CharGenTabBox> vb = app::chargen_verb_boxes(L, s.verbs());
    CHECK(s.press(w, h, vb[0].x + 1, L.verbs_y + 1) == s.row_count());
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
    if (!body_has_morphs()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — набор пропущен");
        return;
    }
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(rs, renderer, nullptr, rig, BODY_PATH));
    CHECK(body.ready());
    // КОНТРОЛЬ: пока тело показано, живые меши ОБЯЗАНЫ быть — тело и клинок
    // в руке (игровой персонаж несёт оба). Без этой строки утверждение
    // держалось бы и для экрана, который не залил ничего.
    const std::uint32_t shown = renderer.live_meshes();
    CHECK(shown >= 1);
    REQUIRE_FALSE(body.morphs().empty());

    for (int i = 0; i < 60; ++i) {
        const std::size_t slot = static_cast<std::size_t>(i) % body.morphs().size();
        const render::MorphTarget& t = body.morphs()[slot];
        (void)body.set_weight(slot, (i % 2 == 0) ? t.hi : t.lo);
        // БЫСТРАЯ ПОЛОВИНА — замена буфера парой create/destroy (RenderSystem::
        // replace_skinned_mesh): число живых не растёт.
        REQUIRE(body.apply(rs, renderer));
        REQUIRE(renderer.live_meshes() == shown);
    }
    // МЕДЛЕННАЯ ПОЛОВИНА — полная пересборка фабрикой: старые номера
    // отпускаются ПЕРЕД новой регистрацией, число живых то же.
    REQUIRE(body.settle(rs, renderer, nullptr));
    CHECK(renderer.live_meshes() == shown);
    body.release(rs, renderer, nullptr);
    CHECK(renderer.live_meshes() == 0);
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
    CHECK_FALSE(obj->skeleton.empty());
    // СЕКЦИЯ MORF — ОТДЕЛЬНЫМ СООБЩЕНИЕМ, А НЕ ОТКАЗОМ: тело без неё законно
    // (выпеченный персонаж, перепечка целей), и предмет ЭТОГО набора —
    // «экран грузит тот же скинированный файл», а не «у файла есть ползунки».
    if (obj->morphs.empty()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — ползунков не будет");
    }

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
    render::RenderSystem rs;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(rs, renderer, nullptr, rig, app::CHARGEN_SOURCE_BODY));
    CHECK(body.triangles() == obj->skin.indices.size() / 3);
    CHECK(body.morphs().size() == obj->morphs.size());
    body.release(rs, renderer, nullptr);
}

TEST_CASE("MORF-бленд ЖИВОЙ: вес двигает вершины, а не только число на экране") {
    // ПОЛЗУНОК, КОТОРЫЙ КРУТИТСЯ И НЕ ЛЕПИТ, — это худший из отказов экрана
    // создания: он выглядит рабочим. Прибор — ГАБАРИТ рест-позы: он считается
    // после бленда и скиннинга, то есть меряет то самое, что уходит на
    // видеокарту.
    if (!body_has_morphs()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — набор пропущен");
        return;
    }
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(rs, renderer, nullptr, rig, app::CHARGEN_SOURCE_BODY));
    const glm::vec3 lo0 = body.lo();
    const glm::vec3 hi0 = body.hi();
    // ЛИЦЕВЫЕ ЦЕЛИ ЖИВУТ ВНУТРИ СИЛУЭТА и габарита не двигают по построению;
    // их живость меряется ВЕРШИНАМИ, телесных — и габаритом тоже.
    app::FacePlan plan;
    std::string why;
    (void)app::read_face_manifest(fs::path(app::FACE_MANIFEST_PATH), plan, why);
    std::vector<glm::vec3> neutral;
    for (const platform::SkinnedVertex& v : body.character().current_vertices()) {
        neutral.push_back(v.position);
    }
    int moved = 0;
    int body_targets = 0;
    int alive = 0;
    for (std::size_t i = 0; i < body.morphs().size(); ++i) {
        const render::MorphTarget& t = body.morphs()[i];
        // ОДНО ТЕЛО, ОДИН ПОЛЗУНОК ЗА РАЗ: тело экрана держит номера мешей
        // игрового персонажа (CharacterFactory), и второе тело на той же
        // системе рендера получило бы отказ «номер занят» — правильный.
        body.reset();
        // КРАЙ ПОЛОСЫ, А НЕ СЕРЕДИНА: у половины целей нейтраль стоит НА краю
        // (belly [0, 0.45], age [0, 0.55]), и «сдвинуть в середину» для них
        // значит сдвинуть меньше, чем позволяет цель.
        const float far_end = (std::fabs(t.hi) > std::fabs(t.lo)) ? t.hi : t.lo;
        REQUIRE(body.set_weight(i, far_end));
        REQUIRE(body.apply(rs, renderer));
        const bool is_face = plan.find(t.name) != nullptr;
        if (!is_face) {
            ++body_targets;
            if (glm::length(body.lo() - lo0) + glm::length(body.hi() - hi0) > 1e-4f) {
                ++moved;
            }
        }
        // ЖИВАЯ ЦЕЛЬ ДВИГАЕТ ВЕРШИНЫ: полмиллиметра — пиксель портретного кадра
        // (tools/check_morph_bands.py, --face-threshold).
        const auto& now = body.character().current_vertices();
        REQUIRE(now.size() == neutral.size());
        std::size_t verts_moved = 0;
        for (std::size_t k = 0; k < now.size(); ++k) {
            if (glm::length(now[k].position - neutral[k]) > 0.0005f) {
                ++verts_moved;
            }
        }
        CAPTURE(t.name);
        CHECK(verts_moved > 0);
        alive += verts_moved > 0 ? 1 : 0;
    }
    // НЕ «ХОТЬ ОДНА»: телесная цель, которая не двигает габарит, ещё может
    // двигать вершины внутри силуэта (мускулатура), поэтому порог — большинство,
    // а не все. Ноль сдвинувших значил бы, что бленда нет вовсе.
    CHECK(moved >= body_targets / 2);
    CHECK(alive == static_cast<int>(body.morphs().size()));
    body.release(rs, renderer, nullptr);
}

TEST_CASE("тело: вес зажимается ПОЛОСОЙ ЦЕЛИ из файла") {
    if (!body_has_morphs()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — набор пропущен");
        return;
    }
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(rs, renderer, nullptr, rig, BODY_PATH));
    const render::MorphTarget& t = body.morphs()[0];
    (void)body.set_weight(0, 1e6f);
    CHECK(body.weights().weights[0] == doctest::Approx(t.hi));
    (void)body.set_weight(0, -1e6f);
    CHECK(body.weights().weights[0] == doctest::Approx(t.lo));
    body.release(rs, renderer, nullptr);
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
    // НАРОД И ТИПАЖ ЕДУТ В ФАЙЛЕ, и это условие того, что генератор населения
    // ест выход экрана создания без переходника (CHARGEN_UI.md, раздел 4):
    // один формат на пресет игрока, типаж народа и запись НПС.
    p.people = "skeldy";
    p.archetype = "whaler";
    p.height_m = 1.712f;
    p.sliders = {{"belly", 0.31f}, {"hips", -0.75f}, {"weight", 0.0f}};
    REQUIRE(app::write_chargen_preset(out, p));
    app::CharGenPreset back;
    REQUIRE(app::read_chargen_preset(out, back));
    CHECK(back.name == p.name);
    CHECK(back.people == p.people);
    CHECK(back.archetype == p.archetype);
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
    if (!body_has_morphs()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — набор пропущен");
        return;
    }
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(rs, renderer, nullptr, rig, BODY_PATH));
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

    body.release(rs, renderer, nullptr);
    std::error_code ec;
    fs::remove(a, ec);
    fs::remove(b, ec);
    fs::remove(canon, ec);
}

TEST_CASE("пресет старше тела: неизвестный ползунок ПРОПУСКАЕТСЯ, а не рушит экран") {
    if (!body_has_morphs()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — набор пропущен");
        return;
    }
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(rs, renderer, nullptr, rig, BODY_PATH));
    app::CharGenPreset p;
    p.height_m = 1.70f;
    p.sliders = {{"нет-такой-цели", 0.5f}, {body.morphs()[0].name, body.morphs()[0].hi}};
    body.apply_preset(p);
    CHECK(body.height_m() == doctest::Approx(1.70f));
    CHECK(body.weights().weights[0] == doctest::Approx(body.morphs()[0].hi));
    body.release(rs, renderer, nullptr);
}

// --- ГОЛОВА И ЛИЦО ------------------------------------------------------------
//
// ДВА ПРИБОРА ОДНОГО ДЕФЕКТА (нос-шип на экране создания, 02.09). На кадре нос
// вытягивался полосой вниз-вправо, и первая гипотеза была «цели MORF трогают
// голову». Прибор без кадра показал другое: цели тела трогают 0 вершин головы,
// а шип живёт в ВЕСАХ СКИНА — 496 вершин лица (нос, губы, челюсть) несли
// DEF-neck (кончик носа: 0.745 шеи против 0.128 у соседа в 5 мм), и в клипе
// покоя, где голова повёрнута относительно шеи, M_neck·p и M_head·p на носу
// расходились на 30 мм. В рест-позе палитры шеи и головы совпадают — потому на
// стенде и на DFN_CHARGEN_POSE=rest лицо было целым. Исправлено в весах
// (fa6b26ee: лицо → DEF-head); эти два набора держат оба утверждения порознь.

namespace {

/// ГРАНИЦА ГОЛОВЫ — ЛИНИЯ ШЕИ, долей роста от макушки вниз. DEF-neck у
/// HumanBase стоит на 1.501 м при росте 1.739 (0.137 роста от макушки); 0.14
/// берёт его с запасом в полсантиметра и не привязывает набор к одному телу.
constexpr float HEAD_FROM_TOP_FRAC = 0.14f;
/// ЛИЦО — выше подбородка (0.125 роста от макушки: 1.52 м) и ПЕРЕД шеей
/// (z < −0.03: персонаж смотрит вдоль −Z, docs/RIG.md).
constexpr float FACE_FROM_TOP_FRAC = 0.125f;
constexpr float FACE_FRONT_Z = -0.03f;

[[nodiscard]] float top_y(std::span<const glm::vec3> rest) {
    float top = -1e9f;
    for (const glm::vec3& p : rest) {
        top = std::max(top, p.y);
    }
    return top;
}

} // namespace

TEST_CASE("лицо едет с головой ЖЁСТКО: в клипе покоя ни одна вершина лица не отстаёт от DEF-head больше 3 мм") {
    if (!body_present()) {
        MESSAGE("HumanBase.dfo нет в дереве — набор пропущен");
        return;
    }
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(rs, renderer, nullptr, rig, BODY_PATH));
    std::vector<glm::vec3> rest;
    body.character().rest_positions(rest);
    const auto& verts = body.character().current_vertices();
    const auto& skel = body.character().skeleton();
    std::size_t head = skel.size();
    for (std::size_t j = 0; j < skel.size(); ++j) {
        if (skel.joints[j].name == "DEF-head") {
            head = j;
        }
    }
    REQUIRE(head < skel.size());
    // ТА ЖЕ ПОЗА, ЧТО В КАДРЕ ПРИЁМКИ: 45 тиков покоя (DFN_SHOT_AFTER=45) и
    // палитра того же build_draw, что уходит на видеокарту; alpha 0 — как у
    // стоящей камеры меню (AppCharGen.cpp).
    for (int frame = 0; frame < 45; ++frame) {
        body.tick(1.0f / 60.0f);
    }
    const render::RenderSystem::SkinnedDraw draw = body.draw(0.0f, nullptr, glm::mat4{1.0f});
    REQUIRE(draw.palette.size() == skel.size());
    const float top = top_y(rest);
    std::size_t face = 0;
    std::size_t over = 0;
    float worst = 0.0f;
    std::size_t worst_i = 0;
    for (std::size_t i = 0; i < verts.size(); ++i) {
        if (rest[i].y < top - FACE_FROM_TOP_FRAC * top || rest[i].z > FACE_FRONT_Z) {
            continue;
        }
        ++face;
        // ЖЁСТКОЕ ДВИЖЕНИЕ ГОЛОВЫ — та же вершина, пронесённая ОДНОЙ матрицей
        // DEF-head; остаток — то, что лицу дали чужие кости.
        const glm::vec3 posed = anim::cpu_skin_position(verts[i], draw.palette);
        const glm::vec3 rigid =
            glm::vec3{draw.palette[head] * glm::vec4{verts[i].position, 1.0f}};
        const float residual = glm::length(posed - rigid);
        if (residual > 0.003f) {
            ++over;
        }
        if (residual > worst) {
            worst = residual;
            worst_i = i;
        }
    }
    // ЛИЦО ЕСТЬ: пустая выборка прошла бы любой порог.
    REQUIRE(face > 1000);
    CAPTURE(face);
    CAPTURE(worst_i);
    MESSAGE("лицо: " << face << " вершин, худший остаток от жёсткой головы "
                     << worst * 1000.0f << " мм (вершина " << worst_i << "), >3 мм: " << over);
    // ДО fa6b26ee (тот же набор на старом теле): 2843 вершины лица, худший
    // остаток 28.9 мм, дальше 3 мм — 1055. ПОСЛЕ: худший 2.0 мм, дальше 3 мм — 0.
    CHECK(over == 0);
    CHECK(worst < 0.003f);
    body.release(rs, renderer, nullptr);
}

TEST_CASE("цели тела не трогают голову: ручки частей — ни одной вершины выше шеи дальше 2 мм, макро — по паспорту") {
    if (!body_has_morphs()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — набор пропущен");
        return;
    }
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(rs, renderer, nullptr, rig, BODY_PATH));
    std::vector<glm::vec3> rest;
    body.character().rest_positions(rest);
    const float top = top_y(rest);
    const float neck_y = top - HEAD_FROM_TOP_FRAC * top;
    // ПАСПОРТ МАКРО-РУЧЕК: они трогают ВСЁ тело по построению MakeHuman (возраст
    // — сутулость, голова едет вниз целиком; вес и мышцы — шея и щёки; глубина
    // туловища — основание шеи). Числа — замер tools/make_body_targets.py на
    // fa6b26ee, ход головы на КРАЮ ПОЛОСЫ: age 16.1 мм, torso-depth 5.9,
    // weight 4.6, muscle 1.8. Потолки — с запасом на четверть; ручка, которой
    // здесь нет, обязана не тронуть НИ ОДНОЙ вершины головы.
    const std::map<std::string, float> passport_mm{
        {"age", 20.0f}, {"torso-depth", 8.0f}, {"weight", 6.0f}, {"muscle", 3.0f}};
    REQUIRE(body.morphs().size() >= 10);
    // ЛИЦЕВЫЕ ЦЕЛИ (манифест) ГОЛОВУ ДВИГАЮТ ПО ОПРЕДЕЛЕНИЮ — это набор про
    // ТЕЛЕСНЫЕ; их собственное утверждение обратное: «не трогают тело ниже
    // шеи», и оно стоит своим набором ниже.
    app::FacePlan plan;
    std::string why;
    (void)app::read_face_manifest(fs::path(app::FACE_MANIFEST_PATH), plan, why);
    std::size_t body_targets = 0;
    for (const render::MorphTarget& t : body.morphs()) {
        if (plan.find(t.name) != nullptr) {
            continue;
        }
        ++body_targets;
        const float band = std::max(std::fabs(t.lo), std::fabs(t.hi));
        std::size_t head_moved = 0;
        float head_worst = 0.0f;
        for (const render::MorphDelta& d : t.deltas) {
            if (d.index >= rest.size() || rest[d.index].y <= neck_y) {
                continue;
            }
            const float len = glm::length(d.offset) * band;
            // ДВА МИЛЛИМЕТРА — порог ВИДИМОГО: полвершины ключицы, зацепленные
            // «плечами» на линии шеи на 0.18 мм, — не дефект, а край региона.
            if (len > 0.002f) {
                ++head_moved;
            }
            head_worst = std::max(head_worst, len);
        }
        CAPTURE(t.name);
        MESSAGE(t.name << ": голова — " << head_moved << " вершин дальше 2 мм, макс "
                       << head_worst * 1000.0f << " мм на краю полосы [" << t.lo << ", "
                       << t.hi << "]");
        if (const auto it = passport_mm.find(t.name); it != passport_mm.end()) {
            CHECK(head_worst * 1000.0f <= it->second);
        } else {
            CHECK(head_moved == 0);
        }
    }
    CHECK(body_targets >= 10);
    body.release(rs, renderer, nullptr);
}

// --- ЛИЦО: ОПИСЬ, ВКЛАДКА, ЦЕЛИ ------------------------------------------------
//
// ВОЛНА «ЛИЦО ПОЛЗУНКАМИ». Вкладка «Лицо» строится ДАННЫМИ: манифест
// assets/characters/targets/face.targets даёт группы и порядок, секция MORF
// тела — полосы, калибровка face.bands — сплошной или полый ромб. Ниже: разбор
// манифеста (в том числе ОТКАЗ на кривой строке), описание с планом, и на
// НАСТОЯЩЕМ теле — «каждая ручка манифеста нашла свою цель», «цели лица не
// трогают тело ниже шеи», «пресет с лицом печётся байт в байт».

namespace {

constexpr const char* FACE_MANIFEST_SAMPLE =
    "# шапка\n"
    "нос | ширина крыльев | nose-width1-decr/incr | -1.0 1.0 | Ширина крыльев | узкие · широкие\n"
    "нос | кончик | nose-point-down/up | -1.0 1.0 | Кончик | опущен · вздёрнут\n"
    "глаза | размер | {l,r}-eye-scale-decr/incr | -0.6 0.6 | Размер глаз | маленькие · большие\n"
    "глаза | брови | eyebrows-trans-down/up | -0.5 0.5 | Высота бровей | низко · высоко\n"
    "голова | форма: овал | head-oval | 0.0 1.0 | Овальное лицо | нет · да\n";

/// Строки-ползунки ПО ИМЕНАМ — так их отдаёт AppCharGen из секции MORF.
[[nodiscard]] std::vector<app::CharGenRow> rows_named(std::initializer_list<const char*> names) {
    std::vector<app::CharGenRow> rows;
    for (const char* n : names) {
        app::CharGenRow r;
        r.kind = app::CharGenRowKind::Slider;
        r.name = n;
        r.lo = -1.0f;
        r.hi = 1.0f;
        rows.push_back(std::move(r));
    }
    return rows;
}

} // namespace

TEST_CASE("манифест лица: имя ручки выводится из стема, группа — из первого стема") {
    // ОДНО ПРАВИЛО С ЭКСПОРТЁРОМ (tools/make_body_targets.py, handle_name).
    CHECK(app::face_handle_name("nose-width1-decr/incr") == "nose-width1");
    CHECK(app::face_handle_name("{l,r}-eye-scale-decr/incr") == "eye-scale");
    CHECK(app::face_handle_name("{l,r}-eye-trans-in/out") == "eye-trans");
    CHECK(app::face_handle_name("nose-point-down/up") == "nose-point");
    CHECK(app::face_handle_name("nose-point-width-decr/incr") == "nose-point-width");
    CHECK(app::face_handle_name("forehead-trans-backward/forward") == "forehead-trans");
    CHECK(app::face_handle_name("head-oval") == "head-oval");
    CHECK(app::face_group_id("{l,r}-eye-scale-decr/incr") == "eye");
    CHECK(app::face_group_id("nose-width1-decr/incr") == "nose");
    CHECK(app::face_group_id("head-oval") == "head");

    app::FacePlan plan;
    std::string why;
    REQUIRE(app::parse_face_manifest(FACE_MANIFEST_SAMPLE, plan, why));
    REQUIRE(plan.groups.size() == 3);
    CHECK(plan.groups[0].id == "nose");
    CHECK(plan.groups[1].id == "eye");
    CHECK(plan.groups[2].id == "head");
    CHECK(plan.handle_count() == 5);
    REQUIRE(plan.find("eye-scale") != nullptr);
    CHECK(plan.find("eye-scale")->lo == doctest::Approx(-0.6f));
    CHECK(plan.find("eye-scale")->hi == doctest::Approx(0.6f));
    CHECK_FALSE(plan.find("eye-scale")->measured); // без калибровки — полый ромб
    CHECK(plan.find("нет-такой") == nullptr);

    // ОТКАЗ ГРОМКИЙ, А НЕ ПРОПУСК: кривая строка — это ручка, которую потом
    // два часа ищут на экране.
    CHECK_FALSE(app::parse_face_manifest("нос | ручка | nose-hump-decr/incr | -1 1\n", plan, why));
    CHECK_FALSE(why.empty());
    CHECK_FALSE(app::parse_face_manifest("нос | а | nose-hump-decr/incr | 1.0 -1.0 | п | а · б\n",
                                         plan, why));
    CHECK_FALSE(app::parse_face_manifest(
        "нос | а | nose-hump-decr/incr | -1 1 | п | а · б\nрот | б | nose-hump-decr/incr | -1 1 | п | а · б\n",
        plan, why)); // одна ручка дважды
    CHECK_FALSE(app::parse_face_manifest("# только шапка\n", plan, why));

    // КАЛИБРОВКА СТАВИТ РОМБ.
    REQUIRE(app::parse_face_manifest(FACE_MANIFEST_SAMPLE, plan, why));
    std::vector<app::FaceBand> bands;
    bands.push_back(app::FaceBand{"eye-scale", -0.6f, 0.6f, true});
    bands.push_back(app::FaceBand{"head-oval", 0.0f, 0.75f, true});
    bands.push_back(app::FaceBand{"чужая", 0.0f, 1.0f, true});
    CHECK(app::face_plan_apply_bands(plan, bands) == 2);
    CHECK(plan.find("eye-scale")->measured);
    CHECK(plan.find("head-oval")->measured);
    CHECK_FALSE(plan.find("nose-width1")->measured);
}

TEST_CASE("описание с лицом: вкладка «Лицо» оживает из манифеста, разделы и ромбы — из данных") {
    app::FacePlan plan;
    std::string why;
    REQUIRE(app::parse_face_manifest(FACE_MANIFEST_SAMPLE, plan, why));
    std::vector<app::FaceBand> bands;
    bands.push_back(app::FaceBand{"eye-scale", -0.6f, 0.6f, true});
    (void)app::face_plan_apply_bands(plan, bands);

    // Секция MORF вперемешку: телесные и лицевые цели, как они лежат в файле
    // (по имени), плюс одна лицевая, которой манифест не знает.
    std::vector<app::CharGenRow> rows = rows_named(
        {"eye-scale", "belly", "head-oval", "nose-point", "weight", "nose-width1", "chin-cleft"});
    std::vector<std::string> missing;
    app::CharGenScreen s;
    s.set_categories(app::chargen_describe(std::move(rows), {}, plan, &missing));
    REQUIRE(s.categories().size() == 6);
    CHECK(s.categories()[2].key == app::CHARGEN_FACE_TAB_KEY);
    CHECK(s.categories()[2].enabled);
    CHECK(s.categories()[2].zoom == doctest::Approx(1.0f)); // кадр вкладки — лицо (Р4)
    CHECK_FALSE(s.categories()[3].enabled);                  // волосы — серые
    // РУЧКА МАНИФЕСТА БЕЗ ЦЕЛИ В ТЕЛЕ НАЗВАНА ВСЛУХ, а не пропала.
    REQUIRE(missing.size() == 1);
    CHECK(missing[0] == "eyebrows-trans");

    s.set_category(2);
    const std::vector<app::CharGenRow>& face = s.rows();
    REQUIRE(face.size() == 4);
    // ПОРЯДОК — МАНИФЕСТА, не файла тела: нос, нос, глаза, голова.
    CHECK(face[0].name == "nose-width1");
    CHECK(face[1].name == "nose-point");
    CHECK(face[2].name == "eye-scale");
    CHECK(face[3].name == "head-oval");
    CHECK(face[0].group_key == "chargen.group.face.nose");
    CHECK(face[1].group_key.empty());
    CHECK(face[2].group_key == "chargen.group.face.eye");
    CHECK(face[3].group_key == "chargen.group.face.head");
    CHECK(face[0].lo_word_key == "morph.edge.nose-width1.lo");
    CHECK(face[0].hi_word_key == "morph.edge.nose-width1.hi");
    // ЗАРУБКА — НОЛЬ, РОМБ — ПО КАЛИБРОВКЕ: eye-scale мерил судья, остальные нет.
    CHECK(face[2].marks.has_notch);
    CHECK(face[2].marks.notch == doctest::Approx(0.0f));
    CHECK(face[2].marks.measured);
    CHECK_FALSE(face[0].marks.measured);
    CHECK_FALSE(face[3].marks.measured);
    // ТЕЛО ОСТАЛОСЬ ТЕЛОМ, а лицевая цель без строки в манифесте (chin-cleft)
    // попала в хвост тела — на экран, не в никуда.
    s.set_category(1);
    bool belly = false;
    bool cleft = false;
    for (const app::CharGenRow& r : s.rows()) {
        belly = belly || r.name == "belly";
        cleft = cleft || r.name == "chin-cleft";
        CHECK(r.name != "eye-scale");
    }
    CHECK(belly);
    CHECK(cleft);
    // БЕЗ ПЛАНА — ВКЛАДКА СЕРАЯ, как и было.
    app::CharGenScreen bare;
    bare.set_categories(app::chargen_describe(rows_named({"eye-scale", "belly"}), {}));
    CHECK_FALSE(bare.categories()[2].enabled);
}

TEST_CASE("кадр категории приезжает сам: приближение едет к цели за четверть секунды и не дальше") {
    app::CharGenView view;
    view.zoom = 0.0f;
    // 0.25 с при 60 к/с — 15 шагов; на пятнадцатом — ровно цель, дальше нет.
    int steps = 0;
    while (app::chargen_glide_zoom(view, 1.0f, 1.0f / 60.0f)) {
        ++steps;
        CHECK(view.zoom > 0.0f);
        CHECK(view.zoom < 1.0f);
        REQUIRE(steps < 100);
    }
    CHECK(view.zoom == doctest::Approx(1.0f));
    CHECK(steps == 14);
    // Обратно — так же; на месте — не дёргается.
    CHECK_FALSE(app::chargen_glide_zoom(view, 1.0f, 0.1f));
    CHECK(view.zoom == doctest::Approx(1.0f));
    CHECK(app::chargen_glide_zoom(view, 0.0f, 0.1f));
    CHECK(view.zoom == doctest::Approx(0.6f));
    // ЗАЩЁЛКА СМЕНЫ ВКЛАДКИ: взводится сменой, снимается вопросом.
    app::CharGenScreen s = screen_with(3);
    CHECK(s.take_frame_change());
    CHECK_FALSE(s.take_frame_change());
    s.set_category(TAB_NAME);
    CHECK(s.take_frame_change());
    s.set_category(TAB_NAME); // та же — не смена
    CHECK_FALSE(s.take_frame_change());
}

TEST_CASE("цели лица: каждая ручка манифеста нашла цель в теле, и ни одна не трогает тело ниже шеи") {
    if (!body_has_morphs()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — набор пропущен");
        return;
    }
    app::FacePlan plan;
    std::string why;
    if (!app::read_face_manifest(fs::path(app::FACE_MANIFEST_PATH), plan, why)) {
        MESSAGE("манифеста лица нет: " << why << " — набор пропущен");
        return;
    }
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(rs, renderer, nullptr, rig, BODY_PATH));
    std::vector<glm::vec3> rest;
    body.character().rest_positions(rest);
    const float top = top_y(rest);
    float floor_y = 1e9f;
    for (const glm::vec3& p : rest) {
        floor_y = std::min(floor_y, p.y);
    }
    const float height = top - floor_y;
    // ПОЛ ЛИЦА — 0.80 роста (tools/make_body_targets.py, FACE_FLOOR_FRAC): ниже
    // основания шеи (плечи 0.818H) лицо не имеет права двигать НИ ОДНОЙ вершины;
    // шею (подбородок тянет горло) — вправе. Тело ниже 0.80H у ЛИЦЕВОЙ цели
    // означает чужой индекс в файле цели MPFB, и экспортёр его отбрасывает.
    const float face_floor = floor_y + 0.80f * height;
    std::size_t face_targets = 0;
    for (const app::FaceGroup& g : plan.groups) {
        for (const app::FaceHandle& h : g.handles) {
            const int idx = render::morph_index(body.morphs(), h.name);
            CAPTURE(h.name);
            // КАЖДАЯ РУЧКА МАНИФЕСТА — ЦЕЛЬ В ТЕЛЕ: два правила имён (экспортёр
            // и FaceManifest.cpp) сошлись на живом файле.
            REQUIRE(idx >= 0);
            const render::MorphTarget& t = body.morphs()[static_cast<std::size_t>(idx)];
            ++face_targets;
            CHECK(t.lo < t.hi);
            // ПОЛОСА ТЕЛА ВНУТРИ МАНИФЕСТНОЙ (калибровка сужает, не расширяет).
            CHECK(t.lo >= h.lo - 1e-4f);
            CHECK(t.hi <= h.hi + 1e-4f);
            std::size_t below = 0;
            float lowest = 1e9f;
            for (const render::MorphDelta& d : t.deltas) {
                REQUIRE(d.index < rest.size());
                lowest = std::min(lowest, rest[d.index].y);
                if (rest[d.index].y < face_floor) {
                    ++below;
                }
            }
            MESSAGE(h.name << ": " << t.deltas.size() << " дельт, нижняя вершина "
                           << (lowest - floor_y) / height << " роста");
            CHECK(below == 0);
        }
    }
    CHECK(face_targets == plan.handle_count());
    CHECK(face_targets >= 40);
    body.release(rs, renderer, nullptr);
}

TEST_CASE("пресет с лицом: чтение-запись переживает лицевые ручки, две выпечки байт в байт") {
    if (!body_has_morphs()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — набор пропущен");
        return;
    }
    app::FacePlan plan;
    std::string why;
    if (!app::read_face_manifest(fs::path(app::FACE_MANIFEST_PATH), plan, why)) {
        MESSAGE("манифеста лица нет: " << why << " — набор пропущен");
        return;
    }
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(rs, renderer, nullptr, rig, BODY_PATH));
    // Треть хода у КАЖДОЙ лицевой ручки плюс одна телесная: пресет со всеми
    // ручками, а не с одной, — сложение float не ассоциативно, и порядок
    // слагаемых проверяется только суммой из многих.
    std::size_t set_face = 0;
    for (const app::FaceGroup& g : plan.groups) {
        for (const app::FaceHandle& h : g.handles) {
            const int idx = render::morph_index(body.morphs(), h.name);
            REQUIRE(idx >= 0);
            const render::MorphTarget& t = body.morphs()[static_cast<std::size_t>(idx)];
            const float v = (t.lo + 2.0f * t.hi) / 3.0f;
            if (body.set_weight(h.name, v)) {
                ++set_face;
            }
        }
    }
    CHECK(set_face >= 40);
    (void)body.set_weight(body.morphs()[0].name, body.morphs()[0].hi * 0.5f);
    const app::CharGenPreset written = body.preset("Ждан");
    const fs::path json = fs::temp_directory_path() / "dfn_chargen_face_preset.json";
    REQUIRE(app::write_chargen_preset(json, written));
    app::CharGenPreset read;
    REQUIRE(app::read_chargen_preset(json, read));
    REQUIRE(read.sliders.size() == written.sliders.size());
    for (std::size_t i = 0; i < read.sliders.size(); ++i) {
        CHECK(read.sliders[i].first == written.sliders[i].first);
        CHECK(read.sliders[i].second == doctest::Approx(written.sliders[i].second));
    }
    // ОДИН ПРОЧИТАННЫЙ ПРЕСЕТ, ДВАЖДЫ ПОДНЯТЫЙ НА ТО ЖЕ ТЕЛО (одно тело на
    // систему рендера — см. набор про бленд), ПЕЧЁТСЯ В ОДИН ФАЙЛ. Предмет —
    // «пресет → выпечка» воспроизводим: числа файла, а не числа в памяти
    // (JSON печатает шесть знаков, и это его точность, а не дефект).
    const fs::path a = fs::temp_directory_path() / "dfn_chargen_face_a.dfo";
    const fs::path b = fs::temp_directory_path() / "dfn_chargen_face_b.dfo";
    body.reset();
    body.apply_preset(read);
    REQUIRE(body.bake(a));
    body.reset();
    (void)body.set_weight(body.morphs()[0].name, body.morphs()[0].lo); // сбить след
    body.apply_preset(read);
    REQUIRE(body.bake(b));
    CHECK(read_bytes(a) == read_bytes(b));
    const auto baked = render::read_object(a);
    REQUIRE(baked);
    CHECK(baked->morphs.empty());
    // И ЛИЦО В ВЫПЕЧКЕ ДЕЙСТВИТЕЛЬНО ДРУГОЕ: вершины головы сдвинуты.
    const auto source = render::read_object(fs::path(BODY_PATH));
    REQUIRE(source);
    REQUIRE(source->skin.vertices.size() == baked->skin.vertices.size());
    std::size_t moved = 0;
    for (std::size_t i = 0; i < baked->skin.vertices.size(); ++i) {
        if (glm::length(baked->skin.vertices[i].position - source->skin.vertices[i].position)
            > 0.0005f) {
            ++moved;
        }
    }
    CHECK(moved > 500);
    body.release(rs, renderer, nullptr);
    std::error_code ec;
    fs::remove(a, ec);
    fs::remove(b, ec);
    fs::remove(json, ec);
}

TEST_CASE("части следуют морфам на пути экрана: на перетаскивании (apply) и после пересборки (settle) волосы стоят над сдвинутым лбом") {
    // ЭКРАН СТРОИТ ТЕЛО ИЗ ВЫПЕЧКИ (settle: baked_object без MORF), и нейтраль
    // для частей приходит из файла HumanBase.dfo (кэш процесса), а не из
    // памяти тела. Без этого волосы после отпускания ручки вернулись бы в
    // рест над уехавшим черепом — ровно хвост отчёта лица (13.7 мм).
    if (!body_has_morphs()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — набор пропущен");
        return;
    }
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::CharGenBody body;
    REQUIRE(body.load(rs, renderer, nullptr, rig, app::CHARGEN_SOURCE_BODY));
    const app::CharacterParts& parts = body.character().parts();
    if (parts.empty()) {
        MESSAGE("частей у тела экрана нет (нет HumanBase.parts.dfo) — набор пропущен");
        body.release(rs, renderer, nullptr);
        return;
    }
    REQUIRE(parts.following());
    const auto hair = [&]() -> const app::AttachedPart* {
        for (const app::AttachedPart& p : parts.parts()) {
            if (p.name == "hair") {
                return &p;
            }
        }
        return nullptr;
    };
    REQUIRE(hair() != nullptr);
    const std::size_t hair_under_rest = [&] {
        for (const app::PartFollowReport& r :
             parts.follow_report(body.character().current_vertices())) {
            if (r.name == "hair") {
                return r.under_skin_rest;
            }
        }
        return std::size_t{0};
    }();
    const int forehead = render::morph_index(body.morphs(), "forehead-scale-vert");
    REQUIRE(forehead >= 0);
    const float hi = body.morphs()[static_cast<std::size_t>(forehead)].hi;
    REQUIRE(hi > 0.0f);

    // Прибор: волосы не отстают и череп не проступает; контроль — волосы в ресте.
    const auto judge = [&](const char* stage) {
        const auto& now = body.character().current_vertices();
        REQUIRE(now.size() == parts.neutral().size());
        float moved = 0.0f;
        for (std::size_t i = 0; i < now.size(); ++i) {
            moved = std::max(moved, glm::length(now[i].position - parts.neutral()[i].position));
        }
        float grow = 0.0f;
        std::size_t under = 0;
        for (const app::PartFollowReport& r : parts.follow_report(now)) {
            if (r.name == "hair") {
                grow = r.gap_grow_m;
                under = r.under_skin_now;
            }
        }
        const std::size_t control = render::follow_penetrations(
            now, parts.neutral_indices(), hair()->map, hair()->rest, 0.001f);
        MESSAGE(std::string(stage) << ": body moved " << moved * 1000.0f << " mm vs neutral; hair gap growth "
                      << grow * 1000.0f << " mm, under skin " << under << " (rest "
                      << hair_under_rest << "), control (hair in rest) " << control);
        CHECK(moved > 0.005f);
        CHECK(grow <= 0.001f);
        CHECK(under <= hair_under_rest + 10);
        CHECK(control >= hair_under_rest + 40);
        // Волосы на GPU — те, что после follow: рест и «сейчас» разошлись.
        REQUIRE(hair()->now.size() == hair()->rest.size());
        float hair_moved = 0.0f;
        for (std::size_t i = 0; i < hair()->now.size(); ++i) {
            hair_moved = std::max(hair_moved, glm::length(hair()->now[i].position
                                                          - hair()->rest[i].position));
        }
        CHECK(hair_moved > 0.005f);
    };
    REQUIRE(body.set_weight(static_cast<std::size_t>(forehead), hi));
    REQUIRE(body.apply(rs, renderer));
    judge("apply");
    REQUIRE(body.settle(rs, renderer, nullptr));
    judge("settle");
    // Рост меняет масштаб тела — нейтраль частей едет тем же множителем.
    REQUIRE(body.set_height_m(app::CHARGEN_HEIGHT_MAX_M));
    REQUIRE(body.settle(rs, renderer, nullptr));
    judge("settle, height max");
    body.release(rs, renderer, nullptr);
}
