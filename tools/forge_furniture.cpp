/*
Created: 21:08:2026 - 13:20:00
Last updated: 21:08:2026 - 13:20:00
Module: tools
File: tools/forge_furniture.cpp

Responsibility:
- КУЗНИЦА ВНУТРЕННЕГО ОФОРМЛЕНИЯ (заказ 21.08: «объекты внутреннего
  оформления домов»). Каждый предмет — ОТДЕЛЬНАЯ ГОТОВАЯ ПОСТРОЙКА в
  assets/houses/furn-*.dfh: стол, лавка, кровать, стеллаж, несущая колонна с
  уголками-подкосами, каменный очаг. Собираются тем же HouseGraph API и
  пишутся тем же каноническим write_house, что и дома, — расстановка внутри
  комнаты ничем не отличается от расстановки дома на улице.

Key items:
- Forge: рука над графом (дедуп вершин, slab/panel/bar/rim).
- table/bench/bed/shelf/column/hearth: шесть рецептов.

Dependencies:
- Uses: engine/world (HouseGraph, HouseFile).
- Used by: цель dfn_furniture; артефакты читают сцена и редактор.

Notes:
- ЧЕЛОВЕЧЕСКИЕ ЧИСЛА (docs/INTERIOR_CATALOG.md §9, обмеры по росту 1.8 м):
  стол 1.8x0.9x0.78, лавка 1.6x0.35x0.45, кровать 1.9x0.85, стеллаж 3 полки,
  столб длинного дома 0.45 с подкосами под 45°, очаг-короб 1.4x1.4x0.35.
- ЛОКАЛЬНЫЕ КООРДИНАТЫ, как у домов: начало — северо-западный угол пятна,
  +X на восток, +Z на юг, вершины Free с явной высотой (предмет ставится на
  пол комнаты, высота из рельефа сделала бы файл недетерминированным).
- У ПРЕДМЕТА НЕТ СТЕН С ПРОЁМАМИ: только балки (Line), плиты-контуры и один
  низкий бортик очага. doors/windows/clad/porch здесь не бывает — раскладка
  фасада на тумбочке даёт дверь в полтора её роста.
- ТОЛЩИНА ПЛИТЫ СИММЕТРИЧНА срединной плоскости (HousePlate.cpp), а человек
  называет ВЕРХ («столешница на 0.80») — slab() принимает верх и опускает
  якоря сам.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Правки предметов — только здесь,
  артефакты .dfh перегенерировать, не править руками.
*/
/*
UPD:
- 21:08:2026 - 13:20:00: Создана: шесть предметов интерьера (стол, лавка, кровать, стеллаж, колонна с подкосами, каменный очаг).
*/

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseGraph.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <fstream>
#include <initializer_list>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using dfn::world::Anchoring;
using dfn::world::ElementId;
using dfn::world::ElementKind;
using dfn::world::HouseGraph;
using dfn::world::VertexId;

using Params = std::initializer_list<std::pair<const char*, const char*>>;

/// Рука над графом: рецепт читается как список деталей, а не как API-вязь.
struct Forge {
    HouseGraph g;
    /// ОДНА ВЕРШИНА НА ТОЧКУ: у мебели точки совпадают чаще, чем у дома (ось
    /// ножки и угол царги — одна точка), а граф не дедуплицирует сам —
    /// иначе пользователь тянет угол стола, и столешница уезжает без ножки.
    std::map<std::tuple<int, int, int>, VertexId> known;

    VertexId v(float x, float y, float z) {
        const auto key = std::make_tuple(static_cast<int>(std::lround(x * 1000.0f)),
                                         static_cast<int>(std::lround(y * 1000.0f)),
                                         static_cast<int>(std::lround(z * 1000.0f)));
        if (const auto it = known.find(key); it != known.end()) {
            return it->second;
        }
        const VertexId id = g.add_vertex(Anchoring::Free, {x, y, z});
        known.emplace(key, id);
        return id;
    }

    VertexId v(glm::vec3 p) { return v(p.x, p.y, p.z); }

