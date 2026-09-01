/*
Module: engine/app
File: engine/app/sources/AppCharGen.cpp

Responsibility:
- ЭКРАН СОЗДАНИЯ ПЕРСОНАЖА, ПРОВОДКА: та половина, которой нужны видеокарта,
  ввод и диск. Кто на экране (CharGenBody), где что нарисовано (CharGen.h) и
  какой свет на фигуре — здесь только соединение и портретный свет.

Key items:
- App::chargen_enter() / chargen_leave(): пара, и она же пара create/destroy
  меша.
- App::chargen_frame(): весь ввод кадра и отрисовка холста.
- App::chargen_commit(): «Готово» — пресет на диск и выпечка тела.
- App::chargen_screen_prop(): длинный объектив, портретный свет, фигура
  поверх холста — тем же механизмом, что и герб главного меню.

Dependencies:
- Uses: engine/app CharGen / CharGenBody / MenuEmblem (свет экрана) /
  Localization / AppDoors, engine/render RenderSystem.
- Used by: engine/app App.cpp (ветка меню).

Notes:
- ЭКРАН ЖИВЁТ В ВЕТКЕ МЕНЮ, А НЕ В МИРЕ, и это не удобство: он открывается
  ДО того, как выбрана карта, то есть мира ещё нет вовсе. Ровно та же дорога,
  которой в кадр меню попадает объёмный герб (RenderSystem::set_screen_prop):
  один меш в осях камеры поверх холста, свет свой, теней ни одной.
- ТРИ ИСТОЧНИКА, А НЕ ДВА, И ЭТО ПОРТРЕТ, А НЕ ЭКРАН. У герба два (ключ и
  заливка), потому что он плоская доска; у фигуры есть затылок, и без
  контрового света силуэт сливается с грунтом на всех ракурсах, кроме анфаса.
  Соотношение взято с принятой владельцем смотровой
  (assets/scenes/stands/viewer.scene): ключ 0.52/0.49/0.44, заливка
  0.20/0.22/0.27, контровой 0.30/0.31/0.34 — тёплый ключ, холодная тень.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
- Всякая видимая строка — ключ локализации (правило 5). Имя персонажа —
  содержимое, введённое игроком.
*/

#include "engine/app/sources/App.h"
#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/Localization.h"
#include "engine/app/sources/MenuEmblem.h"

#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/FirstPersonCamera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace dfn::app {

namespace {

/// ДЛИННЫЙ ОБЪЕКТИВ НА ВРЕМЯ ЭКРАНА. Игровые 60° — это ±30° на краю кадра, и
/// лицо, занимающее две трети высоты, разъезжается по лучам: нос вылезает,
/// уши уходят назад. Портрет снимают длинным стеклом ровно поэтому. 28° —
/// эквивалент 75-миллиметрового объектива, то есть нижняя граница
/// портретного диапазона: уже — и фигура в полный рост перестанет помещаться
/// на разумной глубине. Ставится на кадр экрана и снимается сразу после:
/// поле зрения игры — настройка мира, а не этого экрана.
constexpr float CHARGEN_FOV_DEG = 28.0f;

/// СМЕЩЕНИЯ ТРЁХ ИСТОЧНИКОВ ОТ ЦЕНТРА ФИГУРЫ, в её ВЫСОТАХ и в осях камеры
/// (x вправо, y вверх, z НА ЗРИТЕЛЯ). Ключ — вверх-влево-вперёд, заливка —
/// вниз-вправо и ближе к плоскости лица, контровой — сверху сзади.
constexpr glm::vec3 CHARGEN_KEY_OFFSET{-0.55f, 0.70f, 0.85f};
constexpr glm::vec3 CHARGEN_FILL_OFFSET{0.80f, -0.30f, 0.60f};
constexpr glm::vec3 CHARGEN_RIM_OFFSET{0.35f, 0.75f, -0.85f};
/// ЦВЕТА — С ПРИНЯТОЙ СМОТРОВОЙ, умноженные на общий множитель: там
/// источники стоят в 18 метрах от фигуры в полный рост, здесь — в долях
/// фигуры, нарисованной в двадцати сантиметрах от глаза, и абсолютная
/// яркость точечного света зависит от расстояния. Множитель один на все три,
/// поэтому СООТНОШЕНИЕ ключа, заливки и контрового — ровно то, что владелец
/// принял на смотровой.
constexpr float CHARGEN_LIGHT_GAIN = 2.35f;
constexpr glm::vec3 CHARGEN_KEY_COLOR{0.52f, 0.49f, 0.44f};
constexpr glm::vec3 CHARGEN_FILL_COLOR{0.20f, 0.22f, 0.27f};
constexpr glm::vec3 CHARGEN_RIM_COLOR{0.30f, 0.31f, 0.34f};
/// Радиусы — в высотах фигуры. Крупные: свет обязан накрывать фигуру целиком
/// и на крупном плане, где она вчетверо больше.
constexpr float CHARGEN_KEY_RADIUS_FRAC = 4.2f;
constexpr float CHARGEN_FILL_RADIUS_FRAC = 5.0f;
constexpr float CHARGEN_RIM_RADIUS_FRAC = 4.6f;

std::string_view loc(std::string_view key) {
    return localized(serialization::fnv1a64(key));
}

/// ЗАЖАТ ЛИ SHIFT. Один вопрос — один ответ: он значит «мелкий шаг» у ручки и
/// «назад» у вкладок, и два разных чтения одной клавиши разошлись бы первым же.
[[nodiscard]] bool fine_shift(const platform::IInput& input) {
    return input.is_down(platform::Key::LEFT_SHIFT)
           || input.is_down(platform::Key::RIGHT_SHIFT);
}

} // namespace

