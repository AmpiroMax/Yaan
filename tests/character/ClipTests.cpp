/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 22:39:13
Module: tests
File: tests/character/ClipTests.cpp

Responsibility:
- The gait contract: foot plants and pelvis-bob minima land EXACTLY on the
  FOOTFALL_PHASE_LEFT/RIGHT rows sim fires FootfallEvents at — the desync
  these rows exist to prevent is the rejected instance (research §D1), and
  the control is a deliberately phase-shifted clip that MUST fail (Rule 30).
- Clip sanity: gait amplitude follows step length; crouch keeps feet near
  the ground; air/wave/flex poses differ from idle.

Dependencies:
- Uses: doctest, dfn_anim, constants (FOOTFALL_PHASE_*, STEP_LENGTH_*).
- Used by: ctest (character_clips).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Plant expectations come from the generated FOOTFALL_PHASE_* names — never
  a literal 0.25/0.75 (that would be the two-copies defect the row closed).
*/
/*
UPD:
- 10:08:2026 - 01:56:45: Initial gait-contract suite.
- 10:08:2026 - 12:10:00: Contact is measured at the SOLE against the ground, not at the ankle against its own minimum; double support replaces the single-support release assertion.
- 10:08:2026 - 20:00:23: The wave waves (hand sweep, with the elbow-roll version as the control) + no clip loses motion to the hinge reduction; four doctest::Approx epsilons replaced by explicit metres (lead's broadcast: the admitted band was e*(1+|x|), up to +/-0.27 m on a foot).
- 10:08:2026 - 20:22:44: eye_lean_offset asserted against the rig's own FK, plus the property the seam exists for — leaning harder never brings the chest closer to the eye, with today's non-riding eye as the control.
- 10:08:2026 - 21:34:24: The slip check the swing-cap row asks for, phrased as the outcome — and it surfaced that the row's 0.798 % is IDEALISED: the drawn ankle travels 0.6944 m against the stick model's 0.9722, so real slip at a walk is 29.1 %, not 0.8 %. Plus explicit bounds on the chest-to-eye clearance the repo audit named (the old +/-20.5 mm band also hid a wrong nominal).
- 10:08:2026 - 22:39:13: THE CROUCHED EYE. crouch_eye_offset() checked against the rig's own FK skull at four blends, the property stated as the outcome (the eye is above the neck at every depth of squat), and the retired CROUCH_EYE_HEIGHT 0.85 as the control — a REAL rejected instance, 0.3711 m below the drawn eye and 0.2478 m below the neck. Plus the crouched gaze (-5.73 deg, was -14.3), with the un-stabilized head as its control.
*/

#include <doctest/doctest.h>

#include <array>
#include <cmath>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include "engine/anim/sources/Clips.h"
#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/Rig.h"
#include "engine/core/config/sources/Constants.h"

using namespace dfn;
using namespace dfn::anim;

namespace {

constexpr int SAMPLES = 400; // phase resolution 0.0025 — an order finer than
                             // the 0.02 tolerance the assertions use

[[nodiscard]] float step_at(float v) {
    return static_cast<float>(config::STEP_LENGTH_BASE)
         + static_cast<float>(config::STEP_LENGTH_PER_MPS) * v;
}

// Ankle-joint height of `foot` at `phase` (phase_shift models the broken
// clip the control needs).
// THE LOWEST POINT OF THE SOLE, not the ankle joint. The distinction became
// load-bearing the moment the walk grew a forefoot rocker: through late stance
// the heel lifts and the ANKLE climbs while the TOE is still planted, so an
// ankle-height reading calls the foot airborne a fifth of a cycle before it
// leaves the ground. "In contact" means the sole touches; measure the sole.
[[nodiscard]] float sole_height(const Rig& rig, Bone foot, float phase,
                                float step, float phase_shift = 0.0f) {
    const LocalPose pose = gait_pose(rig, phase + phase_shift, step, 0.0f);
    std::array<glm::mat4, BONE_COUNT> m;
    forward_kinematics(rig, pose, {}, m);
    const auto& p = rig.proportions;
    const float heel = p.foot_length * 0.25f; // BodyMesh FOOT_HEEL_RATIO
    float lowest = 1e9f;
    for (const float z : {heel, -(p.foot_length - heel)}) {
        const glm::vec4 corner{0.0f, -p.ankle_height, z, 1.0f};
        lowest = std::min(lowest, (m[bone_index(foot)] * corner).y);
    }
    return lowest;
}

// TOUCH-DOWN / RELEASE phases: the edges of the foot's ground-contact run —
// touch-down is the perceptual event the footfall sound marks. "Contact" is
// the ankle within `contact_eps` of its own cycle minimum (the wheel-gait
// stance plateau); exactly one contact run exists per cycle, so both edges
// are unique.
struct ContactEdges {
    float touch_down = -1.0f;
    float release = -1.0f;
};
[[nodiscard]] ContactEdges contact_edges(const Rig& rig, Bone foot, float step,
                                         float phase_shift = 0.0f) {
    std::array<float, SAMPLES> h{};
    for (int i = 0; i < SAMPLES; ++i) {
        h[static_cast<size_t>(i)] = sole_height(
            rig, foot, static_cast<float>(i) / SAMPLES, step, phase_shift);
    }
    // 1 mm: the sqrt swing envelope makes approach height LINEAR in phase
    // (~0.5 m per cycle), so a 1 mm epsilon leads the true instant by only
    // ~0.002 of a cycle. A soft (quadratic) approach at a loose epsilon
    // would put the detected edge outside the very tolerance the test
    // asserts — measuring its own epsilon (Rule 36's standing check: when
    // the reading sits near the cutoff, the cutoff is the answer).
    // ABSOLUTE, measured from the GROUND rather than from the curve's own
    // minimum. The relative form was self-referential in the way Rule 36 warns
    // about: a sub-millimetre numerical undershoot at the plant dragged the
    // threshold down with it and shortened every stance it measured. The sole's
    // stance height is zero by construction, so zero is the thing to compare to.
    constexpr float contact_eps = 0.001f;
    constexpr float lowest = 0.0f;
    ContactEdges edges;
    int rises = 0;
    int falls = 0;
    for (int i = 0; i < SAMPLES; ++i) {
        const bool prev_contact =
            h[static_cast<size_t>((i + SAMPLES - 1) % SAMPLES)] < lowest + contact_eps;
        const bool now_contact = h[static_cast<size_t>(i)] < lowest + contact_eps;
        if (now_contact && !prev_contact) {
            ++rises;
            edges.touch_down = static_cast<float>(i) / SAMPLES;
        } else if (!now_contact && prev_contact) {
            ++falls;
            edges.release = static_cast<float>(i) / SAMPLES;
        }
    }
    REQUIRE(rises == 1); // one touch-down per cycle, or the notion is broken
    REQUIRE(falls == 1);
    return edges;
}

[[nodiscard]] float phase_distance(float a, float b) {
    const float d = std::abs(a - b);
    return std::min(d, 1.0f - d);
}

} // namespace

