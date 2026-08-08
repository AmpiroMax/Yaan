/*
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 00:22:00
Module: engine/render
File: engine/render/sources/FirstPersonCamera.h

Responsibility:
- First-person camera: yaw/pitch pose, view/projection matrix construction, and
  interpolation between fixed simulation steps (Rule 12).

Key items:
- FirstPersonCamera: set_poses (prev/curr CameraPose snapshots from sim),
  set_projection, view(alpha) / proj() for IRenderer::begin_frame.

Dependencies:
- Uses: engine/core/components (shared CameraPose), glm. No platform headers.
- Used by: RenderSystem, the tour harness (drives the pose directly),
  engine/app (feeds sim poses + render alpha), engine/editor.

Notes:
- Interpolation contract (agreed with lead + core, relayed to sim, stage 1): the
  sim character controller publishes CameraPose + PreviousCameraPose (shared
  components, lead-owned) each fixed step; the app hands both snapshots here.
  This class stores snapshots only — it never reads the clock or the ECS.
- alpha in [0,1] = fixed-step accumulator fraction, computed by the app loop.
  Position lerps; yaw interpolates over the shortest arc; pitch lerps and is
  clamped (limit constant to be fixed in NUMBERS.md at the sync).
- Projection defaults CAMERA_FOV_Y / CAMERA_NEAR / CAMERA_FAR are provisional
  in NUMBERS.md; until the generated constants header exists, set_projection is
  mandatory before proj().
- Right-handed, Y up, +X east, +Z south; yaw 0 looks toward -Z, positive yaw
  turns right (clockwise seen from above); positive pitch looks up.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Public contract, frozen for the stage (Rule 26): changes only via group sync.
- Keep this class free of input handling; mouse-look lives in the controller.
*/
/*
UPD:
- 09:08:2026 - 00:16:00: Initial stage-1 contract (render zone).
- 09:08:2026 - 00:22:00: Local CameraPose removed in favour of the lead-authored
  shared component (engine/core/components); provisional NUMBERS.md projection
  constants referenced.
*/

#pragma once

#include "engine/core/components/sources/Components.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace dfn::render {

// The camera consumes the shared fixed-step snapshot components authored by the
// lead (proposed by render, stage 1). Radians, meters (Rule 14).
using components::CameraPose;

class FirstPersonCamera {
public:
    // Fixed-step snapshots from the sim (previous and current). Called once per
    // fixed step by the app; render frames between steps reuse the same pair.
    // `previous` comes from PreviousCameraPose — same field layout.
    void set_poses(const CameraPose& previous, const CameraPose& current);

    // Perspective parameters. fov_y in radians, planes in meters.
    void set_projection(float fov_y, float aspect_ratio, float near_plane, float far_plane);

    // Pose blended at alpha in [0,1] between the previous and current snapshots
    // (Rule 12); yaw over the shortest arc, pitch clamped.
    [[nodiscard]] CameraPose interpolated_pose(float alpha) const;

    // Matrices for IRenderer::begin_frame.
    [[nodiscard]] glm::mat4 view(float alpha) const;
    [[nodiscard]] glm::mat4 proj() const;

    // Basis of the interpolated pose — for audio listeners, ray picking, culling.
    [[nodiscard]] glm::vec3 forward(float alpha) const;
    [[nodiscard]] glm::vec3 right(float alpha) const;

    [[nodiscard]] float fov_y() const { return fov_y_; }
    [[nodiscard]] float aspect_ratio() const { return aspect_ratio_; }
    [[nodiscard]] float near_plane() const { return near_plane_; }
    [[nodiscard]] float far_plane() const { return far_plane_; }

private:
    CameraPose prev_{};
    CameraPose curr_{};
    float fov_y_ = 0.0f;        // radians; must be set via set_projection
    float aspect_ratio_ = 1.0f;
    float near_plane_ = 0.0f;   // meters
    float far_plane_ = 0.0f;    // meters
};

} // namespace dfn::render
