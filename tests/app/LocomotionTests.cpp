/*
Module: tests
File: tests/app/LocomotionTests.cpp

Responsibility:
- ПРИБОРЫ НОВОЙ ЛОКОМОЦИИ (LOCOMOTION_GROUNDED.md §16): тело HumanBase на
  пути игрока с дорожкой корня и машиной состояний (SkinnedCharacter,
  DFN_ROOT_TRACK). Фаза 3: ход вперёд идёт из дорожки со скоростью клипа в
  полосе темпа и по курсу корпуса; поворот к камере — одним клипом, корпус
  доворачивается; контрольная рука — прежний путь из того же тела.
  Дальше (фазы 4–7): опорная стопа неподвижна к земле, поза правдоподобна,
  рыск непрерывен, переходов в секунду ≤ бюджета, скорость капсулы =
  клипу, прогон записанного ввода.

Key items:
- forward_walk_comes_from_the_root_track (контроль — прежний путь)
- the_camera_turn_is_one_clip_and_the_body_arrives

Dependencies:
- Uses: doctest, app (SkinnedCharacter, CharacterFactory), anim, NullRenderer,
  HumanBase.dfo (dfn_characters).
- Used by: ctest (app_locomotion).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Порядок тика — как в App: advance → (сим) → commit_root; корень ведётся
  ровно заявкой (вербатим), как будет в фазе 4.
*/
#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/Locomotion.h"
#include "engine/anim/sources/Rig.h"
#include "engine/app/sources/CharGenBody.h"
#include "engine/app/sources/CharacterFactory.h"
#include "engine/app/sources/SkinnedCharacter.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/RenderSystem.h"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using namespace dfn;
namespace fs = std::filesystem;

namespace {

constexpr float DT = static_cast<float>(config::SIM_DT);

struct Harness {
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::SkinnedCharacter body;
    app::CharacterBodies bodies;
    bool ok = false;

    explicit Harness(bool root_track = true) {
        app::CharacterSpec spec;
        spec.proportions = &rig;
        spec.mesh_asset = app::VIEWER_BODY_MESH_ID;
        spec.blade_asset = app::VIEWER_BLADE_MESH_ID;
        ok = app::build_character(body, bodies, rs, renderer, nullptr,
                                  fs::path(app::CHARGEN_SOURCE_BODY), spec);
        if (ok) {
            body.set_root_track(root_track);
        }
    }
};

struct Run {
    glm::vec3 root{0.0f};
    float body_yaw = 0.0f;
    std::vector<std::string> roles;
    float peak_mps = 0.0f;
};

/// Путь игрока: ввод вперёд `hold` тиков, камера на `view_yaw`, всего `total`.
Run run(Harness& h, float speed, anim::Gait gait, uint32_t hold, uint32_t total, float view_yaw) {
    Run out;
    anim::BodyDrive drive;
    drive.grounded = true;
    drive.gait = gait;
    drive.view_valid = true;
    drive.view_yaw = view_yaw;
    for (uint32_t t = 0; t < total; ++t) {
        const bool input = t < hold;
        drive.want_speed_mps = input ? speed : 0.0f;
        drive.speed_mps = input ? speed : 0.0f;
        drive.step_length_m = 0.7f;
        drive.move_dir_model = glm::vec3{0.0f, 0.0f, -1.0f};
        drive.facing_yaw = out.body_yaw;
        h.body.advance(drive, out.root, DT);
        const anim::LocomotionOut& lo = h.body.locomotion();
        if (lo.valid) {
            out.body_yaw += lo.root_yaw_delta;
            // заявка вербатим, повёрнутая рыском корпуса (мир = R(−рыск)·тело)
            const glm::vec3 w = glm::vec3{glm::rotate(glm::mat4{1.0f}, -out.body_yaw,
                                                      glm::vec3{0.0f, 1.0f, 0.0f})
                                          * glm::vec4{lo.root_delta_model, 0.0f}};
            out.root += w;
            out.peak_mps = std::max(out.peak_mps, glm::length(glm::vec2{w.x, w.z}) / DT);
        }
        h.body.commit_root(drive, out.root, DT);
        const std::string name{anim::role_name(h.body.playback().role)};
        if (out.roles.empty() || out.roles.back() != name) {
            out.roles.push_back(name);
        }
    }
    return out;
}

std::string chain(const Run& r) {
    std::string s;
    for (const std::string& x : r.roles) {
        s += x + " ";
    }
    return s;
}

} // namespace

