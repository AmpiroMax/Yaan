/*
Module: tests/app
File: tests/app/CharacterPartsTests.cpp

Responsibility:
- ЧАСТИ НА ТЕЛЕ (волна «части персонажа») на нулевом бэкенде: набор PART
  (HumanBase.parts.dfo) крепится к телу фабрики — шесть частей под номерами
  полосы хозяина, листы подняты, масштаб по костям равен единице на том же
  теле и равен множителю роста на масштабированном; выбор по имени дозой
  («hair,eyes» — две, «none» — ни одной); чужой скелет (Knight) — отказ;
  одежда (HumanBase.clothes.dfo) снимает с тела РОВНО те треугольники, все
  три вершины которых закрыты; сокет (секция SOCK) едет с костью в клипе:
  расстояние «точка сокета — сустав» постоянно, а сама точка движется.
  Контрольные руки (правило 30): «none» и Knight обязаны дать ноль.

Dependencies:
- Uses: engine/app CharacterFactory, SkinnedCharacter, CharacterParts,
  engine/anim Rig/Body, engine/platform/render null backend, engine/render
  RenderSystem/ObjectRegistry, doctest.
- Used by: ctest (app_character_parts).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Файлы берутся НАСТОЯЩИЕ (цель dfn_characters); без них набор пропускается
  вслух, а не зеленеет.
*/

#include "engine/app/sources/CharacterFactory.h"
#include "engine/app/sources/CharacterParts.h"
#include "engine/app/sources/CharacterTextures.h"
#include "engine/app/sources/SkinnedCharacter.h"
#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/Rig.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/RenderSystem.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace dfn;

namespace {

namespace fs = std::filesystem;
constexpr float DT = static_cast<float>(config::SIM_DT);

const char* BODY = "assets/objects/characters/HumanBase.dfo";
const char* PARTS = app::DEFAULT_CHARACTER_PARTS;
const char* CLOTHES = app::DEFAULT_CHARACTER_CLOTHES;
const char* KNIGHT = "assets/objects/characters/Knight.dfo";

[[nodiscard]] bool present(const char* path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

/// Тело фабрикой БЕЗ автоматического крепления: набор ставится в тесте
/// руками, чтобы каждый attach был предметом проверки.
struct Harness {
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::SkinnedCharacter body;
    app::CharacterBodies bodies;
    bool ok = false;

    explicit Harness(const char* path = BODY, bool auto_parts = false, float scale = 1.0f) {
        app::CharacterSpec spec;
        spec.proportions = &rig;
        spec.mesh_asset = app::VIEWER_BODY_MESH_ID;
        spec.blade_asset = app::VIEWER_BLADE_MESH_ID;
        spec.parts_mesh_first = app::VIEWER_PARTS_MESH_ID_FIRST;
        spec.attach_parts = auto_parts;
        if (std::fabs(scale - 1.0f) > 1e-6f) {
            auto obj = render::read_object(path);
            REQUIRE(obj.has_value());
            app::scale_registry_object(*obj, scale);
            ok = app::build_character_object(body, bodies, rs, renderer, nullptr, *obj,
                                             fs::path(path), spec);
        } else {
            ok = app::build_character(body, bodies, rs, renderer, nullptr, fs::path(path),
                                      spec);
        }
    }
    ~Harness() {
        if (ok) {
            app::release_character(body, bodies, rs, renderer, nullptr);
        }
    }
    [[nodiscard]] bool attach(const char* path, const char* selection = nullptr) {
        return body.attach_parts(rs, renderer, fs::path(path), app::VIEWER_PARTS_MESH_ID_FIRST,
                                 app::CHARACTER_PARTS_MAX, selection);
    }
};

} // namespace

TEST_CASE("части: набор HumanBase.parts крепится к телу — шесть частей, полоса номеров, масштаб 1") {
    if (!present(BODY) || !present(PARTS)) {
        MESSAGE("no baked HumanBase.dfo / HumanBase.parts.dfo -- skipped");
        return;
    }
    Harness h;
    REQUIRE(h.ok);
    const uint32_t meshes_before = h.renderer.live_meshes();
    const std::size_t sheets_before = app::body_textures_loaded();
    REQUIRE(h.attach(PARTS));
    const auto& parts = h.body.parts().parts();
    REQUIRE(parts.size() == 6);
    CHECK(h.renderer.live_meshes() == meshes_before + 6);
    // Каждой части — свой номер из полосы хозяина, по порядку.
    std::set<std::string> names;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        CHECK(parts[i].mesh_asset == app::VIEWER_PARTS_MESH_ID_FIRST + i);
        CHECK(parts[i].triangles > 0);
        names.insert(parts[i].name);
    }
    CHECK(names == std::set<std::string>{"eyebrows", "eyelashes", "eyes", "hair", "teeth",
                                         "tongue"});
    // Листы: у каждой части альбедо, у волос ещё и нормаль; листы поднялись.
    for (const app::AttachedPart& p : parts) {
        CHECK(p.texture_asset != 0);
        if (p.name == "hair") {
            CHECK(p.normal_asset != 0);
            CHECK(p.cutout);
            CHECK(p.two_sided);
            CHECK(p.alpha_cutoff == doctest::Approx(0.5f));
        }
        if (p.name == "teeth" || p.name == "tongue") {
            CHECK(!p.cutout);
            CHECK(!p.two_sided);
        }
        CHECK(p.hide_body_vertices.empty());
    }
    CHECK(app::body_textures_loaded() > sheets_before);
    // Та же выпечка, тот же масштаб: множитель по костям — единица.
    CHECK(h.body.parts().last_scale() == doctest::Approx(1.0f).epsilon(1e-4));
    // Ничего не закрыто — тело рисуется полным списком.
    CHECK(h.body.draw_indices().size() == h.body.skin_indices().size());
    // Дро частей едут на палитре и матрице тела.
    std::vector<render::RenderSystem::SkinnedDraw> draws;
    draws.push_back(h.body.build_draw(false, 1.0f));
    h.body.part_draws(draws[0], draws);
    REQUIRE(draws.size() == 7);
    for (std::size_t i = 1; i < draws.size(); ++i) {
        CHECK(draws[i].palette.data() == draws[0].palette.data());
        CHECK(draws[i].transform == draws[0].transform);
        CHECK(draws[i].mesh_asset == parts[i - 1].mesh_asset);
    }
    // Снятие возвращает номера.
    h.body.release(h.rs, h.renderer);
    h.ok = false;
    CHECK(h.renderer.live_meshes() == meshes_before - 2); // тело и клинок ушли тоже
}

