/*
Created: 20:08:2026 - 13:40:00
Last updated: 21:08:2026 - 01:10:00
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
- 20:08:2026 - 20:30:00: Марши у-дома — доски с зазорами; потолок бара с балками; пол сруба ветхий.
- 20:08:2026 - 21:20:00: Стропила двускатов — ПОД настилом (лежали поверх гонта, приёмка №3).
- 20:08:2026 - 22:40:00: Одна вершина на точку (дедуп): угол тянет и стену, и столб.
- 21:08:2026 - 01:50:00: eaves_trim: двойной кант карниза (тёсовая доска + слега) вдоль нижних кромок скатов двускатов.
- 21:08:2026 - 00:45:00: Тон дранки среднего ряда (скат был чёрным сверху); сруб wear 0.55.
- 21:08:2026 - 01:10:00: ГОРОДСКОЙ НАБОР ВАЙТРАНА — 17 рецептов: сегмент стены
  с боевым ходом и зубцами (crenellated_run), башня и донжон с шатром из
  четырёх треугольников, ворота с щеками и перемычкой, лестницы улиц (3 м и
  один марш на 6 м подъёма, open=2 — стык двух маршей дважды ловил бота в
  щель), плазы 12/20, мост с опорами в русле и заездами-аппарелями по торцам
  (без пандуса ступень настила 0.85 м непроходима), кольцо-бортик дерева,
  лавка с верандой, стойло, колодец, лонгхолл в венцах, храм с портиком и
  замок-керн (низ камень, верх фахверк, подиум с двумя маршами, крылья).
  Числа — docs/WHITERUN_RESEARCH.md; артефакты assets/houses/city-*.dfh.
*/

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseGraph.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/geometric.hpp>
#include <fstream>
#include <map>
#include <initializer_list>
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

/// Рука над графом: рецепты читаются как список деталей, а не как API-вязь.
struct Forge {
    HouseGraph g;
    /// ОДНА ВЕРШИНА НА ТОЧКУ: рецепты называют углы повторно (столб и стена в
    /// одном углу), а граф не дедуплицирует — после распаковки пользователь
    /// тянул угол, и стена уезжала БЕЗ столба (аудит #3, находка 10).
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

    /// КАНТ КАРНИЗА (EXTERIOR_CATALOG.md: торец ската — всегда двойной кант,
    /// тёсовая доска + бревно-слега; без него кровля «картонно-тонкая»,
    /// приёмка №3). Кладётся вдоль нижней кромки ската.
    void eaves_trim(glm::vec3 lo_a, glm::vec3 lo_b) {
        // Доска-кант чуть ниже кромки, плашмя по скату.
        (void)beam(v(lo_a.x, lo_a.y - 0.06f, lo_a.z), v(lo_b.x, lo_b.y - 0.06f, lo_b.z),
                   {{"radius", "0.12"}, {"form", "plank"}, {"mat", "1"},
                    {"tone", "1"}});
        // Слега-бревно под самым краем.
        (void)beam(v(lo_a.x, lo_a.y - 0.16f, lo_a.z), v(lo_b.x, lo_b.y - 0.16f, lo_b.z),
                   {{"radius", "0.09"}, {"mat", "0"}, {"tone", "2"}});
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
        eaves_trim({x0 - o, eaves - 0.1f, z0 - o}, {x1 + o, eaves - 0.1f, z0 - o});
        eaves_trim({x0 - o, eaves - 0.1f, z1 + o}, {x1 + o, eaves - 0.1f, z1 + o});
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
                // ПОД настилом, торец выглядывает из-под свеса (приёмка №3:
                // стропило выше кромки лежало ПОВЕРХ гонта). Верх — под
                // коньком с запасом на толщину кровли, низ — под карнизной
                // кромкой, выпуск 0.22 за свес.
                const float out_z = gz + (gz < zm ? -(o + 0.22f) : (o + 0.22f));
                (void)beam(v(x, yr - 0.30f, zm), v(x, eaves - 0.34f, out_z),
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
    // Пол СТАРЫЙ: неровные доски, щели, обломки (wear в паркете).
    {
        const VertexId a = f.v(0.0f, 0.06f, 0.0f);
        const VertexId b = f.v(0.0f, 0.06f, D);
        const VertexId c = f.v(W, 0.06f, D);
        const VertexId d = f.v(W, 0.06f, 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.12"}, {"fill", "5"}, {"mat", "1"},
                         {"tone", "3"}, {"wear", "0.7"}});
    }
    f.frame_posts(0.0f, 0.0f, W, D, 0.0f, H, "0");
    // Стены — тёсаный брус (сруб читается материалом). Южная — со сквозным
    // дверным проёмом; окон нет, как у оригинала (одно жильё, один свет).
    const VertexId nw = f.v(0.0f, 0.0f, 0.0f);
    const VertexId ne = f.v(W, 0.0f, 0.0f);
    const VertexId se = f.v(W, 0.0f, D);
    const VertexId sw = f.v(0.0f, 0.0f, D);
    Params logwall = {{"height", "3.25"}, {"thickness", "0.25"}, {"mat", "0"},
                      {"tone", "1"}, {"fill", "4"}, {"logends", "1"},
                      {"wear", "0.55"}};
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
    f.gable_roof(0.0f, 0.0f, W, D, H, 1.6f, "1", "1", "7", "0.25"); // дранка рядами
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
    // Бар: потолок первого этажа несёт БАЛКИ (каталог интерьеров, топ-1).
    {
        const VertexId a = f.v(0.0f, H, 0.0f);
        const VertexId b = f.v(0.0f, H, 4.0f);
        const VertexId c = f.v(14.0f, H, 4.0f);
        const VertexId d = f.v(14.0f, H, 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.12"}, {"mat", "1"}, {"tone", "1"},
                         {"beams", "1"}});
    }
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
                         {"tone", "1"}, {"open", "1"}, {"wear", "0.3"}});
    }
    // Восток: зеркально.
    {
        const VertexId a = f.v(12.3f, 0.0f, 9.6f);
        const VertexId b = f.v(13.7f, 0.0f, 9.6f);
        const VertexId c = f.v(13.7f, H, 5.0f);
        const VertexId d = f.v(12.3f, H, 5.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.1"}, {"fill", "6"}, {"mat", "1"},
                         {"tone", "1"}, {"open", "1"}, {"wear", "0.3"}});
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


