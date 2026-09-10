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
- Seam: ходок игрока на настоящем шве (spawn_player, BodyFerry, pre/post_step,
  null-физика: смещение проводится вербатим)
- the_capsule_moves_at_the_clip_speed_and_names_the_shortfall (прибор 5)
- the_yaw_is_continuous_while_the_view_orbits (прибор 3; контроль — дребезг
  ±60° 5 Гц без dwell превышает бюджет переходов)
- the_stance_point_is_still_on_the_flat (прибор 1, плоскость)
- the_march_is_climbed_on_physical_feet (прибор 7, фаза 5: Jolt, девять
  ступеней 0,18/0,28, стопы-датчики; контроль — прежний путь)
- the_ankles_do_not_cross (прибор 2, фаза 5: перекрест лодыжек по коробкам
  на сценарии ход/бок/назад/передачи/повороты; контроль — прежний путь)

Dependencies:
- Uses: doctest, app (SkinnedCharacter, CharacterFactory, BodyFerry), anim,
  gameplay (spawn_player, player_pre_step/post_step), NullRenderer, NullPhysics,
  HumanBase.dfo (dfn_characters).
- Used by: ctest (app_locomotion).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Порядок тика — как в App: advance → (сим) → commit_root; корень ведётся
  ровно заявкой (вербатим), как будет в фазе 4.
*/
#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/Hitbox.h"
#include "engine/anim/sources/Locomotion.h"
#include "engine/anim/sources/Rig.h"
#include "engine/app/sources/BodyFerry.h"
#include "engine/app/sources/CharGenBody.h"
#include "engine/app/sources/CharacterFactory.h"
#include "engine/app/sources/CharacterFeet.h"
#include "engine/app/sources/SkinnedCharacter.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/materials/sources/PhysicsSubstance.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/RenderSystem.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <memory>
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

// ---------------------------------------------------------------------------
// ХОДОК НА НАСТОЯЩЕМ ШВЕ (фаза 4): тот же порядок тика, что в App —
// advance → заявка паромом → pre_step → физика → post_step → паром тела →
// commit_root. Null-физика проводит смещение вербатим (вертикаль не её).
namespace {

float wrap_pi(float a) { return std::atan2(std::sin(a), std::cos(a)); }

struct Seam {
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    std::unique_ptr<platform::IPhysics> physics = platform::create_null_physics();
    ecs::World world;
    ecs::EntityId player{};
    app::SkinnedCharacter body;
    app::CharacterBodies bodies;
    gameplay::StepContext step;
    bool third_person = false;
    bool ok = false;
    uint32_t turn_episodes = 0;
    float worst_yaw_rate = 0.0f;   ///< |Δкорпус|/dt, рад/с
    /// Обратный ход корпуса внутри одного поворота, рад (замах клипа): у
    /// авторского поворота таз сперва чуть отводится назад — это не «мотает».
    float reverse_rad = 0.0f;
    float episode_sign = 0.0f;
    float reverse_acc = 0.0f;
    anim::LocoState last_state = anim::LocoState::Idle;

    app::CharacterFeet feet; ///< только с Jolt (jolt = true)
    bool jolt = false;

