/*
Created: 09:08:2026 - 18:56:32
Last updated: 09:08:2026 - 18:56:32
Module: engine/gameplay
File: engine/gameplay/sources/InteractableSpawn.cpp

Responsibility:
- Implements interactable prop spawning: ECS components plus the
  LAYER_INTERACTABLE collision box that makes the prop findable by the
  crosshair ray.

Key items:
- spawn_interactable().

Dependencies:
- Uses: InteractableSpawn.h, core ecs World, core components, CollisionLayers.
- Used by: engine/app, the content loader, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- user_data MUST be the entity's packed id: update_hover() reverses it.
*/
/*
UPD:
- 09:08:2026 - 18:56:32: Initial implementation.
*/

#include "engine/gameplay/sources/InteractableSpawn.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/Interaction.h"
#include "engine/physics/sources/CollisionLayers.h"

namespace dfn::gameplay {

ecs::EntityId spawn_interactable(ecs::World& world, platform::IPhysics& physics,
                                 const InteractableDesc& desc) {
    const ecs::EntityId id = world.spawn();

    world.add(id, components::Transform{.position = desc.position,
                                        .rotation = {1.0f, 0.0f, 0.0f, 0.0f},
                                        .scale = {1.0f, 1.0f, 1.0f}});
    world.add(id, Highlightable{.prompt_key = desc.prompt_key,
                                .max_use_distance = 0.0f}); // 0 = INTERACT_DISTANCE

    switch (desc.kind) {
    case InteractableKind::Pickup:
        world.add(id, Pickup{.item = desc.item, .count = desc.count});
        break;
    case InteractableKind::Openable:
        world.add(id, Openable{.open = desc.starts_open,
                               .locked = desc.locked,
                               .lock_level = 0});
        break;
    case InteractableKind::Usable:
        world.add(id, Usable{.action = desc.action,
                             .repeatable = desc.repeatable,
                             .used = false});
        break;
    }

    // The ray target. user_data carries the entity so a hit resolves back.
    platform::StaticBoxDesc box;
    box.center = desc.position;
    box.half_extents = desc.half_extents;
    box.rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    box.layer = physics::LAYER_INTERACTABLE;
    box.user_data = id.packed();
    (void)physics.create_static_box(box);

    return id;
}

} // namespace dfn::gameplay