TEST_CASE("feet touch down exactly at the FOOTFALL_PHASE rows sim fires at") {
    const Rig rig = Rig::build(RigProportions::from_config());
    const float step = step_at(static_cast<float>(config::WALK_SPEED));
    const auto left = static_cast<float>(config::FOOTFALL_PHASE_LEFT);
    const auto right = static_cast<float>(config::FOOTFALL_PHASE_RIGHT);
    constexpr float TOL = 0.02f; // 1/50 cycle; at walk cadence ~2 cycles/s
                                 // that is ~10 ms — under the audio-visual
                                 // desync detection floor (~20-30 ms)

    const ContactEdges el = contact_edges(rig, Bone::FootL, step);
    const ContactEdges er = contact_edges(rig, Bone::FootR, step);
    CHECK(phase_distance(el.touch_down, left) < TOL);
    CHECK(phase_distance(er.touch_down, right) < TOL);
    // DOUBLE SUPPORT. The old model held the sole flat until the instant the
    // other foot landed, so release == the other plant. A real walk rolls over
    // the toe instead, and toe-off comes AFTER the other foot is down — that
    // overlap is what makes walking walking rather than a series of falls. So
    // the assertion is no longer equality but the thing that actually matters:
    // this foot must still be down when the other one plants, and must let go
    // before its own next touch-down.
    CHECK(phase_distance(el.release, right) < 0.12f);
    CHECK(phase_distance(er.release, left) < 0.12f);
    const float el_stance = std::fmod(el.release - el.touch_down + 1.0f, 1.0f);
    const float er_stance = std::fmod(er.release - er.touch_down + 1.0f, 1.0f);
    CHECK(el_stance > 0.5f); // stance outlasts the other foot's arrival
    CHECK(er_stance > 0.5f);
    CHECK(el_stance < 0.75f); // ...but this is a walk, not a shuffle
    CHECK(er_stance < 0.75f);

    // The planted foot then STAYS grounded into stance: a fifth of a cycle
    // after touch-down the SOLE is still on the ground (the ankle above it is
    // free to rise once the heel lifts, and now does).
    const float at_plant = sole_height(rig, Bone::FootL, left, step);
    const float mid_stance = sole_height(rig, Bone::FootL, left + 0.2f, step);
    // EXPLICIT METRES, not doctest::Approx (lead's broadcast 10:08:2026, core's
    // find): Approx(x).epsilon(e) admits e * (scale + max(|lhs|,|rhs|)) with
    // scale defaulting to 1, so on a quantity this small the "+1" IS the
    // tolerance and the number in the source says nothing about what passes.
    // SOLE_STAYS_DOWN is the perceptual bound: at 640x360 with the body a
    // metre from the eye, a centimetre of sole lift is under a pixel.
    constexpr float SOLE_STAYS_DOWN = 0.01f; // m
    CHECK(std::abs(mid_stance - at_plant) < SOLE_STAYS_DOWN);

    // CONTROL (Rule 30): a clip shifted by 0.07 must FAIL the same check.
    // 0.07 is ~35 ms at walk cadence — the audible desync magnitude the
    // research names as illusion-destroying; the test must reject it.
    const ContactEdges shifted = contact_edges(rig, Bone::FootL, step, 0.07f);
    CHECK(phase_distance(shifted.touch_down, left) > TOL);
    // And in the OTHER direction (a range is two assertions, Rule 30): a
    // -0.07 shift must fail too — late plants and early plants are different
    // defects and a one-sided check would pass one of them.
    const ContactEdges shifted_late = contact_edges(rig, Bone::FootL, step, -0.07f);
    CHECK(phase_distance(shifted_late.touch_down, left) > TOL);

    // No penetration anywhere in the cycle: the swing clearance is
    // non-negative by construction; a model regression that dips the foot
    // through the floor must fail here, not in a frame.
    float lowest = 1e9f;
    for (int i = 0; i < SAMPLES; ++i) {
        lowest = std::min(lowest,
                          sole_height(rig, Bone::FootL,
                                      static_cast<float>(i) / SAMPLES, step));
    }
    CHECK(lowest > at_plant - 0.001f);
}

