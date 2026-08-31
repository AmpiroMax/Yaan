/*
Module: tests/character
File: tests/character/StanceTests.cpp

Responsibility:
- THE INSTRUMENT THE SKYRIM COMPARISON ASKED FOR: every pose of the stand,
  measured in the same numbers the reference frames were read in, against the
  bands those frames gave. The comparison report was written by eye; this file
  is what makes the same claim checkable, so the next wave argues with a
  number instead of with a screenshot.

Key items:
- stance_protocol: prints the whole table (bare clip vs. the layers), which is
  what the report's before/after columns are cut from.
- the_reference_bands: the acceptance — trunk, gaze, elbow, hand height, stance
  width, knee, on the poses the reference has frames of.
- WITH ITS CONTROL ARM (Rule 30): the same measurement on the clip with no
  layers has to FAIL those bands, or the instrument is measuring nothing.
- a_drawn_weapon_keeps_our_proportions: the weapon branch differs in the upper
  half, is bit-for-bit identical in the lower, and no longer throws the arms
  wide.

Dependencies:
- Uses: engine/anim (ClipPlayer, Stance, PoseLayers), engine/render (.dfo
  reader), assets/objects/characters/HumanBase.dfo (target dfn_characters).
- Used by: ctest (character_stance).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The bands are generated constants, never literals here: the layer aims at
  the same rows this file judges by, which is the whole point of the row.
*/

#include <doctest/doctest.h>

#include "engine/anim/sources/ClipPlayer.h"
#include "engine/anim/sources/PoseLayers.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/anim/sources/Stance.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace dfn;

namespace {

constexpr const char* MODEL = "assets/objects/characters/HumanBase.dfo";
constexpr float DEG = 57.29578f;

/// Sim's step model, restated for the reason ClipPlayerTests restates it:
/// gameplay sits ABOVE anim in the DAG and a character test may not link it.
[[nodiscard]] float step_length(float speed) {
    return static_cast<float>(config::STEP_LENGTH_BASE)
           + static_cast<float>(config::STEP_LENGTH_PER_MPS) * speed;
}

struct Model {
    render::RegistryObject obj;
    anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    anim::SkinnedRigBinding binding;
    anim::ClipLibrary lib;
    /// THE CONTROL ARM: the same library with both layers removed, so "the
    /// clip as bought" and "the clip as drawn" come out of one build, one
    /// asset and one sampling path (Rule 47).
    anim::ClipLibrary bare;
};

[[nodiscard]] bool load(Model& m) {
    if (!std::filesystem::exists(MODEL)) {
        return false;
    }
    auto o = render::read_object(MODEL);
    if (!o.has_value() || o->skeleton.empty() || o->clips.empty()) {
        return false;
    }
    m.obj = std::move(*o);
    m.binding = anim::bind_skinned_rig(m.rig, m.obj.skeleton);
    m.lib = anim::build_clip_library(m.rig, m.obj.skeleton, m.binding, m.obj.clips);
    m.bare = m.lib;
    m.bare.relax = anim::ArmRelax{};
    m.bare.stance = anim::StanceLayer{};
    return true;
}

/// One pose off the real playback path, at a stride phase.
struct Shot {
    anim::Gait gait = anim::Gait::Walk;
    float speed = 0.0f;
    bool drawn = false;
    const char* label = "";
};

void sample_shot(const Model& m, const anim::ClipLibrary& lib, const Shot& s,
                 float phase, std::vector<anim::JointLocal>& out) {
    anim::BodyDrive drive;
    drive.gait = s.gait;
    drive.speed_mps = s.speed;
    drive.step_length_m = step_length(s.speed);
    drive.stride_phase = phase;
    drive.grounded = true;
    drive.weapon_drawn = s.drawn;
    drive.run_weight = anim::gait_run_weight(s.gait) * (s.speed > 0.2f ? 1.0f : 0.0f);
    anim::ClipPlayback play;
    // Long enough for every cross-fade in the file to have finished, so what is
    // measured is a STATE and not a point of a transition.
    for (int i = 0; i < 40; ++i) {
        anim::advance_playback(lib, drive, 1.0f / 30.0f, play);
    }
    out.assign(m.obj.skeleton.size(), anim::JointLocal{});
    REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, lib, play,
                                  1.0f, out));
}

/// The pose's metrics, averaged over the cycle where a cycle exists, plus the
/// swing amplitudes, which only a cycle can answer.
struct Reading {
    anim::StanceMetrics mid{};
    float arm_swing_rad = 0.0f;
    float twist_rad = 0.0f;
    float thigh_swing_rad = 0.0f;
};

