/*
Module: tests
File: tests/sim/NavGridTests.cpp

Responsibility:
- ПРИБОР ПОСТРОЙКИ СЕТКИ ПРОХОДИМОСТИ (engine/gameplay/sources/NavGrid.cpp,
  NPC_NAVIGATION.md §2): этажи на плоскости; столбец под столом — этаж пола
  без просвета и этаж столешницы; эрозия на радиус капсулы у стены; марш
  0,18/0,28 связан по шагу (контроль — агент с шагом 0,1 марш не берёт);
  откос: мосты стенда stairs 33° проходимы, 54° — нет; стенд interaction
  строится из чертежей и хэш/число треугольников совпадают у двух сборок
  одного входа (контроль — лишний дом меняет хэш); цена постройки на сцене
  whiterun без окна (условие 2 синка 11.09) — числа в записку.

Key items:
- a_flat_field_is_walkable_except_its_rim
- the_column_under_a_table_holds_a_floor_and_a_top
- a_wall_eats_a_band_of_the_capsule_radius
- the_march_is_connected_by_step_and_a_short_step_refuses_it
- slopes_are_judged_by_the_capsule_limit
- the_interaction_stand_builds_and_the_geometry_hash_is_the_collector_s
- diagnostic_whiterun_build_cost

Dependencies:
- Uses: doctest, dfn_gameplay (NavGrid), dfn_world (Scene, HouseFile,
  HouseMesh) через tests/sim/fixtures/NavSceneSoup.h, реестр.
- Used by: ctest (sim_nav_grid).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Правило 17b: сцены читаются файлами, карта мира не открывается.
*/
#include "engine/core/config/sources/Constants.h"
#include "engine/gameplay/sources/NavGrid.h"
#include "tests/sim/fixtures/NavSceneSoup.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using namespace dfn;

namespace {

float flat_ground(void*, glm::vec2) { return 0.0f; }

struct Field {
    std::vector<gameplay::NavBox> boxes;
    std::vector<gameplay::NavMeshSoup> meshes;
    gameplay::NavInput in;
    gameplay::NavGrid grid;
    std::string err;

    explicit Field(float span = 10.0f) {
        in.ground = &flat_ground;
        in.min_xz = {-0.5f * span, -0.5f * span};
        in.max_xz = {0.5f * span, 0.5f * span};
        in.agent = gameplay::nav_agent_from_registry();
    }
    void box(glm::vec3 c, glm::vec3 he, float yaw = 0.0f) { boxes.push_back({c, he, yaw}); }
    bool build() {
        in.boxes = boxes;
        in.meshes = meshes;
        return gameplay::nav_build(in, grid, &err);
    }
    /// Этажи столбца под точкой.
    [[nodiscard]] std::vector<gameplay::NavFloor> floors_at(glm::vec2 p) const {
        std::vector<gameplay::NavFloor> out;
        const auto ix = static_cast<int64_t>(std::floor((p.x - grid.origin.x) / grid.cell));
        const auto iz = static_cast<int64_t>(std::floor((p.y - grid.origin.y) / grid.cell));
        if (ix < 0 || iz < 0 || ix >= grid.nx || iz >= grid.nz) {
            return out;
        }
        const gameplay::NavColumn& c = grid.columns[static_cast<std::size_t>(iz) * grid.nx + static_cast<std::size_t>(ix)];
        for (uint32_t k = 0; k < c.count; ++k) {
            out.push_back(grid.floors[c.first + k]);
        }
        return out;
    }
    [[nodiscard]] bool walkable_at(glm::vec3 p) const {
        for (const gameplay::NavFloor& f : floors_at({p.x, p.z})) {
            if (f.walkable && std::abs(grid.top_y(f) - p.y) <= grid.agent.step) {
                return true;
            }
        }
        return false;
    }
};

} // namespace

