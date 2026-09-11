/*
Module: tests
File: tests/sim/NavPathTests.cpp

Responsibility:
- ПРИБОР ПУТИ ПО СЕТКЕ ПРОХОДИМОСТИ (NavGrid.cpp: A* + натяжение,
  NPC_NAVIGATION.md §3): на плоскости путь — две точки; вокруг стены —
  длиннее прямой и короче обхода по периметру, натяжение убирает точки
  (контроль — без натяжения точек столько же, сколько ячеек); без эрозии
  путь режет угол вплотную (контроль); на остров пути нет; на стенде
  interaction — от спавна к одинокой лавке, не касаясь стола.

Key items:
- a_straight_path_on_the_flat_is_two_points
- the_path_goes_around_the_wall_and_is_pulled_tight
- without_erosion_the_control_path_hugs_the_corner
- there_is_no_path_to_an_island
- the_interaction_stand_path_reaches_the_lone_bench_around_the_table

Dependencies:
- Uses: doctest, dfn_gameplay (NavGrid), dfn_world через
  tests/sim/fixtures/NavSceneSoup.h, реестр.
- Used by: ctest (sim_nav_path).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
#include "engine/core/config/sources/Constants.h"
#include "engine/gameplay/sources/NavGrid.h"
#include "tests/sim/fixtures/NavSceneSoup.h"

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

using namespace dfn;

namespace {

float flat_ground(void*, glm::vec2) { return 0.0f; }

struct Field {
    std::vector<gameplay::NavBox> boxes;
    gameplay::NavInput in;
    gameplay::NavGrid grid;
    gameplay::NavSearch search;
    std::string err;

    explicit Field(float span = 12.0f) {
        in.ground = &flat_ground;
        in.min_xz = {-0.5f * span, -0.5f * span};
        in.max_xz = {0.5f * span, 0.5f * span};
        in.agent = gameplay::nav_agent_from_registry();
    }
    void box(glm::vec3 c, glm::vec3 he, float yaw = 0.0f) { boxes.push_back({c, he, yaw}); }
    bool build() {
        in.boxes = boxes;
        return gameplay::nav_build(in, grid, &err);
    }
    bool path(glm::vec3 a, glm::vec3 b, gameplay::NavPath& out, bool pull = true) {
        return gameplay::nav_find_path(grid, search, a, b, out, pull);
    }
};

/// Наименьшее расстояние точек пути (и отрезков между ними, по выборке) до
/// прямоугольника ящика в плане.
float min_distance_to_box(const gameplay::NavPath& p, const gameplay::NavBox& b) {
    float best = 1.0e9f;
    auto dist = [&](glm::vec3 q) {
        const glm::vec2 d = glm::abs(glm::vec2{q.x - b.center.x, q.z - b.center.z})
                            - glm::vec2{b.half_extents.x, b.half_extents.z};
        return glm::length(glm::max(d, glm::vec2{0.0f}));
    };
    for (std::size_t i = 0; i < p.points.size(); ++i) {
        best = std::min(best, dist(p.points[i]));
        if (i + 1 < p.points.size()) {
            for (int k = 1; k < 20; ++k) {
                const float t = static_cast<float>(k) / 20.0f;
                best = std::min(best, dist(p.points[i] + (p.points[i + 1] - p.points[i]) * t));
            }
        }
    }
    return best;
}

} // namespace

TEST_CASE("a_straight_path_on_the_flat_is_two_points") {
    Field f;
    REQUIRE(f.build());
    gameplay::NavPath p;
    REQUIRE(f.path({-4.0f, 0.0f, -3.0f}, {4.0f, 0.0f, 3.0f}, p));
    MESSAGE("плоскость: точек " << p.points.size() << ", ячеек " << p.cells << ", раскрыто " << f.search.expanded
                                 << ", длина " << p.length_m() << " м против " << p.cells_length_m << " по ячейкам");
    CHECK(p.points.size() == 2);
    CHECK(p.length_m() == doctest::Approx(10.0f).epsilon(0.02));
    CHECK(p.points.front().x == doctest::Approx(-4.0f));
    CHECK(p.points.back().z == doctest::Approx(3.0f));
    // контроль без натяжения: столько точек, сколько ячеек
    gameplay::NavPath raw;
    REQUIRE(f.path({-4.0f, 0.0f, -3.0f}, {4.0f, 0.0f, 3.0f}, raw, false));
    CHECK(raw.points.size() == raw.cells);
    CHECK(raw.points.size() > 20);
}

TEST_CASE("the_path_goes_around_the_wall_and_is_pulled_tight") {
    Field f;
    const gameplay::NavBox wall{{0.0f, 1.0f, 0.0f}, {2.0f, 1.0f, 0.1f}, 0.0f};
    f.boxes.push_back(wall);
    REQUIRE(f.build());
    gameplay::NavPath p;
    REQUIRE(f.path({0.0f, 0.0f, -3.0f}, {0.0f, 0.0f, 3.0f}, p));
    const float straight = 6.0f;
    const float perimeter = 3.0f + 2.0f + 0.5f + 3.0f + 2.0f + 0.5f; // вдоль стены до края и обратно
    const float d = min_distance_to_box(p, wall);
    MESSAGE("вокруг стены: точек " << p.points.size() << " из " << p.cells << " ячеек, длина " << p.length_m()
                                   << " м (прямая " << straight << ", периметр " << perimeter << "), до стены " << d
                                   << " м, раскрыто " << f.search.expanded);
    CHECK(p.length_m() > straight);
    CHECK(p.length_m() < perimeter);
    CHECK(p.points.size() < p.cells / 2);
    CHECK(p.points.size() >= 3);
    // капсула не режет стену: центр пути не ближе радиуса к грани (по
    // построению порога эрозии — см. nav_build)
    CHECK(d >= f.grid.agent.radius);
}

TEST_CASE("without_erosion_the_control_path_hugs_the_corner") {
    Field f;
    const gameplay::NavBox wall{{0.0f, 1.0f, 0.0f}, {2.0f, 1.0f, 0.1f}, 0.0f};
    f.boxes.push_back(wall);
    f.in.agent.radius = 0.0f;
    REQUIRE(f.build());
    gameplay::NavPath p;
    REQUIRE(f.path({0.0f, 0.0f, -3.0f}, {0.0f, 0.0f, 3.0f}, p));
    const float d = min_distance_to_box(p, wall);
    // рука с радиусом — тот же вопрос
    Field g;
    g.boxes.push_back(wall);
    REQUIRE(g.build());
    gameplay::NavPath q;
    REQUIRE(g.path({0.0f, 0.0f, -3.0f}, {0.0f, 0.0f, 3.0f}, q));
    const float dr = min_distance_to_box(q, wall);
    MESSAGE("контроль без радиуса: до стены " << d << " м, длина " << p.length_m() << "; с радиусом " << dr << " м, длина "
                                             << q.length_m());
    // без эрозии путь идёт по соседней со стеной ячейке (полторы ячейки от
    // грани — капсула 0,35 прошла бы впритирку); с эрозией — не ближе радиуса
    CHECK(d < gameplay::nav_agent_from_registry().radius + 0.5f * f.grid.cell);
    CHECK(d < dr);
    CHECK(p.length_m() < q.length_m());
}

TEST_CASE("there_is_no_path_to_an_island") {
    Field f;
    f.box({3.0f, 0.4f, 3.0f}, {1.0f, 0.4f, 1.0f}); // плита 0,8 — не шаг
    REQUIRE(f.build());
    gameplay::NavPath p;
    CHECK(!f.path({-3.0f, 0.0f, -3.0f}, {3.0f, 0.8f, 3.0f}, p));
    CHECK(p.points.empty());
    // контроль: та же цель на земле рядом с плитой достижима
    CHECK(f.path({-3.0f, 0.0f, -3.0f}, {3.0f, 0.0f, 0.5f}, p));
}

TEST_CASE("the_interaction_stand_path_reaches_the_lone_bench_around_the_table") {
    navtest::NavSceneSoup s;
    if (!navtest::load_scene_soup("assets/scenes/stands/interaction.scene", s)) {
        return;
    }
    gameplay::NavInput in = navtest::soup_input(s);
    gameplay::NavGrid grid;
    std::string err;
    REQUIRE_MESSAGE(gameplay::nav_build(in, grid, &err), err);
    gameplay::NavSearch search;
    gameplay::NavPath p;
    // от спавна (128, 136) к точке перед обеденным столом с другой стороны
    // стола: стол (126.4, 130.6) 1,2 × 0,8 — цель за ним, z = 129.0
    const glm::vec3 from{128.0f, 25.5f, 136.0f};
    const glm::vec3 to{126.4f, 25.5f, 128.6f};
    REQUIRE(gameplay::nav_find_path(grid, search, from, to, p));
    const float straight = glm::length(glm::vec2{to.x - from.x, to.z - from.z});
    // стол — по его собственному чертежу, не по догадке о размере
    glm::vec3 lo, hi;
    REQUIRE(navtest::house_bounds("assets/houses/furn-table.dfh", lo, hi));
    const gameplay::NavBox table{glm::vec3{126.4f, 25.5f, 130.6f} + 0.5f * (lo + hi), 0.5f * (hi - lo), 0.0f};
    const float d = min_distance_to_box(p, table);
    MESSAGE("стенд: путь " << p.length_m() << " м (прямая " << straight << "), точек " << p.points.size() << " из "
                           << p.cells << " ячеек, до стола " << d << " м, раскрыто " << search.expanded);
    CHECK(p.length_m() > straight);
    CHECK(p.length_m() < 2.0f * straight);
    MESSAGE("стол по чертежу: " << (hi.x - lo.x) << " × " << (hi.z - lo.z) << " м, высота " << hi.y);
    CHECK(d >= grid.agent.radius); // стол обходится с запасом капсулы
    for (const glm::vec3& q : p.points) {
        CHECK(q.y == doctest::Approx(25.5f).epsilon(0.02));
    }
}
