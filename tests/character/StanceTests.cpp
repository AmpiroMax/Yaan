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
#include "engine/anim/sources/Hitbox.h"
#include "engine/anim/sources/PoseLayers.h"
#include "engine/anim/sources/RestFit.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/anim/sources/Stance.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <span>
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
    // ОДНА РЕСТ-ПОЗА НА ТЕЛО (RestFit.h): та же, которой тело рисуют экран
    // создания и мир — иначе стенд судил бы позу, которой никто не видит.
    m.rig = anim::rest_rig_for(m.obj.skeleton, m.obj.skin.vertices);
    m.binding = anim::bind_skinned_rig(m.rig, m.obj.skeleton);
    m.lib = anim::build_clip_library(m.rig, m.obj.skeleton, m.binding, m.obj.clips,
                                     m.obj.skin.vertices);
    m.bare = m.lib;
    m.bare.relax = anim::ArmRelax{};
    m.bare.stance = anim::StanceLayer{};
    return true;
}

/// THE MODEL'S OWN THIGH, metres: pelvis joint down to knee joint in the rest
/// pose. The unit the hand's hang is written in, and it is the BODY'S and not
/// the canon's on purpose — see the band that uses it.
[[nodiscard]] float thigh_length(const Model& m) {
    std::vector<glm::mat4> model(m.obj.skeleton.size());
    anim::rest_model_matrices(m.rig, m.obj.skeleton, m.binding, anim::LocalPose{},
                              model);
    const auto y = [&](anim::Bone b) {
        const int32_t j = m.binding.names.joint[anim::bone_index(b)];
        return j >= 0 ? model[static_cast<std::size_t>(j)][3][1] : 0.0f;
    };
    return y(anim::Bone::Pelvis) - y(anim::Bone::ShinL);
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
    //
    // WRITTEN IN THE BODY'S OWN THIGH, NOT IN METRES (01.09). The band used to
    // be 0.03-0.13 m and it was measured on a body the importer had fitted to
    // the canon, whose upper arm is 19 % longer than the one we now ship: the
    // owner kept the RAW asset, its shorter arm puts the same anatomically
    // right wrist 1.2 cm below the hip instead of 4, and a floor in metres
    // went red on a pose nobody changed. What survives a change of body is the
    // pair of statements the metres were standing in for — the wrist hangs
    // BELOW the hip line (it is not held at the belt) and stays ABOVE
    // mid-thigh (it is a hanging arm, not a reach) — plus how far the layer
    // had to move it to get there.
    const float thigh = thigh_length(m);
    REQUIRE(thigh > 0.1f);
    const float drop = 0.5f * (idle.mid.hand_drop_m[0] + idle.mid.hand_drop_m[1]);
    const float bare_drop =
        0.5f * (idle_bare.mid.hand_drop_m[0] + idle_bare.mid.hand_drop_m[1]);
    CAPTURE(thigh);
    CAPTURE(drop);
    CAPTURE(drop / thigh);
    CAPTURE(bare_drop);
    CHECK(drop > 0.0f);
    CHECK(drop < 0.35f * thigh);
    // The control, and it is the same statement with its sign flipped: the
    // bought hand hangs ABOVE the pelvis — the "hands at the belt" the
    // comparison frames show — so the layer had to carry the wrist across the
    // hip line, not merely lower it a little.
    CHECK(bare_drop < 0.0f);
    CHECK(drop > bare_drop + 0.03f);
    const float spread =
        0.5f * (idle.mid.hand_spread_m[0] + idle.mid.hand_spread_m[1]);
    CAPTURE(spread);
    CHECK(spread > 0.18f);
    CHECK(spread < 0.30f);

    // 5. THE STANCE: THE REST POSE'S, knees nearly straight.
    //
    // THE NEUTRAL IS THE REST (owner's order 02.09): the idle stands as wide
    // as the rig's rest pose stands — legs vertical under this body's own
    // hip joints, 0.170 m on HumanBase — and not at a share of the shoulder
    // width read off a Skyrim frame (0.85 shoulders was 0.31 m here, the
    // "wide, comic" stance the owner saw twice). The character screen shows
    // the rest; the world shows the idle; this is the line that says they
    // are one man. The Skyrim band survives as a sanity check in shoulders:
    // a rest narrower than half a shoulder would be a soldier at attention,
    // wider than one would be the raskoryaka.
    const float rest_width = m.lib.stance.rest_stance_width_m;
    CAPTURE(rest_width);
    CAPTURE(idle.mid.stance_width_m);
    CAPTURE(idle.mid.stance_in_shoulders());
    CAPTURE(idle_bare.mid.stance_in_shoulders());
    REQUIRE(rest_width > 0.05f);
    CHECK(std::abs(idle.mid.stance_width_m - rest_width) < 0.01f);
    CHECK(idle.mid.stance_in_shoulders() > 0.40f);
    CHECK(idle.mid.stance_in_shoulders() < 1.00f);
    // THE CONTROL: the bought idle — unarmed and drawn — stands wider than the
    // rest by more than noise, and the layer is what closes it. The drawn one
    // at 1.13 shoulders is the wide stance the owner complained about.
    const Reading idle_drawn_bare = read(m, m.bare, SHOTS[3]);
    CAPTURE(idle_drawn_bare.mid.stance_in_shoulders());
    CHECK(idle_drawn_bare.mid.stance_in_shoulders() > 1.05f); // the control
    CHECK(idle_bare.mid.stance_width_m > rest_width + 0.05f);
    CHECK(idle_bare.mid.stance_in_shoulders() > idle.mid.stance_in_shoulders()
                                                   + 0.10f);
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

TEST_CASE("the_hilt_lies_in_the_hand") {
    // ЗАКАЗ ВЛАДЕЛЬЦА 31.08, ПУНКТ 5: «меч торчит из кисти, не лежит в руке».
    // Три утверждения, и все три — про ГЕОМЕТРИЮ БИНДА, а не про кадр: меч
    // жёстко привязан к кости кисти, поэтому «лежит ли рукоять в ладони» не
    // зависит ни от позы, ни от ракурса и обязано проверяться там, где решено.
    //   1. ОСЬ РУКОЯТИ ПРОХОДИТ ЧЕРЕЗ КУЛАК — расстояние от центра кулака до
    //      оси меньше радиуса кулака;
    //   2. КУЛАК ДЕРЖИТСЯ ЗА РУКОЯТЬ — проекция центра кулака на ось лежит
    //      МЕЖДУ навершием и гардой, а не за ними;
    //   3. ГАРДА У УКАЗАТЕЛЬНОГО — она стоит на дальнем краю ладони, а не в
    //      запястье и не в воздухе перед кистью.
    Model m;
    REQUIRE(load(m));
    std::vector<anim::JointLocal> guard_pose(m.obj.skeleton.size());
    anim::sample_clip_pose(
        m.obj.skeleton,
        m.obj.clips[static_cast<std::size_t>(m.lib[anim::ClipRole::WeaponIdle].clip)],
        0.0f, guard_pose);
    const anim::HeldBlade blade =
        anim::build_held_blade(m.obj.skeleton, m.binding, guard_pose);
    REQUIRE(blade.valid());

    const int32_t hand = m.binding.names.joint[anim::bone_index(anim::Bone::HandR)];
    REQUIRE(hand >= 0);
    const auto bind_of = [&](int32_t j) {
        return glm::vec3{glm::inverse(
            m.obj.skeleton.joints[static_cast<std::size_t>(j)].inverse_bind)[3]};
    };
    const glm::vec3 wrist = bind_of(hand);
    // ОСНОВАНИЯ ПАЛЬЦЕВ — дети кисти; большой находится тем же способом, что и
    // в HeldBlade (самый далёкий от их среднего), чтобы прибор и предмет
    // говорили об одной и той же руке.
    std::vector<glm::vec3> base;
    for (std::size_t j = 0; j < m.obj.skeleton.size(); ++j) {
        if (m.obj.skeleton.joints[j].parent == hand) {
            base.push_back(bind_of(static_cast<int32_t>(j)));
        }
    }
    REQUIRE(base.size() >= 4);
    glm::vec3 mean{0.0f};
    for (const glm::vec3& p : base) {
        mean += p;
    }
    mean /= float(base.size());
    std::size_t thumb = 0;
    float worst = -1.0f;
    for (std::size_t i = 0; i < base.size(); ++i) {
        const float d = glm::length(base[i] - mean);
        if (d > worst) { worst = d; thumb = i; }
    }
    glm::vec3 knuckle_mean{0.0f};
    uint32_t n = 0;
    float knuckle_spread = 0.0f;
    for (std::size_t i = 0; i < base.size(); ++i) {
        if (i == thumb) { continue; }
        knuckle_mean += base[i];
        ++n;
    }
    knuckle_mean /= float(n);
    for (std::size_t i = 0; i < base.size(); ++i) {
        for (std::size_t j = i + 1; j < base.size(); ++j) {
            if (i == thumb || j == thumb) { continue; }
            knuckle_spread = std::max(knuckle_spread, glm::length(base[j] - base[i]));
        }
    }
    // ЦЕНТР КУЛАКА — середина ладони: между запястьем и линией костяшек.
    const glm::vec3 fist = 0.5f * (wrist + knuckle_mean);
    const float palm = glm::length(knuckle_mean - wrist);
    // РАДИУС КУЛАКА — половина размаха костяшек: ширина сжатой ладони и есть
    // расстояние между крайними костяшками, и делить его надо пополам.
    const float fist_radius = 0.5f * knuckle_spread;

    // ОСЬ РУКОЯТИ И ЕЁ КОНЦЫ — из САМИХ ВЕРШИН, а не из констант HeldBlade.cpp:
    // прибор, читающий числа предмета, меряет согласие предмета с собой.
    glm::vec3 a{0.0f};
    glm::vec3 b{0.0f};
    float best = -1.0f;
    for (const platform::SkinnedVertex& v : blade.vertices) {
        const float d = glm::length(v.position - blade.vertices.front().position);
        if (d > best) { best = d; a = v.position; }
    }
    best = -1.0f;
    for (const platform::SkinnedVertex& v : blade.vertices) {
        const float d = glm::length(v.position - a);
        if (d > best) { best = d; b = v.position; }
    }
    // Навершие — тот из двух концов, что ближе к запястью; остриё — второй.
    const bool a_is_pommel = glm::length(a - wrist) < glm::length(b - wrist);
    glm::vec3 pommel = a_is_pommel ? a : b;
    glm::vec3 point = a_is_pommel ? b : a;
    glm::vec3 axis = glm::normalize(point - pommel);
    // И ОСЬ УТОЧНЯЕТСЯ ПО СЕРЕДИНАМ ТОРЦОВ, а не остаётся линией «угол —
    // угол». Две самые далёкие вершины меча — это УГОЛ навершия и УГОЛ
    // острия, оба в паре сантиметров от настоящей оси, и линия между ними
    // косит на столько же. Прибор, читающий по ней «рукоять мимо кулака»,
    // мерил бы собственную косину: замерено, поправка снимает 2.6 см из
    // ответа и оставляет 0.
    for (int pass = 0; pass < 2; ++pass) {
        float lo = 1.0e9f;
        float hi = -1.0e9f;
        for (const platform::SkinnedVertex& v : blade.vertices) {
            const float t = glm::dot(v.position - pommel, axis);
            lo = std::min(lo, t);
            hi = std::max(hi, t);
        }
        glm::vec3 sum_lo{0.0f};
        glm::vec3 sum_hi{0.0f};
        uint32_t n_lo = 0;
        uint32_t n_hi = 0;
        for (const platform::SkinnedVertex& v : blade.vertices) {
            const float t = glm::dot(v.position - pommel, axis);
            if (t <= lo + 0.02f) { sum_lo += v.position; ++n_lo; }
            if (t >= hi - 0.02f) { sum_hi += v.position; ++n_hi; }
        }
        REQUIRE(n_lo > 0);
        REQUIRE(n_hi > 0);
        const glm::vec3 p0 = sum_lo / float(n_lo);
        const glm::vec3 p1 = sum_hi / float(n_hi);
        pommel = p0;
        point = p1;
        axis = glm::normalize(p1 - p0);
    }

    const float along_fist = glm::dot(fist - pommel, axis);
    const float off_axis = glm::length((fist - pommel) - axis * along_fist);
    // ГАРДА — самая широкая поперёк оси точка клинка: перекрестье шире и
    // клинка, и рукояти по построению, и находится это, опять же, по вершинам.
    float guard_at = 0.0f;
    float guard_span = 0.0f;
    {
        // Поперечная ось — та, вдоль которой у гарды размах: берём худшую
        // поперечную координату среди всех вершин и её положение вдоль оси.
        for (const platform::SkinnedVertex& v : blade.vertices) {
            const float t = glm::dot(v.position - pommel, axis);
            const float r = glm::length((v.position - pommel) - axis * t);
            if (r > guard_span) { guard_span = r; guard_at = t; }
        }
    }
    const float knuckle_at = glm::dot(knuckle_mean - pommel, axis);
    const float wrist_at = glm::dot(wrist - pommel, axis);

    MESSAGE("--- хват (бинд, правая кисть) ---");
    MESSAGE("ладонь " << 100.0f * palm << " см, размах костяшек "
                      << 100.0f * knuckle_spread << " см");
    MESSAGE("центр кулака: вдоль оси " << 100.0f * along_fist << " см от навершия, "
                                       << "поперёк оси " << 100.0f * off_axis
                                       << " см (радиус кулака "
                                       << 100.0f * fist_radius << ")");
    MESSAGE("гарда на " << 100.0f * guard_at << " см от навершия; костяшки на "
                        << 100.0f * knuckle_at << ", запястье на "
                        << 100.0f * wrist_at);
    MESSAGE("длина рукояти (навершие->гарда) " << 100.0f * guard_at << " см");

    // 1. ОСЬ РУКОЯТИ ПРОХОДИТ ЧЕРЕЗ КУЛАК.
    CHECK(off_axis < fist_radius);
    // КОНТРОЛЬНАЯ РУКА — ПРЕЖНЕЕ ПРАВИЛО, посчитанное здесь же и из той же
    // геометрии, чтобы две руки отличались ровно правилом размещения, а не
    // сборкой (правило 47). Прежнее правило ставило гарду на ЗАДАННЫЕ 0.055 м
    // от сустава запястья вдоль оси; проверяется то же самое утверждение —
    // «гарда у указательного пальца», — и оно обязано провалиться.
    constexpr float OLD_WRIST_TO_FIST = 0.055f;
    const float control_guard_at =
        glm::dot(wrist - pommel, axis) + OLD_WRIST_TO_FIST;
    const float control_miss = std::abs(control_guard_at - knuckle_at);
    CAPTURE(control_miss);
    MESSAGE("контроль (гарда на заданных 5.5 см от запястья): промах мимо "
            << "костяшек " << 100.0f * control_miss << " см");
    CHECK(control_miss > 0.03f);
    // 2. КУЛАК ДЕРЖИТСЯ ЗА РУКОЯТЬ, а не за навершие и не за клинок.
    CHECK(along_fist > 0.0f);
    CHECK(along_fist < guard_at);
    // 3. ГАРДА У УКАЗАТЕЛЬНОГО ПАЛЬЦА: не дальше сантиметра от линии
    // костяшек. Сантиметр — это половина толщины гарды с запасом, то есть
    // «касается», а не «рядом».
    CHECK(std::abs(guard_at - knuckle_at) < 0.01f);
    // И НАВЕРШИЕ ЗА ПЯТКОЙ ЛАДОНИ, иначе рука соскальзывает с рукояти назад.
    CHECK(wrist_at > 0.0f);
    CAPTURE(guard_span);
    CHECK(guard_span > 0.05f); // перекрестье найдено, а не спутано с клинком
}

TEST_CASE("no_part_of_the_body_passes_through_another") {
    Model m;
    REQUIRE(load(m));
    // ЗАКАЗ ВЛАДЕЛЬЦА 01.09, ТРЕТЬИМ ПУНКТОМ И САМЫМ ОБЩИМ: «у всех частей
    // тела хитбоксы, и стойка решается С ЗАПРЕТОМ ВЗАИМОПРОНИКНОВЕНИЯ». Первые
    // два пункта — «ноги стоят криво и слишком близко» и «руки не вдоль
    // боков» — этим прибором И БЫЛИ НАЙДЕНЫ, а не подтверждены: до него зона
    // умела спросить «далеко ли ТОЧКА сустава от коробки», и на этот вопрос
    // рука, вошедшая в бедро ладонью, отвечала «3.5 см, всё хорошо».
    //
    // ДВА ПРИБОРА, И ВТОРОЙ НЕ ДУБЛИРУЕТ ПЕРВЫЙ.
    //   КОРОБКИ (hitbox_pair_distance) — то, чем ПОЛЬЗУЕТСЯ игра: слой обхода
    //   целится в них, луч попадания спрашивает их. Коробка выпуклая и
    //   прямоугольная, поэтому её угол «касается» там, где мясо от мяса ещё
    //   далеко.
    //   МЕШ (минимум по парам вершин) — то, что ВИДИТ владелец. Он медленнее и
    //   он здесь именно поэтому: заявление «меши бёдер пересекаются» проверяется
    //   мешами, а не их приближением.
    // Расхождение между ними — не шум, а свойство коробки, и оно печатается.
    anim::HitboxSet boxes = anim::build_hitboxes(m.rig.proportions);
    anim::fit_hitboxes_to_skin(boxes, m.rig, m.obj.skeleton, m.binding,
                               m.obj.skin.vertices);

    // Вершины по частям тела: ближайший СВЯЗАННЫЙ предок самой тяжёлой кости —
    // то же правило, что у судьи пропорций и у подгонки коробок.
    const std::size_t n = m.obj.skeleton.size();
    std::vector<int> bone_of(n, -1);
    for (uint32_t b = 0; b < anim::BONE_COUNT; ++b) {
        const int32_t j = m.binding.names.joint[b];
        if (j >= 0) {
            bone_of[static_cast<std::size_t>(j)] = static_cast<int>(b);
        }
    }
    for (std::size_t j = 0; j < n; ++j) {
        if (bone_of[j] < 0) {
            const int32_t par = m.obj.skeleton.joints[j].parent;
            if (par >= 0) {
                bone_of[j] = bone_of[static_cast<std::size_t>(par)];
            }
        }
    }
    std::vector<int> vbone(m.obj.skin.vertices.size(), -1);
    for (std::size_t i = 0; i < vbone.size(); ++i) {
        const platform::SkinnedVertex& v = m.obj.skin.vertices[i];
        int best = -1;
        float bw = -1.0f;
        for (int k = 0; k < 4; ++k) {
            if (v.weights[k] > bw) {
                bw = v.weights[k];
                best = static_cast<int>(v.joints[k]);
            }
        }
        vbone[i] = best >= 0 && static_cast<std::size_t>(best) < n
                       ? bone_of[static_cast<std::size_t>(best)]
                       : -1;
    }
    const auto group = [&](std::initializer_list<anim::Bone> bs) {
        std::vector<std::size_t> out;
        for (std::size_t i = 0; i < vbone.size(); ++i) {
            for (const anim::Bone b : bs) {
                if (vbone[i] == static_cast<int>(anim::bone_index(b))) {
                    out.push_back(i);
                    break;
                }
            }
        }
        return out;
    };
    const std::vector<std::size_t> leg_l =
        group({anim::Bone::ThighL, anim::Bone::ShinL});
    const std::vector<std::size_t> leg_r =
        group({anim::Bone::ThighR, anim::Bone::ShinR});
    const std::vector<std::size_t> hand_l = group({anim::Bone::HandL});
    const std::vector<std::size_t> hand_r = group({anim::Bone::HandR});
    const std::vector<std::size_t> fore_l = group({anim::Bone::ForearmL});
    const std::vector<std::size_t> trunk =
        group({anim::Bone::Pelvis, anim::Bone::Torso});
    const std::vector<std::size_t> thigh_l = group({anim::Bone::ThighL});
    const std::vector<std::size_t> thigh_r = group({anim::Bone::ThighR});
    REQUIRE(leg_l.size() > 50);
    REQUIRE(hand_l.size() > 50);
    REQUIRE(trunk.size() > 50);

    std::vector<glm::vec3> skinned(m.obj.skin.vertices.size());
    const auto mesh_gap = [&](const std::vector<std::size_t>& a,
                              const std::vector<std::size_t>& b) {
        float best = std::numeric_limits<float>::max();
        for (const std::size_t i : a) {
            const glm::vec3 p = skinned[i];
            for (const std::size_t j : b) {
                best = std::min(best, glm::dot(skinned[j] - p, skinned[j] - p));
            }
        }
        return std::sqrt(best);
    };

    struct Row {
        float legs_box = 1e9f;
        float hand_thigh_box = 1e9f;
        float hand_hips_box = 1e9f;
        float fore_abdomen_box = 1e9f;
        float legs_mesh = 1e9f;
        float hand_thigh_mesh = 1e9f;
        float fore_trunk_mesh = 1e9f;
    };
    const auto sweep = [&](const anim::ClipLibrary& lib, const Shot& s) {
        Row r;
        std::vector<anim::JointLocal> pose;
        // ДВЕНАДЦАТЬ ФАЗ, А НЕ ОДНА: пересечение — событие ЦИКЛА. Поза в
        // середине шага ничего не говорит о том, задевает ли маховая нога
        // опорную в момент проноса.
        constexpr int N = 12;
        for (int i = 0; i < N; ++i) {
            sample_shot(m, lib, s, float(i) / float(N), pose);
            const anim::HitboxPose hp =
                anim::hitbox_pose(boxes, m.obj.skeleton, m.binding, pose);
            using P = anim::BodyPart;
            const auto d = [&](P a, P b) {
                return anim::hitbox_pair_distance(boxes, hp, a, b);
            };
            r.legs_box = std::min({r.legs_box, d(P::ThighL, P::ThighR),
                                   d(P::ShinL, P::ShinR)});
            r.hand_thigh_box = std::min({r.hand_thigh_box, d(P::HandL, P::ThighL),
                                         d(P::HandR, P::ThighR)});
            r.hand_hips_box = std::min({r.hand_hips_box, d(P::HandL, P::Hips),
                                        d(P::HandR, P::Hips)});
            r.fore_abdomen_box =
                std::min({r.fore_abdomen_box, d(P::ForearmL, P::Abdomen),
                          d(P::ForearmR, P::Abdomen)});
            std::vector<glm::mat4> palette(n);
            anim::sample_palette(m.obj.skeleton, pose, palette);
            for (std::size_t v = 0; v < skinned.size(); ++v) {
                skinned[v] =
                    anim::cpu_skin_position(m.obj.skin.vertices[v], palette);
            }
            r.legs_mesh = std::min(r.legs_mesh, mesh_gap(leg_l, leg_r));
            r.hand_thigh_mesh = std::min({r.hand_thigh_mesh,
                                          mesh_gap(hand_l, thigh_l),
                                          mesh_gap(hand_r, thigh_r)});
            r.fore_trunk_mesh = std::min(r.fore_trunk_mesh, mesh_gap(fore_l, trunk));
        }
        return r;
    };

    const Shot gaits[] = {SHOTS[0], SHOTS[1], SHOTS[2]};
    for (const Shot& g : gaits) {
        const Row full = sweep(m.lib, g);
        CAPTURE(std::string(g.label));
        MESSAGE("клиренсы «" << std::string(g.label) << "», см — КОРОБКИ: нога-нога "
                             << 100.0f * full.legs_box << ", кисть-бедро "
                             << 100.0f * full.hand_thigh_box << ", кисть-таз "
                             << 100.0f * full.hand_hips_box
                             << ", предплечье-живот "
                             << 100.0f * full.fore_abdomen_box
                             << " | МЕШ: нога-нога " << 100.0f * full.legs_mesh
                             << ", кисть-бедро " << 100.0f * full.hand_thigh_mesh
                             << ", предплечье-туловище "
                             << 100.0f * full.fore_trunk_mesh);
        // 1. НИЧТО НЕ ПРОХОДИТ СКВОЗЬ НИЧЕГО — по мешу, потому что заявление
        //    владельца («меши бёдер и голеней пересекаются») про меш.
        CHECK(full.legs_mesh > 0.0f);
        CHECK(full.hand_thigh_mesh > 0.0f);
        CHECK(full.fore_trunk_mesh > 0.0f);
        // 2. И НЕ ПРОСТО НЕ ПЕРЕСЕКАЕТСЯ, А С ЗАЗОРОМ. Сантиметр — нижний край
        //    заказанной вилки «1-2 см»; замерено 1.66-3.23 (ноги) и 1.66-6.41
        //    (кисть о бедро) на трёх походках.
        CHECK(full.legs_mesh > 0.01f);
        CHECK(full.hand_thigh_mesh > 0.01f);
        // 3. КОРОБКИ — ТО, ЧЕМ ПОЛЬЗУЕТСЯ ИГРА, и у них своя, более грубая
        //    полоса: угол прямоугольной коробки вылезает за скруглённое мясо,
        //    поэтому ноль по коробкам ещё не пересечение по мешу. Ноги и кисть
        //    судятся строго; предплечье о живот только ПЕЧАТАЕТСЯ — на ходьбе
        //    его коробки пересекаются при 5.4 см между мясом, и это свойство
        //    формы, а не позы (см. body_gap в PoseLayers.cpp).
        CHECK(full.legs_box > 0.0f);
        CHECK(full.hand_thigh_box > 0.01f);
        CHECK(full.hand_hips_box > 0.01f);
    }

    // КОНТРОЛЬНАЯ РУКА (правило 30): та же мера на позе БЕЗ наших слоёв. Она
    // обязана быть хуже — иначе прибор меряет тело, а не работу слоёв.
    // НА ПОКОЕ, потому что именно покой владелец и смотрел: купленный idle
    // ставит ноги в 3.66 см асимметрии по лодыжкам и разворачивает таз на
    // 13.3 градуса, и обе цифры видны в кадре.
    {
        std::vector<anim::JointLocal> bare_pose;
        sample_shot(m, m.bare, SHOTS[0], 0.0f, bare_pose);
        std::vector<anim::JointLocal> full_pose;
        sample_shot(m, m.lib, SHOTS[0], 0.0f, full_pose);
        const auto hips_and_ankles = [&](std::span<const anim::JointLocal> pose,
                                         float& yaw_deg, float& asym_mm) {
            std::vector<glm::mat4> mats(n);
            for (std::size_t j = 0; j < n; ++j) {
                mats[j] = glm::translate(glm::mat4{1.0f}, pose[j].translation)
                          * glm::mat4_cast(glm::normalize(pose[j].rotation))
                          * glm::scale(glm::mat4{1.0f}, pose[j].scale);
            }
            skel::skeleton_model_matrices(m.obj.skeleton, mats, mats);
            const auto at = [&](anim::Bone b) {
                const int32_t j = m.binding.names.joint[anim::bone_index(b)];
                return j >= 0 ? glm::vec3{mats[static_cast<std::size_t>(j)][3]}
                              : glm::vec3{0.0f};
            };
            const glm::vec3 hips = at(anim::Bone::ThighR) - at(anim::Bone::ThighL);
            yaw_deg = DEG * std::atan2(hips.z, std::abs(hips.x));
            const float mid = 0.5f * (at(anim::Bone::ThighL).x + at(anim::Bone::ThighR).x);
            asym_mm = 1000.0f
                      * std::abs(std::abs(at(anim::Bone::FootL).x - mid)
                                 - std::abs(at(anim::Bone::FootR).x - mid));
        };
        float bare_yaw = 0.0f;
        float bare_asym = 0.0f;
        float full_yaw = 0.0f;
        float full_asym = 0.0f;
        hips_and_ankles(bare_pose, bare_yaw, bare_asym);
        hips_and_ankles(full_pose, full_yaw, full_asym);
        MESSAGE("покой: рыск линии бёдер " << bare_yaw << " -> " << full_yaw
                                           << " град; асимметрия лодыжек "
                                           << bare_asym << " -> " << full_asym
                                           << " мм");
        CAPTURE(bare_yaw);
        CAPTURE(full_yaw);
        CAPTURE(bare_asym);
        CAPTURE(full_asym);
        // ТАЗ КВАДРАТНЫЙ И НОГИ СИММЕТРИЧНЫ — заказанные ±1 мм.
        CHECK(std::abs(full_yaw) < 0.1f);
        CHECK(full_asym < 1.0f);
        // ...И КОНТРОЛЬ: купленная поза этого не делает ни по одному из двух.
        CHECK(std::abs(bare_yaw) > 5.0f);
        CHECK(bare_asym > 10.0f);
    }
}