TEST_CASE("pelvis bob minima sit on both footfall phases") {
    const Rig rig = Rig::build(RigProportions::from_config());
    const float step = step_at(static_cast<float>(config::WALK_SPEED));
    const auto left = static_cast<float>(config::FOOTFALL_PHASE_LEFT);
    const auto right = static_cast<float>(config::FOOTFALL_PHASE_RIGHT);

    const auto bob = [&](float phase) {
        return gait_pose(rig, phase, step, 0.0f).pelvis_offset.y;
    };
    // Both plants are local minima of the pelvis height (sim's "cycle
    // minimum" definition of a footfall — same words, same numbers).
    for (const float plant : {left, right}) {
        const float here = bob(plant);
        CHECK(here <= bob(plant + 0.05f));
        CHECK(here <= bob(plant - 0.05f));
        CHECK(here < bob(plant + 0.25f) - 0.005f); // vs mid-stance, real dip
    }
    // Control: the midpoint between plants is the maximum, not a minimum.
    const float mid = left + phase_distance(left, right) * 0.5f;
    CHECK(bob(mid) > bob(left) + 0.005f);
}

TEST_CASE("gait amplitude follows step length (and is capped)") {
    const Rig rig = Rig::build(RigProportions::from_config());
    // Feet spread further at a longer step...
    const auto spread = [&](float step) {
        const LocalPose pose = gait_pose(rig, 0.25f, step, 0.0f);
        std::array<glm::mat4, BONE_COUNT> m;
        forward_kinematics(rig, pose, {}, m);
        return std::abs(m[bone_index(Bone::FootL)][3].z
                        - m[bone_index(Bone::FootR)][3].z);
    };
    CHECK(spread(0.9f) > spread(0.5f) + 0.05f);
    // ...but the cap keeps a sprint-multiplier step from a cartoon scissor:
    // beyond the cap the spread stops growing.
    // EXACT, and it can be: past the cap both steps clamp to the same
    // THIGH_SWING_MAX_SIN, so the two spreads are the same float. Measured
    // difference 0.000000 m. The old Approx(...).epsilon(0.01) admitted
    // 0.0169 m here (0.01 x (1 + 0.694)) — 1.7 cm of drift on a quantity that
    // is meant to be bit-identical, which is a different assertion entirely.
    CHECK(std::abs(spread(4.0f) - spread(3.0f)) < 1.0e-4f);
    // Control: zero step length is a standing pose — no spread beyond the
    // foot boxes themselves.
    CHECK(spread(0.0f) < 0.01f);

    // THE CHECK `BODY_THIGH_SWING_MAX_SIN`'s ROW ASKS FOR, and until now the
    // row had ZERO readers in the engine AND zero in this suite — a row that
    // guards nothing while looking like it guards something (the pattern the
    // repo audit named on `min_branch_diameter`; this is its instance in this
    // zone). Clips.cpp now reads the row instead of keeping a private 0.55,
    // and the row is a row precisely because sim measures residual foot slip
    // against the same cap (Rule 35: two zones, one number).
    //
    // PHRASED AS THE OUTCOME, WHICH THE ROW ALSO INSISTS ON: "residual slip
    // stays under a perceptual bound", never "the clamp is inactive" — at
    // WALK_SPEED the clamp still binds by 0.798 %, so the mechanism-shaped
    // version would have gone red on correct code the day it was written, and
    // a test that goes red on correct code gets weakened rather than argued
    // with (Rule 38).
    //
    // AND WRITING IT SURFACED THAT THE ROW'S OWN PERCENTAGE IS IDEALISED.
    // The row derives the shortfall as leg x (required_sin - 0.55) and reports
    // 7.82 mm = 0.798 % of the step at WALK_SPEED. That arithmetic treats
    // hip->ankle as a RIGID STICK at asin(0.55). The drawn leg is not a stick:
    // the knee is flexed through swing and the foot rockers, so the ankle's
    // actual fore-aft excursion is 0.6944 m against the stick model's 0.9722 —
    // **78.6 % of it**. Measured through FK over the evaluated pose rather than
    // recomputed from the cap, which is the whole reason it shows up here:
    //
    // | gear | step model | ankle excursion, DRAWN | slip | row/spec said |
    // |---|---|---|---|---|
    // | walk 1.8 | 0.980 | 0.6944 | 0.2856 = **29.1 %** | 0.798 % |
    // | jog 3.0  | 1.400 | 0.6944 | 0.7056 = 50.4 % | — |
    // | run 6.0  | 2.450 | 0.6944 | 1.7556 = **71.7 %** | ~60 % |
    //
    // Rule 44's shape: a constant fitted through an implementation detail stops
    // meaning what its name says. "Residual slip" in the row means residual in
    // a model of the leg, not residual in the leg that is drawn, and the two
    // differ by 36x at a walk. Reported to lead for the row's note; NOT quietly
    // corrected here, because it is also the number the row says sim's
    // instrument independently agrees with, and I have not read their
    // instrument to know which quantity IT is on (Rule 34).
    const auto excursion = [&](float speed) {
        const float step = step_at(speed);
        float lo = 1e9f;
        float hi = -1e9f;
        for (int i = 0; i < SAMPLES; ++i) {
            const LocalPose pose =
                gait_pose(rig, static_cast<float>(i) / SAMPLES, step, 0.0f);
            std::array<glm::mat4, BONE_COUNT> m;
            forward_kinematics(rig, pose, {}, m);
            const float z = m[bone_index(Bone::FootL)][3].z;
            lo = std::min(lo, z);
            hi = std::max(hi, z);
        }
        return hi - lo;
    };
    const float walk_reach = excursion(static_cast<float>(config::WALK_SPEED));
    const float run_reach = excursion(static_cast<float>(config::RUN_SPEED));

    // THE CAP SATURATES AT EVERY GEAR, INCLUDING A WALK, so the drawn stride is
    // one constant and the slip is entirely a function of how fast the world
    // moves under it. That is the v1 limit stated as a measurement instead of a
    // comment: explicit bounds, 0.6944 measured (Rule 40).
    CHECK(std::abs(walk_reach - run_reach) < 1.0e-3f);
    CHECK(walk_reach > 0.68f);
    CHECK(walk_reach < 0.71f);

    // ...and the slip therefore GROWS with speed. Pinned so the limit cannot
    // silently get worse, and so that the day someone fixes it — slower rows,
    // or hip translation — this goes red and the limit is deleted deliberately
    // rather than drifting into being untrue.
    const float walk_step = step_at(static_cast<float>(config::WALK_SPEED));
    const float run_step = step_at(static_cast<float>(config::RUN_SPEED));
    const float walk_slip = (walk_step - walk_reach) / walk_step;
    const float run_slip = (run_step - run_reach) / run_step;
    CHECK(walk_slip > 0.25f); // measured 0.291
    CHECK(walk_slip < 0.33f);
    CHECK(run_slip > walk_slip + 0.3f); // measured 0.717 vs 0.291
}

