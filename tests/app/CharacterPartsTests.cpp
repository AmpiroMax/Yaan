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
- ЧАСТИ СЛЕДУЮТ МОРФАМ (волна «части следуют морфам»): на ОБОИХ концах полос
  всех целей MORF тела расстояние «вершина части ↔ её опорная вершина тела»
  растёт не больше 1 мм против реста (волосы над черепом, брови над кожей,
  ресницы у век, костюм на животе); глаза, зубы и язык — жёстко: центр группы
  против центроида маски не уходит дальше 1 мм; под кожу (глубже 1 мм до
  ближайшей грани) не уходит больше вершин части, чем в ресте (+10); при
  нулевых весах части побитово равны ресту. Контрольная рука — часть, оставленная в ресте, тем же
  прибором по тем же парам вершин (правило 47): лоб +0.5 уводит череп из-под
  волос на ≥ 10 мм, eyebrows-angle отрывает брови на ≥ 5 мм, head-scale-horiz
  уводит глазницу от яблока на ≥ 3 мм. Цена follow() — миллисекунды вслух.

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
#include <map>
#include <set>
#include <string>
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

// ------------------------------------------------ ЧАСТИ СЛЕДУЮТ МОРФАМ ---

namespace {

/// РУКА БЕЗ ПЕРЕНОСА тем же прибором: часть стоит в ресте, тело ушло.
/// Наибольшее изменение расстояния до кожи в любую сторону у части, оставленной
/// в ресте: череп, ушедший ВВЕРХ сквозь волосы, читается сжатием, а не ростом.
[[nodiscard]] float control_gap_change(const app::CharacterParts& parts,
                                       const app::AttachedPart& p,
                                       std::span<const platform::SkinnedVertex> body_now) {
    float grow = 0.0f;
    float shrink = 0.0f;
    render::follow_gap_change(parts.neutral(), parts.neutral_indices(), body_now, p.map,
                              p.rest, p.rest, grow, shrink);
    return std::max(grow, shrink);
}
[[nodiscard]] float control_vertex_gap(const app::CharacterParts& parts,
                                       const app::AttachedPart& p,
                                       std::span<const platform::SkinnedVertex> body_now) {
    return render::follow_vertex_gap_error(parts.neutral(), body_now, p.map, p.rest, p.rest);
}
[[nodiscard]] std::size_t control_under_skin(const app::CharacterParts& parts,
                                             const app::AttachedPart& p,
                                             std::span<const platform::SkinnedVertex> body_now) {
    return render::follow_penetrations(body_now, parts.neutral_indices(), p.map, p.rest,
                                       0.001f);
}
/// Сдвиг центроида маски жёсткой группы против реста (глазница уехала).
[[nodiscard]] float control_rigid_shift(const app::CharacterParts& parts,
                                        const app::AttachedPart& p,
                                        std::span<const platform::SkinnedVertex> body_now) {
    float worst = 0.0f;
    for (const app::RigidGroup& g : p.rigid) {
        const render::RigidFrame now = render::rigid_frame(body_now, g.mask);
        worst = std::max(worst, glm::length(now.centroid - g.rest.centroid));
    }
    return worst;
}
[[nodiscard]] const app::AttachedPart* part_named(const app::CharacterParts& parts,
                                                  std::string_view name) {
    for (const app::AttachedPart& p : parts.parts()) {
        if (p.name == name) {
            return &p;
        }
    }
    return nullptr;
}

constexpr float FOLLOW_TOL_M = 0.001f;

} // namespace

