/*
Module: tests/character
File: tests/character/ClipSlideTests.cpp

Responsibility:
- ПРИБОР СНОСА ОПОРНОЙ СТОПЫ ПРИ ПЕРЕМЕЩЕНИИ ОТ СТОПЫ (docs/design/
  LOCOMOTION_GROUNDED.md, прибор №1 на уровне зоны anim, без приложения и
  замка): тик за тиком часы клипа идут своим темпом, корень сдвигается на
  −Δ опорной стопы (RootMotion.h), и мировая точка касания каждой стопы за
  окно опоры обязана стоять на месте. Контрольная рука — прежний шов на той
  же библиотеке: корень едет со скоростью сим'а, а клип подгоняется
  стрид-скейлом; он обязан сносить стопу в разы сильнее (правило 30).
- Скорости передач: у каждой локомоционной роли клип (или смесь) со
  скоростью в полосе темпа от заказа передачи — иначе строка загрузки
  красная, и это должно быть видно тестом, а не кадром.

Key items:
- feet_drive_keeps_the_planted_foot_still: worst spread за окно опоры на
  Walk/Jog/Sprint при перемещении от стопы против контрольной руки.
- gears_reach_their_ordered_speed: достигнутая скорость передачи в полосе темпа.

Dependencies:
- Uses: engine/anim (ClipPlayer, FootIk, RootMotion), tests/character/
  ClipTestModel.h, выпечка assets/objects/characters/HumanBase.dfo.
- Used by: ctest (character_clips_slide).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Правило 30: у каждого утверждения контрольная рука. Пороги — строки
  реестра (FOOT_SLIDE_MAX_M — порог ЗАМКА в приложении; здесь, без замка,
  остаток клипа судится против контрольной руки, а число печатается).
*/

#include <doctest/doctest.h>

#include "engine/anim/sources/ClipPlayer.h"
#include "engine/anim/sources/FootIk.h"
#include "engine/anim/sources/RootMotion.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "tests/character/ClipTestModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

using namespace dfn;

namespace {

constexpr float DT = static_cast<float>(config::SIM_DT);

struct SlideRun {
    float worst_spread_m = 0.0f; ///< худший разброс точки касания за окно опоры
    float travelled_m = 0.0f;    ///< сколько прошёл корень
    uint32_t plants = 0;         ///< сколько окон опоры измерено
};

/// Прогон одной передачи: `feet_drive` — корень едет от стопы; иначе корень
/// едет со скоростью сим'а, а фазу крутит сим (прежний шов).
SlideRun run_gear(Model& m, anim::Gait gait, float speed, bool feet_drive, uint32_t ticks) {
    SlideRun out;
    anim::BodyDrive drive;
    drive.grounded = true;
    drive.gait = gait;
    drive.speed_mps = speed;
    drive.want_speed_mps = speed;
    drive.step_length_m = step_length(speed);
    anim::ClipPlayback play;
    const anim::FootIkSetup setup =
        anim::build_foot_ik(m.obj.skeleton, m.binding, m.lib.contacts);
    REQUIRE(setup.valid());
    anim::FootIkProbe flat;
    flat.valid = true;
    std::vector<anim::JointLocal> sample(m.obj.skeleton.size());
    anim::ContactState prev;
    glm::vec3 root{0.0f};
    anim::RootMotionState rm;
    std::array<std::vector<glm::vec3>, 2> window{};
    const float on = static_cast<float>(config::FOOT_LOCK_ON_WEIGHT);
    const float off = static_cast<float>(config::FOOT_LOCK_OFF_WEIGHT);
    std::array<bool, 2> planted{};
    std::array<bool, 2> tracked_toe{};
    const auto close_window = [&](std::size_t side) {
        if (window[side].size() >= 3) {
            float spread = 0.0f;
            for (std::size_t i = 0; i < window[side].size(); ++i) {
                for (std::size_t j = i + 1; j < window[side].size(); ++j) {
                    const glm::vec3 d = window[side][j] - window[side][i];
                    spread = std::max(spread, glm::length(glm::vec2{d.x, d.z}));
                }
            }
            out.worst_spread_m = std::max(out.worst_spread_m, spread);
            ++out.plants;
        }
        window[side].clear();
    };
    for (uint32_t t = 0; t < ticks; ++t) {
        if (!feet_drive) {
            drive.stride_phase += speed * DT / (2.0f * drive.step_length_m);
            drive.stride_phase -= std::floor(drive.stride_phase);
        }
        anim::advance_playback(m.lib, drive, DT, play);
        REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, m.lib, play,
                                      1.0f, sample));
        const anim::FootIkPlan plan = anim::plan_foot_ik(m.obj.skeleton, setup, flat, sample);
        anim::ContactState curr = anim::contact_state(m.obj.skeleton, setup, plan, sample);
        const glm::vec3 d = anim::root_motion_step(prev, curr, DT, rm);
        if (feet_drive) {
            root += d;
            out.travelled_m += glm::length(d);
        } else {
            root += glm::vec3{0.0f, 0.0f, -speed * DT}; // лицом в −Z
            out.travelled_m += speed * DT;
        }
        // окна опоры: тот же гистерезис, что у замка
        if (t >= 30) { // первую секунду кроссфейд из покоя не судим
            for (std::size_t side = 0; side < 2; ++side) {
                const float w = curr.support[side];
                if (!planted[side] && w >= on) {
                    planted[side] = true;
                    tracked_toe[side] = curr.toe_point[side];
                } else if (planted[side] && w < off) {
                    planted[side] = false;
                    close_window(side);
                } else if (planted[side] && tracked_toe[side] != curr.toe_point[side]) {
                    close_window(side); // перекат: новая точка — новое окно
                    tracked_toe[side] = curr.toe_point[side];
                }
                if (planted[side]) {
                    window[side].push_back(root + curr.point[side]);
                }
            }
        }
        prev = curr;
    }
    return out;
}

} // namespace