TEST_CASE("части: выбор по имени — «hair,eyes» две, «none» ни одной (контрольная рука)") {
    if (!present(BODY) || !present(PARTS)) {
        MESSAGE("no baked files -- skipped");
        return;
    }
    {
        Harness h;
        REQUIRE(h.ok);
        REQUIRE(h.attach(PARTS, "hair,eyes"));
        const auto& parts = h.body.parts().parts();
        REQUIRE(parts.size() == 2);
        CHECK(parts[0].name == "eyes"); // порядок файла, не дозы
        CHECK(parts[1].name == "hair");
    }
    {
        Harness h;
        REQUIRE(h.ok);
        const uint32_t before = h.renderer.live_meshes();
        CHECK(!h.attach(PARTS, "none"));
        CHECK(h.body.parts().empty());
        CHECK(h.renderer.live_meshes() == before);
    }
    CHECK(app::part_selected(nullptr, "hair"));
    CHECK(app::part_selected("", "hair"));
    CHECK(!app::part_selected("none", "hair"));
    CHECK(!app::part_selected("0", "hair"));
    CHECK(app::part_selected("eyes,hair", "hair"));
    CHECK(!app::part_selected("eyes,hairs", "hair"));
}

TEST_CASE("части: масштаб по костям равен множителю роста тела") {
    if (!present(BODY) || !present(PARTS)) {
        MESSAGE("no baked files -- skipped");
        return;
    }
    Harness h(BODY, false, 1.08f);
    REQUIRE(h.ok);
    REQUIRE(h.attach(PARTS));
    CHECK(h.body.parts().last_scale() == doctest::Approx(1.08f).epsilon(1e-3));
}

TEST_CASE("части: чужой скелет — отказ (контрольная рука)") {
    if (!present(KNIGHT) || !present(PARTS)) {
        MESSAGE("no baked Knight.dfo -- skipped");
        return;
    }
    Harness h(KNIGHT);
    if (!h.ok) {
        MESSAGE("Knight did not build as a character -- skipped");
        return;
    }
    const uint32_t before = h.renderer.live_meshes();
    CHECK(!h.attach(PARTS));
    CHECK(h.body.parts().empty());
    CHECK(h.renderer.live_meshes() == before);
}

