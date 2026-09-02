/*
Module: engine/anim
File: engine/anim/sources/RestFit.cpp

Responsibility:
- Implements the rest-pose solve: build the rig in the attention stance,
  bind, fit the boxes, measure the gaps on the skin, raise the abduction and
  the splay by the lever the deficit asks for, repeat.

Dependencies:
- Uses: RestFit.h, BodyGaps.h, Hitbox.h, SkinnedBody.h, glm.
- Used by: dfn_anim.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Nothing here reads a clock, a file or the ECS.
*/

#include "engine/anim/sources/RestFit.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dfn::anim {
namespace {

/// A LITTLE PAST THE TARGET, so a pass that lands exactly on the threshold
/// does not spend the next pass a quarter-millimetre short of it. Two
/// millimetres: below what a frame can show, above float noise on a metre.
constexpr float OVERSHOOT_M = 0.002f;

/// The largest step one pass may take, radians. A deficit measured at a
/// crossed leg can be tens of centimetres (the legs are INSIDE each other and
/// the lever reads the whole overlap); asin of that over a leg length is a
/// real angle and a fine first step, but a hand reading "not beside the
/// thigh" must not fling the arm out: the cap keeps every pass a stance a
/// person could stand in.
constexpr float MAX_STEP_RAD = 0.35f;

struct Joints {
    glm::vec3 shoulder[2]{};
    glm::vec3 wrist[2]{};
    glm::vec3 hip[2]{};
    glm::vec3 ankle[2]{};
    bool valid = false;
};

[[nodiscard]] Joints rest_joints(const Rig& rig, const skel::Skeleton& skeleton,
                                 const SkinnedRigBinding& binding) {
    Joints j;
    std::vector<glm::mat4> model(skeleton.size());
    rest_model_matrices(rig, skeleton, binding, LocalPose{}, model);
    const auto at = [&](Bone b, glm::vec3& out) {
        const int32_t k = binding.names.joint[bone_index(b)];
        if (k < 0 || static_cast<std::size_t>(k) >= model.size()) {
            return false;
        }
        out = glm::vec3{model[static_cast<std::size_t>(k)][3]};
        return true;
    };
    j.valid = at(Bone::UpperArmL, j.shoulder[0]) && at(Bone::UpperArmR, j.shoulder[1])
              && at(Bone::HandL, j.wrist[0]) && at(Bone::HandR, j.wrist[1])
              && at(Bone::ThighL, j.hip[0]) && at(Bone::ThighR, j.hip[1])
              && at(Bone::FootL, j.ankle[0]) && at(Bone::FootR, j.ankle[1]);
    return j;
}

/// asin of a ratio, clamped, so a deficit longer than the lever asks for the
/// full quarter turn and not for NaN.
[[nodiscard]] float lever(float deficit_m, float arm_m) {
    if (arm_m < 1.0e-3f) {
        return 0.0f;
    }
    const float r = std::clamp(deficit_m / arm_m, -1.0f, 1.0f);
    return std::min(std::asin(r), MAX_STEP_RAD);
}

} // namespace

