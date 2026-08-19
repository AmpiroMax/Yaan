/*
Created: 18:08:2026 - 18:02:11
Last updated: 20:08:2026 - 12:10:00
Module: engine/editor
File: engine/editor/sources/EditorToolHouse.cpp

Responsibility:
- РЕШЕНИЯ трёх инструментов постройки, объявленных в EditorToolHouse.h: куда
  попал щелчок, что от него изменилось в графе, что нарисовать в мире и что
  сказать подписью. Ни строки про ImGui и ни строки про окно — панели живут в
  EditorToolHouseUi.cpp, и разрез проведён ровно затем, чтобы рукав
  app_editor_house мог спросить всё, что здесь решается (правило 3).

Dependencies:
- Uses: EditorToolHouse.h, engine/world (HouseGraph, HouseFile, HouseMesh), glm.
- Used by: engine/app, tests/app/EditorToolHouseTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ВСЯ ПРАВКА ГРАФА — ЧЕРЕЗ HouseSession::mutate. Она и только она пишет снимок
  для отмены; правка мимо неё создаёт шаг, которого отмена не увидит.
- НОРМАЛЬ ЧЕРНОВИКА СЧИТАЕТСЯ ТЕМ ЖЕ СПОСОБОМ, ЧТО У ГОТОВОГО ЭЛЕМЕНТА
  (world::surface_normal): контур — fit_contour_plane, цепочка — поперечина к
  первому отрезку. Разойдись эти две формулы — и стрелка станет врать ровно в
  тот момент, когда на неё смотрят.
*/
/*
UPD:
- 18:08:2026 - 18:02:11: Создан вместе с EditorToolHouse.h.
- 18:08:2026 - 18:40:24: apply_snapshot поднимает номер жизни графа, снимает выбор и ставит его
  заново ПО МЕСТУ (unique_vertex_at, отказ при двух якорях в одной точке), а
  три инструмента бросают всё, что держали по именам. Причина — измеренная:
  имя, освободившееся при отмене, достаётся ДРУГОЙ точке (v4: 99 м до отмены,
  -77 м после). Контрфакты: без stale() — 4 красных утверждения, без опознания
  по месту — 1.
- 18:08:2026 - 18:56:09: комментарий у apply_snapshot приведён в соответствие с починенным
  читателем: опознание по МЕСТУ обосновано не переименованием (его больше
  нет), а откатом счётчика имён. Полный довод — у revision() в заголовке.
- 18:08:2026 - 18:58:40: Прямая ВВЕРХ: point_on_vertical — ближайшая точка вертикали через якорь к лучу прицела (пара скрещивающихся прямых), вырожденный взгляд вдоль оси назван и держит прежнюю высоту. Подпись называет высоту со знаком.
- 18:08:2026 - 19:44:10: Поиск лучом (сближение луча с точкой и с отрезком, вырожденные случаи названы); призрак якоря один на щелчок, показ и подпись; колесо тянет шарик вдоль луча; магнит на ось.
- 18:08:2026 - 20:26:30: Конец прямой липнет к якорю ПОД ЛУЧОМ (два якоря в воздухе больше не соединяются через пол); прилипание якоря к оси решает прицел, а не подтягивание — стойки ловятся так же, как лежачие брёвна; ray_vs_segment — одно выражение на весь файл; подпись поверхности называет пол и стену до подтверждения.
- 18:08:2026 - 21:12:40: Прямая, отпущенная в пустоте, СТАВИТ ТАМ ЯКОРЬ (решение пользователя: прямая без якоря на конце — бессмыслица); зажим длины садит конец на тот самый якорь, а не на двойника рядом.
- 18:08:2026 - 22:20:15: Якорь двигается вдоль запертой оси; ось общая у прямой и у перетаскивания; стрелка нормали одна на черновик и на готовую стену, рисуется у КАЖДОЙ подсвеченной поверхности из её видного места; стена выбирается тычком в полотно (луч-треугольник), а не только по кромке.
- 18:08:2026 - 23:20:00: Призрак якоря садится в узел сетки; pick_element_ray отдаёт расстояние прицелу.
- 19:08:2026 - 00:12:30: Шарик и отвес призрака уехали из стопки подсветки в свою.
- 19:08:2026 - 00:31:05: Стопка призрака чистится в начале показа: без этой строки шарик добавлялся каждый кадр и рендерер ронял линии пачками.
- 19:08:2026 - 00:48:20: Метка двигаемого якоря вместо призрака: крест по трём осям, формой отличается от шарика.
- 19:08:2026 - 03:22:40: Сетка действует на осях (перетаскивание, прямая, посадка на бревно с пересчётом t); обход стены — жирным пучком со стрелками направления, жёлтые только ВЫБРАННЫЕ якоря (line_color больше не жёлтый).
- 19:08:2026 - 23:58:20: Создание штампует заготовку в params элемента (stamp_draft у прямой, confirm у поверхности); умолчания ключей не пишут.
- 20:08:2026 - 00:58:40: Штамп fill при создании.
- 20:08:2026 - 01:47:30: Штампы spin/stairs/doors при создании.
- 20:08:2026 - 12:10:00: Штамп форм по таблице (3/6/8/12 граней, доска), штамп краски; лестница из форм убрана.
*/

#include "engine/editor/sources/EditorToolHouse.h"

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/geometric.hpp>

namespace dfn::app {
namespace {

using world::Anchoring;
using world::Element;
using world::ElementId;
using world::ElementKind;
using world::GraphResult;
using world::HouseGraph;
using world::NO_ELEMENT;
using world::NO_VERTEX;
using world::Vertex;
using world::VertexId;

constexpr float DEG2RAD = 3.14159265358979323846f / 180.0f;
constexpr float RAD2DEG = 180.0f / 3.14159265358979323846f;
/// Сколько отрезков в кольце шарика. Восемь: девятый на шарике в 10 см уже
/// короче пикселя с той дистанции, с которой шарик вообще виден.
constexpr int BALL_SEGMENTS = 8;

/// Имя элемента так, как его пишет файл (HouseFile: e1, e2, ...). Отказ на
/// удаление называет держателей ИМЕНАМИ ИЗ ФАЙЛА, а не номерами из головы:
/// человек, которому сказали «держит e7», может найти e7 глазами в .dfh.
std::string ename(ElementId id) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "e%u", static_cast<unsigned>(id));
    return buf;
}

/// Ближайшая точка отрезка к точке, и параметр вдоль него.
glm::vec3 closest_on_segment(glm::vec3 a, glm::vec3 b, glm::vec3 p, float& t_out) {
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    if (len2 < 1e-8f) {
        t_out = 0.0f;
        return a;
    }
    const float t = std::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f);
    t_out = t;
    return a + ab * t;
}

} // namespace

// ---------------------------------------------------------------------------
// Отрезки картинки
// ---------------------------------------------------------------------------

