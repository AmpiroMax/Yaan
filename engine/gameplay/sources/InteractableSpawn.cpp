/*
Created: 09:08:2026 - 18:56:32
Last updated: 13:08:2026 - 17:20:00
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
- 13:08:2026 - 17:20:00: The prop is drawn: RenderMesh (the verb's placeholder
  unless content named one), PreviousTransform (without which render's view
  does not select the entity at all), LocalBounds, and Transform.scale =
  half_extents so the drawn cube IS the ray box.
*/

#include "engine/gameplay/sources/InteractableSpawn.h"

#include <algorithm>

#include "engine/core/components/sources/Components.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/Interaction.h"
#include "engine/gameplay/sources/InteractableMesh.h"
#include "engine/physics/sources/CollisionLayers.h"

namespace dfn::gameplay {

uint32_t interactable_mesh_for(InteractableKind kind) {
    // Exhaustive, no `default`: a new verb must fail to COMPILE here rather
    // than fall through to the "draw nothing" sentinel, which is exactly how
    // the three props that already existed came to be invisible.
    switch (kind) {
    case InteractableKind::Pickup:
        return INTERACTABLE_MESH_TORCH;
    case InteractableKind::Openable:
        return INTERACTABLE_MESH_DOOR;
    case InteractableKind::Usable:
        return INTERACTABLE_MESH_LEVER;
    }
    return INTERACTABLE_MESH_TORCH;
}

ecs::EntityId spawn_interactable(ecs::World& world, platform::IPhysics& physics,
                                 const InteractableDesc& desc) {
    const ecs::EntityId id = world.spawn();

    // SCALE IS THE HALF-EXTENTS, and that one line is what makes the drawn prop
    // and the solid prop the same object. Every placeholder mesh is authored in
    // the cube [-1, 1]^3 (InteractableMesh.h), so scaling by the half-extents
    // maps the artist's cube onto the box the crosshair ray hits: the door is
    // EXACTLY its box, and the two shapes cannot drift apart later because
    // there is only one set of numbers.
    const glm::vec3 scale = desc.half_extents;
    const components::Transform transform{.position = desc.position,
                                          .rotation = {1.0f, 0.0f, 0.0f, 0.0f},
                                          .scale = scale};
    world.add(id, transform);
    // PreviousTransform is NOT optional decoration: render's ECS pass selects on
    // Transform + PreviousTransform + RenderMesh, so a prop without it is
    // invisible however good its mesh is. A static prop's previous transform is
    // its current one — it never moves, so the interpolation is a no-op and the
    // prop is rock steady rather than smeared toward an origin it never had.
    world.add(id, components::PreviousTransform{.position = transform.position,
                                                .rotation = transform.rotation,
                                                .scale = transform.scale});
    const uint32_t mesh_asset =
        desc.mesh_asset != 0 ? desc.mesh_asset : interactable_mesh_for(desc.kind);
    world.add(id, components::RenderMesh{mesh_asset, 0});
    // Model-space bounds are the authored cube, by construction.
    world.add(id, components::LocalBounds{.min = {-1.0f, -1.0f, -1.0f},
                                          .max = {1.0f, 1.0f, 1.0f}});
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
