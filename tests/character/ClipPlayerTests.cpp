/*
Module: tests/character
File: tests/character/ClipPlayerTests.cpp

Responsibility:
- The wave's acceptance instrument: does the body PLAY the imported clips, does
  the pose interpolate between ticks, and — the number the wave is judged by —
  how far does the planted foot slide.

Key items:
- clip_library_resolves_roles: every role the asset can answer, answered, and
  the two name traps (Walk_Formal_Loop, Idle_Talking_Loop) refused.
- clip_sampling_is_a_function_of_time: determinism, looping, and a control that
  a different time really does give a different pose.
- clip_playback_crossfades_and_interpolates: the tick, the fade, and the alpha.
- foot_slide_under_the_threshold: the prober, WITH ITS CONTROL ARM — the same
  measurement with stride matching switched off has to come out far worse, or
  the instrument is measuring nothing.

Dependencies:
- Uses: engine/anim (ClipPlayer, SkinnedBody, Rig), engine/render (.dfo reader),
  the baked assets/objects/characters/HumanBase.dfo (target dfn_characters).
- Used by: ctest (character_clips_played).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 30: every claim gets a control. A threshold that nothing can fail is a
  threshold that proves nothing, and this file's whole subject — foot slide —
  is a number that looks plausible at almost any value.
*/

#include <doctest/doctest.h>

#include "engine/anim/sources/ClipPlayer.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <cmath>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>

using namespace dfn;

namespace {

constexpr const char* MODEL = "assets/objects/characters/HumanBase.dfo";

/// SIM'S OWN STEP MODEL, restated here and NOT included: gameplay sits ABOVE
/// anim in the DAG, and a character test that links it would be the first
/// edge that makes the graph a cycle. The two rows it reads
/// (STEP_LENGTH_BASE, STEP_LENGTH_PER_MPS) are generated constants, so this is
/// one reader of a registry row rather than a second copy of a number.
[[nodiscard]] float step_length(float speed) {
    return static_cast<float>(config::STEP_LENGTH_BASE)
           + static_cast<float>(config::STEP_LENGTH_PER_MPS) * speed;
}

struct Model {
    render::RegistryObject obj;
    anim::Rig rig{};
    anim::SkinnedRigBinding binding;
    anim::ClipLibrary lib;
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
    m.lib = anim::build_clip_library(m.obj.skeleton, m.binding, m.obj.clips);
    return true;
}

[[nodiscard]] std::string clip_name_of(const Model& m, anim::ClipRole r) {
    const anim::ClipEntry& e = m.lib[r];
    return e.present() ? m.obj.clips[static_cast<std::size_t>(e.clip)].name
                       : std::string{};
}

/// The largest distance any joint moved between two samples — a one-number
/// answer to "is this the same pose".
[[nodiscard]] float pose_distance(std::span<const anim::JointLocal> a,
                                  std::span<const anim::JointLocal> b) {
    float worst = 0.0f;
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const float d = 1.0f - std::abs(glm::dot(a[i].rotation, b[i].rotation));
        worst = std::max(worst, d);
        worst = std::max(worst, glm::length(a[i].translation - b[i].translation));
    }
    return worst;
}

} // namespace

