/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 12:10:00
Module: tests
File: tests/character/RigPoseTests.cpp

Responsibility:
- Rig + pose math tests: proportion consistency from config, rest-pose FK
  joint heights, yaw convention, mirror involution (with the asymmetric-pose
  control Rule 30 demands), world-plane mirroring, blend endpoints, and the
  segment meshes (every bone non-empty, bounds sane, mesh id arithmetic).

Dependencies:
- Uses: doctest, dfn_anim, constants.
- Used by: ctest (character_rig_pose).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Expectations derive from dfn::config constants, never literal duplicates.
*/
/*
UPD:
- 10:08:2026 - 01:56:45: Initial suite.
- 10:08:2026 - 12:10:00: Stance convergence + hinge-limit suites, each with its control; FK heights now assert the soles on the ground.
*/

#include <doctest/doctest.h>

#include <array>
#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include "engine/anim/sources/BodyMesh.h"
#include "engine/anim/sources/Clips.h"
#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/Rig.h"
#include "engine/core/config/sources/Constants.h"

using namespace dfn;
using namespace dfn::anim;

namespace {

[[nodiscard]] glm::vec3 joint_pos(const std::array<glm::mat4, BONE_COUNT>& m, Bone b) {
    return glm::vec3{m[bone_index(b)][3]};
}

[[nodiscard]] std::array<glm::mat4, BONE_COUNT> fk_rest(const Rig& rig,
                                                        const BodyRoot& root = {}) {
    std::array<glm::mat4, BONE_COUNT> out;
    forward_kinematics(rig, LocalPose{}, root, out);
    return out;
}

} // namespace

TEST_CASE("proportions from config are internally consistent") {
    const RigProportions p = RigProportions::from_config();
    // The leg chain must land back on the hip row: thigh + shin + ankle == hip.
    // This catches a mistyped fraction the day a row changes (each length is
    // derived from TWO rows, so an error cannot cancel).
    CHECK(p.thigh_length() + p.shin_length() + p.ankle_height
          == doctest::Approx(p.hip_height).epsilon(1e-4));
    // Ordering that the rest pose depends on.
    CHECK(p.ankle_height < p.knee_height);
    CHECK(p.knee_height < p.hip_height);
    CHECK(p.hip_height < p.shoulder_height);
    CHECK(p.shoulder_height < p.neck_height);
    // The arm chain must reach below the hip and above the knee on a standing
    // human (canon: fingertips near mid-thigh) — a gross-error tripwire.
    const float wrist =
        p.shoulder_height - p.upper_arm_length - p.forearm_length;
    CHECK(wrist < p.hip_height);
    CHECK(wrist > p.knee_height);
}

TEST_CASE("rest pose FK puts joints at the config heights") {
    const Rig rig = Rig::build(RigProportions::from_config());
    const auto m = fk_rest(rig);
    const auto& p = rig.proportions;

    // THE SOLES STAY ON THE GROUND, which is the assertion the whole leg
    // convergence has to survive: converged legs span less vertical distance,
    // so the pelvis rides standing_hip_height (~7 mm under the anthropometric
    // hip_height) and the ANKLES come out exactly on their row. Checking the
    // hip against hip_height instead would have hidden a floating figure.
    CHECK(joint_pos(m, Bone::Pelvis).y == doctest::Approx(p.standing_hip_height()));
    CHECK(joint_pos(m, Bone::FootL).y == doctest::Approx(p.ankle_height));
    CHECK(joint_pos(m, Bone::FootR).y == doctest::Approx(p.ankle_height));
    CHECK(p.standing_hip_height() < p.hip_height);          // it really does dip
    CHECK(p.hip_height - p.standing_hip_height() < 0.02f);   // ...but only barely
    CHECK(joint_pos(m, Bone::Head).y
          == doctest::Approx(p.standing_hip_height() + p.torso_length()));
    CHECK(joint_pos(m, Bone::UpperArmL).y
          == doctest::Approx(p.standing_hip_height() + p.shoulder_height
                             - p.hip_height));
    // Wrists hang at shoulder minus the arm chain.
    CHECK(joint_pos(m, Bone::HandL).y
          == doctest::Approx(p.standing_hip_height() + p.shoulder_height
                             - p.hip_height - p.upper_arm_length
                             - p.forearm_length));
    // Left is -X at yaw 0 (docs/RIG.md); hips half a hip-width out.
    CHECK(joint_pos(m, Bone::ThighL).x == doctest::Approx(-p.hip_width * 0.5f));
    CHECK(joint_pos(m, Bone::ThighR).x == doctest::Approx(p.hip_width * 0.5f));
}

