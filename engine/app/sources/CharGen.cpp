/*
Module: engine/app
File: engine/app/sources/CharGen.cpp

Responsibility:
- Описание экрана, раскладка, ввод и отрисовка категорий и строк; арифметика
  портретного облёта. Договор и все доли — в CharGen.h.

Dependencies:
- Uses: engine/app UiFont / UiSlider / Localization, engine/render PixelCanvas /
  FirstPersonCamera / RenderSystem (только overlay_depth_m — глубина холста
  экрана названа там один раз).
- Used by: engine/app AppCharGen.cpp, App.cpp, tests/app/CharGenTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
- ОТРИСОВКА СПРАШИВАЕТ ВИД СТРОКИ, А НЕ ЕЁ СМЫСЛ. Ветка «если это живот» здесь
  недопустима: ровно от неё этот каркас и заведён.
*/

#include "engine/app/sources/CharGen.h"

#include "engine/app/sources/Localization.h"
#include "engine/app/sources/UiFont.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/FirstPersonCamera.h"
#include "engine/render/sources/RenderSystem.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dfn::app {

namespace {

// ПАЛИТРА ЭКРАНА — та же, что у страниц меню, и это не копипаста, а
// требование связности: экран создания персонажа открывается ИЗ главного
// меню и обязан читаться как его продолжение, а не как чужое окно. Числа
// повторены здесь, потому что палитра меню живёт в безымянном пространстве
// Menu.cpp (файл лида) и никем наружу не объявлена; вынести её в общий
// заголовок — правка ЕГО файла, а не этого.
constexpr render::Color BACKGROUND{18, 20, 26};
constexpr render::Color TITLE{232, 228, 214};
constexpr render::Color ITEM{176, 172, 160};
constexpr render::Color ITEM_SELECTED{244, 226, 160};
constexpr render::Color BLURB{120, 118, 112};
constexpr render::Color RULE_LINE{54, 56, 64};

std::string_view loc(std::string_view key) {
    return localized(serialization::fnv1a64(key));
}

/// ПОДПИСЬ СТРОКИ. Ключ либо назван описанием, либо строится из ИМЕНИ, взятого
/// из файла тела: новая цель в секции MORF получает строку в локализации, а не
/// строку кода.
std::string_view row_label(const CharGenRow& row) {
    if (!row.label_key.empty()) {
        return loc(row.label_key);
    }
    std::string key = "morph.slider.";
    key += row.name;
    return loc(key);
}

[[nodiscard]] std::string two_decimals(float v) {
    char buf[32] = {};
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
    return buf;
}

/// UTF-8 из одного кодпоинта. Своё, а не <codecvt>: тот объявлен устаревшим,
/// а нужного здесь — четыре строки.
void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

/// СКОЛЬКО В СТРОКЕ ЗНАКОВ, А НЕ БАЙТОВ. Имя вводится и кириллицей, где знак
/// это два байта: предел, посчитанный в байтах, обрезал бы русское имя вдвое
/// раньше латинского.
[[nodiscard]] std::size_t utf8_length(const std::string& s) {
    std::size_t n = 0;
    for (const char c : s) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++n;
        }
    }
    return n;
}

/// Убирает ПОСЛЕДНИЙ ЗНАК (а не байт).
void pop_utf8(std::string& s) {
    while (!s.empty()) {
        const auto c = static_cast<unsigned char>(s.back());
        s.pop_back();
        if ((c & 0xC0) != 0x80) {
            return;
        }
    }
}

/// Значение строки словами — то, что рисуется справа от подписи.
[[nodiscard]] std::string row_value_text(const CharGenRow& row) {
    switch (row.kind) {
    case CharGenRowKind::Slider: {
        std::string v = two_decimals(row.value);
        if (row.metres) {
            v += " ";
            v += std::string(loc("chargen.unit.metre"));
        }
        return v;
    }
    case CharGenRowKind::Option:
        return row.choice < row.choices.size()
                   ? std::string(loc(row.choices[row.choice]))
                   : std::string{};
    case CharGenRowKind::Text:
        return row.text.empty() ? std::string(loc("chargen.name.empty")) : row.text;
    case CharGenRowKind::Button:
        break;
    }
    return {};
}