void append_ball(std::vector<glm::vec3>& seg, glm::vec3 centre, float radius_m) {
    // ТРИ КОЛЬЦА В ТРЁХ ПЛОСКОСТЯХ. Одно кольцо с любого ракурса вырождается в
    // отрезок, два оставляют направление, с которого шарик выглядит крестом;
    // три читаются как объём при любом повороте камеры.
    const glm::vec3 axes[3][2] = {
        {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    };
    for (const auto& pair : axes) {
        glm::vec3 prev = centre + pair[0] * radius_m;
        for (int i = 1; i <= BALL_SEGMENTS; ++i) {
            const float a = 2.0f * 3.14159265358979323846f
                          * static_cast<float>(i) / static_cast<float>(BALL_SEGMENTS);
            const glm::vec3 cur = centre + pair[0] * (std::cos(a) * radius_m)
                                        + pair[1] * (std::sin(a) * radius_m);
            seg.push_back(prev);
            seg.push_back(cur);
            prev = cur;
        }
    }
}

int append_plumb(std::vector<glm::vec3>& seg, glm::vec3 from, float ground_y) {
    // ПУНКТИР, А НЕ СПЛОШНАЯ ЛИНИЯ. Сплошная читается как ЭЛЕМЕНТ постройки —
    // столб под вершиной, — и человек начинает искать, откуда он взялся.
    const float drop = from.y - ground_y;
    if (drop <= HOUSE_PLUMB_DASH_M) {
        return 0; // вершина на земле: отвесу неоткуда взяться
    }
    const float step = HOUSE_PLUMB_DASH_M * 2.0f;
    const int room = std::min(HOUSE_PLUMB_MAX_DASHES,
                              static_cast<int>(drop / step) + 1);
    // ВОЗВРАЩАЕТСЯ ЧИСЛО НАРИСОВАННЫХ ШТРИХОВ, А НЕ ЗАПЛАНИРОВАННЫХ, и разница
    // не косметическая: планировщик считал 7 при 3 метрах, а рисовалось 6 —
    // последний штрих упирался в землю и отбрасывался. Прибор, который называет
    // не то число, что нарисовал, перестаёт быть прибором.
    int drawn = 0;
    for (int i = 0; i < room; ++i) {
        const float top = from.y - static_cast<float>(i) * step;
        if (top <= ground_y) {
            break;
        }
        const float bottom = std::max(ground_y, top - HOUSE_PLUMB_DASH_M);
        seg.push_back({from.x, top, from.z});
        seg.push_back({from.x, bottom, from.z});
        ++drawn;
    }
    return drawn;
}

/// СТРЕЛКА НОРМАЛИ: отрезок из видного места плюс две зазубрины на конце.
/// Одна на весь файл: черновик и готовая стена рисуют её одинаково, иначе
/// «до подтверждения» и «после» отвечали бы на один вопрос по-разному.
static void append_normal_arrow(std::vector<glm::vec3>& dst, glm::vec3 centre,
                                glm::vec3 n) {
    const glm::vec3 tip = centre + n * HOUSE_NORMAL_ARROW_M;
    dst.push_back(centre);
    dst.push_back(tip);
    // ЗАЗУБРИНЫ: отрезок без них читается в обе стороны, а вопрос ровно про
    // сторону.
    glm::vec3 side = glm::cross(n, glm::vec3{0.0f, 1.0f, 0.0f});
    if (glm::length(side) < 1e-3f) {
        side = glm::cross(n, glm::vec3{1.0f, 0.0f, 0.0f});
    }
    side = glm::normalize(side) * (HOUSE_NORMAL_ARROW_M * 0.15f);
    const glm::vec3 back = tip - n * (HOUSE_NORMAL_ARROW_M * 0.25f);
    dst.push_back(tip);
    dst.push_back(back + side);
    dst.push_back(tip);
    dst.push_back(back - side);
}

void build_house_wire(const HouseSession& s, const HouseGroundFn& ground, HouseWire& out) {
    const HouseGraph& g = s.graph();
    const auto lit_element = [&](ElementId id) {
        const auto& v = s.lit_elements();
        return id == s.selected_element() || std::find(v.begin(), v.end(), id) != v.end();
    };
    const auto lit_vertex = [&](VertexId id) {
        const auto& v = s.lit_vertices();
        return id == s.selected_vertex() || std::find(v.begin(), v.end(), id) != v.end();
    };

    for (const Element& e : g.elements()) {
        std::vector<glm::vec3>& dst = lit_element(e.id) ? out.accent : out.plain;
        if (e.kind == ElementKind::Line) {
            glm::vec3 a{0.0f};
            glm::vec3 b{0.0f};
            if (s.line_ends_world(e.id, a, b)) {
                dst.push_back(a);
                dst.push_back(b);
            }
            continue;
        }
        // ПОВЕРХНОСТЬ РИСУЕТСЯ КОНТУРОМ, а не залитой: заливка — работа меша, и
        // рисовать её здесь значило бы завести вторую геометрию поверхности
        // рядом с той, что уйдёт в физику.
        for (std::size_t i = 1; i < e.refs.size(); ++i) {
            dst.push_back(s.vertex_world(e.refs[i - 1]));
            dst.push_back(s.vertex_world(e.refs[i]));
        }
        if (e.closed && e.refs.size() >= 3) {
            dst.push_back(s.vertex_world(e.refs.back()));
            dst.push_back(s.vertex_world(e.refs.front()));
        }
    }

    // ЛИЦО ПОДСВЕЧЕННОЙ СТЕНЫ НАЗЫВАЕТСЯ ВСЕГДА (заказ 18.08: «вектор нормали
    // всегда должен рисоваться, когда стена выделена или выбраны любые из её
    // якорей»). Подсветка уже отвечает на оба случая сразу: выбрал стену —
    // подсветилась она, выбрал её якорь — подсветились его элементы.
    for (const Element& e : g.elements()) {
        if (e.kind != ElementKind::Surface || !lit_element(e.id)) {
            continue;
        }
        glm::vec3 centre_local{0.0f};
        glm::vec3 n_local{0.0f};
        if (!world::surface_centre(g, e.id, centre_local)
            || !world::surface_normal(g, e.id, n_local)) {
            continue;
        }
        // В МИРОВЫЕ: и точка, и направление — но по-разному. Точку переносим,
        // направление только поворачиваем.
        const glm::vec3 zero = s.to_world({0.0f, 0.0f, 0.0f});
        append_normal_arrow(out.accent, s.to_world(centre_local),
                            glm::normalize(s.to_world(n_local) - zero));
    }

    for (const Vertex& v : g.vertices()) {
        const glm::vec3 p = s.vertex_world(v.id);
        std::vector<glm::vec3>& dst = lit_vertex(v.id) ? out.accent : out.plain;
        append_ball(dst, p, HOUSE_BALL_R_M);
        if (ground) {
            append_plumb(dst, p, ground({p.x, p.z}));
        }
    }
}

// ---------------------------------------------------------------------------
// Сессия
// ---------------------------------------------------------------------------

glm::vec3 HouseSession::to_world(glm::vec3 local) const {
    // ЯВНЫЙ ПОВОРОТ ВОКРУГ Y ПО ПРИНЯТОЙ В СЦЕНЕ УСЛОВНОСТИ: локальный +X
    // смотрит в (cos yaw, -sin yaw). Другая условность здесь означала бы дом,
    // повёрнутый не туда ровно в тот день, когда его поставят под углом.
    const float c = std::cos(yaw_);
    const float sn = std::sin(yaw_);
    return origin_ + glm::vec3{local.x * c + local.z * sn, local.y,
                               -local.x * sn + local.z * c};
}

glm::vec3 HouseSession::to_local(glm::vec3 world) const {
    const glm::vec3 d = world - origin_;
    const float c = std::cos(yaw_);
    const float sn = std::sin(yaw_);
    return {d.x * c - d.z * sn, d.y, d.x * sn + d.z * c};
}

glm::vec3 HouseSession::vertex_world(VertexId id) const {
    return to_world(graph_.resolved_local(id));
}

std::string HouseSession::snapshot() const { return world::write_house(graph_); }

bool HouseSession::apply_snapshot(const std::string& text) {
    // ИМЯ ВЫБРАННОЙ ВЕРШИНЫ НЕ СОХРАНЯЕТСЯ — СОХРАНЯЕТСЯ ЕЁ МЕСТО, и довод
    // целиком лежит у HouseSession::revision() в заголовке. Коротко: имена
    // круговой прогон переживают (зона core, 18.08), а вот СЧЁТЧИК имён
    // откатывается вместе с графом, поэтому «v4 из прошлой жизни» и «v4 из
    // этой» — разные точки (измерено: 99 м и -77 м). Опознание по координате
    // не зависит ни от того, ни от другого.
    const bool had_vertex = sel_vertex_ != NO_VERTEX;
    const glm::vec3 was = had_vertex ? vertex_world(sel_vertex_) : glm::vec3{0.0f};
    ++revision_;
    const world::HouseIoResult r = world::read_house(text, graph_);
    sel_vertex_ = NO_VERTEX;
    // ЭЛЕМЕНТ ПО МЕСТУ НЕ ОПОЗНАЁТСЯ: у него нет одной точки, а опознание по
    // набору вершин было бы догадкой. Выбор элемента после отмены снимается —
    // это честнее, чем восстановленный наугад.
    sel_element_ = NO_ELEMENT;
    if (had_vertex && r.ok) {
        sel_vertex_ = unique_vertex_at(was, HOUSE_REID_TOL_M);
    }
    refresh_selection();
    return r.ok;
}

VertexId HouseSession::unique_vertex_at(glm::vec3 world_point, float tol_m) const {
    VertexId found = NO_VERTEX;
    for (const Vertex& v : graph_.vertices()) {
        if (glm::length(vertex_world(v.id) - world_point) > tol_m) {
            continue;
        }
        if (found != NO_VERTEX) {
            // ДВА ЯКОРЯ В ОДНОЙ ТОЧКЕ — законное состояние (стык, наложение), и
            // опознание на нём обязано ОТКАЗАТЬ. Выбрать первый попавшийся
            // значит подсветить не те элементы, а на экране оба якоря в одном
            // месте, и человек об этом не узнает.
            return NO_VERTEX;
        }
        found = v.id;
    }
    return found;
}

void HouseSession::record(std::string label, const std::string& before) {
    if (history_ == nullptr) {
        return;
    }
    history_->record(std::move(label), before, snapshot());
}

void HouseSession::select_vertex(VertexId id) {
    sel_vertex_ = id;
    sel_element_ = NO_ELEMENT;
    refresh_selection();
}

void HouseSession::select_element(ElementId id) {
    sel_element_ = id;
    sel_vertex_ = NO_VERTEX;
    refresh_selection();
}

void HouseSession::clear_selection() {
    sel_vertex_ = NO_VERTEX;
    sel_element_ = NO_ELEMENT;
    lit_elements_.clear();
    lit_vertices_.clear();
}

void HouseSession::refresh_selection() {
    lit_elements_.clear();
    lit_vertices_.clear();
    if (sel_vertex_ != NO_VERTEX) {
        if (graph_.vertex(sel_vertex_) == nullptr) {
            sel_vertex_ = NO_VERTEX; // её больше нет: выбор не переживает удаление
            return;
        }
        lit_elements_ = graph_.incident(sel_vertex_);
        return;
    }
    if (sel_element_ != NO_ELEMENT) {
        const Element* e = graph_.element(sel_element_);
        if (e == nullptr) {
            sel_element_ = NO_ELEMENT;
            return;
        }
        lit_vertices_ = e->refs;
    }
}

VertexId HouseSession::pick_vertex(glm::vec3 world_point, float grab_m) const {
    VertexId best = NO_VERTEX;
    float best_d = grab_m;
    for (const Vertex& v : graph_.vertices()) {
        const float d = glm::length(vertex_world(v.id) - world_point);
        if (d <= best_d) {
            best_d = d;
            best = v.id;
        }
    }
    return best;
}

/// ТОЧКА НА ПРЯМОЙ, БЛИЖАЙШАЯ К ЛУЧУ ПРИЦЕЛА.
///
/// Классическая задача о двух скрещивающихся прямых. Ось идёт из якоря вверх,
/// луч — из глаза в направлении взгляда; берётся точка оси, ближайшая к лучу.
/// Это и есть «мышь двигает по оси»: экранное движение проецируется на неё, а
/// не превращается в пересечение с землёй.
///
/// Вырожденный случай назван, а не пропущен: если смотреть ВДОЛЬ оси (сверху
/// вниз), знаменатель обращается в ноль — ответа не существует, потому что вся
/// ось проецируется в точку. Возвращаем прежнюю высоту: инструмент замирает,
/// а не прыгает в бесконечность.
static glm::vec3 point_on_axis(const glm::vec3& anchor, const glm::vec3& u,
                               const glm::vec3& ray_origin, const glm::vec3& ray_dir,
                               const glm::vec3& fallback) {
    const glm::vec3 w = anchor - ray_origin;
    const float b = glm::dot(u, ray_dir);
    const float denom = 1.0f - b * b; // |u| = |d| = 1
    if (denom < 1e-5f) {
        return fallback;
    }
    const float d = glm::dot(u, w);
    const float e = glm::dot(ray_dir, w);
    // t = (b·e − d)/(1 − b²) — классическая пара скрещивающихся прямых. Знак
    // здесь не украшение: с обратным вышла бы вертикаль, идущая ВНИЗ, когда
    // человек тянет ВВЕРХ, и это тот же дефект, что он показал кадром.
    const float t = (b * e - d) / denom; // параметр вдоль ОСИ
    return anchor + u * t;
}

void HouseSession::cycle_axis(VertexId around) {
    // КРУГ ИЗ ТОГО, ЧТО ЧЕЛОВЕК ВИДИТ: свободно, вертикаль, потом каждая
    // прямая, приходящая в этот якорь, в порядке их имён — тот же порядок, в
    // котором они перечислены в файле и подсвечены на экране.
    std::vector<ElementId> lines;
    for (const ElementId id : graph_.incident(around)) {
        const Element* e = graph_.element(id);
        if (e != nullptr && e->kind == ElementKind::Line && e->refs.size() >= 2) {
            lines.push_back(id);
        }
    }
    if (axis_.kind == AxisLock::Kind::Free) {
        axis_ = AxisLock{AxisLock::Kind::Vertical, NO_ELEMENT};
        return;
    }
    if (axis_.kind == AxisLock::Kind::Vertical) {
        axis_ = lines.empty() ? AxisLock{}
                              : AxisLock{AxisLock::Kind::Edge, lines.front()};
        return;
    }
    const auto it = std::find(lines.begin(), lines.end(), axis_.edge);
    if (it == lines.end() || it + 1 == lines.end()) {
        axis_ = AxisLock{}; // круг замкнулся
        return;
    }
    axis_ = AxisLock{AxisLock::Kind::Edge, *(it + 1)};
}

bool HouseSession::axis_dir(VertexId at, glm::vec3& out) const {
    if (axis_.kind == AxisLock::Kind::Vertical) {
        out = {0.0f, 1.0f, 0.0f};
        return true;
    }
    if (axis_.kind != AxisLock::Kind::Edge) {
        return false;
    }
    const Element* e = graph_.element(axis_.edge);
    if (e == nullptr || e->refs.size() < 2) {
        return false;
    }
    // НАПРАВЛЕНИЕ СЧИТАЕТСЯ ОТ ЯКОРЯ, А НЕ ОТ НАЧАЛА ПРЯМОЙ: человек тянет
    // ИМЕННО этот конец, и знак должен слушаться его руки, а не порядка, в
    // котором прямая когда-то была записана.
    const glm::vec3 a = vertex_world(e->refs.front());
    const glm::vec3 b = vertex_world(e->refs.back());
    const glm::vec3 d = at == e->refs.front() ? b - a : a - b;
    const float len = glm::length(d);
    if (len < 1e-5f) {
        return false; // выродившаяся прямая направления не задаёт
    }
    out = d / len;
    return true;
}

std::string HouseSession::axis_label() const {
    switch (axis_.kind) {
    case AxisLock::Kind::Vertical:
        return "вертикаль";
    case AxisLock::Kind::Edge: {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "вдоль e%u", static_cast<unsigned>(axis_.edge));
        return buf;
    }
    case AxisLock::Kind::Free:
        break;
    }
    return "свободно";
}

