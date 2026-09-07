/*
Module: tests
File: tests/app/PhysicalFeetTests.cpp

Responsibility:
- ПРИБОР ФИЗИЧЕСКИХ СТОП (engine/app/sources/CharacterFeet.h, LOCOMOTION_GROUNDED.md
  §12): тело HumanBase стоит на склоне в мире Jolt; стопы — тела с трением.
  Гранит 5°: держат, скольжение 0, корень на месте. Стекло 30° (пара 0,387 <
  tan 30° = 0,577): стопы ползут по Кулону, якорь замка едет за телом стопы,
  корень съезжает вниз по склону. Контрольная рука: стопы выключены — на льду
  тело стоит как приклеенное (то самое, что владелец велел убрать 04.09).

Key items:
- feet_hold_on_gentle_granite
- feet_slide_on_glass_and_the_body_goes_with_them
- the_control_arm_stands_glued_on_glass

Dependencies:
- Uses: doctest, app (SkinnedCharacter, CharacterFactory, CharacterFeet),
  platform physics (Jolt), HumanBase.dfo (dfn_characters).
- Used by: ctest (app_physical_feet).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Порядок тика — как в App: advance → step → commit_root → feet.tick.
*/
#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/Rig.h"
#include "engine/app/sources/CharGenBody.h"
#include "engine/app/sources/CharacterFactory.h"
#include "engine/app/sources/CharacterFeet.h"
#include "engine/app/sources/SkinnedCharacter.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/materials/sources/PhysicsSubstance.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/RenderSystem.h"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <limits>
#include <filesystem>
#include <memory>

#include <glm/gtc/quaternion.hpp>

using namespace dfn;
namespace fs = std::filesystem;

namespace {

constexpr float DT = 1.0f / 60.0f;

struct Stand {
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::SkinnedCharacter body;
    app::CharacterBodies bodies;
    std::unique_ptr<platform::IPhysics> physics;
    app::CharacterFeet feet;
    bool ok = false;
    glm::vec3 downhill{0.0f};

    explicit Stand(float slope_deg, const char* substance, bool physical_feet) {
        app::CharacterSpec spec;
        spec.proportions = &rig;
        spec.mesh_asset = app::VIEWER_BODY_MESH_ID;
        spec.blade_asset = app::VIEWER_BLADE_MESH_ID;
        ok = app::build_character(body, bodies, rs, renderer, nullptr,
                                  fs::path(app::CHARGEN_SOURCE_BODY), spec);
        if (!ok) {
            return;
        }
        body.set_transitions(false);
        physics = platform::create_jolt_physics();
        REQUIRE(physics->init());
        // склон: поверхность через начало, наклон вокруг Z — вниз по −X
        const float theta = glm::radians(slope_deg);
        const glm::quat rot = glm::angleAxis(theta, glm::vec3{0.0f, 0.0f, 1.0f});
        platform::StaticBoxDesc desc;
        desc.half_extents = {20.0f, 0.5f, 20.0f};
        desc.center = rot * glm::vec3{0.0f, -0.5f, 0.0f};
        desc.rotation = rot;
        desc.layer = physics::LAYER_STATIC;
        desc.substance = core::find_substance(substance);
        REQUIRE(desc.substance != core::SUBSTANCE_NONE);
        desc.user_data = 7;
        REQUIRE(physics->create_static_box(desc).valid());
        downhill = glm::vec3{-std::cos(theta), -std::sin(theta), 0.0f};
        feet.set_enabled(physical_feet);
        feet.bind(physics.get(), 11);
        // ЩУП ЗЕМЛИ — ЛУЧ В ФИЗИКУ, как в игре: IK кладёт стопу по склону, и
        // тело стопы ставится без проникновения (иначе плоская стопа клипа
        // втыкается углом в склон на 2,4 см и считается вдавленной).
        platform::IPhysics* phys = physics.get();
        body.set_ground_probe([phys](const glm::vec3& at) {
            const platform::RayHit hit = phys->raycast(at + glm::vec3{0.0f, 0.5f, 0.0f},
                                                       glm::vec3{0.0f, -1.0f, 0.0f}, 2.0f,
                                                       physics::LAYER_STATIC);
            return hit.hit ? hit.position.y : std::numeric_limits<float>::quiet_NaN();
        });
    }
    ~Stand() {
        feet.shutdown();
        if (physics) {
            physics->shutdown();
        }
    }