const std::vector<CharGenRow>& empty_rows() {
    static const std::vector<CharGenRow> none;
    return none;
}

} // namespace

// --- ОПИСАНИЕ ---------------------------------------------------------------

std::vector<CharGenRow> chargen_verbs() {
    std::vector<CharGenRow> verbs;
    CharGenRow r;
    r.kind = CharGenRowKind::Button;
    r.name = "reset";
    r.label_key = "chargen.reset";
    r.action = CharGenAction::Reset;
    verbs.push_back(r);
    r.name = "done";
    r.label_key = "chargen.done";
    r.action = CharGenAction::Done;
    verbs.push_back(r);
    r.name = "back";
    r.label_key = "chargen.back";
    r.action = CharGenAction::Back;
    verbs.push_back(r);
    return verbs;
}

std::vector<CharGenCategory> chargen_describe(std::vector<CharGenRow> body_rows) {
    std::vector<CharGenCategory> out;
    // ПЕРВАЯ ВКЛАДКА — ТЕЛОСЛОЖЕНИЕ, и её строки приходят ИЗ ФАЙЛА ТЕЛА.
    out.push_back(CharGenCategory{"chargen.tab.body", std::move(body_rows)});
    // ВТОРАЯ — ИМЯ. Отдельной вкладкой, а не строкой под ползунками, потому
    // что дизайн-сессия называет её «Имя и происхождение»: происхождение —
    // это выбор из списка, и он приедет СЮДА строкой описания, не сдвинув ни
    // одной строки телосложения.
    CharGenCategory identity;
    identity.key = "chargen.tab.identity";
    CharGenRow name;
    name.kind = CharGenRowKind::Text;
    name.name = CHARGEN_NAME_ROW;
    name.label_key = "chargen.name";
    name.text_max = CHARGEN_NAME_MAX_CHARS;
    identity.rows.push_back(std::move(name));
    out.push_back(std::move(identity));
    return out;
}

// --- КАМЕРА -----------------------------------------------------------------

void chargen_orbit(CharGenView& view, float dx_px, float dy_px, float sensitivity) {
    view.yaw = std::clamp(view.yaw + dx_px * sensitivity, -CHARGEN_YAW_LIMIT,
                          CHARGEN_YAW_LIMIT);
    view.pitch = std::clamp(view.pitch + dy_px * sensitivity, -CHARGEN_PITCH_LIMIT,
                            CHARGEN_PITCH_LIMIT);
}

void chargen_zoom(CharGenView& view, float notches) {
    view.zoom = std::clamp(view.zoom + notches * CHARGEN_ZOOM_STEP, 0.0f, 1.0f);
}

glm::vec3 chargen_pivot(const glm::vec3& lo, const glm::vec3& hi, float zoom) {
    const float t = std::clamp(zoom, 0.0f, 1.0f);
    const float centre = (1.0f - t) * 0.5f + t * CHARGEN_FACE_CENTER_FRAC;
    return glm::vec3{0.5f * (lo.x + hi.x), lo.y + centre * (hi.y - lo.y),
                     0.5f * (lo.z + hi.z)};
}

