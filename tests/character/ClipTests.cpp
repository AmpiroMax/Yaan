/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 01:56:45
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
*/

#include <doctest/doctest.h>

#include <array>
#include <cmath>

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
[[nodiscard]] float ankle_height(const Rig& rig, Bone foot, float phase,
                                 float step, float phase_shift = 0.0f) {
    const LocalPose pose = gait_pose(rig, phase + phase_shift, step, 0.0f);
    std::array<glm::mat4, BONE_COUNT> m;
    forward_kinematics(rig, pose, {}, m);
    return m[bone_index(foot)][3].y;
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
    float lowest = 1e9f;
    for (int i = 0; i < SAMPLES; ++i) {
        h[static_cast<size_t>(i)] = ankle_height(
            rig, foot, static_cast<float>(i) / SAMPLES, step, phase_shift);
        lowest = std::min(lowest, h[static_cast<size_t>(i)]);
    }
    // 1 mm: the sqrt swing envelope makes approach height LINEAR in phase
    // (~0.5 m per cycle), so a 1 mm epsilon leads the true instant by only
    // ~0.002 of a cycle. A soft (quadratic) approach at a loose epsilon
    // would put the detected edge outside the very tolerance the test
    // asserts — measuring its own epsilon (Rule 36's standing check: when
    // the reading sits near the cutoff, the cutoff is the answer).
    constexpr float contact_eps = 0.001f;
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
    // Single-support walking: one foot's release IS the other's plant.
    CHECK(phase_distance(el.release, right) < TOL);
    CHECK(phase_distance(er.release, left) < TOL);

    // The planted foot then STAYS grounded into stance (the wheel-gait
    // plateau): a fifth of a cycle after touch-down the ankle is still at
    // stance height.
    const float at_plant = ankle_height(rig, Bone::FootL, left, step);
    const float mid_stance = ankle_height(rig, Bone::FootL, left + 0.2f, step);
    CHECK(mid_stance == doctest::Approx(at_plant).epsilon(0.02));

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
                          ankle_height(rig, Bone::FootL,
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
    CHECK(spread(4.0f) == doctest::Approx(spread(3.0f)).epsilon(0.01));
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
    CHECK(m[bone_index(Bone::Pelvis)][3].y
          == doctest::Approx(p.hip_height - expected_drop).epsilon(0.01));
    // Ankles stay near their rest height: the fold is what keeps the feet
    // planted instead of dangling (two-link geometry, not a magic offset).
    CHECK(m[bone_index(Bone::FootL)][3].y
          == doctest::Approx(p.ankle_height).epsilon(0.25));
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
