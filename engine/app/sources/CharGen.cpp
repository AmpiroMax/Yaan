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

/// РАЗБИВКА ПО СЛОВАМ В ЗАДАННУЮ ШИРИНУ, не больше `max_lines` строк; хвост,
/// который не поместился, кончается многоточием.
///
/// СВОЯ, ПОТОМУ ЧТО ХОЛСТ НЕ ПЕРЕНОСИТ. PixelCanvas кладёт строку от точки и
/// молча уезжает за край — то есть длинный текст выглядит не длинным, а
/// обрезанным. Многоточие в конце — единственное честное «здесь есть ещё».
[[nodiscard]] std::vector<std::string> wrap_words(std::string_view text, int px,
                                                  int max_w, int max_lines) {
    std::vector<std::string> out;
    std::string line;
    std::size_t at = 0;
    while (at <= text.size() && static_cast<int>(out.size()) < max_lines) {
        const std::size_t space = text.find(' ', at);
        const std::string word(text.substr(at, space == std::string_view::npos
                                                   ? std::string_view::npos
                                                   : space - at));
        const std::string wider = line.empty() ? word : line + " " + word;
        if (!line.empty() && ui_text_width(wider, px) > max_w) {
            out.push_back(line);
            line = word;
        } else {
            line = wider;
        }
        if (space == std::string_view::npos) {
            break;
        }
        at = space + 1;
    }
    if (!line.empty() && static_cast<int>(out.size()) < max_lines) {
        out.push_back(line);
    }
    // ХВОСТ, КОТОРЫЙ НЕ ПОМЕСТИЛСЯ. Признак — что разбор оборвался не на конце
    // текста; тогда последняя строка получает многоточие.
    if (!out.empty() && static_cast<int>(out.size()) == max_lines
        && !line.empty() && out.back() != line) {
        out.back() += "…";
    }
    return out;
}

/// ВЛЕЗАЮТ ЛИ ГЛАГОЛЫ В ОДНУ СТРОКУ КОЛОНКИ этим кеглем. Считается тем же
/// счётом ширин, каким подвал потом и рисуется.
[[nodiscard]] bool verbs_fit(const CharGenLayout& layout,
                             const std::vector<CharGenRow>& verbs, int px) {
    int need = 0;
    for (std::size_t i = 0; i < verbs.size(); ++i) {
        need += ui_text_width(row_label(verbs[i]), px);
        need += (i + 1 < verbs.size()) ? std::max(2, px) : 0;
    }
    return layout.label_x + need <= layout.panel_right;
}

const std::vector<CharGenRow>& empty_rows() {
    static const std::vector<CharGenRow> none;
    return none;
}

} // namespace

// --- ОПИСАНИЕ ---------------------------------------------------------------

std::vector<CharGenRow> chargen_verbs() {
    // ПОРЯДОК — ПО ЧАСТОТЕ НАЖАТИЯ, а «Готово» и «Назад» отбиты вправо
    // (CHARGEN_UI.md, 2.2): глагол, стирающий работу, и глагол, её
    // заканчивающий, не должны нажиматься по инерции вслед за соседом.
    // Отбивку делает раскладка, а порядок здесь — тот, в котором подвал
    // читается слева направо.
    std::vector<CharGenRow> verbs;
    const auto verb = [&](const char* name, const char* key, CharGenAction action,
                          bool enabled) {
        CharGenRow r;
        r.kind = CharGenRowKind::Button;
        r.name = name;
        r.label_key = key;
        r.action = action;
        r.enabled = enabled;
        verbs.push_back(std::move(r));
    };
    verb("reset", "chargen.reset", CharGenAction::Reset, true);
    verb("random", "chargen.random", CharGenAction::Random, true);
    // ПРЕСЕТЫ СЕРЫЕ, И ЭТО ЧЕСТНЫЙ ХВОСТ, А НЕ НЕДОДЕЛКА. Библиотеки пресетов
    // в дереве нет: единственные пресеты, которые существуют, — это ТИПАЖИ
    // народа, и они стоят на вкладке «Происхождение». Нарисовать здесь живой
    // глагол, открывающий пустоту, значило бы соврать дважды — и про полку, и
    // про то, что типажи не она.
    verb("presets", "chargen.presets", CharGenAction::Presets, false);
    verb("compare", "chargen.compare", CharGenAction::Compare, true);
    verb("back", "chargen.back", CharGenAction::Back, true);
    verb("done", "chargen.done", CharGenAction::Done, true);
    return verbs;
}