VertexId HouseSession::pick_vertex_ray(glm::vec3 origin, glm::vec3 dir, float grab_m) const {
    VertexId best = NO_VERTEX;
    float best_along = 0.0f; // ближайшая К ГЛАЗУ из тех, что луч задел
    for (const Vertex& v : graph_.vertices()) {
        const glm::vec3 p = vertex_world(v.id);
        const float along = glm::dot(p - origin, dir);
        // ЗА СПИНОЙ ЦЕЛЕЙ НЕТ. Без этого якорь позади человека ловил бы щелчок
        // всякий раз, когда человек отвернулся ровно от него.
        if (along <= 0.0f) {
            continue;
        }
        if (glm::length(p - (origin + dir * along)) > grab_m) {
            continue;
        }
        if (best == NO_VERTEX || along < best_along) {
            best_along = along;
            best = v.id;
        }
    }
    return best;
}

/// СБЛИЖЕНИЕ ЛУЧА С ОТРЕЗКОМ — ОДНО ВЫРАЖЕНИЕ НА ВЕСЬ ФАЙЛ. Его спрашивают и
/// посадка вершины на ось, и выбор элемента; две копии этой формулы разошлись
/// бы в тот день, когда одну из них поправят (правило 32).
HouseEdgeHit ray_vs_segment(glm::vec3 origin, glm::vec3 dir, glm::vec3 a, glm::vec3 b,
                            float grab_m) {
    HouseEdgeHit out;
    const glm::vec3 ab = b - a;
    const float ab2 = glm::dot(ab, ab);
    if (ab2 < 1e-8f) {
        return out; // отрезок нулевой длины — это точка, и ею занят другой поиск
    }
    const float len = std::sqrt(ab2);
    const glm::vec3 abn = ab / len;
    // Систему из двух уравнений решаем прямо; вырожденный случай (луч
    // ПАРАЛЛЕЛЕН отрезку) назван отдельно: там общего решения нет, и берётся
    // ближайший конец, а не деление на ноль.
    const glm::vec3 w0 = a - origin;
    const float bcoef = glm::dot(dir, abn);
    const float denom = 1.0f - bcoef * bcoef;
    float t = 0.0f; // доля вдоль отрезка
    if (denom < 1e-5f) {
        t = std::clamp(glm::dot(-w0, abn) / len, 0.0f, 1.0f);
    } else {
        const float d = glm::dot(dir, w0);
        const float ee = glm::dot(abn, w0);
        // s = (b·d − e)/(1 − b²), метры вдоль отрезка от его начала. Знак здесь
        // уже стоил одной ошибки за вечер — на вертикали, в той же формуле:
        // перевёрнутый, он уводит ближайшую точку в противоположную сторону.
        const float s = (bcoef * d - ee) / denom;
        t = std::clamp(s / len, 0.0f, 1.0f);
    }
    const glm::vec3 p = a + ab * t;
    const float along = glm::dot(p - origin, dir);
    // ЗА СПИНОЙ ЦЕЛЕЙ НЕТ.
    if (along <= 0.0f) {
        return out;
    }
    const float miss = glm::length(p - (origin + dir * along));
    if (miss > grab_m) {
        return out;
    }
    // «Попадание есть»; НАСТОЯЩЕЕ ИМЯ ПРОСТАВЛЯЕТ ВЫЗЫВАЮЩИЙ — эта функция про
    // геометрию и про элементы ничего не знает.
    out.host = 1;
    out.t = t;
    out.distance_m = miss;
    out.point = p;
    return out;
}

HouseEdgeHit HouseSession::pick_edge_ray(glm::vec3 origin, glm::vec3 dir,
                                         float grab_m) const {
    HouseEdgeHit best;
    float best_along = 0.0f;
    for (const Element& e : graph_.elements()) {
        // ТОЛЬКО ПРЯМАЯ: на ось садится вершина, а «ось» есть у бруса и нет у
        // полотна — модель разрешает положение такой вершины через два конца,
        // которых у поверхности нет.
        if (e.kind != ElementKind::Line || e.refs.size() < 2) {
            continue;
        }
        HouseEdgeHit probe = ray_vs_segment(origin, dir, vertex_world(e.refs.front()),
                                            vertex_world(e.refs.back()), grab_m);
        if (!probe.hit()) {
            continue;
        }
        const float along = glm::dot(probe.point - origin, dir);
        // ИЗ НЕСКОЛЬКИХ ОСЕЙ ВЫИГРЫВАЕТ БЛИЖАЙШАЯ К ГЛАЗУ, а не ближайшая к
        // лучу: дальняя, стоящая точно за передней, иначе отбирала бы щелчок.
        if (!best.hit() || along < best_along) {
            best_along = along;
            probe.host = e.id;
            best = probe;
        }
    }
    return best;
}

/// ВЫСОТА ВЫДАВЛИВАНИЯ ЦЕПОЧКИ, метры. Спрашивается у ТЕХ ЖЕ параметров, что
/// читает построитель тела: второе место, где «высота стены» вычисляется
/// иначе, — это стена, которую видно не там, где в неё можно ткнуть.
static float house_surface_height(const HouseSession& s, const Element& e) {
    (void)s;
    const std::string h = [&] {
        for (const auto& kv : e.params) {
            if (kv.first == "height") {
                return kv.second;
            }
        }
        return std::string{};
    }();
    if (h.empty()) {
        // 0 — «высота не задана», и построитель тела в этом случае стену НЕ
        // строит. Значит и целиться не во что: ноль здесь — тот же ответ.
        return 0.0f;
    }
    return static_cast<float>(std::atof(h.c_str()));
}

/// ЛУЧ И ТРЕУГОЛЬНИК (Мёллер–Трумбор). Расстояние вдоль луча или -1.
static float ray_vs_triangle(glm::vec3 o, glm::vec3 d, glm::vec3 a, glm::vec3 b,
                             glm::vec3 c) {
    const glm::vec3 e1 = b - a;
    const glm::vec3 e2 = c - a;
    const glm::vec3 pv = glm::cross(d, e2);
    const float det = glm::dot(e1, pv);
    // ЛУЧ ВДОЛЬ ПЛОСКОСТИ ТРЕУГОЛЬНИКА — пересечения нет; знак det здесь НЕ
    // фильтруется, потому что в стену человек смотрит с обеих сторон.
    if (std::fabs(det) < 1e-8f) {
        return -1.0f;
    }
    const float inv = 1.0f / det;
    const glm::vec3 tv = o - a;
    const float u = glm::dot(tv, pv) * inv;
    if (u < 0.0f || u > 1.0f) {
        return -1.0f;
    }
    const glm::vec3 qv = glm::cross(tv, e1);
    const float v = glm::dot(d, qv) * inv;
    if (v < 0.0f || u + v > 1.0f) {
        return -1.0f;
    }
    const float t = glm::dot(e2, qv) * inv;
    return t > 0.0f ? t : -1.0f;
}

ElementId HouseSession::pick_element_ray(glm::vec3 origin, glm::vec3 dir, float grab_m,
                                         float* out_distance) const {
    ElementId best = NO_ELEMENT;
    float best_along = 0.0f;
    // Отрезки контура перебираются здесь, а сближение считает та же пара
    // формул, что и у оси: второе выражение для того же сближения разошлось бы
    // с первым в день, когда одну из копий поправят (правило 32).
    const auto consider = [&](ElementId id, glm::vec3 a, glm::vec3 b) {
        HouseEdgeHit probe = ray_vs_segment(origin, dir, a, b, grab_m);
        if (!probe.hit()) {
            return;
        }
        const float along = glm::dot(probe.point - origin, dir);
        if (best == NO_ELEMENT || along < best_along) {
            best_along = along;
            best = id;
        }
    };
    for (const Element& e : graph_.elements()) {
        if (e.refs.size() < 2) {
            continue;
        }
        if (e.kind == ElementKind::Line) {
            consider(e.id, vertex_world(e.refs.front()), vertex_world(e.refs.back()));
            continue;
        }
        for (std::size_t i = 0; i + 1 < e.refs.size(); ++i) {
            consider(e.id, vertex_world(e.refs[i]), vertex_world(e.refs[i + 1]));
        }
        if (e.closed && e.refs.size() >= 3) {
            consider(e.id, vertex_world(e.refs.back()), vertex_world(e.refs.front()));
        }
        // И САМО ПОЛОТНО, А НЕ ТОЛЬКО ЕГО КРОМКА. Пользователь 18.08 дважды
        // написал «не могу выбрать стену»: он целится в СЕРЕДИНУ стены, а
        // ловился только контур — полоса в тридцать сантиметров по краю.
        //
        // Треугольники берутся те же, что рисует построитель тела: замкнутый
        // контур — веером от первой вершины (это верно для выпуклого и даёт
        // разумный ответ для невыпуклого — хуже кромки не будет), цепочка —
        // выдавленная вверх лента. Полного разбора ушами здесь нет нарочно:
        // выбор мышью не обязан совпадать с мешом ДО ТРЕУГОЛЬНИКА, он обязан
        // не промахиваться по стене.
        const auto face = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            const float t = ray_vs_triangle(origin, dir, a, b, c);
            if (t > 0.0f && (best == NO_ELEMENT || t < best_along)) {
                best_along = t;
                best = e.id;
            }
        };
        if (e.closed && e.refs.size() >= 3) {
            const glm::vec3 a0 = vertex_world(e.refs.front());
            for (std::size_t i = 1; i + 1 < e.refs.size(); ++i) {
                face(a0, vertex_world(e.refs[i]), vertex_world(e.refs[i + 1]));
            }
        } else {
            const float h = house_surface_height(*this, e);
            if (h > 1e-3f) {
                const glm::vec3 up{0.0f, h, 0.0f};
                for (std::size_t i = 0; i + 1 < e.refs.size(); ++i) {
                    const glm::vec3 a = vertex_world(e.refs[i]);
                    const glm::vec3 b = vertex_world(e.refs[i + 1]);
                    face(a, b, b + up);
                    face(a, b + up, a + up);
                }
            }
        }
    }
    if (out_distance != nullptr) {
        *out_distance = best_along;
    }
    return best;
}

