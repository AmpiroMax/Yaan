/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 21:34:24
Module: engine/anim
File: engine/anim/sources/Clips.cpp

Responsibility:
- Procedural clip implementations. Shape values here are procedural ASSET data
  (lead ruling 10:08:2026) — each carries its gait-literature rationale at the
  definition. Phases are NUMBERS rows; nothing here invents a phase.

Dependencies:
- Uses: Clips.h, generated Constants.h (FOOTFALL_PHASE_*), glm.
- Used by: Body.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The foot-plant/bob minima MUST stay on FOOTFALL_PHASE_LEFT/RIGHT; ClipTests
  enforce it against the generated names with an offset control (Rule 30).
*/
/*
UPD:
- 10:08:2026 - 01:56:45: Initial implementation (gait keyed to sim's phases).
- 10:08:2026 - 12:10:00: The stance knee no longer hyperextends (was 33.4 deg) and the foot rolls over the toe instead - the forefoot rocker, 22.4 deg at full swing.
- 10:08:2026 - 20:00:23: The wave's wag moved off the ELBOW (a hinge deleted it, so the wave never waved); flex's forearm rolls likewise; gait_run_weight authored per gear instead of interpolated across the rows.
- 10:08:2026 - 20:22:44: eye_lean_offset() — the eye rides the trunk's lean (sim's request; both zones derived it independently and agree to the millimetre); the head counter-pitch is named HEAD_STABILIZE now that it has a second reader.
- 10:08:2026 - 21:34:24: THIGH_SWING_MAX_SIN now READS its NUMBERS row. The row landed 19:26:40 and this file kept a private 0.55, so the row had zero readers in the engine and zero in the suite — a row that guards nothing while looking like it guards something.
*/

#include "engine/anim/sources/Clips.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>