// ===========================================================================
// ГОРОДСКОЙ НАБОР (Вайтран, 21.08): стены, башни, ворота, замок, залы,
// уличный слой. Числа — docs/WHITERUN_RESEARCH.md (оценки помечены там).
// ===========================================================================

/// Простая каменная стена-цепочка с зубцами и боевым ходом.
static void crenellated_run(Forge& f, glm::vec3 a, glm::vec3 b, float h,
                            float th, const char* wear) {
    const VertexId va = f.v(a.x, a.y, a.z);
    const VertexId vb = f.v(b.x, b.y, b.z);
    char hbuf[16];
    std::snprintf(hbuf, sizeof(hbuf), "%.2f", h);
    char tbuf[16];
    std::snprintf(tbuf, sizeof(tbuf), "%.2f", th);
    (void)f.wall(va, vb,
                 {{"height", hbuf}, {"thickness", tbuf}, {"mat", "3"},
                  {"tone", "1"}, {"fill", "3"}, {"wear", wear},
                  {"unsupported", "1"}});
    // Боевой ход: полка изнутри на 2/3 высоты (лицо стены — наружу, изнанка
    // внутрь города; полка кладётся к изнанке).
    const glm::vec3 d = glm::normalize(b - a);
    const glm::vec3 inw = glm::normalize(glm::cross(glm::vec3{0, 1, 0}, d));
    const glm::vec3 wa = a + inw * (th * 0.5f + 0.55f) + glm::vec3{0, h - 1.7f, 0};
    const glm::vec3 wb = b + inw * (th * 0.5f + 0.55f) + glm::vec3{0, h - 1.7f, 0};
    {
        const VertexId p1 = f.v(wa.x, wa.y, wa.z);
        const VertexId p2 = f.v(wa.x + inw.x * 1.1f, wa.y, wa.z + inw.z * 1.1f);
        const VertexId p3 = f.v(wb.x + inw.x * 1.1f, wb.y, wb.z + inw.z * 1.1f);
        const VertexId p4 = f.v(wb.x, wb.y, wb.z);
        (void)f.contour({p1, p2, p3, p4},
                        {{"thickness", "0.2"}, {"mat", "1"}, {"tone", "2"},
                         {"unsupported", "1"}});
    }
    // Зубцы: короткие стенки поверх кромки с шагом ~2 м.
    const float len = glm::length(b - a);
    const int teeth = std::max(2, static_cast<int>(len / 2.2f));
    for (int t = 0; t < teeth; ++t) {
        const float u0 = (len / teeth) * (static_cast<float>(t) + 0.2f);
        const float u1 = u0 + 0.9f;
        const glm::vec3 ta = a + d * u0 + glm::vec3{0, h, 0};
        const glm::vec3 tb = a + d * std::min(u1, len) + glm::vec3{0, h, 0};
        const VertexId q1 = f.v(ta.x, ta.y, ta.z);
        const VertexId q2 = f.v(tb.x, tb.y, tb.z);
        (void)f.wall(q1, q2,
                     {{"height", "0.8"}, {"thickness", "0.5"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"wear", wear},
                      {"unsupported", "1"}});
    }
}

