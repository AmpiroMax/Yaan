/*
Module: engine/anim
File: engine/anim/sources/Stance.h

Responsibility:
- THE STANCE: what a played clip's pose MEASURES, and the corrective layer that
  brings it to the reference. A bought clip is a stranger's idea of standing —
  this asset's idle folds the elbows to 90 degrees, leans the trunk 15 and
  spreads the feet to 1.4 shoulders — and none of that is expressible as a
  clip-name choice. Both halves live here on purpose: the layer aims at
  numbers, and the numbers are read back by the same function the acceptance
  test uses, so "we asked for 18 degrees" and "the frame has 18 degrees" cannot
  be two different measurements.

Key items:
- StanceMetrics / measure_stance(): the pose as numbers — trunk pitch off the
  plumb (the silhouette angle the reference frames are read at), gaze pitch,
  elbow and knee flexion, where the hands hang against the pelvis, stance
  width against shoulder width, the shoulder-over-hip twist, thigh and arm
  swing.
- StanceLayer / build_stance_layer(): the joints the layer speaks for, resolved
  once per model through the same binding the frame uses.
- StanceDrive: how much of each correction this frame wants (run weight, the
  standing weight the leg half is gated by, the weapon weight).
- apply_stance(): the layer, on a sampled pose, in place.

Dependencies:
- Uses: Rig, SkinnedBody (JointLocal + the retarget), PoseLayers (the arm
  layer it runs beside), core skeleton, generated constants (STANCE_*).
- Used by: ClipPlayer (the frame), tests, tools/quality.

Notes:
- EVERY CORRECTION IS A DIFFERENCE, NEVER A REPLACEMENT. The layer measures
  what the clip did, subtracts the target, and turns the joint by the
  remainder. A layer that WROTE the angle would freeze it: the arm would stop
  swinging, the trunk would stop breathing, and the walk would become a
  slideshow of one pose. This is the same decision apply_arm_relax already
  made for the shoulder and it is made here for six more joints.
- AND THAT IS WHY THE RUN CORRECTIONS ARE GAINS. "The trunk twists 5 degrees
  and wants 13" is not a constant to add: the twist alternates with the stride,
  and adding a constant would list the body permanently to one side. The layer
  multiplies the deviation the clip already has, so the phase is the clip's and
  only the amplitude is ours.
- WHY THE LEG HALF IS GATED ON STANDING STILL. Narrowing the stance and
  straightening the knees moves the feet, and how far a clip's foot travels per
  loop is MEASURED AT LOAD (ClipPlayer's stride curve) with no layer on top.
  Correcting the legs while walking would make every stride scale in the
  library a lie and put the slide back that the last wave took out. Standing,
  there is no stride to lie about.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions and plain data (Rule 8, Rule 30): no ECS, no IO, no clock.
- The targets are NUMBERS rows and not literals here (Rule 35): the layer aims
  at them and the acceptance test judges by them, which is two readers.
*/

#pragma once

#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"

#include <array>
#include <cstdint>
#include <span>

namespace dfn::anim {

/// ONE POSE, AS THE NUMBERS THE REFERENCE FRAMES WERE READ IN. Angles are
/// radians, lengths metres, all in the model's own frame (+X is the
/// character's right, it faces -Z, +Y is up — docs/RIG.md).
struct StanceMetrics {
    /// THE SILHOUETTE ANGLE: how far the pelvis->head line leans off the
    /// plumb, positive = forward. This and not a spine joint's own rotation,
    /// because it is what a profile frame shows and therefore the only trunk
    /// number a reference photograph can be compared against.
    float trunk_pitch_rad = 0.0f;
    /// WHERE THE EYES POINT, positive = down, against the BIND (which stands
    /// level by construction — the importer grounds the rest pose).
    float gaze_pitch_rad = 0.0f;
    /// Elbow and knee flexion, 0 = the limb is straight. [0] = left.
    std::array<float, 2> elbow_rad{0.0f, 0.0f};
    std::array<float, 2> knee_rad{0.0f, 0.0f};
    /// HOW FAR BELOW THE PELVIS THE HAND HANGS, metres, positive = below. The
    /// second half of "the hands are at mid-thigh": spread alone cannot tell a
    /// hanging arm from a guard, because both can be 0.25 m out.
    std::array<float, 2> hand_drop_m{0.0f, 0.0f};
    /// How far sideways the hand sits from the pelvis centre (PoseLayers'
    /// HandSpread, restated here so one call answers the whole pose).
    std::array<float, 2> hand_spread_m{0.0f, 0.0f};
    /// Between the ankles, and between the shoulder joints — the pair the
    /// reference is written in ("0.8-0.9 of shoulder width").
    float stance_width_m = 0.0f;
    float shoulder_width_m = 0.0f;
    /// The shoulder line's yaw against the hip line's, positive = the right
    /// shoulder has come forward.
    float shoulder_twist_rad = 0.0f;
    /// Each thigh's and each upper arm's fore-aft pitch, positive = forward.
    /// The swing amplitude a cycle is judged by is the spread of these over
    /// the phase grid, which is the caller's business, not one pose's.
    std::array<float, 2> thigh_pitch_rad{0.0f, 0.0f};
    std::array<float, 2> arm_pitch_rad{0.0f, 0.0f};
    bool valid = false;

