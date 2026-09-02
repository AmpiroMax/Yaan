/*
Module: tests/render
File: tests/render/ObjectRegistryTextureTests.cpp

Responsibility:
- РУКАВ СЕКЦИИ TEX формата .dfo (волна «текстура на скиннинге»): ссылки на
  листы (роль, путь, SHA-256, цветовое пространство, повтор) переживают запись
  и чтение, входят в личность объекта ВМЕСТЕ С SHA — И НЕ ТРОГАЮТ ЛИЧНОСТЬ
  ОБЪЕКТОВ, У КОТОРЫХ ИХ НЕТ (та же клятва, что у HOUS/MTRL/SKIN/MORF: полка
  из 2544 файлов не перепекается ради секции, которой у неё нет).

Dependencies:
- Uses: engine/render (ObjectRegistry), doctest, std::filesystem.
- Used by: ctest (цель render_object_registry_texture).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ФАЙЛЫ ПИШУТСЯ ВО ВРЕМЕННЫЙ КАТАЛОГ и убираются за собой.
*/

#include "engine/render/sources/ObjectRegistry.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace dfn::render;

namespace {

/// Минимальный персонаж: один треугольник скина и один сустав — ровно то,
/// без чего write_object отказывает как «имени, указывающему в никуда».
RegistryObject tiny_character() {
    RegistryObject obj;
    obj.name = "tex-test";
    obj.kind = "character";
    obj.source = "test";
    for (int i = 0; i < 3; ++i) {
        dfn::platform::SkinnedVertex v{};
        v.position = {static_cast<float>(i), 0.0f, 0.0f};
        v.normal = {0.0f, 1.0f, 0.0f};
        v.uv = {static_cast<float>(i) * 0.5f, 0.25f};
        v.color_rgba = 0xFFFFFFFFu;
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

TextureRef albedo_ref() {
    TextureRef t;
    t.role = "albedo";
    t.path = "assets/objects/characters/textures/HumanBase/albedo.png";
    t.sha256 = "3f94f3f6a5cab92dedf5cce392a87332312fc435f1cb44c6e7b897daef9a5fbb";
    t.colour_space = TEXTURE_COLOUR_SRGB;
    t.wrap = TEXTURE_WRAP_REPEAT;
    return t;
}

std::filesystem::path temp_file(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

} // namespace

TEST_CASE("dfo TEX: ссылка на лист переживает круговой прогон") {
    RegistryObject obj = tiny_character();
    obj.textures.push_back(albedo_ref());
    TextureRef n;
    n.role = "normal";
    n.path = "assets/objects/characters/textures/HumanBase/normal.png";
    n.sha256 = std::string(64, 'a');
    n.colour_space = TEXTURE_COLOUR_LINEAR;
    n.wrap = TEXTURE_WRAP_CLAMP;
    obj.textures.push_back(n);

    const auto path = temp_file("dfn_tex_roundtrip.dfo");
    REQUIRE(write_object(obj, path));
    const auto back = read_object(path);
    std::filesystem::remove(path);
    REQUIRE(back.has_value());
    REQUIRE(back->textures.size() == 2);
    CHECK(back->textures[0].role == "albedo");
    CHECK(back->textures[0].path == albedo_ref().path);
    CHECK(back->textures[0].sha256 == albedo_ref().sha256);
    CHECK(back->textures[0].colour_space == TEXTURE_COLOUR_SRGB);
    CHECK(back->textures[0].wrap == TEXTURE_WRAP_REPEAT);
    CHECK(back->textures[1].role == "normal");
    CHECK(back->textures[1].colour_space == TEXTURE_COLOUR_LINEAR);
    CHECK(back->textures[1].wrap == TEXTURE_WRAP_CLAMP);
    CHECK(back->content_hash == object_content_hash(obj));
    REQUIRE(back->texture("albedo") != nullptr);
    CHECK(back->texture("albedo")->sha256 == albedo_ref().sha256);
    CHECK(back->texture("roughness") == nullptr);
}

TEST_CASE("dfo TEX: пустой список НЕ трогает личность объекта без листа") {
    const RegistryObject bare = tiny_character();
    RegistryObject with = bare;
    with.textures.push_back(albedo_ref());
    // Личность без листа — та же, что до появления секции; с листом — другая.
    CHECK(object_content_hash(bare) != object_content_hash(with));
    RegistryObject emptied = with;
    emptied.textures.clear();
    CHECK(object_content_hash(emptied) == object_content_hash(bare));

    // И файл без листа побайтово тот же: секция не пишется, когда писать нечего.
    const auto p1 = temp_file("dfn_tex_bare1.dfo");
    const auto p2 = temp_file("dfn_tex_bare2.dfo");
    REQUIRE(write_object(bare, p1));
    REQUIRE(write_object(emptied, p2));
    CHECK(std::filesystem::file_size(p1) == std::filesystem::file_size(p2));
    const auto r1 = read_object(p1);
    std::filesystem::remove(p1);
    std::filesystem::remove(p2);
    REQUIRE(r1.has_value());
    CHECK(r1->textures.empty());
}

TEST_CASE("dfo TEX: sha входит в личность — другой PNG, другое тело") {
    RegistryObject a = tiny_character();
    a.textures.push_back(albedo_ref());
    RegistryObject b = a;
    b.textures[0].sha256[0] = b.textures[0].sha256[0] == '0' ? '1' : '0';
    CHECK(object_content_hash(a) != object_content_hash(b));
    RegistryObject c = a;
    c.textures[0].wrap = TEXTURE_WRAP_CLAMP;
    CHECK(object_content_hash(a) != object_content_hash(c));
}

TEST_CASE("dfo TEX: усечённая секция — отказ, а не полперсонажа") {
    RegistryObject obj = tiny_character();
    obj.textures.push_back(albedo_ref());
    const auto path = temp_file("dfn_tex_truncated.dfo");
    REQUIRE(write_object(obj, path));
    // Отрезаем хвост файла (секция TEX пишется последней).
    const auto size = std::filesystem::file_size(path);
    std::filesystem::resize_file(path, size - 24);
    const auto back = read_object(path);
    std::filesystem::remove(path);
    CHECK_FALSE(back.has_value());
}