TEST_CASE("crouch folds the legs and keeps feet near the ground") {
    const Rig rig = Rig::build(RigProportions::from_config());
    LocalPose pose; // rest
    apply_crouch(rig, 1.0f, pose);
    std::array<glm::mat4, BONE_COUNT> m;
    forward_kinematics(rig, pose, {}, m);
    const auto& p = rig.proportions;
    // Pelvis dropped by half the leg (the clip's documented model).
    const float expected_drop = 0.5f * (p.thigh_length() + p.shin_length());
    // 1 cm, EXPLICIT. The residual is 7.3 mm and it has a cause rather than
    // being noise: the fold's arithmetic is planar, while the legs also lean
    // inward by leg_convergence(), which shortens their vertical reach by
    // cos(theta). Anything past a centimetre means the fold model changed.
    CHECK(std::abs(m[bone_index(Bone::Pelvis)][3].y - (p.hip_height - expected_drop))
          < 0.01f);
    // THE FEET STAY PLANTED — and this is the assertion lead's broadcast
    // caught (core's find, 10:08:2026). It read
    // `Approx(p.ankle_height).epsilon(0.25)`, and doctest's tolerance is
    // e * (scale + max(|lhs|,|rhs|)) with scale = 1, so on an ankle_height of
    // 0.0702 m the admitted band was +/-0.268 m: a quarter of a metre of
    // slack on the one property the line names. It could not tell a planted
    // foot from a dangling one, which is Rule 38 pointed at an assertion —
    // green on correct code AND green on the defect.
    // MEASURED: the crouch fold moves the ankle 3.7 mm (same convergence
    // term as above), so 1 cm passes with 2.7x of margin (Rule 30a) while
    // sitting two orders below the -0.23 m the no-fold control produces.
    CHECK(std::abs(m[bone_index(Bone::FootL)][3].y - p.ankle_height) < 0.01f);
    // Control: WITHOUT the fold, the same pelvis drop buries the ankles —
    // the geometry term is load-bearing, not decorative.
    LocalPose no_fold;
    no_fold.pelvis_offset.y = -expected_drop;
    forward_kinematics(rig, no_fold, {}, m);
    CHECK(m[bone_index(Bone::FootL)][3].y < p.ankle_height - 0.3f);
}

