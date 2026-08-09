/*
Created: 09:08:2026 - 22:29:52
Last updated: 09:08:2026 - 22:29:52
Module: engine/gameplay
File: engine/gameplay/sources/PlayerActions.cpp

Responsibility:
- Consumes the player's action latches and performs interact / light / inventory.

Key items:
- player_actions_step().

Dependencies:
- Uses: PlayerActions.h, PlayerMovement.h, InteractionSystem.h, HeldItem.h,
  InventoryScreen.h, core ecs.
- Used by: engine/app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every latch read here is cleared here, whether or not the action succeeded:
  an unspent press is a press that fires again next tick.
*/
/*
UPD:
- 09:08:2026 - 22:29:52: Created.
*/

#include "engine/gameplay/sources/PlayerActions.h"

#include <vector>

#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/HeldItem.h"
#include "engine/gameplay/sources/InteractionSystem.h"
#include "engine/gameplay/sources/InventoryScreen.h"
#include "engine/gameplay/sources/PlayerMovement.h"

namespace dfn::gameplay {

void player_actions_step(ecs::World& world, events::EventBus& events) {
    // Collected first: interact() may despawn a pickup entity, and destroying
    // an entity while iterating a view is a structural change (Rule 9).
    struct Pending {
        ecs::EntityId actor{};
        bool interact = false;
        bool light = false;
        bool inventory = false;
    };
    std::vector<Pending> pending;

    for (auto [id, state] : world.view<PlayerState>()) {
        if (!state.interact_pressed && !state.toggle_light_pressed &&
            !state.toggle_inventory_pressed) {
            continue;
        }
        pending.push_back(Pending{id, state.interact_pressed, state.toggle_light_pressed,
                                  state.toggle_inventory_pressed});
        // Spent on read, always. A latch left set because the action was
        // refused would re-fire every tick until it happened to succeed.
        state.interact_pressed = false;
        state.toggle_light_pressed = false;
        state.toggle_inventory_pressed = false;
    }

    for (const Pending& p : pending) {
        if (p.interact) {
            (void)interact(world, events, p.actor);
        }
        if (p.light) {
            (void)toggle_lit(world, events, p.actor);
        }
        if (p.inventory) {
            if (!world.has_resource<InventoryScreen>()) {
                world.add_resource(InventoryScreen{});
            }
            auto& screen = world.resource<InventoryScreen>();
            screen.open = !screen.open;
        }
        // The list is rebuilt after any of the three: taking an item adds a
        // row, lighting a torch changes a row's held flag, and opening the
        // screen must show what is actually carried right now.
        refresh_inventory_screen(world, p.actor);
    }
}

} // namespace dfn::gameplay
