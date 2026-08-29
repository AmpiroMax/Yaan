/*
Module: tests
File: tests/core/HouseMaterialTests.cpp

Responsibility:
- РЕЦЕПТ ДОМА НАЗЫВАЕТ ВЕЩЕСТВО ИМЕНЕМ (волна 3 зоны МАТЕРИАЛЫ). Инвентаризация
  назвала это невозможным по устройству: `PartMaterial` — одиннадцать ИМЁН —
  живёт в engine/render, а рецепты читает engine/world, которому знать про
  render запрещено, и потому рецепт физически не мог сказать «кирпич» — он
  говорил `mat=4`. Реестр в engine/core снял запрет; здесь проверяется, что
  снял.
- И ОТКАЗЫ ВСЛУХ: неизвестное имя вещества и номер колонки вне листа. Второе —
  тот самый дефект `mat=17 % 9 = 8`, который молча ЗАСТЕКЛЯЛ стену.

Key items:
- `material = brick-red` даёт ту же клетку, что `mat=4, tone=1` (кадр не
  меняется от перехода на имя) и вдобавок НЕСЁТ ИМЯ дальше;
- неизвестное имя — находка, а не подстановка;
- `mat=17` — находка, а остаток прежний (волна обещала не двигать пиксели);
- вещество без зерна (полотно) имя несёт, а клетку не трогает.

Dependencies:
- Uses: engine/world (HouseGraph, HouseMesh: house_part_tile),
  engine/core/materials (реестр), doctest.
- Used by: tests/core.cmake (core_house_material).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; дизайн зоны — docs/design/MATERIALS.md.
- ГЛАВНОЕ УТВЕРЖДЕНИЕ ЗДЕСЬ — «ИМЯ И КООРДИНАТЫ ДАЮТ ОДНО И ТО ЖЕ». Оно и есть
  доказательство, что переход рецептов на имена не перекрасит ни одного дома:
  разойдись эти два пути, миграция полки стала бы перекраской.
*/

#include <doctest/doctest.h>

#include "engine/core/materials/sources/MaterialRegistry.h"
#include "engine/world/sources/HouseGraph.h"
#include "engine/world/sources/HouseMesh.h"

#include <string>
#include <vector>

using namespace dfn::world;

namespace {

/// Одна поверхность с заданными параметрами — минимум, на котором
/// house_part_tile отвечает.
struct OneWall {
    HouseGraph g;
    ElementId id = NO_ELEMENT;

    OneWall() {
        const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
        const VertexId b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
        const auto r = g.add_element(ElementKind::Surface, {a, b},
                                     "plank;height=2.6;thickness=0.2", id);
        REQUIRE(r.ok == true);
    }
    void param(const char* key, const char* value) {
        const auto r = g.set_param(id, key, value);
        REQUIRE(r.ok == true);
    }
    [[nodiscard]] HousePartTile tile(std::vector<ParamIssue>* issues = nullptr) const {
        const Element* e = g.element(id);
        REQUIRE(e != nullptr);
        return house_part_tile(g, *e, -1, -1, issues);
    }
};

} // namespace

TEST_CASE("рецепт называет вещество именем, и клетка та же, что у координат") {
    // ГЛАВНОЕ УТВЕРЖДЕНИЕ ВОЛНЫ ПРО РЕЦЕПТЫ. «Кирпич красный» обязан дать РОВНО
    // ту клетку, которую сегодня пишут числами, — иначе перевод 194 рецептов
    // на имена был бы не переименованием, а перекраской, и первый же дом
    // сменил бы вид.
    OneWall by_numbers;
    by_numbers.param("mat", "4");  // FiredClay
    by_numbers.param("tone", "1"); // Mid
    const HousePartTile numbers = by_numbers.tile();

    OneWall by_name;
    by_name.param("material", "brick-red");
    const HousePartTile named = by_name.tile();

    CHECK(named.surface == numbers.surface);
    CHECK(named.tone == numbers.tone);
    // ...И ИМЯ ЕДЕТ ДАЛЬШЕ. Без него всё это было бы переводом имени в
    // координаты и обратно: клетка одинакова, а сказать «из чего сделано»
    // по-прежнему нельзя.
    CHECK(named.material == "brick-red");
    CHECK(numbers.material.empty());

    // Контроль (правило 45): ДРУГОЕ вещество даёт ДРУГУЮ клетку — иначе
    // проверка выше прошла бы у реестра, отвечающего всем одинаково.
    OneWall other;
    other.param("material", "granite");
    const HousePartTile stone = other.tile();
    CHECK(stone.surface != named.surface);
}