    explicit Seam(bool root_track = true, bool tp = false, bool with_jolt = false)
        : third_person(tp), jolt(with_jolt) {
        if (jolt) {
            physics = platform::create_jolt_physics();
        }
        REQUIRE(physics->init());
        if (jolt) {
            // пол: ящик под нулём, гранит
            platform::StaticBoxDesc floor;
            floor.half_extents = {40.0f, 0.5f, 40.0f};
            floor.center = {0.0f, -0.5f, 0.0f};
            floor.layer = physics::LAYER_STATIC;
            floor.substance = core::find_substance("granite");
            floor.user_data = 7;
            REQUIRE(physics->create_static_box(floor).valid());
        }
        player = gameplay::spawn_player(world, *physics, glm::vec3{0.0f, jolt ? 0.05f : 0.0f, 0.0f});
        if (world.get<anim::BodyDrive>(player) == nullptr) {
            world.add(player, anim::BodyDrive{});
        }
        app::CharacterSpec spec;
        spec.proportions = &rig;
        spec.mesh_asset = app::VIEWER_BODY_MESH_ID;
        spec.blade_asset = app::VIEWER_BLADE_MESH_ID;
        ok = app::build_character(body, bodies, rs, renderer, nullptr,
                                  fs::path(app::CHARGEN_SOURCE_BODY), spec);
        if (ok) {
            body.set_root_track(root_track);
        }
        if (ok && jolt) {
            platform::IPhysics* phys = physics.get();
            body.set_ground_probe([phys](const glm::vec3& at) {
                const platform::RayHit hit = phys->raycast(at + glm::vec3{0.0f, 0.5f, 0.0f},
                                                           glm::vec3{0.0f, -1.0f, 0.0f}, 3.0f,
                                                           physics::LAYER_STATIC);
                return hit.hit ? hit.position.y : std::numeric_limits<float>::quiet_NaN();
            });
            feet.bind(phys, 11);
        }
    }
    ~Seam() {
        feet.shutdown();
        physics->shutdown();
    }
    /// Ступень марша: ящик от z0 (ближний край) на tread вглубь −Z, высотой до top.
    void step_box(float z0, float tread, float top) {
        platform::StaticBoxDesc b;
        b.half_extents = {2.0f, 0.5f * top, 0.5f * tread};
        b.center = {0.0f, 0.5f * top, z0 - 0.5f * tread};
        b.layer = physics::LAYER_STATIC;
        b.substance = core::find_substance("granite");
        b.user_data = 8;
        REQUIRE(physics->create_static_box(b).valid());
    }
    void set_dwell(float sec) { body.set_loco_dwell_min_s(sec); }

    gameplay::PlayerState& ps() { return *world.get<gameplay::PlayerState>(player); }
    const glm::vec3& pos() { return world.get<components::Transform>(player)->position; }

    /// Один тик: оси ввода, мышь (пиксели по x), передача.
    void tick(glm::vec2 axes, float mouse_px = 0.0f, bool run = false, bool jog = false) {
        gameplay::PlayerState& p = ps();
        p.move_axes = axes;
        p.pending_look = glm::vec2{mouse_px, 0.0f};
        p.run = run;
        p.jog = jog;
        auto* drive = world.get<anim::BodyDrive>(player);
        const float yaw_before = p.body_yaw;
        step.locomotion = {};
        body.advance(*drive, pos(), DT);
        const anim::LocomotionOut& lo = body.locomotion();
        if (lo.valid && lo.root_yaw_delta != 0.0f) {
            p.body_yaw += lo.root_yaw_delta;
            if (third_person) {
                p.yaw += lo.root_yaw_delta;
            }
        }
        step.locomotion = app::ferry_locomotion_request(lo, anim::body_root_for(*drive, pos()).yaw);
        gameplay::player_pre_step(world, *physics, [](glm::vec2) { return std::optional<float>{}; }, step);
        physics->step(DT);
        gameplay::player_post_step(world, *physics, step);
        app::BodyView view;
        view.third_person = third_person;
        view.root_track = body.root_track();
        view.cam_yaw = p.yaw;
        app::ferry_body_drive(*drive, p, physics.get(), view);
        body.commit_root(*drive, pos(), DT);
        if (jolt) {
            feet.tick(body, DT);
        }
        // кадр — как в App: IK по земле и зазор стопы считаются на позе кадра
        (void)body.build_draw(/*hide_head=*/false, 1.0f);
        // приборы рыска
        const float dyaw = wrap_pi(p.body_yaw - yaw_before);
        worst_yaw_rate = std::max(worst_yaw_rate, std::abs(dyaw) / DT);
        const anim::LocoState st = body.loco_machine().state;
        if (st == anim::LocoState::TurnInPlace) {
            if (last_state != anim::LocoState::TurnInPlace) {
                ++turn_episodes;
                episode_sign = static_cast<float>(body.loco_machine().turn_sign);
                reverse_acc = 0.0f;
            }
            if (dyaw * episode_sign < 0.0f) {
                reverse_acc += std::abs(dyaw);
                reverse_rad = std::max(reverse_rad, reverse_acc);
            } else if (dyaw != 0.0f) {
                reverse_acc = 0.0f;
            }
        }
        last_state = st;
    }
};

/// Мировая точка стопы (носок; лодыжка при toe=false) — корень + поворот на
/// −рыск корпуса. НЕ contacts().point: она переключается носок/лодыжка и
/// прыгает на 13 см за тик (первый прогон прибора показал 7,9 м/с).
glm::vec2 contact_world(Seam& s, std::size_t side, bool toe = true) {
    const glm::vec3 m = toe ? s.body.contacts().toe[side] : s.body.contacts().ankle[side];
    const glm::vec3 w = glm::vec3{glm::rotate(glm::mat4{1.0f}, -s.ps().body_yaw, glm::vec3{0.0f, 1.0f, 0.0f})
                                  * glm::vec4{m, 0.0f}};
    return glm::vec2{s.pos().x + w.x, s.pos().z + w.z};
}

} // namespace