std::string HouseSession::delete_selection() {
    // ЭЛЕМЕНТ ПЕРЕД ЯКОРЕМ, и порядок здесь — про намерение. Выбрав стену,
    // человек просит убрать стену; якорь под ней он не выбирал.
    if (const ElementId e = selected_element(); e != NO_ELEMENT) {
        const GraphResult r = mutate("убрал элемент", [&](HouseGraph& g) {
            return g.remove_element(e);
        });
        if (r.ok) {
            clear_selection();
            return {};
        }
        return r.why;
    }
    const VertexId v = selected_vertex();
    if (v == NO_VERTEX) {
        return "ничего не выбрано";
    }
    const GraphResult r = mutate("убрал якорь", [&](HouseGraph& g) {
        return g.remove_vertex(v);
    });
    if (r.ok) {
        clear_selection();
        return {};
    }
    // ОТКАЗ СО СПИСКОМ ДЕРЖАТЕЛЕЙ, ИМЕНАМИ ИЗ ФАЙЛА: список отвечает на вопрос
    // «что отвязать», не вставая из кресла.
    std::string why = r.why;
    if (!r.blockers.empty()) {
        why += ": ";
        for (std::size_t i = 0; i < r.blockers.size(); ++i) {
            if (i != 0) {
                why += ", ";
            }
            why += ename(r.blockers[i]);
        }
    }
    return why;
}

HouseEdgeHit HouseSession::pick_edge(glm::vec3 world_point, float grab_m) const {
    HouseEdgeHit best;
    float best_d = grab_m;
    for (const Element& e : graph_.elements()) {
        if (e.kind != ElementKind::Line || e.refs.size() < 2) {
            // ПРЯМАЯ С ОДНОЙ ВЕРШИНОЙ ОСЬЮ НЕ СЧИТАЕТСЯ, и это не мелочь:
            // модель разрешает посадить вершину на такую ось, но разрешает её
            // положение через refs.front()/refs.back(), которых там нет, —
            // вершина оказалась бы не на оси, а в собственном нуле.
            continue;
        }
        const glm::vec3 a = vertex_world(e.refs.front());
        const glm::vec3 b = vertex_world(e.refs.back());
        float t = 0.0f;
        const glm::vec3 p = closest_on_segment(a, b, world_point, t);
        const float d = glm::length(p - world_point);
        if (d <= best_d) {
            best_d = d;
            best.host = e.id;
            best.t = t;
            best.distance_m = d;
            best.point = p;
        }
    }
    return best;
}

bool HouseSession::line_ends_world(ElementId id, glm::vec3& a, glm::vec3& b) const {
    const Element* e = graph_.element(id);
    if (e == nullptr || e->kind != ElementKind::Line || e->refs.empty()) {
        return false;
    }
    a = vertex_world(e->refs.front());
    if (e->refs.size() >= 2) {
        b = vertex_world(e->refs.back());
        return true;
    }
    // ОДНА ВЕРШИНА — ВТОРОЙ КОНЕЦ ИЗ ЧИСЕЛ, тем же выражением, что у
    // построителя меша. Иначе призрак и дом разошлись бы на первой же прямой,
    // проведённой в пустоту.
    const std::string len = graph_.param(id, "length");
    if (len.empty()) {
        return false;
    }
    const float length = std::strtof(len.c_str(), nullptr);
    const std::string ax = graph_.param(id, "angle_x");
    const std::string ay = graph_.param(id, "angle_y");
    const glm::vec3 dir = house_dir_from_angles(
        ax.empty() ? 0.0f : std::strtof(ax.c_str(), nullptr),
        ay.empty() ? 0.0f : std::strtof(ay.c_str(), nullptr));
    const glm::vec3 local_a = graph_.resolved_local(e->refs.front());
    b = to_world(local_a + dir * length);
    return true;
}

// ---------------------------------------------------------------------------
// Углы и числа
// ---------------------------------------------------------------------------

glm::vec3 house_dir_from_angles(float angle_x_deg, float angle_y_deg) {
    const float ax = angle_x_deg * DEG2RAD;
    const float ay = angle_y_deg * DEG2RAD;
    return {std::sin(ay) * std::sin(ax), std::cos(ax), std::cos(ay) * std::sin(ax)};
}

void house_angles_from_dir(glm::vec3 dir, float& angle_x_deg, float& angle_y_deg) {
    const float len = glm::length(dir);
    if (len < 1e-6f) {
        angle_x_deg = 0.0f;
        angle_y_deg = 0.0f;
        return;
    }
    const glm::vec3 d = dir / len;
    angle_x_deg = std::acos(std::clamp(d.y, -1.0f, 1.0f)) * RAD2DEG;
    angle_y_deg = std::atan2(d.x, d.z) * RAD2DEG;
}

std::string house_num(float value) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", static_cast<double>(value));
    return buf;
}

// ---------------------------------------------------------------------------
// 7 — ВЕРШИНЫ
// ---------------------------------------------------------------------------

ToolIdentity HouseVertexTool::identity() const {
    return ToolIdentity{"house.vertex", "editor.tool.house.vertex",
                        "tool.hint.house.vertex", ToolIcon::HouseVertex};
}

float HouseVertexTool::ground_at(glm::vec2 xz, float fallback_y) const {
    if (world_ != nullptr && world_->ground_height) {
        return world_->ground_height(xz);
    }
    // МИР ВЫСОТЫ НЕ ЗНАЕТ — берём высоту прицела, а не ноль. Ноль утопил бы
    // постройку на десятки метров, и этот отказ в репозитории уже был (кольцо
    // кисти под холмом).
    return fallback_y;
}

void HouseVertexTool::on_wheel(float ticks) {
    set_pull_m(pull_m_ + ticks * HOUSE_PULL_STEP_M);
}

HouseVertexTool::Ghost HouseVertexTool::ghost(const ToolAim& aim) const {
    Ghost g;
    if (session_ == nullptr) {
        return g;
    }
    const glm::vec3 dir = aim.direction();
    // ПОД ПРИЦЕЛОМ ЧУЖОЙ ЯКОРЬ — ЗНАЧИТ ЩЕЛЧОК ДОСТАНЕТСЯ ЕМУ, а не новой
    // вершине. Показ обязан знать об этом раньше щелчка, иначе призрак обещает
    // одно, а рука делает другое.
    g.over = session_->pick_vertex_ray(aim.origin, dir, HOUSE_GRAB_M);
    if (g.over != NO_VERTEX) {
        g.point = session_->vertex_world(g.over);
        g.ground_y = ground_at({g.point.x, g.point.z}, g.point.y);
        g.air = g.point.y - g.ground_y > HOUSE_AIR_EPS_M;
        return g;
    }
    // ТОЧКА ВДОЛЬ ЛУЧА: земля минус подтягивание. Луч смотрит вниз, поэтому
    // «ближе к себе» и «выше над травой» — одно движение.
    const float base = aim.hit ? aim.distance_m : HOUSE_PULL_MAX_M;
    const float along = std::max(base - pull_m_, HOUSE_PULL_STEP_M);
    g.point = aim.origin + dir * along;

    // ПРИЛИПАНИЕ К ОСИ — ПО ТОМУ, КУДА ЧЕЛОВЕК СМОТРИТ, И ТОЛЬКО ПО ЭТОМУ.
    //
    // Первый заход искал ось, ближайшую к самому шарику. На горизонтальных
    // брёвнах это работало: шарик едет по лучу над землёй и проходит рядом с
    // ними. У СТОЙКИ не работало вовсе (жалоба 18.08: «к вертикальным нет»), и
    // причина не в допуске. Чтобы поднять шарик на два метра при пологом
    // взгляде, его надо подтянуть метров на десять — а вместе с высотой он
    // уезжает на те же десять метров ПО ГОРИЗОНТАЛИ, навстречу человеку, и от
    // стойки, на которую человек смотрит, оказывается дальше, чем был.
    //
    // Правило теперь одно: целишься в бревно — якорь садится на бревно, и
    // подтягивание тут ни при чём. Второе правило («шарик ищет ось вокруг
    // себя») я завёл было рядом и убрал: оно дёргало якорь на бревно, на
    // которое человек не смотрел, и объяснить, почему шарик уехал, было нечем.
    if (HouseEdgeHit aimed = session_->pick_edge_ray(aim.origin, dir, HOUSE_SNAP_M);
        aimed.hit()) {
        // СЕТКА ДЕЙСТВУЕТ И НА ОСИ: точка съезжает вдоль бревна к ближайшему
        // узлу, не сходя с оси. t пересчитывается — модель хранит вершину на
        // оси именно долей, и доля обязана согласоваться с точкой.
        if (const Element* host = session_->graph().element(aimed.host);
            host != nullptr && host->refs.size() >= 2) {
            const glm::vec3 ea = session_->vertex_world(host->refs.front());
            const glm::vec3 eb = session_->vertex_world(host->refs.back());
            const float elen = glm::length(eb - ea);
            if (elen > 1e-4f) {
                const glm::vec3 eu = (eb - ea) / elen;
                const glm::vec3 snapped = session_->snap_on_axis(aimed.point, ea, eu);
                const float t = glm::clamp(glm::dot(snapped - ea, eu) / elen, 0.0f, 1.0f);
                aimed.point = ea + eu * (t * elen);
                aimed.t = t;
            }
        }
        g.on_edge = aimed;
        g.point = aimed.point;
        g.ground_y = ground_at({g.point.x, g.point.z}, g.point.y);
        g.air = g.point.y - g.ground_y > HOUSE_AIR_EPS_M;
        return g;
    }
    // СЕТКА ЛОВИТ ЯКОРЬ, ЕСЛИ ОНА ВКЛЮЧЕНА. После прилипания к оси — нет: ось
    // уже сказала, где точка, и второй магнит сдвинул бы её С ОСИ, то есть
    // отменил бы первый.
    g.point = session_->snap_to_grid(g.point);
    g.ground_y = ground_at({g.point.x, g.point.z}, g.point.y);
    // ЗЕМЛЯ — ПОЛ, А НЕ СОВЕТ: шарик, подтянутый мимо склона, не имеет права
    // оказаться ПОД травой. Ось из этого правила исключена нарочно — бревно
    // может уходить в грунт, и вершина на его оси уходит вместе с ним.
    if (g.point.y < g.ground_y) {
        g.point.y = g.ground_y;
    }
    g.air = g.point.y - g.ground_y > HOUSE_AIR_EPS_M;
    return g;
}

