/*
Created: 18:08:2026 - 18:02:11
Last updated: 20:08:2026 - 01:47:30
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
*/

#include "engine/editor/sources/EditorToolHouse.h"
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
static const char* HOUSE_FORMS[5] = {"круглая", "квадратная", "шестигранная",
                                     "восьмигранная", "лестница"};

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
                               int& tone) {
    bool changed = false;
    ImGui::PushID(id);
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

/// ТРИ КАРТОЧКИ ЗАПОЛНЕНИЯ СТЕНЫ: гладкая, фахверк, фахверк с окнами.
/// Возвращает -1 (не трогали) или выбранный вариант.
static int draw_fill_cards(const ToolWorld* world, const char* id, int current) {
    int picked = -1;
    ImGui::PushID(id);
    static const char* NAMES[5] = {"гладкая", "фахверк (доски и раскосы)",
                                   "фахверк с окнами", "кирпичная кладка",
                                   "каменные блоки"};
    for (int v = 0; v < 5; ++v) {
        if (v != 0) {
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
    const auto number = [&](const char* key, const char* caption, float lo, float hi,
                            float fallback) {
        const std::string raw = session.graph().param(id, key);
        float value = raw.empty() ? fallback : std::strtof(raw.c_str(), nullptr);
        if (ImGui::SliderFloat(EditorUi::tr(caption), &value, lo, hi, "%.3f")) {
            const float v = value;
            (void)session.mutate("свойство элемента", [&](world::HouseGraph& g) {
                return g.set_param(id, key, house_num(v));
            });
        }
    };
    // МАТЕРИАЛ И ТОН — У ЛЮБОГО ЭЛЕМЕНТА (заказ 19.08: «выбирать текстуры для
    // палок, стен»). Порядок пунктов — ординалы PartSurface/PartTone: плитку
    // режет отрисовка, и число здесь обязано совпадать с числом там.
    {
        const std::string m = session.graph().param(id, "mat");
        const std::string t = session.graph().param(id, "tone");
        int mi = m.empty() ? (line ? 0 : 5) : std::atoi(m.c_str());
        int ti = t.empty() ? (line ? 1 : 0) : std::atoi(t.c_str());
        mi = std::clamp(mi, 0, 8);
        ti = std::clamp(ti, 0, 3);
        // КЛИК ПО КАРТИНКЕ ПРАВИТ ОБЪЕКТ СРАЗУ: версия графа растёт, тело и
        // коллайдер пересобираются тем же кадром — «кликнул и ничего не
        // поменялось» больше невозможно по устройству.
        if (draw_material_grid(g_selected_world, "sel.mat", mi, ti)) {
            (void)session.mutate("материал элемента", [&](world::HouseGraph& g) {
                (void)g.set_param(id, "mat", std::to_string(mi));
                return g.set_param(id, "tone", std::to_string(ti));
            });
        }
    }
    if (line) {
        // ФОРМА ПАЛКИ (заказ 19.08): круг, квадрат, шести- и восьмигранник —
        // это те профили, которые умеет построитель (form/sides).
        {
            const std::string f = session.graph().param(id, "form");
            const std::string n = session.graph().param(id, "sides");
            int fi = 0;
            if (f == "square") {
                fi = 1;
            } else if (n == "6") {
                fi = 2;
            } else if (n == "8") {
                fi = 3;
            }
            if (ImGui::Combo(EditorUi::tr("house.form"), &fi, HOUSE_FORMS, 5)) {
                (void)session.mutate("форма палки", [&](world::HouseGraph& g) {
                    (void)g.set_param(id, "form", fi == 1 ? "square" : "round");
                    (void)g.set_param(id, "sides",
                                      fi == 2 ? "6" : (fi == 3 ? "8" : "0"));
                    return g.set_param(id, "stairs", fi == 4 ? "1" : "0");
                });
            }
            number("angle_z", "house.spin", 0.0f, 90.0f, 0.0f);
        }
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
        if (e->closed) {
            // ПОЛ: срез или паркет. Две кнопки-карточки, как у стен.
            const std::string fl = session.graph().param(id, "fill");
            const bool parquet = !fl.empty() && std::atoi(fl.c_str()) == 5;
            for (int v = 0; v < 2; ++v) {
                if (v != 0) {
                    ImGui::SameLine();
                }
                const bool sel = (v == 1) == parquet;
                if (sel) {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                          ImVec4(0.9f, 0.77f, 0.28f, 1.0f));
                }
                if (ImGui::Button(v == 0 ? EditorUi::tr("house.floor.plain")
                                         : EditorUi::tr("house.floor.parquet"))) {
                    (void)session.mutate("покрытие пола", [&](world::HouseGraph& g) {
                        return g.set_param(id, "fill", v == 1 ? "5" : "0");
                    });
                }
                if (sel) {
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
            const int card = fill_now >= 2 ? fill_now + 1
                                           : (clad ? (wins > 0 ? 2 : 1) : 0);
            if (const int picked = draw_fill_cards(g_selected_world, "sel.fill", card);
                picked >= 0) {
                (void)session.mutate("заполнение стены", [&](world::HouseGraph& g) {
                    (void)g.set_param(id, "clad",
                                      (picked == 1 || picked == 2) ? "1" : "0");
                    (void)g.set_param(id, "fill",
                                      picked >= 3 ? std::to_string(picked - 1) : "0");
                    return g.set_param(id, "windows", picked == 2 ? "2" : "0");
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

void HouseVertexTool::draw_settings() {
    g_selected_world = world_;
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
        const glm::vec3 p = session_->vertex_world(sel);
        ImGui::Text("v%u · %s · (%.2f %.2f %.2f)", static_cast<unsigned>(sel), how,
                    static_cast<double>(p.x), static_cast<double>(p.y),
                    static_cast<double>(p.z));
        // ПОДСВЕЧЕННЫЕ ЭЛЕМЕНТЫ НАЗЫВАЮТСЯ И СЛОВАМИ. Свечение на экране
        // отвечает «какие», список отвечает «сколько», и второе видно, даже
        // когда камера смотрит в другую сторону.
        ImGui::Text("%s %zu", EditorUi::tr("house.incident"),
                    session_->lit_elements().size());
        if (ImGui::Button(EditorUi::tr("house.delete"))) {
            (void)delete_selected();
        }
    }
    draw_refusal(refusal_);
    // СВОЙСТВА ВЫБРАННОГО — В КАЖДОЙ ПАНЕЛИ ПОСТРОЙКИ. Человек выбирает стену
    // тем инструментом, который сейчас в руке, и искать её свойства в чужой
    // панели ему незачем.
    if (session_ != nullptr) {
        ImGui::SeparatorText(EditorUi::tr("house.head.selected"));
        draw_grid_and_coords(*session_);
        draw_selected_element(*session_);
    }
}

void HouseLineTool::draw_settings() {
    g_selected_world = world_;
    // «ЗАГОТОВКА», А НЕ «ВЫБРАННОЕ» — долг 4 второго аудита. Ползунки ниже
    // описывают, каким будет СЛЕДУЮЩИЙ элемент; такие же поля выбранного стоят
    // ниже под своим заголовком, и без надписей эти два блока читались как
    // один, отвечающий непонятно про что (конфликт ID из ImGui был симптомом).
    ImGui::SeparatorText(EditorUi::tr("house.head.draft"));
    // ЗАГОТОВКА ВИДНА В МЕНЮ ИНСТРУМЕНТА (жалоба 19.08: «не вижу ничего нового
    // в меню объекта») — материал, тон и форма следующей прямой выбираются
    // ЗДЕСЬ и штампуются в элемент при создании.
    (void)draw_material_grid(world_, "line.draft", mat_, tone_);
    ImGui::Combo(EditorUi::tr("house.form"), &form_, HOUSE_FORMS, 5);
    ImGui::SliderFloat(EditorUi::tr("house.radius"), &radius_m_, 0.02f, 1.0f, "%.3f m");
    // ПОВОРОТ СЕЧЕНИЯ (заказ 20.08: «все квадратные балки имеют грани вдоль
    // осей мира, а я их поворачивать хочу»). Механика angle_z жила в модели с
    // первого дня — у неё просто не было ручки.
    if (form_ != 0 && form_ != 4) {
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
    draw_refusal(refusal_);
    // СВОЙСТВА ВЫБРАННОГО — В КАЖДОЙ ПАНЕЛИ ПОСТРОЙКИ. Человек выбирает стену
    // тем инструментом, который сейчас в руке, и искать её свойства в чужой
    // панели ему незачем.
    if (session_ != nullptr) {
        ImGui::SeparatorText(EditorUi::tr("house.head.selected"));
        draw_grid_and_coords(*session_);
        draw_selected_element(*session_);
    }
}

void HouseSurfaceTool::draw_settings() {
    g_selected_world = world_;
    // «ЗАГОТОВКА», А НЕ «ВЫБРАННОЕ» — долг 4 второго аудита. Ползунки ниже
    // описывают, каким будет СЛЕДУЮЩИЙ элемент; такие же поля выбранного стоят
    // ниже под своим заголовком, и без надписей эти два блока читались как
    // один, отвечающий непонятно про что (конфликт ID из ImGui был симптомом).
    ImGui::SeparatorText(EditorUi::tr("house.head.draft"));
    // ЗАГОТОВКА ВИДНА В МЕНЮ ИНСТРУМЕНТА (жалоба 19.08: «не вижу ничего нового
    // в меню объекта»): материал, тон, обшивка и окна следующей поверхности
    // выбираются здесь и штампуются в элемент при подтверждении.
    (void)draw_material_grid(world_, "surf.draft", mat_, tone_);
    // ЗАПОЛНЕНИЕ — КАРТОЧКАМИ, НЕ ГАЛОЧКОЙ (заказ 19.08: «галочки не удобны,
    // хочу картинки-примеры»). Карточка и есть выбор: гладкая, фахверк,
    // фахверк с окнами.
    // Карточка выбирает ПРАВИЛО СБОРКИ: 0 гладкая, 1-2 фахверк, 3 кирпич,
    // 4 блоки. Кладка (заказ 20.08) — настоящие кусочки с перевязкой, не
    // текстура.
    const int card = fill_ >= 2 ? fill_ + 1 : (clad_ ? (windows_ > 0 ? 2 : 1) : 0);
    if (const int picked = draw_fill_cards(world_, "surf.fill", card); picked >= 0) {
        clad_ = picked == 1 || picked == 2;
        fill_ = picked >= 3 ? picked - 1 : 0;
        windows_ = picked == 2 ? 2 : windows_;
        if (picked == 0) {
            windows_ = 0;
        }
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
    draw_refusal(refusal_);
    // СВОЙСТВА ВЫБРАННОГО — В КАЖДОЙ ПАНЕЛИ ПОСТРОЙКИ. Человек выбирает стену
    // тем инструментом, который сейчас в руке, и искать её свойства в чужой
    // панели ему незачем.
    if (session_ != nullptr) {
        ImGui::SeparatorText(EditorUi::tr("house.head.selected"));
        draw_grid_and_coords(*session_);
        draw_selected_element(*session_);
    }
}

} // namespace dfn::app
