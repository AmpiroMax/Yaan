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
- the_blade_is_in_the_hand: the sword exists, hangs off ONE joint, and its
  point lands where a sword's point lands — the claim no frame could make
  while the hand was empty.

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
#include "engine/anim/sources/HeldBlade.h"
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
#include <glm/gtc/matrix_transform.hpp>

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
    // THE FIST, in the unit the previous wave reported it in: wrist to
    // fingertip. Open measures further than closed, so the number says how far
    // the hand came back from the clip's sculpted fist — and the weapon hand
    // has to stay in it.
    for (const Shot& sh : {SHOTS[0], SHOTS[3]}) {
        std::vector<anim::JointLocal> pose;
        sample_shot(m, m.bare, sh, 0.25f, pose);
        const float bare_open =
            anim::measure_hand_openness(m.obj.skeleton, m.lib.relax, pose);
        sample_shot(m, m.lib, sh, 0.25f, pose);
        const float full_open =
            anim::measure_hand_openness(m.obj.skeleton, m.lib.relax, pose);
        std::printf("fist   %-12s wrist to fingertip %.3f -> %.3f m\n", sh.label,
                    static_cast<double>(bare_open), static_cast<double>(full_open));
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

    // WHERE THE BLADE POINTS, in the drawn poses, as the angle off the ground
    // plane — the number the reference states ("the blade at 30-40 degrees to
    // the ground, point back and down") and the one a frame of an empty hand
    // could not be asked for at all.
    std::vector<anim::JointLocal> guard(m.obj.skeleton.size());
    anim::sample_clip_pose(
        m.obj.skeleton,
        m.obj.clips[static_cast<std::size_t>(m.lib[anim::ClipRole::WeaponIdle].clip)],
        0.0f, guard);
    const anim::HeldBlade blade =
        anim::build_held_blade(m.obj.skeleton, m.binding, guard);
    if (blade.valid()) {
        for (const Shot& s : SHOTS) {
            if (!s.drawn) {
                continue;
            }
            std::vector<anim::JointLocal> pose;
            sample_shot(m, m.lib, s, 0.25f, pose);
            std::vector<glm::mat4> palette(m.obj.skeleton.size());
            anim::sample_palette(m.obj.skeleton, pose, palette);
            const glm::mat4& p = palette[static_cast<std::size_t>(blade.joint)];
            // The point is the vertex furthest from the pommel end, and the
            // pommel is the one furthest from IT: two passes, no name table.
            glm::vec3 a{0.0f};
            glm::vec3 b{0.0f};
            float best = -1.0f;
            std::vector<glm::vec3> world;
            world.reserve(blade.vertices.size());
            for (const platform::SkinnedVertex& v : blade.vertices) {
                world.push_back(glm::vec3{p * glm::vec4{v.position, 1.0f}});
            }
            for (const glm::vec3& w : world) {
                const float d = glm::length(w - world.front());
                if (d > best) { best = d; a = w; }
            }
            best = -1.0f;
            for (const glm::vec3& w : world) {
                const float d = glm::length(w - a);
                if (d > best) { best = d; b = w; }
            }
            const glm::vec3 axis = glm::normalize(b - a);
            const float tilt = std::asin(std::clamp(std::abs(axis.y), 0.0f, 1.0f));
            std::printf("blade  %-12s tilt from the ground %5.1f deg, "
                        "point %.2f m from the hilt\n",
                        s.label, static_cast<double>(tilt * DEG),
                        static_cast<double>(glm::length(b - a)));
        }
    }
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

TEST_CASE("the_blade_is_in_the_hand") {
    Model m;
    REQUIRE(load(m));
    std::vector<anim::JointLocal> guard(m.obj.skeleton.size());
    anim::sample_clip_pose(
        m.obj.skeleton,
        m.obj.clips[static_cast<std::size_t>(m.lib[anim::ClipRole::WeaponIdle].clip)],
        0.0f, guard);
    const anim::HeldBlade blade =
        anim::build_held_blade(m.obj.skeleton, m.binding, guard);
    REQUIRE(blade.valid());
    const int32_t hand = m.binding.names.joint[anim::bone_index(anim::Bone::HandR)];
    CHECK(blade.joint == hand);
    CAPTURE(blade.length_m);
    CHECK(blade.length_m > 0.80f);
    CHECK(blade.length_m < 1.05f); // an arming sword, not a claymore
    CHECK(blade.indices.size() % 3 == 0);

    // ONE JOINT, WEIGHT ONE, and it is the whole placement mechanism: a vertex
    // that leaked a second influence would make the sword bend when the wrist
    // did.
    for (const platform::SkinnedVertex& v : blade.vertices) {
        REQUIRE(v.joints[0] == static_cast<uint8_t>(hand));
        REQUIRE(v.weights[0] == doctest::Approx(1.0f));
        REQUIRE(v.weights[1] == doctest::Approx(0.0f));
    }

    // AND WHERE IT ENDS UP. The vertices are authored in BIND-model space, so
    // the palette must land them in the POSED hand's frame: the point has to
    // sit a blade's length from the hand joint in every pose, and the hilt
    // has to sit inside the fist. Measured on the drawn walk, which is the
    // pose the reference frame shows.
    std::vector<anim::JointLocal> pose;
    Shot s{anim::Gait::Walk, static_cast<float>(config::WALK_SPEED), true, "w"};
    sample_shot(m, m.lib, s, 0.25f, pose);
    std::vector<glm::mat4> palette(m.obj.skeleton.size());
    anim::sample_palette(m.obj.skeleton, pose, palette);
    const glm::mat4& p = palette[static_cast<std::size_t>(hand)];
    float near_m = 1.0e9f;
    float far_m = 0.0f;
    std::vector<glm::mat4> local(m.obj.skeleton.size());
    std::vector<glm::mat4> model(m.obj.skeleton.size());
    for (std::size_t j = 0; j < m.obj.skeleton.size(); ++j) {
        local[j] = glm::translate(glm::mat4{1.0f}, pose[j].translation)
                   * glm::mat4_cast(glm::normalize(pose[j].rotation))
                   * glm::scale(glm::mat4{1.0f}, pose[j].scale);
    }
    skel::skeleton_model_matrices(m.obj.skeleton, local, model);
    const glm::vec3 hand_pos{model[static_cast<std::size_t>(hand)][3]};
    for (const platform::SkinnedVertex& v : blade.vertices) {
        const glm::vec3 w{p * glm::vec4{v.position, 1.0f}};
        const float d = glm::length(w - hand_pos);
        near_m = std::min(near_m, d);
        far_m = std::max(far_m, d);
    }
    CAPTURE(near_m);
    CAPTURE(far_m);
    // 0.08 m and not something tighter because what is measured is a CORNER:
    // the grip box straddles the wrist, so its nearest VERTEX is at the end of
    // the grip and half its thickness off the axis — 0.054 m — while the
    // surface passes 0.015 m from the joint. A tighter band would be a claim
    // about where a box's corners are.
    CHECK(near_m < 0.08f);              // the grip is IN the fist
    // AND IT IS CARRIED, NOT BRANDISHED. The reference holds a drawn blade at
    // 30-40 degrees to the ground with the point down; the grip's cant is
    // solved for exactly that (STANCE_BLADE_TILT), so this is the check that
    // the solve reached its target through the retarget and the palette.
    {
        std::vector<glm::vec3> w;
        w.reserve(blade.vertices.size());
        for (const platform::SkinnedVertex& v : blade.vertices) {
            w.push_back(glm::vec3{p * glm::vec4{v.position, 1.0f}});
        }
        glm::vec3 a = w.front();
        glm::vec3 b = w.front();
        float best = -1.0f;
        for (const glm::vec3& q : w) {
            const float d = glm::length(q - w.front());
            if (d > best) { best = d; a = q; }
        }
        best = -1.0f;
        for (const glm::vec3& q : w) {
            const float d = glm::length(q - a);
            if (d > best) { best = d; b = q; }
        }
        const glm::vec3 axis = glm::normalize(b - a);
        const float tilt = std::asin(std::clamp(std::abs(axis.y), 0.0f, 1.0f));
        CAPTURE(tilt * DEG);
        CHECK(tilt > 0.44f); // 25 deg: not held out level like a lance...
        CHECK(tilt < 0.79f); // 45 deg: ...and not dragged in the grass
    }
    CHECK(far_m > 0.70f);               // the point is a blade away
    CHECK(far_m < blade.length_m + 0.1f); // and not further than the sword is long
}