// --- ВХОД И ВЫХОД -----------------------------------------------------------

void App::chargen_enter() {
    if (chargen_open_ || renderer_ == nullptr) {
        return;
    }
    ensure_body_rig();
    if (!chargen_body_.load(*renderer_, body_rig_,
                            std::filesystem::path(CHARGEN_SOURCE_BODY))) {
        // ГРОМКО И БЕЗ ЭКРАНА. Экран создания персонажа без персонажа — это
        // столбец ползунков, которые ничего не двигают; лучше остаться в
        // главном меню, сказав почему.
        std::fprintf(stderr, "[создание] экран не открыт: тело не загрузилось\n");
        return;
    }
    chargen_open_ = true;
    chargen_cursor_ = glm::vec2{-1.0f, -1.0f};
    chargen_orbiting_ = false;

    // РУЧКИ СОБИРАЮТСЯ ИЗ ТЕЛА, А НЕ ИЗ СПИСКА ЗДЕСЬ. Полосы — из секции MORF,
    // где они лежат измеренными приёмкой шага 1; новая цель в файле сама
    // становится строкой экрана.
    std::vector<CharGenRow> rows;
    rows.reserve(chargen_body_.morphs().size() + 1);
    for (const render::MorphTarget& t : chargen_body_.morphs()) {
        CharGenRow row;
        row.kind = CharGenRowKind::Slider;
        row.name = t.name;   // подпись ищется как morph.slider.<имя>
        row.lo = t.lo;
        row.hi = t.hi;
        rows.push_back(std::move(row));
    }
    // РОСТ ПОСЛЕДНЕЙ СТРОКОЙ И В МЕТРАХ, а не весом: это единственная ручка,
    // у которой есть человеческая единица, и прятать её за «−1..+1» значило
    // бы заставить игрока угадывать, какого он роста.
    CharGenRow height;
    height.kind = CharGenRowKind::Slider;
    height.name = CHARGEN_HEIGHT_KEY;
    height.lo = CHARGEN_HEIGHT_MIN_M;
    height.hi = CHARGEN_HEIGHT_MAX_M;
    height.value = CHARGEN_BODY_HEIGHT_M;
    height.metres = true;
    rows.push_back(std::move(height));
    // ОПИСАНИЕ РЕШАЕТ, В КАКИХ КАТЕГОРИЯХ ЭТО ЛЕЖИТ, а не этот файл: сюда
    // приезжают строки телосложения, а вкладки и всё остальное — из
    // chargen_describe(). Дизайн-сессия добавит категорию там, не тронув
    // проводку.
    chargen_.set_categories(chargen_describe(std::move(rows)));
    chargen_.set_selection(0);
    chargen_.view() = CharGenView{};
    chargen_.set_status({});

    // ПОВТОРНЫЙ ВХОД — ИЗ ПРЕСЕТА. Персонаж, которого уже создали, обязан
    // открыться таким, каким его сделали: экран, который каждый раз начинает
    // с нейтрали, — это экран, стирающий работу без предупреждения.
    CharGenPreset preset;
    bool from_preset = false;
    if (read_chargen_preset(std::filesystem::path(CHARGEN_PRESET_PATH), preset)) {
        chargen_body_.apply_preset(preset);
        chargen_.set_name(preset.name);
        for (std::size_t i = 0; i < chargen_body_.morphs().size(); ++i) {
            (void)chargen_.set_value(chargen_body_.morphs()[i].name,
                                     chargen_body_.weights().weights[i]);
        }
        (void)chargen_.set_value(CHARGEN_HEIGHT_KEY, chargen_body_.height_m());
        from_preset = true;
    }

    // ДОЗА DFN_MORPH — ТА ЖЕ, ЧТО У ТЕЛА В МИРЕ (шаг 1), и намеренно та же:
    // это одни и те же ползунки одного и того же тела, и второе имя для них
    // было бы вторым определением (правило 32). Здесь она нужна, чтобы кадр
    // приёмки «крайние положения» снимался без единого нажатия.
    if (const char* dose = door_value("DFN_MORPH"); dose != nullptr && *dose != '\0') {
        std::string text(dose);
        std::size_t at = 0;
        while (at < text.size()) {
            const std::size_t comma = text.find(',', at);
            const std::string item =
                text.substr(at, comma == std::string::npos ? std::string::npos
                                                           : comma - at);
            at = comma == std::string::npos ? text.size() : comma + 1;
            const std::size_t eq = item.find('=');
            if (eq == std::string::npos) {
                std::fprintf(stderr, "[создание] DFN_MORPH: \"%s\" не имя=число\n",
                             item.c_str());
                continue;
            }
            const std::string name = item.substr(0, eq);
            const float value = std::strtof(item.c_str() + eq + 1, nullptr);
            if (chargen_.find(name) == nullptr) {
                std::fprintf(stderr,
                             "[создание] DFN_MORPH: ползунка \"%s\" у этого тела "
                             "нет\n", name.c_str());
                continue;
            }
            (void)chargen_.set_value(name, value);
            chargen_push_to_body(name);
        }
        (void)chargen_body_.apply(*renderer_);
    }

    // ДОЗА DFN_CHARGEN_VIEW=<рыскание>,<тангаж>,<приближение> — кадрирование
    // без мыши. Три состояния экрана снимаются одной сборкой и одной рукой
    // (правило 47), а не «поверни и щёлкни».
    if (const char* v = door_value("DFN_CHARGEN_VIEW"); v != nullptr && *v != '\0') {
        char* end = nullptr;
        CharGenView& view = chargen_.view();
        view.yaw = std::strtof(v, &end);
        if (end != nullptr && *end == ',') {
            view.pitch = std::strtof(end + 1, &end);
        }
        if (end != nullptr && *end == ',') {
            view.zoom = std::strtof(end + 1, &end);
        }
        chargen_orbit(view, 0.0f, 0.0f, 0.0f); // зажимает всё три границами
        chargen_zoom(view, 0.0f);
    }

    // ЖУРНАЛ НАЗЫВАЕТ ФАЙЛ И ЕГО ХЭШ, а не только счётчики. Владелец 01.09
    // спросил, почему на экране создания не тот человек, которого он видел в
    // смотровой, и ответить на это счётчиком треугольников нельзя: разошлись
    // бы ФАЙЛЫ — счётчик бы и не дрогнул. Файл, хэш и число целей MORF в одной
    // строке — это ровно тот набор, которым «тело экрана = тело мира»
    // проверяется без второго прогона (CharGenBody.h, CHARGEN_SOURCE_BODY).
    std::fprintf(stderr,
                 "[создание] экран открыт: тело %s (хэш %016llx), %zu "
                 "треугольников, %zu целей MORF, рост %.3f м%s\n",
                 CHARGEN_SOURCE_BODY,
                 static_cast<unsigned long long>(
                     chargen_body_hash(std::filesystem::path(CHARGEN_SOURCE_BODY))),
                 chargen_body_.triangles(), chargen_body_.morphs().size(),
                 static_cast<double>(chargen_body_.height_m()),
                 from_preset ? ", поднят пресет" : "");

    // ДОЗА DFN_CHARGEN_DONE=1: нажать «Готово» и закрыть игру. Без неё весь
    // путь «ползунки → пресет → выпечка → тело в мире» проходим только рукой,
    // то есть НЕ СУДИМ: судья пропорций — отдельный исполняемый файл, и ему
    // нужен .dfo, который кто-то должен испечь без человека.
    if (const char* d = door_value("DFN_CHARGEN_DONE"); d != nullptr && d[0] == '1') {
        chargen_commit();
        if (window_ != nullptr) {
            window_->request_close();
        }
    }
}