std::vector<CharGenCategory> chargen_describe(std::vector<CharGenRow> body_rows,
                                              const std::vector<People>& peoples) {
    std::vector<CharGenCategory> out;

    // ПОРЯДОК ВКЛАДОК — ЭТО ПОРЯДОК ПРИНЯТИЯ РЕШЕНИЙ (CHARGEN_UI.md, Р7), от
    // крупного к мелкому: происхождение -> облик набором -> облик ручками ->
    // имя. С чистого нулевого тела никто не лепит; лепят, поправляя готовое.
    CharGenCategory origin;
    origin.key = "chargen.tab.origin";
    // ВКЛАДКА ЖИВА РОВНО ПОКА В ДЕРЕВЕ ЕСТЬ НАРОДЫ. Пустой каталог — законное
    // состояние дерева без ассетов, и вкладка тогда стоит серой рядом с
    // «Лицом»: игрок видит, что происхождение бывает, и что здесь его нет.
    origin.enabled = !peoples.empty();
    if (!peoples.empty()) {
        CharGenRow folk;
        folk.kind = CharGenRowKind::Option;
        folk.name = CHARGEN_PEOPLE_ROW;
        folk.label_key = "chargen.row.people";
        folk.group_key = "chargen.group.folk";
        folk.note_key = peoples.front().blurb_key;
        for (const People& p : peoples) {
            folk.choices.push_back(p.name_key);
        }
        origin.rows.push_back(std::move(folk));

        CharGenRow kind;
        kind.kind = CharGenRowKind::Option;
        kind.name = CHARGEN_ARCHETYPE_ROW;
        kind.label_key = "chargen.row.archetype";
        kind.group_key = "chargen.group.archetype";
        for (const PeopleArchetype& a : peoples.front().archetypes) {
            kind.choices.push_back(a.name_key);
        }
        origin.rows.push_back(std::move(kind));

        // ПОЛ ЧЕСТНО ОГРАНИЧЕН ИМЕНЕМ, И ЭТО НАПИСАНО СЛОВАМИ (Р9). Второй
        // пол — это второй базовый меш, перенос всех целей MORF заново и
        // перемер всех полос судьёй, то есть волна размером с телосложение и
        // лицо вместе. Притворяться, что «пол пока не завезли», хуже, чем
        // сказать, что он пока не меняет фигуру.
        CharGenRow sex;
        sex.kind = CharGenRowKind::Option;
        sex.name = CHARGEN_SEX_ROW;
        sex.label_key = "chargen.row.sex";
        sex.choices = {"chargen.sex.male", "chargen.sex.female"};
        sex.note_key = "chargen.sex.note";
        origin.rows.push_back(std::move(sex));
    }
    out.push_back(std::move(origin));

    // ТЕЛОСЛОЖЕНИЕ. Строки приходят ИЗ ФАЙЛА ТЕЛА, а РАЗДЕЛЫ — отсюда: какая
    // ручка про туловище, а какая про плечи, знает не файл, а человек.
    //
    // ПОЧЕМУ РАЗДЕЛЫ ПРИБИТЫ ЗДЕСЬ ПО ИМЕНИ ЦЕЛИ, а не приезжают из .dfo:
    // секция MORF — это ЗАМЕР (имя, полоса, дельты вершин), и группировка по
    // смыслу в ней была бы полем, которое замерять нечем. Цель, о разделе
    // которой здесь не сказано, попадает в последний — то есть новая цель в
    // файле по-прежнему становится строкой экрана сама, просто без своего
    // заголовка, пока его не назвали.
    struct Group {
        const char* key;
        std::vector<const char*> knobs;
    };
    static const Group GROUPS[] = {
        {"chargen.group.trunk",
         {"weight", "muscle", "belly", "torso-depth", "buttocks", "hips", "age"}},
        {"chargen.group.arms", {"shoulders", "deltoid", "arm-length", "leg-length"}},
        {"chargen.group.height", {CHARGEN_HEIGHT_KEY}},
    };
    // СЛОВЕСНЫЕ ПАРЫ ПО КРАЯМ ДОРОЖКИ. Ключ строится из имени цели —
    // "morph.edge.<имя>.lo" / ".hi", — поэтому новая цель заводит свою пару
    // строкой в локализации, а не строкой здесь (правило 5 и правило 6).
    CharGenCategory body;
    body.key = "chargen.tab.body";
    for (const Group& g : GROUPS) {
        bool first_in_group = true;
        for (const char* knob : g.knobs) {
            for (CharGenRow& row : body_rows) {
                if (row.name != knob || row.name.empty()) {
                    continue;
                }
                CharGenRow taken = std::move(row);
                row.name.clear(); // взята: второй раздел её уже не увидит
                if (first_in_group) {
                    taken.group_key = g.key;
                    first_in_group = false;
                }
                taken.lo_word_key = "morph.edge." + taken.name + ".lo";
                taken.hi_word_key = "morph.edge." + taken.name + ".hi";
                body.rows.push_back(std::move(taken));
                break;
            }
        }
    }
    // ХВОСТ: цели, которых ни один раздел не назвал. Они обязаны попасть на
    // экран — иначе новая цель в .dfo молча исчезала бы из редактора.
    for (CharGenRow& row : body_rows) {
        if (row.name.empty()) {
            continue;
        }
        row.lo_word_key = "morph.edge." + row.name + ".lo";
        row.hi_word_key = "morph.edge." + row.name + ".hi";
        body.rows.push_back(std::move(row));
    }
    // ЗАРУБКИ ТЕЛОСЛОЖЕНИЯ: ноль — это НАША нейтраль (замер исходного тела), и
    // ромб СПЛОШНОЙ, потому что полосу мерил судья. Рост — единственная
    // строка, у которой ромб ПОЛЫЙ: его полосу держит константа
    // CHARGEN_HEIGHT_MIN_M/MAX_M, а судья пропорций масштабу безразличен по
    // построению (CHARGEN_UI.md, Р3).
    for (CharGenRow& row : body.rows) {
        row.marks.has_notch = true;
        if (row.name == CHARGEN_HEIGHT_KEY) {
            row.marks.notch = CHARGEN_BODY_HEIGHT_M;
            row.marks.measured = false;
        } else {
            row.marks.notch = 0.0f;
            row.marks.measured = true;
        }
    }
    out.push_back(std::move(body));

    // ВКЛАДКИ, КОТОРЫХ ЕЩЁ НЕТ, СТОЯТ СЕРЫМИ. Игрок видит карту дороги, а не
    // пустоту, и не гадает, куда делось лицо. Причины у всех трёх названы и
    // разные: лицу нужна голова (у эталона её нет — блокер фазы Ф2), цветам —
    // зона МАТЕРИАЛОВ (тело красится одной константой CHARGEN_CLAY), волосам —
    // и то и другое.
    for (const char* key : {"chargen.tab.face", "chargen.tab.hair",
                            "chargen.tab.colours"}) {
        CharGenCategory stub;
        stub.key = key;
        stub.enabled = false;
        out.push_back(std::move(stub));
    }

    CharGenCategory identity;
    identity.key = "chargen.tab.name";
    CharGenRow name;
    name.kind = CharGenRowKind::Text;
    name.name = CHARGEN_NAME_ROW;
    name.label_key = "chargen.name";
    name.text_max = CHARGEN_NAME_MAX_CHARS;
    name.group_key = "chargen.group.name";
    identity.rows.push_back(std::move(name));
    if (!peoples.empty()) {
        // «СЛУЧАЙНОЕ ИМЯ» СТОИТ ЗДЕСЬ, А НЕ В ПОДВАЛЕ. Подвальные глаголы —
        // общие для всех вкладок, а бросок имени принадлежит имени: на
        // телосложении он был бы кнопкой, которая молча меняет то, чего на
        // экране не видно.
        CharGenRow roll;
        roll.kind = CharGenRowKind::Button;
        roll.name = "roll-name";
        roll.label_key = "chargen.name.roll";
        roll.action = CharGenAction::RollName;
        roll.note_key = peoples.front().naming.rule_key;
        identity.rows.push_back(std::move(roll));
    }
    out.push_back(std::move(identity));
    return out;
}

