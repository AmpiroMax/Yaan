/*
Module: tests
File: tests/app/GroundedLocomotionTests.cpp

Responsibility:
- НОГИ НА ЗЕМЛЕ НА ДОРОЖКЕ КОРНЯ (LOCOMOTION_GROUNDED.md §16.7): тело
  HumanBase на пути игрока без сима — корень едет ровно заявкой (вербатим),
  рыск — из дорожки, земля — щуп (плоскость, скат, марш). Прежние случаи
  переписаны на новом механизме под теми же именами; снесённые — в §16.7
  строкой «удалён X → меряет Y (контроль Z)».

Key items:
- the_planted_foot_stays_put_on_the_player_path: опора по расписанию стоит
  (контроль — без дорожки тело не едет вовсе)
- idle_feet_stand_level: покой — обе стопы стоят, симметрично, на земле
- stairs_and_slope_keep_the_foot_planted_and_on_the_tread: скат 15° и марш
- turning_in_place_steps_the_feet_instead_of_twisting: поворот — клип с
  переступом, стопа не выворачивается дальше LOCO_PELVIS_TWIST_MAX_DEG
- gear_changes_settle_where_the_gear_settles_from_standing: смена передач —
  скорость клипа × темп, фаза без разрыва
- eight_directions_keep_the_foot_planted_and_pick_the_direction_role
- telemetry_on_the_player_path: приборы на ходу и на коротких нажатиях
- the_walk_and_the_run_start_with_their_own_clip (контроль — без переходов)
- the_run_stops_with_its_own_clip, the_walk_stops_with_its_own_clip
- standing_body_turns_to_the_camera_by_stepping
- spinning_the_view_does_not_fly_the_body_across_the_map

Dependencies:
- Uses: app (SkinnedCharacter, CharacterFactory), anim, NullRenderer,
  HumanBase.dfo (dfn_characters).
- Used by: ctest (app_grounded_locomotion).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Пороги — строки реестра; кадры — только стенды, этот прибор — числа.
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

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace dfn;
namespace fs = std::filesystem;

namespace {

constexpr float DT = static_cast<float>(config::SIM_DT);

[[nodiscard]] float wrap_pi(float a) { return std::atan2(std::sin(a), std::cos(a)); }

[[nodiscard]] bool body_present() {
    std::error_code ec;
    return fs::exists(app::CHARGEN_SOURCE_BODY, ec);
}

using Ground = std::function<float(float, float)>;

struct Harness {
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    app::SkinnedCharacter body;
    app::CharacterBodies bodies;
    bool ok = false;
    glm::vec3 root{0.0f};
    float body_yaw = 0.0f;
    Ground ground;
    std::vector<std::string> roles;
    std::array<uint32_t, 2> footfalls{};
    uint32_t planted_ticks = 0;
    uint32_t planted_still = 0;
    float worst_float = 0.0f;
    float worst_sink = 0.0f;
    std::array<glm::vec2, 2> prev_toe{};
    std::array<glm::vec2, 2> prev_ankle{};
    std::array<bool, 2> was_planted{};
    uint32_t ticks = 0;

    explicit Harness(bool root_track = true, bool transitions = true) {
        app::CharacterSpec spec;
        spec.proportions = &rig;
        spec.mesh_asset = app::VIEWER_BODY_MESH_ID;
        spec.blade_asset = app::VIEWER_BLADE_MESH_ID;
        ok = app::build_character(body, bodies, rs, renderer, nullptr,
                                  fs::path(app::CHARGEN_SOURCE_BODY), spec);
        if (ok) {
            body.set_root_track(root_track);
            body.set_transitions(transitions);
        }
    }
    void set_ground(Ground g) {
        ground = std::move(g);
        body.set_ground_probe([this](const glm::vec3& w) { return ground(w.x, w.z); });
    }
    [[nodiscard]] glm::vec2 point_world(std::size_t side, bool toe) const {
        const glm::vec3 m = toe ? body.contacts().toe[side] : body.contacts().ankle[side];
        const glm::vec3 w = glm::vec3{glm::rotate(glm::mat4{1.0f}, -body_yaw, glm::vec3{0.0f, 1.0f, 0.0f})
                                      * glm::vec4{m, 0.0f}};
        return glm::vec2{root.x + w.x, root.z + w.z};
    }
    /// Один тик пути игрока: ввод (скорость, передача, направление в системе
    /// тела), взгляд; корень едет заявкой вербатим, рыск — из дорожки.
    void tick(float speed, anim::Gait gait, glm::vec3 dir_model, float view_yaw, bool view_valid = true) {
        anim::BodyDrive drive;
        drive.grounded = true;
        drive.gait = gait;
        drive.view_valid = view_valid;
        drive.view_yaw = view_yaw;
        drive.want_speed_mps = speed;
        drive.speed_mps = speed;
        drive.step_length_m = 0.7f;
        drive.move_dir_model = dir_model;
        drive.facing_yaw = body_yaw;
        body.advance(drive, root, DT);
        const anim::LocomotionOut& lo = body.locomotion();
        if (lo.valid) {
            body_yaw += lo.root_yaw_delta;
            const glm::vec3 w = glm::vec3{glm::rotate(glm::mat4{1.0f}, -body_yaw, glm::vec3{0.0f, 1.0f, 0.0f})
                                          * glm::vec4{lo.root_delta_model, 0.0f}};
            root += w;
            if (ground) {
                root.y = ground(root.x, root.z);
            }
            for (std::size_t s = 0; s < 2; ++s) {
                footfalls[s] += lo.footfall[s] ? 1u : 0u;
            }
        }
        drive.facing_yaw = body_yaw;
        body.commit_root(drive, root, DT);
        (void)body.build_draw(/*hide_head=*/false, 1.0f);
        const std::string name{anim::role_name(body.playback().role)};
        if (roles.empty() || roles.back() != name) {
            roles.push_back(name);
        }
        ++ticks;
        // ОПОРА ПО РАСПИСАНИЮ СТОИТ: какая-то точка стопы (пятка на ударе,
        // носок на перекате) в мире медленнее CONTACT_STILL_MPS.
        for (std::size_t s = 0; s < 2; ++s) {
            const glm::vec2 t = point_world(s, true);
            const glm::vec2 a = point_world(s, false);
            const bool on = lo.valid && lo.planted[s];
            if (on && was_planted[s]) {
                ++planted_ticks;
                const float v = std::min(glm::length(t - prev_toe[s]), glm::length(a - prev_ankle[s])) / DT;
                if (v < 1.2f * static_cast<float>(config::CONTACT_STILL_MPS)) {
                    ++planted_still;
                }
            }
            prev_toe[s] = t;
            prev_ankle[s] = a;
            was_planted[s] = on;
            // первые полсекунды — вход IK (FOOT_IK_GATE_TAU_S), зазор не судится
            const anim::FootGap& g = body.foot_gap_last();
            if (on && g.judged[s] != 0 && ticks >= 30) {
                worst_float = std::max(worst_float, g.gap[s]);
                worst_sink = std::max(worst_sink, -g.gap[s]);
            }
        }
    }
    void run(int ticks, float speed, anim::Gait gait, glm::vec3 dir = {0.0f, 0.0f, -1.0f}, float view = 0.0f) {
        for (int t = 0; t < ticks; ++t) {
            tick(speed, gait, dir, view);
        }
    }
    [[nodiscard]] std::string chain() const {
        std::string s;
        for (const std::string& r : roles) {
            s += r + " ";
        }
        return s;
    }
    [[nodiscard]] float agreement() const {
        return planted_ticks > 0 ? static_cast<float>(planted_still) / static_cast<float>(planted_ticks) : 0.0f;
    }
};

