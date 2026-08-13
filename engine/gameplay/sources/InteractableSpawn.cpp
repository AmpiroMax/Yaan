/*
Created: 09:08:2026 - 18:56:32
Last updated: 13:08:2026 - 18:40:00
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
- 13:08:2026 - 18:15:00: The ray box's handle is KEPT and reaped when its
  entity dies. It was discarded at creation, so no box could ever be destroyed.
- 13:08:2026 - 18:25:00: update_interactable_motion — the door swings on its
  hinge and the lever throws its handle, and the ray box moves with them.
- 13:08:2026 - 18:40:00: A SETTLED LEAF SNAPS ITS TRANSFORM PAIR TOGETHER.
  Caught reading my own diff, not by a test: the swing's last tick left
  prev != curr and then the at-rest branch returned early, so render would have
  interpolated between two poses that never change again — a door sweeping
  between its final two frames for ever. That is the run smear, one component
  over.
*/

#include "engine/gameplay/sources/InteractableSpawn.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/Interaction.h"
#include "engine/gameplay/sources/InteractableMesh.h"
#include "engine/physics/sources/CollisionLayers.h"

namespace dfn::gameplay {

namespace {

// How far a door leaf swings, and how long it takes. Look-dev constants, kept
// HERE with their reasons rather than in NUMBERS: nothing outside this file has
// to agree with either yet, and the registry is for numbers two zones share
// (Rule 35). They move the day a door's opening arc matters to level design.
//
// A quarter turn is the smallest angle that cannot be mistaken for a wobble at
// the 2.5 m a player stands at, and 0.28 s is about the time a hand takes to
// push one — fast enough to feel like a response to the key, slow enough that
// the eye sees it MOVE rather than teleport, which is the whole point.
constexpr float DOOR_SWING_RADIANS = 1.5707963f;
constexpr float DOOR_SWING_SECONDS = 0.28f;
// A lever's handle throws through less than a right angle and much faster: it
// is a snap, not a swing, and a slow lever reads as a stuck one.
constexpr float LEVER_THROW_RADIANS = 0.9f;
constexpr float LEVER_THROW_SECONDS = 0.12f;

} // namespace

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
        // A DOOR TURNS ON ITS EDGE, not on its middle. The hinge is the leaf's
        // own -X edge, which after the scale-by-half-extents mapping is exactly
        // half_extents.x away from the centre — one more thing that follows
        // from the drawn cube being the collision box rather than needing a
        // number of its own.
        world.add(id, InteractableMotion{.blend = desc.starts_open ? 1.0f : 0.0f,
                                         .rest_position = transform.position,
                                         .rest_rotation = transform.rotation,
                                         .hinge_offset = desc.half_extents.x,
                                         .swing_radians = DOOR_SWING_RADIANS,
                                         .seconds = DOOR_SWING_SECONDS});
        break;
    case InteractableKind::Usable:
        world.add(id, Usable{.action = desc.action,
                             .repeatable = desc.repeatable,
                             .used = false});
        // A lever turns about its own body, so no hinge offset.
        world.add(id, InteractableMotion{.blend = 0.0f,
                                         .rest_position = transform.position,
                                         .rest_rotation = transform.rotation,
                                         .hinge_offset = 0.0f,
                                         .swing_radians = LEVER_THROW_RADIANS,
                                         .seconds = LEVER_THROW_SECONDS});
        break;
    }

    // The ray target. user_data carries the entity so a hit resolves back.
    platform::StaticBoxDesc box;
    box.center = desc.position;
    box.half_extents = desc.half_extents;
    box.rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    box.layer = physics::LAYER_INTERACTABLE;
    box.user_data = id.packed();
    const platform::PhysicsBodyHandle body = physics.create_static_box(box);
    // THE HANDLE IS KEPT. It used to be dropped on the floor — `(void)` — which
    // made the box immortal: taking an item despawned its entity and left the
    // ray target behind it forever, and every dropped item added one more.
    if (body.valid()) {
        if (!world.has_resource<InteractableBodies>()) {
            world.add_resource(InteractableBodies{});
        }
        world.resource<InteractableBodies>().bodies.emplace(id.packed(), body);
    }

    return id;
}

