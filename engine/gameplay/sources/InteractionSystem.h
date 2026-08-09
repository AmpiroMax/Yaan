/*
Created: 09:08:2026 - 18:56:32
Last updated: 09:08:2026 - 18:56:32
Module: engine/gameplay
File: engine/gameplay/sources/InteractionSystem.h

Responsibility:
- The four interaction verbs (в61): LOOK (crosshair targeting -> HoverTarget),
  TAKE, OPEN/CLOSE and USE. One entry point per concern: a per-tick hover
  update, and a single interact() the input layer calls when the key is pressed.

Key items:
- update_hover(): fixed-tick raycast from the camera, writes HoverTarget.
- interact(): performs the offered verb on the hovered entity.
- offer_for(): what an entity offers right now (verb + prompt + blocked reason).
- ItemTaken / OpenStateChanged / Used / InteractionFailed: the outcome events.

Dependencies:
- Uses: core ecs + components (HoverTarget, CameraPose), core events, platform
  IPhysics (the ray), Interaction.h, Inventory.h, Item.h.
- Used by: engine/app (calls both functions), tests; later the first-person
  hand animation and the quest system, both via the events below.

Notes:
- Verb resolution is ONE function (offer_for) used by both the hover update and
  interact(), so what the reticle promises and what the key press does can
  never disagree.
- Ray: from CameraPose (eye position + yaw/pitch) along the view direction,
  limited by INTERACT_DISTANCE, against LAYER_INTERACTABLE. RayHit.user_data
  carries the EntityId bits, so a hit resolves back to an entity without the
  backend knowing what an entity is.
- HANDS (character agent, later): every verb publishes a typed event carrying
  the actor, the target and the verb. A hand-animation system subscribes and
  plays the matching clip; no verb signature has to change for that to work.
  Failures publish too, so a refused door can play a tug instead of nothing.
- NPCs do NOT interact through this path — Rule 15 says NPC behaviour arrives
  as NpcAction, and an NpcAction alternative for interaction would be added at
  a group sync. This is the player's path, like PlayerMovement.
- Localization is the UI's boundary (agreed with render): gameplay carries
  prompt KEY HASHES only and never resolves a user-facing string (Rule 5).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Systems stay stateless (Rule 9): interfaces are parameters, nothing stored.
- Never mutate interactable state outside interact(); the events and the
  quest-item rules depend on this being the only path.
*/
/*
UPD:
- 09:08:2026 - 18:56:32: Initial four-verb interaction system.
*/

#pragma once

#include <cstdint>

#include "engine/core/ecs/sources/EntityId.h"
#include "engine/gameplay/sources/Interaction.h"
#include "engine/gameplay/sources/Inventory.h"
#include "engine/gameplay/sources/Item.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::ecs {
class World;
}
namespace dfn::events {
class EventBus;
}

namespace dfn::gameplay {

// Why an interaction did not happen. Also drives the "blocked" look of an
// offer, so the reticle can show a refusal before the player presses anything.
enum class InteractionFailure : uint8_t {
    None = 0,
    NothingTargeted,
    OutOfRange,
    Locked,        // Openable::locked — unlocking is a later stage
    AlreadyUsed,   // non-repeatable Usable
    InventoryFull, // reserved: no limit enforced yet
    NoInventory,   // actor has no Inventory component
};

// What a target offers to a given actor at this instant.
struct InteractionOffer {
    ecs::EntityId entity{};
    InteractionVerb verb = InteractionVerb::None;
    uint64_t prompt_key = 0; // hashed localization key, 0 = none
    InteractionFailure blocked = InteractionFailure::None;

    [[nodiscard]] bool available() const {
        return verb != InteractionVerb::None && blocked == InteractionFailure::None;
    }
};

// Resolves what `target` offers. Pure query — no mutation, no events.
[[nodiscard]] InteractionOffer offer_for(const ecs::World& world, ecs::EntityId target);

// The definition table used when no ItemDatabase resource is registered
// (tests, headless tools): every id is unknown, so nothing stacks and nothing
// is a quest item.
[[nodiscard]] const ItemDatabase& empty_item_database();

// LOOK. Once per fixed tick: casts from the player's CameraPose along the view
// direction and writes the components::HoverTarget resource (entity + verb +
// prompt key hash). Nothing hovered => a cleared resource.
void update_hover(ecs::World& world, const platform::IPhysics& physics);

// The key press. Performs the offered verb on the currently hovered entity,
// publishing exactly one outcome event. Returns true when something happened.
bool interact(ecs::World& world, events::EventBus& events, ecs::EntityId actor);

// --- Outcome events (also the hand-animation hooks) --------------------------

struct ItemTaken {
    ecs::EntityId actor{};
    ecs::EntityId source{}; // the pickup entity (already despawned when seen)
    ItemId item{};
    uint32_t count = 0;
};

struct OpenStateChanged {
    ecs::EntityId actor{};
    ecs::EntityId entity{};
    bool open = false;
};

struct Used {
    ecs::EntityId actor{};
    ecs::EntityId entity{};
    uint64_t action = 0; // the content-declared action id
};

struct InteractionFailed {
    ecs::EntityId actor{};
    ecs::EntityId entity{};
    InteractionVerb verb = InteractionVerb::None;
    InteractionFailure reason = InteractionFailure::None;
};

} // namespace dfn::gameplay
