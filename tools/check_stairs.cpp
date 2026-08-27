/*
Created: 27:08:2026 - 01:20:00
Last updated: 27:08:2026 - 11:20:22
Module: tools
File: tools/check_stairs.cpp

Responsibility:
- dfn_stairs_check: СУДЬЯ ВНУТРИДОМОВОГО МАРША. Три крита владельца (27.08)
  меряются по ВЫПЕЧКЕ, а не по коду рецепта:
    рука 1 — ПРОХОД ОТ ДВЕРИ: свободная полоса от дверного проёма вглубь дома
             в поясе от высоты шага до макушки; отдельно называется, если
             полосу занимает ТЕЛО МАРША;
    рука 2 — УКЛОН: подступёнок, проступь, угол, ход и подъём каждого марша;
    рука 3 — ПРОСВЕТ НАД ГОЛОВОЙ: капсула ставится на НОСОК каждой ступени
             (HOUSES.md §9.3) и меряется, что над ней;
    рука 4 — МАРШ ВНУТРИ ОБОЛОЧКИ: пробы марша против выпуклых тел чужих
             элементов — марш, вышедший сквозь стену наружу, называется числом;
    рука 5 — СТЫК ПАНДУСА: ходимая поверхность невидимого пандуса против пола
             внизу и настила наверху (ступенька на входе и на выходе).

Usage:
    dfn_stairs_check <файл.dfh | каталог> [...] [--all] [--tol-head 1.9]

  Запускать из корня репозитория. Ненулевой выход при находках.
  Без --all печатаются только тела С НАХОДКАМИ; с --all — все марши.

Dependencies:
- Uses: engine/world (HouseGraph, HouseFile, HouseMesh), engine/core (Constants).
- Used by: волна лестниц 27.08, человек, будущая приёмка кузницы.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- СУДЬЯ НЕ ЗОВЁТ КАЛЬКУЛЯТОР (HOUSES.md §9.3). Формулы длины проёма здесь нет
  и быть не может: две независимые дороги к одному числу — единственное, что
  способно поймать ошибку в любой из них. Всё, что печатает этот файл,
  получено из ТРЕУГОЛЬНИКОВ и ВЫПУКЛЫХ ТЕЛ собранного меша.
- МЕРИТСЯ ОТРЕЗКАМИ, А НЕ ГАБАРИТНЫМИ ЯЩИКАМИ (§14.1): ящик наклонной тетивы
  накрывает весь марш, и первая редакция такого судьи краснела на КАЖДОМ
  исправном марше.
- МЕРКИ ГЕРОЯ — ИЗ КОНСТАНТ. В день, когда герой подрастёт, дом обязан
  перестать проходить, а не остаться зелёным.
*/
/*
UPD:
- 27:08:2026 - 01:20:00: Создан. Волна лестниц: три крита владельца («марш у
- 27:08:2026 - 11:20:22: координатор: полоса прохода мерит высоту тела В ТОЧКЕ (барицентрика), а не ящиком треугольника — длинный наклонный подкос давал ложный блок; полотно двери исключено и из ПРОСВЕТА (створка открывается, потолком не является); в «УЗКО» печатается пол y0.
  входа», «слишком полого и длинно», «голова в потолок») ни одного прибора не
  имели — приёмка прошлых заходов печатала просвет по ПЛИТАМ, а бьёт не плита.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/geometric.hpp>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using dfn::world::ConvexPart;
using dfn::world::Element;
using dfn::world::ElementId;
using dfn::world::ElementParams;
using dfn::world::HouseGraph;
using dfn::world::HouseMesh;

// МЕРКИ ГЕРОЯ — из констант, не из головы.
constexpr float CAP_R = static_cast<float>(dfn::config::PLAYER_CAPSULE_RADIUS);
constexpr float CAP_H = static_cast<float>(dfn::config::PLAYER_CAPSULE_HEIGHT);
constexpr float STEP_H = static_cast<float>(dfn::config::PLAYER_STEP_HEIGHT);
constexpr float MAX_SLOPE = static_cast<float>(dfn::config::PLAYER_MAX_SLOPE);

/// ТРЕБУЕМЫЙ ПРОСВЕТ НАД МАРШЕМ, м. Крит владельца 27.08 дословно: «нельзя
/// пройти по лестнице — сверху в потолок упираешься». 1.90 — рост капсулы 1.80
/// плюс 0.10 запаса: порог, стоящий НА РАЗРЕШЕНИИ, запаса не имеет.
float g_head_tol = 1.90f;
/// ТРЕБУЕМАЯ ШИРИНА ПРОХОДА ОТ ДВЕРИ, м (крит 1: «надо их перепрыгивать»).
constexpr float PASS_W = 0.80f;
/// НАСКОЛЬКО ВГЛУБЬ ДОМА МЕРИТСЯ ЭТОТ ПРОХОД, м.
constexpr float PASS_LEN = 3.0f;

struct Tri {
    glm::vec3 a, b, c;
    ElementId owner = dfn::world::NO_ELEMENT;
    bool collider_only = false;
};

int g_findings = 0;
bool g_all = false;

// ---------------------------------------------------------------------------
// Мелкая геометрия
// ---------------------------------------------------------------------------

/// Квадрат расстояния от точки до треугольника В ПЛАНЕ (XZ).
///
/// ВЫРОЖДЕННЫЙ В ПЛАНЕ ТРЕУГОЛЬНИК МЕРИТСЯ ОТРЕЗКОМ, И ЭТО НЕ МЕЛОЧЬ: ЛИЦО
/// ЛЮБОЙ ВЕРТИКАЛЬНОЙ СТЕНЫ — это как раз такой треугольник (все три вершины
/// на одной прямой в плане). Знаковые проверки на нём дают три нуля, «внутри»
/// оказывается ЛЮБАЯ точка мира, и первый же прогон судьи объявил дверной
/// проход занятым во всех домах разом.
[[nodiscard]] float dist2_xz(glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c) {
    const auto seg = [](glm::vec2 q, glm::vec2 u, glm::vec2 v) {
        const glm::vec2 d = v - u;
        const float dd = glm::dot(d, d);
        float t = dd > 1e-12f ? glm::dot(q - u, d) / dd : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        const glm::vec2 w = q - (u + d * t);
        return glm::dot(w, w);
    };
    const float area2 = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (std::fabs(area2) < 1e-7f) {
        return std::min({seg(p, a, b), seg(p, b, c), seg(p, c, a)});
    }
    const float d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    const float d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
    const float d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
    const bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    if (!(neg && pos)) {
        return 0.0f;
    }
    return std::min({seg(p, a, b), seg(p, b, c), seg(p, c, a)});
}

/// НАИМЕНЬШАЯ ВЫСОТА ТРЕУГОЛЬНИКА В КРУГЕ радиуса r вокруг xz, считая только
/// то, что ВЫШЕ пола под ногами. Возвращает false, если круг треугольника не
/// задевает или всё его тело ниже ног.
///
/// ОТРЕЗКАМИ, А НЕ ЯЩИКОМ: точки берутся ПО САМОМУ треугольнику (сетка по
/// барицентрике), поэтому наклонная тетива отвечает своей высотой НАД НОСКОМ,
/// а не высотой своего ящика.
///
/// ВЫХОД С МАРША ПОТОЛКОМ НЕ ЯВЛЯЕТСЯ (§14.1). Отсекается он не по элементу, а
/// ПО МЕСТУ: проба, лежащая ДАЛЬШЕ верха марша по ходу и не выше его верха, —
/// это площадка, на которую ступают. Отсев по элементу («плита, чей верх совпал
/// с верхом марша») скрыл бы вместе с площадкой и тот кусок настила, что лёг
/// над НИЖНИМИ ступенями, — то есть ровно искомый дефект.
[[nodiscard]] bool tri_low_above(const Tri& t, glm::vec2 xz, float r, float foot_y,
                                 glm::vec3 foot, glm::vec3 dir, float run, float head_y,
                                 float& out_y) {
    const float top = std::max({t.a.y, t.b.y, t.c.y});
    if (top <= foot_y + 0.05f) {
        return false; // это пол под ногами, а не потолок над головой
    }
    // Дешёвый отсев по плану.
    if (dist2_xz(xz, {t.a.x, t.a.z}, {t.b.x, t.b.z}, {t.c.x, t.c.z}) > r * r) {
        return false;
    }
    constexpr int N = 8;
    bool any = false;
    float best = 1e9f;
    for (int i = 0; i <= N; ++i) {
        for (int j = 0; i + j <= N; ++j) {
            const float u = static_cast<float>(i) / static_cast<float>(N);
            const float v = static_cast<float>(j) / static_cast<float>(N);
            const glm::vec3 p = t.a * (1.0f - u - v) + t.b * u + t.c * v;
            if (p.y <= foot_y + 0.05f) {
                continue;
            }
            const float dx = p.x - xz.x;
            const float dz = p.z - xz.y;
            if (dx * dx + dz * dz > r * r) {
                continue;
            }
            const float s = glm::dot(p - foot, dir);
            // ПЛОЩАДКА ВЫХОДА И ЕЁ КОНСТРУКЦИЯ — НЕ ПОТОЛОК, и полоса отсева
            // равна РАДИУСУ КАПСУЛЫ, потому что этого требует её форма, а не
            // снисходительность. Капсула — цилиндр: у самой кромки проёма она
            // ВСЕГДА заходит под площадку на свой радиус, у любой исправной
            // лестницы мира, — а под площадкой законно висят и ригель обвязки
            // (0.22 ниже изнанки), и накат (0.25). Носок ступени НИЖЕ них
            // ровно потому, что человек ещё не поднялся; это перешагивают, а
            // не бьются об это головой. Мерить тут — значит краснеть на
            // предпоследней ступени КАЖДОГО марша (что первый прогон и делал).
            // Запас по высоте — PLAYER_STEP_HEIGHT, и это не поблажка, а та же
            // константа контроллера: то, что в зоне выхода лежит НЕ ВЫШЕ ОДНОГО
            // ШАГА над верхом марша, — это следующая ступень, на которую
            // поднимаются, а не потолок, в который упираются. Так устроено
            // всякое крыльцо пакета: марш до 0.39, плита площадки 0.45,
            // каменная ступень порога 0.54, пол 0.57.
            if (s > run - r && p.y <= head_y + STEP_H) {
                continue;
            }
            if (p.y < best) {
                best = p.y;
                any = true;
            }
        }
    }
    out_y = best;
    return any;
}

/// Точка ВНУТРИ выпуклой призмы коллайдера (шесть точек: низ 0-2, верх 3-5).
/// Глубина захода — расстояние до ближайшей грани, 0 снаружи.
[[nodiscard]] float convex_depth(const ConvexPart& cp, glm::vec3 p) {
    if (cp.points.size() != 6) {
        return 0.0f;
    }
    glm::vec3 mid{0.0f};
    for (const glm::vec3& q : cp.points) {
        mid += q;
    }
    mid /= 6.0f;
    const int face[5][3] = {{0, 1, 2}, {3, 4, 5}, {0, 1, 4}, {1, 2, 5}, {2, 0, 3}};
    float depth = 1e9f;
    for (const auto& fq : face) {
        const glm::vec3& a = cp.points[static_cast<std::size_t>(fq[0])];
        const glm::vec3& b = cp.points[static_cast<std::size_t>(fq[1])];
        const glm::vec3& c = cp.points[static_cast<std::size_t>(fq[2])];
        glm::vec3 n = glm::cross(b - a, c - a);
        const float len = glm::length(n);
        if (len < 1e-6f) {
            continue;
        }
        n /= len;
        if (glm::dot(mid - a, n) < 0.0f) {
            n = -n;
        }
        const float d = glm::dot(p - a, n);
        if (d <= 0.0f) {
            return 0.0f;
        }
        depth = std::min(depth, d);
    }
    return depth < 1e8f ? depth : 0.0f;
}

// ---------------------------------------------------------------------------
// Разбор одного дома
// ---------------------------------------------------------------------------

struct Flight {
    ElementId id = dfn::world::NO_ELEMENT;
    glm::vec3 foot{0.0f};  ///< середина нижней кромки
    glm::vec3 head{0.0f};  ///< середина верхней кромки
    glm::vec3 dir{0.0f};   ///< единичный, в плане
    float half_w = 0.0f;
    float run = 0.0f;
    float rise_total = 0.0f;
    int steps = 0;
    bool open = false;
};

[[nodiscard]] bool read_graph(const std::string& path, HouseGraph& g) {
    std::ifstream in(path);
    if (!in.good()) {
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return dfn::world::read_house(ss.str(), g).ok;
}

/// ЧИСЛО СТУПЕНЕЙ СУДЬЯ НЕ СЧИТАЕТ, А ЧИТАЕТ ПО МЕШУ: он берёт РАЗНЫЕ высоты
/// ГОРИЗОНТАЛЬНЫХ верхних граней этого элемента. Формулу движка он повторять не
/// имеет права (§9.3) — иначе обе дороги ошибутся одинаково.
///
/// «ГОРИЗОНТАЛЬНЫХ» КУПЛЕНО ПЕРВЫМ ПРОГОНОМ: без него в счёт шли лица ТЕТИВ —
/// наклонные полосы во всю длину марша, у которых верхняя высота у каждого
/// треугольника своя. Марш из пятнадцати ступеней объявлялся тридцатидвух-
/// ступенчатым, и вместе с числом уезжали проступь, подступёнок и угол.
[[nodiscard]] int steps_from_mesh(const std::vector<Tri>& own, float y_lo, float y_hi) {
    std::set<int> tops;
    for (const Tri& t : own) {
        if (t.collider_only) {
            continue;
        }
        const glm::vec3 n = glm::cross(t.b - t.a, t.c - t.a);
        const float len = glm::length(n);
        if (len < 1e-8f || n.y / len < 0.98f) {
            continue; // не проступь: подступёнок, фаска, тетива, ИЗНАНКА доски
        }
        const float y = std::max({t.a.y, t.b.y, t.c.y});
        if (y < y_lo - 0.01f || y > y_hi + 0.01f) {
            continue;
        }
        tops.insert(static_cast<int>(std::lround(y * 1000.0f)));
    }
    // Уровни, отстоящие меньше чем на 2 см, — одна ступень (дрожь износа).
    int n = 0;
    int prev = -1000000;
    for (const int v : tops) {
        if (v - prev > 20) {
            ++n;
        }
        prev = v;
    }
    return n;
}

void check_file(const std::string& path) {
    HouseGraph g;
    if (!read_graph(path, g)) {
        std::printf("%s: НЕ ПРОЧИТАЛСЯ\n", path.c_str());
        ++g_findings;
        return;
    }
    const HouseMesh built = dfn::world::build_house_mesh(g);

    std::vector<Tri> tris;
    for (const dfn::world::MeshPart& part : built.parts) {
        for (std::uint32_t i = 0; i + 2 < part.index_count; i += 3) {
            Tri t;
            t.a = built.vertices[built.indices[part.index_begin + i]].pos;
            t.b = built.vertices[built.indices[part.index_begin + i + 1]].pos;
            t.c = built.vertices[built.indices[part.index_begin + i + 2]].pos;
            t.owner = part.element;
            t.collider_only = part.collider_only;
            tris.push_back(t);
        }
    }
    if (tris.empty()) {
        return;
    }

    // ---- марши: контур fill=6 либо прямая stairs=1 -------------------------
    std::vector<Flight> flights;
    for (const Element& e : g.elements()) {
        const ElementParams p = dfn::world::element_params_of(e, nullptr);
        const bool contour_stairs =
            dfn::world::fill_kind(p) == dfn::world::WallFill::Stairs && e.refs.size() >= 4;
        const bool line_stairs = p.stairs > 0.5f && e.refs.size() >= 2;
        if (!contour_stairs && !line_stairs) {
            continue;
        }
        Flight fl;
        fl.id = e.id;
        fl.open = p.open > 0.5f;
        if (contour_stairs) {
            std::vector<glm::vec3> pts;
            for (const auto& r : e.refs) {
                pts.push_back(g.resolved_local(r));
            }
            std::sort(pts.begin(), pts.end(),
                      [](const glm::vec3& l, const glm::vec3& r) { return l.y < r.y; });
            fl.foot = (pts[0] + pts[1]) * 0.5f;
            fl.head = (pts[pts.size() - 1] + pts[pts.size() - 2]) * 0.5f;
            fl.half_w = std::max(glm::length(pts[1] - pts[0]) * 0.5f, 0.15f);
        } else {
            glm::vec3 a = g.resolved_local(e.refs[0]);
            glm::vec3 b = g.resolved_local(e.refs[1]);
            if (b.y < a.y) {
                std::swap(a, b);
            }
            fl.foot = a;
            fl.head = b;
            fl.half_w = std::max(p.radius, 0.15f);
        }
        const glm::vec3 d = fl.head - fl.foot;
        fl.run = std::sqrt(d.x * d.x + d.z * d.z);
        fl.rise_total = d.y;
        if (fl.run < 1e-4f || fl.rise_total < 1e-4f) {
            continue;
        }
        fl.dir = glm::vec3{d.x / fl.run, 0.0f, d.z / fl.run};
        flights.push_back(fl);
    }
    if (flights.empty()) {
        return;
    }

    std::vector<std::string> lines;
    std::vector<std::string> bad;
    const std::string name = std::filesystem::path(path).filename().string();

    // ---- РУКА 1: ПРОХОД ОТ ДВЕРИ ------------------------------------------
    // Дверная створка (door=1) — единственное тело, которое знает, ГДЕ проём:
    // раскладка ставит его сама, и рецепт этого числа не хранит.
    std::vector<Tri> stair_tris;
    std::set<ElementId> stair_ids;
    for (const Flight& fl : flights) {
        stair_ids.insert(fl.id);
    }
    for (const Tri& t : tris) {
        if (stair_ids.count(t.owner) != 0) {
            stair_tris.push_back(t);
        }
    }
    // СТЕНА С ПРОЁМОМ САМА ПРОХОДУ НЕ МЕШАЕТ, и это не поблажка: раскладка
    // вырезает в ней дверь, а судья видит только треугольники — тело стены
    // толщиной 0.42 закрывало собой первые 0.21 м пути и объявляло проход
    // занятым во ВСЕХ домах с толстой кладкой. Створка (door=1) тоже не
    // считается: она качается и в коллайдер не входит (AppHouse).
    // ЭТО ПОСТРОЙКА ИЛИ ОТДЕЛЬНАЯ ДЕТАЛЬ? Рецепты уличных маршей и крылец
    // (city-stoop*, city-steps*) — ДЕТАЛИ: площадку наверху им даёт мостовая,
    // которую кладёт генератор города, и требовать её внутри .dfh значит
    // судить деталь по правилам дома. Признак — наличие хоть одной стены.
    bool has_walls = false;
    std::set<ElementId> door_bodies;
    for (const Element& e : g.elements()) {
        const ElementParams p = dfn::world::element_params_of(e, nullptr);
        if (p.height > 0.0f) {
            has_walls = true;
        }
        if (p.doors > 0.5f || g.param(e.id, "door") == "1") {
            door_bodies.insert(e.id);
        }
    }
    glm::vec2 plan_lo{1e9f};
    glm::vec2 plan_hi{-1e9f};
    for (const Tri& t : tris) {
        for (const glm::vec3& q : {t.a, t.b, t.c}) {
            plan_lo = glm::min(plan_lo, {q.x, q.z});
            plan_hi = glm::max(plan_hi, {q.x, q.z});
        }
    }
    const glm::vec2 plan_mid = (plan_lo + plan_hi) * 0.5f;

    for (const Element& e : g.elements()) {
        if (g.param(e.id, "door") != "1" || e.refs.size() < 2) {
            continue;
        }
        const glm::vec3 a = g.resolved_local(e.refs[0]);
        const glm::vec3 b = g.resolved_local(e.refs[1]);
        const glm::vec3 c = (a + b) * 0.5f;
        glm::vec2 ax{b.x - a.x, b.z - a.z};
        if (glm::length(ax) < 1e-4f) {
            continue;
        }
        ax = glm::normalize(ax);
        glm::vec2 n{-ax.y, ax.x};
        if (glm::dot(plan_mid - glm::vec2{c.x, c.z}, n) < 0.0f) {
            n = -n;
        }
        // ПОЛОСА МЕРИТСЯ В ПОЯСЕ ВЫШЕ ШАГА: всё, что ниже PLAYER_STEP_HEIGHT,
        // контроллер берёт сам, и ступень марша препятствием не является. То,
        // что торчит В ПОЯСЕ, — это то, что «надо перепрыгивать».
        //
        // ОТСЧЁТ ОТ ПОЛА, А НЕ ОТ ПОДОШВЫ СТВОРКИ: у части рецептов створка
        // авторизована от нуля постройки, а пол лежит на цоколе, и полоса
        // уезжала вниз на всю его высоту — судья ловил ЛЕНТУ ФУНДАМЕНТА и
        // объявлял занятой дверь, в которую входят.
        float y0 = c.y;
        {
            const glm::vec2 in = glm::vec2{c.x, c.z} + n * 0.7f;
            float best = -1e9f;
            for (const Tri& t : tris) {
                const glm::vec3 nn = glm::cross(t.b - t.a, t.c - t.a);
                const float len = glm::length(nn);
                if (len < 1e-8f || nn.y / len < 0.9f) {
                    continue;
                }
                const float y = std::max({t.a.y, t.b.y, t.c.y});
                if (y > c.y + 0.8f || y < c.y - 1.5f) {
                    continue;
                }
                if (dist2_xz(in, {t.a.x, t.a.z}, {t.b.x, t.b.z}, {t.c.x, t.c.z}) > 0.09f) {
                    continue;
                }
                best = std::max(best, y);
            }
            if (best > -1e8f) {
                y0 = best;
            }
        }
        float worst_w = 9.0f;
        float worst_d = 0.0f;
        bool by_stair = false;
        ElementId worst_by = dfn::world::NO_ELEMENT;
        for (float dd = 0.15f; dd <= PASS_LEN + 1e-3f; dd += 0.1f) {
            const glm::vec2 base = glm::vec2{c.x, c.z} + n * dd;
            // Свободная полоса ВОКРУГ ОСИ ДВЕРИ: идём в обе стороны, пока
            // упираемся, и складываем.
            float left = 0.0f;
            float right = 0.0f;
            bool stair_hit = false;
            ElementId hit_by = dfn::world::NO_ELEMENT;
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                float u = 0.0f;
                for (; u <= 1.2f; u += 0.05f) {
                    const glm::vec2 q = base + ax * (u * static_cast<float>(sgn));
                    bool blocked = false;
                    for (const Tri& t : tris) {
                        if (t.collider_only || door_bodies.count(t.owner) != 0) {
                            continue;
                        }
                        const float lo = std::min({t.a.y, t.b.y, t.c.y});
                        const float hi = std::max({t.a.y, t.b.y, t.c.y});
                        if (hi < y0 + STEP_H || lo > y0 + CAP_H) {
                            continue;
                        }
                        if (dist2_xz(q, {t.a.x, t.a.z}, {t.b.x, t.b.z},
                                     {t.c.x, t.c.z}) > 1e-6f) {
                            continue;
                        }
                        // ЯЩИК ТРЕУГОЛЬНИКА — НЕ ЕГО ВЫСОТА В ТОЧКЕ (§14.1 тем
                        // же принципом: отрезками, а не ящиком). Длинное
                        // наклонное тело — подкос столба мельницы — лежит в
                        // плане поперёк всего корпуса, его lo..hi пересекает
                        // пояс ВЕЗДЕ, а сама балка в створе двери давно под
                        // полом. Для невырожденного в плане треугольника
                        // высота берётся интерполяцией В ТОЧКЕ q; вырожденный
                        // (лицо вертикальной стены) остаётся на ящике — у него
                        // интерполяции нет, а lo..hi и есть его высота.
                        {
                            const glm::vec2 ta{t.a.x, t.a.z};
                            const glm::vec2 tb{t.b.x, t.b.z};
                            const glm::vec2 tc{t.c.x, t.c.z};
                            const float area2 = (tb.x - ta.x) * (tc.y - ta.y)
                                              - (tc.x - ta.x) * (tb.y - ta.y);
                            if (std::fabs(area2) > 1e-7f) {
                                const float w0 = ((tb.x - q.x) * (tc.y - q.y)
                                                - (tc.x - q.x) * (tb.y - q.y)) / area2;
                                const float w1 = ((tc.x - q.x) * (ta.y - q.y)
                                                - (ta.x - q.x) * (tc.y - q.y)) / area2;
                                const float w2 = 1.0f - w0 - w1;
                                const float yq = t.a.y * w0 + t.b.y * w1 + t.c.y * w2;
                                if (yq < y0 + STEP_H || yq > y0 + CAP_H) {
                                    continue;
                                }
                            }
                        }
                        blocked = true;
                        hit_by = t.owner;
                        if (stair_ids.count(t.owner) != 0) {
                            stair_hit = true;
                        }
                        break;
                    }
                    if (blocked) {
                        break;
                    }
                }
                (sgn < 0 ? left : right) = u;
            }
            const float w = left + right;
            if (w < worst_w) {
                worst_w = w;
                worst_d = dd;
                by_stair = stair_hit;
                worst_by = hit_by;
            }
        }
        char buf[256];
        if (worst_w + 1e-3f < PASS_W) {
            std::snprintf(buf, sizeof(buf),
                          "  проход от двери (%.2f, %.2f): УЗКО %.2f м на %.2f м "
                          "вглубь, мешает e%u%s [пол y0=%.2f]",
                          c.x, c.z, worst_w, worst_d,
                          static_cast<unsigned>(worst_by),
                          by_stair ? " — ЭТО ТЕЛО МАРША" : "", y0);
            bad.emplace_back(buf);
        } else {
            std::snprintf(buf, sizeof(buf),
                          "  проход от двери (%.2f, %.2f): свободен, узкое место %.2f м",
                          c.x, c.z, worst_w);
            lines.emplace_back(buf);
        }
    }

    // ---- РУКИ 2..5 по каждому маршу ---------------------------------------
    for (const Flight& fl : flights) {
        std::vector<Tri> own;
        for (const Tri& t : tris) {
            if (t.owner == fl.id) {
                own.push_back(t);
            }
        }
        const int steps = steps_from_mesh(own, fl.foot.y, fl.head.y + 0.02f);
        const float rise = steps > 0 ? fl.rise_total / static_cast<float>(steps) : 0.0f;
        const float tread = steps > 0 ? fl.run / static_cast<float>(steps) : 0.0f;
        const float deg = std::atan2(fl.rise_total, fl.run) * 180.0f / 3.14159265f;

        char head[320];
        std::snprintf(head, sizeof(head),
                      "  e%u: подъём %.2f, ход %.2f, ступеней %d, подступёнок %.3f, "
                      "проступь %.3f, угол %.1f гр.",
                      static_cast<unsigned>(fl.id), fl.rise_total, fl.run, steps, rise,
                      tread, deg);
        lines.emplace_back(head);
        if (deg > MAX_SLOPE * 180.0f / 3.14159265f) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "  e%u: УКЛОН %.1f гр. КРУЧЕ ПРОХОДИМОГО %.1f гр.",
                          static_cast<unsigned>(fl.id), deg,
                          MAX_SLOPE * 180.0f / 3.14159265f);
            bad.emplace_back(buf);
        }
        if (rise > STEP_H) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "  e%u: ПОДСТУПЁНОК %.3f ВЫШЕ ШАГА %.2f",
                          static_cast<unsigned>(fl.id), rise, STEP_H);
            bad.emplace_back(buf);
        }

        // -- рука 3: просвет над носком каждой ступени -----------------------
        float min_head = 1e9f;
        int min_step = 0;
        ElementId min_by = dfn::world::NO_ELEMENT;
        glm::vec3 min_at{0.0f};
        for (int k = 1; k <= steps; ++k) {
            const glm::vec3 nose = fl.foot + fl.dir * (tread * static_cast<float>(k))
                                 + glm::vec3{0.0f, rise * static_cast<float>(k), 0.0f};
            float ceil_y = 1e9f;
            ElementId who = dfn::world::NO_ELEMENT;
            for (const Tri& t : tris) {
                // Полотно двери потолком не является — оно ОТКРЫВАЕТСЯ. Марш,
                // приводящий к закрытой створке (амбар, мельница), краснел
                // просветом 0.5 у последней ступени; проход полотна и так
                // исключает (та же строка ниже), просвет забывал.
                if (t.owner == fl.id || t.collider_only ||
                    door_bodies.count(t.owner) != 0) {
                    continue;
                }
                float y = 0.0f;
                if (tri_low_above(t, {nose.x, nose.z}, CAP_R, nose.y, fl.foot, fl.dir,
                                  fl.run, fl.head.y, y)) {
                    if (y < ceil_y) {
                        ceil_y = y;
                        who = t.owner;
                    }
                }
            }
            const float clear = ceil_y - nose.y;
            if (clear < min_head) {
                min_head = clear;
                min_step = k;
                min_by = who;
                min_at = nose;
            }
        }
        if (min_head > 1e8f) {
            lines.emplace_back("    просвет: над маршем ничего нет");
        } else {
            char buf[256];
            if (min_head + 1e-3f < g_head_tol) {
                std::snprintf(buf, sizeof(buf),
                              "  e%u: ПРОСВЕТ %.3f м НА СТУПЕНИ %d (норма %.2f) — "
                              "мешает e%u, носок (%.2f, %.2f, %.2f)",
                              static_cast<unsigned>(fl.id), min_head, min_step,
                              g_head_tol, static_cast<unsigned>(min_by), min_at.x,
                              min_at.y, min_at.z);
                bad.emplace_back(buf);
            } else {
                std::snprintf(buf, sizeof(buf), "    просвет: %.3f м (ступень %d)",
                              min_head, min_step);
                lines.emplace_back(buf);
            }
        }

        // -- рука 4: марш внутри оболочки ------------------------------------
        float worst_in = 0.0f;
        glm::vec3 worst_at{0.0f};
        for (int k = 0; k <= steps; ++k) {
            const float s = fl.run * static_cast<float>(k) / static_cast<float>(steps > 0 ? steps : 1);
            const glm::vec3 base = fl.foot + fl.dir * s
                                 + glm::vec3{0.0f, fl.rise_total * static_cast<float>(k)
                                                       / static_cast<float>(steps > 0 ? steps : 1),
                                             0.0f};
            const glm::vec3 side = glm::normalize(
                glm::cross(fl.dir, glm::vec3{0.0f, 1.0f, 0.0f}));
            for (const float u : {-1.0f, 0.0f, 1.0f}) {
                const glm::vec3 q = base + side * (fl.half_w * u * 0.95f)
                                  + glm::vec3{0.0f, 0.4f, 0.0f};
                for (const ConvexPart& cp : built.convex) {
                    if (cp.element == fl.id) {
                        continue;
                    }
                    const ElementParams pp =
                        dfn::world::element_params_of(*g.element(cp.element), nullptr);
                    if (pp.height <= 0.0f) {
                        continue; // судим по СТЕНАМ: плита под маршем — опора
                    }
                    const float d = convex_depth(cp, q);
                    if (d > worst_in) {
                        worst_in = d;
                        worst_at = q;
                    }
                }
            }
        }
        if (worst_in > 0.05f) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "  e%u: МАРШ В СТЕНЕ на %.3f м (%.2f, %.2f, %.2f)",
                          static_cast<unsigned>(fl.id), worst_in, worst_at.x, worst_at.y,
                          worst_at.z);
            bad.emplace_back(buf);
        }

        // -- рука 5: СТУПЕНЬКА НА ВХОДЕ И НА ВЫХОДЕ ---------------------------
        // Меряется ПО ТОМУ, ПО ЧЕМУ ХОДЯТ, а не по якорям рецепта: у
        // невидимого пандуса берётся его ходимая поверхность, у комнаты и у
        // площадки — верх ближайшей горизонтальной грани в 0.45 м до подножия
        // и в 0.45 м за верхом. Якорь марша у части рецептов стоит на
        // СРЕДИННОЙ плоскости настила, и разность якорей соврала бы на
        // пол-толщины в сторону «всё хорошо».
        if (fl.open) {
            float ramp_lo = -1e9f;
            float ramp_hi = -1e9f;
            for (const Tri& t : own) {
                if (!t.collider_only) {
                    continue;
                }
                for (const glm::vec3& q : {t.a, t.b, t.c}) {
                    const float s = glm::dot(q - fl.foot, fl.dir);
                    if (s < 0.05f) {
                        ramp_lo = std::max(ramp_lo, q.y);
                    }
                    if (s > fl.run - 0.05f) {
                        ramp_hi = std::max(ramp_hi, q.y);
                    }
                }
            }
            // ПОЛ — ЭТО НЕ «САМОЕ ВЫСОКОЕ ГОРИЗОНТАЛЬНОЕ», А БЛИЖАЙШЕЕ К КОНЦУ
            // МАРША. Оба уточнения куплены ложными тревогами: «самое высокое»
            // брало верх КАМЕННОЙ ЗАВАЛИНКИ вдоль стены (на 0.39 выше пола), а
            // после сужения полосы — её же на 0.10 выше. Завалинка стоит именно
            // там, где кончается марш, и по высоте от пола не отличается ничем,
            // кроме того, что она НЕ ПОЛ; отличает её только расстояние.
            const auto surface_at = [&](glm::vec3 probe, float up, float down) {
                float best = -1e9f;
                float best_d = 1e9f;
                for (const Tri& t : tris) {
                    if (t.owner == fl.id) {
                        continue;
                    }
                    const glm::vec3 n = glm::cross(t.b - t.a, t.c - t.a);
                    const float len = glm::length(n);
                    if (len < 1e-8f || n.y / len < 0.9f) {
                        continue;
                    }
                    const float y = std::max({t.a.y, t.b.y, t.c.y});
                    if (y > probe.y + up || y < probe.y - down) {
                        continue;
                    }
                    if (dist2_xz({probe.x, probe.z}, {t.a.x, t.a.z}, {t.b.x, t.b.z},
                                 {t.c.x, t.c.z}) > 0.09f) {
                        continue;
                    }
                    const float d = std::fabs(y - probe.y);
                    if (d < best_d) {
                        best_d = d;
                        best = y;
                    }
                }
                return best;
            };
            const float floor_y = surface_at(fl.foot - fl.dir * 0.45f, 0.10f, 0.6f);
            const float land_y = surface_at(fl.head + fl.dir * 0.45f, 0.20f, 0.6f);
            // НАВЕРХУ ОБЯЗАНО БЫТЬ НА ЧТО СТУПИТЬ, И ЭТО ОТДЕЛЬНАЯ НАХОДКА, а
            // не молчание. Первая редакция руки 5 просто пропускала марш, у
            // которого площадка не нашлась, — и оба замка Вайтрана прошли
            // приёмку с маршем, кончавшимся В ВОЗДУХЕ в четырёх метрах от
            // кромки верхнего пола. Прибор, у которого «нет данных» и «всё
            // хорошо» выглядят одинаково, не прибор.
            if (land_y <= -1e8f && has_walls) {
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                              "  e%u: МАРШ ОБРЫВАЕТСЯ — наверху (%.2f, %.2f, %.2f) "
                              "не на что ступить",
                              static_cast<unsigned>(fl.id), fl.head.x, fl.head.y,
                              fl.head.z);
                bad.emplace_back(buf);
            }
            if (ramp_lo > -1e8f && ramp_hi > -1e8f && floor_y > -1e8f && land_y > -1e8f) {
                const float d_lo = ramp_lo - floor_y;
                const float d_hi = land_y - ramp_hi;
                char buf[256];
                if (std::fabs(d_lo) > 0.06f || std::fabs(d_hi) > 0.06f) {
                    std::snprintf(buf, sizeof(buf),
                                  "  e%u: СТУПЕНЬКА У МАРША — на входе %+.3f, "
                                  "на выходе %+.3f",
                                  static_cast<unsigned>(fl.id), d_lo, d_hi);
                    bad.emplace_back(buf);
                } else {
                    std::snprintf(buf, sizeof(buf),
                                  "    стык: на входе %+.3f, на выходе %+.3f", d_lo,
                                  d_hi);
                    lines.emplace_back(buf);
                }
            }
        }
    }

    if (bad.empty() && !g_all) {
        return;
    }
    std::printf("%s\n", name.c_str());
    if (g_all) {
        for (const std::string& s : lines) {
            std::printf("%s\n", s.c_str());
        }
    }
    for (const std::string& s : bad) {
        std::printf("%s\n", s.c_str());
        ++g_findings;
    }
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--all") {
            g_all = true;
            continue;
        }
        if (a == "--tol-head" && i + 1 < argc) {
            g_head_tol = std::strtof(argv[++i], nullptr);
            continue;
        }
        if (std::filesystem::is_directory(a)) {
            std::vector<std::string> got;
            for (const auto& de : std::filesystem::directory_iterator(a)) {
                if (de.path().extension() == ".dfh") {
                    got.push_back(de.path().string());
                }
            }
            std::sort(got.begin(), got.end());
            files.insert(files.end(), got.begin(), got.end());
            continue;
        }
        files.push_back(a);
    }
    if (files.empty()) {
        std::fprintf(stderr,
                     "dfn_stairs_check: нечего судить\n"
                     "  dfn_stairs_check <файл.dfh | каталог> [--all] [--tol-head 1.9]\n");
        return 2;
    }
    for (const std::string& f : files) {
        check_file(f);
    }
    std::printf("dfn_stairs_check: тел %zu, находок %d\n", files.size(), g_findings);
    return g_findings == 0 ? 0 : 1;
}
