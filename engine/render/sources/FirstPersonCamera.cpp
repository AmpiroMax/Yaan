/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
Module: engine/render
File: engine/render/sources/FirstPersonCamera.cpp

Responsibility:
- FirstPersonCamera implementation: fixed-step pose blending (Rule 12) and
  view/projection construction (right-handed, zero-to-one depth).

Key items:
- interpolated_pose (lerp position, shortest-arc yaw, clamped pitch),
  view/proj, forward/right.

Dependencies:
- Uses: FirstPersonCamera.h, dfn::config (CAMERA_PITCH_LIMIT), glm.
- Used by: dfn_render target.

Notes:
- Depth convention: perspectiveRH_ZO (0..1) — matches Metal/Vulkan/D3D, the
  only bgfx backends we target. A GL backend would need a sync (documented).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Angle conventions are frozen (yaw 0 = -Z, +yaw = turn right, +pitch = up).
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
*/

#include "engine/render/sources/FirstPersonCamera.h"

#include "engine/core/config/sources/Constants.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::render {

namespace {

constexpr float PITCH_LIMIT = static_cast<float>(config::CAMERA_PITCH_LIMIT);

/// Shortest-arc blend of two angles (radians).
float lerp_angle(float a, float b, float alpha) {
    const float two_pi = glm::two_pi<float>();
    float diff = std::fmod(b - a, two_pi);
    if (diff > glm::pi<float>()) {
        diff -= two_pi;
    } else if (diff < -glm::pi<float>()) {
        diff += two_pi;
    }
    return a + diff * alpha;
}

glm::vec3 direction_from(float yaw, float pitch) {
    // Frozen convention: yaw 0 -> -Z, positive yaw turns right (toward +X),
    // positive pitch looks up.
    const float cp = std::cos(pitch);
    return {std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
}

} // namespace

void FirstPersonCamera::set_poses(const CameraPose& previous, const CameraPose& current) {
    prev_ = previous;
    curr_ = current;
}

void FirstPersonCamera::set_projection(float fov_y, float aspect_ratio,
                                       float near_plane, float far_plane) {
    fov_y_ = fov_y;
    aspect_ratio_ = aspect_ratio;
    near_plane_ = near_plane;
    far_plane_ = far_plane;
}

CameraPose FirstPersonCamera::interpolated_pose(float alpha) const {
    const float a = std::clamp(alpha, 0.0f, 1.0f);
    CameraPose pose;
    pose.position = glm::mix(prev_.position, curr_.position, a);
    pose.yaw = lerp_angle(prev_.yaw, curr_.yaw, a);
    pose.pitch = std::clamp(lerp_angle(prev_.pitch, curr_.pitch, a),
                            -PITCH_LIMIT, PITCH_LIMIT);
    return pose;
}

glm::mat4 FirstPersonCamera::view(float alpha) const {
    const CameraPose pose = interpolated_pose(alpha);
    const glm::vec3 dir = direction_from(pose.yaw, pose.pitch);
    return glm::lookAtRH(pose.position, pose.position + dir,
                         glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 FirstPersonCamera::proj() const {
    // Zero-to-one depth for Metal/Vulkan/D3D (see header notes).
    return glm::perspectiveRH_ZO(fov_y_, aspect_ratio_, near_plane_, far_plane_);
}

glm::vec3 FirstPersonCamera::forward(float alpha) const {
    const CameraPose pose = interpolated_pose(alpha);
    return direction_from(pose.yaw, pose.pitch);
}

glm::vec3 FirstPersonCamera::right(float alpha) const {
    const CameraPose pose = interpolated_pose(alpha);
    // Horizontal right vector: rotate forward's yaw by +90 degrees, no pitch.
    return direction_from(pose.yaw + glm::half_pi<float>(), 0.0f);
}

} // namespace dfn::render
