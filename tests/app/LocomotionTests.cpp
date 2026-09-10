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
- the_recorded_input_replays_bit_for_bit (прибор 6, фаза 7: ввод по тикам
  пишется в .dftraj (секция INPT), прогон даёт побитово ту же походку;
  контроль — другой ввод даёт другую)
- a_wall_stops_the_walk_instead_of_the_feet_sliding (§16.9, Jolt: стена на
  пути; контроль — без правила цикл крутится в стену, стопа ≥ 1 м/с)
- a_minute_of_scripted_input_stays_under_the_transition_budget (прибор 4,
  фаза 7: 60 с сценария — смен клипа в секунду ≤ бюджета; контроль —
  нажатие/отпускание каждый тик)

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
#include "engine/app/sources/TrajectoryRecord.h"
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

#include <glm/gtc/constants.hpp>

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
    // КОНТРОЛЬНАЯ РУКА (после сноса фазы 6): без дорожки корня заявки нет — на
    // пути игрока без сима тело не едет; в игре капсулу ведёт модель скорости
    // ввода (PlayerMovement), роль клипа — от ввода.
    Harness old(false);
    REQUIRE(old.ok);
    const Run o = run(old, speed, anim::Gait::Walk, 300, 360, 0.0f);
    MESSAGE("контроль DFN_ROOT_TRACK=0: роли " << chain(o) << "| путь " << glm::length(glm::vec2{o.root.x, o.root.z}) << " м");
    CHECK(o.root.z == 0.0f);
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
        step_tick();
    }
    /// Тик записанным вводом (§16.8): apply_input в той же точке, что App.
    void tick_recorded(const app::InputTick& in) {
        app::apply_input(ps(), in);
        step_tick();
    }
    void step_tick() {
        gameplay::PlayerState& p = ps();
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
    // Канонический марш стенда stairs: девять ступеней 0,18/0,28. Вверх — с
    // пола в −Z; вниз — с площадки наверху в +Z (тот же марш зеркально).
    // Приёмка (лид 11.09): парение судимой стопы ≤ 2 см, проникание ≤ 2 см на
    // подъёме и спуске; контроль — высота тела от капсулы (DFN_ROOT_HEIGHT=
    // capsule), где задняя стопа парила до 41 см.
    struct Arm { const char* label; bool feet_height; };
    const Arm arms[] = {{"высота от опорной стопы", true}, {"контроль: от капсулы", false}};
    for (const bool up : {true, false}) {
        for (const Arm& arm : arms) {
            Seam s(true, false, true);
            REQUIRE(s.ok);
            s.body.set_root_height_from_feet(arm.feet_height);
            const float top = 0.18f * 9.0f;
            if (up) {
                for (int i = 0; i < 9; ++i) {
                    s.step_box(-1.5f - 0.28f * static_cast<float>(i), 0.28f, 0.18f * static_cast<float>(i + 1));
                }
                s.step_box(-1.5f - 0.28f * 9.0f, 20.0f, top);
            } else {
                // площадка под спавном (z > −1,5) на высоте верха, ступени вниз в +Z
                platform::StaticBoxDesc deck;
                deck.half_extents = {2.0f, 0.5f * top, 10.0f};
                deck.center = {0.0f, 0.5f * top, 1.5f - 10.0f}; // z −18,5…1,5 — под спавном
                deck.layer = physics::LAYER_STATIC;
                deck.substance = core::find_substance("granite");
                deck.user_data = 8;
                REQUIRE(s.physics->create_static_box(deck).valid());
                for (int i = 0; i < 9; ++i) {
                    // ступень i (сверху): от z = 1,5 + 0,28·i вглубь +Z, высота top − 0,18·(i+1)
                    platform::StaticBoxDesc b;
                    const float h = top - 0.18f * static_cast<float>(i + 1);
                    b.half_extents = {2.0f, 0.5f * std::max(h, 0.005f), 0.14f};
                    b.center = {0.0f, 0.5f * h, 1.5f + 0.28f * static_cast<float>(i) + 0.14f};
                    b.layer = physics::LAYER_STATIC;
                    b.substance = core::find_substance("granite");
                    b.user_data = 8;
                    if (h > 0.0f) {
                        REQUIRE(s.physics->create_static_box(b).valid());
                    }
                }
                // спавн переносится на площадку
                s.physics->teleport_character(s.ps().character, glm::vec3{0.0f, top + 0.05f, 0.0f});
                s.world.get<components::Transform>(s.player)->position = glm::vec3{0.0f, top + 0.05f, 0.0f};
                s.ps().yaw = glm::pi<float>();
                s.ps().body_yaw = glm::pi<float>();
            }
            float worst_float = 0.0f;
            float worst_sink = 0.0f;
            int float_hits = 0;
            int sink_hits = 0;
            int judged = 0;
            float top_y = -1.0f;
            float min_y = 1.0e9f;
            for (int t = 0; t < 60 * 8; ++t) {
                s.tick({0.0f, 1.0f});
                const anim::FootGap& g = s.body.foot_gap_last();
                // судится стопа, которую ДАТЧИК держит стоящей (тело стопы касается
                // и стоит): на спуске стопа клипа по расписанию «стоит» ещё в
                // воздухе над нижней ступенью — это мах, не парение
                const std::array<bool, 2> stance{s.feet.report(0).planted, s.feet.report(1).planted};
                for (std::size_t side = 0; side < 2; ++side) {
                    // первые полсекунды — посадка капсулы
                    if (t >= 30 && stance[side] && g.judged[side] != 0) {
                        ++judged;
                        worst_float = std::max(worst_float, g.gap[side]);
                        worst_sink = std::max(worst_sink, -g.gap[side]);
                        if (g.gap[side] > 0.02f) {
                            ++float_hits;
                        }
                        if (-g.gap[side] > 0.02f) {
                            ++sink_hits;
                        }
                    }
                }
                top_y = std::max(top_y, s.pos().y);
                if (t > 60) {
                    min_y = std::min(min_y, s.pos().y);
                }
                if (arm.feet_height && std::getenv("DFN_MARCH_TRACE") != nullptr) {
                    const anim::FootIkPlan& pl = s.body.foot_plan();
                    std::fprintf(stderr,
                                 "[march %s] t %d z %.3f y %.3f root_dy %.3f plan %.3f need %.3f/%.3f w %.2f/%.2f "
                                 "sched %d/%d phys %d%d/%d%d gap %.3f/%.3f judged %d/%d phase %.2f %s\n",
                                 up ? "up" : "down", t, s.pos().z, s.pos().y, s.body.foot_root_shift_m(),
                                 pl.root_dy, pl.need[0], pl.need[1], pl.weight[0], pl.weight[1],
                                 s.step.locomotion.planted_left, s.step.locomotion.planted_right,
                                 stance[0], s.feet.report(0).touching, stance[1], s.feet.report(1).touching,
                                 g.gap[0], g.gap[1], g.judged[0], g.judged[1], s.body.loco_machine().phase,
                                 anim::loco_state_name(s.body.loco_machine().state));
                }
            }
            MESSAGE((up ? "подъём" : "спуск") << ", " << arm.label << ": капсула y " << s.pos().y
                    << " (верх " << top << "), z " << s.pos().z << "; парение судимой стопы worst "
                    << 1000.0f * worst_float << " мм (" << float_hits << " тиков > 2 см), проникание worst "
                    << 1000.0f * worst_sink << " мм (" << sink_hits << " > 2 см) из " << judged
                    << " судимых; роль " << anim::role_name(s.body.loco_machine().role));
            if (up) {
                CHECK(top_y >= top - 0.05f);
                CHECK(s.pos().z < -1.5f - 0.28f * 9.0f);
                CHECK(min_y >= -0.05f);
            } else {
                CHECK(s.pos().y <= 0.05f);
                CHECK(s.pos().z > 1.5f + 0.28f * 9.0f);
            }
            if (arm.feet_height) {
                CHECK(worst_float <= 0.02f);
                CHECK(worst_sink <= 0.02f);
            }
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

namespace {
/// Сценарий ввода на минуту: ходьба, повороты, стрейфы, остановки, бег,
/// присед — то, что делает игрок на стенде. Возвращает оси, мышь, передачи.
struct ScriptTick { glm::vec2 axes; float mouse_px; bool run; bool jog; bool crouch; };
ScriptTick scripted_input(int t) {
    const int s = t / 60; // секунда
    ScriptTick k{{0.0f, 0.0f}, 0.0f, false, false, false};
    switch (s % 12) {
    case 0: k.axes = {0.0f, 1.0f}; break;                          // ходьба
    case 1: k.axes = {0.0f, 1.0f}; k.mouse_px = 6.0f; break;       // ходьба с поворотом
    case 2: k.axes = {0.0f, 0.0f}; k.mouse_px = 9.0f; break;       // стоя облёт
    case 3: k.axes = {1.0f, 0.0f}; break;                          // стрейф
    case 4: k.axes = {0.0f, 1.0f}; k.jog = true; break;            // трусца
    case 5: k.axes = {0.0f, 0.0f}; break;                          // стоп
    case 6: k.axes = {0.0f, -1.0f}; break;                         // назад
    case 7: k.axes = {0.0f, 1.0f}; k.run = true; break;            // бег
    case 8: k.axes = {0.0f, 1.0f}; k.mouse_px = -8.0f; break;      // бег с поворотом
    case 9: k.axes = {-1.0f, 1.0f}; break;                         // диагональ
    case 10: k.axes = {0.0f, 0.0f}; k.mouse_px = -20.0f; break;    // резкий облёт стоя
    default: k.axes = {0.0f, 1.0f}; k.crouch = true; break;        // присед
    }
    return k;
}
/// Отпечаток походки на тике — что сравнивается побитово между прогонами.
struct GaitPrint { glm::vec3 root; float body_yaw; float yaw; uint32_t transitions; anim::LocoState state; float phase; };
GaitPrint print_of(Seam& s) {
    return GaitPrint{s.pos(), s.ps().body_yaw, s.ps().yaw, s.body.loco_machine().transitions,
                     s.body.loco_machine().state, s.body.loco_machine().phase};
}
} // namespace

TEST_CASE("the_recorded_input_replays_bit_for_bit") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    const std::string path = (fs::temp_directory_path() / "dfn_loco_replay.dftraj").string();
    // ЗАПИСЬ: 10 с сценария, ввод пишется в той же точке тика, что в App
    // (после сбора ввода, до pre_step) — capture_input с ходока.
    std::vector<GaitPrint> recorded;
    {
        Seam s;
        REQUIRE(s.ok);
        app::TrajectoryRecorder rec;
        rec.begin(/*stand=*/3, /*seed=*/1u);
        for (int t = 0; t < 600; ++t) {
            const ScriptTick k = scripted_input(t);
            gameplay::PlayerState& p = s.ps();
            p.move_axes = k.axes;
            p.pending_look = glm::vec2{k.mouse_px, 0.0f};
            p.run = k.run;
            p.jog = k.jog;
            p.crouch_held = k.crouch;
            const app::InputTick in = app::capture_input(p, 0.0f, 0.0f);
            rec.push_input(in);
            // тик ходока ровно этим вводом
            s.tick_recorded(in);
            recorded.push_back(print_of(s));
        }
        app::TrajectoryFrame f;
        rec.push(f); // глаз не судится — кадр для контейнера
        REQUIRE(!rec.stop_and_write(path).empty());
    }
    // ПРОГОН: файл читается, ввод применяется apply_input — отпечаток совпадает
    // побитово на каждом тике.
    {
        app::TrajectoryPlayer pl;
        REQUIRE(pl.load(path));
        REQUIRE(pl.has_inputs());
        REQUIRE(pl.trajectory()->inputs.size() == 600);
        Seam s;
        REQUIRE(s.ok);
        int mismatches = 0;
        for (int t = 0; t < 600; ++t) {
            const app::InputTick* in = pl.next_input();
            REQUIRE(in != nullptr);
            s.tick_recorded(*in);
            const GaitPrint a = print_of(s);
            const GaitPrint& b = recorded[static_cast<std::size_t>(t)];
            const bool same = a.root == b.root && a.body_yaw == b.body_yaw && a.yaw == b.yaw
                              && a.transitions == b.transitions && a.state == b.state && a.phase == b.phase;
            mismatches += same ? 0 : 1;
        }
        MESSAGE("прогон записанного ввода: расхождений " << mismatches << " из 600 тиков; смен клипа "
                << s.body.loco_machine().transitions << ", путь " << glm::length(glm::vec2{s.pos().x, s.pos().z}) << " м");
        CHECK(mismatches == 0);
        CHECK(s.body.loco_machine().transitions >= 6);
    }
    // КОНТРОЛЬНАЯ РУКА: другой ввод (мышь в другую сторону) — другая походка.
    {
        Seam s;
        REQUIRE(s.ok);
        int mismatches = 0;
        for (int t = 0; t < 600; ++t) {
            ScriptTick k = scripted_input(t);
            k.mouse_px = -k.mouse_px;
            gameplay::PlayerState& p = s.ps();
            p.move_axes = k.axes;
            p.pending_look = glm::vec2{k.mouse_px, 0.0f};
            p.run = k.run;
            p.jog = k.jog;
            p.crouch_held = k.crouch;
            s.tick_recorded(app::capture_input(p, 0.0f, 0.0f));
            const GaitPrint a = print_of(s);
            const GaitPrint& b = recorded[static_cast<std::size_t>(t)];
            mismatches += (a.root == b.root && a.body_yaw == b.body_yaw) ? 0 : 1;
        }
        MESSAGE("контроль (другой ввод): расхождений " << mismatches << " из 600");
        CHECK(mismatches > 100);
    }
    std::error_code ec;
    fs::remove(path, ec);
}

TEST_CASE("a_minute_of_scripted_input_stays_under_the_transition_budget") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    const float budget = static_cast<float>(config::LOCO_TRANSITIONS_PER_S_MAX);
    {
        Seam s;
        REQUIRE(s.ok);
        // DFN_SCRIPT_CSV=<путь> — телеметрия сценария по тикам (разбор пиков)
        const char* csv = std::getenv("DFN_SCRIPT_CSV");
        s.body.set_telemetry(true, csv != nullptr ? std::string{csv} : std::string{});
        uint32_t last = 0;
        float worst_per_s = 0.0f;
        for (int t = 0; t < 3600; ++t) {
            const ScriptTick k = scripted_input(t);
            s.tick(k.axes, k.mouse_px, k.run, k.jog);
            if (t % 60 == 59) {
                const uint32_t now = s.body.loco_machine().transitions;
                worst_per_s = std::max(worst_per_s, static_cast<float>(now - last));
                last = now;
            }
        }
        const anim::LocoTelemetry& tm = s.body.telemetry();
        MESSAGE("60 с сценария: смен клипа " << s.body.loco_machine().transitions << " (худшая секунда "
                << worst_per_s << ", прибор " << tm.row(anim::LocoProbe::TransitionsPerS).worst << "/с, за бюджетом "
                << tm.row(anim::LocoProbe::TransitionsPerS).hits << " тиков), путь "
                << glm::length(glm::vec2{s.pos().x, s.pos().z}) << " м, ход опорной стопы worst "
                << tm.row(anim::LocoProbe::StanceSlip).worst << " м/с (" << tm.row(anim::LocoProbe::StanceSlip).hits
                << " устойчивых), рыск worst " << tm.row(anim::LocoProbe::TurnRate).worst << "°/с");
        CHECK(worst_per_s <= budget);
        CHECK(tm.row(anim::LocoProbe::TransitionsPerS).hits == 0);
    }
    // КОНТРОЛЬНАЯ РУКА: нажатие/отпускание КАЖДЫЙ ТИК — dwell (LOCO_STATE_DWELL_S)
    // держит старт и остановку по 0,15 с: ~6,7 смен/с против 60 без него
    // (замер 11.09). Это не игровой ввод, а предел машины; бюджет судится на
    // сценарии выше.
    uint32_t with_dwell = 0;
    uint32_t without = 0;
    for (const bool dwell : {true, false}) {
        Seam s;
        REQUIRE(s.ok);
        if (!dwell) {
            s.set_dwell(0.0f);
        }
        for (int t = 0; t < 600; ++t) {
            s.tick(t % 2 == 0 ? glm::vec2{0.0f, 1.0f} : glm::vec2{0.0f, 0.0f});
        }
        (dwell ? with_dwell : without) = s.body.loco_machine().transitions;
        MESSAGE("дребезг ввода каждый тик, dwell " << dwell << ": смен клипа за 10 с " << s.body.loco_machine().transitions);
    }
    CHECK(with_dwell * 4 < without);
    CHECK(static_cast<float>(with_dwell) / 10.0f <= 8.0f);
}

TEST_CASE("a_wall_stops_the_walk_instead_of_the_feet_sliding") {
    if (!fs::exists(app::CHARGEN_SOURCE_BODY)) {
        return;
    }
    // СТЕНА ПОПЕРЁК ХОДА В 1,5 М (§16.9). Дорожка корня заказывает 3 см/тик, мир
    // исполняет 0: без правила цикл ходьбы крутится на месте и «опорная»
    // стопа едет со скоростью хода (замер 11.09 у края стенда: 2,2 м/с). С
    // правилом машина останавливается за LOCO_BLOCKED_S и стоит, пока ввод
    // держится в стену.
    for (const bool rule : {true, false}) {
        Seam s(true, false, true);
        REQUIRE(s.ok);
        if (!rule) {
            s.body.set_loco_blocked_min_s(1.0e9f);
        }
        platform::StaticBoxDesc wall;
        wall.half_extents = {2.0f, 1.0f, 0.1f};
        wall.center = {0.0f, 1.0f, -1.5f - 0.1f};
        wall.layer = physics::LAYER_STATIC;
        wall.substance = core::find_substance("granite");
        wall.user_data = 9;
        REQUIRE(s.physics->create_static_box(wall).valid());
        float t_blocked = -1.0f;   ///< когда капсула встала (ход < 1 мм/тик при заявке)
        float t_stopped = -1.0f;   ///< когда машина вышла из цикла/старта
        float worst_planted = 0.0f; ///< мировая скорость нижней точки стоящей по расписанию стопы в установившемся состоянии после t_blocked+0,5 с
        int planted_ticks = 0;
        std::array<glm::vec2, 2> prev{};
        std::array<bool, 2> was{};
        for (int t = 0; t < 240; ++t) {
            const glm::vec3 before = s.pos();
            s.tick({0.0f, 1.0f});
            const float t_s = static_cast<float>(t + 1) * DT;
            const float moved = glm::length(glm::vec2{s.pos().x - before.x, s.pos().z - before.z});
            const float asked = glm::length(s.step.locomotion.delta_xz);
            if (t_blocked < 0.0f && t_s > 0.5f && asked > 0.01f && moved < 0.001f) {
                t_blocked = t_s;
            }
            const anim::LocoState st = s.body.loco_machine().state;
            if (t_blocked >= 0.0f && t_stopped < 0.0f && st != anim::LocoState::Cycle
                && st != anim::LocoState::Start) {
                t_stopped = t_s;
            }
            const std::array<bool, 2> now{s.step.locomotion.planted_left, s.step.locomotion.planted_right};
            for (std::size_t side = 0; side < 2; ++side) {
                const glm::vec2 w = contact_world(s, side);
                // судится установившееся состояние: стык гасит инерциализация, и
                // её склейка двигает стопы до 3 м/с (§16.6) — это не ход в стену
                const bool steady = s.body.loco_machine().dwell_s >= static_cast<float>(config::INERTIAL_BLEND_S) + 2.0f * DT;
                if (t_blocked >= 0.0f && t_s > t_blocked + 0.5f && steady && now[side] && was[side]) {
                    worst_planted = std::max(worst_planted, glm::length(w - prev[side]) / DT);
                    ++planted_ticks;
                }
                prev[side] = w;
            }
            was = now;
        }
        const anim::LocoState end = s.body.loco_machine().state;
        MESSAGE(std::string{rule ? "правило" : "контроль"} << ": капсула встала на " << t_blocked << " с, машина вышла из хода на "
                << t_stopped << " с, в конце " << std::string{anim::loco_state_name(end)} << ", стоящая стопа после остановки худшее "
                << worst_planted << " м/с за " << planted_ticks << " тиков, z " << s.pos().z);
        REQUIRE(t_blocked >= 0.0f);
        if (rule) {
            REQUIRE(t_stopped >= 0.0f);
            CHECK(t_stopped - t_blocked <= static_cast<float>(config::LOCO_BLOCKED_S) + 3.0f * DT);
            CHECK(end == anim::LocoState::Idle);
            CHECK(s.body.loco_machine().blocked);
            CHECK(worst_planted <= 0.1f); // покой: стопы стоят
        } else {
            CHECK(end == anim::LocoState::Cycle);
            CHECK(worst_planted >= 1.0f); // цикл в стену: «опорная» стопа едет со скоростью хода
        }
    }
}
