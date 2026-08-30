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
- ArmRelax / calibrate_arm_relax(): the adduction angle that puts this model's
  hand where OUR REST POSE puts it, solved once per model against the asset's
  own idle. Plus the finger joints, which this asset keys into a fist.
- apply_arm_relax(): the layer, weighted 0..1, on a sampled pose.
- measure_hand_spread(): the number the acceptance is written in — how far the
  hand sits sideways from the pelvis centre, metres.

Dependencies:
- Uses: Rig, Pose, SkinnedBody (JointLocal + the retarget), core skeleton.
- Used by: ClipPlayer (the frame), engine/app, tests.

Notes:
- WHY A MASK BY BRANCH AND NOT BY A LIST OF BONE NAMES. A name list is written
  against one asset and is silently wrong on the next one: this model calls its
  chest DEF-spine.003 and a Skyrim NPC calls it NPC Spine2. "Everything hanging
  off the joint our TORSO bone bound to" is the same sentence for both, and it
  cannot go stale, because the binding is what already had to be right for the
  body to be drawn at all.
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

#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"

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
    int32_t pelvis = -1;
    /// The solved adduction, radians, positive = inward. One angle for both
    /// sides: an asymmetric relax would make the asset's own asymmetry
    /// (this one swings its left arm wider) into a permanent lean.
    float angle_rad = 0.0f;
    /// WHERE OUR REST POSE PUTS THE HAND, metres sideways from the pelvis
    /// centre — the target the angle was solved against, kept so a report can
    /// print what was aimed at next to what was hit.
    float target_m = 0.0f;
    /// What the reference pose measured BEFORE the layer, same units.
    float reference_m = 0.0f;
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

/// Applies the layer to `sample` in place at `weight` in [0,1]: the arms come
/// in by `weight * angle_rad`, the fingers relax `weight` of the way back to
/// their BIND rotation. Weight 0 is a bit-for-bit no-op — the state "weapon
/// drawn" depends on that being true.
void apply_arm_relax(const skel::Skeleton& skeleton, const ArmRelax& relax, float weight,
                     std::span<JointLocal> sample);

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

/// HOW CLOSED THE HAND IS: the mean distance from the fingertip joints to the
/// hand joint, metres. An open hand measures further than a fist, so the
/// finger half of the layer has a number too instead of an assertion.
[[nodiscard]] float measure_hand_openness(const skel::Skeleton& skeleton,
                                          const ArmRelax& relax,
                                          std::span<const JointLocal> sample);

} // namespace dfn::anim
