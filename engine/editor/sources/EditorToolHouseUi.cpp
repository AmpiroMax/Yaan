/*
Created: 18:08:2026 - 18:02:11
Last updated: 20:08:2026 - 22:40:00
Module: engine/editor
File: engine/editor/sources/EditorToolHouseUi.cpp

Responsibility:
- ПАНЕЛИ трёх инструментов постройки: высота вершины над землёй, зажим длины у
  прямой, числа и лицо у поверхности. Здесь и только здесь живёт Dear ImGui —
  решения лежат в EditorToolHouse.cpp, у которого нет ни окна, ни рисования.

WHY THE SPLIT (правило 3): цель app_editor_house линкует EditorToolHouse.cpp и
НЕ линкует этот файл. Иначе рукав тянул бы за собой ImGui, а вместе с ним и
контекст окна — и ни один вопрос про нормаль, зажим и отказ нельзя было бы
задать без экрана.

Dependencies:
- Uses: EditorToolHouse.h, EditorUi.h (tr), Dear ImGui.
- Used by: engine/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ВСЕ ВИДИМЫЕ СТРОКИ ЧЕРЕЗ EditorUi::tr (правило 5). Исключение — ОТКАЗ
  МОДЕЛИ и подписи со списком держателей: это уже готовое предложение, которое
  собрал инструмент, и второй перевод на месте показа сделал бы из одного
  приговора два.
- НИ ОДНОГО РЕШЕНИЯ ЗДЕСЬ. Кнопка зовёт метод инструмента; если кнопке нужно
  что-то посчитать — считать это надо в EditorToolHouse.cpp, иначе оно окажется
  за пределами рукава.
*/
/*
UPD:
- 18:08:2026 - 18:02:11: Создан вместе с EditorToolHouse.{h,cpp}.
- 18:08:2026 - 19:44:10: Ползунок правит то же число, что и колесо, через set_pull_m.
- 18:08:2026 - 22:20:15: Свойства выбранного элемента — один блок на три панели: полутолщина, толщина, высота, поворот текстуры, разворот лица, удаление.
- 18:08:2026 - 23:20:00: Блок сетки и точных координат якоря — один на три панели.
- 19:08:2026 - 01:20:45: PushID на сетку и блок выбранного (ImGui кричал про конфликт ID — подписи ползунков совпадали с инструментными); draw_house_selection_panel — панель выбранного для инструмента выбора.
- 19:08:2026 - 02:34:20: Заголовки «Заготовка» и «Выбрано сейчас» (долг 4): два блока одинаковых полей перестали читаться как один.
- 19:08:2026 - 04:05:50: Панель выбранного: материал (9), тон (4), форма палки (круг/квадрат/6/8), дверь с листанием петли по кругу.
- 19:08:2026 - 05:26:10: У стены-цепочки: галочка «Обшивка» и число окон; сколько влезло — скажет журнал.
- 19:08:2026 - 23:58:20: Комбо заготовки ВИДНЫ в меню инструмента (жалоба «не вижу ничего нового»); списки материалов/тонов/форм — одни на заготовку и выбранное.
- 20:08:2026 - 00:02:30: Материал и тон — СЕТКОЙ КАРТИНОК (свотчи листа набора, выбранный подсвечен), заполнение стены — ТРЕМЯ КАРТОЧКАМИ-примерами вместо галочки; клик в свойствах правит объект сразу.
- 20:08:2026 - 00:58:40: Пять карточек заполнения: гладкая, фахверк, фахверк с окнами, кирпич, блоки — и в заготовке, и у выбранного.
- 20:08:2026 - 01:47:30: Поворот сечения у балок (заготовка и выбранное); кнопка «Дверной проём»; пол: «срез/паркет» карточками; форм пять.
- 20:08:2026 - 01:06:50: Порядок в блоке выбранного: заполнение первым, материал ниже — карточки кладки тонули за прокруткой под сеткой свотчей.
- 20:08:2026 - 12:10:00: Семь профилей палки; ряд красок в сетке материалов; покрытия контура — три карточки (срез/паркет/марш).
- 20:08:2026 - 12:55:00: Ползунок пишет в граф по отпусканию, а не каждый кадр.
- 20:08:2026 - 17:30:00: Износ и детали в панели; покрытия контура — пять карточек (+дранка, черепица); полка стилей .dfstyle; полка готовых построек в панели выбора.
- 20:08:2026 - 20:30:00: Полка построек — в панель инструмента стен (без требования выделения); кнопки марша (сплошной/доски/блоки), балок, распаковки.
- 20:08:2026 - 22:40:00: Единая пара fill_to_card/card_to_fill + карточка венцов; отказ первой строкой; перенос карточек по ширине (две были обрезаны за краем); сетка мира над заголовком «Выбрано»; дубли кнопки удаления и координат сняты; полки не сканируют диск каждый кадр.
*/

#include "engine/editor/sources/EditorToolHouse.h"
#include "engine/world/sources/HouseMesh.h"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>
#include "engine/editor/sources/EditorUi.h"

#include <imgui.h>

#include <cstdio>