void App::chargen_leave() {
    if (!chargen_open_) {
        return;
    }
    if (renderer_ != nullptr) {
        chargen_body_.release(*renderer_);
        render_system_.set_screen_prop(render::RenderSystem::ScreenProp{});
    }
    chargen_open_ = false;
    chargen_orbiting_ = false;
    std::fprintf(stderr,
                 "[создание] экран закрыт: заливок меша %u, уничтожений %u "
                 "(живых %d)\n",
                 chargen_body_.uploads(), chargen_body_.drops(),
                 static_cast<int>(chargen_body_.uploads())
                     - static_cast<int>(chargen_body_.drops()));
}

// --- КАДР -------------------------------------------------------------------

bool App::chargen_frame(int hud_w, int hud_h, int mx, int my, bool pointer_moved) {
    if (!chargen_open_ || input_ == nullptr) {
        return false;
    }
    bool body_dirty = false;

    // БУКВЫ ИДУТ В ИМЯ, ПОКА КУРСОР НА СТРОКЕ ИМЕНИ, и только тогда. Тот же
    // уговор, что у чата: пока набирают, клавиша — это буква, а не команда.
    const bool typing = chargen_.text_focused();
    if (typing) {
        chargen_.feed_text(input_->text_input());
        if (input_->was_pressed(platform::Key::BACKSPACE)) {
            chargen_.backspace();
        }
    }

    // TAB ЛИСТАЕТ ВКЛАДКИ. Не стрелки: стрелки вбок уже заняты значением
    // строки, и отдать им ещё и вкладку значило бы, что край полосы ползунка
    // молча уводит на другую категорию.
    if (input_->was_pressed(platform::Key::TAB)) {
        chargen_.cycle_category(fine_shift(*input_) ? -1 : +1);
    }
    if (input_->was_pressed(platform::Key::UP)) {
        chargen_.move(-1);
    }
    if (input_->was_pressed(platform::Key::DOWN)) {
        chargen_.move(1);
    }
    const bool fine = fine_shift(*input_);
    if (input_->was_pressed(platform::Key::LEFT)) {
        body_dirty = chargen_apply_row(chargen_.adjust(-1, fine)) || body_dirty;
    }
    if (input_->was_pressed(platform::Key::RIGHT)) {
        body_dirty = chargen_apply_row(chargen_.adjust(+1, fine)) || body_dirty;
    }

    // МЫШЬ. Наведение двигает выбор только при настоящем движении — тот же
    // довод, что в меню: рука, лежащая на мыши, иначе утягивала бы выбор
    // из-под стрелок каждый кадр.
    const glm::vec2 cursor = input_->mouse_position();
    const bool moved = pointer_moved
                       || std::abs(cursor.x - chargen_cursor_.x) > 0.5f
                       || std::abs(cursor.y - chargen_cursor_.y) > 0.5f;
    if (input_->was_pressed(platform::MouseButton::LEFT)) {
        const std::size_t grabbed = chargen_.press(hud_w, hud_h, mx, my);
        if (grabbed < chargen_.row_count()) {
            body_dirty = chargen_apply_row(grabbed) || body_dirty;
        } else if (chargen_.over_figure(hud_w, hud_h, mx)) {
            chargen_orbiting_ = true;
        }
    }
    if (input_->is_down(platform::MouseButton::LEFT)) {
        if (chargen_.dragging()) {
            if (chargen_.drag(hud_w, hud_h, mx)) {
                body_dirty = chargen_apply_row(chargen_.dragged_row()) || body_dirty;
            }
        } else if (chargen_orbiting_ && moved && chargen_cursor_.x >= 0.0f) {
            chargen_orbit(chargen_.view(), cursor.x - chargen_cursor_.x,
                          cursor.y - chargen_cursor_.y,
                          static_cast<float>(config::MOUSE_SENSITIVITY));
        }
    } else {
        chargen_.release();
        chargen_orbiting_ = false;
        if (moved) {
            const std::size_t row = chargen_.row_at(hud_w, hud_h, mx, my);
            if (row < chargen_.row_count()) {
                chargen_.set_selection(row);
            }
        }
    }
    chargen_cursor_ = cursor;
    // КОЛЕСО — ПРИБЛИЖЕНИЕ, и единственный потребитель колеса на этом экране.
    chargen_zoom(chargen_.view(), input_->scroll_delta().y);

    CharGenAction action = CharGenAction::None;
    if (!typing && input_->was_pressed(platform::Key::ENTER)) {
        action = chargen_.activate();
    } else if (typing && input_->was_pressed(platform::Key::ENTER)) {
        // ENTER В ПОЛЕ ИМЕНИ ЗАКАНЧИВАЕТ ВВОД, а не жмёт «Готово»: клавиша,
        // которая одновременно завершает строку и печёт тело, однажды
        // испечёт тело вместо точки в имени.
        chargen_.move(1);
    } else if (input_->was_pressed(platform::Key::ESCAPE)) {
        action = CharGenAction::Back;
    } else if (input_->was_pressed(platform::MouseButton::LEFT) && !chargen_.dragging()
               && !chargen_orbiting_) {
        const std::size_t row = chargen_.row_at(hud_w, hud_h, mx, my);
        if (row < chargen_.row_count()
            && chargen_.row_kind(row) == CharGenRowKind::Button) {
            chargen_.set_selection(row);
            action = chargen_.activate();
        }
    }

    switch (action) {
    case CharGenAction::Reset:
        chargen_body_.reset();
        body_dirty = true;
        chargen_.set_status(std::string(loc("chargen.status.reset")));
        break;
    case CharGenAction::Done:
        chargen_commit();
        break;
    case CharGenAction::Back:
        chargen_leave();
        return false;
    case CharGenAction::None:
        break;
    }

    if (body_dirty) {
        (void)chargen_body_.apply(*renderer_);
    }
    chargen_.draw(render_system_.hud());
    render_system_.set_hud_visible(true);
    return true;
}

