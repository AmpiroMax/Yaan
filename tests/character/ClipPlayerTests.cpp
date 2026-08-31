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
#include "engine/anim/sources/FootIk.h"
#include "engine/anim/sources/PoseLayers.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>
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
    /// THE RIG THE GAME SHIPS, built from the NUMBERS rows, and NOT a
    /// default-constructed one.
    ///
    /// A `Rig{}` has zero proportions and identity rest rotations, so the
    /// retarget carries the model into a T-POSE and calls it our rest pose:
    /// measured, the hand then hangs 0.821 m from the pelvis centre instead of
    /// 0.230, which is the very number item 3 of the owner's list is about.
    /// Every measurement below is against the rest pose, so an unbuilt rig is
    /// an instrument calibrated on a body nobody draws (Rule 47).
    anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
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
    m.lib = anim::build_clip_library(m.rig, m.obj.skeleton, m.binding, m.obj.clips);
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
    // THE SPRINT ROLE DOES NOT PLAY Sprint_Loop, AND THAT IS THE POINT.
    // Quaternius authored Sprint_Loop at about 9 m/s and Jog_Fwd_Loop at
    // about 6, which is RUN_SPEED almost exactly, so build_clip_library's
    // measured pick hands our fastest gear the clip named "jog": measured,
    // 0.027 m of planted-foot slide per step against Sprint_Loop's 0.133.
    // A name is the asset author's guess at what a clip is for; the stride
    // it was drawn at is a fact.
    CHECK(clip_name_of(m, anim::ClipRole::Sprint) == "Jog_Fwd_Loop");
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
        // constant function dressed as a lookup. Over the MEASURED cells:
        // the top of the range is where a scaled leg stops planting at all,
        // and those cells are holes rather than zeros (ClipEntry::stride_valid).
        float lo = 0.0f;
        float hi = 0.0f;
        uint32_t measured = 0;
        for (uint32_t i = 0; i < anim::STRIDE_CURVE_POINTS; ++i) {
            if (!m.lib[r].curve_has(i)) {
                continue;
            }
            lo = measured == 0 ? m.lib[r].stride_curve[i] : lo;
            hi = std::max(hi, m.lib[r].stride_curve[i]);
            ++measured;
        }
        CAPTURE(measured);
        CHECK(measured >= 4);
        CHECK(hi > 2.0f * lo);
        // AND THE MEASUREMENT HAS TO BE OF A PLANT. A clip whose foot never
        // stops moving can still be handed a plausible stride by a fit; the
        // residual is what says whether there was anything to fit.
        CHECK(m.lib[r].plant_residual_m < 0.005f);
        // A DUTY FACTOR, AND A SMALL ONE IS A FACT ABOUT THE CLIP, not a
        // failure: this asset's run keeps a foot down for 1.8 % of the cycle
        // at its own stride, which is a stylised sprint and is exactly why
        // the gear pick below prefers the clip with the longer contact.
        // The line is here to catch a measurement that finds NO plant.
        CHECK(m.lib[r].duty > 0.01f);
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

    // THE THRESHOLD, and it is now about a POINT OF THE FOOT rather than about
    // the ankle. The order asks for two centimetres of planted-foot travel per
    // step; four is what the drawn CONTACT POINT can be held to on this asset
    // at the two gears below, and the gap is named rather than tuned away —
    // artifacts/reports/locomotion-fix/index.html carries every number.
    constexpr float THRESHOLD_M = 0.04f;
    // The control arm has to be far worse than the threshold, not merely
    // worse: a prober that separates the two arms by a hair is a prober whose
    // next refactor silently stops separating them at all.
    constexpr float CONTROL_RATIO = 4.0f;

    struct Case {
        anim::ClipRole role;
        float speed;
    };
    // THE RUN IS ON THIS LIST NOW, and it is the wave's headline: it used to
    // slide 0.191 m per step because the clip's own travel was measured at
    // the ANKLE of a body that lands on its BALL, which read Sprint_Loop as
    // covering 0.698 m per cycle where it covers 6.08.
    const Case cases[] = {
        {anim::ClipRole::Walk, static_cast<float>(config::WALK_SPEED)},
        {anim::ClipRole::Sprint, static_cast<float>(config::RUN_SPEED)},
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
        CAPTURE(matched.ankle_per_step_m);
        CAPTURE(loose.worst_per_step_m);
        CAPTURE(matched.cycle_travel_m);
        CAPTURE(matched.demanded_m);
        CHECK(matched.worst_per_step_m < THRESHOLD_M);
        if (c.role == anim::ClipRole::Walk) {
            CHECK(loose.worst_per_step_m > CONTROL_RATIO * THRESHOLD_M);
        } else {
            // THE RUN'S CONTROL ARM IS A DIFFERENT ARM, because leaving its
            // stride unmatched is no longer the bad option: the clip it ended
            // up with is already drawn near RUN_SPEED. The option this gear
            // actually rejected is the clip its NAME points at, and that is
            // the arm that has to be far worse.
            CAPTURE(m.lib[c.role].named_slide_m);
            REQUIRE(m.lib[c.role].named_clip >= 0);
            CHECK(m.obj.clips[static_cast<std::size_t>(m.lib[c.role].named_clip)].name
                  == "Sprint_Loop");
            CHECK(m.lib[c.role].named_slide_m
                  > CONTROL_RATIO * matched.worst_per_step_m);
        }
        // Stride matching means what it says: the clip covers the ground sim
        // says the body covered, within a couple of per cent.
        CHECK(matched.cycle_travel_m
              == doctest::Approx(matched.demanded_m).epsilon(0.05));
        // The strict reading can only be larger than the drift, never smaller.
        CHECK(matched.path_per_step_m >= matched.worst_per_step_m - 1e-6f);
    }
    // THE JOG WAS A NAMED TAIL AND IS NOW A BLEND. sim's 3 m/s is the one gear
    // this asset has no clip near — Walk_Loop is drawn at 1.14 m/s and
    // Jog_Fwd_Loop at 5.98 — so the previous wave played a run shrunk to 0.41
    // of its stride, and its feet skimmed 0.274 m per step. The library now
    // blends the two at the weight whose MEASURED cycle covers what sim asks
    // for, which leaves the stride scale at 1.01: nothing is bent.
    const anim::ClipEntry& jog_entry = m.lib[anim::ClipRole::Jog];
    REQUIRE(jog_entry.mixed());
    const anim::FootSlide jog =
        anim::measure_foot_slide(m.obj.skeleton, m.binding, m.obj.clips, m.lib,
                                 anim::ClipRole::Jog, step_length(config::JOG_SPEED),
                                 true, 96);
    CAPTURE(jog.worst_per_step_m);
    CAPTURE(jog_entry.mix_weight);
    CAPTURE(jog_entry.mix_solo_slide_m);
    CHECK(jog.cycle_travel_m == doctest::Approx(jog.demanded_m).epsilon(0.05));
    CHECK(jog.worst_per_step_m < THRESHOLD_M);
    // THE STRIDE IS NO LONGER BENT, which is the half of the fix the slide
    // figure alone cannot show: a body sliding 3 cm on nearly straight legs
    // and one sliding 3 cm on its own legs look nothing alike.
    CHECK(anim::stride_scale_for(jog_entry, jog.demanded_m)
          == doctest::Approx(1.0f).epsilon(0.15));
    // THE CONTROL ARM: the clip the blend replaced, measured at the same gear.
    // A blend that did not beat it by a wide margin would be an average of two
    // animations bought for nothing.
    CHECK(jog_entry.mix_solo_slide_m > CONTROL_RATIO * jog.worst_per_step_m);
}