TEST_CASE("the legs converge to the stance row (user note: feet too far apart)") {
    const auto p = RigProportions::from_config();
    const Rig rig = Rig::build(p);
    const auto m = fk_rest(rig);

    // THE THING THE USER LOOKED AT: how far apart the feet are. The hips stay
    // a full hip-breadth apart at the top, the ankles close to the stance row.
    const float ankles = joint_pos(m, Bone::FootR).x - joint_pos(m, Bone::FootL).x;
    CHECK(ankles == doctest::Approx(p.stance_width).epsilon(1e-3));
    CHECK(ankles < p.hip_width * 0.5f); // it really is a NARROW stance now
    const float hips = joint_pos(m, Bone::ThighR).x - joint_pos(m, Bone::ThighL).x;
    CHECK(hips == doctest::Approx(p.hip_width)); // ...and the pelvis did not shrink

    // THE MODEL CHECK (Rule 30 in the "prove it is not fitted" direction): the
    // convergence is DERIVED from the stance, and it must land in the band real
    // femoral obliquity occupies. A stance row that pushes it outside means hip
    // width or leg length moved without anyone rethinking the model.
    const float deg = p.leg_convergence() * 180.0f / 3.14159265f;
    CHECK(deg > 5.0f);
    CHECK(deg < 12.0f);

    // CONTROL: the rejected instance is the old rig — legs hung straight down
    // from the hip pivots, which is what a stance equal to the hip breadth
    // means. It must produce no convergence and the wide stance the user
    // complained about, so this test would have failed to notice the defect.
    RigProportions wide = p;
    wide.stance_width = wide.hip_width;
    CHECK(wide.leg_convergence() == doctest::Approx(0.0f));
    const auto wm = fk_rest(Rig::build(wide));
    CHECK(joint_pos(wm, Bone::FootR).x - joint_pos(wm, Bone::FootL).x
          == doctest::Approx(wide.hip_width));

    // CONTROL: the feet must not merge into one block. This is the constraint
    // that forced SHIN_ANKLE_TAPER — at the old constant calf width the two
    // legs would intersect at this stance, and nothing else would have said so.
    const auto foot = build_body_segment_mesh(Bone::FootL, p);
    CHECK(foot.bounds_max.x - foot.bounds_min.x < ankles);
    const auto shin = build_body_segment_mesh(Bone::ShinL, p);
    CHECK(shin.bounds_max.x - shin.bounds_min.x < p.hip_width);
}

TEST_CASE("yaw convention matches sim: forward = (sin yaw, 0, -cos yaw)") {
    const Rig rig = Rig::build(RigProportions::from_config());
    // At yaw pi/2 the character faces +X, so its LEFT side (thigh L) points
    // toward -Z... left = -X rotated: facing +X, left is -Z? Compute from the
    // contract instead of prose: left_dir = forward rotated +90deg about Y
    // gives (cos yaw? ) — assert via the hip positions directly.
    const float yaw = glm::half_pi<float>(); // facing +X (east)
    const auto m = fk_rest(rig, BodyRoot{{0.0f, 0.0f, 0.0f}, yaw});
    const glm::vec3 l = joint_pos(m, Bone::ThighL);
    const glm::vec3 r = joint_pos(m, Bone::ThighR);
    // Facing east, the character's right hip is to the SOUTH (+Z) of the left.
    CHECK(r.z > l.z);
    CHECK(std::abs(r.x - l.x) < 1e-5f);

    // Control (Rule 30): at yaw 0 the same assertion on z must FAIL to
    // discriminate — there hips split on x instead.
    const auto m0 = fk_rest(rig);
    CHECK(std::abs(joint_pos(m0, Bone::ThighR).z - joint_pos(m0, Bone::ThighL).z)
          < 1e-5f);
}