[[nodiscard]] Reading read(const Model& m, const anim::ClipLibrary& lib,
                           const Shot& s) {
    Reading r;
    std::vector<anim::JointLocal> pose;
    constexpr int N = 24;
    for (int i = 0; i < N; ++i) {
        const float phase = float(i) / float(N);
        sample_shot(m, lib, s, phase, pose);
        const anim::StanceMetrics k =
            anim::measure_stance(m.obj.skeleton, m.binding, pose);
        r.arm_swing_rad = std::max(r.arm_swing_rad, 0.5f * std::abs(k.arm_split_rad()));
        r.thigh_swing_rad =
            std::max(r.thigh_swing_rad, 0.5f * std::abs(k.thigh_split_rad()));
        r.twist_rad = std::max(r.twist_rad, std::abs(k.shoulder_twist_rad));
    }
    // The one pose the eye reads a stance from is the plant, and sim's plant is
    // FOOTFALL_PHASE_LEFT. Everything scalar is reported there.
    sample_shot(m, lib, s, static_cast<float>(config::FOOTFALL_PHASE_LEFT), pose);
    r.mid = anim::measure_stance(m.obj.skeleton, m.binding, pose);
    return r;
}

void print(const char* arm, const Shot& s, const Reading& r) {
    std::printf("%-5s %-12s trunk %6.1f deg  gaze %6.1f  elbow %5.1f  knee %5.1f  "
                "hand drop %6.3f m  spread %5.3f  stance %5.2f sh  twist %5.1f  "
                "arm swing %5.1f  thigh %5.1f\n",
                arm, s.label,
                static_cast<double>(r.mid.trunk_pitch_rad * DEG),
                static_cast<double>(r.mid.gaze_pitch_rad * DEG),
                static_cast<double>(0.5f * (r.mid.elbow_rad[0] + r.mid.elbow_rad[1]) * DEG),
                static_cast<double>(0.5f * (r.mid.knee_rad[0] + r.mid.knee_rad[1]) * DEG),
                static_cast<double>(0.5f * (r.mid.hand_drop_m[0] + r.mid.hand_drop_m[1])),
                static_cast<double>(0.5f * (r.mid.hand_spread_m[0] + r.mid.hand_spread_m[1])),
                static_cast<double>(r.mid.stance_in_shoulders()),
                static_cast<double>(r.twist_rad * DEG),
                static_cast<double>(r.arm_swing_rad * DEG),
                static_cast<double>(r.thigh_swing_rad * DEG));
}

const Shot SHOTS[] = {
    {anim::Gait::Walk, 0.0f, false, "idle"},
    {anim::Gait::Walk, static_cast<float>(config::WALK_SPEED), false, "walk"},
    {anim::Gait::Run, static_cast<float>(config::RUN_SPEED), false, "run"},
    {anim::Gait::Walk, 0.0f, true, "idle-drawn"},
    {anim::Gait::Walk, static_cast<float>(config::WALK_SPEED), true, "walk-drawn"},
    {anim::Gait::Run, static_cast<float>(config::RUN_SPEED), true, "run-drawn"},
};

} // namespace

TEST_CASE("stance_protocol") {
    Model m;
    REQUIRE(load(m));
    // THE TABLE THE REPORT IS CUT FROM. Both arms out of one build and one
    // asset: "bare" is the clip exactly as bought, "full" is the clip with the
    // arm layer and the stance layer on it.
    std::printf("\n--- stance protocol (HumanBase) ---\n");
    for (const Shot& s : SHOTS) {
        print("bare", s, read(m, m.bare, s));
        print("full", s, read(m, m.lib, s));
    }
    std::printf("arm layer: adduction %.3f rad, lift %.3f rad, elbow offset "
                "%.3f (stand) / %.3f (run) rad; reference hand %.3f m out, "
                "%.3f m below the pelvis, elbow %.1f deg\n",
                static_cast<double>(m.lib.relax.angle_rad),
                static_cast<double>(m.lib.relax.lift_rad),
                static_cast<double>(m.lib.relax.elbow_stand_rad),
                static_cast<double>(m.lib.relax.elbow_run_rad),
                static_cast<double>(m.lib.relax.reference_m),
                static_cast<double>(m.lib.relax.reference_drop_m),
                static_cast<double>(m.lib.relax.reference_elbow_rad * DEG));
    CHECK(m.lib.stance.valid());
}