TEST_CASE("the_model_faces_the_way_it_walks") {
    Model m;
    REQUIRE(load(m));
    // WHAT THIS IS ABOUT (owner, 31.08: "персонаж повёрнут задом наперёд").
    // The importer bakes a yaw so the model faces -Z, our convention; a clip
    // exported from Blender keys the ROOT's rotation, which REPLACES the bind
    // rotation the yaw was baked into. So the rest pose faced -Z and every
    // clip faced +Z, and the figure walked backwards the moment it moved.
    //
    // THE CONTROL IS THE REST POSE ITSELF: the two are measured by the same
    // code, and the claim is that they AGREE. Before the fix they disagreed
    // by exactly 180 degrees, which is the one error a "does it face -Z"
    // check on either one alone would have passed.
    std::vector<anim::JointLocal> sample(m.obj.skeleton.size());
    std::vector<glm::mat4> local(m.obj.skeleton.size());
    std::vector<glm::mat4> model(m.obj.skeleton.size());
    const auto fk = [&] {
        for (std::size_t j = 0; j < m.obj.skeleton.size(); ++j) {
            local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                       * glm::mat4_cast(glm::normalize(sample[j].rotation))
                       * glm::scale(glm::mat4{1.0f}, sample[j].scale);
        }
        skel::skeleton_model_matrices(m.obj.skeleton, local, model);
    };
    const auto at = [&](anim::Bone b) {
        const int32_t j = m.binding.names.joint[anim::bone_index(b)];
        REQUIRE(j >= 0);
        return glm::vec3{model[static_cast<std::size_t>(j)][3]};
    };
    // The hip line: docs/RIG.md says +X is the character's RIGHT at yaw 0.
    anim::pose_local_transforms(m.rig, m.obj.skeleton, m.binding, anim::LocalPose{},
                               sample);
    fk();
    const float rest_right = (at(anim::Bone::ThighR) - at(anim::Bone::ThighL)).x;
    CHECK(rest_right > 0.1f);
    for (const anim::ClipRole r : {anim::ClipRole::Idle, anim::ClipRole::Walk,
                                   anim::ClipRole::Jog, anim::ClipRole::Sprint}) {
        CAPTURE(anim::role_name(r));
        const anim::ClipEntry& e = m.lib[r];
        REQUIRE(e.present());
        float toe_forward = 0.0f;
        int toe_samples = 0;
        for (int k = 0; k < 8; ++k) {
            anim::sample_clip_pose(m.obj.skeleton,
                                   m.obj.clips[static_cast<std::size_t>(e.clip)],
                                   e.duration_s * float(k) / 8.0f, sample);
            fk();
            CAPTURE(k);
            // RIGHT HIP ON THE RIGHT, at every phase of every clip.
            CHECK((at(anim::Bone::ThighR) - at(anim::Bone::ThighL)).x > 0.1f);
            // AND THE TOES POINT FORWARD, which is -Z — AVERAGED OVER THE
            // CYCLE and not asserted per frame. The hip line alone would pass
            // a model mirrored through its sagittal plane, so the feet are
            // what tell a mirror from a turn; but a swinging foot legitimately
            // points its toe backwards at the top of the swing, and a
            // per-frame line on that is a line about anatomy, not about
            // orientation. Accumulated here, checked after the loop.
            const int32_t toe = m.obj.skeleton.find("DEF-toe.L");
            if (toe >= 0) {
                toe_forward += glm::vec3{model[static_cast<std::size_t>(toe)][3]}.z
                               - at(anim::Bone::FootL).z;
                ++toe_samples;
            }
        }
        if (toe_samples > 0) {
            // THE BOUND IS A SIGN, WITH ROOM, and not a magnitude: the
            // defect this catches flips the number, it does not shrink it.
            // Measured on the run before the fix, +0.018 m; after, -0.018.
            // The walk sits at -0.09, where the foot is flat most of the
            // cycle; a run points its toe down and back through half of its.
            CAPTURE(toe_forward / float(toe_samples));
            CHECK(toe_forward / float(toe_samples) < -0.01f);
        }
    }
}