TEST_CASE("mirror is an involution, and not the identity") {
    // Asymmetric pose: left arm raised, right knee bent, pelvis swayed. A
    // SYMMETRIC pose passes the involution trivially (mirror == identity on
    // it), so it cannot serve as the control (Rule 30a: this pose can pass
    // the involution while still failing identity — real margin both ways).
    LocalPose pose;
    pose.rotation[bone_index(Bone::UpperArmL)] =
        glm::angleAxis(1.2f, glm::vec3{0.0f, 0.0f, 1.0f});
    pose.rotation[bone_index(Bone::ShinR)] =
        glm::angleAxis(-0.8f, glm::vec3{1.0f, 0.0f, 0.0f});
    pose.rotation[bone_index(Bone::Torso)] =
        glm::angleAxis(0.3f, glm::vec3{0.0f, 1.0f, 0.0f});
    pose.pelvis_offset = {0.04f, -0.02f, 0.01f};

    const LocalPose once = mirror_pose(pose);
    const LocalPose twice = mirror_pose(once);

    bool differs_once = false;
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        const glm::quat orig = pose.rotation[b];
        const glm::quat back = twice.rotation[b];
        CHECK(glm::dot(orig, back) == doctest::Approx(1.0f).epsilon(1e-5));
        if (std::abs(glm::dot(pose.rotation[b], once.rotation[b])) < 0.9999f) {
            differs_once = true;
        }
    }
    CHECK(twice.pelvis_offset.x == doctest::Approx(pose.pelvis_offset.x));
    // The control: one mirror must CHANGE an asymmetric pose.
    CHECK(differs_once);
    CHECK(once.pelvis_offset.x == doctest::Approx(-pose.pelvis_offset.x));
}

TEST_CASE("mirror moves the raised arm to the other side in world space") {
    // The involution above proves self-inverse, not correctness — a mirror
    // that did NOTHING would also pass it. This is the discriminating half:
    // raise the LEFT hand, mirror, and the RIGHT hand must be the high one,
    // at the mirrored x.
    const Rig rig = Rig::build(RigProportions::from_config());
    LocalPose pose;
    pose.rotation[bone_index(Bone::UpperArmL)] =
        glm::angleAxis(-2.0f, glm::vec3{0.0f, 0.0f, 1.0f}); // left arm up/out
    std::array<glm::mat4, BONE_COUNT> m;
    forward_kinematics(rig, pose, {}, m);
    std::array<glm::mat4, BONE_COUNT> mm;
    forward_kinematics(rig, mirror_pose(pose), {}, mm);

    const glm::vec3 hand_l = glm::vec3{m[bone_index(Bone::HandL)][3]};
    const glm::vec3 hand_r_mirrored = glm::vec3{mm[bone_index(Bone::HandR)][3]};
    CHECK(hand_r_mirrored.y == doctest::Approx(hand_l.y).epsilon(1e-4));
    CHECK(hand_r_mirrored.x == doctest::Approx(-hand_l.x).epsilon(1e-4));
    // And the mirrored pose's LEFT hand hangs low (the raise moved sides).
    const glm::vec3 hand_l_mirrored = glm::vec3{mm[bone_index(Bone::HandL)][3]};
    CHECK(hand_l_mirrored.y < hand_l.y - 0.2f);
}

TEST_CASE("world mirror: reflection across the plane, twice = identity") {
    const glm::vec3 plane_pt{3.0f, 0.0f, -2.0f};
    const glm::vec2 n{0.0f, 1.0f}; // plane faces +Z
    const glm::vec3 p{1.0f, 1.3f, 4.0f};
    const glm::vec3 r = mirror_point(p, plane_pt, n);
    CHECK(r.x == doctest::Approx(p.x));
    CHECK(r.y == doctest::Approx(p.y));
    CHECK(r.z == doctest::Approx(2.0f * plane_pt.z - p.z));
    const glm::vec3 rr = mirror_point(r, plane_pt, n);
    CHECK(rr.z == doctest::Approx(p.z));

    // Yaw: walking toward the mirror comes back walking toward you.
    const float toward = 0.0f; // facing -Z
    const float back = mirror_yaw(toward, n);
    const glm::vec3 fwd{std::sin(back), 0.0f, -std::cos(back)};
    CHECK(fwd.z == doctest::Approx(1.0f).epsilon(1e-5));
    CHECK(mirror_yaw(back, n) == doctest::Approx(toward).epsilon(1e-5));
}