glm::mat4 chargen_in_camera(const render::FirstPersonCamera& camera,
                            const glm::vec3& lo, const glm::vec3& hi,
                            float height_scale, const CharGenView& view) {
    const float depth = render::RenderSystem::overlay_depth_m(camera)
                        * CHARGEN_DEPTH_FRAC;
    const float half_h = depth * std::tan(camera.fov_y() * 0.5f);
    const float half_w = half_h * camera.aspect_ratio();

    // ГАБАРИТ БЕРЁТСЯ БЕЗ МАСШТАБА РОСТА — И ЭТО ВЕСЬ СМЫСЛ ПОЛЗУНКА РОСТА.
    // Первая версия делила на габарит УЖЕ УМНОЖЕННЫЙ на масштаб, множитель
    // сокращался, и фигура ростом 1.84 занимала в кадре ровно столько же, что
    // и фигура ростом 1.66: ползунок работал, а увидеть его было нельзя.
    // Кадрирование считается по КАНОНУ, рост множит поверх него.
    const float body_h = std::max(1e-4f, hi.y - lo.y);
    const float t = std::clamp(view.zoom, 0.0f, 1.0f);
    // ПРИБЛИЖЕНИЕ — ЭТО ПЕРЕХОД МЕЖДУ ДВУМЯ КАДРИРОВКАМИ, а не деление
    // расстояния. У «всей фигуры» и у «лица» разные не только размеры, но и
    // ЦЕНТРЫ: приближение с общим центром уводит голову за верх кадра ровно в
    // тот момент, когда её и хотели рассмотреть.
    const float span = (1.0f - t) * 1.0f + t * CHARGEN_FACE_SPAN_FRAC;
    const float fill = (1.0f - t) * CHARGEN_BODY_FILL + t * CHARGEN_FACE_FILL;

    const float scale = height_scale * (fill * 2.0f * half_h) / (span * body_h);
    const float x_frac = (1.0f - t) * CHARGEN_FIGURE_X_FRAC + t * CHARGEN_FACE_X_FRAC;
    const float x = (2.0f * x_frac - 1.0f) * half_w;
    const glm::vec3 pivot = chargen_pivot(lo, hi, view.zoom);

    return glm::translate(glm::mat4(1.0f), {x, 0.0f, -depth})
           * glm::scale(glm::mat4(1.0f), glm::vec3(scale))
           * glm::rotate(glm::mat4(1.0f), view.pitch, {1.0f, 0.0f, 0.0f})
           * glm::rotate(glm::mat4(1.0f), view.yaw + CHARGEN_MODEL_FACE_YAW,
                         {0.0f, 1.0f, 0.0f})
           * glm::translate(glm::mat4(1.0f), -pivot);
}

// --- РАСКЛАДКА --------------------------------------------------------------

CharGenLayout chargen_layout(int canvas_w, int canvas_h, std::size_t row_count) {
    CharGenLayout L;
    L.title_px = ui_px(canvas_h, UiText::Title);
    L.item_px = ui_px(canvas_h, UiText::Item);
    L.hint_px = ui_px(canvas_h, UiText::Caption);
    // БЕЗ ИСПЕЧЁННОГО ШРИФТА КЕГЛЬ НУЛЕВОЙ, и раскладка обязана всё равно
    // отдать непротиворечивые числа: тест раскладки не грузит атлас, а
    // деление на высоту прописной ноль дало бы шаг ноль и все строки в одной.
    const auto place = [&](int item_px) {
        const int cap = std::max(1, ui_cap_height(item_px));
        L.title_y = canvas_h / 14;
        L.tabs_y = L.title_y + std::max(1, ui_cap_height(L.title_px)) + cap;
        L.first_y = L.tabs_y + cap * 2;
        L.step = cap + cap / 2;
    };
    place(L.item_px);

    // ВСЕ СТРОКИ ОБЯЗАНЫ ПОМЕСТИТЬСЯ — тот же цикл, что у страницы настроек,
    // и по той же причине: строка за нижним краем не рисуется И НЕ
    // НАЖИМАЕТСЯ, а раскладка — единственная карта, которая есть у мыши.
    const int bottom = canvas_h - canvas_h / 8; // под списком стоит подсказка
    int ladder_h = canvas_h;
    while (L.item_px > 1
           && L.first_y + static_cast<int>(row_count) * L.step > bottom) {
        ladder_h = ladder_h * 4 / 5;
        const int smaller = ui_px(ladder_h, UiText::Item);
        if (smaller >= L.item_px) {
            break; // лестница кончилась: мельче испечённого не бывает
        }
        L.item_px = smaller;
        place(L.item_px);
    }
    const int cap = std::max(1, ui_cap_height(L.item_px));
    if (static_cast<int>(row_count) * L.step > bottom - L.first_y) {
        // Лестница кончилась, а строки не влезли: отдаём воздух междустрочья,
        // но не даём строкам соприкоснуться — два слипшихся текста читаются
        // как поломка обоих.
        L.step = std::max(cap, (bottom - L.first_y)
                                   / std::max<int>(1, static_cast<int>(row_count)));
    }

    // КОЛОНКА СЛЕВА. Три поля: подпись, жёлоб, значение. Ширина панели —
    // CHARGEN_FIGURE_X_FRAC минус запас, чтобы фигура и текст не встретились
    // ни на одной сетке.
    L.label_x = std::max(4, canvas_w / 24);
    const int panel_w = static_cast<int>(static_cast<float>(canvas_w) * 0.40f);
    L.panel_right = L.label_x + panel_w;
    L.value_right = L.panel_right;
    // ТРИ ПОЛЯ И ВОЗДУХ МЕЖДУ НИМИ. Значение справа набирается в самом широком
    // случае («1.84 м», «каштановые»): колонка, посчитанная по «0.00»,
    // подсовывала бы его под правый конец полосы — а полоса и число тогда
    // читаются как одна сломанная строка.
    const int label_w = panel_w * 38 / 100;
    const int value_w = panel_w * 22 / 100;
    L.track_x = L.label_x + label_w;
    L.track_w = std::max(8, panel_w - label_w - value_w);
    L.handle_w = std::max(2, static_cast<int>(std::lround(SLIDER_HANDLE_W_FRAC
                                                          * static_cast<double>(cap))));
    L.handle_h = std::max(3, static_cast<int>(std::lround(SLIDER_HANDLE_H_FRAC
                                                          * static_cast<double>(cap))));
    return L;
}