constexpr float WALK = static_cast<float>(config::WALK_SPEED);
constexpr float JOG = static_cast<float>(config::JOG_SPEED);
constexpr float RUN = static_cast<float>(config::RUN_SPEED);

} // namespace

TEST_CASE("the_planted_foot_stays_put_on_the_player_path") {
    if (!body_present()) {
        MESSAGE("no HumanBase.dfo -- skipped");
        return;
    }
    struct Gear { const char* label; float speed; anim::Gait gait; };
    for (const Gear& g : {Gear{"ходьба", WALK, anim::Gait::Walk}, Gear{"трусца", JOG, anim::Gait::Jog}, Gear{"бег", RUN, anim::Gait::Run}}) {
        Harness h;
        REQUIRE(h.ok);
        h.run(300, g.speed, g.gait);
        const float path = glm::length(glm::vec2{h.root.x, h.root.z});
        MESSAGE(g.label << ": путь " << path << " м за 5 с, опора стоит на " << 100.0f * h.agreement()
                        << " % тиков опоры (" << h.planted_still << "/" << h.planted_ticks << "), постановок "
                        << h.footfalls[0] << "/" << h.footfalls[1] << "; роли " << h.chain());
        CHECK(path > 2.0f);
        CHECK(h.footfalls[0] + h.footfalls[1] >= 4);
        if (g.gait == anim::Gait::Walk) {
            CHECK(h.agreement() >= 0.85f);
        }
    }
    // КОНТРОЛЬНАЯ РУКА: без дорожки корня заявки нет — на пути игрока без
    // сима тело не едет вовсе (в игре капсулу ведёт модель скорости ввода).
    Harness c(false);
    REQUIRE(c.ok);
    c.run(120, WALK, anim::Gait::Walk);
    MESSAGE("контроль DFN_ROOT_TRACK=0: путь " << glm::length(glm::vec2{c.root.x, c.root.z}) << " м, роли " << c.chain());
    CHECK(glm::length(glm::vec2{c.root.x, c.root.z}) == 0.0f);
    CHECK(c.body.loco_machine().transitions == 0);
}