TEST_CASE("неизвестное имя вещества — находка, а не подстановка") {
    // ПОЛОВИНА СМЫСЛА ИМЁН. `mat=17` молча становился глухим окном; «material
    // = кирпыч» обязан сказать о себе, иначе имя не лучше числа.
    OneWall wall;
    wall.param("material", "кирпыч");
    std::vector<ParamIssue> issues;
    const HousePartTile t = wall.tile(&issues);
    CHECK_FALSE(issues.empty());
    // Вещество НЕ названо: имя, которого реестр не знает, не имеет права
    // поехать дальше и притвориться настоящим.
    CHECK(t.material.empty());
    // ...а клетка осталась умолчанием рода элемента (поверхность —
    // штукатурка светлая): кадр от опечатки не съезжает.
    CHECK(t.surface == 5u);
    CHECK(t.tone == 0u);

    // Контроль (правило 30): известное имя находок НЕ даёт.
    OneWall good;
    good.param("material", "brick-red");
    std::vector<ParamIssue> none;
    (void)good.tile(&none);
    CHECK(none.empty());
}

TEST_CASE("mat=17 даёт сообщение, а не тихое остекление") {
    // ДЕФЕКТ, НАЗВАННЫЙ ИНВЕНТАРИЗАЦИЕЙ ЧИСЛОМ: `17 % 9 = 8` = Pane, то есть
    // опечатка застекляет стену без единого сообщения.
    //
    // ОСТАТОК ПРИ ЭТОМ ОСТАВЛЕН НАРОЧНО, и это не полумера: волна обещала не
    // двигать пиксели, а всякий рецепт, который сегодня на остатке выехал,
    // сменил бы вид от «починки». Изменилось ровно то, что нужно было
    // изменить: теперь об этом говорят вслух.
    OneWall wall;
    wall.param("mat", "17");
    std::vector<ParamIssue> issues;
    const HousePartTile t = wall.tile(&issues);
    CHECK(t.surface == 8u); // прежнее поведение — бит в бит
    CHECK_FALSE(issues.empty());

    // Контроль (правило 45): номер В ПРЕДЕЛАХ листа находок не даёт — иначе
    // рукав ругался бы на весь сегодняшний город.
    OneWall ok;
    ok.param("mat", "8");
    std::vector<ParamIssue> quiet;
    const HousePartTile t2 = ok.tile(&quiet);
    CHECK(t2.surface == 8u);
    CHECK(quiet.empty());

    // Ряд — та же история и тот же порог: рядов четыре.
    OneWall row;
    row.param("tone", "9");
    std::vector<ParamIssue> row_issues;
    const HousePartTile t3 = row.tile(&row_issues);
    CHECK(t3.tone == 1u); // 9 % 4
    CHECK_FALSE(row_issues.empty());
}

TEST_CASE("вещество без зерна имя несёт, а клетку не выдумывает") {
    // У полотна, металла и пергамента ПЛИТКИ НЕТ, и это утверждение о них, а
    // не недоделка: у листа набора девять колонок, и ни одна из них не
    // полотно. Приписать такому веществу клетку значило бы соврать, что лён —
    // это тон штукатурки, — ровно то притворство, ради отмены которого волна и
    // затевалась.
    OneWall wall;
    wall.param("material", "linen");
    std::vector<ParamIssue> issues;
    const HousePartTile t = wall.tile(&issues);
    CHECK(issues.empty());       // это не ошибка
    CHECK(t.material == "linen"); // имя едет к рисовальщику: у него блик и оттенок
    CHECK(t.surface == 5u);       // клетка — умолчание рода элемента, не выдумка
}

TEST_CASE("координаты, названные ПОСЛЕ имени, остаются последним словом") {
    // ПОРЯДОК РАЗРЕШЕНИЯ НАЗВАН ЯВНО, потому что миграция пойдёт вперемешку:
    // часть рецепта уже на именах, часть ещё на числах. Числа перебивают имя
    // ПО КЛЕТКЕ (иначе правка одной стены требовала бы переписать весь
    // рецепт), но имя остаётся при куске — рисовальщик по-прежнему знает
    // вещество и его блик.
    OneWall wall;
    wall.param("material", "brick-red");
    wall.param("mat", "3"); // Stone — человек перебил клетку руками
    const HousePartTile t = wall.tile();
    CHECK(t.surface == 3u);
    CHECK(t.material == "brick-red");
}