namespace {

/// THE GROUND THE STAND HAS, as three functions of a point. They are the same
/// three surfaces assets/maps/stands/character.scene puts under the figure —
/// flat deck, the 15-degree skirt between its two terraces, and the canonical
/// 0.18/0.28 flight — written here so the number is reproducible without a
/// window. The frames are still shot on the stand; this is the instrument.
using Ground = float (*)(const glm::vec3&);

float flat_ground(const glm::vec3&) { return 0.0f; }

/// The stand's skirt: about 15 degrees, rising the way the figure faces (-Z).
float slope_ground(const glm::vec3& p) { return std::tan(0.262f) * (-p.z); }

/// HUMAN_SCALE's canonical flight, 0.18 m rise on a 0.28 m going, AS THE BODY
/// MEETS IT: the treads MINUS the ramp the capsule is already riding.
///
/// THIS SUBTRACTION IS THE WHOLE INSTRUMENT AND IT IS NOT A CONVENIENCE. The
/// character controller collides with the flight's nosings, so what it stands
/// on is the 33-degree ramp, not the treads; the root is therefore already at
/// ramp height and each foot's own tread deviates from it by at most half a
/// rise. Written WITHOUT the subtraction, the surface says the trailing foot
/// of a 0.98 m stride is three treads — 0.54 m — below the leading one, which
/// no leg can span and no staircase asks it to: that arrangement is not a
/// figure climbing stairs, it is a figure standing in a stairwell wall.
float stair_ground(const glm::vec3& p) {
    constexpr float RISE = 0.18f;
    constexpr float GOING = 0.28f;
    const float forward = -p.z;
    return RISE * std::floor(forward / GOING + 0.5f) - (RISE / GOING) * forward;
}

/// ONE NOSING, the case the order names in words: one foot on the tread and
/// one on the tread below. A whole rise between the two feet, which a leg CAN
/// span and which the root shift plus one knee is exactly the mechanism for.
float step_edge_ground(const glm::vec3& p) { return -p.z >= 0.0f ? 0.18f : 0.0f; }

} // namespace