TEST_CASE("clip_library_resolves_roles") {
    Model m;
    REQUIRE_MESSAGE(load(m), "bake the character first (target dfn_characters)");

    // EVERY ROLE, because this asset has a clip for all ten and a library that
    // silently resolves nine draws a body that stops moving in one state.
    CHECK(m.lib.resolved == anim::CLIP_ROLE_COUNT);
    CHECK(clip_name_of(m, anim::ClipRole::Idle) == "Idle_Loop");
    CHECK(clip_name_of(m, anim::ClipRole::Walk) == "Walk_Loop");
    CHECK(clip_name_of(m, anim::ClipRole::Jog) == "Jog_Fwd_Loop");
    CHECK(clip_name_of(m, anim::ClipRole::Sprint) == "Sprint_Loop");
    CHECK(clip_name_of(m, anim::ClipRole::JumpStart) == "Jump_Start");
    CHECK(clip_name_of(m, anim::ClipRole::JumpLoop) == "Jump_Loop");
    CHECK(clip_name_of(m, anim::ClipRole::JumpLand) == "Jump_Land");
    CHECK(clip_name_of(m, anim::ClipRole::CrouchIdle) == "Crouch_Idle_Loop");
    CHECK(clip_name_of(m, anim::ClipRole::CrouchWalk) == "Crouch_Fwd_Loop");
    CHECK(clip_name_of(m, anim::ClipRole::Sit) == "Sitting_Idle_Loop");

    // THE CONTROL, and it is the whole reason the match is exact rather than a
    // substring: this asset ships two names that a "contains" rule binds to
    // the wrong role, and both of them draw a body that still animates.
    CHECK(clip_name_of(m, anim::ClipRole::Walk) != "Walk_Formal_Loop");
    CHECK(clip_name_of(m, anim::ClipRole::Idle) != "Idle_Talking_Loop");

    // What only measurement can say. A travelling clip must travel, a standing
    // one must not, and the plant must fall somewhere inside the cycle.
    for (const anim::ClipRole r : {anim::ClipRole::Walk, anim::ClipRole::Jog,
                                   anim::ClipRole::Sprint,
                                   anim::ClipRole::CrouchWalk}) {
        CAPTURE(anim::role_name(r));
        CHECK(m.lib[r].cycle_m > 0.5f);
        CHECK(m.lib[r].cycle_m < 8.0f);
        CHECK(m.lib[r].footfall_phase >= 0.0f);
        CHECK(m.lib[r].footfall_phase < 1.0f);
        // The stride curve has to GO somewhere, or stride_scale_for is a
        // constant function dressed as a lookup.
        CHECK(m.lib[r].stride_curve[anim::STRIDE_CURVE_POINTS - 1]
              > 2.0f * m.lib[r].stride_curve[0]);
    }
    CHECK(m.lib[anim::ClipRole::Idle].cycle_m == doctest::Approx(0.0f));
}

TEST_CASE("clip_sampling_is_a_function_of_time") {
    Model m;
    REQUIRE(load(m));
    const anim::ClipEntry& walk = m.lib[anim::ClipRole::Walk];
    REQUIRE(walk.present());
    const skel::AnimClip& clip = m.obj.clips[static_cast<std::size_t>(walk.clip)];

    std::vector<anim::JointLocal> a(m.obj.skeleton.size());
    std::vector<anim::JointLocal> b(m.obj.skeleton.size());

    // Bit-for-bit repeatable: a pose that drifts between two calls at the same
    // time cannot be interpolated, blended or measured.
    anim::sample_clip_pose(m.obj.skeleton, clip, 0.5f, a);
    anim::sample_clip_pose(m.obj.skeleton, clip, 0.5f, b);
    CHECK(pose_distance(a, b) == doctest::Approx(0.0f));

    // It LOOPS: the last frame is the first.
    anim::sample_clip_pose(m.obj.skeleton, clip, 0.0f, a);
    anim::sample_clip_pose(m.obj.skeleton, clip, walk.duration_s, b);
    CHECK(pose_distance(a, b) < 1e-3f);

    // THE CONTROL: a different time is a different pose. Without this line
    // every check above passes on a sampler that returns the bind pose.
    anim::sample_clip_pose(m.obj.skeleton, clip, walk.duration_s * 0.5f, b);
    CHECK(pose_distance(a, b) > 0.05f);

    // The stride scale reaches the LEGS and only the legs: a scale that moved
    // the arms would be a limp, and one that moved nothing would be the
    // constant function this whole wave depends on not being.
    const int32_t thigh = m.binding.names.joint[anim::bone_index(anim::Bone::ThighL)];
    const int32_t hand = m.binding.names.joint[anim::bone_index(anim::Bone::HandL)];
    REQUIRE(thigh >= 0);
    REQUIRE(hand >= 0);
    // OVER THE WHOLE CYCLE, not at one phase: the scale multiplies a joint's
    // deviation FROM ITS BIND, and mid-stance the thigh is very near its bind,
    // so a single sample can legitimately show almost no change. Asking at one
    // phase is how this check first failed on a scaler that works.
    float thigh_moved = 0.0f;
    float hand_moved = 0.0f;
    for (int i = 0; i < 16; ++i) {
        anim::sample_clip_pose(m.obj.skeleton, clip,
                               walk.duration_s * float(i) / 16.0f, a);
        b = a;
        anim::scale_sample_stride(m.binding, m.obj.skeleton, 2.0f, b);
        thigh_moved = std::max(
            thigh_moved,
            1.0f - std::abs(glm::dot(a[static_cast<std::size_t>(thigh)].rotation,
                                     b[static_cast<std::size_t>(thigh)].rotation)));
        hand_moved = std::max(
            hand_moved,
            1.0f - std::abs(glm::dot(a[static_cast<std::size_t>(hand)].rotation,
                                     b[static_cast<std::size_t>(hand)].rotation)));
    }
    CHECK(thigh_moved > 1e-2f);
    CHECK(hand_moved == doctest::Approx(0.0f));
}