std::vector<CharGenRowShape> chargen_row_shapes(const std::vector<CharGenRow>& rows,
                                                const std::vector<CharGenRow>& verbs) {
    std::vector<CharGenRowShape> out;
    out.reserve(rows.size() + verbs.size());
    for (const CharGenRow& r : rows) {
        CharGenRowShape shape;
        shape.two_line = r.kind == CharGenRowKind::Slider;
        shape.group_head = !r.group_key.empty();
        shape.note = !r.note_key.empty();
        out.push_back(shape);
    }
    for (std::size_t i = 0; i < verbs.size(); ++i) {
        CharGenRowShape shape;
        shape.footer = true;
        out.push_back(shape);
    }
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

int chargen_column_weight(const std::vector<CharGenRowShape>& shapes) {
    int weight = 0;
    for (const CharGenRowShape& shape : shapes) {
        if (shape.footer) {
            continue;
        }
        weight += shape.two_line ? 2 : 1;
        weight += shape.group_head ? 1 : 0;
        weight += shape.note ? 2 : 0;
    }
    return weight;
}

CharGenLayout chargen_layout(int canvas_w, int canvas_h,
                             const std::vector<CharGenRowShape>& shapes,
                             const std::vector<CharGenRowShape>& densest) {
    CharGenLayout L;
    L.title_px = ui_px(canvas_h, UiText::Title);
    L.item_px = ui_px(canvas_h, UiText::Item);
    L.caption_px = ui_px(canvas_h, UiText::Caption);
    L.hint_px = ui_px(canvas_h, UiText::Small);

    // КОЛОНКА СЛЕВА. Три поля: подпись, жёлоб, значение. Ширина панели —
    // CHARGEN_FIGURE_X_FRAC минус запас, чтобы фигура и текст не встретились
    // ни на одной сетке.
    L.label_x = std::max(4, canvas_w / 24);
    const int panel_w = static_cast<int>(static_cast<float>(canvas_w) * 0.40f);
    L.panel_right = L.label_x + panel_w;
    L.value_right = L.panel_right;

    // ПОДВАЛ И ПОДСКАЗКА СЧИТАЮТСЯ ОТ НИЗА, А НЕ ОТ ВЕРХА, и это не стиль.
    // Глаголы и подсказка прибиты к нижнему краю кадра; если считать их
    // сверху, то на короткой вкладке они уползут к её последней строке, и
    // «Готово» будет прыгать по экрану при переключении вкладок.
    const int hint_line = std::max(1, ui_line_height(L.hint_px));
    const int item_cap = std::max(1, ui_cap_height(L.item_px));
    L.hint_y = canvas_h - hint_line - canvas_h / 40;
    L.status_y = L.hint_y - hint_line;
    L.verbs_y = L.status_y - item_cap - item_cap;
    L.footer_rule_y = L.verbs_y - item_cap / 2;

    // СВЕРХУ: ЗАГОЛОВОК ПО ЦЕНТРУ, ЛИНЕЙКА, ПОЛОСА ВКЛАДОК.
    const auto place_top = [&](int item_px) {
        const int title_cap = std::max(1, ui_cap_height(L.title_px));
        const int line = std::max(1, ui_line_height(item_px));
        L.title_y = canvas_h / 22;
        L.rule_y = L.title_y + title_cap + line / 2;
        L.tabs_y = L.rule_y + line / 2;
        return L.tabs_y + line + line / 2;
    };
    int first_y = place_top(L.item_px);

    // ЛЕСТНИЦА КЕГЛЯ: все строки колонки обязаны поместиться МЕЖДУ полосой
    // вкладок и линейкой подвала. Тот же цикл, что у страницы настроек, и по
    // той же причине: строка за краем не рисуется И НЕ НАЖИМАЕТСЯ, а раскладка
    // — единственная карта, которая есть у мыши.
    // ВЫСОТА КОЛОНКИ СЧИТАЕТСЯ ПО ВЫСОТЕ СТРОКИ ШРИФТА, а не по высоте
    // прописной. Первая версия считала по прописной, и подпись пункта налезала
    // на слова под дорожкой: прописная — это буква «Т», а строка везёт ещё и
    // выносные («у», «ц») с надстрочными («ё»), и вычесть их из шага значит
    // сложить два текста в один.
    const auto column_height = [&](int item_px,
                                   const std::vector<CharGenRowShape>& list,
                                   bool compact) {
        const int line = std::max(1, ui_line_height(item_px));
        const int line_small = std::max(1, ui_line_height(L.caption_px));
        const int cap = std::max(1, ui_cap_height(item_px));
        int h = 0;
        for (const CharGenRowShape& shape : list) {
            if (shape.footer) {
                continue;
            }
            if (shape.group_head) {
                h += line_small + cap / 2;
            }
            h += (shape.two_line && !compact) ? (line + line_small + cap / 2)
                                              : (line + cap / 2);
            h += shape.note ? line_small * (compact ? 1 : CHARGEN_NOTE_LINES) : 0;
        }
        return h;
    };
    int ladder_h = canvas_h;
    while (L.item_px > 1
           && first_y + column_height(L.item_px, densest, L.compact)
                  > L.footer_rule_y) {
        ladder_h = ladder_h * 4 / 5;
        const int smaller = ui_px(ladder_h, UiText::Item);
        if (smaller >= L.item_px) {
            break; // лестница кончилась: мельче испечённого не бывает
        }
        L.item_px = smaller;
        L.caption_px = std::min(L.caption_px, L.item_px);
        first_y = place_top(L.item_px);
    }
    // ЛЕСТНИЦА КОНЧИЛАСЬ, А КОЛОНКА НЕ ВЛЕЗЛА: сжимаем ПУНКТ, а не шрифт.
    // Двенадцать ползунков по две строки — это двадцать четыре строки текста,
    // и на ретро-сетке 320x180 их не бывает ни при каком кегле. Одна строка на
    // пункт (без слов по краям) там честнее, чем пара слов поверх соседа.
    if (first_y + column_height(L.item_px, densest, /*compact=*/false)
        > L.footer_rule_y) {
        L.compact = true;
    }

    // МЕЛКИЕ КЕГЛИ ЕДУТ ЗА ЛЕСТНИЦЕЙ. Ладдер сжимает только роль Item, и на
    // первом же кадре подсказка оказалась КРУПНЕЕ подписей, которые она
    // поясняет: роли Caption и Small своей лестницы не проходят. Порядок
    // ролей — часть договора шрифта, а не совпадение кеглей.
    L.caption_px = std::min(L.caption_px, L.item_px);
    L.hint_px = std::min(L.hint_px, L.caption_px);

    const int cap = std::max(1, ui_cap_height(L.item_px));
    const int cap_small = std::max(1, ui_cap_height(L.caption_px));
    // ТРИ ПОЛЯ И ВОЗДУХ МЕЖДУ НИМИ. Значение справа набирается в самом широком
    // случае («1.84 м», «каштановые»): колонка, посчитанная по «0.00»,
    // подсовывала бы его под правый конец полосы.
    //
    // ДОРОЖКА ПОЛЗУНКА ТЕПЕРЬ ЗАНИМАЕТ ВСЮ ШИРИНУ ПАНЕЛИ МЕЖДУ СЛОВАМИ, а не
    // остаток после подписи: подпись уехала на строку выше, и делить с ней
    // ширину больше не надо. Слова по краям («суше», «полнее») набираются в
    // самом широком случае в четверть панели каждое.
    const int word_w = panel_w * 22 / 100;
    L.track_x = L.label_x + word_w;
    L.track_w = std::max(8, panel_w - 2 * word_w);
    L.handle_w = std::max(2, static_cast<int>(std::lround(SLIDER_HANDLE_W_FRAC
                                                          * static_cast<double>(cap))));
    L.handle_h = std::max(3, static_cast<int>(std::lround(SLIDER_HANDLE_H_FRAC
                                                          * static_cast<double>(cap))));

    // КЕГЛЬ ГЛАГОЛОВ. Шесть надписей в ширину колонки одной строкой влезают не
    // всегда, и первый же кадр вкладки происхождения это показал: «Назад» и
    // «Готово», отбитые вправо, наехали на «Случайно» и «Пресеты» буквами.
    // Мельче — честнее, чем внахлёст; проверяется тем же счётом ширин, каким
    // подвал потом и рисуется.
    L.verbs_px = L.item_px;
    // Ширина считается по САМОМУ ДЛИННОМУ возможному подвалу — по нему же и
    // сжимается, чтобы кегль глаголов не прыгал от вкладки к вкладке.

    // РАСКЛАДКА СТРОК: сверху вниз по колонке, глаголы — в подвал по ширине
    // своих надписей. Считается ЗДЕСЬ И ОДИН РАЗ, а читается и глазом, и
    // указателем.
    L.row_mid.assign(shapes.size(), 0);
    L.row_head.assign(shapes.size(), 0);
    L.group_head.assign(shapes.size(), -1);
    const int line = std::max(1, ui_line_height(L.item_px));
    const int line_small = std::max(1, ui_line_height(L.caption_px));

    // ОСТАТОК ВЫСОТЫ ОТДАЁТСЯ МЕЖДУСТРОЧЬЮ, А НЕ ОСТАЁТСЯ ДЫРОЙ ПОД СПИСКОМ.
    // Лестница кегля ходит ИСПЕЧЁННЫМИ ступенями (десять на весь диапазон
    // 11..96 px, шаг около трети), поэтому «самый крупный, который влез» почти
    // всегда влезает С ЗАПАСОМ — и первый же кадр показал этот запас третью
    // экрана пустоты между последним ползунком и подвалом. Промежуточного
    // кегля не существует, значит выбор один: сжать текст ещё или раздать
    // остаток воздуху. Воздух и раздаём, но НЕ БЕЗ ГРАНИЦЫ — прибавка больше
    // высоты прописной рвёт список на отдельные плавающие пункты.
    int rows_in_column = 0;
    for (const CharGenRowShape& shape : shapes) {
        rows_in_column += shape.footer ? 0 : 1;
    }
    const int spare = L.footer_rule_y - first_y
                      - column_height(L.item_px, shapes, L.compact);
    const int pad = std::clamp(spare / std::max(1, rows_in_column), 0, cap);

    int y = first_y;
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        if (shapes[i].footer) {
            continue;
        }
        if (shapes[i].group_head) {
            L.group_head[i] = y;
            y += line_small + cap / 2;
        }
        L.row_head[i] = y;
        if (shapes[i].two_line && !L.compact) {
            // СЕРЕДИНА ДОРОЖКИ — НА ВТОРОЙ СТРОКЕ пункта, там же, где слова.
            L.row_mid[i] = y + line + cap_small / 2;
            y += line + line_small + cap / 2 + pad;
        } else {
            L.row_mid[i] = y + cap / 2;
            y += line + cap / 2 + pad;
        }
        y += shapes[i].note ? line_small * (L.compact ? 1 : CHARGEN_NOTE_LINES) : 0;
    }

    // ПОСЛЕДНЯЯ СТРАХОВКА: если даже сжатая колонка не влезла, вся лестница
    // строк линейно ужимается в оставшуюся высоту. Строки при этом сближаются
    // вплотную и на самой мелкой сетке читаются плохо — но они ОСТАЮТСЯ В
    // КАДРЕ И ПОД УКАЗАТЕЛЕМ, а строка за нижним краем не рисуется И НЕ
    // НАЖИМАЕТСЯ, то есть молча исчезает вместе с тем, чем она правит.
    // Настоящий ответ здесь — прокрутка колонки (CHARGEN_UI.md, 2.2), и её
    // ещё нет; страховка держит экран живым до неё, а не вместо неё.
    if (y > L.footer_rule_y && y > first_y) {
        const int have = L.footer_rule_y - first_y;
        const int want = y - first_y;
        for (std::size_t i = 0; i < shapes.size(); ++i) {
            if (shapes[i].footer) {
                continue;
            }
            const auto squeeze = [&](int v) {
                return first_y + (v - first_y) * have / want;
            };
            L.row_mid[i] = squeeze(L.row_mid[i]);
            L.row_head[i] = squeeze(L.row_head[i]);
            if (L.group_head[i] >= 0) {
                L.group_head[i] = squeeze(L.group_head[i]);
            }
        }
    }
    return L;
}