/// БАШНЯ смотровая: квадратный ствол, площадка, парапет с зубцами, шатёр.
static void forge_tower() {
    Forge f;
    const float S = 3.6f;
    const float H = 9.0f;
    const auto wall_q = [&](float x0, float z0, float x1, float z1) {
        const VertexId a = f.v(x0, 0.0f, z0);
        const VertexId b = f.v(x1, 0.0f, z1);
        (void)f.wall(a, b,
                     {{"height", "9"}, {"thickness", "0.6"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"wear", "0.5"}});
    };
    wall_q(S, 0.0f, 0.0f, 0.0f);  // север, лицо наружу
    wall_q(S, S, S, 0.0f);        // восток
    wall_q(0.0f, 0.0f, 0.0f, S);  // запад
    wall_q(0.0f, S, S, S);        // юг
    // Площадка и парапет.
    {
        const VertexId a = f.v(-0.3f, H, -0.3f);
        const VertexId b = f.v(-0.3f, H, S + 0.3f);
        const VertexId c = f.v(S + 0.3f, H, S + 0.3f);
        const VertexId d = f.v(S + 0.3f, H, -0.3f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.25"}, {"mat", "3"}, {"tone", "1"},
                         {"unsupported", "1"}});
    }
    const float P = H + 0.25f;
    for (int side = 0; side < 4; ++side) {
        for (int t = 0; t < 3; ++t) {
            const float u0 = -0.3f + (S + 0.6f) * (0.08f + 0.3f * t);
            const float u1 = u0 + 0.75f;
            glm::vec3 pa, pb;
            if (side == 0) { pa = {u1, P, -0.3f}; pb = {u0, P, -0.3f}; }
            else if (side == 1) { pa = {S + 0.3f, P, u1}; pb = {S + 0.3f, P, u0}; }
            else if (side == 2) { pa = {u0, P, S + 0.3f}; pb = {u1, P, S + 0.3f}; }
            else { pa = {-0.3f, P, u0}; pb = {-0.3f, P, u1}; }
            const VertexId q1 = f.v(pa.x, pa.y, pa.z);
            const VertexId q2 = f.v(pb.x, pb.y, pb.z);
            (void)f.wall(q1, q2,
                         {{"height", "1.0"}, {"thickness", "0.35"}, {"mat", "3"},
                          {"tone", "1"}, {"fill", "3"}, {"wear", "0.5"},
                          {"unsupported", "1"}});
        }
    }
    // Шатёр: четыре треугольных ската к центру, дранка.
    const float R = P + 1.0f;
    const float apex = R + 1.8f;
    const glm::vec3 cx{S * 0.5f, apex, S * 0.5f};
    const glm::vec3 corners[4] = {{-0.4f, R, -0.4f}, {S + 0.4f, R, -0.4f},
                                  {S + 0.4f, R, S + 0.4f}, {-0.4f, R, S + 0.4f}};
    for (int k = 0; k < 4; ++k) {
        const glm::vec3 c1 = corners[k];
        const glm::vec3 c2 = corners[(k + 1) % 4];
        const VertexId a = f.v(c1.x, c1.y, c1.z);
        const VertexId b = f.v(c2.x, c2.y, c2.z);
        const VertexId t = f.v(cx.x, cx.y, cx.z);
        const ElementId s = f.contour({a, b, t},
                                      {{"thickness", "0.12"}, {"mat", "1"},
                                       {"tone", "1"}, {"unsupported", "1"}});
        (void)f.g.set_param(s, "roof", "1");
    }
    // Дверь внизу южной стены.
    f.door_leaf(S * 0.5f, 0.0f, S);
    f.save("assets/houses/city-tower.dfh");
}

/// ДОНЖОН: та же башня, но 14 м — замковая вертикаль (приёмка №2:
/// «замок не доминирует, сарай»). Отдельный рецепт, а не параметр: полка
/// построек живёт файлами.
static void forge_tower_tall() {
    Forge f;
    const float S = 4.4f;
    const float H = 14.0f;
    const auto wall_q = [&](float x0, float z0, float x1, float z1) {
        const VertexId a = f.v(x0, 0.0f, z0);
        const VertexId b = f.v(x1, 0.0f, z1);
        (void)f.wall(a, b,
                     {{"height", "14"}, {"thickness", "0.7"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"windows", "1"},
                      {"wear", "0.4"}});
    };
    wall_q(S, 0.0f, 0.0f, 0.0f);
    wall_q(S, S, S, 0.0f);
    wall_q(0.0f, 0.0f, 0.0f, S);
    wall_q(0.0f, S, S, S);
    {
        const VertexId a = f.v(-0.35f, H, -0.35f);
        const VertexId b = f.v(-0.35f, H, S + 0.35f);
        const VertexId c = f.v(S + 0.35f, H, S + 0.35f);
        const VertexId d = f.v(S + 0.35f, H, -0.35f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.3"}, {"mat", "3"}, {"tone", "1"},
                         {"unsupported", "1"}});
    }
    const float P = H + 0.3f;
    for (int side = 0; side < 4; ++side) {
        for (int t = 0; t < 3; ++t) {
            const float u0 = -0.35f + (S + 0.7f) * (0.08f + 0.3f * t);
            const float u1 = u0 + 0.9f;
            glm::vec3 pa, pb;
            if (side == 0) { pa = {u1, P, -0.35f}; pb = {u0, P, -0.35f}; }
            else if (side == 1) { pa = {S + 0.35f, P, u1}; pb = {S + 0.35f, P, u0}; }
            else if (side == 2) { pa = {u0, P, S + 0.35f}; pb = {u1, P, S + 0.35f}; }
            else { pa = {-0.35f, P, u0}; pb = {-0.35f, P, u1}; }
            const VertexId q1 = f.v(pa.x, pa.y, pa.z);
            const VertexId q2 = f.v(pb.x, pb.y, pb.z);
            (void)f.wall(q1, q2,
                         {{"height", "1.1"}, {"thickness", "0.4"}, {"mat", "3"},
                          {"tone", "1"}, {"fill", "3"}, {"wear", "0.4"},
                          {"unsupported", "1"}});
        }
    }
    f.door_leaf(S * 0.5f, 0.0f, S);
    f.save("assets/houses/city-donjon.dfh");
}