namespace dfn::app {
namespace {

/// Мир, которого нет. Панель рисуется и тогда, когда крючки не розданы (проверка
/// без App), а кнопке «создать» мир нужен по подписи метода — пустой ToolWorld
/// честнее указателя, который иногда null.
ToolWorld& no_world() {
    static ToolWorld empty;
    return empty;
}

void draw_refusal(const std::string& text) {
    if (text.empty()) {
        return;
    }
    // ОТКАЗ ВИДЕН, А НЕ УХОДИТ В stderr. Ровно это разбирали 18.08 трижды:
    // молча не сработавший инструмент неотличим от сломанного.
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", text.c_str());
}

/// Списки материалов набора — ОДНИ на заготовку и на выбранное: два списка с
/// разным порядком дали бы «камень», который при правке становится глиной.
static const char* HOUSE_MATS[9] = {"тёсаный брус", "пилёная доска", "торец",
                                    "камень",       "обожжённая глина", "штукатурка",
                                    "солома",       "дёрн",           "остекление"};
static const char* HOUSE_TONES[4] = {"светлый", "средний", "тёмный", "выветренный"};
/// ПРОФИЛИ ПАЛКИ. «Лестницы» здесь больше нет (правка 20.08: «лестница же
/// на 4 точках держится») — марш теперь карточка раздела стен, fill=6.
static const char* HOUSE_FORMS[7] = {"круглая",        "квадратная",
                                     "треугольная",    "шестигранная",
                                     "восьмигранная",  "двенадцатигранная",
                                     "доска"};
/// Число граней по индексу формы; 0 — особая форма (круг/квадрат/доска).
static const int HOUSE_FORM_SIDES[7] = {0, 0, 3, 6, 8, 12, 0};
/// Названия красок — подсказки к цветным кнопкам; сами цвета — HOUSE_PAINT_RGB
/// (одна таблица на редактор и загрузку, правило 32).
static const char* HOUSE_PAINTS[world::HOUSE_PAINT_COUNT] = {
    "без краски",   "белила", "охра",   "красная фалу",
    "зелёная медь", "синяя",  "дёготь", "седая известь"};

/// МИР ДЛЯ ПАНЕЛИ ВЫБРАННОГО. draw_selected_element зовут четыре панели
/// (три инструмента постройки и выбор), а крючки картинок живут в ToolWorld
/// у каждого своя ссылка. Статика файла — а не параметр — потому что подпись
/// draw_house_selection_panel отдана инструменту выбора, и менять её ради
/// прокладки мира значило бы трогать три зоны разом; выставляется каждым
/// draw_settings перед вызовом.
static const ToolWorld* g_selected_world = nullptr;

/// СЕТКА КАРТИНОК-МАТЕРИАЛОВ. Возвращает true и пишет выбор, когда человек
/// кликнул по свотчу. Заказ 19.08: «хочу не слова видеть, а картинки»; слова
/// остаются подсказкой при наведении и запасным ходом, когда крючка картинок
/// нет (панель без мира, правило 3).
static bool draw_material_grid(const ToolWorld* world, const char* id, int& mat,
                               int& tone, int* paint = nullptr) {
    bool changed = false;
    ImGui::PushID(id);
    // КРАСКА — цветные кнопки без текстуры: слой отделки поверх материала.
    if (paint != nullptr) {
        for (int c = 0; c < world::HOUSE_PAINT_COUNT; ++c) {
            if (c != 0) {
                ImGui::SameLine();
            }
            ImGui::PushID(200 + c);
            const glm::vec3 rgb = world::HOUSE_PAINT_RGB[c];
            const bool sel = c == *paint;
            if (ImGui::ColorButton("##p", ImVec4(rgb.x, rgb.y, rgb.z, 1.0f),
                                   ImGuiColorEditFlags_NoTooltip
                                       | ImGuiColorEditFlags_NoAlpha,
                                   ImVec2(sel ? 28.0f : 22.0f, sel ? 28.0f : 22.0f))) {
                *paint = c;
                changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", HOUSE_PAINTS[c]);
            }
            ImGui::PopID();
        }
    }
    if (world != nullptr && world->material_swatch) {
        constexpr float PX = 40.0f;
        for (int m = 0; m < 9; ++m) {
            if (m % 5 != 0) {
                ImGui::SameLine();
            }
            const std::uint64_t tex = world->material_swatch(m, tone, 40);
            ImGui::PushID(m);
            const bool sel = m == mat;
            if (sel) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.77f, 0.28f, 1.0f));
            }
            if (tex != 0 ? EditorUi::image_button("##m", tex, PX, PX)
                         : ImGui::Button(HOUSE_MATS[m], ImVec2(PX * 2.2f, PX * 0.6f))) {
                mat = m;
                changed = true;
            }
            if (sel) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", HOUSE_MATS[m]);
            }
            ImGui::PopID();
        }
        // ТОНА — ТОЖЕ КАРТИНКАМИ: те же координаты листа, свой ряд поменьше.
        for (int t = 0; t < 4; ++t) {
            if (t != 0) {
                ImGui::SameLine();
            }
            const std::uint64_t tex = world->material_swatch(mat, t, 24);
            ImGui::PushID(100 + t);
            const bool sel = t == tone;
            if (sel) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.77f, 0.28f, 1.0f));
            }
            if (tex != 0 ? EditorUi::image_button("##t", tex, 24.0f, 24.0f)
                         : ImGui::Button(HOUSE_TONES[t])) {
                tone = t;
                changed = true;
            }
            if (sel) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", HOUSE_TONES[t]);
            }
            ImGui::PopID();
        }
    } else {
        changed |= ImGui::Combo(EditorUi::tr("house.mat"), &mat, HOUSE_MATS, 9);
        changed |= ImGui::Combo(EditorUi::tr("house.tone"), &tone, HOUSE_TONES, 4);
    }
    ImGui::PopID();
    return changed;
}

/// ОДНА ПАРА КОДИРОВОК КАРТОЧКА<->ЗАПОЛНЕНИЕ на заготовку и выбранное
/// (аудит #3, находка 6: две рукописные копии вели себя по-разному — выбор
/// кладки у выбранного ОБНУЛЯЛ окна, а венцы были недостижимы из панели).
static int fill_to_card(int fill, bool clad, int wins) {
    if (fill == 2) { return 3; }
    if (fill == 3) { return 4; }
    if (fill == 4) { return 5; }
    return clad ? (wins > 0 ? 2 : 1) : 0;
}
static void card_to_fill(int card, int& fill, bool& clad, int& wins) {
    clad = card == 1 || card == 2;
    fill = card == 3 ? 2 : (card == 4 ? 3 : (card == 5 ? 4 : 0));
    // Счёт окон НЕ трогается — карточка выбирает правило сборки, а не
    // отменяет пользовательский ввод; исключение — «фахверк с окнами» без
    // окон получает стартовые два, иначе карточка выглядит несработавшей.
    if (card == 2 && wins == 0) { wins = 2; }
    if (card == 0) { wins = 0; }
}

/// КАРТОЧКИ ЗАПОЛНЕНИЯ СТЕНЫ: гладкая, фахверк, фахверк с окнами, кирпич,
/// блоки, венцы. Возвращает -1 (не трогали) или выбранный вариант.
static int draw_fill_cards(const ToolWorld* world, const char* id, int current) {
    int picked = -1;
    ImGui::PushID(id);
    static const char* NAMES[6] = {"гладкая", "фахверк (доски и раскосы)",
                                   "фахверк с окнами", "кирпичная кладка",
                                   "каменные блоки", "венцы сруба"};
    for (int v = 0; v < 6; ++v) {
        // ПЕРЕНОС ПО ОСТАТКУ ШИРИНЫ (UX-аудит: пять карточек по 84 px в
        // колонке 380 — последние обрезались за краем БЕЗ признака обрезки).
        if (v != 0 && ImGui::GetCursorPosX() + 88.0f
                          < ImGui::GetWindowContentRegionMax().x - 88.0f) {
            ImGui::SameLine();
        }
        ImGui::PushID(v);
        const bool sel = v == current;
        if (sel) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.77f, 0.28f, 1.0f));
        }
        const std::uint64_t tex =
            world != nullptr && world->wall_example ? world->wall_example(v, 84) : 0;
        if (tex != 0 ? EditorUi::image_button("##f", tex, 84.0f, 56.0f)
                     : ImGui::Button(NAMES[v])) {
            picked = v;
        }
        if (sel) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", NAMES[v]);
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    return picked;
}


