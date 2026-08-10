/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 12:10:00
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
constexpr float THIGH_SWING_MAX_SIN = 0.55f; // cap on sin(thigh amplitude): an
    // uncapped asin() at sim's brisk 3 m/s walk gives a cartoon scissor, and
    // the cap bounds the derived pelvis arc (below) to T*(1-cos(0.58)) ~ 7 cm.
    // KNOWN v1 LIMIT, recorded: at sim's step model the visual reach covers
    // roughly half the actual step at 3 m/s, so fast feet slide somewhat;
    // honest fix is slower WALK_SPEED (movement grill) or hip translation.
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
constexpr float WAVE_AMP = 0.5f;          // rad, forearm wag.
constexpr float WAVE_HZ = 1.8f;
constexpr float FLEX_RAISE = 1.6f;        // rad, both arms out to the sides.
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
    p.rotation[bone_index(Bone::Head)] = pitch(RUN_LEAN * run_weight * 0.6f);
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
    p.rotation[bone_index(Bone::UpperArmR)] = roll(WAVE_RAISE);
    p.rotation[bone_index(Bone::ForearmR)] =
        roll(WAVE_AMP * std::sin(TWO_PI * WAVE_HZ * time_s)) * pitch(0.3f);
    p.rotation[bone_index(Bone::Head)] = yaw_q(0.1f);
    return p;
}

LocalPose flex_pose(float time_s) {
    LocalPose p;
    const float pump = FLEX_PUMP * std::sin(TWO_PI * FLEX_HZ * time_s);
    p.rotation[bone_index(Bone::UpperArmL)] = roll(-FLEX_RAISE - pump);
    p.rotation[bone_index(Bone::UpperArmR)] = roll(FLEX_RAISE + pump);
    p.rotation[bone_index(Bone::ForearmL)] = pitch(FLEX_CURL) * roll(-0.3f);
    p.rotation[bone_index(Bone::ForearmR)] = pitch(FLEX_CURL) * roll(0.3f);
    p.rotation[bone_index(Bone::Torso)] = pitch(-0.06f);
    return p;
}

} // namespace dfn::anim