TEST_CASE("forward_walk_comes_from_the_root_track") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        MESSAGE("no baked body -- skipped");
        return;
    }
    Harness h;
    REQUIRE(h.ok);
    const float speed = static_cast<float>(config::WALK_SPEED);
    const Run r = run(h, speed, anim::Gait::Walk, 300, 360, 0.0f);
    const anim::ClipEntry& walk = anim::entry_for(h.body.clip_library(), anim::ClipRole::Walk, 0);
    const float band = static_cast<float>(config::LOCOMOTION_TEMPO_BAND);
    const float expect = walk.root.mps * std::clamp(speed / walk.root.mps, 1.0f - band, 1.0f + band);
    MESSAGE("дорожка: роли " << chain(r) << "| путь " << glm::length(glm::vec2{r.root.x, r.root.z})
                              << " м за 6 с (5 с ввода), пик " << r.peak_mps << " м/с; клип "
                              << walk.root.mps << " м/с → в полосе " << expect << "; курс z "
                              << r.root.z << " x " << r.root.x << "; рыск " << glm::degrees(r.body_yaw));
    // ввод с первого тика: старт → цикл → остановка → покой; машина вела клипы
    CHECK(chain(r) == "StartWalk Walk StopWalk Idle ");
    CHECK(h.body.loco_machine().state == anim::LocoState::Idle);
    CHECK(h.body.loco_machine().transitions >= 3);
    // вперёд — это −Z, без бокового сноса и без рыска
    CHECK(r.root.z < -3.0f);
    CHECK(std::abs(r.root.x) < 0.15f);
    CHECK(std::abs(r.body_yaw) < 1.0e-3f);
    // 5 с ввода: старт (≤ START_CLIP_MAX_S) + цикл на скорости клипа в полосе
    CHECK(glm::length(glm::vec2{r.root.x, r.root.z}) > 0.8f * expect * 4.5f);
    // КОНТРОЛЬНАЯ РУКА: прежний путь из того же тела — другой механизм, тот же порядок величин
    Harness old(false);
    REQUIRE(old.ok);
    const Run o = run(old, speed, anim::Gait::Walk, 300, 360, 0.0f);
    MESSAGE("прежний путь: роли " << chain(o) << "| путь " << glm::length(glm::vec2{o.root.x, o.root.z}) << " м");
    CHECK(o.root.z < -3.0f);
    CHECK(old.body.loco_machine().transitions == 0); // машина не работала
}

TEST_CASE("the_camera_turn_is_one_clip_and_the_body_arrives") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    for (const float view : {glm::radians(90.0f), glm::radians(-90.0f), glm::radians(170.0f)}) {
        Harness h;
        REQUIRE(h.ok);
        const Run r = run(h, 0.0f, anim::Gait::Walk, 0, 240, view);
        uint32_t turns = 0;
        for (const std::string& x : r.roles) {
            if (x.rfind("Turn", 0) == 0) {
                ++turns;
            }
        }
        MESSAGE("камера " << glm::degrees(view) << "°: роли " << chain(r) << "| корпус "
                          << glm::degrees(r.body_yaw) << "°, поворотов " << turns << ", путь "
                          << 1000.0f * glm::length(glm::vec2{r.root.x, r.root.z}) << " мм");
        CHECK(turns >= 1);
        CHECK(turns <= 2);
        CHECK(std::abs(std::atan2(std::sin(view - r.body_yaw), std::cos(view - r.body_yaw)))
              < glm::radians(static_cast<float>(config::TURN_FIRE_DEG)));
        CHECK(glm::length(glm::vec2{r.root.x, r.root.z}) < 0.05f); // на месте
    }
}