    /// The stance in shoulders, the unit the reference states it in. Zero
    /// when the shoulders could not be measured, which `valid` already says.
    [[nodiscard]] float stance_in_shoulders() const {
        return shoulder_width_m > 1.0e-4f ? stance_width_m / shoulder_width_m : 0.0f;
    }
    /// The fore-aft split of the legs and of the arms in ONE pose — what the
    /// eye reads as "mid-stride" — as the difference between the two sides.
    [[nodiscard]] float thigh_split_rad() const {
        return thigh_pitch_rad[0] - thigh_pitch_rad[1];
    }
    [[nodiscard]] float arm_split_rad() const {
        return arm_pitch_rad[0] - arm_pitch_rad[1];
    }
};

[[nodiscard]] StanceMetrics measure_stance(const skel::Skeleton& skeleton,
                                           const SkinnedRigBinding& binding,
                                           std::span<const JointLocal> sample);

/// THE JOINTS THE LAYER SPEAKS FOR, resolved once per model. Plain indices and
/// nothing solved: unlike the arm layer, every target here is an ANGLE the
/// reference states directly, so there is nothing to calibrate against the
/// asset — the measurement happens per frame, on the pose that is actually
/// about to be drawn.
struct StanceLayer {
    int32_t pelvis = -1;
    int32_t torso = -1;
    int32_t head = -1;
    std::array<int32_t, 2> upper_arm{-1, -1};
    std::array<int32_t, 2> forearm{-1, -1};
    std::array<int32_t, 2> hand{-1, -1};
    std::array<int32_t, 2> thigh{-1, -1};
    std::array<int32_t, 2> shin{-1, -1};
    std::array<int32_t, 2> foot{-1, -1};
    [[nodiscard]] bool valid() const {
        return pelvis >= 0 && torso >= 0 && head >= 0 && thigh[0] >= 0 && thigh[1] >= 0;
    }
};

[[nodiscard]] StanceLayer build_stance_layer(const skel::Skeleton& skeleton,
                                             const SkinnedRigBinding& binding);

/// HOW MUCH OF THE LAYER THIS FRAME WANTS.
struct StanceDrive {
    /// 0 = walking or standing, 1 = at a full run. Moves the trunk's target
    /// lean, the elbow's target flexion and the twist gain together, because
    /// on a reference run they move together.
    float run_weight = 0.0f;
    /// 1 = standing still. THE LEG HALF'S GATE, and the header says why: the
    /// stride curve was measured without this layer.
    float stand_weight = 0.0f;
    /// HOW MUCH TO MULTIPLY THE TWIST AND THE ARM SWING THE CLIP ALREADY HAS.
    /// 1 = leave the clip alone. These are gains rather than angles for the
    /// reason the header gives, and they are the CALLER's to compute, because
    /// the peak the gain is measured against is a property of the clip being
    /// played and this file has never seen a clip.
    float twist_gain = 1.0f;
    float arm_swing_gain = 1.0f;
    /// The whole layer's master weight, so a body can be drawn with the layer
    /// off for a before/after arm (Rule 47) without a second code path.
    float weight = 1.0f;
};

/// The layer, in place. `sample` is the imported skeleton's local TRS, as
/// playback_sample builds it. A `weight` of 0 is a bit-for-bit no-op.
void apply_stance(const skel::Skeleton& skeleton, const StanceLayer& layer,
                  const StanceDrive& drive, std::span<JointLocal> sample);

} // namespace dfn::anim
