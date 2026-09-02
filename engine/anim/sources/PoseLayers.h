/*
Module: engine/anim
File: engine/anim/sources/PoseLayers.h

Responsibility:
- LAYERS AND MASKS OVER A PLAYED CLIP. Two clips are rarely the answer to one
  frame: the legs walk while the arms hold a sword, and a bought "idle" is a
  fighter's idle whose hands the game has to bring back down. This file owns
  the two mechanisms that make either sentence expressible — a SKELETON-BRANCH
  MASK (which joints a source speaks for) and a CALIBRATED ARM LAYER (how far
  the shoulders come in, measured rather than guessed).

Key items:
- Branch / BranchMask / build_branch_mask(): every imported joint labelled
  Lower (pelvis-down), Upper (spine-up, arms, head, fingers) or Root.
- blend_masked(): one pose from two, the upper half weighted.
- ArmRelax / calibrate_arm_relax(): the SHOULDER-AND-ELBOW pair that puts this
  model's hand where the reference puts it — adduction and lift solved against
  two targets (how far out, how far down), the elbow solved against the
  reference's own flexion. Plus the finger joints, which this asset keys into
  a fist.
- ArmRelaxDose / apply_arm_relax(): the layer on a sampled pose, with the
  shoulder, the elbow and the fingers dosed separately.
- measure_hand_spread(): the number the acceptance is written in — how far the
  hand sits sideways from the pelvis centre, metres.

Dependencies:
- Uses: Rig, Pose, SkinnedBody (JointLocal + the retarget), Stance (the pose
  measurement the calibration solves against), core skeleton.
- Used by: ClipPlayer (the frame), engine/app, tests.

Notes:
- WHY A MASK BY BRANCH AND NOT BY A LIST OF BONE NAMES. A name list is written
  against one asset and is silently wrong on the next one: this model calls its
  chest DEF-spine.003 and a Skyrim NPC calls it NPC Spine2. "Everything hanging
  off the joint our TORSO bone bound to" is the same sentence for both, and it
  cannot go stale, because the binding is what already had to be right for the
  body to be drawn at all.
- WHY THE ELBOW IS AN OFFSET AND THE SHOULDER IS TWO ANGLES. One adduction
  aimed at one sideways target was enough to state "the arms are not held out"
  and could not state anything else: measured on this asset it left the hands
  0.20 m ABOVE the pelvis line with the elbows folded at 90 degrees, which is
  a boxer, correctly adducted. Two targets need two degrees of freedom, and
  the fold needs the joint that folds.
- WHY THE ARM ANGLE IS SOLVED AND NOT AUTHORED. The order named "about 10-12
  degrees inward". On THIS asset 10 degrees is not enough and 12 is not either:
  the hand starts 0.403 m from the pelvis centre and our rest pose puts it at
  0.230, and the arm is 0.55 m long, so the angle that closes that gap is a
  property of the model's proportions, not a constant. Solving it against a
  MEASURED target means the layer stays right on a model with longer arms —
  and means the acceptance number ("the hand is 0.23..0.28 m out") is the thing
  the code is aiming at rather than a coincidence downstream of an angle.
- THE LAYER IS NOT A SECOND POSE. It PRE-MULTIPLIES the upper arm's own local
  rotation in the PARENT's frame, so whatever the clip was doing with the arm
  keeps happening — swung, raised, or reaching — with the whole arm carried
  inward. Replacing the rotation instead would freeze the arm swing, which is
  the defect the mask exists to avoid on the other side.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions and plain data (Rule 8, Rule 30): no ECS, no IO, no clock.
*/

#pragma once

#include "engine/anim/sources/Hitbox.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace dfn::anim {

/// WHICH HALF OF THE BODY A JOINT BELONGS TO.
///
/// Root is not a third half: it is the pelvis and whatever sits above it in
/// the imported hierarchy (the armature node), and it always follows the LOWER
/// source, because the root joint's translation is where the body stands and a
/// body cannot stand in two places.
enum class Branch : uint8_t {
    Root = 0,
    Lower = 1, ///< thighs, shins, feet, toes
    Upper = 2, ///< spine, chest, neck, head, shoulders, arms, fingers
};