TEST_CASE("a_flat_field_is_walkable_except_its_rim") {
    Field f;
    REQUIRE(f.build());
    CHECK(f.grid.stats.columns == 40u * 40u);
    CHECK(f.grid.stats.floors == f.grid.stats.columns); // один этаж на столбец
    // край охвата — граница связного этажа, эрозия съедает полосу радиуса
    CHECK(f.walkable_at({0.0f, 0.0f, 0.0f}));
    CHECK(f.walkable_at({4.0f, 0.0f, 4.0f}));
    CHECK(!f.walkable_at({4.9f, 0.0f, 0.0f}));
    MESSAGE("столбцов " << f.grid.stats.columns << ", этажей " << f.grid.stats.floors << ", проходимых "
                        << f.grid.stats.walkable << ", память " << f.grid.memory_bytes() / 1024 << " КБ");
    CHECK(f.grid.stats.walkable < f.grid.stats.floors);
    CHECK(f.grid.stats.walkable > f.grid.stats.floors * 3 / 4);
}

TEST_CASE("the_column_under_a_table_holds_a_floor_and_a_top") {
    Field f;
    // стол 1,2 × 0,8, столешница 0,05 на 0,80, четыре ножки 0,06
    f.box({0.0f, 0.775f, 0.0f}, {0.6f, 0.025f, 0.4f});
    for (const float sx : {-0.55f, 0.55f}) {
        for (const float sz : {-0.35f, 0.35f}) {
            f.box({sx, 0.375f, sz}, {0.03f, 0.375f, 0.03f});
        }
    }
    REQUIRE(f.build());
    const auto under = f.floors_at({0.0f, 0.0f});
    REQUIRE(under.size() == 2);
    MESSAGE("под столом: пол " << f.grid.top_y(under[0]) << " м, просвет до " << f.grid.top_y({under[0].ceiling})
                               << ", столешница " << f.grid.top_y(under[1]));
    CHECK(!under[0].clear);   // 0,75 м просвета — не под рост
    CHECK(under[1].clear);    // столешница открыта небу
    CHECK(f.grid.top_y(under[1]) == doctest::Approx(0.8f).epsilon(0.1));
    // столешница 1,2 × 0,8 несёт проходимые ячейки (капсула на ней стоит),
    // но это ОСТРОВ: со земли не шагнуть (0,8 > шаг 0,35) — идём по ней к
    // краю, и связь обрывается, не спускаясь на землю
    const auto top = gameplay::nav_locate(f.grid, {0.0f, 0.8f, 0.0f}, 0.2f);
    REQUIRE(top);
    gameplay::NavRef cur = *top;
    int steps = 0;
    while (const auto n = gameplay::nav_neighbour(f.grid, cur, 0, 1)) {
        cur = gameplay::NavRef{cur.ix, cur.iz + 1, *n};
        CHECK(f.grid.top_y(f.grid.floors[cur.floor]) > 0.7f);
        ++steps;
    }
    MESSAGE("остров столешницы: " << steps << " ячеек до края, связи вниз нет");
    CHECK(steps < 4);
    // рядом со столом земля проходима, вплотную (в полосе радиуса) — нет
    CHECK(f.walkable_at({0.0f, 0.0f, 1.2f}));
    CHECK(!f.walkable_at({0.0f, 0.0f, 0.5f}));
}

TEST_CASE("a_wall_eats_a_band_of_the_capsule_radius") {
    Field f;
    f.box({0.0f, 1.0f, 0.0f}, {3.0f, 1.0f, 0.05f}); // стена 6 м вдоль X, толщина 0,1
    REQUIRE(f.build());
    const float r = f.grid.agent.radius;
    // дальше радиуса + ячейки — проходимо; ближе половины радиуса — нет
    CHECK(f.walkable_at({0.0f, 0.0f, r + f.grid.cell + 0.05f}));
    CHECK(f.walkable_at({0.0f, 0.0f, -(r + f.grid.cell + 0.05f)}));
    CHECK(!f.walkable_at({0.0f, 0.0f, 0.5f * r}));
    CHECK(!f.walkable_at({0.0f, 0.0f, -0.5f * r}));
    // контроль: агент нулевого радиуса подходит вплотную
    Field g;
    g.box({0.0f, 1.0f, 0.0f}, {3.0f, 1.0f, 0.05f});
    g.in.agent.radius = 0.0f;
    REQUIRE(g.build());
    CHECK(g.walkable_at({0.0f, 0.0f, 0.3f})); // соседняя со столбцом стены ячейка
    MESSAGE("у стены: проходимых " << f.grid.stats.walkable << " против " << g.grid.stats.walkable
                                   << " без радиуса");
    CHECK(f.grid.stats.walkable < g.grid.stats.walkable);
}