TEST_CASE("air, wave and flex poses are not idle") {
    const auto differs = [](const LocalPose& a, const LocalPose& b) {
        for (uint32_t i = 0; i < BONE_COUNT; ++i) {
            if (std::abs(glm::dot(a.rotation[i], b.rotation[i])) < 0.999f) {
                return true;
            }
        }
        return false;
    };
    const LocalPose idle = idle_pose(0.0f);
    CHECK(differs(air_pose(0.0f), idle));
    CHECK(differs(wave_pose(0.0f), idle));
    CHECK(differs(flex_pose(0.0f), idle));
    // Control: idle at the same instant equals itself.
    CHECK_FALSE(differs(idle_pose(0.0f), idle));
}

// --- THE HINGE DELETES WHAT IT CANNOT REPRESENT ---------------------------
// User, 10:08:2026: «в анимации махания всё такая же проблема, локоть
// неестественно двигается». The joint limits DID cover the wave — that was
// never the gap. The gap is that a hinge does not clamp an off-axis rotation,
// it DROPS it (Pose.cpp: "a knee handed a rotation with yaw or roll in it
// gets no yaw or roll at all"), and the wave asked its ELBOW for a roll. So
// the whole wag was thrown away silently and the arm was a rigid stick.
//
// Both checks below assert the OUTCOME (Rule 38): what the eye gets, not
// which quaternion produced it.

namespace {

[[nodiscard]] glm::vec3 hand_at(const Rig& rig, const LocalPose& raw) {
    LocalPose pose = raw;
    apply_joint_limits(rig, pose); // exactly what the renderer receives
    std::array<glm::mat4, BONE_COUNT> m;
    forward_kinematics(rig, pose, {}, m);
    return glm::vec3{m[bone_index(Bone::HandR)][3]};
}

// The rejected instance, kept as source: the shipped wave up to 10:08:2026.
[[nodiscard]] LocalPose wave_pose_on_the_elbow(float t) {
    LocalPose p = idle_pose(t);
    p.rotation[bone_index(Bone::UpperArmR)] =
        glm::angleAxis(2.4f, glm::vec3{0.0f, 0.0f, 1.0f});
    p.rotation[bone_index(Bone::ForearmR)] =
        glm::angleAxis(0.5f * std::sin(2.0f * glm::pi<float>() * 1.8f * t),
                       glm::vec3{0.0f, 0.0f, 1.0f})
        * glm::angleAxis(0.3f, glm::vec3{1.0f, 0.0f, 0.0f});
    return p;
}

} // namespace

TEST_CASE("the wave waves: the hand sweeps a visible arc") {
    const Rig rig = Rig::build(RigProportions::from_config());
    const auto travel = [&](LocalPose (*clip)(float)) {
        glm::vec3 lo{1e9f};
        glm::vec3 hi{-1e9f};
        for (int i = 0; i <= 64; ++i) { // one full 1.8 Hz wag
            const float t = 4.0f + static_cast<float>(i) / (64.0f * 1.8f);
            const glm::vec3 h = hand_at(rig, clip(t));
            lo = glm::min(lo, h);
            hi = glm::max(hi, h);
        }
        return glm::length(hi - lo);
    };
    // WHY 0.10 m, and the number is a bracket rather than a taste: a wave has
    // to be legible on a body a few metres off at 640x360, and 0.10 m at 3 m
    // is ~11 px of hand travel. Measured, the fixed clip sweeps 0.236 m, so
    // this passes with 2.4x of margin (Rule 30a: a case that CAN pass).
    constexpr float VISIBLE_SWEEP = 0.10f; // m
    CHECK(travel(wave_pose) > VISIBLE_SWEEP);

    // CONTROL (Rule 30), and it is a REAL rejected instance, not a synthetic
    // one: the shipped wave. Its wag lived on the elbow, the hinge dropped
    // it, and the hand moved 0.011 m over the whole cycle — every millimetre
    // of that the idle breath, none of it the wave. The threshold sits 9x
    // above the defect and 2.4x below the fix.
    CHECK(travel(wave_pose_on_the_elbow) < VISIBLE_SWEEP);
    CHECK(travel(wave_pose) > 10.0f * travel(wave_pose_on_the_elbow));
}