std::vector<CharGenTabBox> chargen_tab_boxes(
    const CharGenLayout& layout, const std::vector<CharGenCategory>& categories) {
    std::vector<CharGenTabBox> out;
    out.reserve(categories.size());
    const int gap = std::max(2, layout.item_px);
    int x = layout.label_x;
    for (const CharGenCategory& c : categories) {
        const int w = ui_text_width(loc(c.key), layout.item_px);
        out.push_back(CharGenTabBox{x, w});
        x += w + gap;
    }
    return out;
}

// --- ЭКРАН ------------------------------------------------------------------

void CharGenScreen::set_categories(std::vector<CharGenCategory> categories) {
    categories_ = std::move(categories);
    category_ = 0;
    selection_ = 0;
    drag_row_ = row_count();
}

const std::vector<CharGenRow>& CharGenScreen::rows() const {
    if (category_ >= categories_.size()) {
        return empty_rows();
    }
    return categories_[category_].rows;
}

void CharGenScreen::set_category(std::size_t index) {
    if (index >= categories_.size() || index == category_) {
        return;
    }
    category_ = index;
    // ВЫБОР ВОЗВРАЩАЕТСЯ В НАЧАЛО ВКЛАДКИ, а не остаётся на прежнем номере:
    // у вкладок разная длина, и номер, переживший переключение, означал бы на
    // соседней вкладке другую строку — то же «нажал Вниз три раза», от
    // которого заведены имена строк меню.
    selection_ = 0;
    drag_row_ = row_count();
}

void CharGenScreen::cycle_category(int delta) {
    if (categories_.empty()) {
        return;
    }
    const int n = static_cast<int>(categories_.size());
    int next = (static_cast<int>(category_) + delta) % n;
    if (next < 0) {
        next += n;
    }
    set_category(static_cast<std::size_t>(next));
}

std::size_t CharGenScreen::row_count() const {
    return rows().size() + verbs_.size();
}

const CharGenRow* CharGenScreen::row_at_index(std::size_t row) const {
    const std::vector<CharGenRow>& r = rows();
    if (row < r.size()) {
        return &r[row];
    }
    const std::size_t v = row - r.size();
    return v < verbs_.size() ? &verbs_[v] : nullptr;
}