TEST_CASE("the_reference_bands") {
    Model m;
    REQUIRE(load(m));
    const Reading idle = read(m, m.lib, SHOTS[0]);
    const Reading walk = read(m, m.lib, SHOTS[1]);
    const Reading run = read(m, m.lib, SHOTS[2]);
    const Reading idle_bare = read(m, m.bare, SHOTS[0]);
    const Reading walk_bare = read(m, m.bare, SHOTS[1]);

    const auto trunk_stand = static_cast<float>(config::STANCE_TRUNK_PITCH_STAND);
    const auto trunk_run = static_cast<float>(config::STANCE_TRUNK_PITCH_RUN);

    // 1. THE TRUNK. A standing man does not crouch and a running one does lean.
    CAPTURE(idle.mid.trunk_pitch_rad * DEG);
    CAPTURE(idle_bare.mid.trunk_pitch_rad * DEG);
    CHECK(std::abs(idle.mid.trunk_pitch_rad - trunk_stand) < 0.05f);
    CAPTURE(walk.mid.trunk_pitch_rad * DEG);
    CHECK(std::abs(walk.mid.trunk_pitch_rad - trunk_stand) < 0.09f); // <= 5 deg
    CAPTURE(run.mid.trunk_pitch_rad * DEG);
    CHECK(run.mid.trunk_pitch_rad > trunk_stand + 0.05f);
    CHECK(std::abs(run.mid.trunk_pitch_rad - trunk_run) < 0.09f);
    // ...AND THE CONTROL: the clip as bought is outside the band it is being
    // brought into. Without this line every threshold above could be passing
    // because the asset was already right.
    //
    // ON THE WALK AND NOT ON THE IDLE, and that is a correction to the report
    // this wave was ordered from. Read by eye off a screenshot, our idle was
    // called "15 degrees, crouching"; measured, the bought idle leans 6.6 —
    // 2.6 degrees outside the band, which is real and is not what the frame
    // showed. What the frame showed was the WALK (12.1) and every drawn pose
    // (19 to 31), and those are what a control can be written against. A
    // control aimed at a number that was never true would have gone red on
    // correct code, which is the one thing a control may not do.
    CAPTURE(walk_bare.mid.trunk_pitch_rad * DEG);
    CHECK(std::abs(walk_bare.mid.trunk_pitch_rad - trunk_stand) > 0.12f);

    // 2. THE GAZE. The head keeps HEAD_STABILIZE of the lean off the eyes, so
    //    a standing figure looks at the horizon and not at the floor.
    CAPTURE(idle.mid.gaze_pitch_rad * DEG);
    CAPTURE(idle_bare.mid.gaze_pitch_rad * DEG);
    CHECK(std::abs(idle.mid.gaze_pitch_rad) < 0.09f);
    CHECK(std::abs(idle.mid.gaze_pitch_rad) < std::abs(idle_bare.mid.gaze_pitch_rad));

    // 3. THE ELBOW: the reference's 15-20 deg standing, its 80-90 running.
    const float idle_elbow = 0.5f * (idle.mid.elbow_rad[0] + idle.mid.elbow_rad[1]);
    const float bare_elbow =
        0.5f * (idle_bare.mid.elbow_rad[0] + idle_bare.mid.elbow_rad[1]);
    CAPTURE(idle_elbow * DEG);
    CAPTURE(bare_elbow * DEG);
    CHECK(idle_elbow < 0.40f);
    // The control: the bought idle folds the elbow past twice the reference's.
    // (Also a correction to the report, which read it as 85-95 degrees off a
    // screenshot; measured at the plant it is 37.9.)
    CHECK(bare_elbow > 0.55f);
    const float run_elbow = 0.5f * (run.mid.elbow_rad[0] + run.mid.elbow_rad[1]);
    CAPTURE(run_elbow * DEG);
    CHECK(run_elbow > idle_elbow + 0.4f);

    // 4. THE HANDS. Mid-thigh, not chest — the number the report added.
    const float drop = 0.5f * (idle.mid.hand_drop_m[0] + idle.mid.hand_drop_m[1]);
    const float bare_drop =
        0.5f * (idle_bare.mid.hand_drop_m[0] + idle_bare.mid.hand_drop_m[1]);
    CAPTURE(drop);
    CAPTURE(bare_drop);
    CHECK(drop > 0.03f);
    CHECK(drop < 0.13f);
    // The control: the bought hand hangs 1.6 cm below the pelvis, i.e. level
    // with it — the "hands at the belt" the comparison frames show.
    CHECK(bare_drop < 0.03f);
    const float spread =
        0.5f * (idle.mid.hand_spread_m[0] + idle.mid.hand_spread_m[1]);
    CAPTURE(spread);
    CHECK(spread > 0.18f);
    CHECK(spread < 0.30f);

    // 5. THE STANCE: 0.8-0.9 shoulders, knees nearly straight.
    CAPTURE(idle.mid.stance_in_shoulders());
    CAPTURE(idle_bare.mid.stance_in_shoulders());
    CHECK(idle.mid.stance_in_shoulders() > 0.70f);
    CHECK(idle.mid.stance_in_shoulders() < 1.00f);
    CHECK(idle_bare.mid.stance_in_shoulders() > 1.05f); // the control
    const float knee = 0.5f * (idle.mid.knee_rad[0] + idle.mid.knee_rad[1]);
    const float bare_knee =
        0.5f * (idle_bare.mid.knee_rad[0] + idle_bare.mid.knee_rad[1]);
    CAPTURE(knee * DEG);
    CAPTURE(bare_knee * DEG);
    CHECK(knee < 0.16f); // <= 9 deg
    CHECK(bare_knee > knee + 0.05f);

    // 6. THE SWINGS. The arms have to reach the frame, and the run has to
    //    twist more than the walk.
    CAPTURE(walk.arm_swing_rad * DEG);
    CAPTURE(walk_bare.arm_swing_rad * DEG);
    CHECK(walk.arm_swing_rad > 0.30f); // >= 17 deg, up from under 10
    CHECK(walk.arm_swing_rad > walk_bare.arm_swing_rad + 0.02f);
    CAPTURE(run.twist_rad * DEG);
    CHECK(run.twist_rad > 0.12f);
}

