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

/// THE STEP TAKEN WHEN THE FLESH ALREADY CLEARS AND ONLY THE BOXES TOUCH.
/// hitbox_pair_distance reports an intersection as exactly zero — it has no
/// depth to hand the lever — so the pass turns by one degree and the
/// tightening below finds the smallest angle that clears. One degree is
/// 1 cm at a hanging hand and 2 mm at the crotch: fine enough that six
/// halvings land within a tenth of a degree.
constexpr float BOX_STEP_RAD = 0.01745f;

/// The shortest lever the solve will believe, metres. The worst band can sit
/// right at the joint (the crotch is a few centimetres under the hip), and a
/// lever of a millimetre would ask for a quarter turn per pass.
constexpr float MIN_LEVER_M = 0.05f;

/// How many halvings the tightening takes: within a sixty-fourth of the last
/// lever step, i.e. the gaps within a millimetre of the rows on this body.
constexpr int TIGHTEN_STEPS = 6;

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

/// THE LEVER THE DEFICIT ACTUALLY ACTS ON: from the joint the limb turns
/// about down to the band where the gap is worst. Measured on the MPFB body:
/// the legs' tightest band is 6 cm under the hip joint, and asin over the
/// whole 0.95 m leg asked for 0.1° a pass where 1.7° was needed — twelve
/// passes moved the legs 3.3° and left the gap 3 mm short. Falls back to the
/// whole limb when the gap has no band (pairs only).
[[nodiscard]] float band_lever(float joint_y, float worst_y, float whole_m) {
    if (std::isnan(worst_y)) {
        return whole_m;
    }
    return std::max(MIN_LEVER_M, std::abs(joint_y - worst_y));
}

[[nodiscard]] bool legs_clear(const BodyGaps& g, const BodyGapTargets& t) {
    return g.valid && g.legs.judged_m() >= t.legs_m && g.legs_box_m > 0.0f;
}

[[nodiscard]] bool arms_clear(const BodyGaps& g, const BodyGapTargets& t) {
    return g.valid && g.hand_thigh_worst_m() >= t.hand_thigh_m
           && g.forearm_trunk_worst_m() >= t.forearm_trunk_m
           && g.hand_thigh_box_m[0] > 0.0f && g.hand_thigh_box_m[1] > 0.0f;
}

} // namespace