/// КОЛЬЦО ГИЛДЕРГРИНА: приподнятый восьмигранный бортик вокруг ствола.
static void forge_tree_ring() {
    Forge f;
    const float R = 2.6f;
    const int N = 8;
    for (int k = 0; k < N; ++k) {
        const float a0 = 2.0f * 3.14159265f * static_cast<float>(k) / N;
        const float a1 = 2.0f * 3.14159265f * static_cast<float>(k + 1) / N;
        const VertexId p = f.v(R + R * std::cos(a0), 0.0f, R + R * std::sin(a0));
        const VertexId q = f.v(R + R * std::cos(a1), 0.0f, R + R * std::sin(a1));
        (void)f.wall(p, q,
                     {{"height", "0.5"}, {"thickness", "0.35"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"wear", "0.5"}});
    }
    f.save("assets/houses/city-treering.dfh");
}

/// СЕГМЕНТ СТЕНЫ 12 м с зубцами и боевым ходом.
static void forge_wall12() {
    Forge f;
    crenellated_run(f, {0.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f}, 6.0f, 1.2f, "0.55");
    f.save("assets/houses/city-wall12.dfh");
}

/// ВОРОТА: две щеки, надвратная перемычка, проём 4 м.
static void forge_gate() {
    Forge f;
    crenellated_run(f, {0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}, 7.0f, 1.4f, "0.5");
    crenellated_run(f, {8.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f}, 7.0f, 1.4f, "0.5");
    // Перемычка над проёмом: стена на высоких якорях.
    {
        const VertexId a = f.v(4.0f, 4.6f, 0.0f);
        const VertexId b = f.v(8.0f, 4.6f, 0.0f);
        (void)f.wall(a, b,
                     {{"height", "2.4"}, {"thickness", "1.4"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"wear", "0.5"},
                      {"unsupported", "1"}});
    }
    f.save("assets/houses/city-gate.dfh");
}

/// УЛИЧНАЯ ЛЕСТНИЦА-6: один марш на ПОЛНЫЙ подъём террасы (стык двух
/// маршей дважды ловил бота в щель — класс проблемы снят одним телом).
static void forge_street_stairs6() {
    Forge f;
    const VertexId a = f.v(0.0f, 0.0f, 12.0f);
    const VertexId b = f.v(4.0f, 0.0f, 12.0f);
    const VertexId c = f.v(4.0f, 6.0f, 0.0f);
    const VertexId d = f.v(0.0f, 6.0f, 0.0f);
    (void)f.contour({a, b, c, d},
                    {{"thickness", "0.15"}, {"fill", "6"}, {"open", "2"},
                     {"mat", "3"}, {"tone", "1"}, {"wear", "0.45"}});
    f.save("assets/houses/city-stairs6.dfh");
}

/// УЛИЧНАЯ ЛЕСТНИЦА: каменный марш 4 м шириной, подъём 3 м, блоками.
static void forge_street_stairs() {
    Forge f;
    const VertexId a = f.v(0.0f, 0.0f, 6.0f);
    const VertexId b = f.v(4.0f, 0.0f, 6.0f);
    const VertexId c = f.v(4.0f, 3.0f, 0.0f);
    const VertexId d = f.v(0.0f, 3.0f, 0.0f);
    (void)f.contour({a, b, c, d},
                    {{"thickness", "0.15"}, {"fill", "6"}, {"open", "2"},
                     {"mat", "3"}, {"tone", "1"}, {"wear", "0.45"}});
    f.save("assets/houses/city-stairs.dfh");
}

/// ПЛАЗА: мощёная плита с бордюром-секциями.
static void forge_plaza(const char* file, float w, float d) {
    Forge f;
    const VertexId a = f.v(0.0f, 0.02f, 0.0f);
    const VertexId b = f.v(0.0f, 0.02f, d);
    const VertexId c = f.v(w, 0.02f, d);
    const VertexId e = f.v(w, 0.02f, 0.0f);
    (void)f.contour({a, b, c, e},
                    {{"thickness", "0.16"}, {"mat", "3"}, {"tone", "1"},
                     {"wear", "0.4"}});
    // Бордюр по периметру: низкие каменные ленты (сами секциями).
    const glm::vec3 pts[5] = {{0, 0, 0}, {w, 0, 0}, {w, 0, d}, {0, 0, d}, {0, 0, 0}};
    for (int k = 0; k < 4; ++k) {
        const VertexId p = f.v(pts[k].x, 0.0f, pts[k].z);
        const VertexId q = f.v(pts[k + 1].x, 0.0f, pts[k + 1].z);
        (void)f.wall(p, q,
                     {{"height", "0.22"}, {"thickness", "0.25"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"wear", "0.5"}});
    }
    f.save(file);
}

