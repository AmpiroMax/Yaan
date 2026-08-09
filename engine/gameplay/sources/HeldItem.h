/*
Created: 09:08:2026 - 18:56:32
Last updated: 09:08:2026 - 20:28:17
Module: engine/gameplay
File: engine/gameplay/sources/HeldItem.h

Responsibility:
- What an actor is holding, and whether it is lit. The gameplay half of the
  torch: holding, lighting and dousing are simulation state; the light the
  renderer draws is a separate seam.

Key items:
- HeldItem: the in-hand item and its lit flag (plain data, Rule 8).
- hold_item / stow_item / toggle_lit: the only mutation path.
- HeldItemChanged / HeldLightChanged: events (hands + render seam + audio).

Dependencies:
- Uses: core ecs EntityId, Ids.h, Item.h, Inventory.h.
- Used by: the TAKE verb (holding what you just picked up), the render seam
  that publishes the carried light, first-person hand animation later.

Notes:
- Holding requires the item to be IN the inventory: the hand is a view onto
  inventory contents, not a separate container. Dropping the last one stows
  the hand automatically, so the hand can never show an item you do not have.
- `lit` is meaningful only for items content marks as light sources
  (ItemDef::light_source). Lighting a non-light item is refused rather than
  silently ignored, so content mistakes surface.
- The RENDER SEAM is deliberately not here: this header knows nothing about
  lights, radii or colours. A small system reads HeldItem and writes whatever
  shared component render reads (pending render's answer on what that is), so
  the simulation stays free of rendering concepts (Rule 1).
- Fuel/burn-down is NOT implemented: a lit torch stays lit. The state shape
  already supports it (toggle_lit is the single choke point), so adding fuel
  is a component field plus a system, not a redesign.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Never set HeldItem fields directly; the events and the inventory invariant
  depend on these functions being the only path.
*/
/*
UPD:
- 09:08:2026 - 18:56:32: Initial held-item / torch state.
- 09:08:2026 - 20:28:17: update_carried_lights(): the render bridge
                         (HeldItem -> components::CarriedLight), with the hand
                         offset measured from the capsule bottom.
*/

#pragma once

#include <cstdint>

#include "engine/core/ecs/sources/EntityId.h"
#include "engine/gameplay/sources/Ids.h"
#include "engine/gameplay/sources/Inventory.h"
#include "engine/gameplay/sources/Item.h"

namespace dfn::ecs {
class World;
}
namespace dfn::events {
class EventBus;
}

namespace dfn::gameplay {

// What this actor has in hand. An invalid item means empty-handed.
struct HeldItem {
    ItemId item{};
    bool lit = false; // only meaningful for light sources
};

// Puts `item` in the actor's hand. Fails (returns false) when the actor has no
// HeldItem component or does not actually carry the item. Switching items
// douses the previous one — you cannot leave a lit torch burning in a pocket.
bool hold_item(ecs::World& world, events::EventBus& events, ecs::EntityId actor,
               ItemId item);

// Empties the hand (and douses whatever was lit).
void stow_item(ecs::World& world, events::EventBus& events, ecs::EntityId actor);

// Lights or douses the held item. Refuses (false) when the hand is empty or
// the held item is not a light source per its content definition.
bool toggle_lit(ecs::World& world, events::EventBus& events, ecs::EntityId actor);

// The bridge to rendering: mirrors HeldItem into components::CarriedLight on
// every carrier, so render can collect point lights without knowing what a
// torch is. Call once per fixed tick, after interaction.
//
// The offset is carrier-local and is measured from Transform.position, which
// for a character is the capsule BOTTOM (feet) — NOT the eye. A flame placed
// "0.25 m down from the origin" would therefore sit underground; the hand is
// PLAYER_EYE_HEIGHT - 0.25 m ABOVE the feet. Local +X is the carrier's right
// (at yaw 0 the body faces -Z and Transform.rotation is identity), so the
// offset swings with the body once render rotates it by that rotation.
void update_carried_lights(ecs::World& world);

// --- Events ------------------------------------------------------------------

struct HeldItemChanged {
    ecs::EntityId actor{};
    ItemId item{}; // invalid = hand emptied
};

// The event the render seam and audio listen to; also the hand-animation hook
// for the light-up/douse motion.
struct HeldLightChanged {
    ecs::EntityId actor{};
    ItemId item{};
    bool lit = false;
};

} // namespace dfn::gameplay
