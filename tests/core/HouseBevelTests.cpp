/*
Module: tests
File: tests/core/HouseBevelTests.cpp

Responsibility:
- ПЛЕЧО НЕНУЛЕВОЙ ДОЗЫ ФАСКИ (дефект 3 ТЗ материалов, критерий К4). Соседний
  test_house_mesh держит плечо НУЛЕВОЙ дозы: все его 38 сборок зовутся с
  0.0f и меряют прежнюю острую геометрию числами. Здесь — что ширина выше
  нуля меняет, чего не меняет и чем это оплачено.

Key items:
- К4 на коробке: 0 % до, 100 % после (и контроль — коробка при нулевой дозе).
- Габарит: фаска режет ВНУТРЬ и не двигает ящик тела.
- Полотно тоньше порога фаски не получает (с положительным контролем).
- Ряды кладки фаски не носят, а рама того же проёма — носит.
- Доза ниже порога строит прежний меш ПОБАЙТОВО.

Dependencies:
- Uses: engine/world (HouseGraph, HouseMesh, house_edge_census).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- КАЖДЫЙ СЛУЧАЙ ЕДЕТ СО СВОИМ ОТРИЦАНИЕМ (правило 30). «К4 = 100 %» без
  «К4 = 0 % при нулевой дозе» не отличает работающую фаску от прибора,
  который всегда зелен; «полотно без фаски» без «брусок с фаской» не отличает
  порог от выключенной фаски вовсе.
*/

#include <doctest/doctest.h>

#include "engine/world/sources/HouseGraph.h"
#include "engine/world/sources/HouseMesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/geometric.hpp>
#include <vector>

using dfn::world::Anchoring;
using dfn::world::ElementId;
using dfn::world::ElementKind;
using dfn::world::HouseGraph;
using dfn::world::HouseMesh;
using dfn::world::VertexId;

namespace {

/// Плита-коробка: поверхность из двух якорей, выдавленная в толщину.
[[nodiscard]] HouseGraph slab(float height, float thickness, float span = 2.0f) {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {span, 0.0f, 0.0f});
    ElementId wall = 0;
    const std::string style = "oak;height=" + std::to_string(height)
                            + ";thickness=" + std::to_string(thickness);
    g.add_element(ElementKind::Surface, {a, b}, style, wall);
    return g;
}

struct Box {
    glm::vec3 lo{1e9f};
    glm::vec3 hi{-1e9f};
};

[[nodiscard]] Box bbox_of(const HouseMesh& m) {
    Box b;
    for (const dfn::world::HouseVertex& v : m.vertices) {
        b.lo = glm::min(b.lo, v.pos);
        b.hi = glm::max(b.hi, v.pos);
    }
    return b;
}

} // namespace

TEST_CASE("К4: коробка при нулевой дозе острая целиком, при штатной — вся с фаской") {
    const HouseGraph g = slab(2.5f, 0.20f);

    const HouseMesh sharp = dfn::world::build_house_mesh(g, 0.0f);
    const dfn::world::HouseEdgeCensus c0 = dfn::world::house_edge_census(sharp);
    // КОНТРОЛЬ (правило 30). Он же — измеренный дефект 3 ТЗ: ни одного
    // ребра с фаской, и излом ровно прямой.
    REQUIRE(c0.convex > 0);
    CHECK(c0.share() == doctest::Approx(0.0f));
    CHECK(c0.worst_deg == doctest::Approx(90.0f).epsilon(0.001));

    const HouseMesh cut = dfn::world::build_house_mesh(g);
    const dfn::world::HouseEdgeCensus c1 = dfn::world::house_edge_census(cut);
    CHECK(c1.sharp == 0);
    CHECK(c1.share() == doctest::Approx(1.0f));
    // Прямой угол, срезанный фаской, распадается ровно на два по 45°.
    CHECK(c1.worst_deg == doctest::Approx(45.0f).epsilon(0.001));
    // И это стоит денег: цена названа числом здесь же, чтобы её нельзя было
    // потерять из виду, правя фаску.
    CHECK(cut.triangle_count() == 60);
    CHECK(sharp.triangle_count() == 12);
}

TEST_CASE("габарит: фаска режет внутрь и не двигает ящик тела") {
    const HouseGraph g = slab(2.5f, 0.20f);
    const Box a = bbox_of(dfn::world::build_house_mesh(g, 0.0f));
    const Box b = bbox_of(dfn::world::build_house_mesh(g));
    // Ни одной десятой миллиметра: габарит стоит колонкой в перечне
    // объектов, и раскладка садит по нему предметы на столешницы.
    CHECK(glm::length(a.lo - b.lo) < 1e-4f);
    CHECK(glm::length(a.hi - b.hi) < 1e-4f);
}