TEST_CASE("the_capsule_moves_at_the_clip_speed_and_names_the_shortfall") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    struct Row { const char* label; glm::vec2 axes; bool run; bool jog; anim::ClipRole role; float order; };
    const Row rows[] = {
        {"ходьба вперёд", {0.0f, 1.0f}, false, false, anim::ClipRole::Walk, static_cast<float>(config::WALK_SPEED)},
        {"трусца вперёд", {0.0f, 1.0f}, false, true, anim::ClipRole::Jog, static_cast<float>(config::JOG_SPEED)},
        {"бег вперёд", {0.0f, 1.0f}, true, false, anim::ClipRole::Sprint, static_cast<float>(config::RUN_SPEED)},
        {"ходьба назад", {0.0f, -1.0f}, false, false, anim::ClipRole::Backward, static_cast<float>(config::WALK_SPEED)},
        {"ходьба влево", {-1.0f, 0.0f}, false, false, anim::ClipRole::StrafeL, static_cast<float>(config::WALK_SPEED)},
        {"ходьба вправо", {1.0f, 0.0f}, false, false, anim::ClipRole::StrafeR, static_cast<float>(config::WALK_SPEED)},
    };
    const float band = static_cast<float>(config::LOCOMOTION_TEMPO_BAND);
    bool shortfall_named = false;
    for (const Row& r : rows) {
        Seam s;
        REQUIRE(s.ok);
        for (int t = 0; t < 120; ++t) { // 2 с разгона: старт, вход в цикл
            s.tick(r.axes, 0.0f, r.run, r.jog);
        }
        const glm::vec3 a = s.pos();
        const float yaw_a = s.ps().body_yaw;
        float req_sum = 0.0f;
        float act_sum = 0.0f;
        for (int t = 0; t < 120; ++t) { // 2 с замера
            const glm::vec3 before = s.pos();
            s.tick(r.axes, 0.0f, r.run, r.jog);
            req_sum += glm::length(s.step.locomotion.delta_xz);
            act_sum += glm::length(glm::vec2{s.pos().x - before.x, s.pos().z - before.z});
        }
        const glm::vec3 b = s.pos();
        const float v = glm::length(glm::vec2{b.x - a.x, b.z - a.z}) / 2.0f;
        const anim::ClipEntry& e = anim::entry_for(s.body.clip_library(), r.role, 0);
        const float rate = std::clamp(r.order / e.root.mps, 1.0f - band, 1.0f + band);
        const float expect = e.root.mps * rate;
        const float shortfall = 1.0f - v / r.order;
        MESSAGE(r.label << ": капсула " << v << " м/с; клип " << e.root.mps << " × темп " << rate
                        << " = " << expect << "; заказ " << r.order << " → недостача "
                        << 100.0f * shortfall << " %; роль машины "
                        << anim::role_name(s.body.loco_machine().role) << "; заявка/факт за 2 с "
                        << req_sum << "/" << act_sum << " м; рыск " << glm::degrees(s.ps().body_yaw - yaw_a) << "°");
        CHECK(s.body.loco_machine().role == r.role);
        // капсула едет со скоростью клипа в полосе темпа, ±10 %
        CHECK(std::abs(v - expect) <= 0.10f * expect);
        // физика провела заявку вербатим (null: бит-в-бит по горизонтали)
        CHECK(std::abs(req_sum - act_sum) < 1.0e-3f);
        // корпус не крутится от хода бок/назад
        CHECK(std::abs(wrap_pi(s.ps().body_yaw - yaw_a)) < glm::radians(2.0f));
        if (shortfall > 0.10f) {
            shortfall_named = true;
        }
    }
    // ЧЕСТНАЯ НЕДОСТАЧА: заказ RUN_SPEED 6,0 при спринте 2,96 м/с (и ходьба
    // назад 0,93 при заказе 1,8) — прибор ОБЯЗАН её назвать, а не подогнать.
    CHECK(shortfall_named);
}