CharGenRow* CharGenScreen::mutable_row(std::size_t row) {
    if (category_ >= categories_.size()) {
        return nullptr;
    }
    std::vector<CharGenRow>& r = categories_[category_].rows;
    if (row < r.size()) {
        return &r[row];
    }
    const std::size_t v = row - r.size();
    return v < verbs_.size() ? &verbs_[v] : nullptr;
}

CharGenRow* CharGenScreen::mutable_row(std::string_view name) {
    for (CharGenCategory& c : categories_) {
        for (CharGenRow& r : c.rows) {
            if (r.name == name) {
                return &r;
            }
        }
    }
    return nullptr;
}

const CharGenRow* CharGenScreen::find(std::string_view name) const {
    for (const CharGenCategory& c : categories_) {
        for (const CharGenRow& r : c.rows) {
            if (r.name == name) {
                return &r;
            }
        }
    }
    return nullptr;
}

CharGenRowKind CharGenScreen::row_kind(std::size_t row) const {
    const CharGenRow* r = row_at_index(row);
    return r != nullptr ? r->kind : CharGenRowKind::Button;
}

void CharGenScreen::set_selection(std::size_t row) {
    if (row < row_count()) {
        selection_ = row;
    }
}

void CharGenScreen::move(int delta) {
    const int n = static_cast<int>(row_count());
    if (n == 0) {
        return;
    }
    int next = (static_cast<int>(selection_) + delta) % n;
    if (next < 0) {
        next += n;
    }
    selection_ = static_cast<std::size_t>(next);
}

std::size_t CharGenScreen::adjust(int delta, bool fine) {
    CharGenRow* row = mutable_row(selection_);
    if (row == nullptr) {
        return row_count();
    }
    switch (row->kind) {
    case CharGenRowKind::Slider: {
        const float step = slider_key_step(row->lo, row->hi, fine);
        return set_value(selection_, row->value + static_cast<float>(delta) * step)
                   ? selection_
                   : row_count();
    }
    case CharGenRowKind::Option: {
        if (row->choices.size() < 2) {
            return row_count();
        }
        const int n = static_cast<int>(row->choices.size());
        int next = (static_cast<int>(row->choice) + delta) % n;
        if (next < 0) {
            next += n;
        }
        row->choice = static_cast<std::size_t>(next);
        return selection_;
    }
    case CharGenRowKind::Text:
    case CharGenRowKind::Button:
        break;
    }
    return row_count();
}

bool CharGenScreen::set_value(std::size_t row, float value) {
    CharGenRow* r = mutable_row(row);
    if (r == nullptr || r->kind != CharGenRowKind::Slider) {
        return false;
    }
    const float clamped = std::clamp(value, r->lo, r->hi);
    if (clamped == r->value) {
        return false;
    }
    r->value = clamped;
    return true;
}

bool CharGenScreen::set_value(std::string_view name, float value) {
    CharGenRow* r = mutable_row(name);
    if (r == nullptr || r->kind != CharGenRowKind::Slider) {
        return false;
    }
    const float clamped = std::clamp(value, r->lo, r->hi);
    if (clamped == r->value) {
        return false;
    }
    r->value = clamped;
    return true;
}

void CharGenScreen::reset_rows() {
    for (CharGenCategory& c : categories_) {
        for (CharGenRow& r : c.rows) {
            switch (r.kind) {
            case CharGenRowKind::Slider:
                // НОЛЬ, А НЕ СЕРЕДИНА ПОЛОСЫ. Ноль — это НЕЙТРАЛЬ цели (тело,
                // как его испекли), и у двусторонних ручек он ровно посередине
                // по смыслу, а не по арифметике полосы: `weight` измерена в
                // [-1, 0.65], и её середина −0.175 была бы «слегка похудевшим»
                // телом, объявленным исходным. У РОСТА нуля в полосе нет
                // вовсе, и его нейтраль названа отдельно — канон.
                r.value = (r.name == CHARGEN_HEIGHT_KEY)
                              ? std::clamp(CHARGEN_BODY_HEIGHT_M, r.lo, r.hi)
                              : std::clamp(0.0f, r.lo, r.hi);
                break;
            case CharGenRowKind::Option:
                r.choice = 0;
                break;
            case CharGenRowKind::Text:
            case CharGenRowKind::Button:
                break; // имя — не облик
            }
        }
    }
}

