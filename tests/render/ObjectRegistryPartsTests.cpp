/*
Module: tests/render
File: tests/render/ObjectRegistryPartsTests.cpp

Responsibility:
- РУКАВ СЕКЦИИ PART формата .dfo (волна «части персонажа»): части — имя,
  флаги материала (вырез, порог, двусторонность), свой поток SKIN и свои
  листы — переживают запись и чтение, входят в личность объекта ЦЕЛИКОМ и
  НЕ ТРОГАЮТ ЛИЧНОСТЬ ОБЪЕКТОВ, У КОТОРЫХ ИХ НЕТ (та же клятва, что у
  HOUS/MTRL/SKIN/MORF/TEX). Файл набора (SKEL + PART, без SKIN) пишется и
  читается; часть, весящая на сустав за краем скелета, — отказ.

Dependencies:
- Uses: engine/render (ObjectRegistry), doctest, std::filesystem.
- Used by: ctest (цель render_object_registry_parts).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ФАЙЛЫ ПИШУТСЯ ВО ВРЕМЕННЫЙ КАТАЛОГ и убираются за собой.
*/

#include "engine/render/sources/ObjectRegistry.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

using namespace dfn::render;

namespace {

SkinMesh triangle(float x0, uint8_t joint) {
    SkinMesh m;
    for (int i = 0; i < 3; ++i) {
        dfn::platform::SkinnedVertex v{};
        v.position = {x0 + static_cast<float>(i), 0.0f, 0.0f};
        v.normal = {0.0f, 1.0f, 0.0f};
        v.uv = {static_cast<float>(i) * 0.5f, 0.25f};
        v.color_rgba = 0xFFFFFFFFu;
        v.joints[0] = joint;
        v.weights[0] = 1.0f;
        m.vertices.push_back(v);
    }
    m.indices = {0, 1, 2};
    return m;
}

/// Тело: один треугольник скина и два сустава.
RegistryObject tiny_character() {
    RegistryObject obj;
    obj.name = "parts-test";
    obj.kind = "character";
    obj.source = "test";
    obj.skin = triangle(0.0f, 0);
    dfn::skel::SkeletonJoint root;
    root.name = "root";
    root.parent = -1;
    obj.skeleton.joints.push_back(root);
    dfn::skel::SkeletonJoint head;
    head.name = "DEF-head";
    head.parent = 0;
    head.bind_translation = {0.0f, 1.5f, 0.0f};
    obj.skeleton.joints.push_back(head);
    return obj;
}

SkinPart hair_part() {
    SkinPart p;
    p.name = "hair";
    p.alpha_mask = true;
    p.alpha_cutoff = 0.4f;
    p.double_sided = true;
    p.mesh = triangle(10.0f, 1);
    TextureRef a;
    a.role = "albedo";
    a.path = "assets/objects/characters/textures/HumanBase/hair_albedo.png";
    a.sha256 = std::string(64, 'b');
    a.colour_space = TEXTURE_COLOUR_SRGB;
    p.textures.push_back(a);
    TextureRef n;
    n.role = "normal";
    n.path = "assets/objects/characters/textures/HumanBase/hair_normal.png";
    n.sha256 = std::string(64, 'c');
    n.colour_space = TEXTURE_COLOUR_LINEAR;
    n.wrap = TEXTURE_WRAP_CLAMP;
    p.textures.push_back(n);
    return p;
}

SkinPart suit_part() {
    SkinPart p;
    p.name = "suit";
    p.mesh = triangle(30.0f, 1);
    p.hide_body_vertices = {0, 1, 2, 7, 9};
    return p;
}

SkinPart teeth_part() {
    SkinPart p;
    p.name = "teeth";
    p.mesh = triangle(20.0f, 1);
    return p;
}

std::filesystem::path temp_file(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

} // namespace

TEST_CASE("dfo PART: набор частей переживает круговой прогон") {
    RegistryObject obj = tiny_character();
    obj.skin = SkinMesh{}; // файл набора: скелет и части, без тела
    obj.kind = "character-parts";
    obj.parts.push_back(hair_part());
    obj.parts.push_back(teeth_part());

    const auto path = temp_file("dfn_parts_roundtrip.dfo");
    REQUIRE(write_object(obj, path));
    const auto back = read_object(path);
    std::filesystem::remove(path);
    REQUIRE(back.has_value());
    CHECK(back->skin.empty());
    REQUIRE(back->skeleton.joints.size() == 2);
    REQUIRE(back->parts.size() == 2);
    const SkinPart& hair = back->parts[0];
    CHECK(hair.name == "hair");
    CHECK(hair.alpha_mask);
    CHECK(hair.alpha_cutoff == doctest::Approx(0.4f));
    CHECK(hair.double_sided);
    REQUIRE(hair.mesh.vertices.size() == 3);
    CHECK(hair.mesh.vertices[1].position.x == doctest::Approx(11.0f));
    CHECK(hair.mesh.vertices[1].joints[0] == 1);
    CHECK(hair.mesh.indices.size() == 3);
    REQUIRE(hair.textures.size() == 2);
    REQUIRE(hair.texture("albedo") != nullptr);
    CHECK(hair.texture("albedo")->sha256 == std::string(64, 'b'));
    REQUIRE(hair.texture("normal") != nullptr);
    CHECK(hair.texture("normal")->colour_space == TEXTURE_COLOUR_LINEAR);
    CHECK(hair.texture("normal")->wrap == TEXTURE_WRAP_CLAMP);
    CHECK(hair.texture("roughness") == nullptr);
    const SkinPart& teeth = back->parts[1];
    CHECK(teeth.name == "teeth");
    CHECK(!teeth.alpha_mask);
    CHECK(!teeth.double_sided);
    CHECK(teeth.textures.empty());
    CHECK(back->content_hash == object_content_hash(obj));
}

TEST_CASE("dfo PART: список закрытых вершин тела едет с частью") {
    RegistryObject obj = tiny_character();
    obj.parts.push_back(suit_part());
    RegistryObject naked = obj;
    naked.parts[0].hide_body_vertices.clear();
    CHECK(object_content_hash(obj) != object_content_hash(naked));
    const auto path = temp_file("dfn_parts_hide.dfo");
    REQUIRE(write_object(obj, path));
    const auto back = read_object(path);
    std::filesystem::remove(path);
    REQUIRE(back.has_value());
    REQUIRE(back->parts.size() == 1);
    CHECK(back->parts[0].hide_body_vertices == std::vector<std::uint32_t>{0, 1, 2, 7, 9});
}

TEST_CASE("dfo SOCK: сокеты переживают круг, входят в личность, сустав за краем — отказ") {
    RegistryObject obj = tiny_character();
    Socket ring;
    ring.name = "ring.L";
    ring.joint = 1;
    ring.rest_point = {0.12f, 1.52f, -0.03f};
    obj.sockets.push_back(ring);
    RegistryObject bare = obj;
    bare.sockets.clear();
    CHECK(object_content_hash(obj) != object_content_hash(bare));
    CHECK(object_content_hash(bare) == object_content_hash(tiny_character()));
    const auto path = temp_file("dfn_sock_roundtrip.dfo");
    REQUIRE(write_object(obj, path));
    const auto back = read_object(path);
    std::filesystem::remove(path);
    REQUIRE(back.has_value());
    REQUIRE(back->sockets.size() == 1);
    REQUIRE(back->socket("ring.L") != nullptr);
    CHECK(back->socket("ring.L")->joint == 1);
    CHECK(back->socket("ring.L")->rest_point.y == doctest::Approx(1.52f));
    CHECK(back->socket("weapon.R") == nullptr);

    RegistryObject bad = obj;
    bad.sockets[0].joint = 5;
    const auto bad_path = temp_file("dfn_sock_bad.dfo");
    REQUIRE(write_object(bad, bad_path));
    const auto refused = read_object(bad_path);
    std::filesystem::remove(bad_path);
    CHECK(!refused.has_value());
}

TEST_CASE("dfo PART: пустой список НЕ трогает личность объекта без частей") {
    const RegistryObject bare = tiny_character();
    RegistryObject with = bare;
    with.parts.push_back(hair_part());
    CHECK(object_content_hash(bare) != object_content_hash(with));
    RegistryObject emptied = with;
    emptied.parts.clear();
    CHECK(object_content_hash(emptied) == object_content_hash(bare));

    const auto p1 = temp_file("dfn_parts_bare1.dfo");
    const auto p2 = temp_file("dfn_parts_bare2.dfo");
    REQUIRE(write_object(bare, p1));
    REQUIRE(write_object(emptied, p2));
    CHECK(std::filesystem::file_size(p1) == std::filesystem::file_size(p2));
    const auto r1 = read_object(p1);
    std::filesystem::remove(p1);
    std::filesystem::remove(p2);
    REQUIRE(r1.has_value());
    CHECK(r1->parts.empty());
}

TEST_CASE("dfo PART: флаги, лист и вершины входят в личность") {
    RegistryObject a = tiny_character();
    a.parts.push_back(hair_part());
    RegistryObject b = a;
    b.parts[0].double_sided = false;
    CHECK(object_content_hash(a) != object_content_hash(b));
    RegistryObject c = a;
    c.parts[0].alpha_cutoff = 0.5f;
    CHECK(object_content_hash(a) != object_content_hash(c));
    RegistryObject d = a;
    d.parts[0].textures[0].sha256[0] = 'z';
    CHECK(object_content_hash(a) != object_content_hash(d));
    RegistryObject e = a;
    e.parts[0].mesh.vertices[0].position.y += 0.01f;
    CHECK(object_content_hash(a) != object_content_hash(e));
    RegistryObject f = a;
    f.parts[0].name = "wig";
    CHECK(object_content_hash(a) != object_content_hash(f));
}

TEST_CASE("dfo PART: сустав за краем скелета — отказ, а не полпричёски") {
    RegistryObject obj = tiny_character();
    SkinPart bad = hair_part();
    bad.mesh.vertices[0].joints[0] = 7; // скелет из двух суставов
    obj.parts.push_back(bad);
    const auto path = temp_file("dfn_parts_badjoint.dfo");
    REQUIRE(write_object(obj, path));
    const auto back = read_object(path);
    std::filesystem::remove(path);
    CHECK(!back.has_value());
}

TEST_CASE("dfo PART: усечённая секция — отказ") {
    RegistryObject obj = tiny_character();
    obj.parts.push_back(hair_part());
    const auto path = temp_file("dfn_parts_trunc.dfo");
    REQUIRE(write_object(obj, path));
    const auto size = std::filesystem::file_size(path);
    std::filesystem::resize_file(path, size - 16);
    const auto back = read_object(path);
    std::filesystem::remove(path);
    CHECK(!back.has_value());
}
