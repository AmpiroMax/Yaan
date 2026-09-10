/*
Module: tests
File: tests/character/RootTrackTests.cpp

Responsibility:
- ПРИБОР ДОРОЖКИ КОРНЯ (engine/anim/sources/ClipPlayer.h RootTrack, §16): на
  теле HumanBase дорожка сустава root воспроизводит авторский ход и рыск
  клипа (сумма ходов за петлю = длина дорожки), стык петли непрерывен, варп
  рыска масштабирует только рыск, поза выборки «на месте» (корень = кадр 0),
  расписание контактов даёт по одной постановке на стопу за цикл ходьбы, а
  у покоя — обе стопы внизу весь клип; у старта есть фаза передачи цикла.
Key items:
- the_root_track_reproduces_the_authored_root (контроль — покой: 0)
- the_root_delta_wraps_at_the_loop_point
- warping_a_turn_scales_its_yaw_and_nothing_else (контроль — warp 1)
- the_sampled_pose_stays_in_place
- the_contact_schedule_finds_one_plant_per_foot_per_cycle (контроль — покой)
- a_start_clip_hands_off_to_the_cycle
Dependencies:
- Uses: doctest, engine/anim, tests/character/ClipTestModel.h, HumanBase.dfo.
- Used by: ctest (character_root_track).
AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Пороги — из реестра; знак рыска — сим'а (+ по часовой сверху: правый поворот +).
*/
#include "engine/anim/sources/ClipPlayer.h"
#include "engine/core/config/sources/Constants.h"
#include "tests/character/ClipTestModel.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace dfn;

namespace {

const anim::ClipEntry& role(const Model& m, anim::ClipRole r) {
    return m.lib.role[anim::role_index(r)];
}

} // namespace

TEST_CASE("the_root_track_reproduces_the_authored_root") {
    Model m;
    REQUIRE_MESSAGE(load(m), "bake the character first (target dfn_characters)");
    for (const anim::ClipRole r : {anim::ClipRole::Walk, anim::ClipRole::StartWalk,
                                   anim::ClipRole::Backward, anim::ClipRole::StrafeL,
                                   anim::ClipRole::StopRun}) {
        const anim::ClipEntry& e = role(m, r);
        REQUIRE(e.present());
        const anim::RootTrack& t = e.root;
        MESSAGE(anim::role_name(r) << ": дорожка " << std::string(t.valid ? "есть" : "НЕТ") << ", путь "
                                   << t.total_m << " м, " << t.mps << " м/с, рыск "
                                   << glm::degrees(t.total_yaw) << "°");
        CHECK(t.valid);
        CHECK(t.total_m > 0.3f);
        // сумма ходов по 64 шагам фазы — ровно длина дорожки (± 1 мм)
        float sum = 0.0f;
        for (int i = 0; i < 64; ++i) {
            const anim::RootDelta d = anim::root_track_delta(
                t, static_cast<float>(i) / 64.0f, static_cast<float>(i + 1) / 64.0f, false);
            sum += glm::length(d.xz);
        }
        CHECK(sum == doctest::Approx(t.total_m).epsilon(0.02));
    }
    // КОНТРОЛЬНАЯ РУКА: покой не едет и не крутится
    const anim::RootTrack& idle = role(m, anim::ClipRole::Idle).root;
    CHECK_FALSE(idle.valid);
    CHECK(idle.total_m == 0.0f);
    CHECK(idle.total_yaw == 0.0f); // без дорожки массивы обнулены, не «почти ноль»
}

TEST_CASE("the_root_delta_wraps_at_the_loop_point") {
    Model m;
    REQUIRE(load(m));
    const anim::RootTrack& t = role(m, anim::ClipRole::Walk).root;
    REQUIRE(t.valid);
    const anim::RootDelta w = anim::root_track_delta(t, 0.9f, 0.1f, true);
    const anim::RootDelta a = anim::root_track_delta(t, 0.9f, 1.0f, false);
    const anim::RootDelta b = anim::root_track_delta(t, 0.0f, 0.1f, false);
    CHECK(glm::length(w.xz - (a.xz + b.xz)) < 1.0e-6f);
    CHECK(w.yaw == doctest::Approx(a.yaw + b.yaw).epsilon(1.0e-6));
    // и через стык ход не рвётся: 0.9→0.1 (0.2 цикла) ≈ 0.4→0.6
    const anim::RootDelta mid = anim::root_track_delta(t, 0.4f, 0.6f, true);
    MESSAGE("ход за 0,2 цикла через стык " << glm::length(w.xz) << " м, в середине "
                                             << glm::length(mid.xz) << " м");
    CHECK(glm::length(w.xz) == doctest::Approx(glm::length(mid.xz)).epsilon(0.25));
}