/// МОСТ через городской рукав: плита с бордюрами на двух опорах.
static void forge_bridge() {
    Forge f;
    // Опоры в русле.
    for (const float x : {1.2f, 6.8f}) {
        const VertexId a = f.v(x, -2.2f, 0.4f);
        const VertexId b = f.v(x, -2.2f, 3.6f);
        (void)f.wall(a, b,
                     {{"height", "2.4"}, {"thickness", "0.8"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"wear", "0.6"},
                      {"unsupported", "1"}});
    }
    const VertexId a = f.v(-0.6f, 0.2f, 0.0f);
    const VertexId b = f.v(-0.6f, 0.2f, 4.0f);
    const VertexId c = f.v(8.6f, 0.2f, 4.0f);
    const VertexId e = f.v(8.6f, 0.2f, 0.0f);
    (void)f.contour({a, b, c, e},
                    {{"thickness", "0.3"}, {"mat", "3"}, {"tone", "1"},
                     {"wear", "0.5"}, {"unsupported", "1"}});
    // ЗАЕЗДЫ-АППАРЕЛИ по торцам: настил выше берега, и без пандуса ни бот,
    // ни игрок ступень настила не берут.
    for (const float xr : {-2.4f, 8.6f}) {
        const VertexId r1 = f.v(xr, -0.5f, 0.0f);
        const VertexId r2 = f.v(xr, -0.5f, 4.0f);
        const VertexId r3 = f.v(xr < 0.0f ? -0.6f : 10.4f, 0.32f, 4.0f);
        const VertexId r4 = f.v(xr < 0.0f ? -0.6f : 10.4f, 0.32f, 0.0f);
        if (xr < 0.0f) {
            (void)f.contour({r1, r2, r3, r4},
                            {{"thickness", "0.2"}, {"mat", "3"}, {"tone", "1"},
                             {"wear", "0.5"}, {"unsupported", "1"}});
        } else {
            const VertexId q3 = f.v(8.6f, 0.32f, 4.0f);
            const VertexId q4 = f.v(8.6f, 0.32f, 0.0f);
            const VertexId q1 = f.v(10.4f, -0.5f, 0.0f);
            const VertexId q2 = f.v(10.4f, -0.5f, 4.0f);
            (void)f.contour({q1, q2, q3, q4},
                            {{"thickness", "0.2"}, {"mat", "3"}, {"tone", "1"},
                             {"wear", "0.5"}, {"unsupported", "1"}});
        }
    }
    for (const float z : {0.0f, 4.0f}) {
        const VertexId p = f.v(-0.6f, 0.35f, z);
        const VertexId q = f.v(8.6f, 0.35f, z);
        (void)f.wall(p, q,
                     {{"height", "0.6"}, {"thickness", "0.3"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"wear", "0.6"},
                      {"unsupported", "1"}});
    }
    f.save("assets/houses/city-bridge.dfh");
}

/// МАЛЫЙ ДОМ 4.5x6: фахверк под дранкой, полный набор деталей.
static void forge_small_house() {
    Forge f;
    const float W = 4.5f;
    const float D = 6.0f;
    const float H = 2.8f;
    {
        const VertexId a = f.v(0.0f, 0.06f, 0.0f);
        const VertexId b = f.v(0.0f, 0.06f, D);
        const VertexId c = f.v(W, 0.06f, D);
        const VertexId d = f.v(W, 0.06f, 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.12"}, {"fill", "5"}, {"mat", "1"},
                         {"tone", "1"}, {"wear", "0.5"}});
    }
    f.frame_posts(0.0f, 0.0f, W, D, 0.0f, H, "0");
    const VertexId nw = f.v(0.0f, 0.0f, 0.0f);
    const VertexId ne = f.v(W, 0.0f, 0.0f);
    const VertexId se = f.v(W, 0.0f, D);
    const VertexId sw = f.v(0.0f, 0.0f, D);
    Params clad = {{"height", "2.8"}, {"thickness", "0.25"}, {"mat", "5"},
                   {"tone", "0"}, {"clad", "1"}, {"windows", "1"},
                   {"shutters", "1"}, {"plinth", "1"}, {"wear", "0.45"}};
    (void)f.wall(ne, nw, clad);
    (void)f.wall(se, ne, clad);
    (void)f.wall(nw, sw, clad);
    (void)f.wall(sw, se,
                 {{"height", "2.8"}, {"thickness", "0.25"}, {"mat", "5"},
                  {"tone", "0"}, {"clad", "1"}, {"doors", "1"}, {"porch", "1"},
                  {"plinth", "1"}, {"wear", "0.45"}});
    f.door_leaf(W * 0.5f, 0.0f, D);
    f.gable_roof(0.0f, 0.0f, W, D, H, 1.5f, "1", "1", "7", "0.45");
    f.save("assets/houses/city-house-s.dfh");
}

/// ЛАВКА 6x6 с верандой-навесом на столбах.
static void forge_shop() {
    Forge f;
    const float W = 6.0f;
    const float D = 6.0f;
    const float H = 2.9f;
    {
        const VertexId a = f.v(0.0f, 0.06f, 0.0f);
        const VertexId b = f.v(0.0f, 0.06f, D);
        const VertexId c = f.v(W, 0.06f, D);
        const VertexId d = f.v(W, 0.06f, 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.12"}, {"fill", "5"}, {"mat", "1"},
                         {"tone", "1"}, {"wear", "0.35"}});
    }
    f.frame_posts(0.0f, 0.0f, W, D, 0.0f, H, "0");
    const VertexId nw = f.v(0.0f, 0.0f, 0.0f);
    const VertexId ne = f.v(W, 0.0f, 0.0f);
    const VertexId se = f.v(W, 0.0f, D);
    const VertexId sw = f.v(0.0f, 0.0f, D);
    Params low = {{"height", "2.9"}, {"thickness", "0.28"}, {"mat", "3"},
                  {"tone", "1"}, {"fill", "3"}, {"windows", "1"},
                  {"plinth", "1"}, {"wear", "0.5"}};
    (void)f.wall(ne, nw, low);
    (void)f.wall(se, ne, low);
    (void)f.wall(nw, sw, low);
    (void)f.wall(sw, se,
                 {{"height", "2.9"}, {"thickness", "0.28"}, {"mat", "3"},
                  {"tone", "1"}, {"fill", "3"}, {"doors", "1"}, {"porch", "1"},
                  {"wear", "0.5"}});
    f.door_leaf(W * 0.5f, 0.0f, D);
    // Веранда: столбы + наклонный навес перед входом.
    for (const float x : {0.4f, W - 0.4f}) {
        const VertexId lo = f.v(x, 0.0f, D + 2.0f);
        const VertexId hi = f.v(x, 2.4f, D + 2.0f);
        (void)f.beam(lo, hi, {{"radius", "0.11"}, {"mat", "0"}, {"tone", "2"}});
    }
    {
        const VertexId a = f.v(-0.3f, H + 0.4f, D - 0.2f);
        const VertexId b = f.v(W + 0.3f, H + 0.4f, D - 0.2f);
        const VertexId c = f.v(W + 0.3f, 2.35f, D + 2.4f);
        const VertexId d = f.v(-0.3f, 2.35f, D + 2.4f);
        const ElementId s = f.contour({a, b, c, d},
                                      {{"thickness", "0.12"}, {"mat", "1"},
                                       {"tone", "2"}, {"fill", "7"},
                                       {"wear", "0.4"}});
        (void)f.g.set_param(s, "roof", "1");
        (void)f.g.set_param(s, "unsupported", "1");
    }
    f.gable_roof(0.0f, 0.0f, W, D, H, 1.7f, "1", "1", "7", "0.4");
    f.save("assets/houses/city-shop.dfh");
}

