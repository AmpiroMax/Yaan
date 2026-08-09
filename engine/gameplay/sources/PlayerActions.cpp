/*
Created: 09:08:2026 - 22:29:52
Last updated: 09:08:2026 - 22:40:04
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
- 09:08:2026 - 22:40:04: Inventory navigation and equip while the screen is open.
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
    const bool screen_open =
        world.has_resource<InventoryScreen>() && world.resource<InventoryScreen>().open;

    struct Pending {
        ecs::EntityId actor{};
        bool interact = false;
        bool light = false;
        bool inventory = false;
        int32_t selection = 0;
        bool equip = false;
    };
    std::vector<Pending> pending;

    for (auto [id, state] : world.view<PlayerState>()) {
        const bool anything = state.interact_pressed || state.toggle_light_pressed ||
                              state.toggle_inventory_pressed ||
                              state.pending_selection_delta != 0 || state.equip_pressed ||
                              screen_open;
        if (!anything) {
            continue;
        }
        pending.push_back(Pending{id, state.interact_pressed, state.toggle_light_pressed,
                                  state.toggle_inventory_pressed,
                                  state.pending_selection_delta, state.equip_pressed});
        // Spent on read, always. A latch left set because the action was
        // refused would re-fire every tick until it happened to succeed.
        state.interact_pressed = false;
        state.toggle_light_pressed = false;
        state.toggle_inventory_pressed = false;
        state.pending_selection_delta = 0;
        state.equip_pressed = false;
    }

    for (const Pending& p : pending) {
        // With the screen open, the same key EQUIPS the highlighted row rather
        // than reaching into the world: the crosshair is not what the player is
        // pointing at any more.
        if (p.interact && screen_open) {
            if (const auto* entry = selected_entry(world.resource<InventoryScreen>())) {
                (void)hold_item(world, events, p.actor, entry->item);
            }
        } else if (p.interact) {
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
        if (screen_open) {
            auto& screen = world.resource<InventoryScreen>();
            if (p.selection != 0) {
                move_selection(screen, p.selection);
            }
            if (p.equip) {
                if (const auto* entry = selected_entry(screen)) {
                    (void)hold_item(world, events, p.actor, entry->item);
                }
            }
        }
        // The list is rebuilt after any of these: taking an item adds a row,
        // lighting or equipping changes a row's held flag, and opening the
        // screen must show what is actually carried right now. Rebuilt every
        // tick while open so a count can never go stale on screen.
        refresh_inventory_screen(world, p.actor);
    }
}

} // namespace dfn::gameplay
