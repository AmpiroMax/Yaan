/*
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

#include "engine/gameplay/sources/PlayerActions.h"

#include <vector>

#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/gameplay/sources/HeldItem.h"
#include "engine/gameplay/sources/InteractableSpawn.h"
#include "engine/gameplay/sources/Item.h"
#include "engine/gameplay/sources/ViewModel.h"
#include "engine/gameplay/sources/InteractionSystem.h"
#include "engine/gameplay/sources/InventoryScreen.h"
#include "engine/gameplay/sources/PlayerMovement.h"

namespace dfn::gameplay {

namespace {

// Localization key for a loose item lying in the world. One place rather than
// one per spawn site; content owns it once the loader exists (Rule 5 — this is
// a KEY, never a word).
constexpr const char* PICKUP_PROMPT_KEY = "prompt.take";

// Lets one of `item` go into the world in front of `actor`. Returns the new
// entity, or a null id when the drop was refused.
ecs::EntityId drop_one(ecs::World& world, events::EventBus& events,
                       platform::IPhysics& physics, ecs::EntityId actor, ItemId item) {
    auto* inventory = world.get<Inventory>(actor);
    const auto* eye = world.get<components::CameraPose>(actor);
    if (inventory == nullptr || eye == nullptr || count_item(*inventory, item) == 0) {
        return {};
    }
    const ItemDatabase& items = world.has_resource<ItemDatabase>()
                                    ? world.resource<ItemDatabase>()
                                    : empty_item_database();
    if (!can_drop(items, item)) {
        // Story's rule, refused loudly rather than silently: a quest item may be
        // held and given by quest effects, but the player cannot lose it.
        events.post(InteractionFailed{actor, ecs::EntityId{}, InteractionVerb::Take,
                                      InteractionFailure::Undroppable});
        return {};
    }

    if (remove_item(*inventory, item, 1) == 0) {
        return {};
    }
    // The hand can never show something the inventory does not have.
    const auto* held = world.get<HeldItem>(actor);
    if (held != nullptr && held->item.value == item.value &&
        count_item(*inventory, item) == 0) {
        stow_item(world, events, actor);
    }

    // It appears where it was HELD. That needs no distance number of its own:
    // an item let go leaves from the hand, and the hand anchor already exists.
    InteractableDesc desc;
    desc.kind = InteractableKind::Pickup;
    desc.position = hand_anchor_position(*eye);
    desc.prompt_key = PICKUP_PROMPT_KEY;
    desc.item = item;
    desc.count = 1;
    const ecs::EntityId spawned = spawn_interactable(world, physics, desc);
    events.post(ItemDropped{actor, spawned, item, 1});
    return spawned;
}

} // namespace

void player_actions_step(ecs::World& world, events::EventBus& events,
                         platform::IPhysics& physics) {
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
        bool drop = false;
        glm::vec2 look{0.0f};
    };
    std::vector<Pending> pending;

    for (auto [id, state] : world.view<PlayerState>()) {
        const bool anything = state.interact_pressed || state.toggle_light_pressed ||
                              state.toggle_inventory_pressed ||
                              state.pending_selection_delta != 0 || state.equip_pressed ||
                              state.drop_pressed || screen_open;
        if (!anything) {
            continue;
        }
        pending.push_back(Pending{id, state.interact_pressed, state.toggle_light_pressed,
                                  state.toggle_inventory_pressed,
                                  state.pending_selection_delta, state.equip_pressed,
                                  state.drop_pressed, state.pending_look});
        // The mouse TURNS THE ITEM while the screen is open, so the look it
        // would otherwise have applied is consumed here instead.
        if (screen_open) {
            state.pending_look = glm::vec2{0.0f};
        }
        // Spent on read, always. A latch left set because the action was
        // refused would re-fire every tick until it happened to succeed.
        state.interact_pressed = false;
        state.toggle_light_pressed = false;
        state.toggle_inventory_pressed = false;
        state.pending_selection_delta = 0;
        state.equip_pressed = false;
        state.drop_pressed = false;
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
            // Same radians-per-pixel as looking: same hand, same gesture, and a
            // second sensitivity row would be a second thing to keep in step.
            const float sens = static_cast<float>(config::MOUSE_SENSITIVITY);
            rotate_preview(screen, p.look.x * sens, -p.look.y * sens);
            if (p.selection != 0) {
                move_selection(screen, p.selection);
            }
            if (p.equip) {
                if (const auto* entry = selected_entry(screen)) {
                    (void)hold_item(world, events, p.actor, entry->item);
                }
            }
            if (p.drop) {
                if (const auto* entry = selected_entry(screen)) {
                    (void)drop_one(world, events, physics, p.actor, entry->item);
                }
            }
        }
        // The list is rebuilt after any of these: taking an item adds a row,
        // lighting or equipping changes a row's held flag, and opening the
        // screen must show what is actually carried right now. Rebuilt every
        // tick while open so a count can never go stale on screen.
        refresh_inventory_screen(world, p.actor);
    }

    // The visible half of every verb pressed above (and of any that content or
    // a quest flipped). Before the reap, so a prop that dies this tick is not
    // posed on its way out.
    update_interactable_motion(world, physics);

    // A prop taken last tick has been flushed by now, so its ray box is a
    // target with nothing behind it. Reaped here rather than at the point of
    // taking, because destruction is DEFERRED: the entity is still alive inside
    // interact(), and a reap that ran there would find nothing to reap and be
    // green forever.
    reap_interactable_bodies(world, physics);
}

} // namespace dfn::gameplay
