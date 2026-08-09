/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 00:45:08
Module: engine/gameplay
File: engine/gameplay/sources/PlayerMovementWorld.cpp

Responsibility:
- World-facing player movement wrappers: spawn_player plus the per-tick entry
  points engine/app calls. Deliberately a separate TU so the ECS dependency
  stays isolated from the ref-based movement core.

Key items:
- spawn_player / player_accumulate_input / player_pre_step / player_post_step
  (World overloads) — iterate view<PlayerState, ...> and delegate to the core.

Dependencies:
- Uses: PlayerMovement.h, core ecs World, CollisionLayers, generated constants.
- Used by: engine/app fixed-tick loop.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Systems stay stateless (Rule 9): interfaces are parameters, nothing stored.
- The player entity carries the character's EntityId bits as physics user_data
  so raycasts resolve back to it.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial World wrappers.
*/

#include "engine/gameplay/sources/PlayerMovement.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/physics/sources/CollisionLayers.h"

namespace dfn::gameplay {

ecs::EntityId spawn_player(ecs::World& world, platform::IPhysics& physics,
                           const glm::vec3& spawn_pos) {
    const ecs::EntityId id = world.spawn();

    platform::CharacterDesc desc;
    desc.position = spawn_pos; // capsule bottom
    desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
    desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
    desc.max_slope_radians = static_cast<float>(config::PLAYER_MAX_SLOPE);
    desc.step_height = static_cast<float>(config::PLAYER_STEP_HEIGHT);
    desc.layer = physics::LAYER_CHARACTER;
    desc.collides_with = physics::LAYER_STATIC; // character-vs-character: later sync
    desc.user_data = id.packed();

    PlayerState state;
    state.character = physics.create_character(desc);

    world.add(id, std::move(state));
    world.add(id, components::Transform{.position = spawn_pos,
                                        .rotation = {1.0f, 0.0f, 0.0f, 0.0f},
                                        .scale = {1.0f, 1.0f, 1.0f}});
    world.add(id, components::PreviousTransform{.position = spawn_pos,
                                                .rotation = {1.0f, 0.0f, 0.0f, 0.0f},
                                                .scale = {1.0f, 1.0f, 1.0f}});
    const glm::vec3 eye =
        spawn_pos + glm::vec3{0.0f, static_cast<float>(config::PLAYER_EYE_HEIGHT), 0.0f};
    world.add(id, components::CameraPose{.position = eye, .yaw = 0.0f, .pitch = 0.0f});
    world.add(id,
              components::PreviousCameraPose{.position = eye, .yaw = 0.0f, .pitch = 0.0f});
    return id;
}

void player_accumulate_input(ecs::World& world, const platform::IInput& input) {
    for (auto [id, state] : world.view<PlayerState>()) {
        (void)id;
        accumulate_input(input, state);
    }
}

void player_pre_step(ecs::World& world, platform::IPhysics& physics) {
    for (auto [id, state, transform, prev_transform, camera, prev_camera] :
         world.view<PlayerState, components::Transform, components::PreviousTransform,
                    components::CameraPose, components::PreviousCameraPose>()) {
        (void)id;
        player_pre_step(state, physics, transform, prev_transform, camera, prev_camera);
    }
}

void player_post_step(ecs::World& world, platform::IPhysics& physics) {
    for (auto [id, state, transform, camera] :
         world.view<PlayerState, components::Transform, components::CameraPose>()) {
        (void)id;
        player_post_step(state, physics, transform, camera);
    }
}

} // namespace dfn::gameplay