struct BranchMask {
    std::vector<uint8_t> branch;
    [[nodiscard]] bool valid() const { return !branch.empty(); }
    [[nodiscard]] Branch at(std::size_t j) const {
        return j < branch.size() ? static_cast<Branch>(branch[j]) : Branch::Root;
    }
    [[nodiscard]] bool is_upper(std::size_t j) const {
        return at(j) == Branch::Upper;
    }
    /// How many joints each label caught. A mask with zero uppers is a mask
    /// that would silently make every layered state look like the lower one.
    [[nodiscard]] uint32_t count(Branch b) const;
};

/// Labels every joint of the imported skeleton by DESCENT from the joints our
/// rig's Torso and Thigh bones bound to. Joints below neither (a prop bone, a
/// cape) stay Root and follow the lower source, which is the conservative
/// answer: a bone nobody claimed keeps doing what the locomotion clip says.
[[nodiscard]] BranchMask build_branch_mask(const skel::Skeleton& skeleton,
                                           const SkinnedRigBinding& binding);

/// out[j] = lower[j], except on Upper joints where it is lower blended toward
/// upper by `upper_weight`. Sizes must all be >= skeleton.size(); `out` may
/// alias `lower`.
void blend_masked(std::span<const JointLocal> lower, std::span<const JointLocal> upper,
                  const BranchMask& mask, float upper_weight,
                  std::span<JointLocal> out);

/// THE ARM LAYER, calibrated once per model.
struct ArmRelax {
    /// The joint the adduction is applied to (our UpperArm bone's joint) and
    /// its parent, per side. [0] = left, [1] = right.
    std::array<int32_t, 2> upper_arm{-1, -1};
    std::array<int32_t, 2> parent{-1, -1};
    std::array<int32_t, 2> hand{-1, -1};
    /// THE ELBOW, and it is the joint the layer was missing. A shoulder alone
    /// can carry a folded arm inward and it stays folded: this asset's idle
    /// holds 85-95 degrees of elbow where the reference holds 15-20, and no
    /// adduction angle can unfold it.
    std::array<int32_t, 2> forearm{-1, -1};
    int32_t pelvis = -1;
    /// The solved adduction, radians, positive = inward. One angle for both
    /// sides: an asymmetric relax would make the asset's own asymmetry
    /// (this one swings its left arm wider) into a permanent lean.
    float angle_rad = 0.0f;
    /// The solved shoulder LIFT, radians, positive = the arm carried forward.
    /// The second degree of freedom the second target needed: adduction moves
    /// the hand sideways and cannot move it down, and "the hands hang at
    /// mid-thigh" is a claim about height.
    float lift_rad = 0.0f;
    /// HOW MUCH ELBOW FLEXION THE LAYER ADDS (radians, negative = unfolds),
    /// solved so that the reference pose lands on the target elbow — OUR REST
    /// POSE'S standing (REST_ELBOW_FLEX through the retarget), STANCE_ELBOW_RUN
    /// at a full run.
    ///
    /// AN OFFSET AND NOT AN ANGLE, deliberately, and it is the same decision
    /// the adduction makes: writing the elbow every frame would pin it, and a
    /// pinned elbow is an arm that has stopped swinging. The clip's own elbow
    /// motion survives underneath, shifted to the reference's neighbourhood.
    ///
    /// THESE TWO ARE THE CALIBRATION'S OWN, against the pose the layer was
    /// solved on (the asset's idle). What the FRAME uses is per clip and
    /// arrives through ArmRelaxDose — see there for the 49 degrees this
    /// distinction is worth.
    float elbow_stand_rad = 0.0f;
    float elbow_run_rad = 0.0f;
    /// WHERE OUR REST POSE PUTS THE HAND, metres sideways from the pelvis
    /// centre — the target the angle was solved against, kept so a report can
    /// print what was aimed at next to what was hit.
    float target_m = 0.0f;
    /// What the reference pose measured BEFORE the layer, same units.
    float reference_m = 0.0f;
    /// The same pair for the HEIGHT target (the rest pose's own hand drop):
    /// how far below the pelvis line the hand is asked to hang, and where it
    /// hung before.
    float target_drop_m = 0.0f;
    float reference_drop_m = 0.0f;
    /// The elbow the reference pose came in at, kept for the same reason.
    float reference_elbow_rad = 0.0f;
    /// The finger joints, i.e. everything descending from the hands. This
    /// asset keys them into a closed fist in every clip including its idle,
    /// and a fist is a held weapon the model does not have.
    std::vector<int32_t> finger;
    [[nodiscard]] bool valid() const {
        return upper_arm[0] >= 0 && upper_arm[1] >= 0 && pelvis >= 0;
    }
};