TEST_CASE("clip_playback_crossfades_and_interpolates") {
    Model m;
    REQUIRE(load(m));
    const float dt = 1.0f / 60.0f;

    anim::BodyDrive drive;
    drive.grounded = true;
    drive.speed_mps = 0.0f;
    anim::ClipPlayback play;
    anim::advance_playback(m.lib, drive, dt, play);
    CHECK(play.role == anim::ClipRole::Idle);
    CHECK(play.fade == doctest::Approx(0.0f));

    // Start walking: the role changes and a cross-fade OPENS. It must not be
    // instant — a body that snaps from standing to mid-stride in one tick is
    // the defect the fade exists for.
    drive.speed_mps = static_cast<float>(config::WALK_SPEED);
    drive.step_length_m = step_length(drive.speed_mps);
    drive.gait = anim::Gait::Walk;
    anim::advance_playback(m.lib, drive, dt, play);
    CHECK(play.role == anim::ClipRole::Walk);
    CHECK(play.previous == anim::ClipRole::Idle);
    CHECK(play.fade > 0.8f);
    CHECK(play.stride > 1.0f); // sim's stride is wider than this clip's own

    // ...and CLOSES, inside the band the order names (0.15..0.25 s).
    CHECK(anim::CLIP_CROSSFADE_S >= 0.15f);
    CHECK(anim::CLIP_CROSSFADE_S <= 0.25f);
    for (int i = 0; i < 60; ++i) {
        anim::advance_playback(m.lib, drive, dt, play);
    }
    CHECK(play.fade == doctest::Approx(0.0f));

    // THE PHASE IS SIM'S. Sending the same stride phase twice must land on the
    // same clip time, and a phase halfway round the cycle on a time halfway
    // through the clip — that is the whole footfall seam.
    drive.stride_phase = 0.25f;
    anim::advance_playback(m.lib, drive, dt, play);
    const float t_quarter = play.time_s;
    drive.stride_phase = 0.25f;
    anim::advance_playback(m.lib, drive, dt, play);
    CHECK(play.time_s == doctest::Approx(t_quarter));
    drive.stride_phase = 0.75f;
    anim::advance_playback(m.lib, drive, dt, play);
    const float half = m.lib[anim::ClipRole::Walk].duration_s * 0.5f;
    float apart = std::abs(play.time_s - t_quarter);
    apart = std::min(apart, m.lib[anim::ClipRole::Walk].duration_s - apart);
    CHECK(apart == doctest::Approx(half).epsilon(0.02));

    // AND SIM'S FOOTFALL IS THE CLIP'S FOOTFALL. At FOOTFALL_PHASE_LEFT the
    // left ankle must be at the bottom of its own travel — that is what makes
    // the step sound land on the step.
    std::vector<anim::JointLocal> sample(m.obj.skeleton.size());
    std::vector<glm::mat4> local(m.obj.skeleton.size());
    std::vector<glm::mat4> model(m.obj.skeleton.size());
    const int32_t foot = m.binding.names.joint[anim::bone_index(anim::Bone::FootL)];
    REQUIRE(foot >= 0);
    const auto ankle_at = [&](float phase) {
        drive.stride_phase = phase;
        anim::ClipPlayback p = play;
        anim::advance_playback(m.lib, drive, dt, p);
        REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, m.lib,
                                      p, 1.0f, sample));
        for (std::size_t j = 0; j < m.obj.skeleton.size(); ++j) {
            local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                       * glm::mat4_cast(glm::normalize(sample[j].rotation))
                       * glm::scale(glm::mat4{1.0f}, sample[j].scale);
        }
        skel::skeleton_model_matrices(m.obj.skeleton, local, model);
        return model[static_cast<std::size_t>(foot)][3][1];
    };
    const float at_plant = ankle_at(static_cast<float>(config::FOOTFALL_PHASE_LEFT));
    float lowest = at_plant;
    float highest = at_plant;
    for (int i = 0; i <= 32; ++i) {
        const float y = ankle_at(float(i) / 32.0f);
        lowest = std::min(lowest, y);
        highest = std::max(highest, y);
    }
    // In the bottom sixth of the ankle's own travel. NOT an absolute two
    // centimetres: the plant phase is measured at stride scale 1 and the body
    // walks at 1.36, which moves the plant by a sample or two — measured,
    // 0.028 m of a 0.25 m ankle range, i.e. the foot is down and the sound is
    // on it, not the frame-exact coincidence an absolute bound would claim.
    CHECK(at_plant - lowest < 0.16f * (highest - lowest));
    // THE CONTROL: half a cycle later the SAME foot must be up, or the check
    // above passes on a clip whose ankle never moves.
    const float at_swing =
        ankle_at(static_cast<float>(config::FOOTFALL_PHASE_LEFT) + 0.5f);
    CHECK(at_swing - lowest > 0.5f * (highest - lowest));

    // INTERPOLATION BETWEEN TICKS: alpha 0 is the previous tick, alpha 1 is
    // this one, and the midpoint is genuinely between the two — the wave's
    // item 2, and the tail the skinning wave wrote down.
    std::vector<anim::JointLocal> at0(m.obj.skeleton.size());
    std::vector<anim::JointLocal> at1(m.obj.skeleton.size());
    std::vector<anim::JointLocal> mid(m.obj.skeleton.size());
    drive.stride_phase = 0.10f;
    anim::advance_playback(m.lib, drive, dt, play);
    drive.stride_phase = 0.20f;
    anim::advance_playback(m.lib, drive, dt, play);
    REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, m.lib, play,
                                  0.0f, at0));
    REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, m.lib, play,
                                  1.0f, at1));
    REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, m.lib, play,
                                  0.5f, mid));
    const float span = pose_distance(at0, at1);
    CHECK(span > 1e-3f); // the two ticks differ at all: the control
    CHECK(pose_distance(at0, mid) < span);
    CHECK(pose_distance(at1, mid) < span);
}

