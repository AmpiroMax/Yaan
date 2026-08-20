/*
Created: 20:08:2026 - 13:40:00
Last updated: 20:08:2026 - 19:05:00
Module: tools
File: tools/forge_houses.cpp

Responsibility:
- КУЗНИЦА ГОТОВЫХ ПОСТРОЕК (заказ 20.08: «повторить домики с демки новой
  механикой + Г-образный + П-образный, зарегистрировать как готовые
  постройки»). Пять домов собираются ЧЕРЕЗ HouseGraph API и пишутся
  каноническим write_house в assets/houses/*.dfh — тем же текстом, который
  читает редактор, загрузка карты и тест проходимости.

Key items:
- Forge: рука над графом (вершины, цепочки, контуры, балки).
- log/frame/stone replica + l_house + u_house: рецепты домов.

Dependencies:
- Uses: engine/world (HouseGraph, HouseFile, HouseMesh — константы двери).
- Used by: цель dfn_houses; артефакты читают сцена build.scene и тесты.

Notes:
- ЛОКАЛЬНЫЕ КООРДИНАТЫ: начало — северо-западный угол, дверь смотрит на +Z
  (юг), как у домов из деталей (gen_house_demo.py). Вершины все Free с явной
  высотой: постройка ставится на ровную полку, и высота из рельефа сделала бы
  файл недетерминированным.
- Числа этажа/двери — из HOUSES.md через HouseStyle.h (STOREY у демки 3.25;
  здесь этаж 2.8 — кратен подъёму ступени 0.175 ровно в 16 ступеней).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Правки рецептов — только здесь,
  артефакты .dfh перегенерировать, не править руками.
*/
/*
UPD:
- 20:08:2026 - 13:40:00: Создана: три повтора демки, Г-образный, П-образный с двумя маршами.
- 20:08:2026 - 17:30:00: Конёк-бревно и стропила у двускатов; кровля рядами; детали и износ на всех пяти домах; лица стен наружу (крыльцо строилось в комнате).
- 20:08:2026 - 18:40:00: Сруб венцами; южные фасады с окнами по бокам двери; скаты Г-дома сели на стены + клинья-фронтоны; конёк и стропильные торцы П-дому; износ камня 0.7.
- 20:08:2026 - 19:05:00: Стропильные торцы — над карнизом с выпуском (не «верёвки» по фасаду); у П-дома — из-под южных свесов, а не в небо.
*/

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseGraph.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace {

using dfn::world::Anchoring;
using dfn::world::ElementId;
using dfn::world::ElementKind;
using dfn::world::HouseGraph;
using dfn::world::VertexId;

using Params = std::initializer_list<std::pair<const char*, const char*>>;

/// Рука над графом: рецепты читаются как список деталей, а не как API-вязь.
struct Forge {
    HouseGraph g;

    VertexId v(float x, float y, float z) {
        return g.add_vertex(Anchoring::Free, {x, y, z});
    }

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

    /// Стена-цепочка между двумя углами (низ), выдавленная вверх.
    ElementId wall(VertexId a, VertexId b, Params params) {
        return element(ElementKind::Surface, {a, b}, false, params);
    }

    ElementId contour(std::vector<VertexId> refs, Params params) {
        return element(ElementKind::Surface, std::move(refs), true, params);
    }

    ElementId beam(VertexId a, VertexId b, Params params) {
        return element(ElementKind::Line, {a, b}, false, params);
    }

    /// ДВЕРЬ-СТВОРКА в проёме южной стены: цепочка шириной проёма со свойством
    /// door=1 — рендер качает её вокруг петли, коллайдер её не берёт.
    void door_leaf(float cx, float y0, float z, Params extra = {}) {
        const float w = 1.0f;  // HOUSE_DOOR_W_DEFAULT
        const VertexId a = v(cx - w * 0.5f, y0, z);
        const VertexId b = v(cx + w * 0.5f, y0, z);
        const ElementId leaf = wall(a, b,
                                    {{"height", "2.05"},
                                     {"thickness", "0.05"},
                                     {"mat", "1"},
                                     {"tone", "2"},
                                     {"door", "1"},
                                     {"hinge", "0"}});
        for (const auto& kv : extra) {
            (void)g.set_param(leaf, kv.first, kv.second);
        }
    }

    /// Створка в проёме стены, идущей вдоль Z (для дверей во двор и боковых).
    void door_leaf_x(float x, float y0, float cz) {
        const float w = 1.0f;
        const VertexId a = v(x, y0, cz - w * 0.5f);
        const VertexId b = v(x, y0, cz + w * 0.5f);
        (void)wall(a, b,
                   {{"height", "2.05"},
                    {"thickness", "0.05"},
                    {"mat", "1"},
                    {"tone", "2"},
                    {"door", "1"},
                    {"hinge", "0"}});
    }

    /// Четыре угловых столба и обвязка поверху — каркас, к которому дом
    /// читается «собранным», как у демки из деталей.
    void frame_posts(float x0, float z0, float x1, float z1, float y0, float y1,
                     const char* mat) {
        const float xs[2] = {x0, x1};
        const float zs[2] = {z0, z1};
        VertexId top[4];
        int k = 0;
        for (const float* px : {&xs[0], &xs[1]}) {
            for (const float* pz : {&zs[0], &zs[1]}) {
                const VertexId lo = v(*px, y0, *pz);
                const VertexId hi = v(*px, y1, *pz);
                (void)beam(lo, hi, {{"radius", "0.12"}, {"mat", mat}, {"tone", "2"}});
                top[k++] = hi;
            }
        }
        // Обвязка: 0-1 (вдоль z), 1-3 (вдоль x), 3-2, 2-0.
        const int ring[4][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}};
        for (const auto& e : ring) {
            (void)beam(top[e[0]], top[e[1]],
                       {{"radius", "0.10"}, {"form", "square"}, {"mat", mat},
                        {"tone", "2"}});
        }
    }

    /// Двускат над прямоугольником: конёк вдоль X посередине глубины.
    /// Два наклонных контура + фронтоны + КОНЁК-БРЕВНО и торчащие КОНЦЫ
    /// СТРОПИЛ (правка 20.08 «к скайримскому виду»: крыша собрана, а не
    /// накрыта). roof_fill: "" — гладкий настил (солома), "7" — дранка
    /// рядами, "8" — черепица рядами; wear наследуется скатами.
    void gable_roof(float x0, float z0, float x1, float z1, float eaves,
                    float ridge_h, const char* mat, const char* tone,
                    const char* roof_fill = "", const char* wear = "") {
        const float zm = (z0 + z1) * 0.5f;
        const float o = 0.35f; // свес
        const float yr = eaves + ridge_h;
        const auto slab = [&](VertexId a, VertexId b, VertexId c, VertexId d) {
            const ElementId id = contour(
                {a, b, c, d},
                {{"thickness", "0.15"}, {"mat", mat}, {"tone", tone}});
            if (roof_fill[0] != 0) {
                (void)g.set_param(id, "fill", roof_fill);
            }
            if (wear[0] != 0) {
                (void)g.set_param(id, "wear", wear);
            }
        };
        // Северный скат (от конька к z0), лицо вверх; южный — зеркально.
        slab(v(x0 - o, yr, zm), v(x1 + o, yr, zm),
             v(x1 + o, eaves - 0.1f, z0 - o), v(x0 - o, eaves - 0.1f, z0 - o));
        slab(v(x1 + o, yr, zm), v(x0 - o, yr, zm),
             v(x0 - o, eaves - 0.1f, z1 + o), v(x1 + o, eaves - 0.1f, z1 + o));
        // Фронтоны — треугольники в плоскостях x0 и x1.
        for (const float gx : {x0, x1}) {
            const VertexId a = v(gx, eaves, z0);
            const VertexId b = v(gx, eaves, z1);
            const VertexId c = v(gx, yr, zm);
            (void)contour({a, b, c},
                          {{"thickness", "0.12"}, {"mat", "0"}, {"tone", "1"}});
        }
        // КОНЁК-БРЕВНО: выпуск за фронтоны по 0.4 с обеих сторон.
        (void)beam(v(x0 - 0.4f, yr + 0.02f, zm), v(x1 + 0.4f, yr + 0.02f, zm),
                   {{"radius", "0.14"}, {"mat", "0"}, {"tone", "2"}});
        // СТРОПИЛА: пары от конька к карнизам с шагом ~1.5 м, концы торчат
        // за свес на 0.25 — те самые «торцы», которые читаются с земли.
        const int bays = std::max(1, static_cast<int>((x1 - x0) / 1.5f));
        for (int k = 0; k <= bays; ++k) {
            const float x = x0 + (x1 - x0) * static_cast<float>(k)
                          / static_cast<float>(bays);
            for (const float gz : {z0, z1}) {
                // Конец — НАД карнизом с выпуском за свес: конец ниже верха
                // стены висел перед фасадом и читался «верёвкой» (приёмка).
                const float out_z = gz + (gz < zm ? -(o + 0.22f) : (o + 0.22f));
                (void)beam(v(x, yr - 0.06f, zm), v(x, eaves + 0.14f, out_z),
                           {{"radius", "0.08"}, {"mat", "0"}, {"tone", "2"}});
            }
        }
    }

    /// Марш на четырёх точках (fill=6): низ — пара (z_lo), верх — пара (z_hi).
    void stairs_z(float x0, float x1, float z_lo, float z_hi, float y0, float y1) {
        const VertexId a = v(x0, y0, z_lo);
        const VertexId b = v(x1, y0, z_lo);
        const VertexId c = v(x1, y1, z_hi);
        const VertexId d = v(x0, y1, z_hi);
        (void)contour({a, b, c, d},
                      {{"thickness", "0.1"}, {"fill", "6"}, {"mat", "1"},
                       {"tone", "1"}});
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

/// Пол: замкнутый контур ПРОТИВ часовой сверху (лицо вверх), паркет.
void parquet_floor(Forge& f, float x0, float z0, float x1, float z1, float y) {
    const VertexId a = f.v(x0, y, z0);
    const VertexId b = f.v(x0, y, z1);
    const VertexId c = f.v(x1, y, z1);
    const VertexId d = f.v(x1, y, z0);
    (void)f.contour({a, b, c, d},
                    {{"thickness", "0.12"}, {"fill", "5"}, {"mat", "1"},
                     {"tone", "1"}});
}

// ---------------------------------------------------------------------------
// ПОВТОР 1: одноэтажный сруб 6x4 под соломой (демка: log).
// ---------------------------------------------------------------------------
void forge_log() {
    Forge f;
    const float W = 6.0f;
    const float D = 4.0f;
    const float H = 3.25f; // этаж демки, 13u
    parquet_floor(f, 0.0f, 0.0f, W, D, 0.06f);
    f.frame_posts(0.0f, 0.0f, W, D, 0.0f, H, "0");
    // Стены — тёсаный брус (сруб читается материалом). Южная — со сквозным
    // дверным проёмом; окон нет, как у оригинала (одно жильё, один свет).
    const VertexId nw = f.v(0.0f, 0.0f, 0.0f);
    const VertexId ne = f.v(W, 0.0f, 0.0f);
    const VertexId se = f.v(W, 0.0f, D);
    const VertexId sw = f.v(0.0f, 0.0f, D);
    Params logwall = {{"height", "3.25"}, {"thickness", "0.25"}, {"mat", "0"},
                      {"tone", "1"}, {"fill", "4"}, {"logends", "1"},
                      {"wear", "0.35"}};
    (void)f.wall(ne, nw, logwall); // север, лицо на север
    (void)f.wall(se, ne, logwall); // восток, лицо на восток
    (void)f.wall(nw, sw, logwall); // запад, лицо на запад
    (void)f.wall(sw, se,
                 {{"height", "3.25"}, {"thickness", "0.25"}, {"mat", "0"},
                  {"tone", "1"}, {"fill", "4"}, {"doors", "1"}, {"logends", "1"},
                  {"porch", "1"}, {"wear", "0.35"}});
    f.door_leaf(W * 0.5f, 0.0f, D);
    f.gable_roof(0.0f, 0.0f, W, D, H, 1.4f, "6", "1", "", "0.35"); // солома
    f.save("assets/houses/log-replica.dfh");
}

// ---------------------------------------------------------------------------
// ПОВТОР 2: полутораэтажный фахверк 9x4 (демка: frame) — марш на антресоль.
// ---------------------------------------------------------------------------
void forge_frame() {
    Forge f;
    const float W = 9.0f;
    const float D = 4.0f;
    const float H = 3.25f;
    parquet_floor(f, 0.0f, 0.0f, W, D, 0.06f);
    f.frame_posts(0.0f, 0.0f, W, D, 0.0f, H, "0");
    const VertexId nw = f.v(0.0f, 0.0f, 0.0f);
    const VertexId ne = f.v(W, 0.0f, 0.0f);
    const VertexId se = f.v(W, 0.0f, D);
    const VertexId sw = f.v(0.0f, 0.0f, D);
    Params clad = {{"height", "3.25"}, {"thickness", "0.25"}, {"mat", "5"},
                   {"tone", "0"}, {"clad", "1"}, {"windows", "1"},
                   {"shutters", "1"}, {"plinth", "1"}, {"wear", "0.25"}};
    (void)f.wall(ne, nw,
                 {{"height", "3.25"}, {"thickness", "0.25"}, {"mat", "5"},
                  {"tone", "0"}, {"clad", "1"}, {"windows", "2"},
                  {"shutters", "1"}, {"plinth", "1"}, {"wear", "0.25"}});
    (void)f.wall(se, ne, clad);
    (void)f.wall(nw, sw, clad);
    // ЮЖНЫЙ ФАСАД ИЗ ТРЁХ СТЕН: раскладка умеет один вид проёма на стену
    // (дверь берёт верх) — окна получают СВОИ пролёты по бокам двери.
    {
        const VertexId s1 = f.v(3.2f, 0.0f, D);
        const VertexId s2 = f.v(5.8f, 0.0f, D);
        (void)f.wall(sw, s1,
                     {{"height", "3.25"}, {"thickness", "0.25"}, {"mat", "5"},
                      {"tone", "0"}, {"clad", "1"}, {"windows", "1"},
                      {"shutters", "1"}, {"plinth", "1"}, {"wear", "0.25"}});
        (void)f.wall(s1, s2,
                     {{"height", "3.25"}, {"thickness", "0.25"}, {"mat", "5"},
                      {"tone", "0"}, {"clad", "1"}, {"doors", "1"}, {"porch", "1"},
                      {"wear", "0.25"}});
        (void)f.wall(s2, se,
                     {{"height", "3.25"}, {"thickness", "0.25"}, {"mat", "5"},
                      {"tone", "0"}, {"clad", "1"}, {"windows", "1"},
                      {"shutters", "1"}, {"plinth", "1"}, {"wear", "0.25"}});
    }
    f.door_leaf(W * 0.5f, 0.0f, D);
    // АНТРЕСОЛЬ над западной половиной + крутой марш вдоль северной стены,
    // как у демки (b0ce19e: этаж за собственную длину). Этаж 3.25 не кратен
    // подъёму 0.175 в целых — марш считает сам, шаг остаётся ровным.
    {
        const VertexId a = f.v(0.0f, H, 0.0f);
        const VertexId b = f.v(0.0f, H, D);
        const VertexId c = f.v(4.0f, H, D);
        const VertexId d = f.v(4.0f, H, 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.12"}, {"mat", "1"}, {"tone", "1"}});
    }
    // Марш: низ у (7.6, 0), верх у (4.2, 3.25) — движение на запад вдоль
    // северной стены, ширина 1.2.
    {
        const VertexId a = f.v(7.6f, 0.0f, 0.3f);
        const VertexId b = f.v(7.6f, 0.0f, 1.5f);
        const VertexId c = f.v(4.0f, H, 1.5f);
        const VertexId d = f.v(4.0f, H, 0.3f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.1"}, {"fill", "6"}, {"mat", "1"},
                         {"tone", "1"}});
    }
    f.gable_roof(0.0f, 0.0f, W, D, H, 1.6f, "1", "2", "7", "0.25"); // дранка рядами
    f.save("assets/houses/frame-replica.dfh");
}

// ---------------------------------------------------------------------------
// ПОВТОР 3: двухэтажный камень+фахверк 9x4 под черепицей (демка: stone).
// ---------------------------------------------------------------------------
void forge_stone() {
    Forge f;
    const float W = 9.0f;
    const float D = 4.0f;
    const float H = 3.25f;
    parquet_floor(f, 0.0f, 0.0f, W, D, 0.06f);
    f.frame_posts(0.0f, 0.0f, W, D, 0.0f, 2.0f * H, "3");
    const VertexId nw = f.v(0.0f, 0.0f, 0.0f);
    const VertexId ne = f.v(W, 0.0f, 0.0f);
    const VertexId se = f.v(W, 0.0f, D);
    const VertexId sw = f.v(0.0f, 0.0f, D);
    // НИЗ — каменные блоки (fill=3), окна ставит кладка.
    Params stone = {{"height", "3.25"}, {"thickness", "0.3"}, {"mat", "3"},
                    {"tone", "1"}, {"fill", "3"}, {"windows", "1"},
                    {"wear", "0.5"}, {"plinth", "1"}};
    (void)f.wall(ne, nw, stone);
    (void)f.wall(se, ne, stone);
    (void)f.wall(nw, sw, stone);
    {
        const VertexId s1 = f.v(3.2f, 0.0f, D);
        const VertexId s2 = f.v(5.8f, 0.0f, D);
        (void)f.wall(sw, s1,
                     {{"height", "3.25"}, {"thickness", "0.3"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"windows", "1"},
                      {"wear", "0.7"}, {"plinth", "1"}});
        (void)f.wall(s1, s2,
                     {{"height", "3.25"}, {"thickness", "0.3"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"doors", "1"},
                      {"wear", "0.7"}, {"porch", "1"}});
        (void)f.wall(s2, se,
                     {{"height", "3.25"}, {"thickness", "0.3"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"windows", "1"},
                      {"wear", "0.7"}, {"plinth", "1"}});
    }
    f.door_leaf(W * 0.5f, 0.0f, D);
    // ВЕРХ — фахверк по штукатурке, окна чаще.
    const VertexId nw2 = f.v(0.0f, H, 0.0f);
    const VertexId ne2 = f.v(W, H, 0.0f);
    const VertexId se2 = f.v(W, H, D);
    const VertexId sw2 = f.v(0.0f, H, D);
    Params upper = {{"height", "3.25"}, {"thickness", "0.25"}, {"mat", "5"},
                    {"tone", "0"}, {"clad", "1"}, {"windows", "2"},
                    {"shutters", "1"}, {"wear", "0.3"}};
    (void)f.wall(ne2, nw2, upper);
    (void)f.wall(se2, ne2, upper);
    (void)f.wall(nw2, sw2, upper);
    (void)f.wall(sw2, se2, upper);
    // ЭТАЖНЫЙ ПОЛ с проёмом над маршем: настил из двух кусков (запад 0..6.8
    // без полосы марша, восток за верхом марша).
    {
        const VertexId a = f.v(0.0f, H, 0.0f);
        const VertexId b = f.v(0.0f, H, D);
        const VertexId c = f.v(4.4f, H, D);
        const VertexId d = f.v(4.4f, H, 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.12"}, {"mat", "1"}, {"tone", "1"}});
    }
    {
        const VertexId a = f.v(4.4f, H, 1.8f);
        const VertexId b = f.v(4.4f, H, D);
        const VertexId c = f.v(9.0f, H, D);
        const VertexId d = f.v(9.0f, H, 1.8f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.12"}, {"mat", "1"}, {"tone", "1"}});
    }
    // Марш вдоль северной стены на восток: низ (4.6), верх (8.2).
    {
        const VertexId a = f.v(4.6f, 0.0f, 0.3f);
        const VertexId b = f.v(4.6f, 0.0f, 1.5f);
        const VertexId c = f.v(8.2f, H, 1.5f);
        const VertexId d = f.v(8.2f, H, 0.3f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.1"}, {"fill", "6"}, {"mat", "1"},
                         {"tone", "1"}});
    }
    f.gable_roof(0.0f, 0.0f, W, D, 2.0f * H, 1.6f, "4", "1", "8", "0.4"); // черепица рядами
    f.save("assets/houses/stone-replica.dfh");
}

// ---------------------------------------------------------------------------
// НОВЫЙ 1: Г-образный одноэтажный. Западное крыло 4x8 + северный бар 8x4,
// две комнаты, межкомнатная дверь, вход с юга, два ската по крыльям.
// ---------------------------------------------------------------------------
void forge_l_house() {
    Forge f;
    const float H = 2.8f; // 16 ступеней по 0.175 ровно — и человеку не давит
    // ПОЛ — Г-контур (против часовой сверху), паркет по всей площади.
    {
        const VertexId a = f.v(0.0f, 0.06f, 0.0f);
        const VertexId b = f.v(0.0f, 0.06f, 8.0f);
        const VertexId c = f.v(4.0f, 0.06f, 8.0f);
        const VertexId d = f.v(4.0f, 0.06f, 4.0f);
        const VertexId e = f.v(8.0f, 0.06f, 4.0f);
        const VertexId g2 = f.v(8.0f, 0.06f, 0.0f);
        (void)f.contour({a, b, c, d, e, g2},
                        {{"thickness", "0.12"}, {"fill", "5"}, {"mat", "1"},
                         {"tone", "1"}});
    }
    f.frame_posts(0.0f, 0.0f, 8.0f, 4.0f, 0.0f, H, "0");
    // Наружные стены — фахверк с окнами; южная стена западного крыла — вход.
    const VertexId nw = f.v(0.0f, 0.0f, 0.0f);
    const VertexId ne = f.v(8.0f, 0.0f, 0.0f);
    const VertexId e_out = f.v(8.0f, 0.0f, 4.0f);
    const VertexId inner = f.v(4.0f, 0.0f, 4.0f);
    const VertexId c_s = f.v(4.0f, 0.0f, 8.0f);
    const VertexId sw_s = f.v(0.0f, 0.0f, 8.0f);
    Params clad = {{"height", "2.8"}, {"thickness", "0.25"}, {"mat", "5"},
                   {"tone", "0"}, {"clad", "1"}, {"windows", "1"},
                   {"shutters", "1"}, {"plinth", "1"}, {"wear", "0.3"}};
    (void)f.wall(ne, nw, clad);           // север, лицо на север
    (void)f.wall(e_out, ne, clad);        // восточный торец бара, лицо на восток
    (void)f.wall(inner, e_out, clad);     // южная стена бара, лицо в угол Г
    (void)f.wall(c_s, inner, clad);       // восточная стена крыла, лицо на восток
    (void)f.wall(nw, sw_s, clad);         // западная стена крыла, лицо на запад
    (void)f.wall(sw_s, c_s,
                 {{"height", "2.8"}, {"thickness", "0.25"}, {"mat", "5"},
                  {"tone", "0"}, {"clad", "1"}, {"doors", "1"}, {"porch", "1"},
                  {"plinth", "1"}, {"wear", "0.3"}});
    f.door_leaf(2.0f, 0.0f, 8.0f);
    // МЕЖКОМНАТНАЯ стена с рабочей дверью: z=4, x 0..4 (крыло от бара).
    {
        const VertexId a = f.v(0.0f, 0.0f, 4.0f);
        (void)f.wall(a, inner,
                     {{"height", "2.8"}, {"thickness", "0.15"}, {"mat", "5"},
                      {"tone", "0"}, {"doors", "1"}});
        f.door_leaf(2.0f, 0.0f, 4.0f);
    }
    // КРЫША ИЗ ДВУХ СКАТОВ-НАВЕСОВ по крыльям: над крылом скат на юг, над
    // баром — на восток; выше в углу Г они перекрываются. Нижняя кромка
    // ЛЕЖИТ на стене (2.75 при стене 2.8 — заходит), высокая сторона
    // зашита клином-фронтоном: приёмка кадров 20.08 увидела над стенами
    // сквозное небо.
    {
        const VertexId a = f.v(-0.3f, H + 0.9f, -0.3f);
        const VertexId b = f.v(4.3f, H + 0.9f, -0.3f);
        const VertexId c = f.v(4.3f, H - 0.05f, 8.3f);
        const VertexId d = f.v(-0.3f, H - 0.05f, 8.3f);
        {
            const ElementId r1 = f.contour(
                {a, b, c, d},
                {{"thickness", "0.15"}, {"mat", "1"}, {"tone", "2"}});
            (void)f.g.set_param(r1, "fill", "7");
            (void)f.g.set_param(r1, "wear", "0.3");
        }
    }
    {
        const VertexId a = f.v(3.7f, H + 0.9f, -0.3f);
        const VertexId b = f.v(3.7f, H + 0.9f, 4.3f);
        const VertexId c = f.v(8.3f, H - 0.05f, 4.3f);
        const VertexId d = f.v(8.3f, H - 0.05f, -0.3f);
        {
            const ElementId r2 = f.contour(
                {a, b, c, d},
                {{"thickness", "0.15"}, {"mat", "1"}, {"tone", "2"}});
            (void)f.g.set_param(r2, "fill", "7");
            (void)f.g.set_param(r2, "wear", "0.3");
        }
    }
    // КЛИНЬЯ-ФРОНТОНЫ под высокими кромками скатов: север крыла (обе
    // стороны z=0) и запад бара (x=0), плюс восточный торец крыла над баром.
    const auto wedge = [&](float x0, float z0, float x1, float z1, float y_lo,
                           float y_hi_a, float y_hi_b) {
        const VertexId a = f.v(x0, y_lo, z0);
        const VertexId b = f.v(x1, y_lo, z1);
        const VertexId c = f.v(x1, y_hi_b, z1);
        const VertexId d = f.v(x0, y_hi_a, z0);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.12"}, {"mat", "5"}, {"tone", "0"},
                         {"clad", "1"}});
    };
    wedge(0.0f, 0.0f, 4.0f, 0.0f, H - 0.05f, H + 0.85f, H + 0.85f); // север крыла
    wedge(0.0f, 0.0f, 0.0f, 4.0f, H - 0.05f, H + 0.85f, H + 0.1f);  // запад бара
    wedge(4.0f, 0.0f, 4.0f, 4.0f, H - 0.05f, H + 0.85f, H + 0.1f);  // угол Г изнутри
    f.save("assets/houses/l-house.dfh");
}

// ---------------------------------------------------------------------------
// НОВЫЙ 2: П-образный двухэтажный. Крылья 4x10 запад/восток, бар 14x4 на
// севере, двор 6x6 открыт на юг. Два марша (по крылу), раздельные комнаты,
// двери между всеми, дверь из бара во двор, крыша из ЧЕТЫРЁХ частей-навесов.
// ---------------------------------------------------------------------------
void forge_u_house() {
    Forge f;
    const float H = 2.8f;
    const float H2 = 5.6f;
    // ПОЛ первого этажа — П-контур против часовой сверху.
    {
        const VertexId p1 = f.v(0.0f, 0.06f, 0.0f);
        const VertexId p2 = f.v(0.0f, 0.06f, 10.0f);
        const VertexId p3 = f.v(4.0f, 0.06f, 10.0f);
        const VertexId p4 = f.v(4.0f, 0.06f, 4.0f);
        const VertexId p5 = f.v(10.0f, 0.06f, 4.0f);
        const VertexId p6 = f.v(10.0f, 0.06f, 10.0f);
        const VertexId p7 = f.v(14.0f, 0.06f, 10.0f);
        const VertexId p8 = f.v(14.0f, 0.06f, 0.0f);
        (void)f.contour({p1, p2, p3, p4, p5, p6, p7, p8},
                        {{"thickness", "0.12"}, {"fill", "5"}, {"mat", "1"},
                         {"tone", "1"}});
    }
    f.frame_posts(0.0f, 0.0f, 14.0f, 10.0f, 0.0f, H2, "3");
    // ---------- наружные стены, низ камень / верх фахверк ----------
    const auto two_storey_run = [&](float ax, float az, float bx, float bz,
                                    bool low_door, bool up_windows) {
        const VertexId a0 = f.v(ax, 0.0f, az);
        const VertexId b0 = f.v(bx, 0.0f, bz);
        if (low_door) {
            (void)f.wall(a0, b0,
                         {{"height", "2.8"}, {"thickness", "0.3"}, {"mat", "3"},
                          {"tone", "1"}, {"fill", "3"}, {"doors", "1"},
                          {"porch", "1"}, {"wear", "0.4"}});
        } else {
            (void)f.wall(a0, b0,
                         {{"height", "2.8"}, {"thickness", "0.3"}, {"mat", "3"},
                          {"tone", "1"}, {"fill", "3"}, {"windows", "1"},
                          {"plinth", "1"}, {"wear", "0.4"}});
        }
        const VertexId a1 = f.v(ax, H, az);
        const VertexId b1 = f.v(bx, H, bz);
        (void)f.wall(a1, b1,
                     up_windows
                         ? Params{{"height", "2.8"}, {"thickness", "0.25"},
                                  {"mat", "5"}, {"tone", "0"}, {"clad", "1"},
                                  {"windows", "2"}, {"shutters", "1"},
                                  {"wear", "0.3"}}
                         : Params{{"height", "2.8"}, {"thickness", "0.25"},
                                  {"mat", "5"}, {"tone", "0"}, {"clad", "1"},
                                  {"wear", "0.3"}});
    };
    two_storey_run(14.0f, 0.0f, 0.0f, 0.0f, false, true);   // север, лицо на север
    two_storey_run(0.0f, 0.0f, 0.0f, 10.0f, false, true);   // запад, лицо на запад
    two_storey_run(14.0f, 10.0f, 14.0f, 0.0f, false, true); // восток, лицо на восток
    // Южные торцы крыльев: западный — ГЛАВНЫЙ ВХОД, восточный — второй выход.
    two_storey_run(0.0f, 10.0f, 4.0f, 10.0f, true, false);
    f.door_leaf(2.0f, 0.0f, 10.0f);
    two_storey_run(10.0f, 10.0f, 14.0f, 10.0f, true, false);
    f.door_leaf(12.0f, 0.0f, 10.0f);
    // Дворовые фасады крыльев (x=4 и x=10, z 4..10) — окна на двор,
    // лицо В ДВОР (запад крыла смотрит +X, восток -X).
    two_storey_run(4.0f, 10.0f, 4.0f, 4.0f, false, true);
    two_storey_run(10.0f, 4.0f, 10.0f, 10.0f, false, true);
    // Южная стена бара (фасад во двор): ДВЕРЬ ВО ДВОР на первом этаже.
    two_storey_run(4.0f, 4.0f, 10.0f, 4.0f, true, true);
    f.door_leaf(7.0f, 0.0f, 4.0f);
    // ---------- перегородки крыло-бар с рабочими дверями, оба этажа ----------
    const auto partition = [&](float x0, float x1, float y) {
        const VertexId a = f.v(x0, y, 4.0f);
        const VertexId b = f.v(x1, y, 4.0f);
        (void)f.wall(a, b,
                     {{"height", "2.8"}, {"thickness", "0.15"}, {"mat", "5"},
                      {"tone", "0"}, {"doors", "1"}});
        f.door_leaf((x0 + x1) * 0.5f, y, 4.0f);
    };
    partition(0.0f, 4.0f, 0.0f);   // запад, 1 этаж
    partition(10.0f, 14.0f, 0.0f); // восток, 1 этаж
    partition(0.0f, 4.0f, H);      // запад, 2 этаж
    partition(10.0f, 14.0f, H);    // восток, 2 этаж
    // ---------- полы второго этажа: бар + балконы крыльев + площадки ----------
    const auto deck = [&](float x0, float z0, float x1, float z1) {
        const VertexId a = f.v(x0, H, z0);
        const VertexId b = f.v(x0, H, z1);
        const VertexId c = f.v(x1, H, z1);
        const VertexId d = f.v(x1, H, z0);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.12"}, {"mat", "1"}, {"tone", "1"}});
    };
    deck(0.0f, 0.0f, 14.0f, 4.0f);  // бар
    deck(2.0f, 4.0f, 4.0f, 10.0f);  // комната-балкон западного крыла
    deck(10.0f, 4.0f, 12.0f, 10.0f); // комната-балкон восточного крыла
    deck(0.0f, 4.0f, 2.0f, 5.0f);   // площадка над западным маршем
    deck(12.0f, 4.0f, 14.0f, 5.0f); // площадка над восточным маршем
    // ---------- два марша вдоль наружных стен крыльев ----------
    // Запад: низ у z=9.6, верх у z=5.0 (подъём на север), ширина 1.4.
    {
        const VertexId a = f.v(0.3f, 0.0f, 9.6f);
        const VertexId b = f.v(1.7f, 0.0f, 9.6f);
        const VertexId c = f.v(1.7f, H, 5.0f);
        const VertexId d = f.v(0.3f, H, 5.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.1"}, {"fill", "6"}, {"mat", "1"},
                         {"tone", "1"}});
    }
    // Восток: зеркально.
    {
        const VertexId a = f.v(12.3f, 0.0f, 9.6f);
        const VertexId b = f.v(13.7f, 0.0f, 9.6f);
        const VertexId c = f.v(13.7f, H, 5.0f);
        const VertexId d = f.v(12.3f, H, 5.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.1"}, {"fill", "6"}, {"mat", "1"},
                         {"tone", "1"}});
    }
    // ---------- КРЫША ИЗ ЧЕТЫРЁХ ЧАСТЕЙ-НАВЕСОВ (заказ дословно) ----------
    // Крылья — скаты на юг; бар — двускат из двух половин (на север и во
    // двор). Черепица.
    const char* tile = "4";
    {
        const VertexId a = f.v(-0.3f, H2 + 1.0f, 3.7f);
        const VertexId b = f.v(4.3f, H2 + 1.0f, 3.7f);
        const VertexId c = f.v(4.3f, H2 - 0.1f, 10.3f);
        const VertexId d = f.v(-0.3f, H2 - 0.1f, 10.3f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.15"}, {"mat", tile}, {"tone", "1"}, {"fill", "8"}, {"wear", "0.4"}});
    }
    {
        const VertexId a = f.v(9.7f, H2 + 1.0f, 3.7f);
        const VertexId b = f.v(14.3f, H2 + 1.0f, 3.7f);
        const VertexId c = f.v(14.3f, H2 - 0.1f, 10.3f);
        const VertexId d = f.v(9.7f, H2 - 0.1f, 10.3f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.15"}, {"mat", tile}, {"tone", "1"}, {"fill", "8"}, {"wear", "0.4"}});
    }
    {
        const VertexId a = f.v(-0.3f, H2 + 1.0f, 2.0f);
        const VertexId b = f.v(14.3f, H2 + 1.0f, 2.0f);
        const VertexId c = f.v(14.3f, H2 - 0.1f, -0.3f);
        const VertexId d = f.v(-0.3f, H2 - 0.1f, -0.3f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.15"}, {"mat", tile}, {"tone", "1"}, {"fill", "8"}, {"wear", "0.4"}});
    }
    {
        const VertexId a = f.v(14.3f, H2 + 1.0f, 2.0f);
        const VertexId b = f.v(-0.3f, H2 + 1.0f, 2.0f);
        const VertexId c = f.v(-0.3f, H2 - 0.1f, 4.3f);
        const VertexId d = f.v(14.3f, H2 - 0.1f, 4.3f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.15"}, {"mat", tile}, {"tone", "1"}, {"fill", "8"}, {"wear", "0.4"}});
    }
    // КОНЁК-БРЕВНО двускату бара и СТРОПИЛЬНЫЕ ТОРЦЫ по карнизам крыльев —
    // приёмка кадров: «ни одного стропильного торца, конька нет».
    (void)f.beam(f.v(-0.5f, H2 + 1.02f, 2.0f), f.v(14.5f, H2 + 1.02f, 2.0f),
                 {{"radius", "0.14"}, {"mat", "0"}, {"tone", "2"}});
    // Торцы — из-под НИЗКИХ (южных) кромок скатов крыльев, наружу и чуть
    // вниз; прежние палки у верхних кромок торчали В НЕБО (приёмка кадров).
    for (int k = 0; k <= 3; ++k) {
        const float x_w = 0.4f + 3.2f * static_cast<float>(k) / 3.0f;
        (void)f.beam(f.v(x_w, H2 + 0.05f, 9.6f), f.v(x_w, H2 - 0.35f, 10.75f),
                     {{"radius", "0.07"}, {"mat", "0"}, {"tone", "2"}});
        const float x_e = 10.4f + 3.2f * static_cast<float>(k) / 3.0f;
        (void)f.beam(f.v(x_e, H2 + 0.05f, 9.6f), f.v(x_e, H2 - 0.35f, 10.75f),
                     {{"radius", "0.07"}, {"mat", "0"}, {"tone", "2"}});
    }
    f.save("assets/houses/u-house.dfh");
}

} // namespace

int main() {
    forge_log();
    forge_frame();
    forge_stone();
    forge_l_house();
    forge_u_house();
    return 0;
}