std::vector<CharGenTabBox> chargen_verb_boxes(const CharGenLayout& layout,
                                              const std::vector<CharGenRow>& verbs) {
    std::vector<CharGenTabBox> out(verbs.size(), CharGenTabBox{});
    const int gap = std::max(2, layout.verbs_px);
    int x = layout.label_x;
    // ЛЕВАЯ ГРУППА — ВСЁ, КРОМЕ ДВУХ ПОСЛЕДНИХ; ПРАВАЯ — «Назад» и «Готово».
    // Разделение по РОЛИ, а не по номеру: слева то, что правит персонажа,
    // справа то, что заканчивает разговор о нём.
    const std::size_t right_from = verbs.size() >= 2 ? verbs.size() - 2 : verbs.size();
    for (std::size_t i = 0; i < right_from; ++i) {
        const int w = ui_text_width(row_label(verbs[i]), layout.verbs_px);
        out[i] = CharGenTabBox{x, w};
        x += w + gap;
    }
    // ПРАВАЯ ГРУППА НИКОГДА НЕ ЗАЕЗЖАЕТ НА ЛЕВУЮ. Отбивка вправо — это
    // предпочтение, а не закон: при длинных надписях (или чужом шрифте) шесть
    // слов в ширину колонки не помещаются, и «Назад», положенный поверх
    // «Сравнить», даёт ящики, которые ПЕРЕСЕКАЮТСЯ, — то есть щелчок по одному
    // глаголу нажимает другой. Так и вышло на первом же наборе. Если места
    // нет, правая группа встаёт сразу за левой: порядок и нажимаемость важнее
    // выравнивания.
    int need_right = 0;
    for (std::size_t i = right_from; i < verbs.size(); ++i) {
        need_right += ui_text_width(row_label(verbs[i]), layout.verbs_px);
        need_right += (i + 1 < verbs.size()) ? gap : 0;
    }
    int right = std::max(layout.panel_right, x + need_right + gap);
    for (std::size_t i = verbs.size(); i-- > right_from;) {
        const int w = ui_text_width(row_label(verbs[i]), layout.verbs_px);
        right -= w;
        out[i] = CharGenTabBox{right, w};
        right -= gap;
    }
    return out;
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
    // ЭКРАН НЕ ОТКРЫВАЕТСЯ НА СЕРОЙ ВКЛАДКЕ. Первая по порядку — это
    // «Происхождение», и если народов в дереве нет, она пуста и выключена:
    // открыться на ней значило бы показать игроку пустую колонку и ни одной
    // причины.
    while (category_ < categories_.size() && !categories_[category_].enabled) {
        ++category_;
    }
    if (category_ >= categories_.size()) {
        category_ = 0;
    }
    selection_ = 0;
    drag_row_ = row_count();
}