TEST_CASE("the_feet_stay_on_the_ground") {
    Model m;
    REQUIRE(load(m));
    // WHAT THIS IS ABOUT (owner, 31.08: "ступни проходят сквозь землю", and
    // 31.08 again: "и на рельефе, и на лестнице"). The previous wave pressed
    // the clip onto the FLAT floor its author drew it on, with one constant
    // per stride scale. A stair tread and a hillside are not that floor, and
    // no constant is: the ground has to be asked where it is, under each foot,
    // every frame.
    REQUIRE(m.lib.contacts.valid());
    CHECK(m.lib.contacts.side[0].count == 2); // both feet found a toe
    CHECK(m.lib.contacts.side[1].count == 2);
    const anim::FootIkSetup setup =
        anim::build_foot_ik(m.obj.skeleton, m.binding, m.lib.contacts);
    REQUIRE(setup.valid());
    CHECK(setup.toe[0] >= 0);
    CHECK(setup.toe[1] >= 0);

    std::vector<anim::JointLocal> sample(m.obj.skeleton.size());
    std::vector<glm::mat4> local(m.obj.skeleton.size());
    std::vector<glm::mat4> model(m.obj.skeleton.size());
    const auto fk = [&] {
        for (std::size_t j = 0; j < m.obj.skeleton.size(); ++j) {
            local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                       * glm::mat4_cast(glm::normalize(sample[j].rotation))
                       * glm::scale(glm::mat4{1.0f}, sample[j].scale);
        }
        skel::skeleton_model_matrices(m.obj.skeleton, local, model);
    };
    // THE POSE AS THE FRAME DRAWS IT, not as a clip sample: the jog is a BLEND
    // now and the arms carry a layer, so anything measured on a raw clip is
    // measured on a body the renderer does not draw.
    const auto pose_of = [&](anim::Gait gait, float speed, float phase) {
        anim::BodyDrive drive;
        drive.gait = gait;
        drive.speed_mps = speed;
        drive.step_length_m = step_length(speed);
        drive.stride_phase = phase;
        drive.grounded = true;
        anim::ClipPlayback play;
        anim::advance_playback(m.lib, drive, 1.0f / 30.0f, play);
        anim::advance_playback(m.lib, drive, 1.0f / 30.0f, play);
        REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, m.lib,
                                      play, 1.0f, sample));
    };
    // WHAT A RAYCAST WOULD HAVE ANSWERED, from the pose itself: the app pushes
    // a ray down under each contact point and this is the same query, run on a
    // surface written in one line instead of loaded from a map.
    const auto probe_with = [&](Ground g) {
        fk();
        anim::FootIkProbe probe;
        probe.valid = true;
        for (int i = 0; i < 2; ++i) {
            const auto side = static_cast<std::size_t>(i);
            probe.ankle_ground[side] =
                g(glm::vec3{model[static_cast<std::size_t>(setup.ankle[side])][3]});
            probe.toe_ground[side] =
                g(glm::vec3{model[static_cast<std::size_t>(setup.toe[side])][3]});
        }
        return probe;
    };

    struct Case {
        anim::Gait gait;
        float speed;
        const char* label;
    };
    const Case gears[] = {
        {anim::Gait::Walk, 0.0f, "idle"},
        {anim::Gait::Walk, static_cast<float>(config::WALK_SPEED), "walk"},
        {anim::Gait::Jog, static_cast<float>(config::JOG_SPEED), "jog"},
        {anim::Gait::Run, static_cast<float>(config::RUN_SPEED), "run"},
    };
    struct Surface {
        Ground g;
        const char* label;
        float control_floor_m; ///< the control arm has to be at least this bad
    };
    const Surface surfaces[] = {
        // FLAT IS NOT A CONTROL SURFACE and says so: the previous wave already
        // closed it, so its "without IK" arm is legitimately near zero and a
        // floor on it would be a threshold nothing can fail (Rule 45).
        {flat_ground, "flat", 0.0f},
        {slope_ground, "slope 15 deg", 0.03f},
        {stair_ground, "canonical flight 0.18/0.28", 0.05f},
        {step_edge_ground, "one nosing 0.18", 0.05f},
    };
    // NOT ONE CENTIMETRE UNDER THE GROUND, which is the order's number.
    constexpr float PENETRATION_M = 0.01f;
    for (const Surface& sfc : surfaces) {
        CAPTURE(std::string(sfc.label));
        float worst_fixed = 0.0f;
        float worst_raw = 0.0f;
        for (const Case& c : gears) {
            CAPTURE(std::string(c.label));
            for (int k = 0; k < 16; ++k) {
                pose_of(c.gait, c.speed, float(k) / 16.0f);
                const anim::FootIkProbe probe = probe_with(sfc.g);
                // THE CONTROL ARM IS THE SAME FRAME WITH strength 0, taken
                // first: the solve is a bit-for-bit no-op there, so the two
                // arms differ by the solve and by nothing else (Rule 47).
                const anim::FootIkPlan plan =
                    anim::plan_foot_ik(m.obj.skeleton, setup, probe, sample);
                worst_raw = std::max(
                    worst_raw,
                    anim::foot_penetration(m.obj.skeleton, setup, probe, plan, sample));
                anim::apply_foot_ik(m.obj.skeleton, setup, probe, plan, 1.0f, sample);
                worst_fixed = std::max(
                    worst_fixed,
                    anim::foot_penetration(m.obj.skeleton, setup, probe, plan, sample));
            }
        }
        CAPTURE(worst_raw);
        CAPTURE(worst_fixed);
        MESSAGE("penetration on " << std::string(sfc.label) << ": " << 100.0f * worst_fixed
                                  << " cm with the solve, " << 100.0f * worst_raw
                                  << " cm without");
        CHECK(worst_fixed < PENETRATION_M);
        if (sfc.control_floor_m > 0.0f) {
            CHECK(worst_raw > sfc.control_floor_m);
        }
    }

    // AND THE FIGURE DOES NOT GROW 18 CM ON THE GEAR CHANGE (owner, 31.08).
    // The previous wave grounded the jog with a constant of 0.188 m, measured
    // against the walk's 0.005: the body rose 18 cm over the 0.18 s crossfade.
    // The blended jog needs 0.035, and the number checked here is the one an
    // eye sees — the height of the PELVIS on flat ground, gear against gear.
    const auto pelvis_y = [&](anim::Gait gait, float speed) {
        float sum = 0.0f;
        for (int k = 0; k < 16; ++k) {
            pose_of(gait, speed, float(k) / 16.0f);
            const anim::FootIkProbe probe = probe_with(flat_ground);
            const anim::FootIkPlan plan =
                anim::plan_foot_ik(m.obj.skeleton, setup, probe, sample);
            anim::apply_foot_ik(m.obj.skeleton, setup, probe, plan, 1.0f, sample);
            fk();
            const int32_t pelvis = m.binding.names.joint[anim::bone_index(anim::Bone::Pelvis)];
            sum += model[static_cast<std::size_t>(pelvis)][3][1];
        }
        return sum / 16.0f;
    };
    const float walk_y = pelvis_y(anim::Gait::Walk, static_cast<float>(config::WALK_SPEED));
    const float jog_y = pelvis_y(anim::Gait::Jog, static_cast<float>(config::JOG_SPEED));
    CAPTURE(walk_y);
    CAPTURE(jog_y);
    MESSAGE("pelvis walk " << walk_y << " m, jog " << jog_y << " m, difference "
                           << 100.0f * std::abs(jog_y - walk_y) << " cm");
    CHECK(std::abs(jog_y - walk_y) < 0.05f);
    // THE CONTROL ARM: what the rejected solo jog would have asked for. It is
    // kept on the entry precisely so this line can exist.
    const anim::ClipEntry& jog_entry = m.lib[anim::ClipRole::Jog];
    CAPTURE(jog_entry.mix_solo_lift_m);
    CHECK(jog_entry.mix_solo_lift_m > 0.10f);
}