TEST_CASE("части следуют морфам: на краях полос всех целей часть не отстаёт от тела дальше 1 мм; контроль в ресте отстаёт") {
    if (!present(BODY) || !present(PARTS) || !present(CLOTHES)) {
        MESSAGE("no baked files -- skipped");
        return;
    }
    REQUIRE(app::parts_follow_door());
    Harness h;
    REQUIRE(h.ok);
    REQUIRE(!h.body.morphs().empty());
    REQUIRE(h.attach(PARTS));
    REQUIRE(h.attach(CLOTHES));
    const app::CharacterParts& parts = h.body.parts();
    REQUIRE(parts.following());
    REQUIRE(parts.neutral().size() == h.body.current_vertices().size());
    // Кто как следует: глаза/зубы/язык жёстко (маски есть), остальное переносом.
    for (const app::AttachedPart& p : parts.parts()) {
        INFO(p.name);
        CHECK(p.follow != app::PartFollow::None);
        CHECK(p.map.binds.size() == p.rest.size());
        if (p.name == "eyes" || p.name == "teeth" || p.name == "tongue") {
            CHECK(p.follow == app::PartFollow::Rigid);
            CHECK(!p.rigid.empty());
        } else {
            CHECK(p.follow == app::PartFollow::Transfer);
        }
        if (p.name == "eyes") {
            CHECK(p.rigid.size() == 2);
            CHECK(p.rigid[0].scale);
        }
    }
    // Ноль — рест побитово (веса нули, тело = нейтраль).
    REQUIRE(h.body.apply_morphs(h.rs, h.renderer));
    for (const app::AttachedPart& p : parts.parts()) {
        INFO(p.name);
        REQUIRE(p.now.size() == p.rest.size());
        bool same = true;
        for (std::size_t i = 0; i < p.now.size(); ++i) {
            same = same && p.now[i].position == p.rest[i].position
                   && p.now[i].normal == p.rest[i].normal;
        }
        CHECK(same);
    }
    // Оба конца полосы каждой цели. Худшее — по каждой части отдельно:
    // порог у волос и костюма один и тот же, а места разные.
    struct Worst {
        float gap = 0.0f;
        std::string gap_at;
        float shrink = 0.0f;
        std::string shrink_at;
        float vgap = 0.0f;
        std::string vgap_at;
        float rigid = 0.0f;
        std::string rigid_at;
        std::size_t under_rest = 0;
        std::size_t under = 0;
        std::string under_at;
        std::size_t vertices = 0;
    };
    std::map<std::string, Worst> worst;
    float control_hair_forehead = 0.0f;
    std::size_t control_hair_under = 0;
    std::size_t hair_under_rest = 0;
    float control_brows_angle = 0.0f;
    float control_brows_vertex = 0.0f;
    float control_eyes_horiz = 0.0f;
    double ms_sum = 0.0;
    double ms_max = 0.0;
    std::size_t ends = 0;
    const auto& targets = h.body.morphs();
    for (std::size_t t = 0; t < targets.size(); ++t) {
        for (const float w : {targets[t].lo, targets[t].hi}) {
            if (w == 0.0f) {
                continue;
            }
            h.body.reset_morphs();
            h.body.set_morph_weight(t, w);
            REQUIRE(h.body.apply_morphs(h.rs, h.renderer));
            ++ends;
            ms_sum += parts.last_follow_ms();
            ms_max = std::max(ms_max, parts.last_follow_ms());
            const auto& now = h.body.current_vertices();
            const std::string tag = targets[t].name + (w > 0 ? " hi" : " lo");
            for (const app::PartFollowReport& r : parts.follow_report(now)) {
                Worst& w = worst[r.name];
                w.vertices = r.vertices;
                w.under_rest = r.under_skin_rest;
                if (r.follow == app::PartFollow::Transfer && r.gap_grow_m > w.gap) {
                    w.gap = r.gap_grow_m;
                    w.gap_at = tag;
                }
                if (r.follow == app::PartFollow::Transfer && r.gap_shrink_m > w.shrink) {
                    w.shrink = r.gap_shrink_m;
                    w.shrink_at = tag;
                }
                if (r.follow == app::PartFollow::Transfer && r.vertex_gap_error_m > w.vgap) {
                    w.vgap = r.vertex_gap_error_m;
                    w.vgap_at = tag;
                }
                if (r.follow == app::PartFollow::Rigid && r.rigid_offset_m > w.rigid) {
                    w.rigid = r.rigid_offset_m;
                    w.rigid_at = tag;
                }
                if (r.under_skin_now > w.under) {
                    w.under = r.under_skin_now;
                    w.under_at = tag;
                }
            }
            if (targets[t].name == "forehead-scale-vert" && w > 0) {
                const app::AttachedPart& hair = *part_named(parts, "hair");
                control_hair_forehead = control_gap_change(parts, hair, now);
                control_hair_under = control_under_skin(parts, hair, now);
                hair_under_rest = worst["hair"].under_rest;
            }
            if (targets[t].name == "eyebrows-angle") {
                control_brows_angle = std::max(
                    control_brows_angle,
                    control_gap_change(parts, *part_named(parts, "eyebrows"), now));
                control_brows_vertex = std::max(
                    control_brows_vertex,
                    control_vertex_gap(parts, *part_named(parts, "eyebrows"), now));
            }
            if (targets[t].name == "head-scale-horiz") {
                control_eyes_horiz = std::max(
                    control_eyes_horiz, control_rigid_shift(parts, *part_named(parts, "eyes"), now));
            }
        }
    }
    MESSAGE("band ends " << ends << "; follow " << ms_sum / std::max<std::size_t>(ends, 1)
            << " ms mean, " << ms_max << " ms max");
    for (const auto& [name, w] : worst) {
        MESSAGE(name << " (" << w.vertices << " verts): worst skin gap GROWTH " << w.gap * 1000.0f
                     << " mm (" << w.gap_at << "), shrink " << w.shrink * 1000.0f << " mm ("
                     << w.shrink_at << "), |d| to nearest vertex " << w.vgap * 1000.0f
                     << " mm (" << w.vgap_at << "), worst rigid offset " << w.rigid * 1000.0f
                     << " mm (" << w.rigid_at << "), under skin >1 mm: rest " << w.under_rest
                     << ", worst now " << w.under << " (" << w.under_at << ")");
        INFO(name);
        CHECK(w.gap <= FOLLOW_TOL_M);
        CHECK(w.rigid <= FOLLOW_TOL_M);
        // Просвечивание: под кожу не уходит больше вершин, чем в ресте, плюс
        // десять (0.1 % костюма) на вершины, лежащие на самой коже. Только у
        // переносимых частей: яблоко под веками и зубы за губами лежат «под
        // кожей» по построению, и губы, сжатые ручкой, обязаны налезть на зубы.
        if (part_named(parts, name)->follow == app::PartFollow::Transfer) {
            CHECK(w.under <= w.under_rest + 10);
        }
    }
    MESSAGE("control (parts left in rest): hair under forehead-scale-vert hi "
            << control_hair_forehead * 1000.0f << " mm from skin, under skin "
            << control_hair_under << " of " << worst["hair"].vertices << " (rest "
            << hair_under_rest << "), eyebrows under eyebrows-angle "
            << control_brows_angle * 1000.0f << " mm from skin / " << control_brows_vertex * 1000.0f
            << " mm from nearest vertex, eye socket under head-scale-horiz "
            << control_eyes_horiz * 1000.0f << " mm");
    CHECK(ends >= 90);
    CHECK(worst.size() == 8);
    // Контроль: без переноса те же ручки рвут части (числа отчёта лица: 13.7 / 6.1 / 4.4
    // — там мерился край области, здесь пара «вершина части ↔ вершина тела» и центроид).
    CHECK(control_hair_forehead >= 0.010f);
    CHECK(control_hair_under >= hair_under_rest + 40);
    CHECK(control_brows_angle >= 0.002f);
    CHECK(control_brows_vertex >= 0.005f);
    CHECK(control_eyes_horiz >= 0.002f);
    CHECK(ms_max < 20.0);
}

