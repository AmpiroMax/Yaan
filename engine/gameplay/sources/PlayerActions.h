/*
Created: 09:08:2026 - 22:29:52
Last updated: 09:08:2026 - 22:29:52
Module: engine/gameplay
File: engine/gameplay/sources/PlayerActions.h

Responsibility:
- Turns the player's latched key presses into the gameplay calls they mean:
  interact with what is under the crosshair, light or douse what is in hand,
  open or close the inventory screen.

Key items:
- player_actions_step(): one call, consuming every latch set since the last tick.

Dependencies:
- Uses: core ecs + events, PlayerMovement.h (the latches), InteractionSystem.h,
  HeldItem.h, InventoryScreen.h.
- Used by: engine/app (one line in the fixed tick), tests.

Notes:
- This exists so the composition root has ONE call rather than five, and so the
  latch-consuming discipline lives in the zone that defines the latches. An app
  that read `state.interact_pressed` itself would sooner or later forget to
  clear it, and the player would interact once per frame instead of once per
  press.
- Call it AFTER player_post_step and AFTER update_hover in the tick: interact()
  acts on the hover target, and a hover computed from last tick's eye pose acts
  on whatever you were looking at a frame ago. That is a real difference when
  turning quickly, and it is the kind of ordering bug that only shows up as
  "sometimes it takes the wrong thing".

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Systems stay stateless (Rule 9). Every latch this reads, it clears.
- The PLAYER only: NPC behaviour arrives as NpcAction (Rule 15).
*/
/*
UPD:
- 09:08:2026 - 22:29:52: Created — one entry point for the player's action keys.
*/

#pragma once

#include "engine/core/ecs/sources/EntityId.h"

namespace dfn::ecs {
class World;
}
namespace dfn::events {
class EventBus;
}

namespace dfn::gameplay {

// Consumes the action latches on every PlayerState and performs what they mean.
// Publishes the interaction outcome events (which are also the hand-animation
// hooks). Safe to call every tick; does nothing when no key was pressed.
void player_actions_step(ecs::World& world, events::EventBus& events);

} // namespace dfn::gameplay