/// ПРИЛАВОК РЫНКА: стойка + столбы + тент-навес.
static void forge_stall() {
    Forge f;
    {
        const VertexId a = f.v(0.0f, 0.85f, 0.0f);
        const VertexId b = f.v(0.0f, 0.85f, 0.9f);
        const VertexId c = f.v(2.2f, 0.85f, 0.9f);
        const VertexId d = f.v(2.2f, 0.85f, 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.08"}, {"mat", "1"}, {"tone", "1"},
                         {"unsupported", "1"}});
    }
    // Передние надстолешные и задние столбы.
    for (const float x : {0.1f, 2.1f}) {
        const VertexId lo = f.v(x, 0.0f, 0.1f);
        const VertexId hi = f.v(x, 2.2f, 0.1f);
        (void)f.beam(lo, hi, {{"radius", "0.07"}, {"mat", "0"}, {"tone", "2"}});
        const VertexId lo2 = f.v(x, 0.0f, 0.8f);
        const VertexId hi2 = f.v(x, 1.9f, 0.8f);
        (void)f.beam(lo2, hi2, {{"radius", "0.07"}, {"mat", "0"}, {"tone", "2"}});
    }
    {
        const VertexId a = f.v(-0.25f, 2.25f, -0.35f);
        const VertexId b = f.v(2.45f, 2.25f, -0.35f);
        const VertexId c = f.v(2.45f, 1.85f, 1.25f);
        const VertexId d = f.v(-0.25f, 1.85f, 1.25f);
        const ElementId s = f.contour({a, b, c, d},
                                      {{"thickness", "0.06"}, {"mat", "5"},
                                       {"tone", "0"}, {"unsupported", "1"}});
        (void)f.g.set_param(s, "paint", "3"); // тент — красная фалу
        (void)f.g.set_param(s, "roof", "1");
    }
    f.save("assets/houses/city-stall.dfh");
}

/// КОЛОДЕЦ: каменное кольцо, столбы, мини-двускат.
static void forge_well() {
    Forge f;
    const float S = 1.7f;
    const glm::vec3 pts[5] = {{0, 0, 0}, {S, 0, 0}, {S, 0, S}, {0, 0, S}, {0, 0, 0}};
    for (int k = 0; k < 4; ++k) {
        const VertexId a = f.v(pts[k].x, 0.0f, pts[k].z);
        const VertexId b = f.v(pts[k + 1].x, 0.0f, pts[k + 1].z);
        (void)f.wall(a, b,
                     {{"height", "0.9"}, {"thickness", "0.28"}, {"mat", "3"},
                      {"tone", "1"}, {"fill", "3"}, {"wear", "0.55"}});
    }
    for (const float x : {0.2f, S - 0.2f}) {
        const VertexId lo = f.v(x, 0.0f, S * 0.5f);
        const VertexId hi = f.v(x, 2.2f, S * 0.5f);
        (void)f.beam(lo, hi, {{"radius", "0.08"}, {"mat", "0"}, {"tone", "2"}});
    }
    f.gable_roof(-0.15f, S * 0.5f - 0.8f, S + 0.15f, S * 0.5f + 0.8f, 2.1f, 0.7f,
                 "1", "2", "7", "0.5");
    f.save("assets/houses/city-well.dfh");
}