TEST_CASE("одежда следует телу: на belly/weight/muscle max под кожу не уходит больше вершин костюма, чем в ресте; контроль в ресте — тело сквозь костюм") {
    if (!present(BODY) || !present(CLOTHES)) {
        MESSAGE("no baked files -- skipped");
        return;
    }
    Harness h;
    REQUIRE(h.ok);
    REQUIRE(h.attach(CLOTHES));
    const app::CharacterParts& parts = h.body.parts();
    const app::AttachedPart* suit = part_named(parts, "male_casualsuit01");
    REQUIRE(suit != nullptr);
    CHECK(suit->follow == app::PartFollow::Transfer);
    const std::vector<uint32_t> hidden_before = parts.hidden_body_vertices();
    std::size_t under_rest = 0;
    std::size_t worst_under = 0;
    std::size_t worst_control = 0;
    float worst_gap = 0.0f;
    for (const char* name : {"belly", "weight", "muscle"}) {
        const int slot = render::morph_index(h.body.morphs(), name);
        if (slot < 0) {
            MESSAGE("no target " << name);
            continue;
        }
        h.body.reset_morphs();
        h.body.set_morph_weight(static_cast<std::size_t>(slot), h.body.morphs()[slot].hi);
        REQUIRE(h.body.apply_morphs(h.rs, h.renderer));
        const auto& now = h.body.current_vertices();
        std::size_t under = 0;
        float gap = 0.0f;
        for (const app::PartFollowReport& r : parts.follow_report(now)) {
            if (r.name == suit->name) {
                under = r.under_skin_now;
                under_rest = r.under_skin_rest;
                gap = r.gap_grow_m;
            }
        }
        const std::size_t control = control_under_skin(parts, *suit, now);
        worst_under = std::max(worst_under, under);
        worst_control = std::max(worst_control, control);
        worst_gap = std::max(worst_gap, gap);
        MESSAGE(std::string(name) << " hi: suit vertices under skin >1 mm: rest " << under_rest
                                  << ", now " << under << ", control (suit left in rest) "
                                  << control << " of " << suit->rest.size() << "; skin gap growth "
                                  << gap * 1000.0f << " mm; follow " << parts.last_follow_ms()
                                  << " ms");
    }
    CHECK(worst_under <= under_rest + 10);
    CHECK(worst_gap <= FOLLOW_TOL_M);
    CHECK(worst_control >= under_rest + 40);
    // Список закрытых вершин тела морф не меняет.
    CHECK(parts.hidden_body_vertices() == hidden_before);
}