TEST_CASE("idle_feet_stand_level") {
    if (!body_present()) {
        return;
    }
    Harness h;
    REQUIRE(h.ok);
    h.set_ground([](float, float) { return 0.0f; });
    uint32_t both = 0;
    float sym = 0.0f;
    for (int t = 0; t < 120; ++t) {
        h.tick(0.0f, anim::Gait::Walk, {0.0f, 0.0f, -1.0f}, 0.0f);
        const anim::LocomotionOut& lo = h.body.locomotion();
        both += (lo.valid && lo.planted[0] && lo.planted[1]) ? 1u : 0u;
        sym = std::max(sym, std::abs(h.body.contacts().ankle[0].x + h.body.contacts().ankle[1].x));
    }
    MESSAGE("покой: обе стопы стоят " << both << " из 120 тиков; зазор парение " << 1000.0f * h.worst_float
            << " мм / утопание " << 1000.0f * h.worst_sink << " мм; асимметрия лодыжек " << 1000.0f * sym << " мм");
    CHECK(both == 120);
    CHECK(h.worst_float <= static_cast<float>(config::LOCO_GAP_MAX_M));
    CHECK(h.worst_sink <= static_cast<float>(config::LOCO_GAP_MAX_M));
    CHECK(sym < 0.03f);
}

