/*
Created: 09:08:2026 - 18:56:32
Last updated: 09:08:2026 - 18:56:32
Module: engine/gameplay
File: engine/gameplay/sources/InteractableSpawn.h

Responsibility:
- Spawning interactable props into the world with their collision body, so the
  crosshair ray can find them: doors, chests, levers and loose items.

Key items:
- InteractableDesc: what to spawn (kind + placement + payload).
- spawn_interactable(): one prop, wired to physics and the ECS.

Dependencies:
- Uses: core ecs, core components (Transform), platform IPhysics,
  engine/physics collision layers, Interaction.h.
- Used by: engine/app (placing this stage's proving content), the content
  loader once core's JSON reader lands, tests.

Notes:
- Every prop gets a static box on LAYER_INTERACTABLE whose user_data is the
  entity's packed id — that is how a ray hit becomes an entity again. Props are
  NOT on LAYER_STATIC: the player walks through the ray, not into the prop, and
  making them block movement is a separate decision (a door you cannot walk
  through needs its collision to move with its open state, which is animation
  work this stage does not have).
- Content placement is data (Rule 5): this header is the mechanism, the
  positions and payloads come from the content file.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not add a second spawn path; the loader and the app both call this.
*/
/*
UPD:
- 09:08:2026 - 18:56:32: Initial interactable spawning.
*/

#pragma once

#include <cstdint>
#include <string>

#include <glm/vec3.hpp>

#include "engine/core/ecs/sources/EntityId.h"
#include "engine/gameplay/sources/Ids.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::ecs {
class World;
}

namespace dfn::gameplay {

enum class InteractableKind : uint8_t {
    Pickup,   // loose item -> TAKE
    Openable, // door, chest -> OPEN/CLOSE
    Usable,   // lever, campfire, bed -> USE
};

struct InteractableDesc {
    InteractableKind kind = InteractableKind::Pickup;
    glm::vec3 position{0.0f};      // world space, meters
    glm::vec3 half_extents{0.25f}; // collision box for the crosshair ray
    std::string prompt_key;        // localization key (Rule 5)

    // Pickup payload.
    ItemId item{};
    uint32_t count = 1;

    // Openable payload.
    bool starts_open = false;
    bool locked = false;

    // Usable payload.
    uint64_t action = 0;    // hashed content action id
    bool repeatable = true;
};

// Spawns one prop: entity + Transform + Highlightable + the kind's component,
// and a static collision box on LAYER_INTERACTABLE carrying its id.
ecs::EntityId spawn_interactable(ecs::World& world, platform::IPhysics& physics,
                                 const InteractableDesc& desc);

} // namespace dfn::gameplay