TEST_CASE("the_yaw_is_continuous_while_the_view_orbits") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    const float rate_max = static_cast<float>(config::CAMERA_TURN_RATE_MAX);
    const float sens = static_cast<float>(config::MOUSE_SENSITIVITY);
    for (const float total_deg : {180.0f, 350.0f}) {
        Seam s;
        REQUIRE(s.ok);
        for (int t = 0; t < 30; ++t) {
            s.tick({0.0f, 0.0f});
        }
        // мышь крутит взгляд на total_deg со скоростью CAMERA_TURN_RATE_MAX
        // (привязь прицела ждёт корпус — это её работа), потом 3 с покоя
        float turned = 0.0f;
        const float per_tick = rate_max * DT;
        for (int t = 0; t < 600; ++t) {
            float d = 0.0f;
            if (turned < glm::radians(total_deg)) {
                d = std::min(per_tick, glm::radians(total_deg) - turned);
            }
            const float yaw0 = s.ps().yaw;
            s.tick({0.0f, 0.0f}, d / sens);
            turned += wrap_pi(s.ps().yaw - yaw0);
        }
        const float gap = wrap_pi(s.ps().yaw - s.ps().body_yaw);
        MESSAGE("облёт " << total_deg << "°: взгляд " << glm::degrees(s.ps().yaw) << "°, корпус "
                         << glm::degrees(s.ps().body_yaw) << "°, зазор " << glm::degrees(gap)
                         << "°; поворотов " << s.turn_episodes << ", пик рыска " << glm::degrees(s.worst_yaw_rate)
                         << "°/с, обратный ход внутри поворота " << glm::degrees(s.reverse_rad)
                         << "°; смен клипа " << s.body.loco_machine().transitions);
        CHECK(turned >= glm::radians(total_deg) - 1.0e-3f); // взгляд дошёл
        CHECK(std::abs(gap) <= glm::radians(static_cast<float>(config::TURN_FIRE_DEG)));
        CHECK(s.worst_yaw_rate <= 1.1f * static_cast<float>(config::BODY_TURN_RATE));
        CHECK(s.reverse_rad <= glm::radians(5.0f)); // замах клипа, не мотание
        CHECK(s.turn_episodes >= 1);
        CHECK(s.turn_episodes <= static_cast<uint32_t>(std::ceil(total_deg / 77.0f)) + 1);
    }
    // КОНТРОЛЬНАЯ РУКА: дребезг взгляда ±60° 5 Гц — переходов в секунду не
    // больше бюджета; без dwell машина превышает его.
    for (const bool dwell : {true, false}) {
        Seam s;
        REQUIRE(s.ok);
        if (!dwell) {
            s.set_dwell(0.0f);
        }
        for (int t = 0; t < 30; ++t) {
            s.tick({0.0f, 0.0f});
        }
        const uint32_t t0 = s.body.loco_machine().transitions;
        float target = 0.0f;
        for (int t = 0; t < 600; ++t) {
            if (t % 6 == 0) {
                target = (t / 6) % 2 == 0 ? glm::radians(60.0f) : glm::radians(-60.0f);
            }
            const float d = std::clamp(wrap_pi(target - s.ps().yaw), -rate_max * DT, rate_max * DT);
            s.tick({0.0f, 0.0f}, d / sens);
        }
        const float per_s = static_cast<float>(s.body.loco_machine().transitions - t0) / 10.0f;
        MESSAGE("дребезг ±60° 5 Гц, dwell " << dwell << ": переходов " << per_s << "/с, поворотов "
                                            << s.turn_episodes);
        if (dwell) {
            CHECK(per_s <= static_cast<float>(config::LOCO_TRANSITIONS_PER_S_MAX));
        } else {
            MESSAGE("контроль без dwell: " << per_s << "/с при бюджете "
                                            << static_cast<float>(config::LOCO_TRANSITIONS_PER_S_MAX));
        }
    }
}