/// Solves the adduction against `reference` (the pose the layer has to fix —
/// the asset's own idle sample) so that the hand lands where OUR REST POSE
/// puts it. The target is read out of the rig through the same retarget the
/// frame uses, so it is a measurement of this model and not a row anywhere.
[[nodiscard]] ArmRelax calibrate_arm_relax(const Rig& rig, const skel::Skeleton& skeleton,
                                           const SkinnedRigBinding& binding,
                                           std::span<const JointLocal> reference);

/// HOW MUCH OF EACH HALF OF THE LAYER THIS FRAME WANTS.
///
/// FOUR DIALS AND NOT ONE WEIGHT, because the weapon branch needs them to say
/// different things at the same instant: a drawn sword leaves the SHOULDER
/// half-relaxed (the guard clip's angles are drawn for another body, and our
/// proportions still need their calibration — STANCE_WEAPON_ARM_RELAX) while
/// the FINGERS must stay in the clip's keyed fist, because that fist is what
/// is holding the blade. One number could not have said both, and the version
/// that tried gassed the whole layer to zero and flung the arms 0.53 m out.
struct ArmRelaxDose {
    float arm = 1.0f;    ///< the shoulder: adduction + lift
    /// HOW MUCH FLEXION TO ADD AT THE ELBOW, radians, negative = unfold.
    ///
    /// AN ABSOLUTE OFFSET PASSED IN, AND NOT A DIAL ON A STORED PAIR, because
    /// the offset is a property of the CLIP being played and this struct's
    /// owner is the only one who knows which clip that is. Solved against the
    /// idle and reused on the sprint, it overshot by 49 degrees: the idle
    /// holds 38 and needs +45 to reach the reference's standing elbow, while
    /// the sprint already holds 87 and needs nothing at all.
    float elbow_offset_rad = 0.0f;
    float finger = 1.0f; ///< how far the fist opens back toward the bind
};

/// Applies the layer to `sample` in place: the elbow unfolds toward the
/// reference's, the arms come in and down by `dose.arm * (angle_rad,
/// lift_rad)`, the fingers relax `dose.finger` of the way back to their BIND
/// rotation. An all-zero dose is a bit-for-bit no-op.
void apply_arm_relax(const skel::Skeleton& skeleton, const ArmRelax& relax,
                     const ArmRelaxDose& dose, std::span<JointLocal> sample);

/// HOW FAR THE HANDS SIT SIDEWAYS FROM THE PELVIS CENTRE, metres, in the
/// model's own frame (+X is the character's right, docs/RIG.md). The number
/// item 3 of the owner's list is written in.
struct HandSpread {
    float left = 0.0f;
    float right = 0.0f;
    [[nodiscard]] float worst() const { return left > right ? left : right; }
};
[[nodiscard]] HandSpread measure_hand_spread(const skel::Skeleton& skeleton,
                                             const SkinnedRigBinding& binding,
                                             std::span<const JointLocal> sample);

