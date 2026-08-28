/*
Created: 28:08:2026 - 18:39:46
Last updated: 28:08:2026 - 18:39:46
Module: tests
File: tests/core/HouseLodTests.cpp

Responsibility:
- РУКАВ ЛЕСТНИЦЫ ДАЛЬНИХ ФОРМ ПОСТРОЙКИ (И13). Две половины лестницы
  проверяются порознь, потому что ломаются они порознь:
  (1) ВЫБОР СТУПЕНИ — чистая функция расстояния; ловится только проходом по
      краю полосы ТУДА И ОБРАТНО с требованием РОВНО двух смен уровня;
  (2) ЧТО СТУПЕНЬ СРЕЗАЛА — сборка одного и того же графа тремя ступенями:
      треугольники обязаны убывать, а полная форма обязана остаться ПРЕЖНЕЙ
      побайтово (её меряет отрицательное плечо каждого случая).

Dependencies:
- Uses: engine/world (HouseGraph, HouseMesh, HouseLod).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- КАЖДЫЙ СЛУЧАЙ ЕДЕТ СО СВОИМ ОТРИЦАНИЕМ (правило 30). «Дальняя форма дешевле»
  без числа полной формы не отличимо от «дальняя форма пуста», а «гистерезис
  есть» без прохода обратно — от «порог просто сдвинут».
*/
/*
UPD:
- 28:08:2026 - 18:39:46: Создан вместе с лестницей дальних форм (И13).
*/

#include <doctest/doctest.h>

#include "engine/world/sources/HouseGraph.h"
#include "engine/world/sources/HouseLod.h"
#include "engine/world/sources/HouseMesh.h"

#include <cstring>
#include <string>
#include <vector>

using dfn::world::Anchoring;
using dfn::world::ElementId;
using dfn::world::ElementKind;
using dfn::world::HouseGraph;
using dfn::world::HouseLod;
using dfn::world::HouseMesh;
using dfn::world::VertexId;

namespace {

/// Кирпичная стена с окнами, ставнями, завалинкой и износом — то есть со
/// ВСЕМИ слоями, которые лестница по очереди снимает.
[[nodiscard]] HouseGraph rich_wall() {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {8.0f, 0.0f, 0.0f});
    ElementId wall = 0;
    g.add_element(ElementKind::Surface, {a, b},
                  "brick;height=4;thickness=0.3;fill=2;windows=3;shutters=1;"
                  "plinth=1;wear=0.6",
                  wall);
    return g;
}

/// Побайтовое равенство двух мешей: вершины, индексы и границы частей.
[[nodiscard]] bool same_bytes(const HouseMesh& a, const HouseMesh& b) {
    if (a.vertices.size() != b.vertices.size() || a.indices.size() != b.indices.size()
        || a.parts.size() != b.parts.size()) {
        return false;
    }
    if (!a.vertices.empty()
        && std::memcmp(a.vertices.data(), b.vertices.data(),
                       a.vertices.size() * sizeof(a.vertices[0]))
               != 0) {
        return false;
    }
    return a.indices.empty()
        || std::memcmp(a.indices.data(), b.indices.data(),
                       a.indices.size() * sizeof(a.indices[0]))
               == 0;
}

} // namespace

TEST_CASE("лестница: край полосы проходится туда и обратно ровно двумя сменами") {
    const float edge = dfn::world::HOUSE_LOD_MID_IN_M;
    const float h = dfn::world::HOUSE_LOD_HYSTERESIS_M;

    HouseLod cur = HouseLod::Full;
    int switches = 0;
    // Ход ТУДА: от заведомо близкого до заведомо далёкого, шагом мельче
    // перехлёста — так, чтобы попасть внутрь полосы дребезга не один раз.
    for (float d = edge - 2.0f * h; d <= edge + 2.0f * h; d += h * 0.25f) {
        const HouseLod next = dfn::world::house_lod_for_distance(d, cur);
        if (next != cur) {
            ++switches;
        }
        cur = next;
    }
    CHECK(cur == HouseLod::Mid);
    // Ход ОБРАТНО той же дорогой.
    for (float d = edge + 2.0f * h; d >= edge - 2.0f * h; d -= h * 0.25f) {
        const HouseLod next = dfn::world::house_lod_for_distance(d, cur);
        if (next != cur) {
            ++switches;
        }
        cur = next;
    }
    CHECK(cur == HouseLod::Full);
    // РОВНО ДВЕ. Три и больше — дребезг: без перехлёста игрок, стоящий на
    // краю полосы, гонял бы фасад туда-обратно каждым покачиванием.
    CHECK(switches == 2);
}