    ElementId element(ElementKind kind, std::vector<VertexId> refs, bool closed,
                      Params params) {
        ElementId id = dfn::world::NO_ELEMENT;
        const auto r = g.add_element(kind, std::move(refs), "", id);
        if (!r.ok) {
            std::fprintf(stderr, "forge: add_element: %s\n", r.why.c_str());
            std::exit(2);
        }
        if (closed) {
            (void)g.set_closed(id, true);
        }
        for (const auto& kv : params) {
            (void)g.set_param(id, kv.first, kv.second);
        }
        return id;
    }

    ElementId contour(std::vector<VertexId> refs, Params params) {
        return element(ElementKind::Surface, std::move(refs), true, params);
    }

    ElementId beam(VertexId a, VertexId b, Params params) {
        return element(ElementKind::Line, {a, b}, false, params);
    }

    /// Низкая стенка-цепочка (только у бортика очага): лицо — по правилу
    /// домов, нормаль = cross(направление, вверх), так что обход бортика
    /// идёт (СВ->СЗ->ЮЗ->ЮВ), и камень смотрит наружу.
    ElementId wall(VertexId a, VertexId b, Params params) {
        return element(ElementKind::Surface, {a, b}, false, params);
    }

    // -- руки под мебель -----------------------------------------------------

    /// ПЛИТА ЛИЦОМ ВВЕРХ (столешница, полка, настил, под очага). y_top — та
    /// самая высота, которую называет человек; якоря садятся на y_top - th/2.
    ElementId slab(float x0, float z0, float x1, float z1, float y_top, float th,
                   const char* mat, const char* tone, Params extra = {}) {
        char tb[16];
        std::snprintf(tb, sizeof(tb), "%.3f", th);
        const float y = y_top - th * 0.5f;
        // Обход против часовой сверху — лицо вверх (как у полов домов).
        const ElementId id = contour({v(x0, y, z0), v(x0, y, z1), v(x1, y, z1),
                                      v(x1, y, z0)},
                                     {{"thickness", tb}, {"mat", mat},
                                      {"tone", tone}});
        for (const auto& kv : extra) {
            (void)g.set_param(id, kv.first, kv.second);
        }
        return id;
    }

    /// ДОСКА-ЩЕКА, стоящая вертикально: a и b — низ доски (та же y), y_top —
    /// её верхняя кромка. Опора лавки, стойка стеллажа, изголовье кровати.
    ElementId panel(glm::vec3 a, glm::vec3 b, float y_top, float th,
                    const char* mat, const char* tone, Params extra = {}) {
        char tb[16];
        std::snprintf(tb, sizeof(tb), "%.3f", th);
        const ElementId id = contour({v(a), v(b), v(b.x, y_top, b.z),
                                      v(a.x, y_top, a.z)},
                                     {{"thickness", tb}, {"mat", mat},
                                      {"tone", tone}});
        for (const auto& kv : extra) {
            (void)g.set_param(id, kv.first, kv.second);
        }
        return id;
    }

    /// БРУСОК: прямая с квадратным сечением (radius — от оси до ГРАНИ, то
    /// есть брус 0.09x0.09 — это radius 0.045). Ножка, царга, проножка,
    /// подкос — всё одна деталь, разница только в концах.
    ElementId bar(glm::vec3 a, glm::vec3 b, float r, const char* mat,
                  const char* tone, const char* form = "square") {
        char rb[16];
        std::snprintf(rb, sizeof(rb), "%.3f", r);
        return beam(v(a), v(b),
                    {{"radius", rb}, {"form", form}, {"mat", mat}, {"tone", tone}});
    }

    void save(const std::string& path) {
        const std::string text = dfn::world::write_house(g);
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) {
            std::fprintf(stderr, "forge: не открылся %s\n", path.c_str());
            std::exit(2);
        }
        f << text;
        std::fprintf(stderr, "forge: %s — вершин %zu, элементов %zu\n", path.c_str(),
                     g.vertex_count(), g.element_count());
    }
};