namespace dfn::anim {

namespace {

constexpr float TWO_PI = 6.28318530717958647692f;

// --- Procedural asset data (NOT NUMBERS rows — lead ruling, see header) -----
// Gait shape. Rationales cite Winter's gait chapters / Inman's "Human Walking"
// qualitatively; exact values are authored for the chunky low-res read.
// THIS ONE IS A ROW, NOT ASSET DATA, AND IT HAD TO BECOME ONE (Rule 35): the
// cap bounds the visual half-step, and sim measures the RESIDUAL FOOT SLIP
// against the same bound, so two zones must agree on it. The row landed in
// NUMBERS on 10:08:2026 - 19:26:40 and this literal was never switched over —
// the row then sat with ZERO readers in the engine and zero in the suite,
// which is a row that guards nothing while looking like it guards something.
// Found by sweeping this zone for the pattern the repo audit named elsewhere.
// It is stated as a SINE because that is what the pose consumes; an angle
// would be an intermediate somebody rounds separately.
constexpr float THIGH_SWING_MAX_SIN =
    static_cast<float>(config::BODY_THIGH_SWING_MAX_SIN);
    // Uncapped, asin() at sim's brisk walk gives a cartoon scissor, and the
    // cap bounds the derived pelvis arc (below) to T*(1-cos(0.58)) ~ 7 cm.
    // HOW TO PHRASE ANY CHECK ON IT, and the row says the same: "the residual
    // slip stays under a perceptual bound", NEVER "the clamp is inactive". At
    // WALK_SPEED the clamp still binds by 0.798 %, so the mechanism-shaped
    // assertion would go red on correct code the day it was written (Rule 38).
// Fraction of the foot behind the ankle; mirrors BodyMesh FOOT_HEEL_RATIO so
// the toe used by the rocker is the toe that is drawn.
constexpr float FOOT_HEEL_FRAC = 0.25f;
constexpr float SWING_LIFT = 0.6f;        // rad, swing-knee clearance on a
    // sqrt envelope (below): ~7 cm of foot lift mid-swing, and a SHARP
    // arrival — the sqrt makes approach height linear in phase, so the
    // touch-down edge is crisp instead of a soft quadratic kiss.
constexpr float ARM_SWING_RATIO = 0.65f;  // arm swing vs thigh swing (arms trail
    // the legs' energy; full 1.0 reads as marching).
constexpr float ELBOW_BASE = 0.30f;       // rad, natural standing elbow flex.
constexpr float ELBOW_SWING = 0.35f;      // rad, extra flex as the arm swings fwd.
// THE GAIT MODEL ("wheel gait"), chosen after the compass-pendulum model
// measurably failed (swing foot penetrated the ground on approach and the
// plant instant was a soft quadratic kiss no threshold could pin):
//   - SWING: knee = -thigh - SWING_LIFT*sqrt(max(0,cos(2pi*lp))): clearance
//     is S*(1-cos(lift)), strictly positive through swing, EXACTLY zero at
//     both stance endpoints — the foot can never penetrate and touches down
//     exactly at local 0.25.
//   - STANCE: originally the SHIN WAS HELD VERTICAL (knee = -thigh), which
//     grounded the ankle exactly for every stance angle. IT ALSO BENT THE KNEE
//     BACKWARDS by up to 33.4 deg, because holding a shin vertical under a
//     thigh that has swung behind you is not something a leg can do — the user
//     saw it and called it creepy (10:08:2026). The knee is now capped at
//     BODY_KNEE_HYPEREXT_MAX and the foot rolls over the toe instead (the
//     forefoot rocker in leg_angles): the heel lifts, the toe stays down, and
//     the ankle is free to rise. 22.4 deg of toe-off at full swing, against a
//     real 20-25.
//   - CONSEQUENCE, and it is a contract change: release is no longer the
//     instant the other foot plants. Toe-off comes AFTER the other foot is
//     down — that overlap is DOUBLE SUPPORT and it is what walking is. The
//     footfall test asserts it, and it measures the SOLE, because once the
//     heel lifts the ankle stops being a witness to ground contact.
// Heel-strike shin-vertical posture and a knee-absorbed arc are also what
// real gait does (Winter's stance-knee flexion determinant), so this is not
// only the testable model but the more anatomical one.
constexpr float SWAY_M = 0.03f;           // lateral CoM shift onto the stance
    // foot (real ~4 cm; low-res read prefers slightly less).
constexpr float TORSO_ROLL = 0.06f;       // rad, upper-body list toward the
    // stance side. ON THE TORSO, NOT THE PELVIS, deliberately: the thighs are
    // children of the pelvis, so a pelvis roll tilts the whole leg chain and
    // measurably broke the exact stance grounding (caught by the footfall
    // test's penetration check before any frame was shot).
constexpr float TORSO_TWIST = 0.10f;      // rad, counter-rotation vs the pelvis.
constexpr float RUN_LEAN = 0.20f;         // rad, forward trunk lean at full run.
constexpr float HEAD_STABILIZE = 0.6f;    // the head counter-pitches this
    // fraction of the trunk's lean, so the gaze stays nearer the horizon and
    // the skull's NET world pitch is only (1 - this) x lean. NAMED because it
    // has a second reader: eye_lean_offset() has to counter-rotate the eye by
    // the same fraction, and a literal 0.6 in two places is the two-copies
    // defect at file scope (Rule 35's shape, one file down).
constexpr float RUN_ELBOW = 0.80f;        // rad, elbows carried bent at full run.
// Idle breathing.
constexpr float BREATH_PERIOD_S = 4.0f;   // calm breath ~15/min.
constexpr float BREATH_TORSO = 0.015f;    // rad, chest rise read as a slight sway.
constexpr float BREATH_ARM = 0.02f;       // rad, arms drift with the breath.
// Airborne / landing.
constexpr float AIR_THIGH = 0.5f;         // rad, tuck.
constexpr float AIR_KNEE = 0.9f;          // rad.
constexpr float AIR_ARM_OUT = 0.35f;      // rad, arms out for balance.
constexpr float AIR_LEAN_PER_MPS = 0.02f; // rad per m/s of vertical velocity.
constexpr float LAND_KNEE = 0.7f;         // rad at full dip.
constexpr float LAND_PELVIS_DROP = 0.12f; // m at full dip.
// Showcase.
constexpr float WAVE_RAISE = 2.4f;        // rad, right arm up.
constexpr float WAVE_AMP = 0.5f;          // rad, the wag.
constexpr float WAVE_HZ = 1.8f;
constexpr float WAVE_ELBOW = 1.40f;       // rad, ~80 deg. A wave is given with
    // the elbow BENT — see the note in wave_pose(); the old 0.3 rad held the
    // arm nearly straight, which is a salute rather than a greeting.
constexpr float FLEX_RAISE = 1.6f;        // rad, both arms out to the sides.
constexpr float FLEX_SPLAY = 0.3f;        // rad, humeral rotation that turns
    // the elbows outward. ON THE UPPER ARM'S OWN LONG AXIS, which is the joint
    // that actually does it; it used to be a roll on the FOREARM, i.e. on the
    // elbow, and see wave_pose() for what happened to it.
constexpr float FLEX_CURL = 1.9f;         // rad, biceps curl.
constexpr float FLEX_PUMP = 0.15f;        // rad, slow pump on top of the curl.
constexpr float FLEX_HZ = 0.5f;

[[nodiscard]] glm::quat pitch(float a) { // about local X: + swings a -Y limb forward (-Z)
    return glm::angleAxis(a, glm::vec3{1.0f, 0.0f, 0.0f});
}
[[nodiscard]] glm::quat yaw_q(float a) { // about local Y
    return glm::angleAxis(a, glm::vec3{0.0f, 1.0f, 0.0f});
}
[[nodiscard]] glm::quat roll(float a) { // about local Z: + moves a -Y limb toward +X
    return glm::angleAxis(a, glm::vec3{0.0f, 0.0f, 1.0f});
}

struct LegAngles {
    float thigh = 0.0f;
    float knee = 0.0f; // negative = flexion (shin swings back)
    float foot = 0.0f;
};

// One leg's wheel-gait curves, phase-local: THIS leg plants at local phase
// 0.25 and releases at 0.75 (call with p for the left leg, p + 0.5 for the
// right — the left/right assignment itself comes from the FOOTFALL_PHASE_*
// rows in gait_pose()). amp_ratio scales the swing lift so a standstill has
// straight legs (asserted by ClipTests).
[[nodiscard]] LegAngles leg_angles(const RigProportions& p, float lp, float thigh_amp,
                                  float amp_ratio, float pelvis_dy) {
    LegAngles a;
    const float s = std::sin(TWO_PI * lp);
    a.thigh = thigh_amp * s; // forward-max exactly at local 0.25 (the plant)
    // Knee = -thigh WOULD hold the shin vertical, which is how this clip kept
    // the planted foot down — and it is why the knees bent BACKWARDS (user,
    // 10:08:2026: «не должны выгибаться обратно»). Holding a shin vertical
    // while the thigh swings back is anatomically impossible: it opens the knee
    // by exactly the thigh's swing, measured at 33.4 deg. The sqrt envelope
    // lifts the swing foot and is exactly zero at both stance endpoints.
    const float env = std::max(0.0f, std::cos(TWO_PI * lp));
    const float want = -a.thigh - SWING_LIFT * amp_ratio * std::sqrt(env);
    // Flexion is NEGATIVE for a knee, so hyperextension is the UPPER bound.
    // The rig clamps this too; this is the clip being honest rather than being
    // corrected, so that the rig's clamp stays a guarantee and not a crutch.
    a.knee = std::min(want, static_cast<float>(config::BODY_KNEE_HYPEREXT_MAX));
    a.foot = -(a.thigh + a.knee); // flat: cancels the chain above it

    // THE FOREFOOT ROCKER, and it is only meaningful in STANCE (env == 0 there
    // by construction, which is the same test the swing lift already uses).
    // Refusing the hyperextension straightens the stance leg, which lifts the
    // ankle; a real leg answers by lifting the HEEL and rolling over the toe.
    // Pitch the foot until the toe is back on the ground: solve
    // ankle_h*cos(phi) + toe*sin(phi) = ankle_y. At full swing this asks for
    // 22.4 deg and real toe-off is 20-25, which is the check that the model is
    // right rather than merely fitted.
    if (env <= 0.0f) {
        // The leg is also tilted INWARD by the stance convergence, which
        // shortens its vertical reach by cos(theta). standing_hip_height()
        // already pays for that at rest, so leaving it out here double-counted
        // it and left the toe hovering 7 mm — sub-pixel in a frame, and caught
        // only because the contact test measures the sole to a millimetre.
        const float lean = std::cos(p.leg_convergence());
        const float ankle_y = p.standing_hip_height() + pelvis_dy
                            - lean * (p.thigh_length() * std::cos(a.thigh)
                                      + p.shin_length() * std::cos(a.thigh + a.knee));
        const float toe = p.foot_length * (1.0f - FOOT_HEEL_FRAC);
        const float reach = std::sqrt(p.ankle_height * p.ankle_height + toe * toe);
        if (ankle_y > p.ankle_height && reach > 1.0e-4f) {
            const float ratio = std::clamp(ankle_y / reach, -1.0f, 1.0f);
            const float phi = std::asin(ratio) - std::atan2(p.ankle_height, toe);
            a.foot -= std::max(0.0f, phi); // heel up, toe down
        }
    }
    return a;
}

} // namespace

glm::vec2 eye_lean_offset(const RigProportions& p, float run_weight) {
    // THE EYE RIDES THE LEAN. Requested by sim, derived independently on both
    // sides, and the two derivations agree to the millimetre.
    //
    // WHY IT IS A FUNCTION HERE AND NOT A NUMBERS ROW (sim's argument, and it
    // is `BodyDrive::gait` run backwards): if sim re-derived the lean from the
    // gait, their side would hold a second copy of gait_run_weight's AUTHORED
    // table and of RUN_LEAN, and an authored number with two copies drifts the
    // day it is re-authored. So: the LEAN CHARACTER CHOSE, not the gait it was
    // derived from. One producer, one consumer, no registry row — a row would
    // still be two readers.
    //
    // THE MODEL IS TWO ROTATIONS, because BONE_PARENT puts Head under Torso:
    //   1. the torso pitches -theta about the HIP, carrying the neck through
    //      an arc of (neck_height - hip_height);
    //   2. the head counter-pitches +HEAD_STABILIZE*theta, so the skull's NET
    //      world pitch is -(1 - HEAD_STABILIZE)*theta, and the eye — which
    //      sits in the skull, `PLAYER_EYE_HEIGHT` up and `PLAYER_EYE_FORWARD`
    //      forward — swings by that smaller angle about the neck.
    // Taking only step 1 would put the eye 0.1482 m forward at full run
    // instead of 0.1320: the counter-pitch is 11% of the answer.
    const float theta = RUN_LEAN * std::clamp(run_weight, 0.0f, 1.0f);
    const auto eye_height = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const auto eye_forward = static_cast<float>(config::PLAYER_EYE_FORWARD);
    const float neck_up = p.neck_height - p.hip_height;
    const float eye_above_neck = eye_height - p.neck_height;

    const float neck_fwd = neck_up * std::sin(theta);
    const float neck_drop = neck_up * (1.0f - std::cos(theta));

    const float a = (1.0f - HEAD_STABILIZE) * theta;
    const float eye_fwd_local = eye_above_neck * std::sin(a) + eye_forward * std::cos(a);
    const float eye_up_local = eye_above_neck * std::cos(a) - eye_forward * std::sin(a);

    return {neck_fwd + eye_fwd_local - eye_forward,
            neck_drop + (eye_above_neck - eye_up_local)};
}

float gait_run_weight(Gait gait) {
    switch (gait) {
    case Gait::Walk:
        return 0.0f;
    case Gait::Jog:
        // AUTHORED, and it is an admission as much as a value: there is no jog
        // clip yet (a real one needs a FLIGHT PHASE — sim's own header says so
        // — and that is a stage of work, not a constant). Until it exists, jog
        // is rendered as half of the run layer, and the half is not a
        // midpoint: the run layer's two visible markers are the trunk lean
        // RUN_LEAN 0.20 rad and the carried elbows RUN_ELBOW 0.80, and half of
        // that lean is 0.10 rad = 5.7 deg, which lands inside the measured
        // jogging trunk lean of 5-8 deg (sprinting is 11-15). The model
        // arriving at a number it was not fitted to is the same check the
        // stance row and the 22.4 deg toe-off passed.
        //
        // WHY THIS IS NOT THE 0.286 UNDER A NEW NAME: 0.286 was nobody's
        // decision — it was where a straight line happened to pass. This is a
        // choice with a reason attached, it is on the record, and when the jog
        // clip lands this function stops returning a weight for Jog at all.
        return 0.5f;
    case Gait::Run:
        return 1.0f;
    }
    return 0.0f;
}

LocalPose idle_pose(float time_s) {
    LocalPose p;
    const float b = std::sin(TWO_PI * time_s / BREATH_PERIOD_S);
    p.rotation[bone_index(Bone::Torso)] = pitch(-BREATH_TORSO * b);
    p.rotation[bone_index(Bone::UpperArmL)] = roll(-BREATH_ARM * b);
    p.rotation[bone_index(Bone::UpperArmR)] = roll(BREATH_ARM * b);
    p.rotation[bone_index(Bone::ForearmL)] = pitch(ELBOW_BASE * 0.5f);
    p.rotation[bone_index(Bone::ForearmR)] = pitch(ELBOW_BASE * 0.5f);
    return p;
}

LocalPose gait_pose(const Rig& rig, float phase, float step_length_m, float run_weight) {
    const auto& pr = rig.proportions;
    LocalPose p;

    // Left/right assignment is the NUMBERS contract: the left leg's local
    // cycle is shifted so ITS plant (local 0.25) lands on FOOTFALL_PHASE_LEFT.
    const auto left_phase = static_cast<float>(config::FOOTFALL_PHASE_LEFT);
    const auto right_phase = static_cast<float>(config::FOOTFALL_PHASE_RIGHT);
    const float pl = phase - (left_phase - 0.25f);
    const float pr_ = phase - (right_phase - 0.25f);

    // Amplitude from the actual step: sin(A) = (step/2) / leg length, capped
    // (see THIGH_SWING_MAX_SIN). Legs measure hip-to-ankle.
    const float leg_len = std::max(0.01f, pr.hip_height - pr.ankle_height);
    const float sin_a =
        std::clamp(step_length_m * 0.5f / leg_len, 0.0f, THIGH_SWING_MAX_SIN);
    const float amp = std::asin(sin_a);
    const float amp_ratio = sin_a / THIGH_SWING_MAX_SIN;
    const float thigh_len = std::max(0.01f, pr.thigh_length());

    // The pelvis arc is needed BEFORE the legs now: the forefoot rocker asks
    // how high the ankle actually is, and that depends on where the pelvis is.
    const float dy = thigh_len * (std::cos(amp * std::sin(TWO_PI * pl)) - 1.0f);
    const LegAngles left = leg_angles(pr, pl, amp, amp_ratio, dy);
    const LegAngles right = leg_angles(pr, pr_, amp, amp_ratio, dy);
    p.rotation[bone_index(Bone::ThighL)] = pitch(left.thigh);
    p.rotation[bone_index(Bone::ShinL)] = pitch(left.knee);
    p.rotation[bone_index(Bone::FootL)] = pitch(left.foot);
    p.rotation[bone_index(Bone::ThighR)] = pitch(right.thigh);
    p.rotation[bone_index(Bone::ShinR)] = pitch(right.knee);
    p.rotation[bone_index(Bone::FootR)] = pitch(right.foot);

    // Pelvis: the DERIVED wheel-gait arc (see the model note atop this file):
    // dy = T*(cos(theta_stance)-1), |theta| shared by both legs' locals.
    // Minima land exactly on the plants, the maximum on mid-stance, and the
    // planted foot sits on the ground through stance BY CONSTRUCTION rather
    // than by a second tuned number.
    const float sway = std::sin(TWO_PI * (phase - left_phase));
    p.pelvis_offset = {-SWAY_M * sway, dy, 0.0f};

    // Torso counter-rotates the pelvis, lists onto the stance side (see
    // TORSO_ROLL note), and run adds forward lean.
    const float twist = TORSO_TWIST * std::sin(TWO_PI * pl);
    p.rotation[bone_index(Bone::Torso)] =
        pitch(-RUN_LEAN * run_weight) * yaw_q(twist) * roll(TORSO_ROLL * sway);

    // Arms counterphase to same-side legs; run carries the elbows bent.
    const float arm_l = -ARM_SWING_RATIO * amp * std::sin(TWO_PI * pl);
    const float arm_r = -ARM_SWING_RATIO * amp * std::sin(TWO_PI * pr_);
    p.rotation[bone_index(Bone::UpperArmL)] = pitch(arm_l);
    p.rotation[bone_index(Bone::UpperArmR)] = pitch(arm_r);
    const float elbow_extra = glm::mix(ELBOW_SWING, RUN_ELBOW, run_weight);
    p.rotation[bone_index(Bone::ForearmL)] =
        pitch(ELBOW_BASE + elbow_extra * std::max(0.0f, -std::sin(TWO_PI * pl)));
    p.rotation[bone_index(Bone::ForearmR)] =
        pitch(ELBOW_BASE + elbow_extra * std::max(0.0f, -std::sin(TWO_PI * pr_)));

    // Head stabilizes the gaze against the torso lean.
    p.rotation[bone_index(Bone::Head)] = pitch(RUN_LEAN * run_weight * HEAD_STABILIZE);
    return p;
}

void apply_crouch(const Rig& rig, float blend, LocalPose& pose) {
    const float b = glm::clamp(blend, 0.0f, 1.0f);
    if (b <= 0.0f) {
        return;
    }
    const auto& pr = rig.proportions;
    const float t = pr.thigh_length();
    const float s = pr.shin_length();
    // Lower the pelvis; fold both legs symmetrically so the feet stay put:
    // with equal fold angle a, hip height above ankle = (t+s) * cos(a).
    // Drop half the leg at full crouch (matches CROUCH_EYE_HEIGHT being about
    // half of PLAYER_EYE_HEIGHT without duplicating sim's camera constant).
    const float drop = 0.5f * (t + s) * b;
    const float cos_a = glm::clamp(1.0f - drop / (t + s), 0.05f, 1.0f);
    const float a = std::acos(cos_a);
    pose.pelvis_offset.y -= drop;
    const auto fold = [&](Bone thigh, Bone shin, Bone foot) {
        pose.rotation[bone_index(thigh)] = pitch(a) * pose.rotation[bone_index(thigh)];
        pose.rotation[bone_index(shin)] = pitch(-2.0f * a) * pose.rotation[bone_index(shin)];
        pose.rotation[bone_index(foot)] = pitch(a) * pose.rotation[bone_index(foot)];
    };
    fold(Bone::ThighL, Bone::ShinL, Bone::FootL);
    fold(Bone::ThighR, Bone::ShinR, Bone::FootR);
    // Hunch the torso forward a touch — a bolt-upright crouch reads as sitting.
    pose.rotation[bone_index(Bone::Torso)] =
        pitch(-0.25f * b) * pose.rotation[bone_index(Bone::Torso)];
}

LocalPose air_pose(float vertical_velocity_mps) {
    LocalPose p;
    const float lean =
        glm::clamp(AIR_LEAN_PER_MPS * vertical_velocity_mps, -0.15f, 0.15f);
    p.rotation[bone_index(Bone::ThighL)] = pitch(AIR_THIGH + lean);
    p.rotation[bone_index(Bone::ThighR)] = pitch(AIR_THIGH + lean);
    p.rotation[bone_index(Bone::ShinL)] = pitch(-AIR_KNEE);
    p.rotation[bone_index(Bone::ShinR)] = pitch(-AIR_KNEE);
    p.rotation[bone_index(Bone::UpperArmL)] = roll(-AIR_ARM_OUT);
    p.rotation[bone_index(Bone::UpperArmR)] = roll(AIR_ARM_OUT);
    p.rotation[bone_index(Bone::ForearmL)] = pitch(ELBOW_BASE);
    p.rotation[bone_index(Bone::ForearmR)] = pitch(ELBOW_BASE);
    p.rotation[bone_index(Bone::Torso)] = pitch(-lean);
    return p;
}

void apply_land_dip(const Rig& rig, float dip01, LocalPose& pose) {
    const float d = glm::clamp(dip01, 0.0f, 1.0f);
    if (d <= 0.0f) {
        return;
    }
    (void)rig;
    pose.pelvis_offset.y -= LAND_PELVIS_DROP * d;
    const float a = 0.5f * LAND_KNEE * d;
    for (const Bone thigh : {Bone::ThighL, Bone::ThighR}) {
        pose.rotation[bone_index(thigh)] = pitch(a) * pose.rotation[bone_index(thigh)];
    }
    for (const Bone shin : {Bone::ShinL, Bone::ShinR}) {
        pose.rotation[bone_index(shin)] =
            pitch(-LAND_KNEE * d) * pose.rotation[bone_index(shin)];
    }
    for (const Bone foot : {Bone::FootL, Bone::FootR}) {
        pose.rotation[bone_index(foot)] =
            pitch(0.5f * LAND_KNEE * d) * pose.rotation[bone_index(foot)];
    }
}

LocalPose wave_pose(float time_s) {
    LocalPose p = idle_pose(time_s);
    // THE WAG MOVED OFF THE ELBOW, and it had to: an elbow is a HINGE, so the
    // rig reduces it to its own X axis (Pose.cpp) and a roll asked of it is
    // not clamped, it is DELETED. This clip used to wag the forearm with
    // roll(WAVE_AMP * sin), and measured through evaluate_body_pose the
    // clamped forearm quaternion was the CONSTANT (0.989, 0.149, 0, 0) at
    // every instant of the cycle — the right hand travelled 11 mm over a full
    // wag, all of it the idle breath. The clip was asking a hinge to do
    // something hinges cannot do, so nothing moved at all.
    //
    // A human waves with humeral ROTATION: the upper arm turns about its own
    // long axis while the elbow holds a bend, and the forearm sweeps sideways
    // as a result. That axis is the upper arm's rest -Y, the bone is FREE
    // (no hinge range), and composing raise * twist puts the twist in the
    // arm's own frame. Same gesture, on the joint that owns it (Rule 32: the
    // mechanism, which is why flex_pose below is fixed in the same change and
    // ClipTests now checks every clip rather than this one).
    const float wag = WAVE_AMP * std::sin(TWO_PI * WAVE_HZ * time_s);
    p.rotation[bone_index(Bone::UpperArmR)] = roll(WAVE_RAISE) * yaw_q(wag);
    p.rotation[bone_index(Bone::ForearmR)] = pitch(WAVE_ELBOW);
    p.rotation[bone_index(Bone::Head)] = yaw_q(0.1f);
    return p;
}

LocalPose flex_pose(float time_s) {
    LocalPose p;
    const float pump = FLEX_PUMP * std::sin(TWO_PI * FLEX_HZ * time_s);
    // The elbow-splay rolls that used to ride on the forearms were deleted by
    // the hinge reduction exactly as the wave's wag was (see wave_pose): they
    // cost two multiplications and changed nothing on screen. The splay is
    // humeral rotation, so it belongs on the upper arm.
    p.rotation[bone_index(Bone::UpperArmL)] = roll(-FLEX_RAISE - pump) * yaw_q(FLEX_SPLAY);
    p.rotation[bone_index(Bone::UpperArmR)] = roll(FLEX_RAISE + pump) * yaw_q(-FLEX_SPLAY);
    p.rotation[bone_index(Bone::ForearmL)] = pitch(FLEX_CURL);
    p.rotation[bone_index(Bone::ForearmR)] = pitch(FLEX_CURL);
    p.rotation[bone_index(Bone::Torso)] = pitch(-0.06f);
    return p;
}

} // namespace dfn::anim