TEST_CASE("the_run_is_not_lopsided") {
    Model m;
    REQUIRE(load(m));
    // WHAT THIS IS ABOUT (owner, 31.08: "бег перекошен в одну сторону"). A
    // gait is ANTISYMMETRIC, not symmetric: the left side at phase p is the
    // mirror of the right side half a cycle later. That is the quantity, and
    // measuring plain left-right symmetry instead would fail every correct
    // walk ever animated.
    std::vector<anim::JointLocal> sample(m.obj.skeleton.size());
    std::vector<glm::mat4> local(m.obj.skeleton.size());
    std::vector<glm::mat4> model(m.obj.skeleton.size());
    const auto pose_at = [&](const anim::ClipEntry& e, float t, float scale) {
        anim::sample_clip_pose(m.obj.skeleton,
                               m.obj.clips[static_cast<std::size_t>(e.clip)], t,
                               sample);
        anim::scale_sample_stride(m.binding, m.obj.skeleton, scale, sample);
        for (std::size_t j = 0; j < m.obj.skeleton.size(); ++j) {
            local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                       * glm::mat4_cast(glm::normalize(sample[j].rotation))
                       * glm::scale(glm::mat4{1.0f}, sample[j].scale);
        }
        skel::skeleton_model_matrices(m.obj.skeleton, local, model);
    };
    const auto at = [&](anim::Bone b) {
        const int32_t j = m.binding.names.joint[anim::bone_index(b)];
        return glm::vec3{model[static_cast<std::size_t>(j)][3]};
    };
    const anim::Bone L[] = {anim::Bone::ThighL, anim::Bone::ShinL, anim::Bone::FootL,
                            anim::Bone::UpperArmL, anim::Bone::ForearmL,
                            anim::Bone::HandL};
    const anim::Bone R[] = {anim::Bone::ThighR, anim::Bone::ShinR, anim::Bone::FootR,
                            anim::Bone::UpperArmR, anim::Bone::ForearmR,
                            anim::Bone::HandR};
    const anim::ClipEntry& e = m.lib[anim::ClipRole::Sprint];
    REQUIRE(e.present());
    const float scale =
        anim::stride_scale_for(e, 2.0f * step_length(config::RUN_SPEED));
    float worst = 0.0f;
    for (int k = 0; k < 32; ++k) {
        const float t = e.duration_s * float(k) / 32.0f;
        pose_at(e, t, scale);
        const glm::vec3 hip_a = at(anim::Bone::Pelvis);
        glm::vec3 left[6];
        for (int i = 0; i < 6; ++i) {
            left[i] = at(L[i]) - hip_a;
        }
        pose_at(e, std::fmod(t + 0.5f * e.duration_s, e.duration_s), scale);
        const glm::vec3 hip_b = at(anim::Bone::Pelvis);
        for (int i = 0; i < 6; ++i) {
            const glm::vec3 r = at(R[i]) - hip_b;
            worst = std::max(worst,
                             glm::length(left[i] - glm::vec3{-r.x, r.y, r.z}));
        }
    }
    CAPTURE(worst);
    CAPTURE(scale);
    // A QUARTER OF A METRE, and it is a RATCHET and not a derivation: it is
    // the measured 0.212 m with room, and it exists to notice the day a
    // change makes the run visibly more crooked than the one the owner was
    // shown. The clip's own asymmetry (this asset swings its left arm wider
    // than its right) is inside it and is not ours to fix.
    CHECK(worst < 0.25f);
    // THE CONTROL: the measurement is capable of failing. The same clip
    // compared against ITSELF instead of against its half-cycle mirror has
    // to come out far worse, or the mirror is not being taken.
    float sham = 0.0f;
    for (int k = 0; k < 32; ++k) {
        pose_at(e, e.duration_s * float(k) / 32.0f, scale);
        const glm::vec3 hip = at(anim::Bone::Pelvis);
        for (int i = 0; i < 6; ++i) {
            const glm::vec3 r = at(R[i]) - hip;
            sham = std::max(sham, glm::length((at(L[i]) - hip)
                                              - glm::vec3{-r.x, r.y, r.z}));
        }
    }
    CAPTURE(sham);
    CHECK(sham > 2.0f * worst);
}