TEST_CASE("части следуют морфам на масштабированном теле: нейтраль в масштабе частей, ноль = рест") {
    if (!present(BODY) || !present(PARTS)) {
        MESSAGE("no baked files -- skipped");
        return;
    }
    Harness h(BODY, false, 1.08f);
    REQUIRE(h.ok);
    REQUIRE(h.attach(PARTS));
    const app::CharacterParts& parts = h.body.parts();
    REQUIRE(parts.following());
    REQUIRE(parts.neutral().size() == h.body.current_vertices().size());
    // Нейтраль ×1.08 совпадает с телом ×1.08 — нигде дальше 1e-5 м.
    float worst = 0.0f;
    for (std::size_t i = 0; i < parts.neutral().size(); ++i) {
        worst = std::max(worst, glm::length(parts.neutral()[i].position
                                            - h.body.current_vertices()[i].position));
    }
    CHECK(worst < 1e-5f);
    for (const app::PartFollowReport& r : parts.follow_report(h.body.current_vertices())) {
        CHECK(r.gap_grow_m < 1e-5f);
        CHECK(r.rigid_offset_m < 1e-5f);
    }
}

TEST_CASE("маски лица: файл читается, области глаз и рта есть, число вершин — тела") {
    app::FaceMasks masks;
    if (!present(app::FACE_MASKS_PATH)) {
        MESSAGE("no face.masks -- skipped");
        return;
    }
    REQUIRE(app::read_face_masks(app::FACE_MASKS_PATH, masks));
    CHECK(masks.verts == 14517);
    for (const char* name : {"eye-l", "eye-r", "mouth-angles", "lip-upper", "lip-lower"}) {
        INFO(name);
        const auto* m = masks.find(name);
        REQUIRE(m != nullptr);
        CHECK(m->size() > 100);
        CHECK(m->back() < masks.verts);
    }
    CHECK(masks.find("no-such-mask") == nullptr);
    app::FaceMasks none;
    CHECK(!app::read_face_masks("assets/characters/targets/no-such.masks", none));
    CHECK(none.empty());
}
