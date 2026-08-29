/*
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

#include "engine/gameplay/sources/HeldItem.h"

#include <vector>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/InteractionSystem.h" // empty_item_database()

namespace dfn::gameplay {

namespace {

[[nodiscard]] const ItemDatabase& items_of(const ecs::World& world) {
    return world.has_resource<ItemDatabase>() ? world.resource<ItemDatabase>()
                                              : empty_item_database();
}

// Where the flame sits relative to the carrier's ORIGIN, which for a character
// is the capsule BOTTOM (the feet) — not the eye. Hence the vertical term is
// PLAYER_EYE_HEIGHT minus the below-eye drop: measuring "0.25 m down" from the
// origin instead would bury the light under the floor, where it lights the
// underside of the terrain and casts nothing. The constant is named
// TORCH_HAND_OFFSET_BELOW_EYE precisely because the datum is the eye.
// Local +X is the carrier's right (at yaw 0 the body faces -Z).
constexpr glm::vec3 HAND_OFFSET{
    static_cast<float>(config::TORCH_HAND_OFFSET_RIGHT),
    static_cast<float>(config::PLAYER_EYE_HEIGHT) -
        static_cast<float>(config::TORCH_HAND_OFFSET_BELOW_EYE),
    0.0f};

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

void update_carried_lights(ecs::World& world) {
    // Collect first: adding a component during view iteration is a structural
    // change (Rule 9), so the adds happen after the pass.
    std::vector<ecs::EntityId> needs_light;
    for (auto [id, held] : world.view<HeldItem>()) {
        const bool lit = held.item.valid() && held.lit;
        auto* light = world.get<components::CarriedLight>(id);
        if (light == nullptr) {
            if (lit) {
                needs_light.push_back(id);
            }
            continue; // an unlit carrier needs no component at all
        }
        light->active = lit;
        light->radius_m = 0.0f;  // 0 = render's default torch radius
        light->color_rgb = 0;    // 0 = render's default warm flame
        light->offset = HAND_OFFSET;
    }
    for (const ecs::EntityId id : needs_light) {
        world.add(id, components::CarriedLight{.active = true,
                                               .radius_m = 0.0f,
                                               .color_rgb = 0,
                                               .offset = HAND_OFFSET});
    }
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
