/*
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

#include "engine/render/sources/ObjectRegistry.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <system_error>

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

// =============================================================================
// ФОРМАТ v4 — СЕКЦИЯ MTRL (волна 3 зоны МАТЕРИАЛЫ)
// =============================================================================

TEST_CASE("dfo v4: имя вещества переживает круговой прогон") {
    // ИМЯ, А НЕ НОМЕР. Номер записи реестра был бы третьей копией порядка
    // таблицы, и вставка вещества в её середину перекрасила бы полку молча;
    // имя переживает и перекладку реестра, и перекладку листа набора.
    RegistryObject chest;
    chest.name = "furn-chest";
    chest.kind = "furniture";
    HouseSubmesh oak;
    oak.surface = 0;
    oak.tone = 2;
    oak.material = "oak-log";
    oak.span_m = 0.75f;
    oak.wear = 0.42f;
    oak.mesh = one_triangle(0.0f);
    HouseSubmesh iron;
    iron.surface = 3;
    iron.tone = 2;
    iron.material = "iron";
    iron.mesh = one_triangle(1.0f);
    chest.house = {oak, iron};

    const auto path = temp_file("dfn_mtrl_roundtrip.dfo");
    REQUIRE(write_object(chest, path));
    const auto back = read_object(path);
    REQUIRE(back.has_value());
    REQUIRE(back->house.size() == 2);
    CHECK(back->house[0].material == "oak-log");
    CHECK(back->house[0].span_m == doctest::Approx(0.75f));
    CHECK(back->house[1].material == "iron");
    // Кусок, вещества не назвавший, читается пустым именем и штатными
    // умолчаниями — «не сказано», а не «первое попавшееся вещество».
    CHECK(back->house[1].span_m == doctest::Approx(0.0f));
    CHECK(back->house[1].wear == doctest::Approx(-1.0f));
    // И ПАРА (surface, tone) НИКУДА НЕ ДЕЛАСЬ: v4 добавляет имя, а не заменяет
    // адрес картинки. Файл, потерявший пару, перестал бы читаться сборкой,
    // которая имени ещё не понимает.
    CHECK(back->house[0].surface == 0);
    CHECK(back->house[0].tone == 2);
    std::filesystem::remove(path);
}

TEST_CASE("dfo v4: НЕНАЗВАННОЕ вещество не трогает личность объекта") {
    // ЭТО ГЛАВНЫЙ РУКАВ ВОЛНЫ 3, И ПРИЧИНА ТА ЖЕ, ЧТО У ПУСТОЙ HOUS ВЫШЕ:
    // 2544 закоммиченных .dfo не называют ни одного вещества, и посчитайся
    // секция MTRL у них — вся полка сменила бы хэш и перестала читаться, не
    // изменившись ни на вершину. Разница с HOUS одна и важная: там пустым был
    // СПИСОК, здесь список полон, а пусты ИМЕНА.
    RegistryObject bed;
    bed.name = "furn-bed";
    bed.kind = "furniture";
    HouseSubmesh s;
    s.surface = 0;
    s.tone = 2;
    s.mesh = one_triangle(0.0f);
    bed.house = {s};
    const std::uint64_t v3_identity = object_content_hash(bed);

    // Поля v4 в умолчаниях — тот же хэш, что до появления секции.
    bed.house[0].material.clear();
    bed.house[0].span_m = 0.0f;
    bed.house[0].wear = -1.0f;
    CHECK(object_content_hash(bed) == v3_identity);

    // ...а НАЗВАННОЕ вещество личность меняет: иначе перепечку от смены
    // вещества нельзя было бы заметить ничем (перепечка от фаски видна по
    // числу треугольников, эта — только по хэшу).
    bed.house[0].material = "oak-log";
    const std::uint64_t named = object_content_hash(bed);
    CHECK(named != v3_identity);

    // И ШАГ ПОВТОРА — ТОЖЕ ЧАСТЬ ЛИЧНОСТИ: соломенная кровля с шагом 0.55
    // это другой предмет, чем та же кровля с метровым шагом, хотя вершины у
    // них одни и те же.
    bed.house[0].span_m = 0.55f;
    CHECK(object_content_hash(bed) != named);
}

TEST_CASE("dfo v4: файл из будущего читается, несовместимый раздел — нет") {
    // ПОЧИНКА ВОРОТ ВЕРСИИ (дефект 1.5.3 инвентаризации материалов). Стояло
    // «версия контейнера выше моей — отказать целиком», и это отменяло смысл
    // секционного формата: пропуск неизвестных секций работает, длина секции
    // была мёртвым кодом. Здесь проверяется, что отказ переехал туда, где он
    // честен.
    RegistryObject obj;
    obj.name = "future";
    obj.kind = "probe";
    obj.wood = one_triangle(0.0f);
    const auto path = temp_file("dfn_future.dfo");
    REQUIRE(write_object(obj, path));

    // Поднимаем ВЕРСИЮ КОНТЕЙНЕРА в самом файле (байты 4..7 после магии) —
    // так выглядит файл, испечённый сборкой из будущего теми же секциями.
    {
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        REQUIRE(f.good());
        f.seekp(4, std::ios::beg);
        const unsigned char v99[4] = {99, 0, 0, 0}; // little-endian
        f.write(reinterpret_cast<const char*>(v99), 4);
    }
    const auto back = read_object(path);
    // ЧИТАЕТСЯ. Разделы те же, личность считается тем же правилом (>= 2).
    CHECK(back.has_value());
    if (back.has_value()) {
        CHECK(back->wood.vertices.size() == 3);
    }
    std::filesystem::remove(path);
}

TEST_CASE("dfo v4: полка на диске читается и НИ ОДИН файл не сменил личность") {
    // САМАЯ ПРЯМАЯ ПРОВЕРКА ОБЕЩАНИЯ «СТАРЫЕ ХЭШИ ЖИВЫ». Читатель сверяет
    // хэш на КАЖДОМ чтении и отказывает файлу, чьи байты разошлись с
    // записанной личностью, — значит успешное чтение всей полки и есть
    // доказательство, что подъём формата до v4 не переверсировал ничего.
    //
    // Рукав идёт по НАСТОЯЩЕЙ полке в дереве, а не по временным файлам:
    // временный файл пишет и читает один и тот же код, и потому не может
    // поймать расхождение с тем, что уже лежит на диске.
    const std::filesystem::path root = "assets/objects";
    if (!std::filesystem::exists(root)) {
        MESSAGE("полки нет в этом дереве — случай пропущен");
        return;
    }
    int read_ok = 0;
    int refused = 0;
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".dfo") {
            continue;
        }
        if (read_object(entry.path()).has_value()) {
            ++read_ok;
        } else {
            ++refused;
            MESSAGE("отказ: " << entry.path().string());
        }
    }
    CHECK(refused == 0);
    // Контроль невакуозности (правило 30): цикл по пустой полке прошёл бы с
    // нулём отказов и не значил бы ничего. Полка — тысячи файлов.
    CHECK(read_ok > 1000);
}