/// ДЛИННЫЙ ЗАЛ (Йоррваскр): 16x8, торцевой вход, огромные свесы.
static void forge_longhall() {
    Forge f;
    const float W = 16.0f;
    const float D = 8.0f;
    const float H = 3.6f;
    {
        const VertexId a = f.v(0.0f, 0.06f, 0.0f);
        const VertexId b = f.v(0.0f, 0.06f, D);
        const VertexId c = f.v(W, 0.06f, D);
        const VertexId d = f.v(W, 0.06f, 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.14"}, {"fill", "5"}, {"mat", "1"},
                         {"tone", "2"}, {"wear", "0.5"}});
    }
    f.frame_posts(0.0f, 0.0f, W, D, 0.0f, H, "0");
    const VertexId nw = f.v(0.0f, 0.0f, 0.0f);
    const VertexId ne = f.v(W, 0.0f, 0.0f);
    const VertexId se = f.v(W, 0.0f, D);
    const VertexId sw = f.v(0.0f, 0.0f, D);
    Params logw = {{"height", "3.6"}, {"thickness", "0.3"}, {"mat", "0"},
                   {"tone", "2"}, {"fill", "4"}, {"logends", "1"},
                   {"wear", "0.5"}};
    (void)f.wall(ne, nw, logw);
    (void)f.wall(nw, sw, logw);
    (void)f.wall(sw, se, logw);
    // Восточный торец — вход.
    (void)f.wall(se, ne,
                 {{"height", "3.6"}, {"thickness", "0.3"}, {"mat", "0"},
                  {"tone", "2"}, {"fill", "4"}, {"logends", "1"},
                  {"doors", "1"}, {"porch", "1"}, {"wear", "0.5"}});
    {
        // Дверь на восточной стене (вдоль Z).
        f.door_leaf_x(W, 0.0f, D * 0.5f);
    }
    // Потолок с балками.
    {
        const VertexId a = f.v(0.0f, H, 0.0f);
        const VertexId b = f.v(0.0f, H, D);
        const VertexId c = f.v(W, H, D);
        const VertexId d = f.v(W, H, 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.12"}, {"mat", "1"}, {"tone", "2"},
                         {"beams", "1"}});
    }
    f.gable_roof(0.0f, 0.0f, W, D, H, 2.6f, "1", "2", "7", "0.5");
    f.save("assets/houses/city-longhall.dfh");
}

/// ХРАМ: каменный зал с портиком.
static void forge_temple() {
    Forge f;
    const float W = 12.0f;
    const float D = 8.0f;
    const float H = 4.6f;
    {
        const VertexId a = f.v(0.0f, 0.05f, 0.0f);
        const VertexId b = f.v(0.0f, 0.05f, D);
        const VertexId c = f.v(W, 0.05f, D);
        const VertexId d = f.v(W, 0.05f, 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.15"}, {"mat", "3"}, {"tone", "1"},
                         {"wear", "0.35"}});
    }
    const VertexId nw = f.v(0.0f, 0.0f, 0.0f);
    const VertexId ne = f.v(W, 0.0f, 0.0f);
    const VertexId se = f.v(W, 0.0f, D);
    const VertexId sw = f.v(0.0f, 0.0f, D);
    Params st = {{"height", "4.6"}, {"thickness", "0.4"}, {"mat", "3"},
                 {"tone", "1"}, {"fill", "3"}, {"windows", "2"},
                 {"plinth", "1"}, {"wear", "0.4"}};
    (void)f.wall(ne, nw, st);
    (void)f.wall(se, ne, st);
    (void)f.wall(nw, sw, st);
    (void)f.wall(sw, se,
                 {{"height", "4.6"}, {"thickness", "0.4"}, {"mat", "3"},
                  {"tone", "1"}, {"fill", "3"}, {"doors", "1"}, {"porch", "1"},
                  {"wear", "0.4"}});
    f.door_leaf(W * 0.5f, 0.0f, D);
    // Портик: 4 колонны + плита-навес.
    for (int k = 0; k < 4; ++k) {
        const float x = W * 0.5f - 2.7f + 1.8f * static_cast<float>(k);
        const VertexId lo = f.v(x, 0.0f, D + 2.2f);
        const VertexId hi = f.v(x, 3.4f, D + 2.2f);
        (void)f.beam(lo, hi,
                     {{"radius", "0.22"}, {"sides", "8"}, {"mat", "3"},
                      {"tone", "1"}});
    }
    {
        const VertexId a = f.v(W * 0.5f - 3.3f, H - 0.2f, D - 0.2f);
        const VertexId b = f.v(W * 0.5f + 3.3f, H - 0.2f, D - 0.2f);
        const VertexId c = f.v(W * 0.5f + 3.3f, 3.35f, D + 2.6f);
        const VertexId d = f.v(W * 0.5f - 3.3f, 3.35f, D + 2.6f);
        const ElementId s = f.contour({a, b, c, d},
                                      {{"thickness", "0.15"}, {"mat", "1"},
                                       {"tone", "2"}, {"fill", "7"},
                                       {"wear", "0.35"}});
        (void)f.g.set_param(s, "roof", "1");
        (void)f.g.set_param(s, "unsupported", "1");
    }
    // НОРДСКАЯ ПАЛИТРА (приёмка №2: красная черепица — «чужеродно сильнее
    // всего»): тёмный гонт, как у всего города.
    f.gable_roof(0.0f, 0.0f, W, D, H, 2.2f, "1", "2", "7", "0.35");
    f.save("assets/houses/city-temple.dfh");
}

