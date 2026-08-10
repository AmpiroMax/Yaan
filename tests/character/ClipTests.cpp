/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 20:00:23
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