TEST_CASE("многогранник: неострый угол не срезается — ни лишних граней, ни съехавшего габарита") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::Free, {0.0f, 1.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a, b}, "oak;form=round;radius=0.12", post).ok);

    const HouseMesh sharp = dfn::world::build_house_mesh(g, 0.0f);
    const HouseMesh cut = dfn::world::build_house_mesh(g);
    const Box s = bbox_of(sharp);
    const Box c = bbox_of(cut);
    CHECK(glm::length(s.lo - c.lo) < 1e-4f);
    CHECK(glm::length(s.hi - c.hi) < 1e-4f);

    // У восьмигранника продольные рёбра ломаются на 45° и БЕЗ фаски: мера К4
    // их и так засчитывает, а срезать их значило бы сделать
    // шестнадцатигранник и подвинуть силуэт внутрь на ширину фаски.
    // КОНТРОЛЬ: квадратный брус того же радиуса корни срезает — значит
    // отказ выше про УГОЛ, а не про то, что фаска не дошла до прямых.
    HouseGraph q;
    const VertexId qa = q.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId qb = q.add_vertex(Anchoring::Free, {0.0f, 1.0f, 0.0f});
    ElementId bar = 0;
    REQUIRE(q.add_element(ElementKind::Line, {qa, qb}, "oak;form=square;radius=0.12", bar).ok);
    const HouseMesh qs = dfn::world::build_house_mesh(q, 0.0f);
    const HouseMesh qc = dfn::world::build_house_mesh(q);
    CHECK(qc.triangle_count() > qs.triangle_count() * 3);
    CHECK(dfn::world::house_edge_census(qc).sharp == 0);
}

TEST_CASE("полотно тоньше порога фаски не получает, брусок — получает") {
    // Ковёр в 12 мм, полотно картины в 16, переплёт книги в 8: у полотна
    // кромка, а не фаска, и фаска в 2-3 мм стоила бы вчетверо больше
    // треугольников за полоску, которой с метра четыре пикселя.
    const HouseMesh sheet = dfn::world::build_house_mesh(slab(1.4f, 0.006f));
    CHECK(dfn::world::house_edge_census(sheet).share() == doctest::Approx(0.0f));
    CHECK(sheet.triangle_count() == 12);

    // КОНТРОЛЬ: та же плита толще порога — фаску получает. Без него «нет
    // фаски» доказывало бы только, что фаска не работает вовсе.
    const HouseMesh board = dfn::world::build_house_mesh(slab(1.4f, 0.040f));
    CHECK(dfn::world::house_edge_census(board).sharp == 0);
    CHECK(board.triangle_count() == 60);
}

TEST_CASE("ряды кладки фаски не носят, а стена без кладки — носит") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    ElementId wall = 0;
    REQUIRE(g.add_element(ElementKind::Surface, {a, b},
                          "stone;height=2.5;thickness=0.3;fill=3", wall)
                .ok);
    const HouseMesh sharp = dfn::world::build_house_mesh(g, 0.0f);
    const HouseMesh cut = dfn::world::build_house_mesh(g);
    // Кладка — разделка ПОВЕРХНОСТИ, а не тела: у куска свободен только обвод
    // лица. Прибавка меньше половины — плита стены под кладкой её получила,
    // сами куски нет.
    CHECK(cut.triangle_count() < sharp.triangle_count() * 3 / 2);

    // КОНТРОЛЬ: та же стена БЕЗ кладки платит полную цену. Без него
    // «кладка дешева» доказывало бы, что фаска выключена на всей стене.
    HouseGraph p;
    const VertexId pa = p.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId pb = p.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    ElementId plain = 0;
    REQUIRE(p.add_element(ElementKind::Surface, {pa, pb}, "stone;height=2.5;thickness=0.3",
                          plain)
                .ok);
    const HouseMesh ps = dfn::world::build_house_mesh(p, 0.0f);
    const HouseMesh pc = dfn::world::build_house_mesh(p);
    CHECK(pc.triangle_count() >= ps.triangle_count() * 4);
}

TEST_CASE("доза ниже порога строит прежний меш ПОБАЙТОВО, а штатная — другой") {
    const HouseGraph g = slab(2.5f, 0.20f);
    const HouseMesh zero = dfn::world::build_house_mesh(g, 0.0f);
    const HouseMesh tiny = dfn::world::build_house_mesh(g, 0.0002f);
    REQUIRE(zero.vertices.size() == tiny.vertices.size());
    CHECK(std::memcmp(zero.vertices.data(), tiny.vertices.data(),
                      zero.vertices.size() * sizeof(dfn::world::HouseVertex))
          == 0);
    CHECK(zero.indices == tiny.indices);

    // ОТКЛИК НА ДОЗУ (правило 48): без него равенство выше доказывало бы
    // только, что параметр никуда не проведён.
    const HouseMesh full = dfn::world::build_house_mesh(g, 0.010f);
    CHECK(full.vertices.size() != zero.vertices.size());
}

TEST_CASE("коллайдер фаски не знает: физика бит-в-бит при любой ширине") {
    const HouseGraph g = slab(2.5f, 0.20f);
    const HouseMesh zero = dfn::world::build_house_mesh(g, 0.0f);
    const HouseMesh full = dfn::world::build_house_mesh(g, 0.010f);
    REQUIRE(zero.convex.size() == full.convex.size());
    for (std::size_t i = 0; i < zero.convex.size(); ++i) {
        REQUIRE(zero.convex[i].points.size() == full.convex[i].points.size());
        for (std::size_t k = 0; k < zero.convex[i].points.size(); ++k) {
            CHECK(glm::length(zero.convex[i].points[k] - full.convex[i].points[k]) < 1e-6f);
        }
    }
}

TEST_CASE("фаска детерминирована: две сборки одного графа совпадают побайтово") {
    const HouseGraph g = slab(2.5f, 0.20f);
    const HouseMesh a = dfn::world::build_house_mesh(g);
    const HouseMesh b = dfn::world::build_house_mesh(g);
    REQUIRE(a.vertices.size() == b.vertices.size());
    CHECK(std::memcmp(a.vertices.data(), b.vertices.data(),
                      a.vertices.size() * sizeof(dfn::world::HouseVertex))
          == 0);
    CHECK(a.indices == b.indices);
}
