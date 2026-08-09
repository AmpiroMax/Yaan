/*
Created: 09:08:2026 - 00:15:56
Last updated: 09:08:2026 - 18:58:05
Module: engine/core/components
File: engine/core/components/sources/Components.h

Responsibility:
- Cross-zone plain-data components shared by two or more zones (Rule 25: this
  module is lead-owned; zones propose shapes via message, the lead authors them).

Key items:
- Transform / PreviousTransform: current and previous fixed-step pose (Rule 12
  interpolation pair — sim writes both each tick, render blends with alpha).
- CameraPose / PreviousCameraPose: first-person eye pose published by the
  character controller, consumed by FirstPersonCamera.
- RenderMesh: engine-level asset ids (never platform handles — Rule 8).
- LocalBounds: model-space AABB for culling.

Dependencies:
- Uses: glm only (Rule 2).
- Used by: engine/render (interpolation, culling, submission), engine/physics /
  engine/gameplay (pose writes), engine/world (chunk entity spawning).

Notes:
- Components are plain data (Rule 8): no methods beyond trivial defaults, no
  pointers, no backend types. Asset ids are resolved to platform handles inside
  engine/render, never stored here.
- Two-snapshot layout (Transform + PreviousTransform) chosen over one combined
  struct so systems that do not interpolate touch only Transform (render's
  proposal, approved by lead 09:08:2026).
- LocalBounds keeps raw min/max rather than depending on core/math types; may
  migrate to core::math::Aabb at a group sync if core's type lands first.
- CameraPose uses yaw/pitch radians (Rule 14), position = eye point in meters.
  PENDING sim ACK at the stage-1 group sync.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not add components here yourself — propose to the lead (Rule 25).
*/
/*
UPD:
- 09:08:2026 - 00:15:56: Initial shared component set approved from render's
                         stage-1 proposal (Transform pair, CameraPose pair,
                         RenderMesh, LocalBounds).
- 09:08:2026 - 00:29:27: Added HoverTarget world-resource (sim's Q11 design:
- 09:08:2026 - 18:58:05: HoverTarget gains verb + prompt_key (sim's diff,
                         lead-authored per Rule 26; agreed sim<->render).
                         gameplay raycasts and writes, render reads). Stage-1
                         sync: CameraPose ACKed by sim; LocalBounds stays raw
                         min/max; RenderMesh ids settled as registry-assigned
                         dense ids, not truncated hashes.
*/

#pragma once

#include "engine/core/ecs/sources/EntityId.h"

#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace dfn::components {

// Current pose, written by simulation at each fixed step (meters / radians).
struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

// Pose at the previous fixed step. Sim snapshots Transform into this before
// integrating; render interpolates between the two with the frame alpha.
struct PreviousTransform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

// First-person eye pose published by the character controller (sim zone).
// yaw/pitch in radians (Rule 14). Same prev/curr snapshot discipline.
struct CameraPose {
    glm::vec3 position{0.0f}; // eye point, meters
    float yaw = 0.0f;
    float pitch = 0.0f;
};

struct PreviousCameraPose {
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
};

// Renderable reference by engine-level asset id. engine/render resolves ids to
// platform handles (IRenderer) internally; ids are stable across backend swaps.
struct RenderMesh {
    uint32_t mesh_asset = 0;    // 0 = none
    uint32_t texture_asset = 0; // 0 = untextured
};

// Model-space axis-aligned bounds for culling.
struct LocalBounds {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};

// World RESOURCE (not a per-entity component): the entity the player is
// currently pointing at, or null. Written each fixed tick by gameplay's
// interaction system (raycast via IPhysics, Q11); read by render for the
// hover highlight and by UI for the interaction prompt. Approved at the
// stage-1 sync from sim's design.
struct HoverTarget {
    ecs::EntityId entity{};   // null id = nothing hovered
    uint8_t verb = 0;         // gameplay::InteractionVerb value; 0 = None.
                              // Deliberately a raw integer, not the enum:
                              // render cannot include engine/gameplay (Rule 1),
                              // so gameplay owns the meaning and render only
                              // switches on the value. Without it the reticle
                              // could never change with what is looked at.
    uint64_t prompt_key = 0;  // localization key hash, 0 = none. Inert until a
                              // font exists; landed now rather than reopening a
                              // shared component later for a field already
                              // known to be needed. 64-bit to stay in the one
                              // frozen FNV-1a hash space (Rule 5).
};

} // namespace dfn::components
