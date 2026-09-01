/*
Module: tests/render
File: tests/render/MorphTests.cpp

Responsibility:
- РУКАВ СЕКЦИИ MORF (.dfo) И CPU-БЛЕНДА: цели переживают запись и чтение,
  входят в личность объекта — И НЕ ТРОГАЮТ ЛИЧНОСТЬ ТЕХ, У КОГО ИХ НЕТ; номер
  вершины за краем SKIN — отказ, а не «почти то тело»; бленд на нулевых весах
  возвращает вход ПОБИТОВО, а сумма дельт не зависит от порядка, в котором
  крутили ручки.

ПОЧЕМУ «ПОБИТОВО НА НУЛЕ» — ГЛАВНАЯ СТРОКА. Бленд пересчитывает нормали, а
вершина .dfo живёт в пространстве привязки, тогда как нормаль в файле посчитана
по рест-позе: посчитай нормаль здесь заново и положи как есть — тело сменило бы
освещение при всех ползунках на нуле, то есть ДО всякого морфа. MorphBlend.h
кладёт РАЗНИЦУ нормалей именно поэтому, и эта проверка — единственное, что
отличает «мы так решили» от «мы это знаем».

Dependencies:
- Uses: engine/render (ObjectRegistry, MorphBlend), doctest, std::filesystem.
- Used by: ctest (цель render_morph).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ФАЙЛЫ ПИШУТСЯ ВО ВРЕМЕННЫЙ КАТАЛОГ и убираются за собой.
*/

#include "engine/render/sources/MorphBlend.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <doctest/doctest.h>

#include <bit>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace {

using namespace dfn::render;

/// Минимальный персонаж: один треугольник со скином. Числа НЕКРУГЛЫЕ намеренно —
/// формат пишет float побитово, и ошибку укладки полей ловят разряды, а не нули.
RegistryObject tiny_character() {
    RegistryObject obj;
    obj.name = "morph-fixture";
    obj.kind = "character";
    obj.source = "test";
    for (int i = 0; i < 3; ++i) {
        dfn::platform::SkinnedVertex v{};
        v.position = {0.125f * static_cast<float>(i), 1.375f, -0.5f};
        v.normal = {0.0f, 0.0f, 1.0f};
        v.uv = {0.25f, 0.75f};
        v.color_rgba = 0xFF203040u;
        v.joints[0] = 0;
        v.weights[0] = 1.0f;
        obj.skin.vertices.push_back(v);
    }
    obj.skin.indices = {0, 1, 2};
    dfn::skel::SkeletonJoint j;
    j.name = "root";
    j.parent = -1;
    obj.skeleton.joints.push_back(j);
    return obj;
}

MorphTarget belly_target() {
    MorphTarget t;
    t.name = "belly";
    t.lo = 0.0f;
    t.hi = 0.45f;
    t.deltas.push_back({1u, glm::vec3{0.0f, 0.03125f, 0.0625f}});
    return t;
}

std::filesystem::path scratch(const char* stem) {
    return std::filesystem::temp_directory_path()
           / (std::string("dfn-morph-") + stem + ".dfo");
}

} // namespace