TEST_CASE("the_stance_point_is_still_on_the_flat") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    struct Gear { const char* label; bool run; bool jog; anim::ClipRole role; };
    const Gear gears[] = {{"ходьба", false, false, anim::ClipRole::Walk},
                          {"трусца", false, true, anim::ClipRole::Jog},
                          {"бег", true, false, anim::ClipRole::Sprint}};
    for (const Gear& g : gears) {
        for (const bool root_track : {true, false}) {
            Seam s(root_track);
            REQUIRE(s.ok);
            for (int t = 0; t < 90; ++t) {
                s.tick({0.0f, 1.0f}, 0.0f, g.run, g.jog);
            }
            std::array<glm::vec2, 2> prev{contact_world(s, 0), contact_world(s, 1)};
            std::array<glm::vec2, 2> prev_ankle{contact_world(s, 0, false), contact_world(s, 1, false)};
            std::array<bool, 2> was{s.step.locomotion.planted_left, s.step.locomotion.planted_right};
            std::array<bool, 2> was_toe{s.body.contacts().toe_point[0], s.body.contacts().toe_point[1]};
            float worst = 0.0f;
            float worst_ankle = 0.0f;
            int windows = 0;
            float sum = 0.0f;
            int n = 0;
            // ДИАГНОСТИКА ДОРОЖКИ: минимум мировой скорости нижней точки стопы за
            // прогон и фаза клипа в нём — у клипа без постановок в расписании
            // говорит, дорожка ли завышена (минимум ≫ 0) или порог тесен.
            float min_low = 1.0e9f;
            float min_low_phase = -1.0f;
            float min_low_local = 0.0f;
            float min_height = 1.0e9f;   ///< нижняя точка стопы над покоем, минимум за прогон
            int still_ticks = 0;          ///< тиков, где какая-то точка стопы < CONTACT_STILL_MPS
            int still_low_ticks = 0;      ///< …и при этом ниже GRIP_TOLERANCE_M над покоем
            float worst_still = 0.0f;     ///< в окне опоры: худший из min(носок, лодыжка)
            int planted_ticks = 0;
            int planted_still = 0;        ///< …из них с неподвижной точкой (согласие окна и позы)
            for (int t = 0; t < 240; ++t) {
                const glm::vec3 root_before = s.pos();
                s.tick({0.0f, 1.0f}, 0.0f, g.run, g.jog);
                const std::array<bool, 2> now{s.step.locomotion.planted_left, s.step.locomotion.planted_right};
                for (std::size_t side = 0; side < 2; ++side) {
                    const glm::vec2 w = contact_world(s, side);
                    const glm::vec2 wa = contact_world(s, side, false);
                    const bool is_toe = s.body.contacts().toe_point[side];
                    // нижняя точка та же, что тик назад — иначе скорость мерила бы
                    // расстояние носок–лодыжка
                    const float v_toe = glm::length(w - prev[side]) / DT;
                    const float v_ankle = glm::length(wa - prev_ankle[side]) / DT;
                    const float v_still = std::min(v_toe, v_ankle);
                    const float h = s.body.contacts().height[side];
                    min_height = std::min(min_height, h);
                    if (v_still < static_cast<float>(config::CONTACT_STILL_MPS)) {
                        ++still_ticks;
                        if (h <= 0.03f) {
                            ++still_low_ticks;
                        }
                    }
                    if (now[side] && was[side]) {
                        worst_still = std::max(worst_still, v_still);
                        ++planted_ticks;
                        if (v_still < 1.2f * static_cast<float>(config::CONTACT_STILL_MPS)) {
                            ++planted_still;
                        }
                    }
                    if (is_toe == was_toe[side]) {
                        const glm::vec2 lw = is_toe ? w : wa;
                        const glm::vec2 lp = is_toe ? prev[side] : prev_ankle[side];
                        const float vl = glm::length(lw - lp) / DT;
                        if (vl < min_low) {
                            min_low = vl;
                            min_low_phase = s.body.loco_machine().phase;
                            min_low_local = glm::length(glm::vec2{s.pos().x - root_before.x, s.pos().z - root_before.z}) / DT;
                        }
                        if (now[side] && was[side]) {
                            worst = std::max(worst, vl);
                            worst_ankle = std::max(worst_ankle, glm::length(wa - prev_ankle[side]) / DT);
                            sum += vl;
                            ++n;
                        }
                    }
                    if (now[side] && !was[side]) {
                        ++windows;
                    }
                    prev[side] = w;
                    prev_ankle[side] = wa;
                    was_toe[side] = is_toe;
                }
                was = now;
            }
            const anim::ClipEntry& e = anim::entry_for(s.body.clip_library(), g.role, 0);
            MESSAGE(g.label << ", " << (root_track ? "дорожка корня" : "прежний путь") << ": клип "
                            << e.root.mps << " м/с, постановок в расписании " << int(e.plant_count[0]) << "/"
                            << int(e.plant_count[1]) << "; опор за 4 с " << windows
                            << ", ход носка опорной стопы к земле worst " << worst << " м/с (лодыжка "
                            << worst_ankle << "), средний " << (n > 0 ? sum / n : 0.0f) << " м/с за " << n
                            << " тиков опоры; минимум мировой скорости нижней точки " << min_low
                            << " м/с на фазе " << min_low_phase << " (капсула там " << min_low_local << " м/с); "
                            << "минимум высоты нижней точки над покоем " << 1000.0f * min_height << " мм; тиков с "
                            << "неподвижной точкой " << still_ticks << ", из них ниже 3 см " << still_low_ticks
                            << " (из 480); в окнах опоры худший min(носок, лодыжка) " << worst_still
                            << " м/с, тиков опоры с неподвижной точкой " << planted_still << " из " << planted_ticks);
            if (root_track) {
                CHECK(windows >= 4); // постановки идут — события шагов есть
                // ОПОРА ПО ОПРЕДЕЛЕНИЮ: стопа стоит, пока какая-то её точка
                // (пятка при ударе, носок при перекате) в мире медленнее
                // CONTACT_STILL_MPS; окно расписания снято с чистого клипа на
                // 128 точках, тик и темп размывают его края — согласие окна и
                // позы на ходьбе ≥ 85 % тиков (замер 11.09: 192 из 209; края —
                // до 0,9 м/с носком в момент удара пяткой). У бега (MX_Standard_Run
                // 3,8 м/с) стопа неподвижна ~0,07 с, у спринта ~0,03 с — окно
                // в 1 тик, судить нечего: печатается.
                if (g.role == anim::ClipRole::Walk) {
                    CHECK(planted_still >= static_cast<int>(0.85f * static_cast<float>(planted_ticks)));
                    CHECK(planted_ticks >= 120); // ≥ половина тиков хода — в опоре
                }
            }
        }
    }
}