TEST_CASE("the_walk_gear_plays_the_walk_clip") {
    Model m;
    REQUIRE(load(m));
    // WHY THIS IS AN ACCEPTANCE AND NOT AN ASSUMPTION: the comparison read our
    // walk as a jog's stride, and a gear playing the wrong clip is invisible
    // to every other number in this file — a jog bent to a walk's stride still
    // measures a walk's stride.
    anim::BodyDrive drive;
    drive.gait = anim::Gait::Walk;
    drive.speed_mps = static_cast<float>(config::WALK_SPEED);
    drive.step_length_m = step_length(drive.speed_mps);
    drive.grounded = true;
    const anim::ClipRole role = anim::role_for_drive(m.lib, drive);
    CHECK(role == anim::ClipRole::Walk);
    const anim::ClipEntry& e = m.lib[role];
    REQUIRE(e.present());
    const std::string& name = m.obj.clips[static_cast<std::size_t>(e.clip)].name;
    CAPTURE(name);
    CHECK(name == "Walk_Loop");
}

TEST_CASE("a_drawn_weapon_keeps_our_proportions") {
    Model m;
    REQUIRE(load(m));
    const auto walk_pose = [&](bool drawn, std::vector<anim::JointLocal>& out) {
        Shot s{anim::Gait::Walk, static_cast<float>(config::WALK_SPEED), drawn, "w"};
        sample_shot(m, m.lib, s, 0.25f, out);
    };
    std::vector<anim::JointLocal> sheathed;
    std::vector<anim::JointLocal> drawn;
    walk_pose(false, sheathed);
    walk_pose(true, drawn);

    // THE LEGS DO NOT MOVE — the claim the mask exists for.
    float worst_lower = 0.0f;
    float worst_upper = 0.0f;
    for (std::size_t j = 0; j < m.obj.skeleton.size(); ++j) {
        const float d = 1.0f - std::abs(glm::dot(glm::normalize(sheathed[j].rotation),
                                                 glm::normalize(drawn[j].rotation)));
        if (m.lib.mask.at(j) == anim::Branch::Lower) {
            worst_lower = std::max(worst_lower, d);
        } else if (m.lib.mask.at(j) == anim::Branch::Upper) {
            worst_upper = std::max(worst_upper, d);
        }
    }
    CAPTURE(worst_lower);
    CAPTURE(worst_upper);
    CHECK(worst_lower < 1e-5f);
    CHECK(worst_upper > 1e-3f);

    // AND THE HANDS STAY OURS. Before this wave, drawing threw them from
    // 0.256 m out to 0.529 — the arm layer was switched off entirely. A guard
    // IS wider than a hanging arm; it is not twice as wide as the body.
    const anim::StanceMetrics s = anim::measure_stance(m.obj.skeleton, m.binding, sheathed);
    const anim::StanceMetrics d = anim::measure_stance(m.obj.skeleton, m.binding, drawn);
    const float ds = 0.5f * (d.hand_spread_m[0] + d.hand_spread_m[1]);
    const float ss = 0.5f * (s.hand_spread_m[0] + s.hand_spread_m[1]);
    CAPTURE(ss);
    CAPTURE(ds);
    CHECK(ds > ss);          // a guard is wider than a hanging arm...
    // ...but not by the width of a second body. 0.26 m and not something
    // tighter because the remaining width is the GUARD CLIP's own and is
    // named as a tail: this asset splays BOTH arms, where the reference
    // carries the off hand in front of the chest. Half the arm layer takes
    // 0.06 m off it; the rest needs a guard clip that holds a sword.
    CHECK(ds < ss + 0.26f);
}
