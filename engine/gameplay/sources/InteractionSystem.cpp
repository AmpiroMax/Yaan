/*
Created: 09:08:2026 - 18:56:32
Last updated: 13:08:2026 - 18:10:00
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
- 13:08:2026 - 17:30:00: THE HOVER PROBE (DFN_HOVER_PROBE). "The prompt appears
  when the prop is in front of you and not before" is a claim about a
  TRANSITION, and no still frame can show a transition. One row per fixed tick:
  eye, ray hit, distance, verb, prompt key. A tick that found nothing writes a
  row saying so, because a gap and a zero read the same and only one of them is
  information.
- 13:08:2026 - 18:10:00: THE FIRST ENTITY A WORLD SPAWNS COULD NOT BE
  INTERACTED WITH. `packed()` is `index << 32 | generation`, so entity {0,0}
  packs to 0, and update_hover threw away every hit whose user_data was 0 as
  "no entity". The ray found the prop, the hit was discarded, and no prompt
  appeared however carefully you aimed. Surfaced by a probe where the torch
  happened to be entity 0 and the geometrically identical lever beside it
  hovered perfectly. The running game escapes it only because the app spawns
  the player and the chunk entities first — luck, not design.
*/

#include "engine/gameplay/sources/InteractionSystem.h"

#include <cmath>
#include <cstdio>  // hover probe (DFN_HOVER_PROBE) only
#include <cstdlib> // hover probe (DFN_HOVER_PROBE) only

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

// THE HOVER PROBE (DFN_HOVER_PROBE=<path>): one row per fixed tick saying where
// the eye was, what the crosshair ray found, and how far away it was.
//
// Why a probe and not a screenshot. "The prompt appears when the prop is in
// front of you and not before" is a claim about a TRANSITION, and no still
// frame can show a transition -- the same lesson the run-smear investigation
// paid for with three clean captures of a defect that lives between frames. A
// row per tick shows the empty rows, the first hit, and the distance at which
// it happened, which is the whole claim.
//
// Off unless the env var names a file; nothing here feeds back into the world,
// so a run is bit-identical with the probe on or off (Rule 13).
struct HoverProbe {
    std::FILE* file = nullptr;
    uint64_t tick = 0;
};

[[nodiscard]] HoverProbe& hover_probe() {
    static HoverProbe probe = [] {
        HoverProbe p;
        const char* path = std::getenv("DFN_HOVER_PROBE");
        if (path != nullptr && *path != '\0') {
            p.file = std::fopen(path, "w");
            if (p.file == nullptr) {
                // LOUD: an empty file reads exactly like "the run measured zero
                // hovers", which is the answer this probe exists to distinguish
                // from "the run measured nothing at all".
                std::fprintf(stderr,
                             "[hover_probe] cannot open \"%s\" -- NOTHING WILL BE "
                             "MEASURED THIS RUN\n",
                             path);
            } else {
                std::fprintf(p.file,
                             "tick,eye_x,eye_y,eye_z,yaw,pitch,hit,distance_m,"
                             "verb,prompt_key,entity\n");
            }
        }
        return p;
    }();
    return probe;
}

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

    // The probe writes the row for THIS tick when the pass is over, whichever
    // way it left, so a tick that found nothing is a row saying so rather than
    // a gap. Gaps and zeros read the same and only one of them is information.
    struct ProbeRow {
        components::CameraPose camera{};
        bool had_player = false;
        bool hit = false;
        float distance = 0.0f;
    } row;

    for (auto [id, state, camera] :
         world.view<PlayerState, components::CameraPose>()) {
        (void)id;
        (void)state;
        row.camera = camera;
        row.had_player = true;
        const glm::vec3 direction = view_direction(camera.yaw, camera.pitch);
        const platform::RayHit hit =
            physics.raycast(camera.position, direction, DEFAULT_REACH,
                            physics::LAYER_INTERACTABLE);
        row.hit = hit.hit;
        row.distance = hit.distance;
        if (!hit.hit) {
            break;
        }

        // user_data carries the EntityId bits (packed()); rebuild and validate.
        //
        // THERE IS NO `user_data == 0` TEST HERE, AND THAT IS A FIX. It used to
        // read `if (!hit.hit || hit.user_data == 0) break;` on the reasonable-
        // looking ground that 0 means "no entity" — but `packed()` is
        // `index << 32 | generation`, so the entity {index 0, generation 0}
        // packs to EXACTLY 0. The first entity a World ever spawns is therefore
        // invisible to the crosshair: the ray hits its box, the hit is thrown
        // away as "nothing", and no prompt appears however carefully you aim.
        // Found by a probe that spawned the props into a fresh world, where the
        // torch WAS entity 0 and the identical lever beside it hovered fine.
        //
        // It does not bite the running game today only by accident — the app
        // spawns the player and a great many chunk entities before any prop —
        // which is exactly the kind of luck that stops holding the day someone
        // reorders startup. `world.alive()` below is the real validation, and
        // every body on LAYER_INTERACTABLE is created by spawn_interactable
        // (checked repo-wide), so a hit here always carries a real id.
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

    if (HoverProbe& probe = hover_probe(); probe.file != nullptr && row.had_player) {
        std::fprintf(probe.file, "%llu,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%.4f,%u,%llu,%u\n",
                     static_cast<unsigned long long>(probe.tick++),
                     static_cast<double>(row.camera.position.x),
                     static_cast<double>(row.camera.position.y),
                     static_cast<double>(row.camera.position.z),
                     static_cast<double>(row.camera.yaw),
                     static_cast<double>(row.camera.pitch), row.hit ? 1 : 0,
                     static_cast<double>(row.distance), hover.verb,
                     static_cast<unsigned long long>(hover.prompt_key),
                     hover.entity.index);
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