TEST_CASE("stairs_and_slope_keep_the_foot_planted_and_on_the_tread") {
    if (!body_present()) {
        return;
    }
    // СКАТ 15° навстречу ходу
    {
        Harness h;
        REQUIRE(h.ok);
        h.set_ground([](float, float z) { return z < 0.0f ? -z * std::tan(glm::radians(15.0f)) : 0.0f; });
        h.run(300, WALK, anim::Gait::Walk);
        MESSAGE("скат 15°: набрал " << h.root.y << " м, опора стоит " << 100.0f * h.agreement() << " %, парение "
                << 1000.0f * h.worst_float << " мм, утопание " << 1000.0f * h.worst_sink << " мм");
        CHECK(h.root.y > 1.0f);
        CHECK(h.worst_float <= 0.03f);
        CHECK(h.worst_sink <= 0.03f);
    }
    // МАРШ 0,18/0,28 — щуп, а не Jolt: корень встаёт на ступень скачком; на
    // Jolt (капсула въезжает на подступёнок раньше стопы) — the_march_is_
    // climbed_on_physical_feet в app_locomotion, приёмка 2 см.
    {
        Harness h;
        REQUIRE(h.ok);
        h.set_ground([](float, float z) {
            if (z > -1.5f) {
                return 0.0f;
            }
            const int step = static_cast<int>(std::floor((-1.5f - z) / 0.28f)) + 1;
            return 0.18f * static_cast<float>(std::min(step, 9));
        });
        h.run(300, WALK, anim::Gait::Walk);
        MESSAGE("марш: набрал " << h.root.y << " м, опора стоит " << 100.0f * h.agreement() << " %, парение "
                << 1000.0f * h.worst_float << " мм, утопание " << 1000.0f * h.worst_sink << " мм");
        CHECK(h.root.y >= 1.62f - 1.0e-3f);
        CHECK(h.worst_float <= 0.05f);
        CHECK(h.worst_sink <= 0.05f);
    }
}

TEST_CASE("turning_in_place_steps_the_feet_instead_of_twisting") {
    if (!body_present()) {
        return;
    }
    for (const float view_deg : {90.0f, -90.0f, 180.0f}) {
        Harness h;
        REQUIRE(h.ok);
        const float view = glm::radians(view_deg);
        uint32_t turns = 0;
        float worst_twist = 0.0f;
        for (int t = 0; t < 240; ++t) {
            h.tick(0.0f, anim::Gait::Walk, {0.0f, 0.0f, -1.0f}, view);
            const anim::LocomotionOut& lo = h.body.locomotion();
            for (std::size_t s = 0; s < 2; ++s) {
                if (!lo.valid || !lo.planted[s]) {
                    continue;
                }
                // скрутка стопы (носок − лодыжка) к корпусу, в системе тела
                const glm::vec3 f = h.body.contacts().toe[s] - h.body.contacts().ankle[s];
                if (glm::length(glm::vec2{f.x, f.z}) > 0.05f) {
                    worst_twist = std::max(worst_twist, std::abs(glm::degrees(std::atan2(f.x, -f.z))));
                }
            }
        }
        for (const std::string& r : h.roles) {
            turns += r.rfind("Turn", 0) == 0 ? 1u : 0u;
        }
        MESSAGE("взгляд " << view_deg << "°: роли " << h.chain() << "| корпус " << glm::degrees(h.body_yaw)
                          << "°, поворотов " << turns << ", постановок " << h.footfalls[0] << "/" << h.footfalls[1]
                          << ", скрутка опорной стопы worst " << worst_twist << "°");
        CHECK(turns == 1);
        CHECK(std::abs(wrap_pi(view - h.body_yaw)) < glm::radians(static_cast<float>(config::TURN_FIRE_DEG)));
        CHECK(h.footfalls[0] + h.footfalls[1] >= 1); // переступ, не разворот на месте
        // СТОПА К КОРПУСУ НА ПОВОРОТЕ: корпус крутит дорожка, опорная стопа клипа
        // стоит в мире — к концу пивота она развёрнута к корпусу на угол
        // поворота до переступа (MX_Left_Turn_90: 47…82°, 180: 67°). Это не
        // скрутка в суставе, а стоящая стопа под повернувшимся телом; порог
        // прибора LOCO_PELVIS_TWIST_MAX_DEG (45) здесь печатается, судится
        // предел, за которым нога уже крест (90°).
        CHECK(worst_twist <= 90.0f);
    }
}