TEST_CASE("no shipped clip loses motion to the hinge reduction") {
    const Rig rig = Rig::build(RigProportions::from_config());
    // The standing guard the wave defect earned (Rule 32 — the mechanism, not
    // the one clip): if a clip's authored pose already places every hinge on
    // its own axis, then the reduction has nothing to delete and the limits
    // can only ever CLAMP AN ANGLE, which is their job. This is deliberately
    // not "the clamp changed nothing": crouch legitimately folds a knee past
    // BODY_KNEE_FLEX_MAX and getting clamped there is correct animation.
    const auto off_axis = [&](const LocalPose& pose) {
        float worst = 0.0f;
        for (uint32_t b = 0; b < BONE_COUNT; ++b) {
            if (!std::isfinite(rig.hinge_range[b].x)) {
                continue; // free bone: any axis is legal there
            }
            const glm::quat& q = pose.rotation[b];
            worst = std::max(worst, std::abs(q.y) + std::abs(q.z));
        }
        return worst;
    };
    constexpr float ON_AXIS = 1.0e-6f;
    const float step = step_at(static_cast<float>(config::WALK_SPEED));
    for (int i = 0; i <= 32; ++i) {
        const float t = static_cast<float>(i) * 0.1f;
        CHECK(off_axis(idle_pose(t)) < ON_AXIS);
        CHECK(off_axis(wave_pose(t)) < ON_AXIS);
        CHECK(off_axis(flex_pose(t)) < ON_AXIS);
        CHECK(off_axis(air_pose(4.0f - t)) < ON_AXIS);
        LocalPose g = gait_pose(rig, static_cast<float>(i) / 32.0f, step, 0.0f);
        CHECK(off_axis(g) < ON_AXIS);
        apply_crouch(rig, static_cast<float>(i) / 32.0f, g);
        apply_land_dip(rig, static_cast<float>(i) / 32.0f, g);
        CHECK(off_axis(g) < ON_AXIS);
    }
    // CONTROL: the shipped wave put 0.233 of quaternion z on an elbow.
    CHECK(off_axis(wave_pose_on_the_elbow(4.0f)) > 0.2f);
}

// --- THE EYE RIDES THE LEAN ----------------------------------------------

namespace {

// Where the eye actually is, in world space, for a given pose: the eye lives
// in the SKULL, so it is the head bone's world matrix applied to the eye's
// offset from the neck joint. Reading it through FK is the point — it is what
// makes these assertions checks on the RIG rather than on a second copy of
// the arithmetic in eye_lean_offset().
[[nodiscard]] glm::vec3 eye_world(const Rig& rig, const LocalPose& pose) {
    std::array<glm::mat4, BONE_COUNT> m;
    forward_kinematics(rig, pose, {}, m);
    const auto eye_height = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const auto eye_forward = static_cast<float>(config::PLAYER_EYE_FORWARD);
    const glm::vec4 in_skull{0.0f, eye_height - rig.proportions.neck_height,
                             -eye_forward, 1.0f};
    return glm::vec3{m[bone_index(Bone::Head)] * in_skull};
}

} // namespace

TEST_CASE("eye_lean_offset matches where the lean actually puts the skull") {
    const Rig rig = Rig::build(RigProportions::from_config());
    const float step = step_at(static_cast<float>(config::RUN_SPEED));
    // Phase 0.25 puts the torso's lateral SWAY at zero, so the only nuisance
    // rotation left is the counter-twist, which shortens the measured forward
    // component by 0.7 mm — hence the 2 mm bound below rather than a tighter
    // one. Naming the reason is the point: the bound is a measurement of the
    // instrument, not a guess.
    constexpr float PHASE_NO_SWAY = 0.25f;
    constexpr float FK_AGREEMENT = 0.002f; // m

    for (const float w : {0.0f, 0.286f, 0.5f, 1.0f}) {
        const glm::vec3 flat =
            eye_world(rig, gait_pose(rig, PHASE_NO_SWAY, step, 0.0f));
        const glm::vec3 leaning =
            eye_world(rig, gait_pose(rig, PHASE_NO_SWAY, step, w));
        const glm::vec2 said = eye_lean_offset(rig.proportions, w);
        // Forward is -Z; drop is positive-down.
        CHECK(std::abs((flat.z - leaning.z) - said.x) < FK_AGREEMENT);
        CHECK(std::abs((flat.y - leaning.y) - said.y) < FK_AGREEMENT);
    }
    // Measured, and agreed to the millimetre with sim's independent
    // derivation: 0.1320 m forward and 0.0206 m down at full lean.
    const glm::vec2 full = eye_lean_offset(rig.proportions, 1.0f);
    CHECK(std::abs(full.x - 0.1320f) < 0.0005f);
    CHECK(std::abs(full.y - 0.0206f) < 0.0005f);
    // Control (Rule 30): a walk moves the eye not at all, and clamping holds
    // past the ends so an out-of-range weight cannot invent travel.
    CHECK(eye_lean_offset(rig.proportions, 0.0f).x == doctest::Approx(0.0f));
    CHECK(eye_lean_offset(rig.proportions, 2.0f).x == doctest::Approx(full.x));
    CHECK(eye_lean_offset(rig.proportions, -1.0f).x == doctest::Approx(0.0f));
}