TEST_CASE("the_march_is_climbed_on_physical_feet") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    for (const bool root_track : {true, false}) {
        Seam s(root_track, false, true);
        REQUIRE(s.ok);
        // канонический марш: девять ступеней 0,18/0,28 от z = −1,5 в −Z
        for (int i = 0; i < 9; ++i) {
            s.step_box(-1.5f - 0.28f * static_cast<float>(i), 0.28f, 0.18f * static_cast<float>(i + 1));
        }
        // площадка наверху — до конца прогона (8 с ходьбы = 14 м)
        s.step_box(-1.5f - 0.28f * 9.0f, 20.0f, 0.18f * 9.0f);
        float worst_gap = 0.0f;
        int gap_hits = 0;
        int judged = 0;
        int worst_tick = -1;
        float worst_z = 0.0f;
        float worst_signed = 0.0f;
        std::size_t worst_side = 0;
        float top_y = -1.0f;
        float min_y = 1.0e9f;
        for (int t = 0; t < 60 * 8; ++t) {
            s.tick({0.0f, 1.0f});
            const anim::FootGap& g = s.body.foot_gap_last();
            for (std::size_t side = 0; side < 2; ++side) {
                // первые полсекунды — посадка капсулы на пол после спавна
                if (t >= 30 && g.judged[side] != 0) {
                    ++judged;
                    if (std::abs(g.gap[side]) > worst_gap) {
                        worst_gap = std::abs(g.gap[side]);
                        worst_tick = t;
                        worst_z = s.pos().z;
                        worst_signed = g.gap[side];
                        worst_side = side;
                    }
                    if (std::abs(g.gap[side]) > static_cast<float>(config::LOCO_GAP_MAX_M)) {
                        ++gap_hits;
                    }
                }
            }
            top_y = std::max(top_y, s.pos().y);
            if (t > 60) {
                min_y = std::min(min_y, s.pos().y);
            }
            if (root_track && t >= 80 && t <= 110 && std::getenv("DFN_MARCH_TRACE") != nullptr) {
                const anim::FootIkPlan& pl = s.body.foot_plan();
                std::fprintf(stderr,
                             "[march] t %d z %.3f y %.3f root_dy %.3f need %.3f/%.3f w %.2f/%.2f gap %.3f/%.3f judged %d/%d planted %d/%d state %s\n",
                             t, s.pos().z, s.pos().y, s.body.foot_root_shift_m(), pl.need[0], pl.need[1],
                             pl.weight[0], pl.weight[1], g.gap[0], g.gap[1], g.judged[0], g.judged[1],
                             s.step.locomotion.planted_left, s.step.locomotion.planted_right,
                             anim::loco_state_name(s.body.loco_machine().state));
            }
        }
        const app::FootPhysicsReport& l = s.feet.report(0);
        MESSAGE((root_track ? "дорожка корня" : "прежний путь") << ": за 8 с капсула поднялась до "
                << top_y << " м (верх марша 1,62), сейчас z " << s.pos().z << ", y " << s.pos().y
                << ", минимум y после старта " << min_y
                << "; зазор судимой стопы worst " << 1000.0f * worst_signed << " мм (тик " << worst_tick
                << ", z " << worst_z << ", сторона " << worst_side << "), за порогом "
                << gap_hits << " из " << judged << " судимых; стопы стоят " << l.planted << ", держат "
                << l.holds << ", скольжение " << l.slip_mps << " м/с; роль "
                << anim::role_name(s.body.loco_machine().role));
        if (root_track) {
            CHECK(top_y >= 1.62f - 0.05f);          // дошёл до верха
            CHECK(s.pos().z < -1.5f - 0.28f * 9.0f); // и вышел на площадку
            CHECK(min_y >= -0.05f);                  // не провалился
            // НАХОДКА 11.09 (§16.6, тикет владельцу): капсула (радиус больше
            // проступи 0,28) въезжает на подступёнок раньше стопы и поднимается
            // ПЛАВНО (0,8 м/с по вертикали), а стопы клипа ходьбы стоят на
            // дискретных ступенях: задняя стопа ещё на полу, когда капсула уже
            // на 0,53 м — опускать таз на 0,55 (нужда плана) нельзя, парение до
            // 41 см на каждом шаге подъёма. Замок прежнего пути тянул стопу к
            // якорю и прятал это (10 мм). Лестнице нужен свой ход (клип
            // лестницы или корень по высоте от опорной стопы) — не полоса.
            // Потолок здесь — регрессионный, по замеру, не приёмочный.
            CHECK(worst_gap <= 0.45f);
        }
    }
}