std::vector<CharGenRowShape> CharGenScreen::row_shapes() const {
    return chargen_row_shapes(rows(), verbs_);
}

std::vector<CharGenRowShape> CharGenScreen::densest_shapes() const {
    std::vector<CharGenRowShape> best;
    int best_weight = -1;
    for (const CharGenCategory& c : categories_) {
        std::vector<CharGenRowShape> shapes = chargen_row_shapes(c.rows, verbs_);
        const int weight = chargen_column_weight(shapes);
        if (weight > best_weight) {
            best_weight = weight;
            best = std::move(shapes);
        }
    }
    return best;
}

CharGenLayout CharGenScreen::layout(int canvas_w, int canvas_h) const {
    CharGenLayout L = chargen_layout(canvas_w, canvas_h, row_shapes(),
                                     densest_shapes());
    // ГЛАГОЛЫ СЖИМАЮТСЯ ЗДЕСЬ, А НЕ В chargen_layout: там нет НАДПИСЕЙ, а
    // «влезли ли шесть слов» — вопрос про буквы, а не про строки. Одна точка
    // на глаз и на указатель: оба зовут этот метод.
    if (!verbs_fit(L, verbs_, L.verbs_px)) {
        L.verbs_px = L.caption_px;
    }
    return L;
}

bool CharGenScreen::set_choices(std::string_view name,
                               std::vector<std::string> choices) {
    CharGenRow* row = mutable_row(name);
    if (row == nullptr) {
        return false;
    }
    row->choices = std::move(choices);
    // ВЫБОР ЗАЖИМАЕТСЯ НОВЫМ СПИСКОМ, а не сбрасывается в ноль: у народов
    // разное число типажей только в теории — сегодня их у всех восемь, — но
    // номер, переживший смену списка длиннее нынешнего, показывал бы пустое
    // слово, и это была бы ровно та тихая поломка, ради которой заведён
    // localized() с видимым маркером.
    if (row->choice >= row->choices.size()) {
        row->choice = row->choices.empty() ? 0 : row->choices.size() - 1;
    }
    return true;
}