TEST_CASE("blend endpoints return the inputs") {
    LocalPose a; // rest
    LocalPose b;
    b.rotation[bone_index(Bone::Head)] =
        glm::angleAxis(0.7f, glm::vec3{0.0f, 1.0f, 0.0f});
    b.pelvis_offset = {0.0f, -0.1f, 0.0f};
    const LocalPose at0 = blend(a, b, 0.0f);
    const LocalPose at1 = blend(a, b, 1.0f);
    CHECK(glm::dot(at0.rotation[bone_index(Bone::Head)],
                   a.rotation[bone_index(Bone::Head)])
          == doctest::Approx(1.0f));
    CHECK(glm::dot(at1.rotation[bone_index(Bone::Head)],
                   b.rotation[bone_index(Bone::Head)])
          == doctest::Approx(1.0f));
    CHECK(at1.pelvis_offset.y == doctest::Approx(-0.1f));
}

TEST_CASE("every bone builds a non-empty segment mesh with sane bounds") {
    const RigProportions p = RigProportions::from_config();
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        const auto bone = static_cast<Bone>(b);
        CAPTURE(bone_name(bone));
        const BodySegmentMesh mesh = build_body_segment_mesh(bone, p);
        // A gap here is an invisible limb (the missing-castle defect class).
        CHECK(mesh.indices.size() >= 36); // at least one box
        CHECK(mesh.vertices.size() >= 24);
        CHECK(mesh.bounds_min.x < mesh.bounds_max.x);
        CHECK(mesh.bounds_min.y < mesh.bounds_max.y);
        CHECK(mesh.bounds_min.z < mesh.bounds_max.z);
        // No segment is bigger than the whole body (gross-scale tripwire).
        const glm::vec3 size = mesh.bounds_max - mesh.bounds_min;
        CHECK(size.x < static_cast<float>(config::PLAYER_CAPSULE_HEIGHT));
        CHECK(size.y < static_cast<float>(config::PLAYER_CAPSULE_HEIGHT));
        CHECK(size.z < static_cast<float>(config::PLAYER_CAPSULE_HEIGHT));
    }
    // Mesh id arithmetic pins the range agreed with render (34..48, spare 49).
    CHECK(body_segment_mesh_id(Bone::Pelvis) == 34);
    CHECK(body_segment_mesh_id(Bone::FootR) == 48);
}