TEST_CASE("warping_a_turn_scales_its_yaw_and_nothing_else") {
    Model m;
    REQUIRE(load(m));
    const anim::ClipEntry& l = role(m, anim::ClipRole::TurnL);
    const anim::ClipEntry& r = role(m, anim::ClipRole::TurnR);
    REQUIRE(l.present());
    REQUIRE(r.present());
    MESSAGE("TurnL рыск " << glm::degrees(l.root.total_yaw) << "°, TurnR "
                          << glm::degrees(r.root.total_yaw) << "° (зеркало: "
                          << std::string(r.mirrored ? "да" : "нет") << ")");
    CHECK(l.root.valid);
    CHECK(r.root.valid);
    // левый поворот — против часовой сверху = отрицательный рыск сим'а
    CHECK(l.root.total_yaw < glm::radians(-60.0f));
    CHECK(r.root.total_yaw > glm::radians(60.0f));
    CHECK(l.root.total_yaw == doctest::Approx(-r.root.total_yaw).epsilon(0.02));
    const anim::RootDelta full = anim::root_track_delta(l.root, 0.0f, 1.0f, false, 1.0f);
    const anim::RootDelta half = anim::root_track_delta(l.root, 0.0f, 1.0f, false, 0.5f);
    CHECK(half.yaw == doctest::Approx(0.5f * full.yaw));
    CHECK(glm::length(half.xz - full.xz) < 1.0e-7f);
    // КОНТРОЛЬНАЯ РУКА: warp 1 — то же самое бит-в-бит
    const anim::RootDelta one = anim::root_track_delta(l.root, 0.0f, 1.0f, false);
    CHECK(one.yaw == full.yaw);
}

TEST_CASE("the_sampled_pose_stays_in_place") {
    Model m;
    REQUIRE(load(m));
    const anim::ClipEntry& e = role(m, anim::ClipRole::StartWalk);
    REQUIRE(e.present());
    REQUIRE(e.root.valid);
    const skel::AnimClip& clip = m.obj.clips[static_cast<std::size_t>(e.clip)];
    std::vector<anim::JointLocal> a(m.obj.skeleton.size());
    std::vector<anim::JointLocal> b(m.obj.skeleton.size());
    anim::sample_clip_pose(m.obj.skeleton, clip, 0.0f, a);
    anim::sample_clip_pose(m.obj.skeleton, clip, 0.6f * clip.duration_s, b);
    int roots = 0;
    for (std::size_t j = 0; j < m.obj.skeleton.size(); ++j) {
        if (m.obj.skeleton.joints[j].parent >= 0) {
            continue;
        }
        ++roots;
        CHECK(glm::length(a[j].translation - b[j].translation) < 1.0e-6f);
        CHECK(std::abs(glm::dot(a[j].rotation, b[j].rotation)) == doctest::Approx(1.0f).epsilon(1.0e-6));
        CHECK(glm::length(a[j].translation - e.root.pose0_t) < 1.0e-6f);
    }
    CHECK(roots >= 1);
    // …а сама дорожка на 0,6 клипа — не ноль (иначе нечего было нейтрализовать)
    CHECK(glm::length(anim::root_track_xz_at(e.root, 0.6f)) > 0.2f);
}

TEST_CASE("the_contact_schedule_finds_one_plant_per_foot_per_cycle") {
    Model m;
    REQUIRE(load(m));
    for (const anim::ClipRole r : {anim::ClipRole::Walk, anim::ClipRole::Backward,
                                   anim::ClipRole::StrafeL, anim::ClipRole::StrafeR}) {
        const anim::ClipEntry& e = role(m, r);
        REQUIRE(e.present());
        for (std::size_t side = 0; side < 2; ++side) {
            const float stance = e.plant_count[side] > 0
                                     ? std::fmod(e.lift_phase[side][0] - e.plant_phase[side][0] + 1.0f, 1.0f)
                                     : 0.0f;
            MESSAGE(anim::role_name(r) << " сторона " << side << ": постановок "
                                       << int(e.plant_count[side]) << ", опора с "
                                       << e.plant_phase[side][0] << " по " << e.lift_phase[side][0]
                                       << " (" << stance << " цикла)");
            CHECK(e.plant_count[side] == 1);
            CHECK(stance > 0.3f);
            CHECK(stance < 0.85f);
        }
    }
    // КОНТРОЛЬНАЯ РУКА: покой — обе стопы стоят весь клип (один отрезок на цикл)
    const anim::ClipEntry& idle = role(m, anim::ClipRole::Idle);
    for (std::size_t side = 0; side < 2; ++side) {
        CHECK(idle.plant_count[side] == 1);
        CHECK(idle.plant_phase[side][0] == idle.lift_phase[side][0]); // полный круг
    }
}

