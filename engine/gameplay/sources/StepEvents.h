/*
Created: 10:08:2026 - 01:53:17
Last updated: 10:08:2026 - 01:53:17
Module: engine/gameplay
File: engine/gameplay/sources/StepEvents.h

Responsibility:
- The step-as-an-event vocabulary (в3): the events the stride cycle publishes
  so that sound, animation and any future consumer land on the SAME tick as
  the camera's bob minimum. The research finding this encodes
  (LIVING_WORLD_RESEARCH.md §D1): delayed footfall feedback destroys the
  walking illusion — one clock, one tick, several consumers.

Key items:
- FootfallEvent: a foot planted (published at the bob-cycle minima,
  FOOTFALL_PHASE_LEFT/RIGHT registry rows).
- Jumped / Landed: takeoff and the grounded edge with measured impact speed.
- WaterEntered: the walk->swim transition (the plunge, not every wet step —
  wet steps are FootfallEvents with `wading` set).

Dependencies:
- Uses: core ecs EntityId, core math SurfaceClass, glm.
- Used by: PlayerMovement (publisher), StepAudio (consumer), character's
  animation layer (consumer, via the bus), tests.

Notes:
- Plain copyable structs (EventBus contract, Rule 8 spirit).
- Events are POSTED (queued); the app pumps within the same fixed tick, after
  post_step — so "same tick" is real, not aspirational.
- surface is math::SurfaceClass under the feet at the plant. Unknown terrain
  (chunk not resident, headless test without a world) reports Grass — the
  soft default is the least wrong sound.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not add a second publisher: the stride clock in PlayerState is THE step
  clock (Rule 35, state form; agreed with character 10:08:2026).
*/
/*
UPD:
- 10:08:2026 - 01:53:17: Created for the landscape stage (шаг как событие).
*/

#pragma once

#include <glm/vec3.hpp>

#include "engine/core/ecs/sources/EntityId.h"
#include "engine/core/math/sources/SurfaceField.h"

namespace dfn::gameplay {

// A foot hit the ground: the bob-cycle minimum. Fired only while grounded and
// actually displacing (a player pushing a wall plants no feet).
struct FootfallEvent {
    ecs::EntityId walker{};
    glm::vec3 position{0.0f};  // feet (capsule bottom) at the plant
    math::SurfaceClass surface = math::SurfaceClass::Grass;
    float speed = 0.0f;        // actual horizontal speed, m/s
    bool left_foot = false;    // FOOTFALL_PHASE_LEFT vs _RIGHT
    bool wading = false;       // water above the feet: the step splashes
};

// Takeoff (the jump was granted, not merely requested).
struct Jumped {
    ecs::EntityId walker{};
    glm::vec3 position{0.0f};
};

// The grounded edge after being airborne. impact_speed is the measured
// downward speed at contact — the landing dip and the land sound both scale
// from it, which is why it is carried rather than recomputed.
struct Landed {
    ecs::EntityId walker{};
    glm::vec3 position{0.0f};
    float impact_speed = 0.0f; // m/s, >= 0
    math::SurfaceClass surface = math::SurfaceClass::Grass;
    bool in_water = false;     // landed into standing water (softened thud)
};

// Walking became swimming (the plunge). Depth is water above the feet.
struct WaterEntered {
    ecs::EntityId walker{};
    glm::vec3 position{0.0f};
    float depth = 0.0f;
};

} // namespace dfn::gameplay
