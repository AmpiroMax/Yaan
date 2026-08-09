/*
Created: 09:08:2026 - 18:56:32
Last updated: 09:08:2026 - 18:56:32
Module: engine/gameplay
File: engine/gameplay/sources/HeldItem.cpp

Responsibility:
- Implements holding, stowing and lighting the in-hand item (the torch).

Key items:
- hold_item / stow_item / toggle_lit.

Dependencies:
- Uses: HeldItem.h, core ecs World, core events.
- Used by: the interaction layer, tests, the render seam system.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The hand is a view onto the inventory: never let it hold what the actor
  does not carry.
*/
/*
UPD:
- 09:08:2026 - 18:56:32: Initial implementation.
*/

#include "engine/gameplay/sources/HeldItem.h"

#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/InteractionSystem.h" // empty_item_database()

namespace dfn::gameplay {

namespace {

[[nodiscard]] const ItemDatabase& items_of(const ecs::World& world) {
    return world.has_resource<ItemDatabase>() ? world.resource<ItemDatabase>()
                                              : empty_item_database();
}

} // namespace

bool hold_item(ecs::World& world, events::EventBus& events, ecs::EntityId actor,
               ItemId item) {
    auto* held = world.get<HeldItem>(actor);
    if (held == nullptr || !item.valid()) {
        return false;
    }
    const auto* inventory = world.get<Inventory>(actor);
    if (inventory == nullptr || count_item(*inventory, item) == 0) {
        return false; // the hand may only show what is actually carried
    }
    if (held->item.value == item.value) {
        return true; // already in hand, lit state preserved
    }

    if (held->lit) {
        // Switching items douses the old one; no lit torch in a pocket.
        events.post(HeldLightChanged{actor, held->item, false});
    }
    held->item = item;
    held->lit = false;
    events.post(HeldItemChanged{actor, item});
    return true;
}

void stow_item(ecs::World& world, events::EventBus& events, ecs::EntityId actor) {
    auto* held = world.get<HeldItem>(actor);
    if (held == nullptr || !held->item.valid()) {
        return;
    }
    if (held->lit) {
        events.post(HeldLightChanged{actor, held->item, false});
    }
    held->item = ItemId{};
    held->lit = false;
    events.post(HeldItemChanged{actor, ItemId{}});
}

bool toggle_lit(ecs::World& world, events::EventBus& events, ecs::EntityId actor) {
    auto* held = world.get<HeldItem>(actor);
    if (held == nullptr || !held->item.valid()) {
        return false;
    }
    const ItemDef* def = items_of(world).find(held->item);
    if (def == nullptr || !def->light_source) {
        return false; // content says this is not something you can light
    }
    held->lit = !held->lit;
    events.post(HeldLightChanged{actor, held->item, held->lit});
    return true;
}

} // namespace dfn::gameplay