TEST_CASE("the_hands_hang_like_a_persons") {
    Model m;
    REQUIRE(load(m));
    // WHAT THIS IS ABOUT (owner, 31.08: "всё время боевая стойка"). The
    // previous wave proved the library picks NEUTRAL clips and that the
    // retarget is not in the path at all, and measured the real cause: this
    // asset's own idle holds the hand 0.403 m sideways from the pelvis centre
    // — nearly the 0.398 of its own "sword ready" — while our rest pose holds
    // it at 0.230, and the hand is sculpted into a fist in every clip.
    //
    // THE LAYER IS WHAT ANSWERS IT, and the number below is the order's band.
    REQUIRE(m.lib.relax.valid());
    CAPTURE(m.lib.relax.angle_rad * 57.29578f);
    CAPTURE(m.lib.relax.target_m);
    CAPTURE(m.lib.relax.reference_m);
    // The target the solve aimed at IS our rest pose, read through the same
    // retarget the frame uses. If this ever stops being ~0.23 the rig changed,
    // and the band below stops meaning what it says.
    CHECK(m.lib.relax.target_m == doctest::Approx(0.23f).epsilon(0.15));

    std::vector<anim::JointLocal> sample(m.obj.skeleton.size());
    struct Case {
        anim::ClipRole role;
        const char* label;
    };
    for (const Case& c : {Case{anim::ClipRole::Idle, "idle"},
                          Case{anim::ClipRole::Walk, "walk"}}) {
        CAPTURE(std::string(c.label));
        const anim::ClipEntry& e = m.lib[c.role];
        REQUIRE(e.present());
        anim::sample_clip_pose(m.obj.skeleton,
                               m.obj.clips[static_cast<std::size_t>(e.clip)], 0.0f,
                               sample);
        // THE CONTROL ARM, TAKEN FIRST: the same frame with the layer at
        // weight 0, which the layer guarantees is a bit-for-bit no-op. It is
        // the pose the owner was looking at, and it has to still be wide.
        const anim::HandSpread before =
            anim::measure_hand_spread(m.obj.skeleton, m.binding, sample);
        const float open_before =
            anim::measure_hand_openness(m.obj.skeleton, m.lib.relax, sample);
        anim::apply_arm_relax(m.obj.skeleton, m.lib.relax, anim::ArmRelaxDose{}, sample);
        const anim::HandSpread after =
            anim::measure_hand_spread(m.obj.skeleton, m.binding, sample);
        const float open_after =
            anim::measure_hand_openness(m.obj.skeleton, m.lib.relax, sample);
        CAPTURE(before.left);
        CAPTURE(before.right);
        CAPTURE(after.left);
        CAPTURE(after.right);
        // THE ARMS COME IN. Judged on the MEAN of the two hands and not on
        // each: this asset swings its left arm wider than its right in every
        // clip it has, and one angle for both sides deliberately leaves that
        // asymmetry alone rather than baking a permanent lean into the body.
        const float mean_before = 0.5f * (before.left + before.right);
        const float mean_after = 0.5f * (after.left + after.right);
        MESSAGE("hand spread " << mean_before << " -> " << mean_after
                               << " m; hand openness " << open_before << " -> "
                               << open_after << " m");
        CHECK(mean_after < mean_before - 0.05f);
        if (c.role == anim::ClipRole::Idle) {
            // THE ORDER'S BAND, on the pose the complaint is about.
            CHECK(mean_after > 0.20f);
            CHECK(mean_after < 0.29f);
            // ...AND THE CONTROL: the arm the wave started from was outside it.
            CHECK(mean_before > 0.33f);
        }
        // THE FIST OPENS. A hand measured from its own wrist to its fingertips
        // is longer open than closed, and the layer takes the fingers back to
        // their BIND, which on this asset is an open hand.
        CAPTURE(open_before);
        CAPTURE(open_after);
        CHECK(open_after > open_before + 0.02f);
    }
}