void HouseVertexTool::on_press(const ToolAim& aim, ToolWorld& world) {
    (void)world;
    refusal_.clear();
    if (session_ == nullptr) {
        return;
    }
    if (stale()) {
        // МЕЖДУ ДВУМЯ ЩЕЛЧКАМИ БЫЛА ОТМЕНА. Всё, что рука помнила по именам,
        // теперь про другой граф — забываем, а не доигрываем на чужих вершинах.
        dragging_ = NO_VERTEX;
        drag_before_.clear();
        seen_revision_ = session_->revision();
    }
    if (const VertexId hit = session_->pick_vertex_ray(aim.origin, aim.direction(), HOUSE_GRAB_M);
        hit != NO_VERTEX) {
        session_->select_vertex(hit);
        const Vertex* v = session_->graph().vertex(hit);
        if (v != nullptr && v->anchoring != Anchoring::OnEdge) {
            // СНИМОК СНИМАЕТСЯ ЗДЕСЬ, А ЗАПИСЫВАЕТСЯ НА ОТПУСКАНИИ: протаскивание
            // двигает вершину каждый кадр, и шаг истории на кадр превратил бы
            // одно движение руки в сотню отмен.
            dragging_ = hit;
            drag_before_ = session_->snapshot();
            const glm::vec3 p = session_->vertex_world(hit);
            drag_lift_m_ = p.y - ground_at({p.x, p.z}, p.y);
        }
        return;
    }
    const Ghost g = ghost(aim);
    if (const HouseEdgeHit edge = g.on_edge; edge.hit()) {
        VertexId made = NO_VERTEX;
        const GraphResult r = session_->mutate("вершина на оси", [&](HouseGraph& g) {
            return g.add_vertex_on_edge(edge.host, edge.t, made);
        });
        if (r.ok) {
            session_->select_vertex(made);
        } else {
            refusal_ = r.why;
        }
        return;
    }
    // ЗЕМЛЯ ИЛИ ВОЗДУХ — РЕШАЕТ ПРИЗРАК, тот же самый, что человек видел перед
    // щелчком. Заземлённая вершина хранит только XZ (высоту ей даёт рельеф),
    // висящая — все три числа и отвес.
    VertexId made = NO_VERTEX;
    (void)session_->mutate(g.air ? "вершина в воздухе" : "вершина по земле",
                           [&](HouseGraph& gr) {
                               made = gr.add_vertex(g.air ? Anchoring::Free
                                                          : Anchoring::OnGround,
                                                    session_->to_local(g.point));
                               return GraphResult{};
                           });
    session_->select_vertex(made);
}

void HouseVertexTool::on_drag(const ToolAim& aim, float dt_s, ToolWorld& world) {
    (void)dt_s;
    (void)world;
    if (session_ == nullptr || dragging_ == NO_VERTEX || stale()) {
        return;
    }
    // ОСЬ, ЕСЛИ ОНА ЗАПЕРТА, РЕШАЕТ ВСЁ. Заказ 18.08: «надо продумать систему,
    // чтобы я мог двигать якоря вдоль любой из линий, которые рисуются от
    // якоря». Тогда точка берётся с ЭТОЙ прямой, а не с земли: балку можно
    // удлинить, не сбив её направления, и поднять стойку ровно по её же оси.
    if (glm::vec3 u{0.0f}; session_->axis_dir(dragging_, u)) {
        const glm::vec3 here = session_->vertex_world(dragging_);
        glm::vec3 target = point_on_axis(here, u, aim.origin, aim.direction(), here);
        // Сетка действует и на оси: округлить, спроецировать обратно.
        target = session_->snap_on_axis(target, here, u);
        (void)session_->graph().move_vertex(dragging_, session_->to_local(target));
        return;
    }
    // ВЫСОТА ДЕРЖИТСЯ НАД РЕЛЬЕФОМ, а не в мире: якорь, стоявший в двух метрах
    // над склоном, обязан остаться в двух метрах над склоном и после сдвига.
    const float gy = ground_at({aim.point.x, aim.point.z}, aim.point.y);
    const glm::vec3 target{aim.point.x, gy + drag_lift_m_, aim.point.z};
    (void)session_->graph().move_vertex(dragging_, session_->to_local(target));
}

void HouseVertexTool::on_release(ToolWorld& world) {
    (void)world;
    if (session_ == nullptr || dragging_ == NO_VERTEX) {
        return;
    }
    if (stale()) {
        // Отмена посреди протаскивания: снимок «до» относится к прежней жизни
        // графа, и шаг истории из него был бы переходом из состояния, которого
        // больше нет.
        dragging_ = NO_VERTEX;
        drag_before_.clear();
        seen_revision_ = session_->revision();
        return;
    }
    // ОДИН ШАГ ИСТОРИИ НА ОДНО ДВИЖЕНИЕ РУКИ. Шаг, ничего не сдвинувший
    // (щёлкнул по якорю и отпустил), история отбрасывает сама — before == after.
    session_->record("двинул якорь", drag_before_);
    dragging_ = NO_VERTEX;
    drag_before_.clear();
}

void HouseVertexTool::on_deselected(ToolWorld& world) {
    (void)world;
    dragging_ = NO_VERTEX;
    drag_before_.clear();
    refusal_.clear();
}

bool HouseVertexTool::delete_selected() {
    refusal_.clear();
    if (session_ == nullptr) {
        return false;
    }
    // РЕШАЕТ СЕССИЯ, А НЕ КНОПКА. Тот же метод зовёт клавиша Delete, и два
    // разных «убрать» — с разными правилами и разными отказами — были бы двумя
    // ответами на один вопрос человека.
    refusal_ = session_->delete_selection();
    return refusal_.empty();
}

ToolPreview HouseVertexTool::preview(const ToolAim& aim) const {
    ToolPreview out;
    wire_.clear();
    // СТОПКА ПРИЗРАКА ЧИСТИТСЯ ЗДЕСЬ, РЯДОМ С ОСТАЛЬНЫМИ. Забытая строка стоила
    // ровно то, что видно на кадре пользователя: шарик добавлялся каждый кадр,
    // за минуту набралось 650 тысяч вершин, и рендерер начал ронять линии
    // пачками. Показ пересобирается ЦЕЛИКОМ каждый кадр — это его свойство, и
    // любая стопка, живущая дольше кадра, ему противоречит.
    ghost_pairs_.clear();
    if (session_ == nullptr) {
        return out;
    }
    HouseGroundFn ground;
    if (world_ != nullptr && world_->ground_height) {
        ground = world_->ground_height;
    }
    build_house_wire(*session_, ground, wire_);

    // ПРИЗРАК ТОЙ ВЕРШИНЫ, КОТОРУЮ ПОСТАВИТ ЭТОТ ЩЕЛЧОК, с её собственным
    // отвесом. Без него человек узнаёт высоту постановки ПОСЛЕ постановки.
    // ПРАВЯТ СТРЕЛКАМИ — ПОД ПРИЦЕЛОМ НИЧЕГО НЕ РИСУЕТСЯ, помечается двигаемый
    // якорь. Довод целиком у HouseSession::nudging().
    if (session_->nudging()) {
        if (const VertexId sel = session_->selected_vertex(); sel != NO_VERTEX) {
            // МЕТКА, А НЕ ВТОРОЙ ШАРИК: крест по трём осям вокруг якоря. Он
            // отличается от шарика формой, и потому не спорит с ним за смысл.
            const glm::vec3 p = session_->vertex_world(sel);
            const float r = HOUSE_BALL_R_M * 2.5f;
            ghost_pairs_.push_back(p - glm::vec3{r, 0.0f, 0.0f});
            ghost_pairs_.push_back(p + glm::vec3{r, 0.0f, 0.0f});
            ghost_pairs_.push_back(p - glm::vec3{0.0f, r, 0.0f});
            ghost_pairs_.push_back(p + glm::vec3{0.0f, r, 0.0f});
            ghost_pairs_.push_back(p - glm::vec3{0.0f, 0.0f, r});
            ghost_pairs_.push_back(p + glm::vec3{0.0f, 0.0f, r});
        }
    } else if (dragging_ == NO_VERTEX && aim.in_reach) {
        // ПРИЗРАК — ОБЕЩАНИЕ ЩЕЛЧКА, и за пределом дальности его нет. Сама
        // постройка выше построена уже и никуда не денется: она факт.
        //
        // ПРИЗРАК РИСУЕТСЯ И КОГДА ЛУЧ НЕ ВСТРЕТИЛ ЗЕМЛИ: подтянутый к себе
        // шарик висит в воздухе, и смотреть на него человек может как угодно.
        const Ghost g = ghost(aim);
        if (g.over == NO_VERTEX) {
            append_ball(ghost_pairs_, g.point, HOUSE_BALL_R_M);
            // ОТВЕС — ЭТО ОТВЕТ НА ВОПРОС «НА КАКОЙ ОН ВЫСОТЕ». Без него шарик
            // в воздухе неотличим от шарика на дальнем склоне.
            if (g.air) {
                append_plumb(ghost_pairs_, g.point, g.ground_y);
            }
        }
    }
    out.handles = wire_.plain.empty() ? nullptr : &wire_.plain;
    out.accent = wire_.accent.empty() ? nullptr : &wire_.accent;
    out.ghost_pairs = ghost_pairs_.empty() ? nullptr : &ghost_pairs_;
    out.ghost_color = HOUSE_GHOST_COLOR;
    out.line_color = HOUSE_WIRE_COLOR;
    out.accent_color = HOUSE_ACCENT_COLOR;
    return out;
}