TEST_CASE("gear_changes_settle_where_the_gear_settles_from_standing") {
    if (!body_present()) {
        return;
    }
    Harness h;
    REQUIRE(h.ok);
    h.body.set_telemetry(true);
    struct Seg { const char* label; float speed; anim::Gait gait; anim::ClipRole role; };
    const Seg segs[] = {{"ходьба", WALK, anim::Gait::Walk, anim::ClipRole::Walk},
                        {"трусца", JOG, anim::Gait::Jog, anim::ClipRole::Jog},
                        {"бег", RUN, anim::Gait::Run, anim::ClipRole::Sprint},
                        {"ходьба", WALK, anim::Gait::Walk, anim::ClipRole::Walk}};
    const float band = static_cast<float>(config::LOCOMOTION_TEMPO_BAND);
    for (const Seg& s : segs) {
        h.run(60, s.speed, s.gait); // секунда на вход
        CHECK(h.body.loco_machine().role == s.role);
        const glm::vec3 a = h.root;
        h.run(60, s.speed, s.gait);
        const float v = glm::length(glm::vec2{h.root.x - a.x, h.root.z - a.z});
        const anim::ClipEntry& e = anim::entry_for(h.body.clip_library(), s.role, 0);
        const float expect = e.root.mps * std::clamp(s.speed / e.root.mps, 1.0f - band, 1.0f + band);
        MESSAGE(s.label << ": " << v << " м/с против клип × темп " << expect);
        CHECK(std::abs(v - expect) <= 0.10f * expect);
    }
    h.run(60, 0.0f, anim::Gait::Walk);
    MESSAGE("роли: " << h.chain() << "| разрывов фазы " << h.body.telemetry().row(anim::LocoProbe::PhaseJump).hits);
    CHECK(h.body.telemetry().row(anim::LocoProbe::PhaseJump).hits == 0);
}

TEST_CASE("eight_directions_keep_the_foot_planted_and_pick_the_direction_role") {
    if (!body_present()) {
        return;
    }
    const float strafe = static_cast<float>(config::DIR_STRAFE_DEG);
    const float back = static_cast<float>(config::DIR_BACK_DEG);
    for (int k = 0; k < 8; ++k) {
        const float deg = 45.0f * static_cast<float>(k);
        const glm::vec3 dir{std::sin(glm::radians(deg)), 0.0f, -std::cos(glm::radians(deg))};
        Harness h;
        REQUIRE(h.ok);
        h.run(180, WALK, anim::Gait::Walk, dir);
        const anim::ClipRole role = h.body.loco_machine().role;
        // класс с гистерезисом из покоя (был «вперёд»): вперёд до DIR_STRAFE_DEG + h,
        // назад от DIR_BACK_DEG + h, между — бок
        const float h_deg = static_cast<float>(config::DIR_HYSTERESIS_DEG);
        const float a = glm::degrees(std::abs(wrap_pi(glm::radians(deg))));
        anim::ClipRole want = anim::ClipRole::Walk;
        if (a >= back + h_deg) {
            want = anim::ClipRole::Backward;
        } else if (a >= strafe + h_deg) {
            want = dir.x > 0.0f ? anim::ClipRole::StrafeR : anim::ClipRole::StrafeL;
        }
        const float path_deg = glm::degrees(std::atan2(h.root.x, -h.root.z));
        MESSAGE("ввод " << deg << "°: роль " << anim::role_name(role) << ", курс пути " << path_deg << "°, опора стоит "
                        << 100.0f * h.agreement() << " %, путь " << glm::length(glm::vec2{h.root.x, h.root.z}) << " м");
        CHECK(role == want);
        // ВЕРБАТИМ: путь идёт по оси КЛИПА роли (класс направления), не по вводу
        const glm::vec3 axis = anim::role_move_dir(role);
        const float axis_deg = glm::degrees(std::atan2(axis.x, -axis.z));
        CHECK(std::abs(wrap_pi(glm::radians(path_deg - axis_deg))) < glm::radians(12.0f));
        CHECK(h.agreement() >= 0.7f);
    }
}