bool CharGenScreen::text_focused() const {
    return row_kind(selection_) == CharGenRowKind::Text;
}

void CharGenScreen::feed_text(const std::vector<std::uint32_t>& codepoints) {
    CharGenRow* row = mutable_row(selection_);
    if (row == nullptr || row->kind != CharGenRowKind::Text) {
        return;
    }
    for (const std::uint32_t cp : codepoints) {
        if (cp < 0x20 || cp == 0x7F) {
            continue; // управляющие знаки — не текст
        }
        if (utf8_length(row->text) >= row->text_max) {
            return;
        }
        append_utf8(row->text, cp);
    }
}

void CharGenScreen::backspace() {
    CharGenRow* row = mutable_row(selection_);
    if (row != nullptr && row->kind == CharGenRowKind::Text) {
        pop_utf8(row->text);
    }
}

std::string CharGenScreen::name() const {
    const CharGenRow* row = find(CHARGEN_NAME_ROW);
    return row != nullptr ? row->text : std::string{};
}

void CharGenScreen::set_name(std::string value) {
    if (CharGenRow* row = mutable_row(CHARGEN_NAME_ROW); row != nullptr) {
        row->text = std::move(value);
    }
}

// --- МЫШЬ -------------------------------------------------------------------

std::size_t CharGenScreen::row_at(int canvas_w, int canvas_h, int x, int y) const {
    const CharGenLayout L = chargen_layout(canvas_w, canvas_h, row_count());
    if (x < 0 || x > L.panel_right + L.step) {
        return row_count();
    }
    const int half = std::max(1, L.step / 2);
    for (std::size_t i = 0; i < row_count(); ++i) {
        const int cy = L.row_y(i);
        if (y >= cy - half && y < cy + half) {
            return i;
        }
    }
    return row_count();
}

std::size_t CharGenScreen::tab_at(int canvas_w, int canvas_h, int x, int y) const {
    if (categories_.empty()) {
        return categories_.size();
    }
    const CharGenLayout L = chargen_layout(canvas_w, canvas_h, row_count());
    const int cap = std::max(1, ui_cap_height(L.item_px));
    if (y < L.tabs_y - cap / 2 || y > L.tabs_y + cap + cap / 2) {
        return categories_.size();
    }
    const std::vector<CharGenTabBox> boxes = chargen_tab_boxes(L, categories_);
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        // Поле в половину зазора с каждой стороны: указатель между вкладками
        // обязан попасть в одну из них, а не в пустоту.
        const int pad = std::max(1, L.item_px / 2);
        if (x >= boxes[i].x - pad && x < boxes[i].x + boxes[i].w + pad) {
            return i;
        }
    }
    return categories_.size();
}

std::size_t CharGenScreen::press(int canvas_w, int canvas_h, int x, int y) {
    if (const std::size_t tab = tab_at(canvas_w, canvas_h, x, y);
        tab < categories_.size()) {
        set_category(tab);
        drag_row_ = row_count();
        return row_count();
    }
    const std::size_t row = row_at(canvas_w, canvas_h, x, y);
    if (row >= row_count() || row_kind(row) != CharGenRowKind::Slider) {
        drag_row_ = row_count();
        return row_count();
    }
    const CharGenLayout L = chargen_layout(canvas_w, canvas_h, row_count());
    const SliderTrack track = L.track_of(row);
    if (!slider_hit(track, x, y)) {
        drag_row_ = row_count();
        return row_count();
    }
    selection_ = row;
    drag_row_ = row;
    // НАЖАТИЕ УЖЕ СТАВИТ ЗНАЧЕНИЕ, а не только берётся за ручку: щелчок по
    // середине полосы обязан отвести ручку туда — иначе полоса выглядит
    // нажимаемой и не нажимается.
    const CharGenRow* r = row_at_index(row);
    (void)set_value(row, slider_value_at(track, x, r->lo, r->hi));
    return row;
}

