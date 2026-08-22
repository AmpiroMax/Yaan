/*
Created: 21:08:2026 - 13:20:00
Last updated: 22:08:2026 - 21:30:00
Module: tools
File: tools/forge_furniture.cpp

Responsibility:
- КУЗНИЦА ОФОРМЛЕНИЯ ДОМА И ДВОРА (заказ 21.08: «объекты внутреннего
  оформления домов», затем «дворовый набор»). Каждый предмет — ОТДЕЛЬНАЯ
  ГОТОВАЯ ПОСТРОЙКА в assets/houses/furn-*.dfh: внутри дома — стол, лавка,
  кровать, стеллаж, несущая колонна с уголками-подкосами, каменный очаг; на
  заднем дворе — секция забора, калитка, поленница, бочка, грядка. Собираются тем же HouseGraph API и
  пишутся тем же каноническим write_house, что и дома, — расстановка внутри
  комнаты ничем не отличается от расстановки дома на улице.

Key items:
- Forge: рука над графом (дедуп вершин, slab/panel/bar/rim).
- table/bench/bed/shelf/column/hearth: шесть предметов интерьера.
- fence2/fence_gate/woodpile/barrel/bed_garden: пять предметов двора.
- walk2: сегмент каменной дорожки, из них набирается путь от двери.

Dependencies:
- Uses: engine/world (HouseGraph, HouseFile).
- Used by: цель dfn_furniture; артефакты читают сцена и редактор.

Notes:
- ЧЕЛОВЕЧЕСКИЕ ЧИСЛА (docs/INTERIOR_CATALOG.md §9, обмеры по росту 1.8 м):
  стол 1.8x0.9x0.78, лавка 1.6x0.35x0.45, кровать 1.9x0.85, стеллаж 3 полки,
  столб длинного дома 0.45 с подкосами под 45°, очаг-короб 1.4x1.4x0.35.
  Двор — docs/CITY_DESIGN_GUIDE.md §3 и §7: забор парцеллы 1.2-1.8 м
  (здесь 1.4), порядок вглубь двора «бочка/поленница -> грядки -> забор»,
  клаттер вдоль стен и по углам.
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
- 21:08:2026 - 19:10:00: ДВОРОВЫЙ НАБОР (CITY_DESIGN_GUIDE.md §7): секция
  забора 2.0x1.4 с перевивом жердей, калитка 1.0 со щелью и раскосом,
  поленница из 10 брёвнышек с детерминированным джиттером, бочка из восьми
  клёпок двумя поясами через пузо, грядка 1.0x2.4 с гребнями. Руки Forge:
  bar() принял свои параметры (износ жердей), добавлены quad() (клёпка с
  завалом) и disc8() (дно и крышка).
- 22:08:2026 - 14:30:00: СЕГМЕНТ ДОРОЖКИ furn-walk2 (город по утверждённой
  схеме docs/WHITERUN_PLAN.json: генератор собирает дорожки от дверей из
  готовых кусков, как забор — из секций). Полотно 2.0x1.2, толщина 0.1, верх
  на 0.08 — плита УТОПЛЕНА в землю (низ -0.02): положенная поверх, она на
  уклоне показывает торец. Два бордюра 0.12x0.14 по длинным кромкам, верх на
  0.12 — выступ 0.04 над полотном: без кромки лента камня читается пятном
  текстуры, а не мощением, и 0.04 хватает на тень, не цепляя шаг. Бордюр —
  panel(), а не bar(): у bar() сечение квадратное по построению (radius — одно
  число), а тут 0.12 x 0.14. Прежние 11 предметов байт-в-байт.
- 22:08:2026 - 21:30:00: СТОЛБ ПОД БАЛКУ — ДЛИНА ПАРАМЕТРОМ (заказ раскладчика
  22.08, пакет D владельца: «опоры не упираются в потолок, который должны
  держать»). Столб ростом 2.60 не доставал до балки ни в одном интерьере, где
  стоит. Три новых длины по ЕГО замеру низа балки, считая от верха половой
  плиты: furn-column-h341 (city-longhall, 3.410), furn-column-h281
  (city-keep-s, 2.805), furn-column-h272 (city-house-l и -old, 2.720).
  ЭТО ПАРАМЕТР, А НЕ ВТОРОЙ РЕЦЕПТ — тот же довод, что у Aging у домов.
  Отметки подкосов и хомутов переписаны ОТ ВЕРХА (H-0.10, H-0.80, H-0.40,
  H-1.40); при H = 2.6 это ровно прежние 2.50 / 1.80 / 2.20 / 1.20, поэтому
  furn-column перепёкся бит-в-бит вместе с остальными одиннадцатью.
  ЗАМЕР ГОТОВОГО МЕША, А НЕ ПАСПОРТ (порог владельца «верх столба = низ балки
  ±0.02»): верх — 2.600 / 3.410 / 2.805 / 2.720, ноль расхождения с паспортом;
  пятно у всех четырёх 1.506 x 1.506, а НЕ 1.4 x 1.4 — габарит задают ПОДКОСЫ
  (ось 0.85 плюс катет 0.70 плюс радиус бруса), и зазор надо считать по 1.506.
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
                  const char* tone, const char* form = "square",
                  Params extra = {}) {
        char rb[16];
        std::snprintf(rb, sizeof(rb), "%.3f", r);
        const ElementId id = beam(v(a), v(b),
                                  {{"radius", rb}, {"form", form}, {"mat", mat},
                                   {"tone", tone}});
        for (const auto& kv : extra) {
            (void)g.set_param(id, kv.first, kv.second);
        }
        return id;
    }

    /// ЧЕТЫРЁХУГОЛЬНИК ПРОИЗВОЛЬНОЙ ПОСАДКИ: panel() умеет только строго
    /// вертикальную доску (низ и верх над одной точкой), а клёпке бочки надо
    /// заваливаться внутрь — у неё низ и верх на РАЗНЫХ радиусах.
    ElementId quad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, float th,
                   const char* mat, const char* tone, Params extra = {}) {
        char tb[16];
        std::snprintf(tb, sizeof(tb), "%.3f", th);
        const ElementId id = contour({v(a), v(b), v(c), v(d)},
                                     {{"thickness", tb}, {"mat", mat},
                                      {"tone", tone}});
        for (const auto& kv : extra) {
            (void)g.set_param(id, kv.first, kv.second);
        }
        return id;
    }

    /// ГОРИЗОНТАЛЬНЫЙ ВОСЬМИУГОЛЬНИК ЛИЦОМ ВВЕРХ (дно и крышка бочки).
    /// Обход по УБЫВАНИЮ угла: по возрастанию нормаль контура смотрит вниз.
    ElementId disc8(float cx, float cz, float r, float y_top, float th,
                    const char* mat, const char* tone, Params extra = {}) {
        char tb[16];
        std::snprintf(tb, sizeof(tb), "%.3f", th);
        const float y = y_top - th * 0.5f;
        std::vector<VertexId> ring;
        ring.reserve(8);
        for (int k = 7; k >= 0; --k) {
            const float t = 0.7853981634f * static_cast<float>(k);
            ring.push_back(v(cx + r * std::cos(t), y, cz + r * std::sin(t)));
        }
        const ElementId id = contour(std::move(ring),
                                     {{"thickness", tb}, {"mat", mat},
                                      {"tone", tone}});
        for (const auto& kv : extra) {
            (void)g.set_param(id, kv.first, kv.second);
        }
        return id;
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
/// ДЛИНА СТВОЛА — ПАРАМЕТР, А НЕ ВТОРОЙ РЕЦЕПТ (тот же довод, что у Aging у
/// домов). Заказ раскладчика 22.08: столб ростом 2.60 не достаёт до балки НИ В
/// ОДНОМ интерьере, где стоит, и «столб, не достающий до балки, изображает
/// опору, а не работает ею». Отметки подкосов и хомутов отсчитываются ОТ ВЕРХА
/// (H - 0.10, H - 0.80, H - 0.40, H - 1.40): при H = 2.6 это ровно прежние
/// 2.50 / 1.80 / 2.20 / 1.20, поэтому furn-column перепекается бит-в-бит.
void forge_column_h(float H, const char* file) {
    Forge f;
    const float cx = 0.85f;
    const float cz = 0.85f;
    const float arm = 0.70f;         // катет подкоса
    const float y_top = H - 0.10f;   // где он подпирает балку
    const float y_knee = y_top - 0.70f;  // где подкос врезан в столб

    // Каменный башмак под столбом.
    f.slab(cx - 0.3f, cz - 0.3f, cx + 0.3f, cz + 0.3f, 0.15f, 0.15f, "3", "1",
           {{"wear", "0.35"}});
    // Столб 0.3x0.3: radius — от оси до грани, значит 0.15.
    f.bar({cx, 0.0f, cz}, {cx, H, cz}, 0.15f, "0", "2");
    // Капитель-подушка под балкой. ВЕРХ КАПИТЕЛИ = H: это и есть та отметка,
    // которую раскладчик сажает на низ балки.
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
    // Кованые пояски-хомуты (каталог §4): две тонкие шайбы под капителью и
    // ниже. При H = 2.6 это ровно прежние 1.20 и 2.20.
    for (const float y : {H - 1.40f, H - 0.40f}) {
        f.slab(cx - 0.17f, cz - 0.17f, cx + 0.17f, cz + 0.17f, y, 0.06f, "0", "1");
    }

    f.save(file);
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


// ===========================================================================
// ДВОРОВЫЙ НАБОР (заказ 21.08 по docs/CITY_DESIGN_GUIDE.md §3 и §7: порядок
// вглубь парцеллы — дом, дворик с бочкой и поленницей, огород-грядки, забор
// 1.2-1.8 м плетнём или частоколом; клаттер ставится ВДОЛЬ стен и по углам).
// Каждый предмет — своя постройка: забор набирается секциями, как стена.
// ===========================================================================

// ---------------------------------------------------------------------------
// 7. СЕКЦИЯ ЗАБОРА 2.0 м, высота 1.4 (§3: забор парцеллы 1.2-1.8). Три кола и
//    четыре жерди, жерди ПО ОЧЕРЕДИ спереди и сзади кольев — этот перевив и
//    отличает плетень от четырёх параллельных палок.
// ---------------------------------------------------------------------------
void forge_fence2() {
    Forge f;
    const float L = 2.0f;
    const float H = 1.4f;
    const float cz = 0.07f;  // ось забора; секция целиком лежит в 0..0.14

    for (const float x : {0.07f, L * 0.5f, L - 0.07f}) {
        f.bar({x, 0.0f, cz}, {x, H, cz}, 0.07f, "0", "2", "round",
              {{"wear", "0.5"}});
    }
    // Жерди: 0.30 / 0.65 / 1.00 / 1.30, перевив ±0.04 от оси кольев.
    int k = 0;
    for (const float y : {0.30f, 0.65f, 1.00f, 1.30f}) {
        const float z = cz + ((k++ % 2 == 0) ? 0.04f : -0.04f);
        f.bar({0.02f, y, z}, {L - 0.02f, y, z}, 0.045f, "0", "2", "round",
              {{"wear", "0.5"}});
    }

    f.save("assets/houses/furn-fence2.dfh");
}

// ---------------------------------------------------------------------------
// 8. КАЛИТКА 1.0 м: два кола повыше (1.6) и дощатое полотно с двумя
//    перекладинами и раскосом (Z-образная обвязка — без раскоса дощатая
//    калитка провисает углом, и это видно даже на неподвижном кадре).
//    Полотно закрытое, но со ЩЕЛЬЮ 0.13 у восточного кола: створка висит на
//    петлях, а не врезана в столб.
// ---------------------------------------------------------------------------
void forge_fence_gate() {
    Forge f;
    const float W = 1.0f;
    const float H = 1.6f;
    const float cz = 0.07f;
    const float x0 = 0.14f;   // край полотна у петель
    const float x1 = 0.81f;   // край полотна; дальше щель до кола 0.93
    const float y0 = 0.12f;   // просвет под калиткой
    const float y1 = 1.25f;

    for (const float x : {0.07f, W - 0.07f}) {
        f.bar({x, 0.0f, cz}, {x, H, cz}, 0.075f, "0", "2", "round",
              {{"wear", "0.5"}});
    }
    // Полотно — доски (fill=5 кладёт раскладку досок по контуру).
    f.panel({x0, y0, cz}, {x1, y0, cz}, y1, 0.05f, "1", "2",
            {{"fill", "5"}, {"wear", "0.45"}});
    // Обвязка и раскос — снаружи полотна, на 0.05 от его плоскости.
    const float zf = cz - 0.05f;
    f.bar({x0, y0 + 0.15f, zf}, {x1, y0 + 0.15f, zf}, 0.035f, "0", "2", "square",
          {{"wear", "0.45"}});
    f.bar({x0, y1 - 0.12f, zf}, {x1, y1 - 0.12f, zf}, 0.035f, "0", "2", "square",
          {{"wear", "0.45"}});
    f.bar({x0, y0 + 0.15f, zf}, {x1, y1 - 0.12f, zf}, 0.035f, "0", "2", "square",
          {{"wear", "0.45"}});

    f.save("assets/houses/furn-fence-gate.dfh");
}

// ---------------------------------------------------------------------------
// 9. ПОЛЕННИЦА (§7: жилой переулок — поленница вдоль стены): два кола по
//    торцам и десять брёвнышек в пять рядов. Радиус и посадка каждого полена
//    гуляют по номеру — ряд одинаковых цилиндров читается трубами, а не
//    дровами; джиттер ДЕТЕРМИНИРОВАННЫЙ (номер, не случай), иначе две сборки
//    дадут два разных файла.
// ---------------------------------------------------------------------------
void forge_woodpile() {
    Forge f;
    const float xa = 0.08f;         // торцы поленьев
    const float xb = 1.48f;         // длина полена ровно 1.4
    const float z_front = 0.20f;
    const float z_back = 0.44f;

    for (const float x : {0.03f, 1.53f}) {
        f.bar({x, 0.0f, 0.32f}, {x, 1.20f, 0.32f}, 0.05f, "0", "2", "round",
              {{"wear", "0.5"}});
    }
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 2; ++col) {
            const int k = row * 2 + col;
            const float r = 0.095f + 0.008f * static_cast<float>(k % 4);
            const float dz = (k % 3 == 0) ? 0.02f : ((k % 3 == 1) ? -0.025f : 0.01f);
            const float dy = (k % 2 == 0) ? 0.012f : -0.008f;
            const float y = 0.13f + 0.22f * static_cast<float>(row) + dy;
            const float z = (col == 0 ? z_front : z_back) + dz;
            f.bar({xa, y, z}, {xb, y, z}, r, "0", "2", "round", {{"wear", "0.4"}});
        }
    }

    f.save("assets/houses/furn-woodpile.dfh");
}

// ---------------------------------------------------------------------------
// 10. БОЧКА дождевой воды (§7): восемь клёпок по кругу, дно и крышка —
//     восьмиугольные диски. Клёпка идёт ДВУМЯ поясами через пузо на 0.42
//     (низ R 0.28 -> пузо R 0.33 -> верх R 0.28): одним поясом с завалом
//     внутрь получается ведро, а бочку от ведра отличает именно пузо.
//     Обручей нет — тонкое кольцо прямой не собрать (заказ дословно).
// ---------------------------------------------------------------------------
void forge_barrel() {
    Forge f;
    const float cx = 0.35f;
    const float cz = 0.35f;
    const float r_lo = 0.28f;
    const float r_mid = 0.33f;
    const float r_hi = 0.28f;
    const float y_mid = 0.42f;
    const float H = 0.80f;
    const auto at = [&](float r, float y, int k) {
        const float t = 0.7853981634f * static_cast<float>(k);
        return glm::vec3{cx + r * std::cos(t), y, cz + r * std::sin(t)};
    };
    // Обход по УБЫВАНИЮ угла — лицо клёпки наружу.
    for (int k = 8; k > 0; --k) {
        const int k0 = k;
        const int k1 = k - 1;
        f.quad(at(r_lo, 0.0f, k0), at(r_lo, 0.0f, k1), at(r_mid, y_mid, k1),
               at(r_mid, y_mid, k0), 0.04f, "1", "2", {{"wear", "0.45"}});
        f.quad(at(r_mid, y_mid, k0), at(r_mid, y_mid, k1), at(r_hi, H, k1),
               at(r_hi, H, k0), 0.04f, "1", "2", {{"wear", "0.45"}});
    }
    f.disc8(cx, cz, r_lo - 0.02f, 0.07f, 0.05f, "1", "1", {{"wear", "0.5"}});
    f.disc8(cx, cz, r_hi + 0.02f, H + 0.04f, 0.05f, "1", "1", {{"wear", "0.5"}});

    f.save("assets/houses/furn-barrel.dfh");
}

// ---------------------------------------------------------------------------
// 11. ГРЯДКА 1.0 x 2.4 (§3: огород-грядки за двориком). Низкий короб из
//     досок 0.25, земля утоплена на 0.05 ниже борта, поверх — четыре
//     гребня-рядка: пустая коричневая плита читается лужей, а рядки сразу
//     говорят «здесь копали».
// ---------------------------------------------------------------------------
void forge_bed_garden() {
    Forge f;
    const float W = 1.0f;
    const float L = 2.4f;
    const float H = 0.25f;
    const float th = 0.06f;
    const float b = th * 0.5f;

    // Борта-доски: две вдоль (север-юг) и две поперёк.
    f.panel({b, 0.0f, 0.0f}, {b, 0.0f, L}, H, th, "1", "2", {{"wear", "0.5"}});
    f.panel({W - b, 0.0f, 0.0f}, {W - b, 0.0f, L}, H, th, "1", "2",
            {{"wear", "0.5"}});
    f.panel({0.0f, 0.0f, b}, {W, 0.0f, b}, H, th, "1", "2", {{"wear", "0.5"}});
    f.panel({0.0f, 0.0f, L - b}, {W, 0.0f, L - b}, H, th, "1", "2",
            {{"wear", "0.5"}});
    // Земля — на 0.05 ниже кромки борта.
    f.slab(th, th, W - th, L - th, H - 0.05f, 0.16f, "3", "3", {{"wear", "0.9"}});
    // Четыре гребня поперёк грядки.
    for (int k = 0; k < 4; ++k) {
        const float zc = 0.36f + 0.56f * static_cast<float>(k);
        f.slab(0.14f, zc - 0.11f, W - 0.14f, zc + 0.11f, H + 0.02f, 0.10f, "3", "3",
               {{"wear", "0.9"}});
    }

    f.save("assets/houses/furn-bed-garden.dfh");
}

// ---------------------------------------------------------------------------
// 12. СЕГМЕНТ КАМЕННОЙ ДОРОЖКИ 2.0 (X) x 1.2 (Z): плита-полотно 0.1 и два
//     бордюра-бруска 0.12 x 0.14 по ДЛИННЫМ кромкам. Из таких сегментов
//     генератор набирает дорожки от дверей — ровно так же, как забор
//     набирается секциями (§7: дорожка от калитки к двери).
//     ПОЧЕМУ БОРДЮР ВЫСТУПАЕТ НАД ПОЛОТНОМ: без кромки лента камня на земле
//     читается пятном текстуры, а не мощением; 0.04 хватает на тень, и это
//     ниже порога, за который цепляется шаг.
//     ПОЛОТНО УТОПЛЕНО (верх 0.08 при толщине 0.1, низ -0.02): дорожка лежит
//     В земле, а не поверх неё, и на уклоне не показывает торец плиты.
//     Бордюр — panel(), а не bar(): у бруска сечение 0.12 x 0.14, а brus у
//     bar() квадратный по построению (radius — одно число).
// ---------------------------------------------------------------------------
void forge_walk2() {
    Forge f;
    const float L = 2.0f;        // длина сегмента, восток-запад
    const float D = 1.2f;        // ширина дорожки, север-юг
    const float TOP = 0.08f;     // верх полотна
    const float th = 0.10f;      // толщина полотна
    const float kb = 0.12f;      // бордюр: сечение поперёк
    const float kh = 0.14f;      // бордюр: сечение по высоте
    const float y0 = TOP - th;   // низ полотна: бордюр садится на ту же отметку

    // Полотно: камень с лёгким износом — по нему ходят, но его и метут.
    f.slab(0.0f, 0.0f, L, D, TOP, th, "3", "1", {{"wear", "0.3"}});
    // Бордюры по длинным кромкам: оси на полтолщины внутрь, чтобы брусок
    // уложился в габарит 2.0 x 1.2 без свеса.
    for (const float cz : {kb * 0.5f, D - kb * 0.5f}) {
        f.panel({0.0f, y0, cz}, {L, y0, cz}, y0 + kh, kb, "3", "1",
                {{"fill", "3"}, {"wear", "0.35"}});
    }

    f.save("assets/houses/furn-walk2.dfh");
}

} // namespace

int main() {
    forge_table();
    forge_bench();
    forge_bed();
    forge_shelf();
    forge_column_h(2.6f, "assets/houses/furn-column.dfh");
    // ДЛИНЫ ПОД БАЛКИ ПРИНЯТЫХ ИНТЕРЬЕРОВ (заказ раскладчика 22.08, замер низа
    // балки снят у него с .dfh, длина считается от ВЕРХА половой плиты):
    // city-longhall 3.410, city-keep-s 2.805, city-house-l 2.720.
    forge_column_h(3.410f, "assets/houses/furn-column-h341.dfh");
    forge_column_h(2.805f, "assets/houses/furn-column-h281.dfh");
    forge_column_h(2.720f, "assets/houses/furn-column-h272.dfh");
    forge_hearth();
    forge_fence2();
    forge_fence_gate();
    forge_woodpile();
    forge_barrel();
    forge_bed_garden();
    forge_walk2();
    return 0;
}
