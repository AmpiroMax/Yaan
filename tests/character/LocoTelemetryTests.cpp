/*
Module: tests
File: tests/character/LocoTelemetryTests.cpp

Responsibility:
- ПРИБОРЫ ЛОКОМОЦИИ НА СИНТЕТИКЕ (engine/anim/sources/LocoTelemetry.h): каждый
  детектор получает тик, в котором дефект заложен руками, и обязан его
  посчитать и засчитать сверх порога реестра; тик без дефекта — ноль
  срабатываний. Скелет — восемь суставов (корень, таз, две ноги), без файла.
Key items:
- clean_ticks_do_not_trip_anything: покой без дефектов — hits 0.
- slide / tap_drift / thigh_jerk / phase_jump / ankle_cross / lateral_drift /
  start_foot: по детектору на случай.
Dependencies:
- Uses: doctest, engine/anim (LocoTelemetry, FootIk, ClipPlayer, Body),
  engine/core (Constants).
- Used by: ctest (character_loco_telemetry).
AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Пороги — строки реестра; здесь дефекты заложены с запасом ×2 от порога.
*/
#include "engine/anim/sources/LocoTelemetry.h"
#include "engine/core/config/sources/Constants.h"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace dfn;

namespace {

constexpr float DT = 1.0f / 60.0f;

enum J : int32_t { Root = 0, Pelvis, ThighL, ShinL, FootL, ThighR, ShinR, FootR, COUNT };

struct Bench {
    skel::Skeleton skeleton;
    anim::FootIkSetup setup;
    anim::LocoTelemetry tm;
    std::vector<anim::JointLocal> pose;
    anim::ContactState contacts;
    anim::FootLockState locks;
    anim::ClipPlayback play;
    anim::LocomotionOut loco;
    anim::BodyDrive drive;
    anim::BodyRoot root;
    anim::BodyRoot root_prev;
    std::array<glm::vec3, 2> contact_world{};
    anim::FootGap gap{};

    Bench() {
        const auto add = [&](const char* name, int32_t parent, glm::vec3 t) {
            skel::SkeletonJoint j;
            j.name = name;
            j.parent = parent;
            j.bind_translation = t;
            skeleton.joints.push_back(j);
        };
        add("root", -1, {0.0f, 0.0f, 0.0f});
        add("pelvis", Root, {0.0f, 0.95f, 0.0f});
        add("thigh.L", Pelvis, {0.1f, 0.0f, 0.0f});
        add("shin.L", ThighL, {0.0f, -0.45f, 0.0f});
        add("foot.L", ShinL, {0.0f, -0.45f, 0.0f});
        add("thigh.R", Pelvis, {-0.1f, 0.0f, 0.0f});
        add("shin.R", ThighR, {0.0f, -0.45f, 0.0f});
        add("foot.R", ShinR, {0.0f, -0.45f, 0.0f});
        setup.hip = {ThighL, ThighR};
        setup.knee = {ShinL, ShinR};
        setup.ankle = {FootL, FootR};
        setup.roots = {Root};
        tm.reset(skeleton, setup);
        pose.resize(skeleton.size());
        for (std::size_t i = 0; i < pose.size(); ++i) {
            pose[i].translation = skeleton.joints[i].bind_translation;
        }
        // слегка согнутые колени, чтобы сгиб не был нулём
        pose[ShinL].rotation = glm::angleAxis(0.2f, glm::vec3{1.0f, 0.0f, 0.0f});
        pose[ShinR].rotation = glm::angleAxis(0.2f, glm::vec3{1.0f, 0.0f, 0.0f});
        contacts.valid = true;
        contacts.support = {0.5f, 0.5f};
        contacts.weight = {1.0f, 1.0f};
        contacts.ankle = {glm::vec3{0.1f, 0.05f, 0.0f}, glm::vec3{-0.1f, 0.05f, 0.0f}};
        contacts.point = contacts.ankle;
        drive.grounded = true;
        drive.step_length_m = 0.7f;
        drive.move_dir_model = {0.0f, 0.0f, -1.0f};
        play.role = anim::ClipRole::Idle;
        loco.valid = true;
        contact_world = contacts.ankle;
        // прогрев прибора: первые WARM_TICKS тиков он не судит
        for (uint32_t i = 0; i < anim::LocoTelemetry::WARM_TICKS + 1; ++i) {
            tick();
        }
    }
    void tick() {
        anim::LocoTick t;
        t.dt = DT;
        t.pose = pose;
        t.contacts = &contacts;
        t.locks = &locks;
        t.contact_world = contact_world;
        t.gap = gap;
        t.play = &play;
        t.loco = &loco;
        t.drive = &drive;
        t.root = root;
        t.root_prev = root_prev;
        tm.push(t);
        root_prev = root;
    }
    [[nodiscard]] const anim::LocoProbeRow& row(anim::LocoProbe p) const { return tm.row(p); }
};

} // namespace