// ---------------------------------------------------------------------------
// 1. ОБЕДЕННЫЙ СТОЛ 1.8 x 0.9, столешница на 0.80 (каталог §9: 1.8x0.9x0.78,
//    столешница 0.07, ноги-бруски 0.09). Четыре ножки, царги под крышкой и
//    Н-проножка внизу: без неё четыре палки под плитой читаются «карточкой»
//    (та же находка, что у прилавка рынка, приёмка 21.08).
// ---------------------------------------------------------------------------
void forge_table() {
    Forge f;
    const float W = 1.8f;
    const float D = 0.9f;
    const float TOP = 0.80f;
    const float th_top = 0.07f;
    const float y_leg = TOP - th_top;      // ножка упирается в изнанку крышки
    const float in = 0.12f;                // отступ ножки от кромки
    const float y_rail = 0.62f;            // царга
    const float y_str = 0.16f;             // проножка

    f.slab(0.0f, 0.0f, W, D, TOP, th_top, "1", "1");

    const float xs[2] = {in, W - in};
    const float zs[2] = {in, D - in};
    for (const float x : xs) {
        for (const float z : zs) {
            f.bar({x, 0.0f, z}, {x, y_leg, z}, 0.045f, "0", "2");
        }
    }
    // Царги-подстолье по кругу: брус 0.06x0.06 под самой крышкой.
    f.bar({xs[0], y_rail, zs[0]}, {xs[1], y_rail, zs[0]}, 0.03f, "1", "2");
    f.bar({xs[0], y_rail, zs[1]}, {xs[1], y_rail, zs[1]}, 0.03f, "1", "2");
    f.bar({xs[0], y_rail, zs[0]}, {xs[0], y_rail, zs[1]}, 0.03f, "1", "2");
    f.bar({xs[1], y_rail, zs[0]}, {xs[1], y_rail, zs[1]}, 0.03f, "1", "2");
    // Н-проножка: две поперечины у торцов + продольная по центру.
    f.bar({xs[0], y_str, zs[0]}, {xs[0], y_str, zs[1]}, 0.035f, "0", "2");
    f.bar({xs[1], y_str, zs[0]}, {xs[1], y_str, zs[1]}, 0.035f, "0", "2");
    f.bar({xs[0], y_str, D * 0.5f}, {xs[1], y_str, D * 0.5f}, 0.035f, "0", "2");

    f.save("assets/houses/furn-table.dfh");
}

// ---------------------------------------------------------------------------
// 2. ЛАВКА 1.6 x 0.35, сиденье на 0.45 (каталог §9: скамья без спинки
//    1.6x0.35x0.45). Доска на двух опорах-щеках, между щеками — проножка.
// ---------------------------------------------------------------------------
void forge_bench() {
    Forge f;
    const float W = 1.6f;
    const float D = 0.35f;
    const float SEAT = 0.45f;
    const float th_seat = 0.055f;
    const float y_leg = SEAT - th_seat;

    f.slab(0.0f, 0.0f, W, D, SEAT, th_seat, "1", "1");

    // Опоры — ДОСКИ-ЩЁКИ поперёк лавки, а не палки: у скамьи из каталога
    // (copy 4, copy 2) нога — плаха во всю глубину сиденья.
    for (const float x : {0.22f, W - 0.22f}) {
        f.panel({x, 0.0f, 0.03f}, {x, 0.0f, D - 0.03f}, y_leg, 0.05f, "1", "2");
    }
    // Проножка на уровне 0.15 — она и держит щёки от расшатывания.
    f.bar({0.22f, 0.15f, D * 0.5f}, {W - 0.22f, 0.15f, D * 0.5f}, 0.035f, "0", "2");

    f.save("assets/houses/furn-bench.dfh");
}