ToolStatus HouseVertexTool::status(const ToolAim& aim) const {
    if (!refusal_.empty()) {
        return ToolStatus{"", refusal_, false};
    }
    if (session_ == nullptr) {
        return ToolStatus{"house.hint.nomodel", {}, false};
    }
    // ПОДПИСЬ ЧИТАЕТ ТОТ ЖЕ ПРИЗРАК, ЧТО И ПОКАЗ СО ЩЕЛЧКОМ. Три места, один
    // ответ: подпись, разошедшаяся с делом, хуже отсутствующей.
    const Ghost g = ghost(aim);
    if (g.over != NO_VERTEX || dragging_ != NO_VERTEX) {
        if (!session_->axis().free()) {
            // ОСЬ НАЗЫВАЕТ СЕБЯ, пока якорь в руке: иначе человек тянет и не
            // понимает, почему шарик не идёт за прицелом.
            char buf[96];
            std::snprintf(buf, sizeof(buf), "якорь по оси: %s (V — следующая)",
                          session_->axis_label().c_str());
            return ToolStatus{"", buf, true};
        }
        return ToolStatus{"house.hint.grab", {}, true};
    }
    if (g.on_edge.hit()) {
        return ToolStatus{"house.hint.onedge", {}, true};
    }
    if (g.air) {
        // ВЫСОТА ЧИСЛОМ, а не словом «в воздухе»: человек ставит вершину под
        // балку, и ему нужно знать, на сколько она поднялась.
        char buf[96];
        std::snprintf(buf, sizeof(buf), "в воздухе: %.2f м над землёй (колесо — ближе/дальше)",
                      static_cast<double>(g.point.y - g.ground_y));
        return ToolStatus{"", buf, true};
    }
    return ToolStatus{"house.hint.ground", {}, true};
}

// ---------------------------------------------------------------------------
// 8 — ПРЯМАЯ
// ---------------------------------------------------------------------------

HouseClampHit house_clamp_length(const HouseSession& s, VertexId from, glm::vec3 dir_world,
                                 float raw_length_m, float axis_tol_m, HouseClamp mode) {
    HouseClampHit out;
    if (mode == HouseClamp::None) {
        return out;
    }
    const float dl = glm::length(dir_world);
    if (dl < 1e-6f) {
        return out;
    }
    const glm::vec3 d = dir_world / dl;
    const glm::vec3 a = s.vertex_world(from);
    float best_gap = 0.0f;
    for (const Vertex& v : s.graph().vertices()) {
        if (v.id == from) {
            continue;
        }
        const glm::vec3 p = s.vertex_world(v.id);
        const float proj = glm::dot(p - a, d);
        if (proj <= 0.0f) {
            continue; // позади начала — это не «дальше по прямой»
        }
        // РАССТОЯНИЕ ДО ОСИ, а не до конца: якорь считается лежащим на этой
        // прямой, если она проходит рядом с ним, независимо от того, где
        // сейчас рука.
        const float off = glm::length(p - (a + d * proj));
        if (off > axis_tol_m) {
            continue;
        }
        const float gap = proj - raw_length_m;
        const bool above = gap > 0.0f;
        if ((mode == HouseClamp::Above) != above) {
            continue;
        }
        const float mag = std::fabs(gap);
        if (!out.found || mag < best_gap) {
            out.found = true;
            best_gap = mag;
            out.length_m = proj;
            out.at = v.id;
        }
    }
    return out;
}

ToolIdentity HouseLineTool::identity() const {
    return ToolIdentity{"house.line", "editor.tool.house.line", "tool.hint.house.line",
                        ToolIcon::HouseLine};
}


void HouseLineTool::update_end(const ToolAim& aim) {
    raw_end_ = aim.point;
    clamp_hit_ = HouseClampHit{};
    if (from_ == NO_VERTEX) {
        return;
    }
    const glm::vec3 a = session_->vertex_world(from_);
    snap_ = NO_VERTEX;
    // МАГНИТ НА ЯКОРЬ — И ЭТО ГЛАВНОЕ, ЧЕГО НЕ ХВАТАЛО. Конец прямой брался
    // там, где луч встретил ЗЕМЛЮ, поэтому два якоря в воздухе соединить было
    // нечем: прямая упиралась в пол ровно так же, как до починки вертикали
    // («нет проверки на то, что я смотрю прямо на якорь», 18.08).
    //
    // Спрашивается ЛУЧ, а не точка прицела: якорь в воздухе рядом с точкой на
    // земле не окажется никогда. Найденный якорь и есть конец — не «рядом с
    // ним», а ровно он, иначе прямая упрётся в воздух в сантиметре от цели.
    if (const VertexId hit = session_->pick_vertex_ray(aim.origin, aim.direction(),
                                                       HOUSE_GRAB_M);
        hit != NO_VERTEX && hit != from_) {
        snap_ = hit;
        raw_end_ = session_->vertex_world(hit);
    }
    if (glm::vec3 u{0.0f}; session_->axis_dir(from_, u)) {
        // ЛУЧ БЕРЁТСЯ ИЗ САМОГО ПРИЦЕЛА, а не восстанавливается по камере:
        // ToolAim несёт и глаз, и точку, и второй способ узнать направление
        // взгляда разъехался бы с первым в день, когда прицел сместят.
        raw_end_ = point_on_axis(a, u, aim.origin, aim.direction(), raw_end_);
        raw_end_ = session_->snap_on_axis(raw_end_, a, u); // сетка и на оси
        snap_ = NO_VERTEX; // на запертой оси конец решает ось, а не магнит
    }
    const glm::vec3 delta = raw_end_ - a;
    const float len = glm::length(delta);
    if (len < 1e-4f) {
        return;
    }
    clamp_hit_ = house_clamp_length(*session_, from_, delta, len, HOUSE_CLAMP_AXIS_TOL_M,
                                    clamp_);
}

glm::vec3 HouseLineTool::ghost_end() const {
    if (session_ == nullptr || from_ == NO_VERTEX) {
        return raw_end_;
    }
    const glm::vec3 a = session_->vertex_world(from_);
    const glm::vec3 delta = raw_end_ - a;
    const float len = glm::length(delta);
    if (len < 1e-4f || !clamp_hit_.found) {
        return raw_end_;
    }
    return a + delta / len * clamp_hit_.length_m;
}

void HouseLineTool::on_press(const ToolAim& aim, ToolWorld& world) {
    (void)world;
    refusal_.clear();
    if (session_ == nullptr) {
        return;
    }
    if (stale()) {
        from_ = NO_VERTEX;
        last_ = NO_ELEMENT;
        clamp_hit_ = HouseClampHit{};
        seen_revision_ = session_->revision();
    }
    const VertexId hit = session_->pick_vertex_ray(aim.origin, aim.direction(), HOUSE_GRAB_M);
    if (hit == NO_VERTEX) {
        // ПРЯМАЯ НАЧИНАЕТСЯ С ЯКОРЯ И НИ С ЧЕГО ДРУГОГО. Начать её с пустого
        // места значило бы поставить вершину исподтишка — а вершины ставит
        // другой инструмент, и он один.
        refusal_ = "начни с якоря: прямая тянется от вершины";
        return;
    }
    from_ = hit;
    session_->select_vertex(hit);
    update_end(aim);
}

void HouseLineTool::on_drag(const ToolAim& aim, float dt_s, ToolWorld& world) {
    (void)dt_s;
    (void)world;
    if (session_ == nullptr || from_ == NO_VERTEX || stale()) {
        return;
    }
    update_end(aim);
}

void HouseLineTool::on_release(ToolWorld& world) {
    (void)world;
    if (session_ == nullptr || from_ == NO_VERTEX) {
        return;
    }
    if (stale()) {
        // Якорь, из которого тянули, принадлежал прежней жизни графа. Строить
        // от его имени сейчас значит пристроить бревно к вершине, которую
        // человек не выбирал.
        from_ = NO_VERTEX;
        clamp_hit_ = HouseClampHit{};
        seen_revision_ = session_->revision();
        return;
    }
    const VertexId from = from_;
    // КОНЕЦ ПРИЗРАКА СНИМАЕТСЯ ДО ТОГО, КАК РУКА ОПУСТЕЕТ. Порядок этих двух
    // строк — не стиль: ghost_end() читает from_, и обнулив его первым, я
    // получал СЫРОЙ конец вместо зажатого — то есть прямая строилась не той
    // длины, которую человек видел на экране. Поймано рукавом (зажим «ниже» дал
    // 3.5 м вместо 2.0 м), а не глазом: на картинке призрак был правильный.
    const glm::vec3 end = ghost_end();
    from_ = NO_VERTEX;
    // ОТПУСТИЛ НА ЯКОРЕ — ЯКОРЯ СОЕДИНИЛИСЬ. Ищем по СЫРОМУ прицелу, а не по
    // зажатому концу: зажим — про длину, а соединение — про то, куда человек
    // отпустил руку.
    // К КОМУ ПРИЛИПЛА РУКА, ТОТ И ВТОРОЙ КОНЕЦ. Прилипание нашёл луч ещё во
    // время протаскивания; искать заново по точке значило бы задать другой
    // вопрос и получить другой ответ — тот самый, из-за которого якоря в
    // воздухе не соединялись.
    // ЗАЖИМ ДЛИНЫ — ЭТО ТОЖЕ СОЕДИНЕНИЕ. Он ставит конец РОВНО на чужой якорь
    // («зажать до ближайшего сверху/снизу»), и породить там вторую вершину в
    // той же точке значило бы построить дом на двойных якорях: на вид один
    // шарик, на деле два, и половина связей идёт не туда.
    const VertexId to = snap_ != NO_VERTEX      ? snap_
                        : clamp_hit_.found      ? clamp_hit_.at
                                                : session_->pick_vertex(raw_end_, HOUSE_GRAB_M);
    // ЗАГОТОВКА ДОЕЗЖАЕТ ДО ЭЛЕМЕНТА: материал, тон и форма пишутся при
    // создании — правка потом идёт через блок «Выбрано сейчас».
    const auto stamp_draft = [&](HouseGraph& g, ElementId id) {
        g.set_param(id, "radius", house_num(radius_m_));
        g.set_param(id, "mat", std::to_string(mat_));
        g.set_param(id, "tone", std::to_string(tone_));
        if (paint_ > 0) {
            g.set_param(id, "paint", std::to_string(paint_));
        }
        // ПРОФИЛЬ ПО ИНДЕКСУ ФОРМЫ: 0 круг, 1 квадрат, 2/3/4/5 — многогранники
        // (3/6/8/12 граней), 6 — доска. «Лестницы» среди форм больше нет
        // (правка 20.08) — марш строится картой раздела стен, fill=6.
        static constexpr int FORM_SIDES[7] = {0, 0, 3, 6, 8, 12, 0};
        if (form_ == 1) {
            g.set_param(id, "form", "square");
        } else if (form_ == 6) {
            g.set_param(id, "form", "plank");
        } else if (form_ >= 2 && form_ <= 5) {
            g.set_param(id, "sides", std::to_string(FORM_SIDES[form_]));
        }
        if (spin_deg_ > 0.01f && form_ != 0) {
            g.set_param(id, "angle_z", house_num(spin_deg_));
        }
    };
    ElementId made = NO_ELEMENT;
    if (to != NO_VERTEX && to != from) {
        const GraphResult r = session_->mutate("прямая между якорями", [&](HouseGraph& g) {
            const GraphResult add = g.add_element(ElementKind::Line, {from, to}, style_, made);
            if (add.ok) {
                stamp_draft(g, made);
            }
            return add;
        });
        if (!r.ok) {
            refusal_ = r.why;
            return;
        }
        last_ = made;
        session_->select_element(made);
        return;
    }
    // ОТПУСТИЛ В ПУСТОТЕ — ТАМ ПОЯВЛЯЕТСЯ ЯКОРЬ, И ПРЯМАЯ ИДЁТ К НЕМУ.
    //
    // Раньше здесь рождалась прямая С ОДНОЙ вершиной, а её дальний конец
    // описывался числами (длина и два угла). Пользователь назвал это точнее
    // меня: «прямая без якоря на конце — бессмыслица». Он прав, и не только по
    // смыслу — такой конец нельзя было ни схватить, ни соединить со второй
    // прямой, ни натянуть на него поверхность, а поиск оси такие прямые
    // пропускал вовсе (у них нет двух концов).
    //
    // Теперь у каждой прямой два якоря. Длина и углы никуда не делись: они
    // выражены положением второго якоря, и правятся тем же перетаскиванием, что
    // и всё остальное.
    const glm::vec3 gy_at = end;
    const float gy = world_ != nullptr && world_->ground_height
                         ? world_->ground_height({gy_at.x, gy_at.z})
                         : 0.0f;
    if (glm::length(end - session_->vertex_world(from)) < 1e-3f) {
        refusal_ = "прямая нулевой длины: потяни дальше";
        return;
    }
    const bool air = end.y - gy > HOUSE_AIR_EPS_M;
    VertexId tip = NO_VERTEX;
    const GraphResult r = session_->mutate("прямая до нового якоря", [&](HouseGraph& g) {
        tip = g.add_vertex(air ? Anchoring::Free : Anchoring::OnGround,
                           session_->to_local(air ? end : glm::vec3{end.x, gy, end.z}));
        const GraphResult add = g.add_element(ElementKind::Line, {from, tip}, style_, made);
        if (add.ok) {
            stamp_draft(g, made);
        }
        return add;
    });
    if (!r.ok) {
        refusal_ = r.why;
        return;
    }
    last_ = made;
    session_->select_element(made);
}

