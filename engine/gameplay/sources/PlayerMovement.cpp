/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 17:08:40
Module: engine/gameplay
File: engine/gameplay/sources/PlayerMovement.cpp

Responsibility:
- Ref-based player movement core: input accumulation, look integration with
  pitch clamp, gravity, displacement submission, post-step component writes.
  No ECS dependency in this TU (World wrappers live in PlayerMovementWorld.cpp).

Key items:
- accumulate_input / player_pre_step / player_post_step (ref overloads).

Dependencies:
- Uses: PlayerMovement.h, generated constants (dfn::config), glm.
- Used by: PlayerMovementWorld.cpp wrappers, tests (null physics).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every tuning number comes from dfn::config (Rule 14) — no literals here.
- Conventions (yaw/pitch/axes) are documented in the header; keep in sync.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial implementation.
- 09:08:2026 - 17:08:40: DEBUG CONVENIENCE (user request): Shift now sprints at
                         RUN_SPEED * DEBUG_SPRINT_MULTIPLIER (30 m/s) for
                         crossing the valley on foot. RUN_SPEED unchanged.
                         Revisit at the movement/combat grill.
*/

#include "engine/gameplay/sources/PlayerMovement.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include "engine/core/config/sources/Constants.h"

namespace dfn::gameplay {

namespace {

// NUMBERS constants, narrowed once (generated header emits doubles).
constexpr float DT = static_cast<float>(config::SIM_DT);
constexpr float GRAVITY = static_cast<float>(config::GRAVITY);
constexpr float WALK_SPEED = static_cast<float>(config::WALK_SPEED);
// DEBUG CONVENIENCE (user request, 09:08:2026): Shift sprints at
// RUN_SPEED * DEBUG_SPRINT_MULTIPLIER = 30 m/s so the valley can be crossed in
// seconds while it is being built out. RUN_SPEED itself stays the game-design
// value (6 m/s) and must not be touched. REVISIT at the movement/combat grill —
// this must NOT ship as the release feel.
constexpr float SPRINT_SPEED =
    static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);
constexpr float MOUSE_SENSITIVITY = static_cast<float>(config::MOUSE_SENSITIVITY);
constexpr float PITCH_LIMIT = static_cast<float>(config::CAMERA_PITCH_LIMIT);
constexpr float EYE_HEIGHT = static_cast<float>(config::PLAYER_EYE_HEIGHT);

} // namespace

void accumulate_input(const platform::IInput& input, PlayerState& state) {
    // Look accumulates across frames between fixed ticks (no lost motion).
    state.pending_look += input.mouse_delta();

    // Key axes: latest sample wins. +y forward (W), +x strafe right (D).
    glm::vec2 axes{0.0f};
    if (input.is_down(platform::Key::W)) {
        axes.y += 1.0f;
    }
    if (input.is_down(platform::Key::S)) {
        axes.y -= 1.0f;
    }
    if (input.is_down(platform::Key::D)) {
        axes.x += 1.0f;
    }
    if (input.is_down(platform::Key::A)) {
        axes.x -= 1.0f;
    }
    state.move_axes = axes;
    state.run = input.is_down(platform::Key::LEFT_SHIFT) ||
                input.is_down(platform::Key::RIGHT_SHIFT);
}

void player_pre_step(PlayerState& state, platform::IPhysics& physics,
                     const components::Transform& transform,
                     components::PreviousTransform& prev_transform,
                     const components::CameraPose& camera,
                     components::PreviousCameraPose& prev_camera) {
    // 1. Snapshot discipline (Rule 12 contract): prev <- curr, before anything.
    prev_transform.position = transform.position;
    prev_transform.rotation = transform.rotation;
    prev_transform.scale = transform.scale;
    prev_camera.position = camera.position;
    prev_camera.yaw = camera.yaw;
    prev_camera.pitch = camera.pitch;

    // 2. Look: mouse +x -> +yaw (turn right), mouse +y -> -pitch (look down).
    state.yaw += state.pending_look.x * MOUSE_SENSITIVITY;
    state.pitch = std::clamp(state.pitch - state.pending_look.y * MOUSE_SENSITIVITY,
                             -PITCH_LIMIT, PITCH_LIMIT);
    state.pending_look = glm::vec2{0.0f};

    // 3. Horizontal intent in world space (conventions: header notes).
    const glm::vec3 forward{std::sin(state.yaw), 0.0f, -std::cos(state.yaw)};
    const glm::vec3 right{std::cos(state.yaw), 0.0f, std::sin(state.yaw)};
    glm::vec2 axes = state.move_axes;
    if (const float len = glm::length(axes); len > 1.0f) {
        axes /= len; // diagonals are not faster
    }
    // DEBUG: run input sprints (see SPRINT_SPEED above), not RUN_SPEED.
    const float speed = state.run ? SPRINT_SPEED : WALK_SPEED;
    glm::vec3 displacement = (right * axes.x + forward * axes.y) * speed * DT;

    // 4. Gravity (backend only collides-and-slides; vertical motion is ours).
    state.vertical_velocity -= GRAVITY * DT;
    displacement.y = state.vertical_velocity * DT;

    physics.move_character(state.character, displacement);
}

void player_post_step(PlayerState& state, platform::IPhysics& physics,
                      components::Transform& transform,
                      components::CameraPose& camera) {
    const glm::vec3 position = physics.character_position(state.character);
    if (physics.character_grounded(state.character) && state.vertical_velocity < 0.0f) {
        state.vertical_velocity = 0.0f;
    }

    transform.position = position;
    // Body faces yaw: default forward is -Z; math rotation about +Y is CCW from
    // above, our yaw is CW, hence the negation (header conventions).
    transform.rotation = glm::angleAxis(-state.yaw, glm::vec3{0.0f, 1.0f, 0.0f});

    camera.position = position + glm::vec3{0.0f, EYE_HEIGHT, 0.0f};
    camera.yaw = state.yaw;
    camera.pitch = state.pitch;
}

} // namespace dfn::gameplay