TEST_CASE("feet_drive_keeps_the_planted_foot_still") {
    Model m;
    REQUIRE(load(m, true));
    Model legacy;
    REQUIRE(load(legacy, false));
    struct Gear {
        anim::Gait gait;
        float speed;
        const char* name;
    };
    const Gear gears[] = {{anim::Gait::Walk, static_cast<float>(config::WALK_SPEED), "walk"},
                          {anim::Gait::Jog, static_cast<float>(config::JOG_SPEED), "jog"},
                          {anim::Gait::Run, static_cast<float>(config::RUN_SPEED), "run"}};
    for (const Gear& g : gears) {
        const SlideRun feet = run_gear(m, g.gait, g.speed, true, 240);
        const SlideRun sim = run_gear(legacy, g.gait, g.speed, false, 240);
        MESSAGE(g.name << ": от стопы worst " << 1000.0f * feet.worst_spread_m
                       << " мм за " << feet.plants << " опор, прошёл "
                       << feet.travelled_m << " м (" << feet.travelled_m / (240 * DT)
                       << " м/с); прежний шов worst " << 1000.0f * sim.worst_spread_m
                       << " мм");
        CHECK(feet.plants >= 4);
        CHECK(sim.plants >= 4);
        // ПОКА БЕЗ ЗАМКА: остаток клипа (авторское скольжение опорной точки
        // внутри клипа) обязан быть В РАЗЫ меньше сноса прежнего шва.
        CHECK(feet.worst_spread_m < 0.5f * sim.worst_spread_m);
        CHECK(feet.worst_spread_m < 0.02f);
        // и тело действительно едет: скорость в полосе темпа от заказа
        const float mps = feet.travelled_m / (240 * DT);
        const float band = static_cast<float>(config::LOCOMOTION_TEMPO_BAND);
        CHECK(mps > g.speed * (1.0f - band) * 0.9f);
        CHECK(mps < g.speed * (1.0f + band) * 1.1f);
    }
}


TEST_CASE("diagnostic_speed_by_layer") {
    struct Variant {
        const char* name;
        bool mirror;
        bool stance;
        bool relax;
        bool clearance;
    };
    const Variant variants[] = {{"full", true, true, true, true},
                                {"no mirror", false, true, true, true},
                                {"no stance", true, false, true, true},
                                {"no relax", true, true, false, true},
                                {"no clearance", true, true, true, false},
                                {"bare", false, false, false, false}};
    for (const Variant& v : variants) {
        Model m;
        REQUIRE(load(m, true));
        if (!v.mirror) {
            m.lib.mirror_dose = 0.0f;
        }
        if (!v.stance) {
            m.lib.stance = anim::StanceLayer{};
        }
        if (!v.relax) {
            m.lib.relax = anim::ArmRelax{};
        }
        if (!v.clearance) {
            m.lib.arm_clearance_m = 0.0f;
        }
        const SlideRun walk = run_gear(m, anim::Gait::Walk, static_cast<float>(config::WALK_SPEED), true, 240);
        const SlideRun run = run_gear(m, anim::Gait::Run, static_cast<float>(config::RUN_SPEED), true, 240);
        MESSAGE(v.name << ": walk " << walk.travelled_m / (240 * DT) << " m/s (lib "
                       << m.lib[anim::ClipRole::Walk].natural_mps << "), run "
                       << run.travelled_m / (240 * DT) << " m/s (lib "
                       << m.lib[anim::ClipRole::Sprint].natural_mps << ")");
    }
    CHECK(true);
}