// ---------------------------------------------------------------------------
// 3. КРОВАТЬ 1.0 (Х) x 2.0 (Z): рама на четырёх столбиках, дощатый настил,
//    изголовье-щит с навершиями (каталог §9: односпальная 1.9x0.85, рама
//    0.4, спинки со столбиками 0.9 и пирамидными навершиями).
// ---------------------------------------------------------------------------
void forge_bed() {
    Forge f;
    const float W = 1.0f;  // ширина, восток-запад
    const float L = 2.0f;  // длина, север-юг; изголовье на севере (z=0)
    // Отступ столбика от кромки — 0.09, а не 0.06: навершие-шайба 0.18 иначе
    // свешивается за габарит, и предмет, поставленный вплотную к стене,
    // въезжает в неё углом.
    const float in = 0.09f;
    const float y_rail = 0.34f;  // царга рамы
    const float DECK = 0.40f;    // верх настила
    const float y_head = 0.95f;  // верх изголовья
    const float y_foot = 0.55f;  // верх ножного столбика

    const float xs[2] = {in, W - in};
    // Столбики: изголовные выше, ножные ниже — так кровать «смотрит» в комнату.
    for (const float x : xs) {
        f.bar({x, 0.0f, in}, {x, y_head, in}, 0.06f, "0", "2");
        f.bar({x, 0.0f, L - in}, {x, y_foot, L - in}, 0.06f, "0", "2");
    }
    // Рама: четыре царги по периметру на 0.34.
    f.bar({xs[0], y_rail, in}, {xs[1], y_rail, in}, 0.05f, "0", "2");
    f.bar({xs[0], y_rail, L - in}, {xs[1], y_rail, L - in}, 0.05f, "0", "2");
    f.bar({xs[0], y_rail, in}, {xs[0], y_rail, L - in}, 0.05f, "0", "2");
    f.bar({xs[1], y_rail, in}, {xs[1], y_rail, L - in}, 0.05f, "0", "2");
    // НАСТИЛ-ДОСКИ: плита с раскладкой досок (fill=5), лежит на царгах.
    f.slab(in, in, W - in, L - in, DECK, 0.06f, "1", "1",
           {{"fill", "5"}, {"wear", "0.3"}});
    // Изголовье — щит между высокими столбиками; изножье — низкая доска.
    f.panel({in, y_rail + 0.14f, in}, {W - in, y_rail + 0.14f, in}, y_head - 0.06f,
            0.05f, "1", "2");
    f.panel({in, y_rail + 0.14f, L - in}, {W - in, y_rail + 0.14f, L - in},
            y_foot - 0.06f, 0.05f, "1", "2");
    // Навершия столбиков изголовья — плоские шайбы 0.18 (каталог §7).
    for (const float x : xs) {
        f.slab(x - 0.09f, in - 0.09f, x + 0.09f, in + 0.09f, y_head + 0.04f, 0.05f,
               "0", "2");
    }

    f.save("assets/houses/furn-bed.dfh");
}

// ---------------------------------------------------------------------------
// 4. СТЕЛЛАЖ У СТЕНЫ 1.2 x 0.35, высота 1.8: две стойки-щеки и три полки
//    (каталог §9: открытый стеллаж 1.0x0.35x1.5, шаг полок ~0.45).
//    Ставится задней кромкой (z = D) к стене.
// ---------------------------------------------------------------------------
void forge_shelf() {
    Forge f;
    const float W = 1.2f;
    const float D = 0.35f;
    const float H = 1.8f;
    const float sx0 = 0.05f;      // ось левой стойки
    const float sx1 = W - 0.05f;  // ось правой

    for (const float x : {sx0, sx1}) {
        f.panel({x, 0.0f, 0.0f}, {x, 0.0f, D}, H, 0.06f, "1", "2");
    }
    // Три полки: 0.55 / 1.10 / 1.65 — шаг 0.55, под кувшин и котелок влезает.
    for (const float y : {0.55f, 1.10f, 1.65f}) {
        f.slab(sx0, 0.02f, sx1, D - 0.02f, y, 0.04f, "1", "1");
    }
    // Обвязка сзади: верхняя и нижняя рейки — без них стеллаж складывается
    // ромбом, и это видно даже на статичной картинке (плоские щёки).
    f.bar({sx0, H - 0.05f, D - 0.04f}, {sx1, H - 0.05f, D - 0.04f}, 0.03f, "0", "2");
    f.bar({sx0, 0.12f, D - 0.04f}, {sx1, 0.12f, D - 0.04f}, 0.03f, "0", "2");

    f.save("assets/houses/furn-shelf.dfh");
}