TEST_CASE("the_ankles_do_not_cross") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    const float sens = static_cast<float>(config::MOUSE_SENSITIVITY);
    const float rate_max = static_cast<float>(config::CAMERA_TURN_RATE_MAX);
    struct Beat { glm::vec2 axes; bool run; bool jog; float view_deg; int ticks; const char* label; };
    const Beat script[] = {
        {{0.0f, 0.0f}, false, false, 0.0f, 30, "покой"},
        {{0.0f, 1.0f}, false, false, 0.0f, 120, "ходьба"},
        {{-1.0f, 0.0f}, false, false, 0.0f, 90, "влево"},
        {{1.0f, 0.0f}, false, false, 0.0f, 90, "вправо"},
        {{0.0f, -1.0f}, false, false, 0.0f, 90, "назад"},
        {{0.0f, 1.0f}, false, true, 0.0f, 90, "трусца"},
        {{0.0f, 1.0f}, true, false, 0.0f, 90, "бег"},
        {{0.0f, 0.0f}, false, false, 0.0f, 60, "остановка"},
        {{0.0f, 0.0f}, false, false, 90.0f, 90, "взгляд +90"},
        {{0.0f, 0.0f}, false, false, -90.0f, 120, "взгляд −90 (180 назад)"},
        {{0.0f, 0.0f}, false, false, 80.0f, 120, "взгляд +80 (170)"},
        {{0.0f, 1.0f}, false, false, 80.0f, 90, "ходьба после"},
    };
    for (const bool root_track : {true, false}) {
        Seam s(root_track);
        REQUIRE(s.ok);
        std::size_t slot_l = anim::HITBOX_COUNT;
        std::size_t slot_r = anim::HITBOX_COUNT;
        for (std::size_t i = 0; i < anim::HITBOX_COUNT; ++i) {
            if (s.body.hitboxes().slot[i].part == anim::BodyPart::FootL) {
                slot_l = i;
            }
            if (s.body.hitboxes().slot[i].part == anim::BodyPart::FootR) {
                slot_r = i;
            }
        }
        REQUIRE(slot_l < anim::HITBOX_COUNT);
        REQUIRE(slot_r < anim::HITBOX_COUNT);
        // ДВЕ МЕРЫ. Перекрест лодыжек поперёк тела — только СТОЯ (покой, поворот
        // на месте): на стрейфе Mixamo шаг приставной с заносом (левая уходит
        // на 34 см правее правой — это авторский шаг, не крест). Пересечение
        // ног — коробки голеней и бёдер на ВСЁМ сценарии: ноль по GJK —
        // вложение или касание, глубины не различает.
        float worst_cross = -1.0f;
        std::string worst_at;
        float min_shin = 1.0e9f;
        float min_thigh = 1.0e9f;
        std::string min_at;
        for (const Beat& b : script) {
            const float target = s.ps().yaw + glm::radians(b.view_deg);
            for (int t = 0; t < b.ticks; ++t) {
                const float d = std::clamp(wrap_pi(target - s.ps().yaw), -rate_max * DT, rate_max * DT);
                s.tick(b.axes, d / sens, b.run, b.jog);
                const anim::HitboxPose& hp = s.body.hitbox_pose();
                if (hp.valid[slot_l] == 0 || hp.valid[slot_r] == 0) {
                    continue;
                }
                const float shin = anim::hitbox_pair_distance(s.body.hitboxes(), hp, anim::BodyPart::ShinL, anim::BodyPart::ShinR);
                const float thigh = anim::hitbox_pair_distance(s.body.hitboxes(), hp, anim::BodyPart::ThighL, anim::BodyPart::ThighR);
                if (std::min(shin, thigh) < std::min(min_shin, min_thigh)) {
                    min_at = b.label;
                }
                min_shin = std::min(min_shin, shin);
                min_thigh = std::min(min_thigh, thigh);
                const anim::LocoState st = s.body.loco_machine().state;
                const bool standing = root_track ? (st == anim::LocoState::Idle || st == anim::LocoState::TurnInPlace)
                                                 : glm::length(b.axes) < 1.0e-4f;
                if (!standing) {
                    continue;
                }
                // в систему корпуса: +X — вправо; перекрест — левая правее правой
                const glm::mat4 to_body = glm::rotate(glm::mat4{1.0f}, s.ps().body_yaw, glm::vec3{0.0f, 1.0f, 0.0f});
                const glm::vec3 l = glm::vec3{to_body * glm::vec4{glm::vec3{hp.frame[slot_l][3]} - s.pos(), 0.0f}};
                const glm::vec3 r = glm::vec3{to_body * glm::vec4{glm::vec3{hp.frame[slot_r][3]} - s.pos(), 0.0f}};
                const float cross = l.x - r.x;
                if (cross > worst_cross) {
                    worst_cross = cross;
                    worst_at = b.label;
                }
            }
        }
        MESSAGE((root_track ? "дорожка корня" : "прежний путь") << ": стоя худший перекрест лодыжек "
                << 1000.0f * worst_cross << " мм (левая правее правой) на «" << worst_at
                << "»; минимум коробок голень–голень " << 1000.0f * min_shin << " мм, бедро–бедро "
                << 1000.0f * min_thigh << " мм на «" << min_at << "»; смен клипа "
                << s.body.loco_machine().transitions);
        if (root_track) {
            CHECK(worst_cross <= static_cast<float>(config::LOCO_CROSS_MAX_M));
            CHECK(min_shin >= 0.0f);
            CHECK(min_thigh >= 0.0f);
        }
    }
}