bool App::chargen_apply_row(std::size_t row_index) {
    const CharGenRow* row = chargen_.row_at_index(row_index);
    if (row == nullptr || row->kind != CharGenRowKind::Slider) {
        return false;
    }
    return chargen_push_to_body(row->name);
}

bool App::chargen_push_to_body(const std::string& name) {
    const CharGenRow* row = chargen_.find(name);
    if (row == nullptr) {
        return false;
    }
    if (name == CHARGEN_HEIGHT_KEY) {
        // РОСТ МЕША НЕ ТРОГАЕТ: он живёт в матрице кадра, и перепекать восемь
        // тысяч вершин ради множителя было бы работой впустую.
        (void)chargen_body_.set_height_m(row->value);
        return false;
    }
    // ПО ИМЕНИ, А НЕ ПО НОМЕРУ. Номер строки экрана и номер цели в файле
    // совпадают ровно до первой категории, вставленной перед телосложением, —
    // то есть до первой правки описания.
    return chargen_body_.set_weight(name, row->value);
}

// --- «ГОТОВО» ---------------------------------------------------------------

void App::chargen_commit() {
    const std::filesystem::path preset_path(CHARGEN_PRESET_PATH);
    const std::filesystem::path baked_path(CHARGEN_BAKED_PATH);
    const bool wrote = write_chargen_preset(preset_path,
                                            chargen_body_.preset(chargen_.name()));
    const bool baked = chargen_body_.bake(baked_path);
    std::string status(loc(wrote && baked ? "chargen.status.done"
                                          : "chargen.status.failed"));
    chargen_.set_status(status);
    std::fprintf(stderr,
                 "[создание] «Готово»: пресет %s (%s), выпечка %s (%s), рост "
                 "%.3f м, масштаб %.4f\n",
                 preset_path.string().c_str(), wrote ? "записан" : "ОТКАЗ",
                 baked_path.string().c_str(), baked ? "записана" : "ОТКАЗ",
                 static_cast<double>(chargen_body_.height_m()),
                 static_cast<double>(chargen_body_.height_scale()));
}