void update_interactable_motion(ecs::World& world, platform::IPhysics& physics) {
    const auto dt = static_cast<float>(config::SIM_DT);
    InteractableBodies* bodies =
        world.has_resource<InteractableBodies>() ? &world.resource<InteractableBodies>()
                                                 : nullptr;

    for (auto [id, motion, transform, previous] :
         world.view<InteractableMotion, components::Transform,
                    components::PreviousTransform>()) {
        // WHERE THE POSE COMES FROM: the verb's own boolean. A lever that is
        // repeatable never un-uses itself, so its handle stays thrown — which
        // is honest, and the moment content wants it to spring back the target
        // below is the one line that changes.
        float target = 0.0f;
        if (const auto* openable = world.get<Openable>(id); openable != nullptr) {
            target = openable->open ? 1.0f : 0.0f;
        } else if (const auto* usable = world.get<Usable>(id); usable != nullptr) {
            target = usable->used ? 1.0f : 0.0f;
        }

        const float step = motion.seconds > 0.0f ? dt / motion.seconds : 1.0f;
        const float blend =
            std::clamp(motion.blend + std::clamp(target - motion.blend, -step, step),
                       0.0f, 1.0f);
        if (blend == motion.blend) {
            // AT REST, AND THE PAIR MUST AGREE. Leaving prev != curr here is the
            // run-smear defect exactly (docs/FINDING_RUN_SMEAR.md): render
            // interpolates prev -> curr with alpha sweeping 0..1 INSIDE EVERY
            // TICK, so two poses that never change again are not a still door —
            // they are a door that sweeps between its last two frames for ever.
            // The swing's final tick leaves precisely that pair behind.
            previous.position = transform.position;
            previous.rotation = transform.rotation;
            previous.scale = transform.scale;
            continue;
        }
        motion.blend = blend;

        // SNAPSHOT FIRST, THEN WRITE — the same discipline the player's own
        // transform pair follows (Rule 12). Render interpolates prev -> curr,
        // so a leaf that wrote only `curr` would jump a whole tick's worth of
        // arc every frame instead of sweeping.
        previous.position = transform.position;
        previous.rotation = transform.rotation;
        previous.scale = transform.scale;

        // Smoothstep, so the leaf eases out of the frame and into its stop
        // rather than starting and ending at full speed.
        const float eased = blend * blend * (3.0f - 2.0f * blend);
        const glm::quat turn =
            glm::angleAxis(motion.swing_radians * eased, glm::vec3{0.0f, 1.0f, 0.0f});
        // The hinge, in world space, from the REST pose: deriving from the
        // current pose would accumulate its own error and walk the door out of
        // its frame over a session.
        const glm::vec3 hinge =
            motion.rest_position
            + motion.rest_rotation * glm::vec3{-motion.hinge_offset, 0.0f, 0.0f};
        transform.rotation = turn * motion.rest_rotation;
        transform.position = hinge + turn * (motion.rest_position - hinge);

        // THE RAY TARGET GOES WITH THE LEAF. A door you can see but cannot aim
        // at is the same defect as a trunk you can see but walk through.
        if (bodies != nullptr) {
            const auto it = bodies->bodies.find(id.packed());
            if (it != bodies->bodies.end()) {
                physics.set_body_transform(it->second, transform.position,
                                           transform.rotation);
            }
        }
    }
}

void reap_interactable_bodies(ecs::World& world, platform::IPhysics& physics) {
    if (!world.has_resource<InteractableBodies>()) {
        return;
    }
    auto& state = world.resource<InteractableBodies>();
    for (auto it = state.bodies.begin(); it != state.bodies.end();) {
        const ecs::EntityId owner{static_cast<uint32_t>(it->first >> 32),
                                  static_cast<uint32_t>(it->first & 0xFFFFFFFFull)};
        if (world.alive(owner)) {
            ++it;
            continue;
        }
        physics.destroy_body(it->second);
        it = state.bodies.erase(it);
    }
}

} // namespace dfn::gameplay