/// СЕТКА МИРА И ТОЧНЫЕ КООРДИНАТЫ ЯКОРЯ — ОДИН БЛОК НА ВСЕ ТРИ ИНСТРУМЕНТА.
///
/// Два заказа 18.08 рядом, потому что отвечают на один вопрос — «поставь ровно
/// туда, куда я хочу»: «возможность включать и выключать сетку мира, чтобы я
/// мог по сетке некоторые вещи строить... шаг сетки менять» и «нет возможности
/// указать точную высоту якоря или другие его координаты».
///
/// СЕТКА В МИРОВЫХ КООРДИНАТАХ, и это его слова: узел там, где координата
/// кратна шагу, а не там, где начали строить. Один шаг на всё — по нему
/// прилипает якорь, им же шагают стрелки, его же рисуют отсечки на земле.
static void draw_grid_and_coords(HouseSession& session) {
    // СВОЯ ОБЛАСТЬ ИМЁН: ползунки ниже повторяют подписи ползунков инструмента
    // («Толщина», «Поворот текстуры»), и без PushID ImGui честно кричал
    // «2 visible items with conflicting ID» — кадр пользователя 19.08.
    ImGui::PushID("house.grid");
    bool on = session.grid_on();
    if (ImGui::Checkbox(EditorUi::tr("house.grid"), &on)) {
        session.set_grid_on(on);
    }
    float step = session.grid_step_m();
    if (ImGui::SliderFloat(EditorUi::tr("house.grid.step"), &step, 0.05f, 5.0f, "%.2f m")) {
        session.set_grid_step_m(step);
    }
    ImGui::TextDisabled("%s", EditorUi::tr("house.grid.note"));

    const world::VertexId sel = session.selected_vertex();
    if (sel == world::NO_VERTEX) {
        ImGui::PopID();
        return;
    }
    // КООРДИНАТЫ ЧИТАЮТСЯ ИЗ МИРА И ПИШУТСЯ В МИР. Внутри граф хранит местные
    // координаты постройки, но человек думает в мировых — он их и видит в
    // отладочном выводе, и по ним ставит дом на карте.
    glm::vec3 p = session.vertex_world(sel);
    const float before[3] = {p.x, p.y, p.z};
    if (ImGui::InputFloat3(EditorUi::tr("house.coords"), &p.x, "%.3f")) {
        if (p.x != before[0] || p.y != before[1] || p.z != before[2]) {
            (void)session.mutate("координаты якоря", [&](world::HouseGraph& g) {
                return g.move_vertex(sel, session.to_local(p));
            });
        }
    }
    // ЗАЗЕМЛЁННЫЙ ЯКОРЬ ВЫСОТУ НЕ ХРАНИТ, и сказать это надо здесь, а не дать
    // человеку впечатать высоту и увидеть, что она не взялась.
    if (const world::Vertex* v = session.graph().vertex(sel);
        v != nullptr && v->anchoring == world::Anchoring::OnGround) {
        ImGui::TextDisabled("%s", EditorUi::tr("house.coords.ground"));
    }
    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// ПОЛКА СТИЛЕЙ (.dfstyle, заказ 20.08): снятый с элемента набор отделки —
// материал, тон, краска, заполнение, окна, износ, детали — файлом в
// assets/styles. Файл человекочитаем (key=value), diff осмыслен.
// ---------------------------------------------------------------------------

/// Ключи, которые стиль несёт. ЯВНЫЙ СПИСОК, а не «все параметры»: геометрия
/// (height, радиусы, лестницы) стилю не принадлежит — стиль это ОТДЕЛКА.
static const char* STYLE_KEYS[] = {"mat",  "tone",    "paint",   "clad",
                                   "fill", "windows", "doors",   "wear",
                                   "logends", "shutters", "porch", "plinth",
                                   "tex_deg"};

static std::vector<std::string> list_styles() {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& it :
         std::filesystem::directory_iterator("assets/styles", ec)) {
        if (it.path().extension() == ".dfstyle") {
            out.push_back(it.path().stem().string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

static void save_style(HouseSession& session, world::ElementId id,
                       const std::string& name) {
    std::error_code ec;
    std::filesystem::create_directories("assets/styles", ec);
    std::ofstream f("assets/styles/" + name + ".dfstyle",
                    std::ios::binary | std::ios::trunc);
    if (!f) {
        return;
    }
    f << "# dfstyle 1 — отделка постройки, снята редактором\n";
    for (const char* key : STYLE_KEYS) {
        const std::string v = session.graph().param(id, key);
        if (!v.empty()) {
            f << key << " = " << v << "\n";
        }
    }
}

static void apply_style(HouseSession& session, world::ElementId id,
                        const std::string& name) {
    std::ifstream f("assets/styles/" + name + ".dfstyle");
    if (!f) {
        return;
    }
    std::vector<std::pair<std::string, std::string>> kv;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        auto trim = [](std::string t) {
            const auto b = t.find_first_not_of(" \t");
            const auto e2 = t.find_last_not_of(" \t\r");
            return b == std::string::npos ? std::string{} : t.substr(b, e2 - b + 1);
        };
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));
        // Ключ вне списка отделки игнорируется: файл мог написать инструмент
        // побогаче, а геометрию выбранного стиль трогать не вправе.
        for (const char* known : STYLE_KEYS) {
            if (key == known) {
                kv.emplace_back(key, val);
                break;
            }
        }
    }
    if (kv.empty()) {
        return;
    }
    (void)session.mutate("применил стиль", [&](world::HouseGraph& g) {
        world::GraphResult last;
        for (const auto& [key, val] : kv) {
            last = g.set_param(id, key, val);
        }
        return last;
    });
}

/// СВОЙСТВА ВЫБРАННОГО ЭЛЕМЕНТА — ОДИН БЛОК НА ВСЕ ТРИ ИНСТРУМЕНТА.
///
/// Заказ 18.08: «не могу выбрать стену или прямую, чтобы поменять её
/// свойства». Выбор починен отдельно (тычок в полотно, а не только в кромку);
/// здесь — вторая половина: то, что после выбора можно ПРАВИТЬ.
///
/// Блок общий нарочно. Три копии этих полей разошлись бы в первый же день,
/// когда к стене добавят свойство, — и человек, открывший не ту панель, решил
/// бы, что свойства у стены нет.
static void draw_selected_element(HouseSession& session) {
    ImGui::PushID("house.selected"); // та же причина, что у сетки выше
    const struct PopGuard { ~PopGuard() { ImGui::PopID(); } } pop_guard;
    const world::ElementId id = session.selected_element();
    if (id == world::NO_ELEMENT) {
        ImGui::TextDisabled("%s", EditorUi::tr("house.noelem"));
        return;
    }
    const world::Element* e = session.graph().element(id);
    if (e == nullptr) {
        return;
    }
    const bool line = e->kind == world::ElementKind::Line;
    ImGui::Text("e%u · %s · %s %zu", static_cast<unsigned>(id),
                EditorUi::tr(line ? "house.kind.line" : "house.kind.surface"),
                EditorUi::tr("house.refs"), e->refs.size());

    // ЧИСЛА ЧИТАЮТСЯ ИЗ ГРАФА И ПИШУТСЯ В ГРАФ ЧЕРЕЗ ДВЕРЬ МУТАЦИЙ: правка мимо
    // неё не попала бы ни в отмену, ни в номер версии — то есть ни в тело дома,
    // ни в коллайдер.
    // МУТАЦИЯ — ПО ОТПУСКАНИЮ ползунка, не по каждому кадру перетаскивания
    // (аудит 20.08, находка 7): запись на каждом кадре пересобирала меш и
    // физическое тело шестьдесят раз в секунду и засоряла отмену сотней шагов
    // на одно движение руки. Пока палец на ползунке, число живёт в live —
    // ImGui ведёт положение от мыши, и рука видит цифру сразу.
    const auto number = [&](const char* key, const char* caption, float lo, float hi,
                            float fallback) {
        static float live = 0.0f; // активный ползунок один на весь ImGui
        const std::string raw = session.graph().param(id, key);
        float value = raw.empty() ? fallback : std::strtof(raw.c_str(), nullptr);
        if (ImGui::SliderFloat(EditorUi::tr(caption), &value, lo, hi, "%.3f")) {
            live = value;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            const float v = live;
            (void)session.mutate("свойство элемента", [&](world::HouseGraph& g) {
                return g.set_param(id, key, house_num(v));
            });
        }
    };
    // МАТЕРИАЛ РИСУЕТСЯ НИЖЕ ЗАПОЛНЕНИЯ (жалоба 20.08 «кирпичей нет в режиме
    // выбора»: карточки кладки тонули под сеткой свотчей за прокруткой —
    // важное обязано стоять первым). Клик по картинке правит объект СРАЗУ:
    // версия графа растёт, тело и коллайдер пересобираются тем же кадром.
    const auto material_block = [&] {
        const std::string m = session.graph().param(id, "mat");
        const std::string t = session.graph().param(id, "tone");
        const std::string c = session.graph().param(id, "paint");
        int mi = m.empty() ? (line ? 0 : 5) : std::atoi(m.c_str());
        int ti = t.empty() ? (line ? 1 : 0) : std::atoi(t.c_str());
        int ci = c.empty() ? 0 : std::atoi(c.c_str());
        mi = std::clamp(mi, 0, 8);
        ti = std::clamp(ti, 0, 3);
        ci = std::clamp(ci, 0, world::HOUSE_PAINT_COUNT - 1);
        if (draw_material_grid(g_selected_world, "sel.mat", mi, ti, &ci)) {
            (void)session.mutate("материал элемента", [&](world::HouseGraph& g) {
                (void)g.set_param(id, "mat", std::to_string(mi));
                (void)g.set_param(id, "paint", std::to_string(ci));
                return g.set_param(id, "tone", std::to_string(ti));
            });
        }
    };
    if (line) {
        // ФОРМА ПАЛКИ (заказ 19.08): круг, квадрат, шести- и восьмигранник —
        // это те профили, которые умеет построитель (form/sides).
        {
            const std::string f = session.graph().param(id, "form");
            const std::string n = session.graph().param(id, "sides");
            int fi = 0;
            if (f == "square") {
                fi = 1;
            } else if (f == "plank") {
                fi = 6;
            } else {
                for (int k = 2; k <= 5; ++k) {
                    if (n == std::to_string(HOUSE_FORM_SIDES[k])) {
                        fi = k;
                    }
                }
            }
            if (ImGui::Combo(EditorUi::tr("house.form"), &fi, HOUSE_FORMS, 7)) {
                (void)session.mutate("форма палки", [&](world::HouseGraph& g) {
                    (void)g.set_param(id, "form",
                                      fi == 1 ? "square"
                                              : (fi == 6 ? "plank" : "round"));
                    (void)g.set_param(id, "sides",
                                      (fi >= 2 && fi <= 5)
                                          ? std::to_string(HOUSE_FORM_SIDES[fi])
                                          : "0");
                    // Наследный лестничный флаг гасится: марш — раздел стен.
                    return g.set_param(id, "stairs", "0");
                });
            }
            number("angle_z", "house.spin", 0.0f, 90.0f, 0.0f);
        }
        material_block();
        number("radius", "house.radius", 0.02f, 1.0f, config::HOUSE_LINE_RADIUS_DEFAULT);
    } else {
        number("thickness", "house.thickness", 0.02f, 1.0f,
               config::HOUSE_SURFACE_THICKNESS_DEFAULT);
        if (!e->closed) {
            number("height", "house.height", 0.1f, 12.0f, 2.5f);
        } else {
            ImGui::TextDisabled("%s", EditorUi::tr("house.height.chain"));
        }
        number("tex_deg", "house.tex", 0.0f, 360.0f, 0.0f);
        // ОБШИВКА ПО РАСКЛАДКЕ: доски, раскосы и окна «сколько влезло» —
        // геометрией поверх несущей пластины. Окна НЕ масштабируются; влезло
        // меньше, чем просили, — раскладка скажет это находкой в журнале.
        // ЗАПОЛНЕНИЕ — ПЕРВЫМ, материал ниже.
        if (e->closed) {
            // ПОЛ: срез или паркет. Две кнопки-карточки, как у стен.
            const std::string fl = session.graph().param(id, "fill");
            const int fnow = fl.empty() ? 0 : std::atoi(fl.c_str());
            // ПОКРЫТИЯ КОНТУРА: срез, паркет, лестничный марш (fill 0/5/6).
            // Марш живёт здесь, а не в формах палки: «лестница же на 4 точках
            // держится» (правка 20.08).
            static constexpr int FLOOR_FILL[5] = {0, 5, 6, 7, 8};
            static const char* FLOOR_KEY[5] = {"house.floor.plain",
                                               "house.floor.parquet",
                                               "house.floor.stairs",
                                               "house.floor.shingle",
                                               "house.floor.tile"};
            for (int v = 0; v < 5; ++v) {
                if (v != 0 && (v % 2) != 0) {
                    ImGui::SameLine(); // по две в ряд — пять в строку не влезали
                }
                const bool sel = FLOOR_FILL[v] == fnow;
                if (sel) {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                          ImVec4(0.9f, 0.77f, 0.28f, 1.0f));
                }
                if (ImGui::Button(EditorUi::tr(FLOOR_KEY[v]))) {
                    (void)session.mutate("покрытие пола", [&](world::HouseGraph& g) {
                        return g.set_param(id, "fill",
                                           std::to_string(FLOOR_FILL[v]));
                    });
                }
                if (sel) {
                    ImGui::PopStyleColor();
                }
            }
            // МАРШ: сплошной / доски с зазорами / каменные блоки (20.08).
            if (fnow == 6) {
                const std::string op = session.graph().param(id, "open");
                const int onow = op.empty() ? 0 : std::atoi(op.c_str());
                static const char* OPEN_KEY[3] = {"house.stairs.solid",
                                                  "house.stairs.planks",
                                                  "house.stairs.blocks"};
                for (int v = 0; v < 3; ++v) {
                    if (v != 0) {
                        ImGui::SameLine();
                    }
                    const bool sel2 = v == onow;
                    if (sel2) {
                        ImGui::PushStyleColor(ImGuiCol_Button,
                                              ImVec4(0.9f, 0.77f, 0.28f, 1.0f));
                    }
                    if (ImGui::Button(EditorUi::tr(OPEN_KEY[v]))) {
                        (void)session.mutate("вид марша", [&](world::HouseGraph& g) {
                            return g.set_param(id, "open", std::to_string(v));
                        });
                    }
                    if (sel2) {
                        ImGui::PopStyleColor();
                    }
                }
            }
            // БАЛКИ ПОД ПОТОЛКОМ — кнопка-переключатель (каталог интерьеров).
            {
                const bool on = session.graph().param(id, "beams") == "1";
                if (on) {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                          ImVec4(0.9f, 0.77f, 0.28f, 1.0f));
                }
                if (ImGui::Button(EditorUi::tr("house.beams"))) {
                    (void)session.mutate("балки", [&](world::HouseGraph& g) {
                        return g.set_param(id, "beams", on ? "0" : "1");
                    });
                }
                if (on) {
                    ImGui::PopStyleColor();
                }
            }
        }
        if (!e->closed) {
            const bool clad = session.graph().param(id, "clad") == "1";
            const std::string fl = session.graph().param(id, "fill");
            const int fill_now = fl.empty() ? 0 : std::atoi(fl.c_str());
            const std::string w = session.graph().param(id, "windows");
            int wins = w.empty() ? 0 : std::atoi(w.c_str());
            const int card = fill_to_card(fill_now, clad, wins);
            if (const int picked = draw_fill_cards(g_selected_world, "sel.fill", card);
                picked >= 0) {
                int nfill = fill_now;
                bool nclad = clad;
                int nwins = wins;
                card_to_fill(picked, nfill, nclad, nwins);
                (void)session.mutate("заполнение стены", [&](world::HouseGraph& g) {
                    (void)g.set_param(id, "clad", nclad ? "1" : "0");
                    (void)g.set_param(id, "fill", std::to_string(nfill));
                    return g.set_param(id, "windows", std::to_string(nwins));
                });
            }
            if (clad || fill_now >= 2) {
                if (ImGui::SliderInt(EditorUi::tr("house.windows"), &wins, 0, 6)) {
                    (void)session.mutate("окна", [&](world::HouseGraph& g) {
                        return g.set_param(id, "windows", std::to_string(wins));
                    });
                }
                const bool dooro = session.graph().param(id, "doors") == "1";
                if (dooro) {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                          ImVec4(0.9f, 0.77f, 0.28f, 1.0f));
                }
                if (ImGui::Button(EditorUi::tr("house.dooropen"))) {
                    (void)session.mutate("дверной проём", [&](world::HouseGraph& g) {
                        return g.set_param(id, "doors", dooro ? "0" : "1");
                    });
                }
                if (dooro) {
                    ImGui::PopStyleColor();
                }
            }
        }
        // ИЗНОС И ДЕТАЛИ (заказ 20.08: «износ, старение, больше мелких
        // деталей»). Износ 0..1 — глубже дрожь кладки, щербины, мох по низу,
        // сильный уводит тон в выветренный ряд. Детали — кнопки-переключатели.
        number("wear", "house.wear", 0.0f, 1.0f, 0.0f);
        if (!e->closed) {
            ImGui::TextDisabled("%s", EditorUi::tr("house.det"));
            const auto toggle = [&](const char* key, const char* caption,
                                    bool first) {
                if (!first) {
                    ImGui::SameLine();
                }
                const bool on = session.graph().param(id, key) == "1";
                if (on) {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                          ImVec4(0.9f, 0.77f, 0.28f, 1.0f));
                }
                if (ImGui::Button(EditorUi::tr(caption))) {
                    (void)session.mutate("деталь стены", [&](world::HouseGraph& g) {
                        return g.set_param(id, key, on ? "0" : "1");
                    });
                }
                if (on) {
                    ImGui::PopStyleColor();
                }
            };
            toggle("logends", "house.det.logends", true);
            toggle("shutters", "house.det.shutters", false);
            toggle("porch", "house.det.porch", false);
            toggle("plinth", "house.det.plinth", false);
        }
        material_block();
        // ПОЛКА СТИЛЕЙ: снять отделку с выбранного / примерить сохранённую.
        {
            ImGui::PushID("house.styles");
            static char style_name[48] = "мой-стиль";
            static std::vector<std::string> shelf;
            static bool scanned = false;
            static int pick = 0;
            if (!scanned) {
                shelf = list_styles();
                scanned = true;
            }
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputText("##stylename", style_name, sizeof(style_name));
            ImGui::SameLine();
            if (ImGui::Button(EditorUi::tr("house.style.save"))) {
                save_style(session, id, style_name);
                shelf = list_styles();
            }
            if (!shelf.empty()) {
                pick = std::clamp(pick, 0, static_cast<int>(shelf.size()) - 1);
                ImGui::SetNextItemWidth(140.0f);
                if (ImGui::BeginCombo("##styles", shelf[static_cast<std::size_t>(pick)].c_str())) {
                    // Открытие комбо перечитывает папку: файл, положенный
                    // руками, раньше не появлялся до перезапуска.
                    shelf = list_styles();
                    pick = std::clamp(pick, 0, static_cast<int>(shelf.size()) - 1);
                    for (int i = 0; i < static_cast<int>(shelf.size()); ++i) {
                        if (ImGui::Selectable(shelf[static_cast<std::size_t>(i)].c_str(), i == pick)) {
                            pick = i;
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::Button(EditorUi::tr("house.style.apply"))) {
                    apply_style(session, id, shelf[static_cast<std::size_t>(pick)]);
                }
            }
            ImGui::PopID();
        }
        // ДВЕРЬ — СВОЙСТВО СТЕНЫ, а не отдельная деталь (заказ 19.08: «ставлю
        // стену и меняю ей свойство на дверь... и так я убираю необходимость
        // делать стены специально с дверьми»). Петля — пара соседних якорей
        // обхода, листается по кругу; дверь качается вокруг неё прямо в
        // редакторе, чтобы выбор петли был виден, а не угадан.
        {
            bool is_door = session.graph().param(id, "door") == "1";
            if (ImGui::Checkbox(EditorUi::tr("house.door"), &is_door)) {
                (void)session.mutate("дверь", [&](world::HouseGraph& g) {
                    return g.set_param(id, "door", is_door ? "1" : "0");
                });
            }
            if (is_door && e->refs.size() >= 2) {
                const std::size_t n = e->refs.size();
                std::size_t hinge = 0;
                if (const std::string h = session.graph().param(id, "hinge"); !h.empty()) {
                    hinge = static_cast<std::size_t>(std::atoi(h.c_str())) % n;
                }
                ImGui::Text("%s v%u–v%u", EditorUi::tr("house.hinge"),
                            static_cast<unsigned>(e->refs[hinge]),
                            static_cast<unsigned>(e->refs[(hinge + 1) % n]));
                ImGui::SameLine();
                if (ImGui::SmallButton(EditorUi::tr("house.hinge.next"))) {
                    const std::size_t next = (hinge + 1) % n;
                    (void)session.mutate("петля двери", [&](world::HouseGraph& g) {
                        return g.set_param(id, "hinge", std::to_string(next));
                    });
                }
            }
        }
        if (ImGui::Button(EditorUi::tr("house.flip"))) {
            const bool now = e->facing_flipped;
            (void)session.mutate("развернул лицо", [&](world::HouseGraph& g) {
                return g.set_facing(id, !now);
            });
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorUi::tr("house.delete.elem"))) {
        (void)session.delete_selection();
    }
}

} // namespace