/// ЗАМОК (Драконий Предел, упрощение): зал 20x12 камень+фахверк, два крыла,
/// парадный подиум с двумя маршами, высокий двускат.
static void forge_keep() {
    Forge f;
    const float W = 20.0f;
    const float D = 12.0f;
    const float H1 = 4.2f;
    const float H2 = 8.0f;
    // Подиум-плаза перед входом (юг), два марша.
    {
        const VertexId a = f.v(3.0f, 0.02f, D + 0.0f);
        const VertexId b = f.v(3.0f, 0.02f, D + 6.0f);
        const VertexId c = f.v(W - 3.0f, 0.02f, D + 6.0f);
        const VertexId d = f.v(W - 3.0f, 0.02f, D + 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.2"}, {"mat", "3"}, {"tone", "1"},
                         {"wear", "0.35"}});
    }
    for (const float x0 : {5.0f, W - 9.0f}) {
        const VertexId a = f.v(x0, -3.0f, D + 12.0f);
        const VertexId b = f.v(x0 + 4.0f, -3.0f, D + 12.0f);
        const VertexId c = f.v(x0 + 4.0f, 0.0f, D + 6.0f);
        const VertexId d = f.v(x0, 0.0f, D + 6.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.15"}, {"fill", "6"}, {"open", "2"},
                         {"mat", "3"}, {"tone", "1"}, {"wear", "0.4"},
                         {"unsupported", "1"}});
    }
    // Зал: низ камень (эт.1), верх фахверк (эт.2).
    const auto ring = [&](float y, float h, bool stone, bool door_south) {
        char hb[16];
        std::snprintf(hb, sizeof(hb), "%.2f", h);
        const VertexId nw = f.v(0.0f, y, 0.0f);
        const VertexId ne = f.v(W, y, 0.0f);
        const VertexId se = f.v(W, y, D);
        const VertexId sw = f.v(0.0f, y, D);
        Params wallp =
            stone ? Params{{"height", hb}, {"thickness", "0.5"}, {"mat", "3"},
                           {"tone", "1"}, {"fill", "3"}, {"windows", "2"},
                           {"plinth", "1"}, {"wear", "0.35"}}
                  : Params{{"height", hb}, {"thickness", "0.35"}, {"mat", "5"},
                           {"tone", "0"}, {"clad", "1"}, {"windows", "3"},
                           {"shutters", "1"}, {"wear", "0.3"}};
        (void)f.wall(ne, nw, wallp);
        (void)f.wall(se, ne, wallp);
        (void)f.wall(nw, sw, wallp);
        if (door_south) {
            Params dp = stone ? Params{{"height", hb}, {"thickness", "0.5"},
                                       {"mat", "3"}, {"tone", "1"}, {"fill", "3"},
                                       {"doors", "1"}, {"porch", "1"},
                                       {"wear", "0.35"}}
                              : wallp;
            (void)f.wall(sw, se, dp);
        } else {
            (void)f.wall(sw, se, wallp);
        }
    };
    ring(0.0f, H1, true, true);
    ring(H1, H2 - H1, false, false);
    f.door_leaf(W * 0.5f, 0.0f, D);
    // Межэтажный пол с балками.
    {
        const VertexId a = f.v(0.0f, H1, 0.0f);
        const VertexId b = f.v(0.0f, H1, D);
        const VertexId c = f.v(W, H1, D);
        const VertexId d = f.v(W, H1, 0.0f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.15"}, {"mat", "1"}, {"tone", "1"},
                         {"beams", "1"}});
    }
    // Внутренняя лестница на второй этаж.
    {
        const VertexId a = f.v(1.0f, 0.0f, 9.5f);
        const VertexId b = f.v(3.2f, 0.0f, 9.5f);
        const VertexId c = f.v(3.2f, H1, 3.4f);
        const VertexId d = f.v(1.0f, H1, 3.4f);
        (void)f.contour({a, b, c, d},
                        {{"thickness", "0.12"}, {"fill", "6"}, {"open", "1"},
                         {"mat", "1"}, {"tone", "1"}, {"wear", "0.3"}});
    }
    f.frame_posts(0.0f, 0.0f, W, D, 0.0f, H2, "3");
    // Крылья по бокам, ниже зала.
    // Крылья отодвинуты от зала (приёмка: крыша крыла резала щипец).
    for (const float wx : {-8.4f, W + 0.4f}) {
        const float x0 = wx;
        const float x1 = wx + 8.0f;
        const VertexId nw = f.v(x0, 0.0f, 1.5f);
        const VertexId ne = f.v(x1, 0.0f, 1.5f);
        const VertexId se = f.v(x1, 0.0f, D - 1.5f);
        const VertexId sw = f.v(x0, 0.0f, D - 1.5f);
        Params wing = {{"height", "3.6"}, {"thickness", "0.4"}, {"mat", "3"},
                       {"tone", "1"}, {"fill", "3"}, {"windows", "1"},
                       {"plinth", "1"}, {"wear", "0.4"}};
        (void)f.wall(ne, nw, wing);
        (void)f.wall(se, ne, wing);
        (void)f.wall(nw, sw, wing);
        (void)f.wall(sw, se, wing);
        f.gable_roof(x0, 1.5f, x1, D - 1.5f, 3.6f, 1.8f, "1", "1", "7", "0.45");
    }
    f.gable_roof(0.0f, 0.0f, W, D, H2, 3.6f, "1", "1", "7", "0.35");
    f.save("assets/houses/city-keep.dfh");
}

} // namespace

int main() {
    forge_log();
    forge_frame();
    forge_stone();
    forge_l_house();
    forge_u_house();
    forge_tower();
    forge_tower_tall();
    forge_tree_ring();
    forge_wall12();
    forge_gate();
    forge_street_stairs();
    forge_street_stairs6();
    forge_plaza("assets/houses/city-plaza12.dfh", 12.0f, 12.0f);
    forge_plaza("assets/houses/city-plaza20.dfh", 20.0f, 20.0f);
    forge_bridge();
    forge_small_house();
    forge_shop();
    forge_stall();
    forge_well();
    forge_longhall();
    forge_temple();
    forge_keep();
    return 0;
}