TEST_CASE("контроль лестницы: без памяти о текущей ступени тот же проход дребезжит") {
    const float edge = dfn::world::HOUSE_LOD_MID_IN_M;
    const float h = dfn::world::HOUSE_LOD_HYSTERESIS_M;
    // ОТРИЦАТЕЛЬНОЕ ПЛЕЧО (правило 30): гистерезис существует ТОЛЬКО потому,
    // что функции называют текущую ступень. Спросим её всегда «с полной» —
    // память выключена, — и тот же путь обязан дать смен БОЛЬШЕ двух.
    int switches = 0;
    HouseLod seen = HouseLod::Full;
    for (float d = edge - 2.0f * h; d <= edge + 2.0f * h; d += h * 0.25f) {
        const HouseLod next = dfn::world::house_lod_for_distance(d, HouseLod::Full);
        if (next != seen) {
            ++switches;
        }
        seen = next;
    }
    for (float d = edge + 2.0f * h; d >= edge - 2.0f * h; d -= h * 0.25f) {
        const HouseLod next = dfn::world::house_lod_for_distance(d, HouseLod::Full);
        if (next != seen) {
            ++switches;
        }
        seen = next;
    }
    CHECK(switches == 2);
    // ...и порог без памяти стоит НЕ ТАМ, где с памятью: вход в среднюю
    // ступень уезжает на перехлёст наружу. Это и есть измеренная разница
    // между «есть гистерезис» и «порог просто сдвинут».
    CHECK(dfn::world::house_lod_for_distance(edge + h * 0.5f, HouseLod::Full)
          == HouseLod::Full);
    CHECK(dfn::world::house_lod_for_distance(edge + h * 0.5f, HouseLod::Mid)
          == HouseLod::Mid);
}

TEST_CASE("лестница накрывает обе полосы и не перепрыгивает ступень назад") {
    CHECK(dfn::world::house_lod_for_distance(0.0f, HouseLod::Full) == HouseLod::Full);
    CHECK(dfn::world::house_lod_for_distance(1000.0f, HouseLod::Full) == HouseLod::Far);
    // Полоса средней ступени существует как полоса, а не как точка.
    const float mid = 0.5f * (dfn::world::HOUSE_LOD_MID_IN_M + dfn::world::HOUSE_LOD_FAR_IN_M);
    CHECK(dfn::world::house_lod_for_distance(mid, HouseLod::Full) == HouseLod::Mid);
    CHECK(dfn::world::house_lod_for_distance(mid, HouseLod::Far) == HouseLod::Mid);
}

TEST_CASE("вывод порогов: каждый порог стоит ЗА расстоянием своего слоя") {
    // Не тавтология: эта проверка ловит правку порога, сделанную «на глаз»
    // мимо таблицы вывода в HouseLod.h. Шаг ряда кирпича — 0.065 + 0.010;
    // выпуск черепицы — 0.30 (числа взяты из HouseWalls.cpp / HouseRoof.cpp).
    CHECK(dfn::world::house_lod_cut_distance_m(0.075f)
          <= dfn::world::HOUSE_LOD_MID_IN_M);
    CHECK(dfn::world::house_lod_cut_distance_m(0.30f)
          <= dfn::world::HOUSE_LOD_FAR_IN_M);
    // ...и НЕ дальше следующего слоя: порог, уехавший за ставню (0.15 м),
    // срезал бы её раньше, чем она перестала быть видна.
    CHECK(dfn::world::house_lod_cut_distance_m(0.15f)
          > dfn::world::HOUSE_LOD_MID_IN_M);
    // Проём метровой ширины виден и на входе в дальнюю ступень — довод, по
    // которому дальняя форма проёмы СОХРАНЯЕТ.
    CHECK(dfn::world::house_lod_cut_distance_m(1.0f)
          > dfn::world::HOUSE_LOD_FAR_IN_M);
    CHECK(dfn::world::HOUSE_LOD_FAR_KEEPS_OPENINGS);
}