TEST_CASE("leaning harder never brings the chest closer to the eye") {
    // THE PROPERTY THE WHOLE SEAM EXISTS FOR, and it is assertable entirely on
    // this side: however hard the trunk leans, the chest must not gain on the
    // eye. Today it gains 0.103 m of the 0.129 m gap, which is why the chest
    // enters frame at 27 deg at a run against the feet's 41.
    const Rig rig = Rig::build(RigProportions::from_config());
    const auto& p = rig.proportions;
    const float step = step_at(static_cast<float>(config::RUN_SPEED));
    // The torso's own front corner, through FK and the drawn depth — the
    // thing that actually blocks the view.
    const auto chest_ahead_of_eye = [&](float w, bool eye_rides) {
        const LocalPose pose = gait_pose(rig, 0.25f, step, w);
        std::array<glm::mat4, BONE_COUNT> m;
        forward_kinematics(rig, pose, {}, m);
        const glm::vec4 corner{0.0f, p.shoulder_height - p.hip_height,
                               -0.5f * p.torso_depth, 1.0f};
        const float chest_fwd = -(m[bone_index(Bone::Torso)] * corner).z;
        const float eye_fwd = static_cast<float>(config::PLAYER_EYE_FORWARD)
                            + (eye_rides ? eye_lean_offset(p, w).x : 0.0f);
        return chest_fwd - eye_fwd;
    };
    float previous = chest_ahead_of_eye(0.0f, true);
    for (const float w : {0.286f, 0.5f, 1.0f}) {
        const float now = chest_ahead_of_eye(w, true);
        CHECK(now <= previous + 1.0e-4f); // never gains
        previous = now;
    }
    // At full lean the gap has not merely shrunk, it has CROSSED: the chest
    // ends up 5.5 mm BEHIND the eye. Not a tuned coincidence — the eye and the
    // shoulder hang off the same hip pivot at comparable lever arms, so they
    // advance together by construction.
    CHECK(chest_ahead_of_eye(1.0f, true) < 0.0f);
    // EXPLICIT BOUNDS ON A CLEARANCE (Rule 40; the repo audit named this exact
    // line). It read `Approx(0.026f).epsilon(0.02)`, which admits
    // 0.02 x (1 + 0.026) = +/-0.0205 m — a band 158 % as wide as the gap it is
    // asserting, on the one quantity that decides whether you can see your own
    // chest. AND THE BAND HID A WRONG NOMINAL: the resting gap measures
    // 0.02537 m, not the 0.026 quoted here and in the spec, so the assertion
    // was 0.63 mm off its own subject and nothing could say so. That is the
    // sharper cost of a loose epsilon — not only that it fails to catch a
    // regression, but that the number written IN it stops being checked.
    const float resting_gap = chest_ahead_of_eye(0.0f, true);
    CHECK(resting_gap > 0.0244f); // measured 0.02537 m, +/-1 mm
    CHECK(resting_gap < 0.0264f);

    // CONTROL, and it is today's shipped behaviour rather than a synthetic
    // case: with the eye NOT riding the lean, the same sweep goes the wrong
    // way at every step and the gap ends up 5x its resting value.
    float worst = chest_ahead_of_eye(0.0f, false);
    for (const float w : {0.286f, 0.5f, 1.0f}) {
        CHECK(chest_ahead_of_eye(w, false) > worst);
        worst = chest_ahead_of_eye(w, false);
    }
    CHECK(worst > 4.0f * chest_ahead_of_eye(0.0f, false));
}

// --- THE CROUCHED EYE SITS IN THE CROUCHED HEAD ---------------------------