// ---------------------------------------------------------------------------
// 5. НЕСУЩАЯ КОЛОННА 0.3 x 0.3 высотой 2.6 с ЧЕТЫРЬМЯ УГОЛКАМИ-ПОДКОСАМИ
//    (каталог §4, ключевая деталь длинного дома: подкос под 45°, катет
//    0.5-0.8, брус 0.12x0.18; база — каменный башмак 0.6x0.6x0.15;
//    капитель — накладка-подушка 0.5x0.5x0.10 прямо под балкой).
//    Ось стоит в (0.85, 0.85): подкосы уходят на 0.7 в каждую сторону, и всё
//    пятно предмета укладывается в 1.7 x 1.7 без отрицательных координат.
// ---------------------------------------------------------------------------
void forge_column() {
    Forge f;
    const float cx = 0.85f;
    const float cz = 0.85f;
    const float H = 2.6f;
    const float arm = 0.70f;      // катет подкоса
    const float y_knee = 1.80f;   // где подкос врезан в столб
    const float y_top = 2.50f;    // где он подпирает балку

    // Каменный башмак под столбом.
    f.slab(cx - 0.3f, cz - 0.3f, cx + 0.3f, cz + 0.3f, 0.15f, 0.15f, "3", "1",
           {{"wear", "0.35"}});
    // Столб 0.3x0.3: radius — от оси до грани, значит 0.15.
    f.bar({cx, 0.0f, cz}, {cx, H, cz}, 0.15f, "0", "2");
    // Капитель-подушка под балкой.
    f.slab(cx - 0.25f, cz - 0.25f, cx + 0.25f, cz + 0.25f, H, 0.10f, "0", "2");
    // ЧЕТЫРЕ ПОДКОСА под 45°: от тела столба наружу и вверх. Нижний конец
    // сидит НА ОСИ (врубка в столб), верхний — на 0.7 в сторону, у самой
    // капители; ровно та стрельчатая арка между столбами, которую даёт
    // симметричная пара на соседних столбах.
    const glm::vec3 dirs[4] = {{-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                               {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}};
    for (const glm::vec3& d : dirs) {
        f.bar({cx, y_knee, cz},
              {cx + d.x * arm, y_top, cz + d.z * arm}, 0.075f, "0", "2");
    }
    // Кованые пояски-хомуты (каталог §4): две тонкие шайбы на 1.2 и 2.2.
    for (const float y : {1.20f, 2.20f}) {
        f.slab(cx - 0.17f, cz - 0.17f, cx + 0.17f, cz + 0.17f, y, 0.06f, "0", "1");
    }

    f.save("assets/houses/furn-column.dfh");
}

// ---------------------------------------------------------------------------
// 6. КАМЕННЫЙ ОЧАГ 1.4 x 1.4 (каталог §10: приподнятый короб очага, бортик
//    обложен камнем, внутри — утопленный под с золой). Низкий короб:
//    плита-основание, бортик из блоков по периметру, под утоплен на 0.18
//    ниже кромки — в яму встаёт котёл на треноге.
// ---------------------------------------------------------------------------
void forge_hearth() {
    Forge f;
    const float S = 1.4f;
    const float base_top = 0.14f;
    const float rim_h = 0.24f;   // бортик: верх на 0.38
    const float rim_th = 0.20f;

    f.slab(0.0f, 0.0f, S, S, base_top, 0.14f, "3", "1", {{"wear", "0.45"}});

    // БОРТИК: цепочки по осевой линии кладки (отступ в полтолщины), обход
    // СВ -> СЗ -> ЮЗ -> ЮВ -> СВ — при нём лицо камня смотрит НАРУЖУ.
    const float a = rim_th * 0.5f;
    const float b = S - a;
    const Params stone = {{"height", "0.24"}, {"thickness", "0.20"}, {"mat", "3"},
                          {"tone", "1"}, {"fill", "3"}, {"wear", "0.55"}};
    (void)f.wall(f.v(b, base_top, a), f.v(a, base_top, a), stone);  // север
    (void)f.wall(f.v(a, base_top, a), f.v(a, base_top, b), stone);  // запад
    (void)f.wall(f.v(a, base_top, b), f.v(b, base_top, b), stone);  // юг
    (void)f.wall(f.v(b, base_top, b), f.v(b, base_top, a), stone);  // восток
    (void)rim_h;

    // ПОД: утопленная плита золы внутри бортика — верх на 0.20, то есть на
    // 0.18 ниже кромки. Тон темнее и износ выкручен: у очага камень чёрный.
    f.slab(rim_th, rim_th, S - rim_th, S - rim_th, 0.20f, 0.06f, "3", "2",
           {{"wear", "0.85"}});

    f.save("assets/houses/furn-hearth.dfh");
}

} // namespace

int main() {
    forge_table();
    forge_bench();
    forge_bed();
    forge_shelf();
    forge_column();
    forge_hearth();
    return 0;
}
