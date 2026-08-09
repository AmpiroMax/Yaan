/*
Created: 09:08:2026 - 18:56:32
Last updated: 09:08:2026 - 18:56:32
Module: engine/gameplay
File: engine/gameplay/sources/InteractionSystem.cpp

Responsibility:
- Implements the four verbs: crosshair targeting (LOOK) writing HoverTarget,
  and interact() performing TAKE / OPEN / CLOSE / USE with one outcome event.

Key items:
- offer_for(), update_hover(), interact().

Dependencies:
- Uses: InteractionSystem.h, core ecs World, core components, core events,
  generated constants, engine/physics collision layers.
- Used by: engine/app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- offer_for() is the SINGLE source of verb resolution: the reticle and the key
  press must never disagree about what a target offers.
*/
/*
UPD:
- 09:08:2026 - 18:56:32: Initial implementation of the four verbs.
*/

#include "engine/gameplay/sources/InteractionSystem.h"

#include <cmath>

#include <glm/geometric.hpp>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/physics/sources/CollisionLayers.h"

namespace dfn::gameplay {

namespace {

// The reach of every verb unless a Highlightable overrides it.
constexpr float DEFAULT_REACH = static_cast<float>(config::INTERACT_DISTANCE);

// View direction from the fixed-tick camera pose. Matches PlayerMovement's
// conventions: yaw 0 faces -Z, positive yaw clockwise from above, +pitch up.
[[nodiscard]] glm::vec3 view_direction(float yaw, float pitch) {
    const float cos_pitch = std::cos(pitch);
    return {std::sin(yaw) * cos_pitch, std::sin(pitch), -std::cos(yaw) * cos_pitch};
}

[[nodiscard]] float reach_of(const Highlightable* highlight) {
    if (highlight == nullptr || highlight->max_use_distance <= 0.0f) {
        return DEFAULT_REACH;
    }
    return highlight->max_use_distance;
}

} // namespace

InteractionOffer offer_for(const ecs::World& world, ecs::EntityId target) {
    InteractionOffer offer;
    if (!world.alive(target)) {
        offer.blocked = InteractionFailure::NothingTargeted;
        return offer;
    }
    offer.entity = target;

    const auto* highlight = world.get<Highlightable>(target);
    if (highlight != nullptr && !highlight->prompt_key.empty()) {
        offer.prompt_key = serialization::fnv1a64(highlight->prompt_key);
    }

    // Verb precedence is deliberate and fixed: a chest that is both openable
    // and lootable reads as OPEN; an item lying on the floor reads as TAKE.
    if (world.get<Pickup>(target) != nullptr) {
        offer.verb = InteractionVerb::Take;
        return offer;
    }
    if (const auto* openable = world.get<Openable>(target); openable != nullptr) {
        offer.verb = openable->open ? InteractionVerb::Close : InteractionVerb::Open;
        if (!openable->open && openable->locked) {
            // Shown as blocked BEFORE the press, so the reticle can refuse.
            offer.blocked = InteractionFailure::Locked;
        }
        return offer;
    }
    if (const auto* usable = world.get<Usable>(target); usable != nullptr) {
        offer.verb = InteractionVerb::Use;
        if (!usable->repeatable && usable->used) {
            offer.blocked = InteractionFailure::AlreadyUsed;
        }
        return offer;
    }
    return offer; // highlightable but offering nothing: verb stays None
}

void update_hover(ecs::World& world, const platform::IPhysics& physics) {
    if (!world.has_resource<components::HoverTarget>()) {
        world.add_resource(components::HoverTarget{});
    }
    auto& hover = world.resource<components::HoverTarget>();
    hover = components::HoverTarget{}; // cleared unless the ray finds something

    for (auto [id, state, camera] :
         world.view<PlayerState, components::CameraPose>()) {
        (void)id;
        (void)state;
        const glm::vec3 direction = view_direction(camera.yaw, camera.pitch);
        const platform::RayHit hit =
            physics.raycast(camera.position, direction, DEFAULT_REACH,
                            physics::LAYER_INTERACTABLE);
        if (!hit.hit || hit.user_data == 0) {
            break;
        }

        // user_data carries the EntityId bits (packed()); rebuild and validate.
        const ecs::EntityId target{static_cast<uint32_t>(hit.user_data >> 32),
                                   static_cast<uint32_t>(hit.user_data & 0xFFFFFFFFull)};
        if (!world.alive(target)) {
            break;
        }
        const InteractionOffer offer = offer_for(world, target);
        if (offer.verb == InteractionVerb::None) {
            break;
        }
        // Reach is per-target: a Highlightable may shorten it, and the ray
        // already stopped at DEFAULT_REACH.
        if (hit.distance > reach_of(world.get<Highlightable>(target))) {
            break;
        }

        hover.entity = target;
        hover.verb = static_cast<uint8_t>(offer.verb);
        hover.prompt_key = offer.prompt_key;
        break; // one player
    }
}

bool interact(ecs::World& world, events::EventBus& events, ecs::EntityId actor) {
    if (!world.has_resource<components::HoverTarget>()) {
        return false;
    }
    const components::HoverTarget hover = world.resource<components::HoverTarget>();
    const ecs::EntityId target = hover.entity;

    const InteractionOffer offer = offer_for(world, target);
    if (offer.verb == InteractionVerb::None) {
        events.post(InteractionFailed{actor, target, InteractionVerb::None,
                                      InteractionFailure::NothingTargeted});
        return false;
    }
    if (offer.blocked != InteractionFailure::None) {
        // Locked doors and spent one-shot levers land here; the event lets a
        // hand animation play a tug and audio play a rattle.
        events.post(InteractionFailed{actor, target, offer.verb, offer.blocked});
        return false;
    }

    switch (offer.verb) {
    case InteractionVerb::Take: {
        auto* pickup = world.get<Pickup>(target);
        auto* inventory = world.get<Inventory>(actor);
        if (inventory == nullptr) {
            events.post(InteractionFailed{actor, target, offer.verb,
                                          InteractionFailure::NoInventory});
            return false;
        }
        const auto& items = world.has_resource<ItemDatabase>()
                                ? world.resource<ItemDatabase>()
                                : empty_item_database();
        const ItemId item = pickup->item;
        const uint32_t count = pickup->count;
        add_item(*inventory, items, item, count);
        // The pickup entity is gone; the event carries what it was.
        world.destroy_deferred(target);
        events.post(ItemTaken{actor, target, item, count});
        events.post(ItemCountChanged{actor, item, count_item(*inventory, item),
                                     static_cast<int32_t>(count)});
        return true;
    }
    case InteractionVerb::Open:
    case InteractionVerb::Close: {
        auto* openable = world.get<Openable>(target);
        openable->open = !openable->open;
        events.post(OpenStateChanged{actor, target, openable->open});
        return true;
    }
    case InteractionVerb::Use: {
        auto* usable = world.get<Usable>(target);
        usable->used = true;
        events.post(Used{actor, target, usable->action});
        return true;
    }
    case InteractionVerb::None:
    default:
        return false;
    }
}

const ItemDatabase& empty_item_database() {
    // Used when no ItemDatabase resource is registered (tests, headless tools):
    // unknown ids are non-stackable and never quest items, which is the safe
    // reading of "no content loaded".
    static const ItemDatabase empty;
    return empty;
}

} // namespace dfn::gameplay