TEST_CASE("a_start_clip_hands_off_to_the_cycle") {
    Model m;
    REQUIRE(load(m));
    const anim::ClipEntry& s = role(m, anim::ClipRole::StartWalk);
    const anim::ClipEntry& w = role(m, anim::ClipRole::Walk);
    REQUIRE(s.present());
    MESSAGE("StartWalk: передача цикла на фазе " << s.handoff_phase << " ("
                                                  << s.handoff_phase * s.duration_s << " с из "
                                                  << s.duration_s << "); цикл " << w.root.mps
                                                  << " м/с, старт до " << s.root.mps << " м/с");
    // Пик скорости старта по дорожке: MX_Start_Walking не доходит до 95 %
    // цикла MX_Walking (1,57 м/с) — тогда передачи по скорости нет (−1),
    // машина передаёт по потолку START_CLIP_MAX_S. Недостача названа вслух.
    float peak = 0.0f;
    const float dphase = 1.0f / static_cast<float>(anim::ROOT_TRACK_POINTS - 1);
    for (uint32_t i = 1; i < anim::ROOT_TRACK_POINTS; ++i) {
        peak = std::max(peak, glm::length(s.root.xz[i] - s.root.xz[i - 1]) / (dphase * s.duration_s));
    }
    const float want = static_cast<float>(config::START_HANDOFF_FRAC) * w.root.mps;
    MESSAGE("StartWalk: пик " << peak << " м/с против порога передачи " << want << " м/с"
                              << (s.handoff_phase > 0.0f ? "" : " — НЕДОСТАЧА: передача по потолку START_CLIP_MAX_S"));
    if (s.handoff_phase > 0.0f) {
        CHECK(peak >= want);
        CHECK(s.handoff_phase < 0.75f); // старт разгоняется до цикла к 1,9 с — факт клипа, машина режет раньше
    } else {
        CHECK(peak < want);
        CHECK(peak > 0.5f * want); // старт всё же разгоняет, а не стоит
    }
}


TEST_CASE("diagnostic_root_track_speed_profile") {
    // ПРОФИЛЬ СКОРОСТИ ДОРОЖКИ ПО ФАЗЕ: где таз клипа замирает (провал до нуля
    // — тик капсулы без хода, видимая запинка) и где рвётся вперёд.
    Model m;
    REQUIRE(load(m));
    for (const anim::ClipRole r : {anim::ClipRole::Walk, anim::ClipRole::Jog, anim::ClipRole::Sprint,
                                   anim::ClipRole::Backward, anim::ClipRole::StrafeL, anim::ClipRole::StrafeR}) {
        const anim::ClipEntry& e = role(m, r);
        if (!e.present() || !e.root.valid) {
            continue;
        }
        const float dphase = 1.0f / static_cast<float>(anim::ROOT_TRACK_POINTS - 1);
        float vmin = 1.0e9f;
        float vmax = 0.0f;
        float at_min = 0.0f;
        int zeros = 0;
        for (uint32_t i = 1; i < anim::ROOT_TRACK_POINTS; ++i) {
            const float v = glm::length(e.root.xz[i] - e.root.xz[i - 1]) / (dphase * e.duration_s);
            if (v < vmin) {
                vmin = v;
                at_min = static_cast<float>(i) * dphase;
            }
            vmax = std::max(vmax, v);
            zeros += v < 0.05f ? 1 : 0;
        }
        const skel::AnimClip& clip = m.obj.clips[static_cast<std::size_t>(e.clip)];
        float t_first = 0.0f;
        float t_last = 0.0f;
        std::size_t keys = 0;
        for (const skel::AnimChannel& ch : clip.channels) {
            if (ch.joint == 0 && ch.path == skel::AnimPath::Translation && !ch.times.empty()) {
                t_first = ch.times.front();
                t_last = ch.times.back();
                keys = ch.times.size();
            }
        }
        MESSAGE(anim::role_name(r) << ": канал root: " << keys << " ключей, первый " << t_first << " с, последний "
                                   << t_last << " с, длительность клипа " << e.duration_s);
        MESSAGE(anim::role_name(r) << ": средняя " << e.root.mps << " м/с, минимум " << vmin << " на фазе " << at_min
                                   << ", максимум " << vmax << ", сегментов медленнее 0,05 м/с: " << zeros << " из 127");
        // цикл ходьбы не замирает: минимум мгновенной скорости выше трети средней
        CHECK(vmin > 0.33f * e.root.mps);
    }
}