RestFit fit_rest_pose(const RigProportions& p, const skel::Skeleton& skeleton,
                      std::span<const platform::SkinnedVertex> skin,
                      const BodyGapTargets& targets, bool legacy) {
    RestFit fit;
    fit.targets = targets;
    RestStance stance = legacy ? RestStance::converged(p) : RestStance::attention();
    fit.rig = Rig::build(p, stance);
    fit.binding = bind_skinned_rig(fit.rig, skeleton);
    if (skeleton.empty() || skin.empty() || fit.binding.bound_count() == 0) {
        return fit; // nothing to measure against: the unfitted rest, said so
    }
    fit.parts = label_skin_parts(skeleton, fit.binding, skin);
    std::vector<JointLocal> sample(skeleton.size());
    const auto measure = [&] {
        fit.boxes = build_hitboxes(fit.rig.proportions);
        fit_hitboxes_to_skin(fit.boxes, fit.rig, skeleton, fit.binding, skin);
        pose_local_transforms(fit.rig, skeleton, fit.binding, LocalPose{}, sample);
        fit.gaps = measure_body_gaps(skeleton, fit.binding, fit.boxes, skin, fit.parts,
                                     sample);
    };
    measure();
    if (legacy) {
        fit.met = gaps_meet(fit.gaps, targets);
        return fit; // the "before" arm is measured, never corrected
    }
    // THE LAST STANCE THAT DID NOT MEET THE TARGETS, kept for the tightening
    // below: the lever overshoots (it reads the deficit at a limb that is
    // INSIDE the other one), and «по швам» means the smallest angle that
    // clears, not the first one that does.
    RestStance unmet = stance;
    const auto rebuild = [&](const RestStance& s) {
        stance = s;
        fit.rig = Rig::build(p, stance);
        fit.binding = bind_skinned_rig(fit.rig, skeleton);
        measure();
    };
    for (uint32_t pass = 0; pass < REST_FIT_MAX_PASSES; ++pass) {
        fit.passes = pass + 1;
        if (gaps_meet(fit.gaps, targets)) {
            fit.met = true;
            break;
        }
        unmet = stance;
        const Joints j = rest_joints(fit.rig, skeleton, fit.binding);
        if (!j.valid) {
            return fit;
        }
        // THE ARMS: whichever of the two pairs is shorter of its target asks
        // for the turn; the hand's lever is the whole arm, the forearm's the
        // arm down to the forearm's middle (its elbow sits roughly halfway).
        const float arm_len = 0.5f * (glm::length(j.wrist[0] - j.shoulder[0])
                                      + glm::length(j.wrist[1] - j.shoulder[1]));
        const float hand_short = targets.hand_thigh_m + OVERSHOOT_M
                                 - fit.gaps.hand_thigh_worst_m();
        const float fore_short = targets.forearm_trunk_m + OVERSHOOT_M
                                 - fit.gaps.forearm_trunk_worst_m();
        float arm_turn = 0.0f;
        if (hand_short > 0.0f) {
            arm_turn = std::max(arm_turn, lever(hand_short, arm_len));
        }
        if (fore_short > 0.0f) {
            arm_turn = std::max(arm_turn, lever(fore_short, 0.6f * arm_len));
        }
        // THE LEGS: half the deficit per leg, on the hip->ankle lever.
        const float leg_len = 0.5f * (glm::length(j.ankle[0] - j.hip[0])
                                      + glm::length(j.ankle[1] - j.hip[1]));
        const float legs_short = targets.legs_m + OVERSHOOT_M - fit.gaps.legs.judged_m();
        const float leg_turn = legs_short > 0.0f ? lever(0.5f * legs_short, leg_len) : 0.0f;
        if (arm_turn <= 0.0f && leg_turn <= 0.0f) {
            return fit; // met by the box measure but not by ours: report it
        }
        RestStance next = stance;
        next.arm_abduction_rad += arm_turn;
        next.leg_splay_rad += leg_turn;
        rebuild(next);
    }
    if (!fit.met) {
        fit.met = gaps_meet(fit.gaps, targets);
        return fit;
    }
    // TIGHTEN: bisect between the last stance that fell short and the one
    // that cleared, on both angles at once, keeping the clearing side. Six
    // halvings bring the angles within a sixty-fourth of the last step, i.e.
    // the gaps within a millimetre of the rows on this body.
    RestStance met = stance;
    for (int step = 0; step < 6; ++step) {
        RestStance mid = met;
        mid.arm_abduction_rad = 0.5f * (unmet.arm_abduction_rad + met.arm_abduction_rad);
        mid.leg_splay_rad = 0.5f * (unmet.leg_splay_rad + met.leg_splay_rad);
        rebuild(mid);
        ++fit.passes;
        if (gaps_meet(fit.gaps, targets)) {
            met = mid;
        } else {
            unmet = mid;
        }
    }
    if (stance.arm_abduction_rad != met.arm_abduction_rad
        || stance.leg_splay_rad != met.leg_splay_rad) {
        rebuild(met);
    }
    fit.met = gaps_meet(fit.gaps, targets);
    return fit;
}

Rig rest_rig_for(const skel::Skeleton& skeleton,
                 std::span<const platform::SkinnedVertex> skin, bool legacy) {
    return fit_rest_pose(RigProportions::from_config(), skeleton, skin,
                         BodyGapTargets::from_config(), legacy)
        .rig;
}

} // namespace dfn::anim