bool rest_pose_clear(const BodyGaps& gaps, const BodyGapTargets& targets) {
    return legs_clear(gaps, targets) && arms_clear(gaps, targets);
}

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
        fit.met = rest_pose_clear(fit.gaps, targets);
        return fit; // the "before" arm is measured, never corrected
    }
    const auto rebuild = [&](const RestStance& s) {
        stance = s;
        fit.rig = Rig::build(p, stance);
        fit.binding = bind_skinned_rig(fit.rig, skeleton);
        measure();
        ++fit.passes;
    };
    // TWO ANGLES, SOLVED ONE AFTER THE OTHER, LEGS FIRST. The leg splay moves
    // the thighs OUTWARD, toward the hanging hands, so every degree of splay
    // costs the hand-thigh gap 2 mm on this body: an arm solved before the
    // legs is an arm solved against a thigh that is about to move. The
    // abduction does not move the legs, so the order closes the coupling in
    // one direction and needs no second round.
    //
    // EACH ANGLE: THE LEVER UPWARD UNTIL ITS PAIRS CLEAR, THEN TIGHTENED by
    // bisection between the last stance that fell short and the first that
    // cleared, keeping the clearing side — «по швам» means the smallest angle
    // that clears, not the first one that does. The lever overshoots on
    // purpose (it reads a deficit at a limb that is INSIDE the other one),
    // and the tightening is what turns the overshoot into a measurement.
    struct Axis {
        bool (*clear)(const BodyGaps&, const BodyGapTargets&);
        float RestStance::*angle;
    };
    const Axis axes[] = {{&legs_clear, &RestStance::leg_splay_rad},
                         {&arms_clear, &RestStance::arm_abduction_rad}};
    for (const Axis& axis : axes) {
        RestStance unmet = stance;
        bool cleared = axis.clear(fit.gaps, targets);
        for (uint32_t pass = 0; pass < REST_FIT_MAX_PASSES && !cleared; ++pass) {
            unmet = stance;
            const Joints j = rest_joints(fit.rig, skeleton, fit.binding);
            if (!j.valid) {
                fit.met = false;
                return fit;
            }
            float turn = 0.0f;
            if (axis.angle == &RestStance::leg_splay_rad) {
                // THE LEGS: half the deficit per leg, on the hip->band lever.
                const float leg_len = 0.5f * (glm::length(j.ankle[0] - j.hip[0])
                                              + glm::length(j.ankle[1] - j.hip[1]));
                const float arm = band_lever(0.5f * (j.hip[0].y + j.hip[1].y),
                                             fit.gaps.legs.worst_y, leg_len);
                const float legs_short =
                    targets.legs_m + OVERSHOOT_M - fit.gaps.legs.judged_m();
                turn = legs_short > 0.0f ? lever(0.5f * legs_short, arm) : 0.0f;
            } else {
                // THE ARMS: whichever of the two pairs is shorter of its target
                // asks for the turn; each on the shoulder->band lever.
                const float arm_len = 0.5f * (glm::length(j.wrist[0] - j.shoulder[0])
                                              + glm::length(j.wrist[1] - j.shoulder[1]));
                const float shoulder_y = 0.5f * (j.shoulder[0].y + j.shoulder[1].y);
                for (int side = 0; side < 2; ++side) {
                    const MeshGap& h = fit.gaps.hand_thigh[static_cast<std::size_t>(side)];
                    const MeshGap& f = fit.gaps.forearm_trunk[static_cast<std::size_t>(side)];
                    const float hand_short = targets.hand_thigh_m + OVERSHOOT_M - h.judged_m();
                    const float fore_short =
                        targets.forearm_trunk_m + OVERSHOOT_M - f.judged_m();
                    if (hand_short > 0.0f) {
                        turn = std::max(turn, lever(hand_short,
                                                    band_lever(shoulder_y, h.worst_y, arm_len)));
                    }
                    if (fore_short > 0.0f) {
                        turn = std::max(turn, lever(fore_short, band_lever(shoulder_y, f.worst_y,
                                                                           0.6f * arm_len)));
                    }
                }
            }
            if (turn <= 0.0f) {
                turn = BOX_STEP_RAD; // the flesh clears, only the boxes touch
            }
            RestStance next = stance;
            next.*(axis.angle) += turn;
            rebuild(next);
            cleared = axis.clear(fit.gaps, targets);
        }
        if (!cleared) {
            fit.met = false;
            return fit; // the passes ran out: report the stance it reached
        }
        RestStance met = stance;
        if (met.*(axis.angle) > unmet.*(axis.angle)) {
            for (int step = 0; step < TIGHTEN_STEPS; ++step) {
                RestStance mid = met;
                mid.*(axis.angle) = 0.5f * (unmet.*(axis.angle) + met.*(axis.angle));
                rebuild(mid);
                if (axis.clear(fit.gaps, targets)) {
                    met = mid;
                } else {
                    unmet = mid;
                }
            }
            if (stance.*(axis.angle) != met.*(axis.angle)) {
                rebuild(met);
            }
        }
    }
    fit.met = rest_pose_clear(fit.gaps, targets);
    return fit;
}

Rig rest_rig_for(const skel::Skeleton& skeleton,
                 std::span<const platform::SkinnedVertex> skin, bool legacy) {
    return fit_rest_pose(RigProportions::from_config(), skeleton, skin,
                         BodyGapTargets::from_config(), legacy)
        .rig;
}

} // namespace dfn::anim