TEST_CASE("crouch_eye_offset puts the camera in the skull, not in the chest") {
    // THE USER'S BUG, AS A NUMBER («при присяди голова в коробку туловища
    // залезает», twice). The camera is not tested here — it is RECONSTRUCTED
    // from what sim does with this producer (standing eye height minus the
    // ferried drop, plus the ferried advance along the facing), and then
    // compared against the eye the RIG actually draws, through FK. That is the
    // point of the seam: one number, two consumers, and the test can see both.
    const Rig rig = Rig::build(RigProportions::from_config());
    const auto& p = rig.proportions;
    const auto eye_height = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const auto eye_forward = static_cast<float>(config::PLAYER_EYE_FORWARD);
    constexpr float FK_AGREEMENT = 0.002f; // m — the same instrument bound the
        // lean uses; the residual here is the planar fold against the legs'
        // inward convergence, ~1 mm at full crouch.

    // MEASURED AS A DIFFERENCE FROM STANDING, which is what the offset IS —
    // and the alternative is a worse test, not a stricter one: the drawn eye
    // stands at 1.6927 rather than PLAYER_EYE_HEIGHT 1.7000, because converged
    // legs span 7.3 mm less vertical than straight ones. That gap is the
    // STANDING pose's and predates every crouch; folding it in here would make
    // this case fail on correct code and pass on a crouch that cancelled it by
    // accident (Rule 38).
    LocalPose upright;
    const glm::vec3 standing = eye_world(rig, upright);
    for (const float b : {0.0f, 0.25f, 0.5f, 1.0f}) {
        LocalPose pose; // rest: the crouch is the only thing acting
        apply_crouch(rig, b, pose);
        const glm::vec3 drawn = eye_world(rig, pose);
        const glm::vec2 said = crouch_eye_offset(p, b);
        // Forward is -Z; drop is positive-down. Agreement is ~1e-5 m.
        CHECK(std::abs((standing.y - drawn.y) - said.y) < FK_AGREEMENT);
        CHECK(std::abs((standing.z - drawn.z) - said.x) < FK_AGREEMENT);
        // And the camera sim builds from it lands on the drawn eye to within
        // that same standing 7.3 mm — against 0.3711 m before this change.
        CHECK(std::abs((eye_height - said.y) - drawn.y) < 0.010f);
        CHECK(std::abs((eye_forward + said.x) - (-drawn.z)) < 0.010f);
    }

    // THE PROPERTY THE BUG IS, stated as the outcome (Rule 38): the eye is
    // above the NECK at every depth of squat. "Inside the torso" is not a
    // tolerance on a height — it is the eye falling below the top of the box.
    for (int i = 0; i <= 20; ++i) {
        const float b = static_cast<float>(i) / 20.0f;
        LocalPose pose;
        apply_crouch(rig, b, pose);
        std::array<glm::mat4, BONE_COUNT> m;
        forward_kinematics(rig, pose, {}, m);
        const float neck_y = m[bone_index(Bone::Head)][3].y;
        CHECK((eye_height - crouch_eye_offset(p, b).y) > neck_y);
    }

    // CONTROL, and it is the REJECTED INSTANCE rather than a synthetic one
    // (Rule 30): the shipped camera height, sim's retired CROUCH_EYE_HEIGHT
    // row. It fails both assertions above at full crouch — 0.36 m below the
    // drawn eye, and 0.25 m BELOW the neck, which is the chest the user was
    // standing inside.
    LocalPose full;
    apply_crouch(rig, 1.0f, full);
    std::array<glm::mat4, BONE_COUNT> m;
    forward_kinematics(rig, full, {}, m);
    const float neck_y = m[bone_index(Bone::Head)][3].y;
    // The value is written out rather than read from the row ON PURPOSE: the
    // row is being retired, and a control must keep failing after its subject
    // is deleted. This is the height the camera actually sat at, frozen.
    constexpr float retired = 0.85f; // sim's CROUCH_EYE_HEIGHT, retired
    CHECK(retired < neck_y - 0.2f);                        // inside the chest
    CHECK(std::abs(retired - eye_world(rig, full).y) > 0.3f); // 0.3711 m off

    // THE FIXED NUMBERS, explicit rather than epsilon-banded (Rule 40). At full
    // crouch the eye drops 0.4716 m — the pelvis fold 0.4419 plus 0.0297 the
    // hunch adds — and advances 0.1643 m, which is the hunch alone. The camera
    // therefore sits at 1.2284 against the drawn eye's 1.2211 and the crouched
    // neck's 1.0978. It used to sit at 0.85.
    const glm::vec2 deep = crouch_eye_offset(p, 1.0f);
    CHECK(std::abs(deep.y - 0.4716f) < 0.0010f);
    CHECK(std::abs(deep.x - 0.1643f) < 0.0010f);
    // Standing is untouched, and clamping holds past both ends so an
    // out-of-range blend cannot invent a squat.
    CHECK(crouch_eye_offset(p, 0.0f).x == doctest::Approx(0.0f));
    CHECK(crouch_eye_offset(p, 0.0f).y == doctest::Approx(0.0f));
    CHECK(crouch_eye_offset(p, 2.0f).y == doctest::Approx(deep.y));
    CHECK(crouch_eye_offset(p, -1.0f).y == doctest::Approx(0.0f));
}

TEST_CASE("the crouch does not make the character stare at the floor") {
    // THE SECOND HALF, and it is only visible on the mirror double and on
    // NPCs: the hunch used to carry the skull through its full -0.25 rad, so a
    // crouched body looked 14.3 deg into the ground. Asserted as the OUTCOME —
    // where the gaze points — not as "the counter-pitch was applied".
    const Rig rig = Rig::build(RigProportions::from_config());
    const auto gaze_pitch_deg = [&](const LocalPose& pose) {
        std::array<glm::mat4, BONE_COUNT> m;
        forward_kinematics(rig, pose, {}, m);
        // The head's own forward (-Z) carried into the world.
        const glm::vec3 fwd = glm::normalize(
            glm::vec3{m[bone_index(Bone::Head)] * glm::vec4{0.0f, 0.0f, -1.0f, 0.0f}});
        return std::asin(glm::clamp(fwd.y, -1.0f, 1.0f)) * (180.0f / glm::pi<float>());
    };
    LocalPose standing;
    CHECK(std::abs(gaze_pitch_deg(standing)) < 0.5f);

    LocalPose crouched;
    apply_crouch(rig, 1.0f, crouched);
    const float deep = gaze_pitch_deg(crouched);
    CHECK(deep < 0.0f);          // still a hunch: the gaze does dip
    CHECK(deep > -6.0f);         // measured -5.73 deg, a look-ahead squat
    // CONTROL (Rule 30): the shipped pose, i.e. the same hunch with the head
    // NOT counter-pitching. It must fail the bound above — and it does, by a
    // factor of 2.5.
    LocalPose unstabilized;
    unstabilized.rotation[bone_index(Bone::Torso)] =
        glm::angleAxis(-0.25f, glm::vec3{1.0f, 0.0f, 0.0f});
    CHECK(gaze_pitch_deg(unstabilized) < -14.0f);
}