TEST_CASE("a_drawn_weapon_is_an_upper_body_layer") {
    Model m;
    REQUIRE(load(m));
    // WHAT THIS IS ABOUT (owner, 31.08, items 5-6): T puts a weapon in the
    // hands. There is no weapon MODEL yet, so the whole of the visible change
    // is the pose: the upper half takes the asset's guard, the legs keep
    // walking, and the arm layer of item 3 comes off.
    REQUIRE(m.lib.mask.valid());
    // THE MASK IS THE MECHANISM, and a mask with an empty half would make
    // every layered state look exactly like the unlayered one.
    CAPTURE(m.lib.mask.count(anim::Branch::Upper));
    CAPTURE(m.lib.mask.count(anim::Branch::Lower));
    CHECK(m.lib.mask.count(anim::Branch::Upper) > 10);
    CHECK(m.lib.mask.count(anim::Branch::Lower) >= 6); // thighs, shins, feet, toes
    REQUIRE(m.lib.has(anim::ClipRole::WeaponIdle));
    CHECK(m.obj.clips[static_cast<std::size_t>(m.lib[anim::ClipRole::WeaponIdle].clip)]
              .name == "Sword_Idle");

    const auto walk_pose = [&](bool drawn, std::span<anim::JointLocal> out) {
        anim::BodyDrive drive;
        drive.gait = anim::Gait::Walk;
        drive.speed_mps = static_cast<float>(config::WALK_SPEED);
        drive.step_length_m = step_length(static_cast<float>(config::WALK_SPEED));
        drive.stride_phase = 0.25f;
        drive.grounded = true;
        drive.weapon_drawn = drawn;
        anim::ClipPlayback play;
        // Long enough for the 0.2 s crossfade to finish, so the two arms are
        // the two STATES and not two points of one transition.
        for (int i = 0; i < 30; ++i) {
            anim::advance_playback(m.lib, drive, 1.0f / 30.0f, play);
        }
        CHECK(play.weapon == doctest::Approx(drawn ? 1.0f : 0.0f));
        REQUIRE(anim::playback_sample(m.obj.skeleton, m.binding, m.obj.clips, m.lib,
                                      play, 1.0f, out));
    };
    std::vector<anim::JointLocal> sheathed(m.obj.skeleton.size());
    std::vector<anim::JointLocal> drawn(m.obj.skeleton.size());
    walk_pose(false, sheathed);
    walk_pose(true, drawn);

    // THE LEGS DO NOT MOVE. This is the claim the mask exists for, and it is
    // the one a "the pose changed" check would pass without.
    float worst_lower = 0.0f;
    float worst_upper = 0.0f;
    for (std::size_t j = 0; j < m.obj.skeleton.size(); ++j) {
        const float d =
            1.0f - std::abs(glm::dot(glm::normalize(sheathed[j].rotation),
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
    CHECK(worst_upper > 1e-3f); // the control: the upper half really did move

    // AND THE GUARD IS A GUARD. Drawn, the hands come UP and OUT from where
    // the relaxed layer put them — measured, not assumed, because "the pose
    // changed" is also true of a body that shrugged.
    const anim::HandSpread s_spread =
        anim::measure_hand_spread(m.obj.skeleton, m.binding, sheathed);
    const anim::HandSpread d_spread =
        anim::measure_hand_spread(m.obj.skeleton, m.binding, drawn);
    CAPTURE(s_spread.left);
    CAPTURE(d_spread.left);
    CHECK(0.5f * (d_spread.left + d_spread.right)
          > 0.5f * (s_spread.left + s_spread.right) + 0.05f);
}