TEST_CASE("одежда: снимаются ровно треугольники тела, закрытые всеми тремя вершинами") {
    if (!present(BODY) || !present(CLOTHES)) {
        MESSAGE("no baked HumanBase.clothes.dfo -- skipped");
        return;
    }
    Harness h;
    REQUIRE(h.ok);
    const std::vector<uint32_t> full = h.body.skin_indices();
    REQUIRE(h.attach(CLOTHES));
    const auto& parts = h.body.parts().parts();
    REQUIRE(parts.size() == 2);
    const std::vector<uint32_t> hidden = h.body.parts().hidden_body_vertices();
    CHECK(hidden.size() > 3000);
    CHECK(hidden.size() <= parts[0].hide_body_vertices.size()
                               + parts[1].hide_body_vertices.size());
    // Пересчёт независимой арифметикой: сколько треугольников закрыто целиком.
    std::size_t covered = 0;
    std::size_t touched = 0;
    for (std::size_t t = 0; t + 2 < full.size(); t += 3) {
        int in = 0;
        for (int k = 0; k < 3; ++k) {
            in += std::binary_search(hidden.begin(), hidden.end(), full[t + k]) ? 1 : 0;
        }
        covered += in == 3 ? 1 : 0;
        touched += in > 0 && in < 3 ? 1 : 0;
    }
    CHECK(covered > 0);
    CHECK(touched > 0); // кромка костюма существует и остаётся на теле
    CHECK(h.body.drawn_triangles() == full.size() / 3 - covered);
    CHECK(h.body.draw_indices().size() == full.size() - covered * 3);
    // Ни один оставшийся треугольник не закрыт целиком.
    const std::vector<uint32_t>& drawn = h.body.draw_indices();
    for (std::size_t t = 0; t + 2 < drawn.size(); t += 3) {
        const bool all = std::binary_search(hidden.begin(), hidden.end(), drawn[t])
                         && std::binary_search(hidden.begin(), hidden.end(), drawn[t + 1])
                         && std::binary_search(hidden.begin(), hidden.end(), drawn[t + 2]);
        if (all) {
            FAIL("a fully covered triangle is still drawn");
        }
    }
    // Части и одежда вместе — восемь, полоса хозяина заполнена ровно.
    if (present(PARTS)) {
        REQUIRE(h.attach(PARTS));
        CHECK(h.body.parts().parts().size() == 8);
    }
}

TEST_CASE("фабрика: части и одежда крепятся сами, когда наборы лежат рядом с телом") {
    if (!present(BODY) || !present(PARTS) || !present(CLOTHES)) {
        MESSAGE("no baked files -- skipped");
        return;
    }
    Harness h(BODY, /*auto_parts=*/true);
    REQUIRE(h.ok);
    CHECK(h.body.parts().parts().size() == 8);
    CHECK(h.body.drawn_triangles() < h.body.triangle_count());
}

TEST_CASE("сокет едет с костью: расстояние до сустава постоянно, точка движется") {
    if (!present(BODY)) {
        MESSAGE("no baked HumanBase.dfo -- skipped");
        return;
    }
    Harness h;
    REQUIRE(h.ok);
    REQUIRE(h.body.sockets().size() == 12);
    const render::Socket* weapon = nullptr;
    for (const render::Socket& s : h.body.sockets()) {
        if (s.name == "weapon.R") {
            weapon = &s;
        }
    }
    REQUIRE(weapon != nullptr);
    CHECK(h.body.skeleton().joints[weapon->joint].name == "DEF-hand.R");
    // Покой: рамка стоит в точке покоя, палитра — привязка.
    glm::mat4 rest{1.0f};
    REQUIRE(h.body.socket_frame("weapon.R", rest));
    CHECK(glm::length(glm::vec3{rest[3]} - weapon->rest_point) < 1e-5f);
    glm::mat4 none{1.0f};
    CHECK(!h.body.socket_frame("no-such-socket", none));

    const auto bone_bind = glm::vec3{
        glm::inverse(h.body.skeleton().joints[weapon->joint].inverse_bind)[3]};
    const float rest_distance = glm::length(weapon->rest_point - bone_bind);

    anim::BodyDrive drive;
    drive.gait = anim::Gait::Walk;
    drive.speed_mps = static_cast<float>(config::WALK_SPEED);
    drive.want_speed_mps = drive.speed_mps;
    drive.step_length_m = static_cast<float>(config::STEP_LENGTH_BASE)
                          + static_cast<float>(config::STEP_LENGTH_PER_MPS) * drive.speed_mps;
    drive.facing_yaw = 0.0f;
    glm::vec3 root{0.0f};
    glm::vec3 prev_point{0.0f};
    float travelled = 0.0f;
    float worst = 0.0f;
    for (int tick = 0; tick < 60; ++tick) {
        h.body.advance(drive, root, DT);
        h.body.commit_root(drive, root, DT);
        const render::RenderSystem::SkinnedDraw d = h.body.build_draw(false, 1.0f);
        glm::mat4 frame{1.0f};
        REQUIRE(h.body.socket_frame("weapon.R", frame));
        const glm::vec3 point{frame[3]};
        const glm::vec3 bone{
            (d.palette[weapon->joint]
             * glm::inverse(h.body.skeleton().joints[weapon->joint].inverse_bind))[3]};
        worst = std::max(worst, std::fabs(glm::length(point - bone) - rest_distance));
        if (tick > 0) {
            travelled += glm::length(point - prev_point);
        }
        prev_point = point;
    }
    MESSAGE("socket weapon.R: worst |distance - rest| " << worst << " m, travelled "
            << travelled << " m over 60 ticks");
    CHECK(worst < 1e-4f);     // сокет пришит к кости жёстко
    CHECK(travelled > 0.01f); // и кисть в ходьбе движется — точка едет с ней
}