    /// Статистика хода якорей от тел стоп: худший ход за тик, сумма, постановки.
    float worst_step_m = 0.0f;
    float total_slip_m = 0.0f;
    int plants = 0;
    int worst_tick = -1;

    /// Стоять (или идти, want > 0) N тиков; корень едет за стопами. Возвращает
    /// путь корня вниз по склону, м.
    float stand(int ticks, glm::vec3& root, float want_mps = 0.0f) {
        anim::BodyDrive drive;
        drive.grounded = true;
        drive.facing_yaw = 0.0f;
        if (want_mps > 0.0f) {
            drive.gait = anim::Gait::Walk;
            drive.speed_mps = want_mps;
            drive.want_speed_mps = want_mps;
            drive.step_length_m = 0.7f;
            drive.move_dir_model = glm::vec3{0.0f, 0.0f, -1.0f};
        }
        const glm::vec3 start = root;
        for (int i = 0; i < ticks; ++i) {
            body.advance(drive, root, DT);
            const anim::LocomotionOut& lo = body.locomotion();
            if (lo.valid) {
                root += lo.root_delta_model; // рыск 0: система тела = мир
                // корень держится на поверхности склона (y = −x·tanθ)
                root.y = -root.x * (downhill.y / downhill.x);
            }
            physics->step(DT);
            body.commit_root(drive, root, DT);
            std::array<bool, 2> was{feet.report(0).planted, feet.report(1).planted};
            feet.tick(body, DT);
            for (std::size_t side = 0; side < 2; ++side) {
                const app::FootPhysicsReport& rep = feet.report(side);
                if (rep.planted && !was[side]) {
                    ++plants;
                }
                const float step = glm::length(glm::vec2{rep.slip_delta.x, rep.slip_delta.z});
                total_slip_m += step;
                if (step > worst_step_m) {
                    worst_step_m = step;
                    worst_tick = i;
                }
            }
        }
        return glm::dot(root - start, glm::normalize(downhill));
    }
};

} // namespace

TEST_CASE("feet_hold_on_gentle_granite") {
    Stand s(5.0f, "granite", true);
    REQUIRE_MESSAGE(s.ok, "bake the character first (target dfn_characters)");
    glm::vec3 root{0.0f};
    const float path = s.stand(180, root);
    const app::FootPhysicsReport& l = s.feet.report(0);
    const app::FootPhysicsReport& r = s.feet.report(1);
    MESSAGE("гранит 5°: путь корня " << 1000.0f * path << " мм, стопы стоят " << l.planted << "/"
                                     << r.planted << ", держат " << l.holds << "/" << r.holds
                                     << ", скольжение " << l.slip_mps << "/" << r.slip_mps
                                     << " м/с, пара " << l.friction_pair);
    CHECK(l.has_body);
    CHECK(r.has_body);
    CHECK(l.planted);
    CHECK(r.planted);
    CHECK(l.holds);
    CHECK(r.holds);
    CHECK(l.slip_mps < 0.01f);
    CHECK(r.slip_mps < 0.01f);
    CHECK(std::abs(path) < 0.005f);
}

TEST_CASE("feet_slide_on_glass_and_the_body_goes_with_them") {
    Stand s(30.0f, "glass", true);
    REQUIRE(s.ok);
    glm::vec3 root{0.0f};
    const float path = s.stand(120, root);
    const app::FootPhysicsReport& l = s.feet.report(0);
    MESSAGE("стекло 30°: путь корня вниз " << path << " м за 2 с, держит " << l.holds
                                          << ", скольжение " << l.slip_mps << " м/с, пара "
                                          << l.friction_pair << ", tan " << l.slope_tan);
    CHECK(l.planted);
    CHECK_FALSE(l.holds);
    CHECK(l.slip_mps > 0.3f);
    // закон: a = g·(sinθ − μ·cosθ) = 9.81·(0.5 − 0.387·0.866) = 1.62 м/с² → за 2 с ≈ 3.2 м;
    // стопа стартует из воздуха/с задержкой постановки — требуем хотя бы четверть
    CHECK(path > 0.8f);
}

