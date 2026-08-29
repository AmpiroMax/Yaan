/*
Module: engine/anim
File: engine/anim/sources/Pose.cpp

Responsibility:
- FK, mirroring and blending implementations (contract in Pose.h/docs/RIG.md).

Dependencies:
- Uses: Pose.h, glm.
- Used by: Clips, Body, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- FK is a single forward pass and depends on BONE_PARENT listing parents
  before children — preserve that if the enum ever grows.
*/

#include "engine/anim/sources/Pose.h"

#include <cassert>
#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace dfn::anim {

void forward_kinematics(const Rig& rig, const LocalPose& pose, const BodyRoot& root,
                        std::span<glm::mat4> out) {
    assert(out.size() >= BONE_COUNT);

    // Root: lift the ground point to the hip center, apply bob/sway, then yaw.
    // Yaw rotation about +Y: sim's convention (yaw 0 -> -Z) equals a rotation
    // of -yaw around +Y in the right-handed frame... verified by test: at yaw 0
    // forward is -Z; positive yaw turns clockwise seen from above (+X first).
    // standing_hip_height(), not hip_height: converged legs span less vertical
    // distance than straight ones, so the pelvis rides ~7 mm lower. Taking the
    // compensation here is what keeps the SOLES on the ground — a straight
    // hip_height lift would float the whole figure by that much.
    const glm::vec3 pelvis_world =
        root.ground + glm::vec3{0.0f, rig.proportions.standing_hip_height(), 0.0f};
    glm::mat4 root_m = glm::translate(glm::mat4{1.0f}, pelvis_world);
    root_m = glm::rotate(root_m, -root.yaw, glm::vec3{0.0f, 1.0f, 0.0f});
    root_m = glm::translate(root_m, pose.pelvis_offset);

    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        const glm::mat4 local = glm::translate(glm::mat4{1.0f}, rig.rest_offset[b])
                              * glm::mat4_cast(rig.rest_rotation[b])
                              * glm::mat4_cast(pose.rotation[b]);
        const int8_t parent = BONE_PARENT[b];
        out[b] = (parent < 0 ? root_m : out[static_cast<uint32_t>(parent)]) * local;
    }
}

void apply_joint_limits(const Rig& rig, LocalPose& pose) {
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        const glm::vec2 range = rig.hinge_range[b];
        if (!std::isfinite(range.x) || !std::isfinite(range.y)) {
            continue; // a free bone: shoulders, hips, torso, head, ankles
        }
        // Swing-twist about X, keeping only the twist: this is what makes a
        // hinge a hinge. A knee handed a rotation with yaw or roll in it does
        // not get a "mostly correct" knee, it gets no yaw or roll at all.
        const glm::quat& q = pose.rotation[b];
        // q and -q are the same rotation; pick the w>=0 representative so the
        // extracted angle lands in (-pi, pi] instead of wrapping to its
        // complement and clamping to the wrong end of the range.
        const float w = q.w < 0.0f ? -q.w : q.w;
        const float x = q.w < 0.0f ? -q.x : q.x;
        const float angle = 2.0f * std::atan2(x, w);
        const float clamped = glm::clamp(angle, range.x, range.y);
        pose.rotation[b] = glm::angleAxis(clamped, glm::vec3{1.0f, 0.0f, 0.0f});
    }
}

LocalPose mirror_pose(const LocalPose& pose) {
    LocalPose m;
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        const glm::quat& q = pose.rotation[MIRROR_BONE[b]];
        // Reflection across the local YZ plane: S * R * S with S = diag(-1,1,1).
        m.rotation[b] = glm::quat{q.w, q.x, -q.y, -q.z};
    }
    m.pelvis_offset = {-pose.pelvis_offset.x, pose.pelvis_offset.y, pose.pelvis_offset.z};
    return m;
}

glm::vec3 mirror_point(const glm::vec3& point, const glm::vec3& plane_point,
                       const glm::vec2& plane_normal_xz) {
    const glm::vec3 n{plane_normal_xz.x, 0.0f, plane_normal_xz.y};
    const float d = glm::dot(point - plane_point, n);
    return point - 2.0f * d * n;
}

float mirror_yaw(float yaw, const glm::vec2& plane_normal_xz) {
    const glm::vec3 n{plane_normal_xz.x, 0.0f, plane_normal_xz.y};
    const glm::vec3 fwd{std::sin(yaw), 0.0f, -std::cos(yaw)};
    const glm::vec3 m = fwd - 2.0f * glm::dot(fwd, n) * n;
    return std::atan2(m.x, -m.z);
}

LocalPose blend(const LocalPose& a, const LocalPose& b, float weight) {
    const float w = glm::clamp(weight, 0.0f, 1.0f);
    LocalPose r;
    for (uint32_t i = 0; i < BONE_COUNT; ++i) {
        r.rotation[i] = glm::slerp(a.rotation[i], b.rotation[i], w);
    }
    r.pelvis_offset = glm::mix(a.pelvis_offset, b.pelvis_offset, w);
    return r;
}

} // namespace dfn::anim