void HouseLineTool::on_deselected(ToolWorld& world) {
    (void)world;
    from_ = NO_VERTEX;
    refusal_.clear();
    clamp_hit_ = HouseClampHit{};
    snap_ = NO_VERTEX;
}

void HouseLineTool::on_cancel(ToolWorld& world) { on_deselected(world); }

ToolPreview HouseLineTool::preview(const ToolAim& aim) const {
    ToolPreview out;
    wire_.clear();
    ghost_.clear();
    if (session_ == nullptr) {
        return out;
    }
    HouseGroundFn ground;
    if (world_ != nullptr && world_->ground_height) {
        ground = world_->ground_height;
    }
    build_house_wire(*session_, ground, wire_);
    if (from_ != NO_VERTEX && !stale()) {
        const glm::vec3 a = session_->vertex_world(from_);
        const glm::vec3 b = ghost_end();
        ghost_.push_back(a);
        ghost_.push_back(b);
        // КОНЕЦ ПРИЗРАКА — ШАРИК С ОТВЕСОМ, как и всякая вершина: конец прямой
        // висит в воздухе ровно так же, и «над каким местом» про него
        // спрашивают так же.
        append_ball(wire_.accent, b, HOUSE_BALL_R_M);
        if (ground) {
            append_plumb(wire_.accent, b, ground({b.x, b.z}));
        }
    } else {
        (void)aim;
    }
    out.polyline = ghost_.empty() ? nullptr : &ghost_;
    out.handles = wire_.plain.empty() ? nullptr : &wire_.plain;
    out.accent = wire_.accent.empty() ? nullptr : &wire_.accent;
    out.line_color = HOUSE_ACCENT_COLOR;
    out.accent_color = HOUSE_ACCENT_COLOR;
    return out;
}

ToolStatus HouseLineTool::status(const ToolAim& aim) const {
    if (!refusal_.empty()) {
        return ToolStatus{"", refusal_, false};
    }
    if (session_ == nullptr) {
        return ToolStatus{"house.hint.nomodel", {}, false};
    }
    if (from_ == NO_VERTEX) {
        const bool on_anchor = session_->pick_vertex_ray(aim.origin, aim.direction(), HOUSE_GRAB_M) != NO_VERTEX;
        return ToolStatus{on_anchor ? "house.hint.line.from" : "house.hint.line.needanchor",
                          {}, on_anchor};
    }
    if (!session_->axis().free()) {
        // ВЕРТИКАЛЬ НАЗЫВАЕТ СЕБЯ ВЫСОТОЙ, а не словом «включено»: пользователь
        // 18.08 тянул вверх и получал прямую на траве, поэтому подпись обязана
        // отвечать на его вопрос — насколько вверх ушёл конец. Знак сохранён:
        // вниз от якоря — такая же законная стойка, как вверх.
        // РАССТОЯНИЕ СЧИТАЕТСЯ ВДОЛЬ САМОЙ ОСИ, а не по высоте: у вертикали это
        // одно и то же, а у лежачей балки высота всегда ноль — подпись
        // отвечала бы «0.00 м» на любое движение руки.
        glm::vec3 u{0.0f, 1.0f, 0.0f};
        (void)session_->axis_dir(from_, u);
        const float along = glm::dot(ghost_end() - session_->vertex_world(from_), u);
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s: %+.2f м от якоря (V — следующая ось)",
                      session_->axis_label().c_str(), static_cast<double>(along));
        return ToolStatus{"", buf, true};
    }
    if (clamp_hit_.found) {
        // ЗАЖИМ НАЗЫВАЕТ СЕБЯ ЧИСЛОМ. Молча укоротившаяся прямая выглядит как
        // промах руки, и человек тянет ещё раз — вместо того чтобы понять, что
        // это и есть заказанное поведение.
        char buf[96];
        std::snprintf(buf, sizeof(buf), "зажато до %.2f м (якорь v%u)",
                      static_cast<double>(clamp_hit_.length_m),
                      static_cast<unsigned>(clamp_hit_.at));
        return ToolStatus{"", buf, true};
    }
    return ToolStatus{"house.hint.line.drag", {}, true};
}

// ---------------------------------------------------------------------------
// 9 — ПОВЕРХНОСТЬ
// ---------------------------------------------------------------------------

ToolIdentity HouseSurfaceTool::identity() const {
    return ToolIdentity{"house.surface", "editor.tool.house.surface",
                        "tool.hint.house.surface", ToolIcon::HouseSurface};
}

bool HouseSurfaceTool::draft_normal(glm::vec3& out) const {
    if (session_ == nullptr || refs_.size() < 2 || stale()) {
        return false;
    }
    std::vector<glm::vec3> pts;
    pts.reserve(refs_.size());
    for (const VertexId r : refs_) {
        pts.push_back(session_->graph().resolved_local(r));
    }
    if (!closed_ || refs_.size() < 3) {
        // ЦЕПОЧКА: поперечина к первому отрезку — ТО ЖЕ ВЫРАЖЕНИЕ, что в
        // world::surface_normal. Стена выдавливается вверх, и её лицо смотрит
        // вбок, а не вдоль.
        const glm::vec3 d = pts[1] - pts[0];
        if (std::sqrt(d.x * d.x + d.z * d.z) < world::HOUSE_GEOM_EPS) {
            return false;
        }
        out = glm::normalize(glm::cross(d, glm::vec3{0.0f, 1.0f, 0.0f}));
    } else {
        const world::FittedPlane plane = world::fit_contour_plane(pts);
        if (plane.degenerate) {
            return false;
        }
        out = plane.normal;
    }
    if (flipped_) {
        out = -out;
    }
    // В МИРОВЫЕ: нормаль считалась в координатах постройки, а стрелка рисуется
    // в мире. Поворот без переноса — нормаль это направление, а не точка.
    out = session_->to_world(out) - session_->to_world({0.0f, 0.0f, 0.0f});
    return true;
}

void HouseSurfaceTool::on_press(const ToolAim& aim, ToolWorld& world) {
    refusal_.clear();
    if (session_ == nullptr) {
        return;
    }
    if (stale()) {
        // ОБХОД СОБРАН ИЗ ИМЁН, а имена после отмены другие. Дострой его — и
        // поверхность натянется на случайные якоря, потому что прежние номера
        // достались другим вершинам. Обход бросается целиком.
        clear_draft();
        last_ = NO_ELEMENT;
        seen_revision_ = session_->revision();
    }
    const VertexId hit = session_->pick_vertex_ray(aim.origin, aim.direction(), HOUSE_GRAB_M);
    if (hit == NO_VERTEX) {
        refusal_ = "щёлкай по якорям: поверхность натягивается на них";
        return;
    }
    if (!refs_.empty() && hit == refs_.front() && refs_.size() >= 3) {
        // ЗАМКНУЛ НА ПЕРВОЙ — ЭТО КОНТУР, и это ЖЕСТ, а не следствие чисел:
        // модель хранит closed отдельным полем именно затем, чтобы плоский пол
        // с заданной высотой не притворился стеной.
        closed_ = true;
        (void)confirm(world);
        return;
    }
    if (!refs_.empty() && hit == refs_.back()) {
        // ВТОРОЙ ЩЕЛЧОК ПО ПОСЛЕДНЕЙ — «ГОТОВО». Тот же жест, что у ломаной в
        // любом редакторе; он нужен, потому что Enter до инструмента сегодня не
        // доходит (клавиша живёт в таблице действий чужой зоны), а цепочка
        // обязана уметь заканчиваться мышью.
        (void)confirm(world);
        return;
    }
    if (std::find(refs_.begin(), refs_.end(), hit) != refs_.end()) {
        refusal_ = "этот якорь уже в обходе";
        return;
    }
    refs_.push_back(hit);
    session_->select_vertex(hit);
}

void HouseSurfaceTool::on_drag(const ToolAim& aim, float dt_s, ToolWorld& world) {
    (void)aim;
    (void)dt_s;
    (void)world;
}

void HouseSurfaceTool::on_release(ToolWorld& world) { (void)world; }

void HouseSurfaceTool::on_confirm(ToolWorld& world) { (void)confirm(world); }

void HouseSurfaceTool::on_cancel(ToolWorld& world) {
    (void)world;
    clear_draft();
}

void HouseSurfaceTool::on_deselected(ToolWorld& world) {
    (void)world;
    // ОБХОД НЕ ОСТАЁТСЯ В РУКАХ. Он ещё нигде не записан: поверхность
    // появляется только на подтверждении, поэтому «положить инструмент» значит
    // бросить набор, а не потерять сделанное.
    clear_draft();
}