TEST_CASE("the_control_arm_stands_glued_on_glass") {
    Stand s(30.0f, "glass", false);
    REQUIRE(s.ok);
    glm::vec3 root{0.0f};
    const float path = s.stand(120, root);
    MESSAGE("контроль (DFN_PHYSICAL_FEET=0), стекло 30°: путь корня " << 1000.0f * path << " мм");
    CHECK_FALSE(s.feet.report(0).has_body);
    CHECK(std::abs(path) < 0.001f);
}

TEST_CASE("one_foot_on_a_bench_does_not_throw_the_body") {
    // Владелец 07.09 22:32: у лавки поставил одну стопу на неё и ничего не
    // делал — тело задёргало, закрутило и отбросило. Лавка — статический
    // ящик 0,45; левая стопа над ним, щуп земли — луч в физику.
    for (int arm = 1; arm >= 0; --arm) {
        Stand s(0.0f, "granite", arm == 1);
        REQUIRE(s.ok);
        platform::StaticBoxDesc bench;
        bench.half_extents = {0.6f, 0.225f, 0.25f};
        bench.center = {-0.65f, 0.225f, 0.0f}; // край ящика под левой стопой
        bench.layer = physics::LAYER_STATIC;
        bench.substance = core::find_substance("pine");
        bench.user_data = 9;
        REQUIRE(s.physics->create_static_box(bench).valid());
        glm::vec3 root{0.0f};
        s.stand(300, root);
        const float moved = glm::length(glm::vec2{root.x, root.z});
        const anim::LocoProbeRow& acc = s.body.telemetry().row(anim::LocoProbe::RootAccel);
        MESSAGE((arm ? "стопы ВКЛ" : "стопы ВЫКЛ") << ": корень ушёл на " << 1000.0f * moved
                << " мм за 5 с; худший ход якоря за тик " << 1000.0f * s.worst_step_m
                << " мм (тик " << s.worst_tick << "), сумма " << s.total_slip_m << " м; роль "
                << anim::role_name(s.body.playback().role));
        CHECK(moved < 0.05f);
        CHECK(s.worst_step_m < 0.02f);
    }
}

TEST_CASE("walking_on_granite_keeps_the_gait") {
    // ходьба по ровному граниту: с физическими стопами путь тот же, что без
    // них, скольжение поставленной стопы — нулевое (пара 0,648 держит)
    float paths[2] = {0.0f, 0.0f};
    float worst_slip = 0.0f;
    float stats_worst = 0.0f, stats_total = 0.0f;
    int stats_plants = 0, stats_tick = -1;
    for (int arm = 0; arm < 2; ++arm) {
        Stand s(0.0f, "granite", arm == 1);
        REQUIRE(s.ok);
        glm::vec3 root{0.0f};
        s.stand(300, root, 1.3f);
        paths[arm] = -root.z;
        if (arm == 1) {
            stats_worst = s.worst_step_m;
            stats_total = s.total_slip_m;
            stats_plants = s.plants;
            stats_tick = s.worst_tick;
            worst_slip = 0.0f;
            for (std::size_t side = 0; side < 2; ++side) {
                worst_slip = std::max(worst_slip, s.feet.report(side).slip_mps);
            }
        }
    }
    MESSAGE("ходьба 5 с по граниту: путь без стоп " << paths[0] << " м, со стопами " << paths[1]
                                                    << " м, скольжение в конце " << worst_slip
                                                    << " м/с; ход якорей XZ: худший за тик "
                                                    << 1000.0f * stats_worst << " мм (тик "
                                                    << stats_tick << "), сумма " << stats_total
                                                    << " м за " << stats_plants << " постановок");
    CHECK(paths[1] > 0.9f * paths[0]);
    CHECK(paths[1] < 1.1f * paths[0]);
    CHECK(worst_slip < 0.05f);
}