/// ОБХОД ТЕЛА РУКОЙ (заказ владельца 31.08, пункт 1: «руки проходят сквозь
/// ягодицы при ходьбе/беге»).
///
/// ПОЧЕМУ ЭТО ОТДЕЛЬНЫЙ СЛОЙ, А НЕ ПОПРАВКА К ПРИВЕДЕНИЮ. Приведение решается
/// ОДИН РАЗ на позе покоя и держит руку у тела всю дорогу — в этом его работа.
/// Проход сквозь таз случается в ФАЗЕ МАХА, когда рука уходит назад и таз
/// оказывается у неё на пути; величина, которую надо снять, зависит от фазы, а
/// значит не может жить в калиброванном угле. Уменьшить приведение «на всякий
/// случай» — значит развести руки на всём цикле ради двух его кадров.
///
/// МЕРИТСЯ ДО ХИТБОКСОВ, А НЕ ДО КОСТИ. «Насколько кисть близко к ягодице» —
/// вопрос об ОБЪЁМЕ таза, и объём этот в проекте уже описан один раз
/// (Hitbox.h, выведен из пропорций рига). Расстояние до сустава таза было бы
/// вторым описанием тела и разошлось бы с первым на первой же правке
/// пропорций (правило 35).
struct ArmClearance {
    std::array<int32_t, 2> upper_arm{-1, -1};
    std::array<int32_t, 2> parent{-1, -1};
    std::array<int32_t, 2> forearm{-1, -1};
    std::array<int32_t, 2> hand{-1, -1};
    /// Длина руки от плеча до кисти в позе покоя, метры: она переводит
    /// «не хватает стольких сантиметров» в «повернуть на столько радиан».
    std::array<float, 2> arm_len{0.0f, 0.0f};
    [[nodiscard]] bool valid() const {
        return upper_arm[0] >= 0 && upper_arm[1] >= 0 && hand[0] >= 0 && hand[1] >= 0;
    }
};

[[nodiscard]] ArmClearance build_arm_clearance(const skel::Skeleton& skeleton,
                                               const SkinnedRigBinding& binding);

/// НАСКОЛЬКО БЛИЗКО РУКА ПОДОШЛА К ТЕЛУ, метры: минимум по кисти и предплечью
/// обеих рук против форм таза и обоих бёдер. Число, которым написана приёмка
/// пункта 1.
struct ArmBodyGap {
    std::array<float, 2> hand{0.0f, 0.0f};    ///< [0] левая
    std::array<float, 2> forearm{0.0f, 0.0f};
    [[nodiscard]] float worst() const {
        return std::min({hand[0], hand[1], forearm[0], forearm[1]});
    }
};

[[nodiscard]] ArmBodyGap measure_arm_body_gap(const skel::Skeleton& skeleton,
                                              const ArmClearance& arms,
                                              const HitboxSet& boxes,
                                              const SkinnedRigBinding& binding,
                                              std::span<const JointLocal> sample);

/// ОТВОДИТ ПЛЕЧО НАРУЖУ РОВНО НАСТОЛЬКО, ЧТОБЫ КЛИРЕНС ВЕРНУЛСЯ К `want_m`.
/// Ничего не делает на руке, у которой клиренс и так есть — то есть на
/// подавляющем большинстве кадров это побитовое тождество, и «до/после» на
/// стоящей фигуре обязано совпасть до бита.
///
/// УГОЛ РЕШАЕТСЯ, А НЕ СКАНИРУЕТСЯ: нехватка клиренса — это ДЛИНА, плечо —
/// РЫЧАГ известной длины, и частное одно делит другое. Скан, как у калибровки
/// приведения, стоил бы шестьдесят четыре прохода FK на кадр там, где хватает
/// одного деления и одной проверки.
///
/// `dose` в [0,1] масштабирует весь слой; 0 — побитовое тождество, и на этом
/// стоит контрольная рука приёмки.
void apply_arm_clearance(const skel::Skeleton& skeleton, const ArmClearance& arms,
                         const HitboxSet& boxes, const SkinnedRigBinding& binding,
                         float want_m, float dose, std::span<JointLocal> sample);

/// HOW CLOSED THE HAND IS: the mean distance from the fingertip joints to the
/// hand joint, metres. An open hand measures further than a fist, so the
/// finger half of the layer has a number too instead of an assertion.
[[nodiscard]] float measure_hand_openness(const skel::Skeleton& skeleton,
                                          const ArmRelax& relax,
                                          std::span<const JointLocal> sample);

} // namespace dfn::anim