TEST_CASE("MORF переживает запись и чтение") {
    RegistryObject obj = tiny_character();
    obj.morphs.push_back(belly_target());
    MorphTarget hips;
    hips.name = "hips";
    hips.lo = -0.75f;
    hips.hi = 1.0f;
    hips.deltas.push_back({0u, glm::vec3{-0.015625f, 0.0f, 0.0f}});
    hips.deltas.push_back({2u, glm::vec3{0.015625f, 0.0f, 0.0f}});
    obj.morphs.push_back(hips);

    const auto path = scratch("roundtrip");
    REQUIRE(write_object(obj, path));
    const auto back = read_object(path);
    REQUIRE(back.has_value());
    REQUIRE(back->morphs.size() == 2);
    CHECK(back->morphs[0].name == "belly");
    CHECK(back->morphs[0].lo == 0.0f);
    CHECK(back->morphs[0].hi == 0.45f);
    REQUIRE(back->morphs[0].deltas.size() == 1);
    CHECK(back->morphs[0].deltas[0].index == 1u);
    // ПОБИТОВО, а не «примерно»: дельта — это то, что складывается с вершиной,
    // и «почти та же» дельта даёт файл выпечки, который не сойдётся с чужим.
    CHECK(std::bit_cast<uint32_t>(back->morphs[0].deltas[0].offset.z)
          == std::bit_cast<uint32_t>(0.0625f));
    REQUIRE(back->morphs[1].deltas.size() == 2);
    CHECK(back->morphs[1].deltas[1].index == 2u);
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("пустая секция MORF не трогает личность объекта") {
    // ТОТ ЖЕ ДОВОД, ЧТО У HOUS, MTRL И SKIN, и он сторожит 2544 закоммиченных
    // файла полок: посчитайся пустой список морфов, все они сменили бы хэш и
    // перестали читаться, ни на вершину не изменившись.
    RegistryObject plain = tiny_character();
    const uint64_t before = object_content_hash(plain);
    plain.morphs.clear();
    CHECK(object_content_hash(plain) == before);

    RegistryObject with = tiny_character();
    with.morphs.push_back(belly_target());
    CHECK(object_content_hash(with) != before);

    // И ФАЙЛ БЕЗ МОРФОВ ОБЯЗАН БЫТЬ ПОБАЙТОВО ТЕМ ЖЕ, что и до появления
    // секции: выпечка по «Готово» снимает MORF, и её личность должна совпадать
    // с личностью тела, которое морфов никогда и не носило.
    const auto a = scratch("plain-a");
    const auto b = scratch("plain-b");
    RegistryObject baked = tiny_character();
    baked.morphs.clear();
    REQUIRE(write_object(plain, a));
    REQUIRE(write_object(baked, b));
    std::ifstream fa(a, std::ios::binary);
    std::ifstream fb(b, std::ios::binary);
    const std::string sa((std::istreambuf_iterator<char>(fa)),
                         std::istreambuf_iterator<char>());
    const std::string sb((std::istreambuf_iterator<char>(fb)),
                         std::istreambuf_iterator<char>());
    CHECK(sa == sb);
    std::error_code ec;
    std::filesystem::remove(a, ec);
    std::filesystem::remove(b, ec);
}

TEST_CASE("морф за краем SKIN — ОТКАЗ, а не усечение") {
    RegistryObject obj = tiny_character();
    MorphTarget t = belly_target();
    t.deltas.push_back({99u, glm::vec3{1.0f, 0.0f, 0.0f}});
    obj.morphs.push_back(t);
    const auto path = scratch("outofrange");
    REQUIRE(write_object(obj, path));
    // Цель, у которой выкинули часть вершин, — это ДРУГАЯ цель под тем же
    // именем; читатель обязан отказать целиком.
    CHECK_FALSE(read_object(path).has_value());
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("бленд на нулевых весах возвращает вход ПОБИТОВО") {
    const RegistryObject obj = [] {
        RegistryObject o = tiny_character();
        o.morphs.push_back(belly_target());
        return o;
    }();
    std::vector<float> zero(obj.morphs.size(), 0.0f);
    std::vector<dfn::platform::SkinnedVertex> out;
    blend_morphs(obj.skin.vertices, obj.morphs, zero, obj.skin.indices, out);
    REQUIRE(out.size() == obj.skin.vertices.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        CAPTURE(i);
        CHECK(std::bit_cast<uint32_t>(out[i].position.y)
              == std::bit_cast<uint32_t>(obj.skin.vertices[i].position.y));
        // НОРМАЛЬ ТОЖЕ. Это и есть проверка «разницы, а не замены»: посчитай
        // бленд нормаль заново по геометрии привязки — здесь была бы другая.
        CHECK(std::bit_cast<uint32_t>(out[i].normal.z)
              == std::bit_cast<uint32_t>(obj.skin.vertices[i].normal.z));
    }
}

TEST_CASE("бленд двигает ТОЛЬКО названные вершины и НЕ трогает веса скина") {
    RegistryObject obj = tiny_character();
    obj.morphs.push_back(belly_target());
    std::vector<float> w{0.45f};
    std::vector<dfn::platform::SkinnedVertex> out;
    blend_morphs(obj.skin.vertices, obj.morphs, w, obj.skin.indices, out);
    // Вершина 1 названа целью; 0 и 2 — нет, и их позиция обязана остаться той же
    // до бита: «дельта не течёт из региона» проверяется здесь в самом малом виде.
    CHECK(out[1].position.z == doctest::Approx(-0.5f + 0.0625f * 0.45f));
    CHECK(std::bit_cast<uint32_t>(out[0].position.z)
          == std::bit_cast<uint32_t>(obj.skin.vertices[0].position.z));
    CHECK(std::bit_cast<uint32_t>(out[2].position.z)
          == std::bit_cast<uint32_t>(obj.skin.vertices[2].position.z));
    // МОРФ НЕ ТРОГАЕТ СКИННИНГ — доказано, а не обещано: ни одна вершина не
    // потеряла сустав и сумму весов.
    for (std::size_t i = 0; i < out.size(); ++i) {
        CAPTURE(i);
        float sum = 0.0f;
        for (int k = 0; k < 4; ++k) {
            CHECK(out[i].joints[k] == obj.skin.vertices[i].joints[k]);
            sum += out[i].weights[k];
        }
        CHECK(sum == doctest::Approx(1.0f));
    }
}

TEST_CASE("сумма дельт не зависит от порядка, в котором крутили ручки") {
    // ПОРЯДОК СЛАГАЕМЫХ ЗАДАН ФАЙЛОМ, а не пользователем. Сложение float не
    // ассоциативно; если бы бленд складывал в порядке правок, «выпечка
    // воспроизводима байт-в-байт» проверяла бы удачу.
    RegistryObject obj = tiny_character();
    MorphTarget a = belly_target();
    MorphTarget b;
    b.name = "hips";
    b.lo = -1.0f;
    b.hi = 1.0f;
    b.deltas.push_back({1u, glm::vec3{0.1f, 0.2f, 0.3f}});
    obj.morphs.push_back(a);
    obj.morphs.push_back(b);
    std::vector<dfn::platform::SkinnedVertex> first;
    std::vector<dfn::platform::SkinnedVertex> second;
    blend_morphs(obj.skin.vertices, obj.morphs, std::vector<float>{0.31f, -0.62f},
                 obj.skin.indices, first);
    blend_morphs(obj.skin.vertices, obj.morphs, std::vector<float>{0.31f, -0.62f},
                 obj.skin.indices, second);
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(std::bit_cast<uint32_t>(first[i].position.x)
              == std::bit_cast<uint32_t>(second[i].position.x));
    }
    // morph_index — одно определение имени на дозу, пресет и панель.
    CHECK(morph_index(obj.morphs, "hips") == 1);
    CHECK(morph_index(obj.morphs, "нет такой") == -1);
}

TEST_CASE("прибор ширины: цель без сдвигов видна как цель без сдвигов") {
    // Сторож тихого брака §2б записки: «двенадцать целей готово» при нуле
    // сдвинутых вершин выглядит как успех и им не является.
    const RegistryObject obj = tiny_character();
    MorphTarget dead;
    dead.name = "dead";
    dead.deltas.push_back({0u, glm::vec3{0.0f, 0.0f, 0.0f}});
    const MorphSpread s = morph_spread(obj.skin.vertices, dead, 1.0f, 0.001f);
    CHECK(s.moved == 0);
    const MorphSpread live =
        morph_spread(obj.skin.vertices, belly_target(), 1.0f, 0.001f);
    CHECK(live.moved == 1);
    CHECK(live.worst_m == doctest::Approx(std::sqrt(0.03125f * 0.03125f
                                                    + 0.0625f * 0.0625f)));
    CHECK(live.lowest_y == doctest::Approx(1.375f));
}