bool CharGenScreen::drag(int canvas_w, int canvas_h, int x) {
    if (!dragging()) {
        return false;
    }
    const CharGenLayout L = chargen_layout(canvas_w, canvas_h, row_count());
    const SliderTrack track = L.track_of(drag_row_);
    const CharGenRow* r = row_at_index(drag_row_);
    if (r == nullptr) {
        return false;
    }
    return set_value(drag_row_, slider_value_at(track, x, r->lo, r->hi));
}

void CharGenScreen::release() { drag_row_ = row_count(); }

bool CharGenScreen::over_figure(int canvas_w, int canvas_h, int x) const {
    const CharGenLayout L = chargen_layout(canvas_w, canvas_h, row_count());
    return x > L.panel_right + L.step;
}

CharGenAction CharGenScreen::activate() {
    const CharGenRow* row = row_at_index(selection_);
    if (row == nullptr || row->kind != CharGenRowKind::Button) {
        return CharGenAction::None;
    }
    const CharGenAction action = row->action;
    if (action == CharGenAction::Reset) {
        reset_rows();
    }
    return action;
}

// --- ОТРИСОВКА --------------------------------------------------------------

void CharGenScreen::draw(render::PixelCanvas& canvas) const {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    canvas.clear(BACKGROUND);
    const CharGenLayout L = chargen_layout(w, h, row_count());
    if (L.item_px <= 0) {
        return; // шрифт не испечён: рисовать блочным игроку нельзя (UiFont.h)
    }
    const int cap = std::max(1, ui_cap_height(L.item_px));

    ui_draw_text(canvas, L.label_x, L.title_y, loc("chargen.title"), TITLE,
                 L.title_px, /*shadow=*/true);

    // ПОЛОСА ВКЛАДОК. Ящик и надпись считаются одной арифметикой (tab_at):
    // «экран иногда не берёт мой клик» рождается ровно из второй копии.
    canvas.fill_rect(L.label_x, L.tabs_y + cap + cap / 3, L.panel_right - L.label_x,
                     std::max(1, L.item_px / 24), RULE_LINE);
    const std::vector<CharGenTabBox> tabs = chargen_tab_boxes(L, categories_);
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        const bool active = (i == category_);
        ui_draw_text(canvas, tabs[i].x, L.tabs_y, loc(categories_[i].key),
                     active ? ITEM_SELECTED : BLURB, L.item_px, /*shadow=*/true);
        if (active) {
            canvas.fill_rect(tabs[i].x, L.tabs_y + cap + cap / 3, tabs[i].w,
                             std::max(1, L.item_px / 12), ITEM_SELECTED);
        }
    }

    SliderInk ink;
    ink.track = RULE_LINE;
    ink.fill = BLURB;

    for (std::size_t i = 0; i < row_count(); ++i) {
        const CharGenRow* row = row_at_index(i);
        if (row == nullptr) {
            continue;
        }
        const bool sel = (i == selection_);
        const render::Color colour = sel ? ITEM_SELECTED : ITEM;
        const int y = L.row_y(i);
        const int text_y = y - cap / 2;
        if (sel) {
            ui_draw_text(canvas, L.label_x - L.item_px, text_y, ">", ITEM_SELECTED,
                         L.item_px, /*shadow=*/true);
        }
        const std::string value = row_value_text(*row);
        switch (row->kind) {
        case CharGenRowKind::Slider:
            ink.label = colour;
            ink.value = sel ? ITEM_SELECTED : BLURB;
            ink.handle = colour;
            draw_slider(canvas, L.track_of(i), row_label(*row), L.label_x, value,
                        L.value_right, row->value, row->lo, row->hi, ink, L.item_px,
                        sel, dragging() && drag_row_ == i);
            break;
        case CharGenRowKind::Option: {
            ui_draw_text(canvas, L.label_x, text_y, row_label(*row), colour,
                         L.item_px, /*shadow=*/true);
            // СТРЕЛКИ ПО БОКАМ — ЕДИНСТВЕННОЕ, ЧТО ОТЛИЧАЕТ ПЕРЕКЛЮЧАТЕЛЬ ОТ
            // ПОДПИСИ. Без них строка со словом справа неотличима от строки со
            // значением, которое нельзя тронуть.
            const render::Color arrow = sel ? ITEM_SELECTED : BLURB;
            ui_draw_text(canvas, L.track_x, text_y, "<", arrow, L.item_px,
                         /*shadow=*/true);
            const int vw = ui_text_width(value, L.item_px);
            ui_draw_text(canvas, L.track_x + (L.value_right - L.track_x - vw) / 2,
                         text_y, value, colour, L.item_px, /*shadow=*/true);
            ui_draw_text(canvas, L.value_right - ui_text_width(">", L.item_px),
                         text_y, ">", arrow, L.item_px, /*shadow=*/true);
            break;
        }
        case CharGenRowKind::Text: {
            ui_draw_text(canvas, L.label_x, text_y, row_label(*row), colour,
                         L.item_px, /*shadow=*/true);
            // ПОЛЕ ВВОДА — ПОДЧЁРКНУТАЯ СТРОКА, а не рамка: рамка на этом
            // холсте это ящик, а ящиков на экране больше нет ни одного, и
            // единственный смотрелся бы кнопкой.
            canvas.fill_rect(L.track_x, y + cap / 2 + 1, L.value_right - L.track_x,
                             std::max(1, L.item_px / 24), RULE_LINE);
            const bool empty = row->text.empty();
            ui_draw_text(canvas, L.track_x, text_y, value, empty ? BLURB : colour,
                         L.item_px, /*shadow=*/true);
            if (sel) {
                // КУРСОР СТОИТ, А НЕ МИГАЕТ. Мигание — функция стенных часов,
                // и два прогона одной дозы дали бы разные кадры (правило 13).
                const int cx = L.track_x
                               + (empty ? 0 : ui_text_width(row->text, L.item_px));
                canvas.fill_rect(cx + 1, text_y, std::max(1, L.item_px / 16), cap,
                                 ITEM_SELECTED);
            }
            break;
        }
        case CharGenRowKind::Button:
            ui_draw_text(canvas, L.label_x, text_y, row_label(*row), colour,
                         L.item_px, /*shadow=*/true);
            break;
        }
    }

    // СОСТОЯНИЕ И ПОДСКАЗКА — внизу колонки. Подсказка здесь ЗАКОННА, в
    // отличие от главного меню (там её сняли 27.08): органов управления у
    // экрана пять, и три из них — мышиные, о которых список строк не говорит
    // ничего.
    int y = h - std::max(6, h / 10);
    if (!status_.empty()) {
        ui_draw_text(canvas, L.label_x, y, status_, ITEM_SELECTED, L.hint_px,
                     /*shadow=*/true);
        y += ui_line_height(L.hint_px);
    }
    // ПОДСКАЗКА В ДВЕ СТРОКИ И ТОЛЬКО В КОЛОНКЕ. Одной строкой во всю ширину
    // она проходила ПОД ФИГУРОЙ — то есть половина её букв ложилась на ноги
    // персонажа. Экран поделён на две половины, и текст живёт в левой.
    ui_draw_text(canvas, L.label_x, y, loc("chargen.hint.mouse"), BLURB, L.hint_px,
                 /*shadow=*/true);
    ui_draw_text(canvas, L.label_x, y + ui_line_height(L.hint_px),
                 loc("chargen.hint.keys"), BLURB, L.hint_px, /*shadow=*/true);
}

} // namespace dfn::app
