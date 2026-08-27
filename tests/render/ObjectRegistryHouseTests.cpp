/*
Created: 27:08:2026 - 00:00:00
Last updated: 27:08:2026 - 00:00:00
Module: tests/render
File: tests/render/ObjectRegistryHouseTests.cpp

Responsibility:
- РУКАВ СЕКЦИИ HOUS формата .dfo (v3): куски запечённой постройки с номером
  плитки листа набора переживают запись и чтение, входят в личность объекта и
  в его замер — И НЕ ТРОГАЮТ ЛИЧНОСТЬ ОБЪЕКТОВ, У КОТОРЫХ ИХ НЕТ.

ПОЧЕМУ ПОСЛЕДНЕЕ — ГЛАВНОЕ. На полках лежит 2400+ файлов деревьев, деталей и
табличек, испечённых до этой секции. Если бы её появление изменило их хэш,
каждый из них пришлось бы перепечь, а до перепечки ВСЯКОЕ ЧТЕНИЕ отказывало бы
с «личность не сходится» — то есть город остался бы без единого дерева. Рукав
приколачивает правило «пустая секция в хэш не входит» к дереву.

Dependencies:
- Uses: engine/render (ObjectRegistry), doctest, std::filesystem.
- Used by: ctest (цель render_object_registry_house).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ФАЙЛЫ ПИШУТСЯ ВО ВРЕМЕННЫЙ КАТАЛОГ и убираются за собой: рукав, оставляющий
  мусор в дереве, однажды подсунет свой мусор полке.
*/
/*
UPD:
- 27:08:2026 - 00:00:00: Создан вместе с секцией HOUS (заказ владельца 27.08:
  кровать — единый запечённый объект реестра).
*/

#include "engine/render/sources/ObjectRegistry.h"

#include <doctest/doctest.h>

#include <filesystem>

namespace {

using namespace dfn::render;

/// Треугольник в потоке — минимум, который формат обязан пронести. Числа
/// НЕКРУГЛЫЕ намеренно: формат пишет float побитово, и ошибку укладки полей
/// ловят разряды, а не нули.
MeshData one_triangle(float shift) {
    MeshData m;
    for (int i = 0; i < 3; ++i) {
        dfn::platform::Vertex v{};
        v.position = {shift + 0.125f * static_cast<float>(i), 0.375f, -1.5f};
        v.normal = {0.0f, 1.0f, 0.0f};
        v.uv = {0.25f * static_cast<float>(i), 0.75f};
        v.color_rgba = 0xC0FFEE01u + static_cast<std::uint32_t>(i);
        m.vertices.push_back(v);
        m.indices.push_back(static_cast<std::uint32_t>(i));
    }
    return m;
}

std::filesystem::path temp_file(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

} // namespace

TEST_CASE("dfo: куски постройки переживают круговой прогон") {
    RegistryObject obj;
    obj.name = "furn-test";
    obj.kind = "furniture";
    obj.source = "test";
    HouseSubmesh a;
    a.surface = 5;
    a.tone = 3;
    a.emissive = false;
    a.mesh = one_triangle(0.0f);
    HouseSubmesh b;
    b.surface = 0;
    b.tone = 1;
    b.emissive = true;
    b.mesh = one_triangle(2.0f);
    obj.house = {a, b};

    const auto path = temp_file("dfn_hous_roundtrip.dfo");
    REQUIRE(write_object(obj, path));
    const auto back = read_object(path);
    REQUIRE(back.has_value());
    CHECK(back->house.size() == 2);
    CHECK(back->house[0].surface == 5);
    CHECK(back->house[0].tone == 3);
    CHECK(back->house[0].emissive == false);
    CHECK(back->house[1].surface == 0);
    CHECK(back->house[1].tone == 1);
    CHECK(back->house[1].emissive == true);
    CHECK(back->house[1].mesh.vertices.size() == 3);
    CHECK(back->house[1].mesh.vertices[2].color_rgba == 0xC0FFEE03u);
    CHECK(back->content_hash == object_content_hash(obj));
    std::filesystem::remove(path);
}

TEST_CASE("dfo: плитка куска входит в личность объекта") {
    // ДВА ОБЪЕКТА С ОДНОЙ ГЕОМЕТРИЕЙ И РАЗНЫМ МАТЕРИАЛОМ — РАЗНЫЕ ОБЪЕКТЫ.
    // Ровно на этом стоит приёмка владельца: «обновил предмет — обновился
    // везде» проверяется тем, что перекрашенная кровать получает ДРУГОЙ хэш,
    // и в перечне видно, что он сменился.
    RegistryObject a;
    a.name = "same";
    HouseSubmesh s;
    s.surface = 1;
    s.tone = 1;
    s.mesh = one_triangle(0.0f);
    a.house = {s};

    RegistryObject b = a;
    b.house[0].tone = 3; // «одеяло другого тона»
    CHECK(object_content_hash(a) != object_content_hash(b));

    RegistryObject c = a;
    c.house[0].emissive = true;
    CHECK(object_content_hash(a) != object_content_hash(c));
}

TEST_CASE("dfo: пустая секция постройки НЕ трогает личность старых объектов") {
    // ПОЛКА ДЕРЕВЬЕВ И НАБОРА ОБЯЗАНА ОСТАТЬСЯ БАЙТ В БАЙТ. Хэш объекта без
    // кусков постройки считается ровно так же, как считался до секции HOUS —
    // это проверяется тем, что добавление ПУСТОГО списка ничего не меняет, а
    // добавление непустого меняет.
    RegistryObject tree;
    tree.name = "oak-forge-a";
    tree.kind = "tree";
    tree.wood = one_triangle(0.0f);
    tree.cards = one_triangle(1.0f);
    const std::uint64_t bare = object_content_hash(tree);

    tree.house.clear(); // «секция есть в структуре, но её нет у объекта»
    CHECK(object_content_hash(tree) == bare);

    HouseSubmesh s;
    s.surface = 2;
    s.mesh = one_triangle(3.0f);
    tree.house = {s};
    CHECK(object_content_hash(tree) != bare);
}

TEST_CASE("dfo: объект из одних кусков постройки пишется и меряется") {
    // ОБЪЕКТ БЕЗ ЧЕТЫРЁХ ПРЕЖНИХ ПОТОКОВ — ЗАКОННЫЙ ОБЪЕКТ, а не «имя,
    // указывающее в пустоту»: у запечённой мебели вся геометрия живёт в
    // секции HOUS. Та же ошибка уже случалась с потоком bark в день, когда
    // строительный набор стал текстурным целиком.
    RegistryObject bed;
    bed.name = "furn-bed";
    bed.kind = "furniture";
    HouseSubmesh s;
    s.surface = 0;
    s.tone = 2;
    s.mesh = one_triangle(0.0f);
    // Высота выше шага человека: предмет обязан считаться СПЛОШНЫМ.
    for (auto& v : s.mesh.vertices) {
        v.position.y = 0.99f;
    }
    bed.house = {s};

    const auto path = temp_file("dfn_hous_only.dfo");
    REQUIRE(write_object(bed, path));
    const auto back = read_object(path);
    REQUIRE(back.has_value());

    const ObjectExtent e = measure_object(*back);
    CHECK(e.top == doctest::Approx(0.99f));
    CHECK(e.solid);
    CHECK(e.hi.x > e.lo.x);
    std::filesystem::remove(path);
}