void HouseSurfaceTool::undo_last() {
    if (!refs_.empty()) {
        refs_.pop_back();
    }
    closed_ = false;
}

void HouseSurfaceTool::clear_draft() {
    refs_.clear();
    closed_ = false;
    refusal_.clear();
}

bool HouseSurfaceTool::confirm(ToolWorld& world) {
    (void)world;
    refusal_.clear();
    if (session_ == nullptr) {
        return false;
    }
    if (stale()) {
        clear_draft();
        seen_revision_ = session_->revision();
        refusal_ = "обход собран до отмены — набери его заново";
        return false;
    }
    if (closed_ && refs_.size() < 3) {
        refusal_ = "контур нужен хотя бы из трёх якорей";
        return false;
    }
    if (refs_.size() < world::min_refs_for(ElementKind::Surface)) {
        refusal_ = "мало якорей: стена натягивается хотя бы на два";
        return false;
    }
    ElementId made = NO_ELEMENT;
    const std::vector<VertexId> refs = refs_;
    const bool closed = closed_;
    const GraphResult r = session_->mutate(closed ? "контур" : "стена",
                                           [&](HouseGraph& g) {
        const GraphResult add = g.add_element(ElementKind::Surface, refs, style_, made);
        if (!add.ok) {
            return add;
        }
        g.set_closed(made, closed);
        if (flipped_) {
            g.set_facing(made, true);
        }
        g.set_param(made, "thickness", house_num(thickness_m_));
        g.set_param(made, "mat", std::to_string(mat_));
        g.set_param(made, "tone", std::to_string(tone_));
        if (paint_ > 0) {
            g.set_param(made, "paint", std::to_string(paint_));
        }
        if (!closed && clad_) {
            g.set_param(made, "clad", "1");
        }
        if (!closed && fill_ >= 2) {
            g.set_param(made, "fill", std::to_string(fill_));
        }
        if (!closed && (clad_ || fill_ >= 2) && windows_ > 0) {
            g.set_param(made, "windows", std::to_string(windows_));
        }
        if (!closed && (clad_ || fill_ >= 2) && doors_ > 0) {
            g.set_param(made, "doors", std::to_string(doors_));
        }
        if (!closed) {
            // ВЫСОТА — ТОЛЬКО У ЦЕПОЧКИ. У контура она означала бы вторую
            // толщину, сказанную другим словом.
            g.set_param(made, "height", house_num(height_m_));
        }
        if (tex_deg_ != 0.0f) {
            g.set_param(made, "tex_deg", house_num(tex_deg_));
        }
        return add;
    });
    if (!r.ok) {
        refusal_ = r.why;
        return false;
    }
    last_ = made;
    clear_draft();
    session_->select_element(made);
    return true;
}

ToolPreview HouseSurfaceTool::preview(const ToolAim& aim) const {
    (void)aim;
    ToolPreview out;
    wire_.clear();
    draft_line_.clear();
    if (session_ == nullptr) {
        return out;
    }
    HouseGroundFn ground;
    if (world_ != nullptr && world_->ground_height) {
        ground = world_->ground_height;
    }
    build_house_wire(*session_, ground, wire_);

    if (!stale()) {
        // ОБХОД — ЖИРНОЙ ЛИНИЕЙ СО СТРЕЛКАМИ НАПРАВЛЕНИЯ (заказ 19.08: якоря
        // соединять «линиями обхода, со стрелками обхода, причём не такими
        // тонкими какие есть, а потолще»). Толщина у debug-линий одна на всех,
        // поэтому жирность набирается ПУЧКОМ: три параллельных отрезка со
        // сдвигом в перпендикуляре. Стрелка — на середине каждого звена: у
        // конца она сливалась бы с шариком якоря.
        for (const VertexId r : refs_) {
            // ВЫБРАННЫЕ ЯКОРЯ — ЖЁЛТЫМ, шариком в стопке подсветки: жёлтыми
            // должны быть ИМЕННО ОНИ, а не вся проволока.
            append_ball(wire_.accent, session_->vertex_world(r), HOUSE_BALL_R_M * 1.2f);
        }
        std::vector<glm::vec3> chain;
        for (const VertexId r : refs_) {
            chain.push_back(session_->vertex_world(r));
        }
        if (closed_ && refs_.size() >= 3) {
            chain.push_back(chain.front());
        }
        for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
            const glm::vec3 a = chain[i];
            const glm::vec3 b = chain[i + 1];
            const glm::vec3 d = b - a;
            const float len = glm::length(d);
            if (len < 1e-4f) {
                continue;
            }
            const glm::vec3 dir = d / len;
            glm::vec3 side = glm::cross(dir, glm::vec3{0.0f, 1.0f, 0.0f});
            if (glm::length(side) < 1e-3f) {
                side = glm::cross(dir, glm::vec3{1.0f, 0.0f, 0.0f});
            }
            side = glm::normalize(side) * 0.03f;
            for (const glm::vec3 off : {glm::vec3{0.0f}, side, -side}) {
                draft_line_.push_back(a + off);
                draft_line_.push_back(b + off);
            }
            // Стрелка направления: две черты назад от середины звена.
            const glm::vec3 mid = a + d * 0.5f;
            const glm::vec3 wing = glm::normalize(side) * 0.18f;
            draft_line_.push_back(mid);
            draft_line_.push_back(mid - dir * 0.35f + wing);
            draft_line_.push_back(mid);
            draft_line_.push_back(mid - dir * 0.35f - wing);
        }
    }
    // СТРЕЛКА НОРМАЛИ ДО ПОДТВЕРЖДЕНИЯ. Она и есть весь смысл этого превью:
    // порядок обхода задаёт лицо, а по ломаной на экране порядок не читается.
    glm::vec3 n{0.0f};
    if (draft_normal(n) && !refs_.empty()) {
        glm::vec3 centre{0.0f};
        for (const VertexId r : refs_) {
            centre += session_->vertex_world(r);
        }
        centre /= static_cast<float>(refs_.size());
        // ЧЕРНОВИК ЦЕПОЧКИ ПОДНИМАЕТСЯ НА ПОЛОВИНУ ВЫСОТЫ — как и готовая
        // стена: середина двух нижних якорей лежит на нижней кромке, и стрелка
        // росла из-под пола («рисуется от какой-то грани, а не в видном
        // месте»). У замкнутого контура высоты нет, и поправка нулевая.
        if (!closed_) {
            centre.y += height_m_ * 0.5f;
        }
        append_normal_arrow(wire_.accent, centre, n);
    }
    // Обход уезжает ПАРАМИ в стопку ghost_pairs (жёлтым): полилиния соединяла
    // бы пучки и стрелки сквозными штрихами. line_color БОЛЬШЕ НЕ ЖЁЛТЫЙ: он
    // красит ОБЫЧНУЮ проволоку, и жёлтая проволока целиком читалась как «все
    // якоря выбраны» — ровно жалоба 19.08.
    out.ghost_pairs = draft_line_.empty() ? nullptr : &draft_line_;
    out.ghost_color = HOUSE_ACCENT_COLOR;
    out.handles = wire_.plain.empty() ? nullptr : &wire_.plain;
    out.accent = wire_.accent.empty() ? nullptr : &wire_.accent;
    out.line_color = HOUSE_WIRE_COLOR;
    out.accent_color = HOUSE_ACCENT_COLOR;
    return out;
}

ToolStatus HouseSurfaceTool::status(const ToolAim& aim) const {
    (void)aim;
    if (!refusal_.empty()) {
        return ToolStatus{"", refusal_, false};
    }
    if (session_ == nullptr) {
        return ToolStatus{"house.hint.nomodel", {}, false};
    }
    if (refs_.empty()) {
        return ToolStatus{"house.hint.surface.first", {}, true};
    }
    // ЧТО ПОЛУЧИТСЯ ИЗ ЭТОГО ОБХОДА — НАЗЫВАЕТСЯ ДО ПОДТВЕРЖДЕНИЯ.
    //
    // Пользователь 18.08 поставил три якоря на полу, ждал ПОЛ, а получил стену:
    // «вектор нормали построился параллельный земле и по 2-м точкам». Так и
    // есть — открытая цепочка выдавливается вверх, и её лицо смотрит вбок, по
    // первому отрезку. Ошибки в геометрии нет, ошибка в том, что рука молчала о
    // разнице: пол и стена отличаются ОДНИМ ЖЕСТОМ, и жест этот ниоткуда не
    // виден.
    glm::vec3 n{0.0f};
    char buf[192];
    if (refs_.size() >= 3) {
        // ЧТО ИМЕННО ПОЛУЧИТСЯ, СЛОВАМИ ЧЕЛОВЕКА. Пользователь 18.08: «стены
        // рисуются не от якоря до якоря сверху» — и это про открытую цепочку,
        // которую выдавливает вверх ЧИСЛО из панели, а не верхние якоря.
        // Полотно ОТ ЯКОРЯ ДО ЯКОРЯ — это замкнутый обход: обошёл четыре угла,
        // замкнул на первом, и стена встала ровно по ним. Обе дороги названы,
        // потому что выбор между ними — это один щелчок, и он невидим.
        // ЛИЦО НАЗЫВАЕТСЯ ЗДЕСЬ ЖЕ. Стрелку с ребра камеры видно плохо, и слово
        // — второй прибор на тот же вопрос: пропав отсюда, оно оставило бы
        // человека без единственного способа узнать сторону, глядя на контур
        // сверху.
        const char* where = "вбок";
        if (draft_normal(n)) {
            where = n.y > 0.7f ? "вверх" : (n.y < -0.7f ? "вниз" : "вбок");
        }
        std::snprintf(buf, sizeof(buf),
                      "якорей %zu · лицо %s · замкни на ПЕРВОМ — полотно ровно по "
                      "якорям; по последнему — быстрая стена вверх на %.1f м",
                      refs_.size(), where, static_cast<double>(height_m_));
        return ToolStatus{"", buf, true};
    }
    if (draft_normal(n)) {
        // СЛОВАМИ, А НЕ ТОЛЬКО СТРЕЛКОЙ: стрелку с ребра камеры видно плохо,
        // а вопрос «куда смотрит лицо» задают именно тогда, когда смотрят на
        // контур сверху.
        const char* where = n.y > 0.7f ? "вверх" : (n.y < -0.7f ? "вниз" : "вбок");
        std::snprintf(buf, sizeof(buf), "якорей %zu · лицо %s (%.2f %.2f %.2f)",
                      refs_.size(), where, static_cast<double>(n.x),
                      static_cast<double>(n.y), static_cast<double>(n.z));
    } else {
        std::snprintf(buf, sizeof(buf), "якорей %zu · лицо пока не определено",
                      refs_.size());
    }
    return ToolStatus{"", buf, true};
}

} // namespace dfn::app