TEST_CASE("the_march_is_connected_by_step_and_a_short_step_refuses_it") {
    auto march = [](Field& f) {
        // девять ступеней 0,18/0,28 вдоль −Z от z = 0, потом терраса 1,62
        for (int i = 0; i < 9; ++i) {
            const float top = 0.18f * static_cast<float>(i + 1);
            f.box({0.0f, 0.5f * top, -0.28f * (static_cast<float>(i) + 0.5f)}, {2.0f, 0.5f * top, 0.14f});
        }
        f.box({0.0f, 0.81f, -0.28f * 9.0f - 2.0f}, {2.0f, 0.81f, 2.0f});
    };
    Field f(14.0f);
    march(f);
    REQUIRE(f.build());
    const auto bottom = gameplay::nav_locate(f.grid, {0.0f, 0.0f, 1.0f}, 0.3f);
    const auto top = gameplay::nav_locate(f.grid, {0.0f, 1.62f, -4.0f}, 0.3f);
    REQUIRE(bottom);
    REQUIRE(top);
    // связность: идём по −Z от подножия, каждый шаг находит соседа
    gameplay::NavRef cur = *bottom;
    int climbed = 0;
    while (true) {
        const auto n = gameplay::nav_neighbour(f.grid, cur, 0, -1);
        if (!n) {
            break;
        }
        cur = gameplay::NavRef{cur.ix, cur.iz - 1, *n};
        ++climbed;
        if (cur.iz == top->iz) {
            break;
        }
    }
    MESSAGE("марш: с " << f.grid.top_y(f.grid.floors[bottom->floor]) << " до " << f.grid.top_y(f.grid.floors[cur.floor])
                       << " м за " << climbed << " ячеек");
    CHECK(f.grid.top_y(f.grid.floors[cur.floor]) == doctest::Approx(1.62f).epsilon(0.05));
    // контроль: шаг 0,1 — первый же подступёнок не взят
    Field g(14.0f);
    march(g);
    g.in.agent.step = 0.1f;
    REQUIRE(g.build());
    const auto b2 = gameplay::nav_locate(g.grid, {0.0f, 0.0f, 1.0f}, 0.3f);
    REQUIRE(b2);
    gameplay::NavRef c2 = *b2;
    int climbed2 = 0;
    while (true) {
        const auto n = gameplay::nav_neighbour(g.grid, c2, 0, -1);
        if (!n) {
            break;
        }
        c2 = gameplay::NavRef{c2.ix, c2.iz - 1, *n};
        ++climbed2;
    }
    MESSAGE("контроль шаг 0,1: дошёл до " << g.grid.top_y(g.grid.floors[c2.floor]) << " м");
    CHECK(g.grid.top_y(g.grid.floors[c2.floor]) < 0.2f);
}

TEST_CASE("slopes_are_judged_by_the_capsule_limit") {
    // рельеф-скат: высота = tan(a)·x при x > 0
    struct Ctx { float tan_a; };
    auto ramp = [](void* c, glm::vec2 p) { return p.x > 0.0f ? static_cast<Ctx*>(c)->tan_a * p.x : 0.0f; };
    for (const float deg : {33.0f, 54.0f}) {
        Ctx ctx{std::tan(glm::radians(deg))};
        Field f;
        f.in.ground = ramp;
        f.in.ground_ctx = &ctx;
        REQUIRE(f.build());
        const glm::vec3 on_ramp{2.0f, ctx.tan_a * 2.0f, 0.0f};
        const bool walk = f.walkable_at(on_ramp);
        MESSAGE("скат " << deg << "°: " << (walk ? "проходим" : "нет") << " (предел "
                        << glm::degrees(f.grid.agent.max_slope_rad) << "°)");
        CHECK(walk == (deg < glm::degrees(f.grid.agent.max_slope_rad)));
        CHECK(f.walkable_at({-2.0f, 0.0f, 0.0f})); // плоская половина всегда
    }
}