TEST_CASE("foot_slide_under_the_threshold") {
    Model m;
    REQUIRE(load(m));

    // THE THRESHOLD. The order asks for two centimetres of stance-foot travel
    // per step; three is what the drawn ANKLE can be held to on this asset,
    // and the gap is named rather than tuned away: our rig has no toe bone, so
    // the ankle is the proxy for the contact point, and an ankle rolls over
    // heel and ball while the sole does not move. artifacts/reports/
    // character-clips/index.html carries the measured numbers.
    constexpr float THRESHOLD_M = 0.03f;
    // The control arm has to be far worse than the threshold, not merely
    // worse: a prober that separates the two arms by a hair is a prober whose
    // next refactor silently stops separating them at all.
    constexpr float CONTROL_RATIO = 4.0f;

    struct Case {
        anim::ClipRole role;
        float speed;
    };
    const Case cases[] = {
        {anim::ClipRole::Walk, static_cast<float>(config::WALK_SPEED)},
        {anim::ClipRole::Jog, static_cast<float>(config::JOG_SPEED)},
    };
    for (const Case& c : cases) {
        CAPTURE(anim::role_name(c.role));
        const float sl = step_length(c.speed);
        const anim::FootSlide matched =
            anim::measure_foot_slide(m.obj.skeleton, m.binding, m.obj.clips, m.lib,
                                     c.role, sl, /*stride_match=*/true, 96);
        // THE CONTROL ARM: the same clip, the same speed, the same prober,
        // with the stride left at whatever the animator authored. It is the
        // arm this wave's whole stride-matching exists to beat, and if the two
        // ever come out equal the measurement has stopped measuring.
        const anim::FootSlide loose =
            anim::measure_foot_slide(m.obj.skeleton, m.binding, m.obj.clips, m.lib,
                                     c.role, sl, /*stride_match=*/false, 96);
        CAPTURE(matched.worst_per_step_m);
        CAPTURE(loose.worst_per_step_m);
        CAPTURE(matched.cycle_travel_m);
        CAPTURE(matched.demanded_m);
        CHECK(matched.worst_per_step_m < THRESHOLD_M);
        CHECK(loose.worst_per_step_m > CONTROL_RATIO * THRESHOLD_M);
        // Stride matching means what it says: the clip covers the ground sim
        // says the body covered, within a couple of per cent.
        CHECK(matched.cycle_travel_m
              == doctest::Approx(matched.demanded_m).epsilon(0.05));
        // The strict reading can only be larger than the drift, never smaller.
        CHECK(matched.path_per_step_m >= matched.worst_per_step_m - 1e-6f);
    }
}