// --- КАДР: СВЕТ И ФИГУРА ----------------------------------------------------

void App::chargen_screen_prop() {
    if (!chargen_open_ || !chargen_body_.ready()) {
        return;
    }
    camera_.set_projection(glm::radians(CHARGEN_FOV_DEG), camera_.aspect_ratio(),
                           camera_.near_plane(), camera_.far_plane());

    // ОКРУЖЕНИЕ БЕРЁТСЯ У ЭКРАНА МЕНЮ — солнце погашено и опущено ниже порога
    // построения теней, луна убита, небо ровное и холодное. Это ОДНО решение
    // («кадр экрана не строит карту теней»), и второй его копии здесь быть не
    // должно; своими остаются только источники, потому что у портрета их три
    // и стоят они вокруг фигуры, а не вокруг доски герба.
    light_menu_screen(render_system_.environment(), camera_, menu_lights_);

    const glm::vec3 fwd = camera_.forward(0.0f);
    const glm::vec3 right = camera_.right(0.0f);
    const glm::vec3 up = glm::normalize(glm::cross(right, fwd));
    const glm::vec3 eye = camera_.interpolated_pose(0.0f).position;

    const glm::mat4 in_camera =
        chargen_in_camera(camera_, chargen_body_.lo(), chargen_body_.hi(),
                          chargen_body_.height_scale(), chargen_.view());
    // ЯКОРЬ СВЕТА — ТА ЖЕ ТОЧКА, ЧТО СТОИТ В СЕРЕДИНЕ КАДРА, и та же матрица,
    // которой фигуру рисуют. Свет, посчитанный по второй арифметике, разошёлся
    // бы с фигурой на первом же изменении кадрирования, и разошёлся бы молча.
    const glm::vec3 mid =
        chargen_pivot(chargen_body_.lo(), chargen_body_.hi(), chargen_.view().zoom);
    const glm::vec3 cam_mid = glm::vec3(in_camera * glm::vec4(mid, 1.0f));
    const glm::vec3 cam_top =
        glm::vec3(in_camera * glm::vec4(mid.x, chargen_body_.hi().y, mid.z, 1.0f));
    const glm::vec3 cam_bot =
        glm::vec3(in_camera * glm::vec4(mid.x, chargen_body_.lo().y, mid.z, 1.0f));
    const float figure_h = std::max(1e-3f, glm::length(cam_top - cam_bot));
    const glm::vec3 centre = eye + right * cam_mid.x + up * cam_mid.y
                             + fwd * (-cam_mid.z);

    const auto place = [&](const glm::vec3& offset, const glm::vec3& colour,
                           float radius_frac) {
        render::RenderSystem::ExtraLight light;
        light.position = centre + right * (offset.x * figure_h)
                         + up * (offset.y * figure_h)
                         + (-fwd) * (offset.z * figure_h);
        light.color = colour * CHARGEN_LIGHT_GAIN;
        light.radius_m = radius_frac * figure_h;
        light.casts_shadow = false;
        return light;
    };
    menu_lights_.clear();
    menu_lights_.push_back(place(CHARGEN_KEY_OFFSET, CHARGEN_KEY_COLOR,
                                 CHARGEN_KEY_RADIUS_FRAC));
    menu_lights_.push_back(place(CHARGEN_FILL_OFFSET, CHARGEN_FILL_COLOR,
                                 CHARGEN_FILL_RADIUS_FRAC));
    menu_lights_.push_back(place(CHARGEN_RIM_OFFSET, CHARGEN_RIM_COLOR,
                                 CHARGEN_RIM_RADIUS_FRAC));
    render_system_.set_transient_lights(menu_lights_);

    render::RenderSystem::ScreenProp prop;
    prop.mesh = chargen_body_.mesh();
    prop.program = chargen_body_.program();
    prop.in_camera = in_camera;
    render_system_.set_screen_prop(prop);
}

} // namespace dfn::app