TEST_CASE("the_interaction_stand_builds_and_the_geometry_hash_is_the_collector_s") {
    navtest::NavSceneSoup s;
    if (!navtest::load_scene_soup("assets/scenes/stands/interaction.scene", s)) {
        return; // сцены нет — не этот прибор
    }
    REQUIRE(s.houses_read == s.houses);
    gameplay::NavInput in = navtest::soup_input(s);
    gameplay::NavGrid grid;
    std::string err;
    REQUIRE_MESSAGE(gameplay::nav_build(in, grid, &err), err);
    MESSAGE("стенд interaction: домов " << s.houses_read << ", треугольников " << grid.stats.triangles << ", столбцов "
                                        << grid.stats.columns << ", этажей " << grid.stats.floors << ", проходимых "
                                        << grid.stats.walkable << ", хэш " << std::hex << grid.stats.geometry_hash);
    CHECK(grid.stats.triangles == s.indices.size() / 3);
    CHECK(grid.stats.triangles > 100);
    // стол обеденной группы: столбец под ним — пол без просвета и столешница
    const auto under = [&](glm::vec2 p) {
        Field probe;
        probe.grid = grid;
        return probe.floors_at(p);
    }({126.4f, 130.6f});
    REQUIRE(!under.empty());
    {
        std::string floors;
        for (const gameplay::NavFloor& fl : under) {
            floors += std::to_string(grid.top_y(fl)) + (fl.clear ? "(просвет) " : "(нет просвета) ");
        }
        MESSAGE("столбец под столом: " << floors);
    }
    // по земле под столом не пройти; верх стола на высоте столешницы
    bool ground_walkable = false;
    for (const gameplay::NavFloor& fl : under) {
        ground_walkable = ground_walkable || (fl.walkable && std::abs(grid.top_y(fl) - 25.5f) < 0.1f);
    }
    CHECK(!ground_walkable);
    CHECK(grid.top_y(under.back()) > 25.5f + 0.5f);
    CHECK(grid.top_y(under.back()) < 25.5f + 1.2f);
    // спавн стоит на проходимом этаже на высоте площадки
    const auto at_spawn = gameplay::nav_locate(grid, {128.0f, 25.5f, 136.0f}, 0.3f);
    REQUIRE(at_spawn);
    CHECK(grid.top_y(grid.floors[at_spawn->floor]) == doctest::Approx(25.5f).epsilon(0.01));
    // ОДИН СБОРЩИК: хэш второй сборки того же входа совпадает; лишний дом (ящик)
    // обязан менять хэш и число треугольников — так прибор в приложении ловит
    // сетку и коллайдер, собранные из разного.
    CHECK(gameplay::nav_geometry_hash(in) == grid.stats.geometry_hash);
    std::vector<gameplay::NavBox> extra{{{100.0f, 26.0f, 100.0f}, {1.0f, 1.0f, 1.0f}, 0.0f}};
    gameplay::NavInput in2 = in;
    in2.boxes = extra;
    CHECK(gameplay::nav_geometry_hash(in2) != grid.stats.geometry_hash);
    CHECK(gameplay::nav_geometry_triangles(in2) == grid.stats.triangles + 12);
}

TEST_CASE("diagnostic_whiterun_build_cost") {
    // УСЛОВИЕ 2 СИНКА: цена постройки на городе без окна. Порог решения —
    // ~1 с или ~200 МБ → сетка тайлами. Числа печатаются; прибор красный
    // только если постройка отказала.
    navtest::NavSceneSoup s;
    if (!std::filesystem::exists("assets/scenes/whiterun.scene")
        || !navtest::load_scene_soup("assets/scenes/whiterun.scene", s)) {
        return;
    }
    gameplay::NavInput in = navtest::soup_input(s);
    gameplay::NavGrid grid;
    std::string err;
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = gameplay::nav_build(in, grid, &err);
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    MESSAGE("whiterun: домов " << s.houses_read << "/" << s.houses << ", треугольников " << grid.stats.triangles
                               << ", охват " << (s.max_xz.x - s.min_xz.x) << "×" << (s.max_xz.y - s.min_xz.y)
                               << " м, столбцов " << grid.stats.columns << ", этажей " << grid.stats.floors
                               << ", проходимых " << grid.stats.walkable << ", пролётов " << grid.stats.spans
                               << ", постройка " << ms << " мс, память результата " << grid.memory_bytes() / (1024 * 1024)
                               << " МБ, пик постройки " << grid.stats.build_peak_bytes / (1024 * 1024) << " МБ");
    REQUIRE_MESSAGE(ok, err);
}