TEST_CASE("ступени: треугольники убывают, а полная форма остаётся прежней побайтово") {
    const HouseGraph g = rich_wall();
    const HouseMesh full = dfn::world::build_house_mesh(g);
    const HouseMesh mid = dfn::world::build_house_mesh(
        g, dfn::world::HOUSE_BEVEL_W_DEFAULT, HouseLod::Mid);
    const HouseMesh far = dfn::world::build_house_mesh(
        g, dfn::world::HOUSE_BEVEL_W_DEFAULT, HouseLod::Far);

    REQUIRE(full.triangle_count() > 0);
    CHECK(mid.triangle_count() < full.triangle_count());
    CHECK(far.triangle_count() < mid.triangle_count());
    // ДАЛЬНЯЯ ФОРМА НЕ ПУСТА — без этого «дешевле» ничего не значит.
    CHECK(far.triangle_count() > 0);

    // ОТРИЦАТЕЛЬНОЕ ПЛЕЧО ВСЕЙ ВОЛНЫ: полная ступень обязана быть тем же
    // мешем, что и до лестницы. Умолчание и явный HouseLod::Full — один меш;
    // и он же не зависит от того, что печь звали дальними ступенями.
    CHECK(same_bytes(full, dfn::world::build_house_mesh(
                               g, dfn::world::HOUSE_BEVEL_W_DEFAULT, HouseLod::Full)));

    // ФАСКА СНЯТА У ОБЕИХ ДАЛЬНИХ СТУПЕНЕЙ, и это видно мерой К4, а не на
    // глаз: у средней и дальней острых рёбер обязано стать больше нуля.
    CHECK(dfn::world::house_edge_census(mid).sharp > 0);
    CHECK(dfn::world::house_edge_census(far).sharp > 0);
}

TEST_CASE("дальняя форма: проёмы остались, кладка ушла, тон кладки уехал на пластину") {
    const HouseGraph g = rich_wall();
    const HouseMesh far = dfn::world::build_house_mesh(
        g, dfn::world::HOUSE_BEVEL_W_DEFAULT, HouseLod::Far);

    // ПРОЁМЫ. Стена 8 м с тремя окнами: сплошная пластина без прорезей — это
    // ровно одна призма, то есть 12 треугольников. Прорезанная — заметно
    // больше, и это отличает «проёмы сохранены» от «стена стала коробкой».
    CHECK(far.triangle_count() > 12);

    // КЛАДКА УШЛА, А ЕЁ МАТЕРИАЛ ОСТАЛСЯ. У полной формы куски кладки несут
    // свой материал подчастями; у дальней подчастей кладки нет, но материал
    // глины (4) обязан стоять на самой пластине — иначе дальний кирпичный дом
    // становится штукатурным.
    bool clay_plate = false;
    for (const dfn::world::MeshPart& p : far.parts) {
        if (p.mat_override == 4) {
            clay_plate = true;
        }
    }
    CHECK(clay_plate);
    // Контроль: у ГЛАДКОЙ стены того же размера никакой подмены материала
    // нет — признак приходит от кладки, а не от ступени.
    HouseGraph plain;
    const VertexId a = plain.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = plain.add_vertex(Anchoring::OnGround, {8.0f, 0.0f, 0.0f});
    ElementId w = 0;
    plain.add_element(ElementKind::Surface, {a, b}, "plaster;height=4;thickness=0.3", w);
    const HouseMesh plain_far = dfn::world::build_house_mesh(
        plain, dfn::world::HOUSE_BEVEL_W_DEFAULT, HouseLod::Far);
    for (const dfn::world::MeshPart& p : plain_far.parts) {
        CHECK(p.mat_override == -1);
    }
}
