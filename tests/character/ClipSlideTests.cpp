/*
Module: tests/character
File: tests/character/ClipSlideTests.cpp

Responsibility:
- THE FOOT-SLIDE PROBER: how far the planted contact point of the foot
  travels per step at each gear's stride, with the control arm (the same
  clip unbent) that has to come out far worse — and the jog blend's cycle
  against what sim demands. A KNOWN DEFECT ON THE MPFB BODY (ctest label
  known-defect, like sim_great_oak_stair): red on purpose, not relaxed.

Key items:
- foot_slide_under_the_threshold: the prober and its passport.
- the_run_is_not_lopsided: the sprint's antisymmetry under the stride bend —
  a ratchet set on the jog clip the sprint used to play, over on Sprint_Loop.

WHY THIS FILE IS RED AND WHY IT STAYS THAT WAY UNTIL A NAMED WAVE. The
locomotion of 31.08 matched a clip's stride to sim's speed by BENDING the leg
rotations about the bind (stride scale) and handed a gear whichever clip bent
least; the owner's order of 02.09 («ноги твёрдо стоят на земле — искоренить на
глубоком уровне, не костылить подгонкой») retires that mechanism for root
motion taken from the clip's own travel, a planted foot LOCKED, and FootIk
closing the rest — docs/design/LOCOMOTION_GROUNDED.md, a separate wave. This
suite is the instrument that wave is judged by, so its bands are not tuned to
the body of the day.

THE PASSPORT (02.09, MPFB body, legs 0.459/0.493 m against the donor's
0.40/0.43, roles by name — see clip_library_resolves_roles):
- walk 1.8 m/s: Walk_Loop at scale 1.36 slides 3.8 cm/step (unbent: 31.5) —
  inside the band;
- run 6.0 m/s: Sprint_Loop at scale 0.79 slides 8.3 cm/step, and UNBENT it
  slides 5.6 — the bend makes this clip worse, which is the sharpest
  statement the passport has about the bend (the library no longer swaps in
  the jog, which slid 3.2 cm at 0.79 on the previous bake of the body);
- jog 3.0 m/s: the Walk+Jog blend at weight 0.70 covers 2.50 m per cycle where
  2.80 is demanded (11 % short, band 5 %). ROOT CAUSE, found and left for the
  wave that owns the blend: build_clip_library scans the weight with the two
  clips' plants aligned on the SOLO jog's footfall phase, then measure_travel
  overwrites ClipEntry::footfall_phase with the BLEND's plant phase, and every
  later sampling (mix_of, and the frame) aligns clip B on the new value — a
  different blend from the one the scan measured. The fix is a separate
  alignment field (the solo plant) that measure_travel never touches, used by
  mix_of and by the frame's mix time alike.

Dependencies:
- Uses: engine/anim (ClipPlayer), tests/character/ClipTestModel.h, the baked
  assets/objects/characters/HumanBase.dfo (target dfn_characters).
- Used by: ctest (character_clips_slide, label known-defect).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 30: every claim gets a control. The suite going green is the event it
  exists to announce; the assertions are not to be loosened to get there.
*/

#include <doctest/doctest.h>

#include "engine/anim/sources/ClipPlayer.h"
#include "engine/core/skeleton/sources/Skeleton.h"

#include "tests/character/ClipTestModel.h"

#include <glm/gtc/matrix_transform.hpp>

TEST_CASE("foot_slide_under_the_threshold") {
    Model m;
    REQUIRE(load(m));

    // THE THRESHOLD, and it is about a POINT OF THE FOOT rather than about the
    // ankle. The order asks for two centimetres of planted-foot travel per
    // step; four is what the drawn CONTACT POINT could be held to on the
    // Quaternius body at the two gears below, and the gap is named rather
    // than tuned away — artifacts/reports/locomotion-fix/index.html carries
    // every number. THE PASSPORT ON THE MPFB BODY (02.09, legs 0.459/0.493 m
    // against the donor's 0.40/0.43): walk 3.8 cm at scale 1.36 (loose 31.5),
    // Sprint_Loop 8.3 cm at 0.79 (loose 5.6 — the bend makes it worse), and
    // the jog blend covering 2.50 m where 2.80 is demanded. None of these numbers is to be tuned here: the contract that
    // replaces the bend is docs/design/LOCOMOTION_GROUNDED.md.
    constexpr float THRESHOLD_M = 0.04f;
    // The control arm has to be far worse than the threshold, not merely
    // worse: a prober that separates the two arms by a hair is a prober whose
    // next refactor silently stops separating them at all.
    constexpr float CONTROL_RATIO = 4.0f;

    struct Case {
        anim::ClipRole role;
        float speed;
    };
    // THE RUN IS ON THIS LIST, and it was the locomotion wave's headline: it
    // used to slide 0.191 m per step because the clip's own travel was
    // measured at the ANKLE of a body that lands on its BALL, which read
    // Sprint_Loop as covering 0.698 m per cycle where it covers 6.08.
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
        // THE CONTROL ARM IS THE SAME ARM FOR BOTH GEARS since the sprint
        // plays its own clip again (clip_library_resolves_roles): the clip
        // left at the stride its author drew, which a stride-matched clip has
        // to beat by a wide margin or the matching bought nothing.
        CHECK(loose.worst_per_step_m > CONTROL_RATIO * THRESHOLD_M);
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
    // PASSPORT (02.09, MPFB body): Sprint_Loop at stride scale 0.79 reads
    // 0.274 — the ratchet was set on the JOG clip the sprint used to play
    // (0.212 at 0.81), and the sprint clip is the more lopsided of the two.
    // The number is a property of the bend as much as of the clip, and the
    // bend is what LOCOMOTION_GROUNDED retires: re-measure at scale 1 there.
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
