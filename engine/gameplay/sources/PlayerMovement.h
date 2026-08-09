/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 00:45:08
Module: engine/gameplay
File: engine/gameplay/sources/PlayerMovement.h

Responsibility:
- First-person player movement: mouse look (pitch clamp), WASD + run intent,
  gravity, driving the IPhysics character, and publishing the fixed-tick
  Transform/CameraPose snapshot pairs render interpolates (Rule 12; contract
  ACKed with render and the lead at the stage-1 sync).

Key items:
- PlayerState: per-player component (Rule 8) — character handle, look angles,
  vertical velocity, accumulated input intent.
- accumulate_input / player_pre_step / player_post_step: ref-based core,
  unit-testable with the null backend, no World required.
- spawn_player + World-facing wrappers: what engine/app calls.

Dependencies:
- Uses: core components (lead-owned pairs), platform IInput/IPhysics
  interfaces, core ecs (EntityId; World forward-declared), generated constants.
- Used by: engine/app fixed-tick loop, tests.

Notes:
- Tick order (agreed with the lead): render frame -> player_accumulate_input
  (after IInput::update); each fixed tick -> player_pre_step, THEN the app
  calls IPhysics::step(SIM_DT), THEN player_post_step. Movement code never
  calls step() itself.
- Conventions (render sync): right-handed Y-up, +X east, +Z south; yaw 0
  faces -Z, positive yaw = clockwise from above; positive pitch = look up;
  forward = (sin yaw, 0, -cos yaw). Mouse +x -> +yaw, mouse +y -> -pitch.
- All numbers from dfn::config (Rule 14): WALK/RUN_SPEED, GRAVITY,
  MOUSE_SENSITIVITY, CAMERA_PITCH_LIMIT, PLAYER_* capsule constants.
- Look pixels ACCUMULATE across render frames between fixed ticks (no lost
  mouse motion when render outpaces sim); key axes are latest-sampled.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Snapshot discipline is contract: pre_step copies curr->prev for BOTH pairs
  before any mutation; only post_step writes new curr values.
- The PLAYER is not an NPC: NpcAction (Rule 15) governs NPCs; this system is
  the player-input path and never touches NPC state.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial movement contract + implementation.
*/

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "engine/core/components/sources/Components.h"
#include "engine/core/ecs/sources/EntityId.h"
#include "engine/platform/input/interfaces/IInput.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::ecs {
class World;
}

namespace dfn::gameplay {

// Per-player component (Rule 8: plain data). Holds controller state at the
// fixed tick plus the input intent accumulated since the previous tick.
struct PlayerState {
    platform::CharacterHandle character{};
    float yaw = 0.0f;               // radians; 0 = -Z, positive = clockwise from above
    float pitch = 0.0f;             // radians; positive = up, clamped by CAMERA_PITCH_LIMIT
    float vertical_velocity = 0.0f; // m/s (gravity integration)
    glm::vec2 pending_look{0.0f};   // pixels accumulated since the last fixed tick
    glm::vec2 move_axes{0.0f};      // x = +strafe right, y = +forward; each in [-1, 1]
    bool run = false;
};

// --- Ref-based core (unit-testable without a World) --------------------------

// Once per render frame, after input.update(): accumulates mouse delta into
// pending_look, samples WASD into move_axes, shift into run.
void accumulate_input(const platform::IInput& input, PlayerState& state);

// Fixed tick, BEFORE IPhysics::step: snapshots curr->prev on both pairs,
// applies look, integrates gravity, submits the displacement via move_character.
void player_pre_step(PlayerState& state, platform::IPhysics& physics,
                     const components::Transform& transform,
                     components::PreviousTransform& prev_transform,
                     const components::CameraPose& camera,
                     components::PreviousCameraPose& prev_camera);

// Fixed tick, AFTER IPhysics::step: reads back position/grounded and writes
// the new current Transform + CameraPose (eye = bottom + PLAYER_EYE_HEIGHT).
void player_post_step(PlayerState& state, platform::IPhysics& physics,
                      components::Transform& transform,
                      components::CameraPose& camera);

// --- World-facing API (what engine/app calls) --------------------------------

// Spawns the player entity with PlayerState + Transform/PreviousTransform +
// CameraPose/PreviousCameraPose and creates the IPhysics capsule from the
// PLAYER_* constants. spawn_pos is the capsule BOTTOM point.
[[nodiscard]] ecs::EntityId spawn_player(ecs::World& world, platform::IPhysics& physics,
                                         const glm::vec3& spawn_pos);

void player_accumulate_input(ecs::World& world, const platform::IInput& input);
void player_pre_step(ecs::World& world, platform::IPhysics& physics);
void player_post_step(ecs::World& world, platform::IPhysics& physics);

} // namespace dfn::gameplay