TEST_CASE("clean_ticks_do_not_trip_anything") {
    Bench b;
    for (int i = 0; i < 30; ++i) {
        b.tick();
    }
    CHECK(b.tm.ticks() == 30 + anim::LocoTelemetry::WARM_TICKS + 1);
    CHECK(b.tm.total_hits() == 0);
    CHECK(b.tm.summary_lines().size() == 4);
    CHECK(b.tm.report().find("RED") == std::string::npos);
}

TEST_CASE("a_locked_foot_away_from_its_anchor_is_slide") {
    Bench b;
    b.locks.locked[0] = true;
    b.locks.strength[0] = 1.0f;
    b.locks.anchor[0] = b.contact_world[0];
    b.tick();
    CHECK(b.row(anim::LocoProbe::Residual).last == doctest::Approx(0.0f));
    const float limit = static_cast<float>(config::FOOT_SLIDE_MAX_M);
    b.contact_world[0].x += 2.0f * limit;
    b.tick();
    // остаток до замка — показание, не срабатывание
    CHECK(b.row(anim::LocoProbe::Residual).last == doctest::Approx(2000.0f * limit).epsilon(0.01));
    CHECK(b.row(anim::LocoProbe::Residual).hits == 0);
    // кадр после замка: лодыжка ушла от якоря — это снос
    // лодыжка L стенда: бедро +0.1 по X, колено согнуто на 0.2 рад — стопа ушла по Z
    b.locks.anchor[0] = glm::vec3{0.1f, 0.0f, -0.45f * std::sin(0.2f)};
    b.tm.push_frame(b.pose, b.root, 1.0f, b.locks);
    CHECK(b.row(anim::LocoProbe::Slide).hits == 0);
    b.locks.anchor[0].x += 2.0f * limit;
    b.tick(); // кадры чаще тика прибор не судит
    b.tm.push_frame(b.pose, b.root, 1.0f, b.locks);
    CHECK(b.row(anim::LocoProbe::Slide).last == doctest::Approx(2000.0f * limit).epsilon(0.01));
    CHECK(b.row(anim::LocoProbe::Slide).hits == 1);
}

TEST_CASE("travel_without_input_is_tap_drift") {
    Bench b;
    b.play.role = anim::ClipRole::Walk;
    b.drive.want_speed_mps = 0.0f;
    const float limit = static_cast<float>(config::LOCO_TAP_DRIFT_MAX_M);
    for (int i = 0; i < 20; ++i) {
        b.root.ground.z -= 2.0f * limit / 20.0f;
        b.tick();
    }
    CHECK(b.row(anim::LocoProbe::TapDrift).worst == doctest::Approx(2000.0f * limit).epsilon(0.02));
    CHECK(b.row(anim::LocoProbe::TapDrift).hits > 0);
    // нажали — эпизод закрыт, показание сброшено
    b.drive.want_speed_mps = 1.5f;
    b.tick();
    CHECK(b.row(anim::LocoProbe::TapDrift).last == 0.0f);
}