bool CharGenScreen::set_choice(std::string_view name, std::size_t choice) {
    CharGenRow* row = mutable_row(name);
    if (row == nullptr || row->choices.empty()) {
        return false;
    }
    const std::size_t clamped = std::min(choice, row->choices.size() - 1);
    if (clamped == row->choice) {
        return false;
    }
    row->choice = clamped;
    return true;
}

std::size_t CharGenScreen::choice_of(std::string_view name) const {
    const CharGenRow* row = find(name);
    return row != nullptr ? row->choice : 0;
}

bool CharGenScreen::set_note(std::string_view name, std::string key) {
    CharGenRow* row = mutable_row(name);
    if (row == nullptr) {
        return false;
    }
    row->note_key = std::move(key);
    return true;
}

bool CharGenScreen::set_marks(std::string_view name, const SliderMarks& marks) {
    CharGenRow* row = mutable_row(name);
    if (row == nullptr) {
        return false;
    }
    row->marks = marks;
    return true;
}

const std::vector<CharGenRow>& CharGenScreen::rows() const {
    if (category_ >= categories_.size()) {
        return empty_rows();
    }
    return categories_[category_].rows;
}

void CharGenScreen::set_category(std::size_t index) {
    if (index >= categories_.size() || index == category_
        || !categories_[index].enabled) {
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
    // ЛИСТАНИЕ ПЕРЕПРЫГИВАЕТ СЕРЫЕ ВКЛАДКИ, а не останавливается на них: Tab,
    // упирающийся в «Лицо», читается как сломанный Tab, а не как ненаписанная
    // фаза. Шаг делается ровно categories() раз, чтобы экран из одних заглушек
    // не завис в поиске живой.
    const int n = static_cast<int>(categories_.size());
    const int step = delta >= 0 ? 1 : -1;
    int next = static_cast<int>(category_);
    for (int tried = 0; tried < n; ++tried) {
        next = (next + step) % n;
        if (next < 0) {
            next += n;
        }
        if (categories_[static_cast<std::size_t>(next)].enabled) {
            set_category(static_cast<std::size_t>(next));
            return;
        }
    }
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
    // ВЫБОР НЕ ВСТАЁТ НА ВЫКЛЮЧЕННУЮ СТРОКУ. Тот же довод, что у серой
    // вкладки: строка, на которую можно встать и с которой ничего нельзя
    // сделать, читается как сломанный Enter.
    const int n = static_cast<int>(row_count());
    if (n == 0) {
        return;
    }
    const int step = delta >= 0 ? 1 : -1;
    int next = static_cast<int>(selection_);
    for (int tried = 0; tried < n; ++tried) {
        next = (next + step) % n;
        if (next < 0) {
            next += n;
        }
        const CharGenRow* row = row_at_index(static_cast<std::size_t>(next));
        if (row != nullptr && row->enabled) {
            selection_ = static_cast<std::size_t>(next);
            return;
        }
    }
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
    const CharGenLayout L = layout(canvas_w, canvas_h);
    const int cap = std::max(1, ui_cap_height(L.item_px));
    const std::size_t first_verb = rows().size();
    // ГЛАГОЛЫ — В ПОДВАЛЕ, И У НИХ ЯЩИК ПО ШИРИНЕ НАДПИСИ. Спрашивать про них
    // «в какой строке колонки лежит y» нельзя: они все на одной высоте, и
    // ответ был бы «в первом», куда бы ни целился игрок.
    if (y >= L.verbs_y - cap / 2 && y <= L.verbs_y + cap + cap / 2) {
        const std::vector<CharGenTabBox> boxes = chargen_verb_boxes(L, verbs_);
        // ТОЧНОЕ ПОПАДАНИЕ СНАЧАЛА, ПОЛЯ — ПОТОМ, и порядок здесь не
        // вкусовой: поля соседних надписей ПЕРЕКРЫВАЮТСЯ, и цикл, который
        // сразу мерит с полями, отдаёт левого соседа за точку, лежащую ровно
        // в середине правого. Так и вышло на первом же кадре подвала.
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            if (boxes[i].w > 0 && x >= boxes[i].x && x < boxes[i].x + boxes[i].w) {
                return first_verb + i;
            }
        }
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            const int pad = std::max(1, L.verbs_px / 3);
            if (boxes[i].w > 0 && x >= boxes[i].x - pad
                && x < boxes[i].x + boxes[i].w + pad) {
                return first_verb + i;
            }
        }
        return row_count();
    }
    if (x < 0 || x > L.panel_right + cap) {
        return row_count();
    }
    const int half = std::max(1, cap);
    for (std::size_t i = 0; i < first_verb && i < L.row_mid.size(); ++i) {
        const int cy = L.row_mid[i];
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
    const CharGenLayout L = layout(canvas_w, canvas_h);
    const int cap = std::max(1, ui_cap_height(L.item_px));
    if (y < L.tabs_y - cap / 2 || y > L.tabs_y + cap + cap / 2) {
        return categories_.size();
    }
    const std::vector<CharGenTabBox> boxes = chargen_tab_boxes(L, categories_);
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        // ВЫКЛЮЧЕННАЯ ВКЛАДКА НЕ ОТВЕЧАЕТ. Серая надпись, съевшая клик, — это
        // худший вид заглушки: игрок нажал и ничего не произошло, и он не
        // знает, промахнулся он или фаза не написана.
        if (!categories_[i].enabled) {
            continue;
        }
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
    const CharGenLayout L = layout(canvas_w, canvas_h);
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
    const CharGenLayout L = layout(canvas_w, canvas_h);
    const SliderTrack track = L.track_of(drag_row_);
    const CharGenRow* r = row_at_index(drag_row_);
    if (r == nullptr) {
        return false;
    }
    return set_value(drag_row_, slider_value_at(track, x, r->lo, r->hi));
}

void CharGenScreen::release() { drag_row_ = row_count(); }

bool CharGenScreen::over_figure(int canvas_w, int canvas_h, int x) const {
    const CharGenLayout L = layout(canvas_w, canvas_h);
    return x > L.panel_right + std::max(1, ui_cap_height(L.item_px));
}