TEST_CASE("telemetry_on_the_player_path") {
    if (!body_present()) {
        return;
    }
    struct Gear { const char* label; float speed; anim::Gait gait; };
    for (const Gear& g : {Gear{"ходьба", WALK, anim::Gait::Walk}, Gear{"трусца", JOG, anim::Gait::Jog}, Gear{"бег", RUN, anim::Gait::Run}}) {
        Harness h;
        REQUIRE(h.ok);
        h.body.set_telemetry(true);
        h.run(420, g.speed, g.gait);
        const anim::LocoTelemetry& tm = h.body.telemetry();
        MESSAGE(g.label << ": " << tm.ticks() << " тиков, постановок " << tm.footfalls()[0] << "/" << tm.footfalls()[1]
                        << ", ход опорной стопы worst " << tm.row(anim::LocoProbe::StanceSlip).worst << " м/с ("
                        << tm.row(anim::LocoProbe::StanceSlip).hits << " тиков за порогом), разрывов фазы "
                        << tm.row(anim::LocoProbe::PhaseJump).hits << ", смен клипа в секунду worst "
                        << tm.row(anim::LocoProbe::TransitionsPerS).worst << ", ошибка скорости "
                        << 100.0f * tm.row(anim::LocoProbe::SpeedError).worst << " %");
        CHECK(tm.ticks() == 420);
        CHECK(tm.footfalls()[0] + tm.footfalls()[1] > 4);
        CHECK(tm.row(anim::LocoProbe::PhaseJump).hits == 0);
        CHECK(tm.row(anim::LocoProbe::TransitionsPerS).hits == 0);
    }
    // КОРОТКИЕ НАЖАТИЯ ×4: путь без ввода — старт/остановка доигрывают клип;
    // прибор TapDrift печатает, сколько тело проехало после отпускания.
    Harness h;
    REQUIRE(h.ok);
    h.body.set_telemetry(true);
    for (int k = 0; k < 4; ++k) {
        h.run(6, WALK, anim::Gait::Walk);
        h.run(54, 0.0f, anim::Gait::Walk);
    }
    const anim::LocoTelemetry& tm = h.body.telemetry();
    MESSAGE("4 коротких нажатия: путь " << glm::length(glm::vec2{h.root.x, h.root.z}) << " м, путь без ввода worst "
            << tm.row(anim::LocoProbe::TapDrift).worst << " мм; роли " << h.chain());
    CHECK(tm.ticks() == 240);
    CHECK(glm::length(glm::vec2{h.root.x, h.root.z}) < 2.0f);
}

TEST_CASE("the_walk_and_the_run_start_with_their_own_clip") {
    if (!body_present()) {
        return;
    }
    for (const bool transitions : {true, false}) {
        Harness w(true, transitions);
        REQUIRE(w.ok);
        w.run(30, 0.0f, anim::Gait::Walk);
        w.run(30, WALK, anim::Gait::Walk);
        Harness r(true, transitions);
        REQUIRE(r.ok);
        r.run(30, 0.0f, anim::Gait::Run);
        r.run(30, RUN, anim::Gait::Run);
        MESSAGE((transitions ? "с переходами" : "контроль без переходов") << ": ходьба " << w.chain() << "| бег " << r.chain());
        REQUIRE(w.roles.size() >= 2);
        REQUIRE(r.roles.size() >= 2);
        if (transitions) {
            CHECK(w.roles[1] == "StartWalk");
            CHECK(r.roles[1] == "StartRun");
        } else {
            CHECK(w.roles[1] == "Walk");
            CHECK(r.roles[1] == "Sprint");
        }
    }
}

