/*
Created: 28:08:2026 - 13:01:31
Last updated: 28:08:2026 - 13:01:31
Module: tests/app
File: tests/app/PropPhysicsTests.cpp

Responsibility:
- ФИЗИЧЕСКИЕ СВОЙСТВА ПРЕДМЕТА РЕЕСТРА: арифметика массы и её отвергаемый
  случай. Рукав линкует ТОЛЬКО PropPhysics.cpp плюс чтение полки — ни окна, ни
  физики, ни App: массу предмета обязано быть можно провалить нарочно.

Key items:
- Замкнутый куб: объём, площадь и масса известны наизусть (случай, который
  МОЖЕТ пройти, правило 30a).
- ОТВЕРГАЕМЫЙ СЛУЧАЙ НАСТОЯЩИЙ (правило 30): двухсоткилограммовый сундук из
  записки №4 ресёрчера — «габарит 0.31 м³ x дуб 700 = 217 кг, настоящий 15-25».
  Проверяется, что наше правило его отвергает, а не что оно даёт «какое-то
  число».
- Несшитая оболочка НАЗЫВАЕТСЯ вслух, а не даёт молчаливую массу.
- Вся полка: у каждого подвижного предмета масса в человеческой полосе, и
  числа ПЕЧАТАЮТСЯ — рука судит по числам, а не только по приговору.

Dependencies:
- Uses: engine/app/sources/PropPhysics.h, engine/render (чтение .dfo), doctest.
- Used by: ctest (app_prop_physics).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона big-grab владеет этим файлом.
*/
/*
UPD:
- 28:08:2026 - 13:01:31: Создан вместе с PropPhysics.*.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/PropPhysics.h"
#include "engine/core/materials/sources/PhysicsSubstance.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <cstdio>
#include <filesystem>
#include <vector>

using namespace dfn;

namespace {

// Замкнутый единичный куб: 12 треугольников, объём 1, площадь 6.
struct Cube {
    std::vector<glm::vec3> positions;
    std::vector<std::uint32_t> indices;
};

Cube unit_cube(float side) {
    Cube c;
    const float s = side;
    c.positions = {{0, 0, 0}, {s, 0, 0}, {s, s, 0}, {0, s, 0},
                   {0, 0, s}, {s, 0, s}, {s, s, s}, {0, s, s}};
    c.indices = {0, 2, 1, 0, 3, 2,  // -z
                 4, 5, 6, 4, 6, 7,  // +z
                 0, 1, 5, 0, 5, 4,  // -y
                 3, 7, 6, 3, 6, 2,  // +y
                 0, 4, 7, 0, 7, 3,  // -x
                 1, 2, 6, 1, 6, 5}; // +x
    return c;
}

// Собирает позиции и индексы предмета реестра из его кусков постройки.
bool object_geometry(const std::string& name, std::vector<glm::vec3>& positions,
                     std::vector<std::uint32_t>& indices) {
    const auto obj =
        render::read_object(std::filesystem::path("assets/objects/furniture") / (name + ".dfo"));
    if (!obj) {
        return false;
    }
    for (const render::HouseSubmesh& sub : obj->house) {
        const auto base = static_cast<std::uint32_t>(positions.size());
        for (const platform::Vertex& v : sub.mesh.vertices) {
            positions.push_back(v.position);
        }
        for (const std::uint32_t i : sub.mesh.indices) {
            indices.push_back(base + i);
        }
    }
    return !positions.empty();
}

} // namespace

TEST_CASE("замкнутый куб мерится наизусть") {
    const Cube c = unit_cube(1.0f);
    const app::MeshBulk bulk = app::measure_bulk(c.positions, c.indices);
    CHECK(bulk.volume_m3 == doctest::Approx(1.0f).epsilon(0.001));
    CHECK(bulk.area_m2 == doctest::Approx(6.0f).epsilon(0.001));
    CHECK(bulk.bbox_m3 == doctest::Approx(1.0f).epsilon(0.001));
    CHECK(bulk.triangles == 12);

    // СЛУЧАЙ, КОТОРЫЙ МОЖЕТ ПРОЙТИ (правило 30a): сплошной сосновый куб
    // объёмом 1 м³ весит ровно плотность сосны... но упирается в потолок
    // массы, и это тоже проверяемое поведение.
    app::PropRow solid;
    solid.substance = "pine";
    const app::PropMass m = app::prop_mass(bulk, solid, "куб");
    CHECK_FALSE(m.fell_back);
    CHECK(m.mass_kg == doctest::Approx(app::PROP_MASS_MAX_KG));

    // Тот же куб как ОБОЛОЧКА со стенкой 10 мм: 6 м² x 0.01 = 0.06 м³.
    app::PropRow shell = solid;
    shell.wall_m = 0.01f;
    const app::PropMass ms = app::prop_mass(bulk, shell, "ящик");
    CHECK(ms.shell);
    // 6 м² замкнутой поверхности — это 3 м² стенки (обе стороны посчитаны),
    // и на 10 мм это 0.03 м³ сосны = 15 кг.
    CHECK(ms.used_volume_m3 == doctest::Approx(0.03f).epsilon(0.01));
    CHECK(ms.mass_kg == doctest::Approx(15.0f).epsilon(0.01));
}

TEST_CASE("отвергаемый случай — двухсоткилограммовый сундук") {
    // НАСТОЯЩИЙ ОТВЕРГНУТЫЙ ОБРАЗЕЦ (записка №4, А3): габарит сундука полки
    // 0.94 x 0.55 x 0.60 = 0.31 м³; дуб 700 по габариту даёт 217 кг, а
    // настоящий сундук такого размера весит 15-25. Порог стоит ВЫШЕ
    // отвергнутого образца и НИЖЕ правдоподобного.
    std::vector<glm::vec3> positions;
    std::vector<std::uint32_t> indices;
    REQUIRE(object_geometry("furn-chest", positions, indices));
    const app::MeshBulk bulk = app::measure_bulk(positions, indices);

    const float naive_kg = bulk.bbox_m3 * app::substance_density("oak");
    std::printf("[сундук] габарит %.3f м³, знаковый объём %.4f м³, площадь %.3f м², "
                "наивная масса по габариту %.1f кг\n",
                static_cast<double>(bulk.bbox_m3), static_cast<double>(bulk.volume_m3),
                static_cast<double>(bulk.area_m2), static_cast<double>(naive_kg));
    CHECK(naive_kg > 150.0f); // образец, который надо отвергнуть, действительно тяжёл

    const auto table = app::load_prop_table("assets/objects/furniture/PHYSICS.txt");
    const auto row = table.find("furn-chest");
    REQUIRE(row != table.end());
    const app::PropMass m = app::prop_mass(bulk, row->second, "furn-chest");
    std::printf("[сундук] по правилу зоны: %.1f кг (объём в дело %.4f м³)\n",
                static_cast<double>(m.mass_kg), static_cast<double>(m.used_volume_m3));
    // ПОРОГ СТОИТ ВЫШЕ ОТВЕРГНУТОГО ОБРАЗЦА И НИЖЕ ПРАВДОПОДОБНОГО: 217 кг
    // отвергаются, 42 кг (сундук как двадцатимиллиметровые дубовые доски)
    // проходят. Верхний конец — 60 кг: выше него сундук уже не поднимет
    // никакой человек, и спор был бы не о числе, а о том, сундук ли это.
    CHECK(m.mass_kg < 60.0f);
    CHECK(m.mass_kg > 5.0f);
}

TEST_CASE("контрольная рука массы: стул, сундук, стол") {
    // ТРИ ПРЕДМЕТА С ИЗВЕСТНЫМ ОТВЕТОМ (ужесточение координатора 28.08 по
    // расхождению Р4). Ответ приходит НЕ из нашего кода: это вес настоящей
    // вещи той же формы — стул ~5 кг, сундук 15-25, стол 25-40. Полоса +-50%
    // выбрана не «чтобы прошло»: она шире разброса между сосной и дубом
    // (1.4x) и уже, чем ошибка, ради которой рука заведена (по габариту стул
    // выходил 152 кг — в тридцать раз).
    //
    // ЭТА РУКА — ЕДИНСТВЕННОЕ, ЧТО ЛОВИТ «ЧЕСТНОГО ЧИТАТЕЛЯ КОНТРАКТА»:
    // масса из ВЫПУКЛОЙ ОБОЛОЧКИ (а её берёт тело физики) для стула почти
    // равна его коробке, и симптом «стул не поднимается» списали бы на
    // потолок силы хвата, а не на массу.
    struct Expect {
        const char* name;
        float low_kg;
        float high_kg;
    };
    const Expect table_of_truth[] = {
        {"furn-chair", 2.5f, 7.5f},   // настоящий ~5
        {"furn-chest", 7.5f, 37.5f},  // настоящий 15-25
        {"furn-table", 12.5f, 60.0f}, // настоящий 25-40
    };
    const auto table = app::load_prop_table("assets/objects/furniture/PHYSICS.txt");
    for (const Expect& e : table_of_truth) {
        std::vector<glm::vec3> positions;
        std::vector<std::uint32_t> indices;
        REQUIRE(object_geometry(e.name, positions, indices));
        const app::MeshBulk bulk = app::measure_bulk(positions, indices);
        const auto row = table.find(e.name);
        REQUIRE(row != table.end());
        const app::PropMass m = app::prop_mass(bulk, row->second, e.name);
        // ОТВЕРГАЕМЫЙ СЛУЧАЙ ПЕЧАТАЕТСЯ РЯДОМ С ПРИНЯТЫМ: рука печатает числа,
        // по которым судит, а не только приговор.
        const float by_hull = bulk.bbox_m3 * app::substance_density(row->second.substance);
        std::printf("[контроль массы] %-12s по оболочке %7.1f кг, по правилу %6.2f кг "
                    "(ждём %.1f..%.1f)\n",
                    e.name, static_cast<double>(by_hull), static_cast<double>(m.mass_kg),
                    static_cast<double>(e.low_kg), static_cast<double>(e.high_kg));
        CHECK(m.mass_kg >= e.low_kg);
        CHECK(m.mass_kg <= e.high_kg);
        // И сам отвергаемый случай обязан лежать ВНЕ полосы — иначе рука не
        // различает то, ради чего заведена (правило 30).
        CHECK(by_hull > e.high_kg);
    }
}

TEST_CASE("несшитая оболочка называется вслух") {
    // Два треугольника плоским лоскутом: знаковый объём не значит ничего.
    const std::vector<glm::vec3> flat{{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}};
    const std::vector<std::uint32_t> idx{0, 1, 2, 0, 2, 3};
    const app::MeshBulk bulk = app::measure_bulk(flat, idx);
    app::PropRow row;
    row.substance = "wicker";
    const app::PropMass m = app::prop_mass(bulk, row, "лоскут");
    CHECK(m.fell_back);
    CHECK_FALSE(m.finding.empty());

    // КОНТРОЛЬ (правило 30): замкнутая коробка того же габарита НЕ должна
    // попасть в тот же откат — иначе правило отвергает всё подряд.
    const Cube c = unit_cube(1.0f);
    const app::MeshBulk closed = app::measure_bulk(c.positions, c.indices);
    const app::PropMass mc = app::prop_mass(closed, row, "куб");
    CHECK_FALSE(mc.fell_back);
}

TEST_CASE("таблица веществ отвечает только за то, что знает") {
    CHECK(app::substance_density("oak") == doctest::Approx(700.0f));
    CHECK(app::substance_density("мифрил") < 0.0f); // не знает — и говорит об этом
    CHECK(core::substances().size() > 20);
    // НУЛЕВАЯ ЗАПИСЬ — «не сказано», и она обязана нести умолчания Jolt: это
    // контрольная рука всякого утверждения про вещества (правило 47).
    CHECK(core::substance(core::SUBSTANCE_DEFAULT).friction == doctest::Approx(0.2f));
    CHECK(core::substance(core::SUBSTANCE_DEFAULT).restitution == doctest::Approx(0.0f));
    CHECK(core::find_substance("нетакого") == core::SUBSTANCE_NONE);
}

TEST_CASE("вся полка: масса каждого подвижного предмета человеческая") {
    const auto table = app::load_prop_table("assets/objects/furniture/PHYSICS.txt");
    REQUIRE(table.size() > 30);
    std::size_t loose = 0;
    std::printf("%-18s %8s %8s %8s %9s %s\n", "предмет", "габ,м³", "объём,м³", "площ,м²",
                "масса,кг", "примечание");
    for (const auto& [name, row] : table) {
        std::vector<glm::vec3> positions;
        std::vector<std::uint32_t> indices;
        if (!object_geometry(name, positions, indices)) {
            continue; // предмет ещё не испечён этой полкой — не наша беда
        }
        const app::MeshBulk bulk = app::measure_bulk(positions, indices);
        const app::PropMass m = app::prop_mass(bulk, row, name);
        std::printf("%-18s %8.4f %8.4f %8.3f %9.2f %s%s\n", name.c_str(),
                    static_cast<double>(bulk.bbox_m3), static_cast<double>(bulk.volume_m3),
                    static_cast<double>(bulk.area_m2), static_cast<double>(m.mass_kg),
                    row.cls == app::PropClass::Loose ? "подвижный " : "",
                    m.fell_back ? "[по габариту]" : "");
        if (row.cls != app::PropClass::Loose) {
            continue;
        }
        ++loose;
        // ЧЕЛОВЕЧЕСКАЯ ПОЛОСА: легче 30 граммов — это не предмет, а числовой
        // шум; тяжелее сотни килограммов — это не утварь. Оба конца выведены
        // (правило 30, «диапазон — два утверждения»): нижний — порог, ниже
        // которого Jolt перестаёт держать контакт устойчиво, верхний — вес,
        // который человек не сдвинет и телом.
        CHECK(m.mass_kg >= 0.03f);
        CHECK(m.mass_kg <= 100.0f);
    }
    CHECK(loose >= 15); // полка обязана нести утварь, а не только мебель
}

TEST_CASE("прореживание оболочки не роняет крайние вершины") {
    std::vector<glm::vec3> pts;
    for (int i = 0; i < 1000; ++i) {
        const auto f = static_cast<float>(i);
        pts.push_back({f * 0.001f, std::sin(f) * 0.1f, std::cos(f) * 0.1f});
    }
    const auto hull = app::hull_points(pts, 64);
    CHECK(hull.size() <= 64 + 8);
    CHECK(hull.size() >= 4);
    float max_x = -1.0f;
    for (const glm::vec3& p : hull) {
        max_x = std::max(max_x, p.x);
    }
    CHECK(max_x == doctest::Approx(0.999f).epsilon(0.001)); // самая дальняя вершина взята

    // КОНТРОЛЬ: маленький список не прореживается вовсе.
    const auto small = app::hull_points(std::span<const glm::vec3>(pts).first(10), 64);
    CHECK(small.size() == 10);
}