CharGenAction CharGenScreen::activate() {
    const CharGenRow* row = row_at_index(selection_);
    if (row == nullptr || row->kind != CharGenRowKind::Button || !row->enabled) {
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
    const std::vector<CharGenRowShape> shapes = row_shapes();
    const CharGenLayout L = layout(w, h);
    if (L.item_px <= 0) {
        return; // шрифт не испечён: рисовать блочным игроку нельзя (UiFont.h)
    }
    const int cap = std::max(1, ui_cap_height(L.item_px));
    const int cap_small = std::max(1, ui_cap_height(L.caption_px));
    const int line_small = std::max(1, ui_line_height(L.caption_px));
    const int rule_h = std::max(1, L.item_px / 24);

    // ЗАГОЛОВОК ПО ЦЕНТРУ ХОЛСТА. Не по центру КОЛОНКИ: экран поделён надвое,
    // но заголовок принадлежит экрану целиком, и прижатый влево он читался бы
    // подписью к столбцу ползунков.
    const std::string_view title = loc("chargen.title");
    ui_draw_text(canvas, (w - ui_text_width(title, L.title_px)) / 2, L.title_y,
                 title, TITLE, L.title_px, /*shadow=*/true);
    canvas.fill_rect(L.label_x, L.rule_y, w - 2 * L.label_x, rule_h, RULE_LINE);

    // ПОЛОСА ВКЛАДОК. Ящик и надпись считаются одной арифметикой (tab_at):
    // «экран иногда не берёт мой клик» рождается ровно из второй копии.
    const std::vector<CharGenTabBox> tabs = chargen_tab_boxes(L, categories_);
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        const bool active = (i == category_);
        // ТРИ ТОНА, А НЕ ДВА: выбранная — ITEM_SELECTED, доступная — ITEM,
        // ненаписанная — BLURB. Без третьего тона серая вкладка неотличима от
        // просто невыбранной, и игрок жмёт её снова и снова.
        const render::Color colour = !categories_[i].enabled ? BLURB
                                     : active               ? ITEM_SELECTED
                                                            : ITEM;
        ui_draw_text(canvas, tabs[i].x, L.tabs_y, loc(categories_[i].key), colour,
                     L.item_px, /*shadow=*/true);
        if (active) {
            canvas.fill_rect(tabs[i].x, L.tabs_y + cap + cap / 3, tabs[i].w,
                             std::max(1, L.item_px / 12), ITEM_SELECTED);
        }
    }

    SliderInk ink;
    ink.track = RULE_LINE;
    ink.fill = BLURB;
    ink.notch = BLURB;
    ink.band = ITEM;

    const std::size_t first_verb = rows().size();
    for (std::size_t i = 0; i < first_verb && i < L.row_mid.size(); ++i) {
        const CharGenRow* row = row_at_index(i);
        if (row == nullptr) {
            continue;
        }
        if (L.group_head[i] >= 0) {
            ui_draw_text(canvas, L.label_x, L.group_head[i], loc(row->group_key),
                         BLURB, L.caption_px, /*shadow=*/true);
        }
        const bool sel = (i == selection_);
        const render::Color colour = !row->enabled     ? BLURB
                                     : sel             ? ITEM_SELECTED
                                                       : ITEM;
        const int mid = L.row_mid[i];
        const int head = L.row_head[i];
        const std::string value = row_value_text(*row);
        switch (row->kind) {
        case CharGenRowKind::Slider: {
            // ДВЕ СТРОКИ НА ПУНКТ: подпись со значением сверху, дорожка со
            // словами по краям снизу (CHARGEN_UI.md, 2.2). Число справа
            // мелким кеглем — оно нужно не игроку, а нам: им сводятся кадр,
            // DFN_MORPH и player.json.
            ui_draw_text(canvas, L.label_x, head, row_label(*row), colour,
                         L.item_px, /*shadow=*/true);
            const int vw = ui_text_width(value, L.caption_px);
            ui_draw_text(canvas, L.value_right - vw, head + (cap - cap_small), value,
                         sel ? ITEM_SELECTED : ITEM, L.caption_px, /*shadow=*/true);
            if (!row->lo_word_key.empty() && !L.compact) {
                ui_draw_text(canvas, L.label_x, mid - cap_small, loc(row->lo_word_key),
                             BLURB, L.caption_px, /*shadow=*/true);
            }
            if (!row->hi_word_key.empty() && !L.compact) {
                const std::string_view word = loc(row->hi_word_key);
                ui_draw_text(canvas,
                             L.value_right - ui_text_width(word, L.caption_px),
                             mid - cap_small, word, BLURB, L.caption_px,
                             /*shadow=*/true);
            }
            ink.handle = colour;
            draw_slider(canvas, L.track_of(i), {}, L.label_x, {}, L.value_right,
                        row->value, row->lo, row->hi, ink, L.item_px, sel,
                        dragging() && drag_row_ == i, &row->marks);
            break;
        }
        case CharGenRowKind::Option: {
            ui_draw_text(canvas, L.label_x, head, row_label(*row), colour,
                         L.item_px, /*shadow=*/true);
            // СТРЕЛКИ ПО БОКАМ — ЕДИНСТВЕННОЕ, ЧТО ОТЛИЧАЕТ ПЕРЕКЛЮЧАТЕЛЬ ОТ
            // ПОДПИСИ. Без них строка со словом справа неотличима от строки со
            // значением, которое нельзя тронуть.
            const render::Color arrow = sel ? ITEM_SELECTED : BLURB;
            // СТРЕЛКА ОТСТУПАЕТ ОТ ПОДПИСИ, А НЕ СТОИТ НА МЕСТЕ. Жёлоб начат
            // там, где кончается самое широкое СЛОВО края дорожки, — а подпись
            // переключателя («Народ», «Типаж») бывает длиннее, и на первом же
            // кадре «<» въехал в неё буквами.
            const int arrow_x =
                std::max(L.track_x, L.label_x
                                        + ui_text_width(row_label(*row), L.item_px)
                                        + L.item_px);
            ui_draw_text(canvas, arrow_x, head, "<", arrow, L.item_px,
                         /*shadow=*/true);
            const int vw = ui_text_width(value, L.item_px);
            ui_draw_text(canvas, arrow_x + (L.value_right - arrow_x - vw) / 2, head,
                         value, colour, L.item_px, /*shadow=*/true);
            ui_draw_text(canvas, L.value_right - ui_text_width(">", L.item_px),
                         head, ">", arrow, L.item_px, /*shadow=*/true);
            break;
        }
        case CharGenRowKind::Text: {
            ui_draw_text(canvas, L.label_x, head, row_label(*row), colour,
                         L.item_px, /*shadow=*/true);
            // ПОЛЕ ВВОДА — ПОДЧЁРКНУТАЯ СТРОКА, а не рамка: рамка на этом
            // холсте это ящик, а ящиков на экране больше нет ни одного, и
            // единственный смотрелся бы кнопкой.
            canvas.fill_rect(L.track_x, head + cap + 1, L.value_right - L.track_x,
                             rule_h, RULE_LINE);
            const bool empty = row->text.empty();
            ui_draw_text(canvas, L.track_x, head, value, empty ? BLURB : colour,
                         L.item_px, /*shadow=*/true);
            if (sel) {
                // КУРСОР СТОИТ, А НЕ МИГАЕТ. Мигание — функция стенных часов,
                // и два прогона одной дозы дали бы разные кадры (правило 13).
                const int cx = L.track_x
                               + (empty ? 0 : ui_text_width(row->text, L.item_px));
                canvas.fill_rect(cx + 1, head, std::max(1, L.item_px / 16), cap,
                                 ITEM_SELECTED);
            }
            break;
        }
        case CharGenRowKind::Button:
            ui_draw_text(canvas, L.label_x, head, row_label(*row), colour,
                         L.item_px, /*shadow=*/true);
            break;
        }
        if (!row->note_key.empty() && !L.compact) {
            // ПОЯСНЕНИЕ ПОД ПУНКТОМ. Лорная строка народа, честная пометка про
            // пол, правило имени — всё, что игрок обязан прочесть один раз и
            // больше не вспоминать. ПЕРЕНОСИТСЯ ПО СЛОВАМ В ШИРИНУ КОЛОНКИ:
            // холст не переносит сам, и первая же лорная строка ушла на
            // фигуру, лёгши буквами по её груди.
            const int note_y = shapes[i].two_line ? mid + cap_small
                                                  : head + line_small;
            const int note_w = L.panel_right - L.label_x;
            int written = 0;
            for (const std::string& one :
                 wrap_words(loc(row->note_key), L.caption_px, note_w,
                            CHARGEN_NOTE_LINES)) {
                ui_draw_text(canvas, L.label_x, note_y + written * line_small, one,
                             BLURB, L.caption_px, /*shadow=*/true);
                ++written;
            }
        }
        // УКАЗАТЕЛЬ ВЫБОРА — У ПОДПИСИ, а не у дорожки: подпись стоит у всех
        // четырёх видов строк, дорожка — только у ползунка.
        if (sel) {
            ui_draw_text(canvas, L.label_x - L.item_px, head, ">", ITEM_SELECTED,
                         L.item_px, /*shadow=*/true);
        }
    }

    // ПОДВАЛ ГЛАГОЛОВ. Он живёт ВНУТРИ КОЛОНКИ, а не во всю ширину экрана, и
    // это не отступление от дизайна, а его же довод, доведённый до кадра:
    // фигура занимает 0.86 высоты и её стопы доходят почти до низа, поэтому
    // строка во всю ширину прошла бы ПО НОГАМ персонажа — ровно та беда, из-за
    // которой подсказку когда-то и загнали в колонку.
    canvas.fill_rect(L.label_x, L.footer_rule_y, L.panel_right - L.label_x, rule_h,
                     RULE_LINE);
    const std::vector<CharGenTabBox> verb_boxes = chargen_verb_boxes(L, verbs_);
    for (std::size_t v = 0; v < verbs_.size(); ++v) {
        const std::size_t row_index = first_verb + v;
        const bool sel = (row_index == selection_);
        const render::Color colour = !verbs_[v].enabled ? BLURB
                                     : sel              ? ITEM_SELECTED
                                                        : ITEM;
        ui_draw_text(canvas, verb_boxes[v].x, L.verbs_y, row_label(verbs_[v]),
                     colour, L.item_px, /*shadow=*/true);
        if (sel) {
            canvas.fill_rect(verb_boxes[v].x, L.verbs_y + cap + cap / 3,
                             verb_boxes[v].w, std::max(1, L.item_px / 12),
                             ITEM_SELECTED);
        }
    }

    // СОСТОЯНИЕ И ПОДСКАЗКА — ПОД ПОДВАЛОМ, ОДНОЙ СТРОКОЙ КАЖДАЯ. Подсказка
    // здесь ЗАКОННА, в отличие от главного меню (там её сняли 27.08): органов
    // управления у экрана пять, и три из них — мышиные, о которых список строк
    // не говорит ничего.
    if (!status_.empty()) {
        ui_draw_text(canvas, L.label_x, L.status_y, status_, ITEM_SELECTED,
                     L.hint_px, /*shadow=*/true);
    }
    // ПОДСКАЗКА ОДНОЙ СТРОКОЙ, И ОНА ОБЯЗАНА ПОМЕСТИТЬСЯ. Строка, уехавшая за
    // правый край, обрывается на полуслове и читается как поломка шрифта, а не
    // как длинный текст: холст не переносит и не сжимает. Если полная не
    // влезла, берётся КОРОТКАЯ — честный хвост, а не обрезок.
    std::string_view hint = loc("chargen.hint");
    if (L.label_x + ui_text_width(hint, L.hint_px) > w - L.label_x) {
        hint = loc("chargen.hint.short");
    }
    ui_draw_text(canvas, L.label_x, L.hint_y, hint, BLURB, L.hint_px,
                 /*shadow=*/true);
}

} // namespace dfn::app