TEST_CASE("diagnostic_pure_clip_speed") {
    Model m;
    REQUIRE(load(m, true));
    const anim::FootIkSetup setup =
        anim::build_foot_ik(m.obj.skeleton, m.binding, m.lib.contacts);
    REQUIRE(setup.valid());
    anim::FootIkProbe flat;
    flat.valid = true;
    std::vector<anim::JointLocal> sample(m.obj.skeleton.size());
    for (const char* name : {"Walk_Loop", "Jog_Fwd_Loop", "Sprint_Loop"}) {
        int32_t idx = -1;
        for (std::size_t i = 0; i < m.obj.clips.size(); ++i) {
            if (m.obj.clips[i].name == name) {
                idx = static_cast<int32_t>(i);
            }
        }
        REQUIRE(idx >= 0);
        const skel::AnimClip& clip = m.obj.clips[static_cast<std::size_t>(idx)];
        const float T = clip.duration_s;
        const uint32_t N = 120;
        glm::vec3 root{0.0f};
        anim::RootMotionState rm;
        anim::ContactState prev;
        float back_max = 0.0f;
        for (uint32_t k = 0; k <= 2 * N; ++k) {
            const float t0 = std::fmod(T * float(k) / float(N), T);
            anim::sample_clip_pose(m.obj.skeleton, clip, t0, sample);
            const anim::FootIkPlan plan = anim::plan_foot_ik(m.obj.skeleton, setup, flat, sample);
            anim::ContactState curr = anim::contact_state(m.obj.skeleton, setup, plan, sample);
            if (k > 0) {
                const glm::vec3 d = anim::root_motion_step(prev, curr, T / float(N), rm);
                if (k > N) {
                    root += d;
                }
                for (std::size_t s = 0; s < 2; ++s) {
                    const float vz = (curr.toe[s].z - prev.toe[s].z) / (T / float(N));
                    back_max = std::max(back_max, vz);
                }
            }
            prev = curr;
        }
        MESSAGE(name << ": T " << T << " s, per-tick root integration over one cycle "
                     << glm::length(root) << " m -> " << glm::length(root) / T
                     << " m/s; fastest backward toe speed " << back_max << " m/s");
    }
    CHECK(true);
}

TEST_CASE("gears_reach_their_ordered_speed") {
    // ДОСТИГНУТАЯ скорость (тем же интегратором, что ведёт корень) против
    // заказа передачи: в полосе темпа с запасом на дискретизацию тика.
    Model m;
    REQUIRE(load(m, true));
    const float band = static_cast<float>(config::LOCOMOTION_TEMPO_BAND);
    struct Row {
        anim::Gait gait;
        anim::ClipRole role;
        float speed;
    };
    const Row rows[] = {{anim::Gait::Walk, anim::ClipRole::Walk, static_cast<float>(config::WALK_SPEED)},
                        {anim::Gait::Jog, anim::ClipRole::Jog, static_cast<float>(config::JOG_SPEED)},
                        {anim::Gait::Run, anim::ClipRole::Sprint, static_cast<float>(config::RUN_SPEED)}};
    for (const Row& r : rows) {
        const anim::ClipEntry& e = m.lib[r.role];
        REQUIRE(e.present());
        CHECK(e.natural_mps > 0.3f);
        const SlideRun feet = run_gear(m, r.gait, r.speed, true, 240);
        const float mps = feet.travelled_m / (240 * DT);
        MESSAGE(anim::role_name(r.role) << ": clip " << e.natural_mps << " m/s, ordered "
                                        << r.speed << ", achieved " << mps);
        CHECK(mps >= r.speed * (1.0f - band) * 0.95f);
        CHECK(mps <= r.speed * (1.0f + band) * 1.05f);
    }
    // контроль: у покоя скорости нет
    CHECK(m.lib[anim::ClipRole::Idle].natural_mps == doctest::Approx(0.0f));
}