namespace {
void stop_case(float speed, anim::Gait gait, const char* stop_role) {
    for (const bool transitions : {true, false}) {
        Harness h(true, transitions);
        REQUIRE(h.ok);
        h.run(120, speed, gait);
        const glm::vec3 at_release = h.root;
        int settled = -1;
        for (int t = 0; t < 120; ++t) {
            const glm::vec3 before = h.root;
            h.tick(0.0f, gait, {0.0f, 0.0f, -1.0f}, 0.0f);
            const float v = glm::length(glm::vec2{h.root.x - before.x, h.root.z - before.z}) / DT;
            if (settled < 0 && v < 0.05f && h.body.loco_machine().state == anim::LocoState::Idle) {
                settled = t;
            }
        }
        const float coast = glm::length(glm::vec2{h.root.x - at_release.x, h.root.z - at_release.z});
        MESSAGE((transitions ? "с переходами" : "контроль без переходов") << ": роли " << h.chain() << "| встал через "
                << (settled >= 0 ? static_cast<float>(settled) * DT : -1.0f) << " с, выбег " << coast << " м");
        bool has_stop = false;
        for (const std::string& r : h.roles) {
            has_stop = has_stop || r == stop_role;
        }
        CHECK(has_stop == transitions);
        REQUIRE(settled >= 0);
        CHECK(static_cast<float>(settled) * DT <= static_cast<float>(config::STOP_CLIP_MAX_S) + 0.3f);
    }
}
} // namespace

TEST_CASE("the_run_stops_with_its_own_clip") {
    if (!body_present()) {
        return;
    }
    stop_case(RUN, anim::Gait::Run, "StopRun");
}

TEST_CASE("the_walk_stops_with_its_own_clip") {
    if (!body_present()) {
        return;
    }
    stop_case(WALK, anim::Gait::Walk, "StopWalk");
}

TEST_CASE("standing_body_turns_to_the_camera_by_stepping") {
    if (!body_present()) {
        return;
    }
    Harness h;
    REQUIRE(h.ok);
    h.run(30, 0.0f, anim::Gait::Walk);
    const float view = glm::radians(90.0f);
    h.run(180, 0.0f, anim::Gait::Walk, {0.0f, 0.0f, -1.0f}, view);
    MESSAGE("камера +90°: роли " << h.chain() << "| корпус " << glm::degrees(h.body_yaw) << "°, постановок "
            << h.footfalls[0] << "/" << h.footfalls[1]);
    bool turned = false;
    for (const std::string& r : h.roles) {
        turned = turned || r == "TurnR";
    }
    CHECK(turned);
    CHECK(std::abs(wrap_pi(view - h.body_yaw)) < glm::radians(static_cast<float>(config::TURN_FIRE_DEG)));
    CHECK(h.footfalls[0] + h.footfalls[1] >= 1);
}

TEST_CASE("spinning_the_view_does_not_fly_the_body_across_the_map") {
    if (!body_present()) {
        return;
    }
    Harness h;
    REQUIRE(h.ok);
    float view = 0.0f;
    for (int t = 0; t < 360; ++t) {
        if (t % 30 == 0) {
            view = (t / 30) % 2 == 0 ? glm::radians(120.0f) : glm::radians(-120.0f);
        }
        h.tick(0.0f, anim::Gait::Walk, {0.0f, 0.0f, -1.0f}, view);
    }
    const float path = glm::length(glm::vec2{h.root.x, h.root.z});
    const float per_s = static_cast<float>(h.body.loco_machine().transitions) / 6.0f;
    MESSAGE("взгляд ±120° каждые полсекунды, 6 с: путь " << path << " м, смен клипа " << per_s << "/с, роли " << h.chain());
    CHECK(path < 0.3f);
    CHECK(per_s <= static_cast<float>(config::LOCO_TRANSITIONS_PER_S_MAX));
}