TEST_CASE("hinges are hinges: knees and elbows cannot bend backwards") {
    const Rig rig = Rig::build(RigProportions::from_config());
    const auto knee_lo = -static_cast<float>(config::BODY_KNEE_FLEX_MAX);
    const auto knee_hi = static_cast<float>(config::BODY_KNEE_HYPEREXT_MAX);
    const auto elbow_lo = -static_cast<float>(config::BODY_ELBOW_HYPEREXT_MAX);
    const auto elbow_hi = static_cast<float>(config::BODY_ELBOW_FLEX_MAX);
    const auto ax = glm::vec3{1.0f, 0.0f, 0.0f};
    const auto angle_of = [](const glm::quat& q) {
        const float w = q.w < 0.0f ? -q.w : q.w;
        const float x = q.w < 0.0f ? -q.x : q.x;
        return 2.0f * std::atan2(x, w);
    };

    // CONTROL (Rule 30), and it is the user's actual complaint: a limb bent the
    // wrong way. Both joints, both directions — a range is two assertions, and
    // the two joints run in OPPOSITE senses, so a single-sign implementation
    // would pass one of these and fail the other.
    LocalPose bad;
    bad.rotation[bone_index(Bone::ShinL)] = glm::angleAxis(1.0f, ax);   // knee back
    bad.rotation[bone_index(Bone::ForearmL)] = glm::angleAxis(-1.0f, ax); // elbow back
    bad.rotation[bone_index(Bone::ShinR)] = glm::angleAxis(-3.0f, ax);  // knee folded past the heel
    bad.rotation[bone_index(Bone::ForearmR)] = glm::angleAxis(3.0f, ax); // elbow past the shoulder
    // Before: every one of these is out of range, i.e. the check can fail.
    CHECK(angle_of(bad.rotation[bone_index(Bone::ShinL)]) > knee_hi);
    CHECK(angle_of(bad.rotation[bone_index(Bone::ForearmL)]) < elbow_lo);
    CHECK(angle_of(bad.rotation[bone_index(Bone::ShinR)]) < knee_lo);
    CHECK(angle_of(bad.rotation[bone_index(Bone::ForearmR)]) > elbow_hi);

    apply_joint_limits(rig, bad);
    CHECK(angle_of(bad.rotation[bone_index(Bone::ShinL)]) <= knee_hi + 1e-5f);
    CHECK(angle_of(bad.rotation[bone_index(Bone::ForearmL)]) >= elbow_lo - 1e-5f);
    CHECK(angle_of(bad.rotation[bone_index(Bone::ShinR)]) >= knee_lo - 1e-5f);
    CHECK(angle_of(bad.rotation[bone_index(Bone::ForearmR)]) <= elbow_hi + 1e-5f);

    // A hinge has ONE axis. Yaw and roll handed to a knee are not reduced, they
    // are removed: a knee that can twist is its own kind of creepy.
    LocalPose twisted;
    twisted.rotation[bone_index(Bone::ShinL)] =
        glm::angleAxis(0.8f, glm::normalize(glm::vec3{0.3f, 1.0f, 0.4f}));
    apply_joint_limits(rig, twisted);
    const glm::quat k = twisted.rotation[bone_index(Bone::ShinL)];
    CHECK(std::abs(k.y) < 1e-5f);
    CHECK(std::abs(k.z) < 1e-5f);

    // Free bones are left alone — the limit must not quietly flatten a
    // shoulder, which is a ball joint and has to stay one.
    LocalPose free_pose;
    const glm::quat shoulder =
        glm::angleAxis(0.7f, glm::normalize(glm::vec3{0.2f, 0.5f, 1.0f}));
    free_pose.rotation[bone_index(Bone::UpperArmL)] = shoulder;
    apply_joint_limits(rig, free_pose);
    CHECK(free_pose.rotation[bone_index(Bone::UpperArmL)].y
          == doctest::Approx(shoulder.y));

    // Idempotent: a legal pose survives the clamp untouched, so applying it at
    // several layers cannot drift a clip.
    LocalPose once = bad;
    apply_joint_limits(rig, once);
    CHECK(angle_of(once.rotation[bone_index(Bone::ShinR)])
          == doctest::Approx(angle_of(bad.rotation[bone_index(Bone::ShinR)])));
}

TEST_CASE("every shipped clip stays inside the joint limits") {
    const Rig rig = Rig::build(RigProportions::from_config());
    const auto knee_hi = static_cast<float>(config::BODY_KNEE_HYPEREXT_MAX);
    const auto elbow_lo = -static_cast<float>(config::BODY_ELBOW_HYPEREXT_MAX);
    const float step = static_cast<float>(config::STEP_LENGTH_BASE)
                     + static_cast<float>(config::STEP_LENGTH_PER_MPS)
                           * static_cast<float>(config::WALK_SPEED);
    const auto angle_of = [](const glm::quat& q) {
        const float w = q.w < 0.0f ? -q.w : q.w;
        const float x = q.w < 0.0f ? -q.x : q.x;
        return 2.0f * std::atan2(x, w);
    };
    // The clips must be HONEST, not merely corrected: these are the raw poses,
    // before the rig's clamp is applied. The walk clip failed this by 33.4 deg
    // when the user reported it, and the fix was the forefoot rocker, not a
    // wider limit.
    for (int i = 0; i <= 64; ++i) {
        const float t = static_cast<float>(i) / 64.0f;
        const LocalPose clips[] = {gait_pose(rig, t, step, 0.0f),
                                   gait_pose(rig, t, step, 1.0f),
                                   idle_pose(t * 6.0f), wave_pose(t * 4.0f),
                                   flex_pose(t * 4.0f), air_pose(8.0f * (t - 0.5f))};
        for (const LocalPose& p : clips) {
            CHECK(angle_of(p.rotation[bone_index(Bone::ShinL)]) <= knee_hi + 1e-4f);
            CHECK(angle_of(p.rotation[bone_index(Bone::ShinR)]) <= knee_hi + 1e-4f);
            CHECK(angle_of(p.rotation[bone_index(Bone::ForearmL)]) >= elbow_lo - 1e-4f);
            CHECK(angle_of(p.rotation[bone_index(Bone::ForearmR)]) >= elbow_lo - 1e-4f);
        }
    }
}