void draw_house_selection_panel(HouseSession& session, const ToolWorld* world) {
    g_selected_world = world;
    // ПАНЕЛЬ ВЫБРАННОГО — ДЛЯ ИНСТРУМЕНТА ВЫБОРА (заказ 19.08: «когда я выбираю
    // объект, справа должно рисоваться меню свойств этого объекта, а не меню
    // инструмента»). Тот же код, что в панелях постройки: третья копия этих
    // полей разошлась бы с первыми двумя.
    draw_grid_and_coords(session);
    ImGui::Separator();
    draw_selected_element(session);
}

/// ПОЛКА ГОТОВЫХ ПОСТРОЕК — В ПАНЕЛИ ИНСТРУМЕНТА СТЕН (правка 20.08: «полка
/// в инструменте выбора, доступная только при выделении, — бред; готовые
/// дома должны быть в секции постройки»). Дом встаёт ПОД ПРИЦЕЛ, к узлу
/// сетки, если она включена; запись — в секцию [house] сцены.
void draw_house_shelf(const ToolWorld* world) {
    if (world != nullptr && world->house_assets && world->place_house_at_aim) {
        ImGui::PushID("house.shelf");
        static std::vector<std::string> shelf;
        static bool scanned = false;
        static int pick = 0;
        static float yaw_deg = 0.0f;
        // Скан — по кнопке и ОДИН раз при первом показе: пустая папка
        // сканировалась каждый кадр (UX-аудит, дефект 3).
        if (!scanned || ImGui::Button(EditorUi::tr("house.shelf.refresh"))) {
            shelf = world->house_assets();
            scanned = true;
        }
        if (shelf.empty()) {
            ImGui::TextDisabled("%s", EditorUi::tr("house.shelf.empty"));
        } else {
            pick = std::clamp(pick, 0, static_cast<int>(shelf.size()) - 1);
            ImGui::SetNextItemWidth(180.0f);
            if (ImGui::BeginCombo("##shelf", shelf[static_cast<std::size_t>(pick)].c_str())) {
                for (int i = 0; i < static_cast<int>(shelf.size()); ++i) {
                    if (ImGui::Selectable(shelf[static_cast<std::size_t>(i)].c_str(), i == pick)) {
                        pick = i;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SliderFloat(EditorUi::tr("house.shelf.yaw"), &yaw_deg, 0.0f,
                               270.0f, "%.0f°");
            if (ImGui::Button(EditorUi::tr("house.shelf.place"))) {
                world->place_house_at_aim(shelf[static_cast<std::size_t>(pick)], yaw_deg);
            }
            if (world->remove_last_house) {
                ImGui::SameLine();
                if (ImGui::Button(EditorUi::tr("house.shelf.remove"))) {
                    world->remove_last_house();
                }
            }
            if (world->unpack_house_at_aim
                && ImGui::Button(EditorUi::tr("house.shelf.unpack"))) {
                world->unpack_house_at_aim();
            }
        }
        ImGui::PopID();
    }
}

void HouseVertexTool::draw_settings() {
    g_selected_world = world_;
    // ОТКАЗ — ПЕРВОЙ СТРОКОЙ (UX-аудит: красная строка жила в подвале, за
    // прокруткой — ровно там, куда не смотрят в момент отказа).
    draw_refusal(refusal_);
    // «ЗАГОТОВКА», А НЕ «ВЫБРАННОЕ» — долг 4 второго аудита. Ползунки ниже
    // описывают, каким будет СЛЕДУЮЩИЙ элемент; такие же поля выбранного стоят
    // ниже под своим заголовком, и без надписей эти два блока читались как
    // один, отвечающий непонятно про что (конфликт ID из ImGui был симптомом).
    ImGui::SeparatorText(EditorUi::tr("house.head.draft"));
    // ВЫСОТА НАД ЗЕМЛЁЙ — ГЛАВНЫЙ ОРГАН ЭТОГО ИНСТРУМЕНТА. Ноль заземляет
    // вершину, больше нуля вешает её в воздухе, и тогда у неё появляется
    // пунктирный отвес: «я буду видеть, над какой точкой ставлю свой объект».
    // ТОТ ЖЕ ОРГАН, ЧТО И КОЛЕСО МЫШИ, а не второе состояние рядом: ползунок
    // зовёт set_pull_m, зажим пределов живёт внутри него.
    float pull = pull_m_;
    if (ImGui::SliderFloat(EditorUi::tr("house.air"), &pull, 0.0f, HOUSE_PULL_MAX_M,
                           "%.2f m")) {
        set_pull_m(pull);
    }
    if (pull_m_ <= HOUSE_AIR_EPS_M) {
        ImGui::TextDisabled("%s", EditorUi::tr("house.air.ground"));
    } else {
        ImGui::TextDisabled("%s", EditorUi::tr("house.air.plumb"));
    }

    ImGui::Spacing();
    if (session_ == nullptr) {
        ImGui::TextDisabled("%s", EditorUi::tr("house.hint.nomodel"));
        return;
    }
    ImGui::Text("%s %zu · %s %zu", EditorUi::tr("house.vertices"),
                session_->graph().vertex_count(), EditorUi::tr("house.elements"),
                session_->graph().element_count());

    const world::VertexId sel = session_->selected_vertex();
    if (sel == world::NO_VERTEX) {
        ImGui::TextDisabled("%s", EditorUi::tr("house.nosel"));
    } else {
        const world::Vertex* v = session_->graph().vertex(sel);
        const char* how = "?";
        if (v != nullptr) {
            how = v->anchoring == world::Anchoring::OnGround  ? EditorUi::tr("house.anch.ground")
                : v->anchoring == world::Anchoring::OnEdge    ? EditorUi::tr("house.anch.edge")
                                                              : EditorUi::tr("house.anch.free");
        }
        // Числа координат живут ниже РЕДАКТИРУЕМЫМ InputFloat3 (сетка/
        // координаты); текст-дубль и вторая кнопка удаления сняты по
        // UX-аудиту 20.08 (то же число дважды, «Удалить якорь» дублировал
        // «Убрать элемент»).
        ImGui::Text("v%u · %s", static_cast<unsigned>(sel), how);
        ImGui::SameLine();
        ImGui::Text("%s %zu", EditorUi::tr("house.incident"),
                    session_->lit_elements().size());
    }
    // СВОЙСТВА ВЫБРАННОГО — В КАЖДОЙ ПАНЕЛИ ПОСТРОЙКИ. Человек выбирает стену
    // тем инструментом, который сейчас в руке, и искать её свойства в чужой
    // панели ему незачем.
    if (session_ != nullptr) {
        draw_grid_and_coords(*session_);
        ImGui::SeparatorText(EditorUi::tr("house.head.selected"));
        draw_selected_element(*session_);
    }
}

void HouseLineTool::draw_settings() {
    g_selected_world = world_;
    // ОТКАЗ — ПЕРВОЙ СТРОКОЙ (UX-аудит: красная строка жила в подвале, за
    // прокруткой — ровно там, куда не смотрят в момент отказа).
    draw_refusal(refusal_);
    // «ЗАГОТОВКА», А НЕ «ВЫБРАННОЕ» — долг 4 второго аудита. Ползунки ниже
    // описывают, каким будет СЛЕДУЮЩИЙ элемент; такие же поля выбранного стоят
    // ниже под своим заголовком, и без надписей эти два блока читались как
    // один, отвечающий непонятно про что (конфликт ID из ImGui был симптомом).
    ImGui::SeparatorText(EditorUi::tr("house.head.draft"));
    // ЗАГОТОВКА ВИДНА В МЕНЮ ИНСТРУМЕНТА (жалоба 19.08: «не вижу ничего нового
    // в меню объекта») — материал, тон и форма следующей прямой выбираются
    // ЗДЕСЬ и штампуются в элемент при создании.
    (void)draw_material_grid(world_, "line.draft", mat_, tone_, &paint_);
    ImGui::Combo(EditorUi::tr("house.form"), &form_, HOUSE_FORMS, 7);
    ImGui::SliderFloat(EditorUi::tr("house.radius"), &radius_m_, 0.02f, 1.0f, "%.3f m");
    // ПОВОРОТ СЕЧЕНИЯ (заказ 20.08: «все квадратные балки имеют грани вдоль
    // осей мира, а я их поворачивать хочу»). Механика angle_z жила в модели с
    // первого дня — у неё просто не было ручки.
    if (form_ != 0) {
        ImGui::SliderFloat(EditorUi::tr("house.spin"), &spin_deg_, 0.0f, 90.0f, "%.0f°");
    }

    // ЗАЖИМ ДЛИНЫ — ТРИ ПОЛОЖЕНИЯ, А НЕ ГАЛОЧКА. «Механика клипа длины прямой
    // до ближайшего сверху / снизу НА ВЫБОР якоря»: направление выбирает
    // человек, потому что ближайший вперёд и ближайший назад — разные ответы, и
    // угадывать за него значит промахиваться в половине случаев.
    ImGui::Text("%s", EditorUi::tr("house.clamp"));
    int mode = static_cast<int>(clamp_);
    ImGui::RadioButton(EditorUi::tr("house.clamp.none"), &mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton(EditorUi::tr("house.clamp.above"), &mode, 1);
    ImGui::SameLine();
    ImGui::RadioButton(EditorUi::tr("house.clamp.below"), &mode, 2);
    clamp_ = static_cast<HouseClamp>(mode);

    if (clamp_hit_.found) {
        ImGui::Text("%s %.2f m (v%u)", EditorUi::tr("house.clamp.now"),
                    static_cast<double>(clamp_hit_.length_m),
                    static_cast<unsigned>(clamp_hit_.at));
    } else {
        ImGui::TextDisabled("%s", EditorUi::tr("house.clamp.free"));
    }

    ImGui::Spacing();
    if (last_ != world::NO_ELEMENT && session_ != nullptr) {
        ImGui::Text("%s e%u", EditorUi::tr("house.last"), static_cast<unsigned>(last_));
    }
    // СВОЙСТВА ВЫБРАННОГО — В КАЖДОЙ ПАНЕЛИ ПОСТРОЙКИ. Человек выбирает стену
    // тем инструментом, который сейчас в руке, и искать её свойства в чужой
    // панели ему незачем.
    if (session_ != nullptr) {
        draw_grid_and_coords(*session_);
        ImGui::SeparatorText(EditorUi::tr("house.head.selected"));
        draw_selected_element(*session_);
    }
}

void HouseSurfaceTool::draw_settings() {
    g_selected_world = world_;
    // ОТКАЗ — ПЕРВОЙ СТРОКОЙ (UX-аудит: красная строка жила в подвале, за
    // прокруткой — ровно там, куда не смотрят в момент отказа).
    draw_refusal(refusal_);
    // ГОТОВЫЕ ПОСТРОЙКИ — ЗДЕСЬ, в секции постройки (правка 20.08), и без
    // каких-либо условий на выделение.
    ImGui::SeparatorText(EditorUi::tr("house.shelf"));
    draw_house_shelf(world_);
    // «ЗАГОТОВКА», А НЕ «ВЫБРАННОЕ» — долг 4 второго аудита. Ползунки ниже
    // описывают, каким будет СЛЕДУЮЩИЙ элемент; такие же поля выбранного стоят
    // ниже под своим заголовком, и без надписей эти два блока читались как
    // один, отвечающий непонятно про что (конфликт ID из ImGui был симптомом).
    ImGui::SeparatorText(EditorUi::tr("house.head.draft"));
    // ЗАГОТОВКА ВИДНА В МЕНЮ ИНСТРУМЕНТА (жалоба 19.08: «не вижу ничего нового
    // в меню объекта»): материал, тон, обшивка и окна следующей поверхности
    // выбираются здесь и штампуются в элемент при подтверждении.
    (void)draw_material_grid(world_, "surf.draft", mat_, tone_, &paint_);
    // ЗАПОЛНЕНИЕ — КАРТОЧКАМИ, НЕ ГАЛОЧКОЙ (заказ 19.08: «галочки не удобны,
    // хочу картинки-примеры»). Карточка и есть выбор: гладкая, фахверк,
    // фахверк с окнами.
    // Карточка выбирает ПРАВИЛО СБОРКИ: 0 гладкая, 1-2 фахверк, 3 кирпич,
    // 4 блоки. Кладка (заказ 20.08) — настоящие кусочки с перевязкой, не
    // текстура.
    const int card = fill_to_card(fill_, clad_, windows_);
    if (const int picked = draw_fill_cards(world_, "surf.fill", card); picked >= 0) {
        bool clad_b = clad_;
        card_to_fill(picked, fill_, clad_b, windows_);
        clad_ = clad_b;
    }
    if (clad_ || fill_ >= 2) {
        ImGui::SliderInt(EditorUi::tr("house.windows"), &windows_, 0, 6);
        // ДВЕРНОЙ ПРОЁМ — КНОПКА-ПЕРЕКЛЮЧАТЕЛЬ, не галочка: подсвечена, пока
        // включена. Дверь-СТВОРКА (качается) остаётся свойством выбранной
        // стены; здесь — именно проём в кладке от пола.
        const bool on = doors_ > 0;
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.77f, 0.28f, 1.0f));
        }
        if (ImGui::Button(EditorUi::tr("house.dooropen"))) {
            doors_ = on ? 0 : 1;
        }
        if (on) {
            ImGui::PopStyleColor();
        }
    }
    if (session_ == nullptr) {
        ImGui::TextDisabled("%s", EditorUi::tr("house.hint.nomodel"));
        return;
    }
    ImGui::Text("%s %zu", EditorUi::tr("house.walk"), refs_.size());

    // КУДА СМОТРИТ ЛИЦО — ДО ПОДТВЕРЖДЕНИЯ И ЧИСЛАМИ. Стрелка в мире отвечает
    // на тот же вопрос, но её видно не с каждого ракурса, а порядок обхода
    // назад не отматывается: текстура ляжет на изнанку, и узнается это на
    // готовом доме.
    glm::vec3 n{0.0f};
    if (draft_normal(n)) {
        ImGui::Text("%s (%.2f %.2f %.2f)", EditorUi::tr("house.facing"),
                    static_cast<double>(n.x), static_cast<double>(n.y),
                    static_cast<double>(n.z));
    } else {
        ImGui::TextDisabled("%s", EditorUi::tr("house.facing.none"));
    }
    ImGui::Checkbox(EditorUi::tr("house.flip"), &flipped_);

    ImGui::SliderFloat(EditorUi::tr("house.thickness"), &thickness_m_, 0.02f, 1.0f, "%.3f m");
    ImGui::SliderFloat(EditorUi::tr("house.height"), &height_m_, 0.1f, 12.0f, "%.2f m");
    ImGui::TextDisabled("%s", EditorUi::tr("house.height.chain"));
    ImGui::SliderFloat(EditorUi::tr("house.tex"), &tex_deg_, 0.0f, 360.0f, "%.0f°");

    ImGui::Spacing();
    ToolWorld& w = world_ != nullptr ? *world_ : no_world();
    if (ImGui::Button(EditorUi::tr("house.make.chain"))) {
        (void)confirm(w);
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorUi::tr("house.make.contour"))) {
        // ЗАМКНУТЬ И СОЗДАТЬ — ТОТ ЖЕ ЖЕСТ, ЧТО ЩЕЛЧОК ПО ПЕРВОМУ ЯКОРЮ, и он
        // тот же метод: closed выставляется здесь, а создаёт confirm(). Второе
        // место, которое само собирает элемент, забыло бы про facing или про
        // толщину — вопрос только в том, когда.
        closed_ = true;
        (void)confirm(w);
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorUi::tr("house.walk.back"))) {
        undo_last();
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorUi::tr("house.walk.drop"))) {
        clear_draft();
    }
    // СВОЙСТВА ВЫБРАННОГО — В КАЖДОЙ ПАНЕЛИ ПОСТРОЙКИ. Человек выбирает стену
    // тем инструментом, который сейчас в руке, и искать её свойства в чужой
    // панели ему незачем.
    if (session_ != nullptr) {
        draw_grid_and_coords(*session_);
        ImGui::SeparatorText(EditorUi::tr("house.head.selected"));
        draw_selected_element(*session_);
    }
}

} // namespace dfn::app