TEST_CASE("a_thigh_that_snaps_is_a_jerk") {
    Bench b;
    b.tick();
    b.tick();
    b.tick();
    CHECK(b.row(anim::LocoProbe::ThighAccel).hits == 0);
    // 10° за один тик из покоя: ω = 0.17/DT ≈ 10 рад/с, α ≈ 630 рад/с² — сверх 400
    b.pose[ThighL].rotation = glm::angleAxis(glm::radians(10.0f), glm::vec3{1.0f, 0.0f, 0.0f});
    b.tick();
    CHECK(b.row(anim::LocoProbe::ThighAccel).last
          > static_cast<float>(config::LOCO_JOINT_ACCEL_MAX_RADPS2));
    CHECK(b.row(anim::LocoProbe::ThighAccel).hits == 1);
}

TEST_CASE("a_phase_jump_on_role_change_is_counted") {
    Bench b;
    b.play.role = anim::ClipRole::Walk;
    b.play.phase = 0.1f;
    b.tick();
    b.play.role = anim::ClipRole::Jog;
    b.play.phase = 0.6f;
    b.tick();
    CHECK(b.row(anim::LocoProbe::PhaseJump).last == doctest::Approx(0.5f));
    CHECK(b.row(anim::LocoProbe::PhaseJump).hits == 1);
    CHECK(b.tm.role_changes() == 2); // прогрев шёл в покое: покой→ходьба→трусца
}

TEST_CASE("the_left_ankle_right_of_the_right_one_is_a_cross") {
    Bench b;
    b.tick();
    CHECK(b.row(anim::LocoProbe::AnkleCross).hits == 0);
    // левое бедро уводит стопу поперёк на 25 см: лодыжка L при x=−0.15, R при −0.1
    b.pose[ThighL].translation.x = -0.15f;
    b.tick();
    CHECK(b.row(anim::LocoProbe::AnkleCross).last == doctest::Approx(50.0f).epsilon(0.02));
    CHECK(b.row(anim::LocoProbe::AnkleCross).hits == 1);
}

TEST_CASE("sideways_creep_under_forward_input_is_lateral_drift") {
    Bench b;
    b.play.role = anim::ClipRole::Walk;
    b.drive.want_speed_mps = 1.5f;
    const float limit = static_cast<float>(config::LOCO_DRIFT_MAX_M);
    // змейка мерится от постановки до постановки той же стопы: левая ставится
    // каждые 15 тиков, за цикл тело уходит вбок на два порога
    for (int i = 0; i < 46; ++i) {
        b.root.ground.z -= 0.02f;                // вперёд, как заказано
        b.root.ground.x += 2.0f * limit / 15.0f; // и вбок — змейка
        b.loco.footfall[0] = (i % 15) == 0;
        b.tick();
    }
    CHECK(b.row(anim::LocoProbe::LateralDrift).worst == doctest::Approx(2000.0f * limit).epsilon(0.02));
    CHECK(b.row(anim::LocoProbe::LateralDrift).hits > 0);
}

TEST_CASE("the_first_foot_to_lift_after_a_start_is_the_start_foot") {
    Bench b;
    b.tick();
    b.play.role = anim::ClipRole::Walk;
    b.drive.want_speed_mps = 1.5f;
    b.tick();
    b.contacts.support[1] = 0.0f; // правая ушла первой
    b.tick();
    CHECK(b.tm.start_foot()[0] == 0);
    CHECK(b.tm.start_foot()[1] == 1);
}

TEST_CASE("a_floating_support_foot_is_a_gap") {
    Bench b;
    b.gap.gap = {2.0f * static_cast<float>(config::LOCO_GAP_MAX_M), 0.0f};
    b.gap.judged = {1, 1};
    b.tick();
    CHECK(b.row(anim::LocoProbe::Gap).hits == 1);
}
