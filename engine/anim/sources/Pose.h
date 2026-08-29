/*
Module: engine/anim
File: engine/anim/sources/Pose.h

Responsibility:
- Pose math on the humanoid rig: local pose (per-bone rotations relative to
  rest + pelvis offset), forward kinematics to world transforms, mirroring
  (the mirror map's core), and weighted blending.

Key items:
- LocalPose: quat per bone + pelvis translation offset; identity = rest.
- forward_kinematics(): rig + pose + root (ground point, yaw) -> one mat4 per
  bone; a segment entity's Transform is exactly this matrix.
- mirror_pose(): sagittal mirror + L/R swap. Involution (tested with an
  asymmetric-pose control, Rule 30).
- mirror_point/mirror_yaw(): world-space reflection across the mirror plane.
- blend(): weighted slerp toward a second pose.

Dependencies:
- Uses: Rig.h, glm.
- Used by: Clips, Body, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Conventions are docs/RIG.md and are contract: yaw 0 faces -Z, +X is the
  character's right, mirror quat map is (w,x,y,z) -> (w,x,-y,-z).
*/

#pragma once

#include "engine/anim/sources/Rig.h"

#include <array>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <span>

namespace dfn::anim {

// Rotations are RELATIVE TO REST (identity everywhere = standing).
// pelvis_offset is the root bob/sway in character-local meters (x = toward the
// character's right at yaw 0, y = up, z = toward its back).
struct LocalPose {
    std::array<glm::quat, BONE_COUNT> rotation{};
    glm::vec3 pelvis_offset{0.0f};

    LocalPose() { rotation.fill(glm::quat{1.0f, 0.0f, 0.0f, 0.0f}); }
};

// Root placement of a body in the world: `ground` is the point under the
// pelvis (the owner entity's Transform.position = capsule bottom), yaw per
// sim's convention (forward = (sin yaw, 0, -cos yaw)).
struct BodyRoot {
    glm::vec3 ground{0.0f};
    float yaw = 0.0f;
};

// One world matrix per bone (index = Bone). out.size() must be >= BONE_COUNT.
void forward_kinematics(const Rig& rig, const LocalPose& pose, const BodyRoot& root,
                        std::span<glm::mat4> out);

// Sagittal mirror: swap L/R bones, (w,x,y,z) -> (w,x,-y,-z) on every rotation,
// negate pelvis_offset.x. mirror_pose(mirror_pose(p)) == p.
// HINGES ARE HINGES. Reduces every bone the rig marks as a hinge to a pure
// rotation about its own X axis and clamps it into the bone's range, in place;
// free bones are untouched. Called at the END of pose evaluation, so no clip —
// present or future, authored or blended — can produce a limb bent the wrong
// way. Idempotent: applying it twice changes nothing.
void apply_joint_limits(const Rig& rig, LocalPose& pose);

[[nodiscard]] LocalPose mirror_pose(const LocalPose& pose);

// World-space reflection across the vertical mirror plane through plane_point
// with horizontal unit normal plane_normal_xz (x,z components).
[[nodiscard]] glm::vec3 mirror_point(const glm::vec3& point, const glm::vec3& plane_point,
                                     const glm::vec2& plane_normal_xz);
[[nodiscard]] float mirror_yaw(float yaw, const glm::vec2& plane_normal_xz);

// result = slerp(a, b, weight) per bone, lerp on pelvis_offset. weight is
// clamped to [0, 1].
[[nodiscard]] LocalPose blend(const LocalPose& a, const LocalPose& b, float weight);

} // namespace dfn::anim
