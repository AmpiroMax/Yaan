/*
Created: 09:08:2026 - 00:45:08
Last updated: 28:08:2026 - 14:18:16
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
- 10:08:2026 - 01:53:17: Step-feel wiring (в3): both wrappers take a
                         StepContext; the wrapper fills walker with the
                         iterated entity, post_step now feeds the real
                         PreviousTransform so the stride runs on actual
                         displacement.
- 09:08:2026 - 00:45:08: Stage 2 — initial World wrappers.
- 09:08:2026 - 22:18:17: player_pre_step takes a water-surface callback,
                         bound to world::ChunkManager::water_surface_at by
                         the app; an unbound callback means a dry world.
- 09:08:2026 - 22:40:04: Mouse turns the previewed item while the inventory
                         screen is open.
- 09:08:2026 - 22:44:47: Preview rotation MOVED OUT of the movement path
                         into player_actions_step: the world pausing
                         behind the inventory skips movement, and a
                         preview that turned here would freeze with it.
- 11:08:2026 - 13:51:09: Rule 32 sweep after the run smear: the spawn-time
  PreviousCameraPose now spells out `fov_scale` too. Harmless at spawn (the
  camera also starts at 1.0), listed anyway because the omission had the same
  cause as the per-tick one that WAS the defect: an initialiser list that
  reads as complete.
- 13:08:2026 - 16:20:00: The wrapper reads the brush drag out of the World's
  BrushField and ferries it on StepContext. NOT a new parameter, and the
  asymmetry with water is the point: where water is belongs to engine/world, so
  its depth arrives as a callback the app binds; where brush is belongs to THIS
  zone -- update_prop_collision built the drag field from the same scatter it
  built the trunk bodies from. Asking the app to ferry a number gameplay
  already owns would be a wire with no information on it, and one more thing
  for a caller to forget. It also means engine/app is untouched by today's work.
- 28:08:2026 - 14:18:16: капсула сталкивается с подвижными предметами
  (LAYER_LOOSE) и получила ПОТОЛОК СИЛЫ ТОЛЧКА push_force_n = 30 Н (заказ
  владельца 28.08: «моё тело тоже имеет физические свойства — хочу банки,
  бутылки, еду толкать»). Умолчание Jolt (100 Н) измерено слишком большим:
  полукилограммовый кубок, задетый на ходу, улетал на 4.8-6.6 м — человек не
  задевал посуду, а пинал её. Это ВТОРОЙ из двух потолков зоны; первый —
  сила хвата (GrabDrive.h), и разводить их обязательно: иначе «не могу
  поднять шкаф» и «не могу отпихнуть шкаф ногой» стали бы одной ручкой.
*/

#include "engine/gameplay/sources/PlayerMovement.h"

#include <algorithm>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/InventoryScreen.h"
#include "engine/gameplay/sources/PropCollision.h"
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
    // КАПСУЛА СТАЛКИВАЕТСЯ И С ПОДВИЖНЫМИ ПРЕДМЕТАМИ (28.08, владелец: «моё
    // тело тоже имеет физические свойства — хочу банки, бутылки, еду толкать,
    // двигать»). Толчок ограничен силой (CharacterDesc::push_force_n), поэтому
    // бутыль откатывается, а бочка в двести килограммов — нет: одна ручка, а
    // не таблица классов веса. Character-vs-character — по-прежнему позже.
    desc.collides_with = physics::LAYER_STATIC | physics::LAYER_LOOSE;
    // Потолок силы толчка телом — именованной величиной, а не литералом:
    // её читает и рукав приёмки (PlayerMovement.h, PLAYER_PUSH_FORCE_N).
    desc.push_force_n = PLAYER_PUSH_FORCE_N;
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
    // EVERY FIELD SPELLED OUT, including the one whose default is right anyway.
    // `fov_scale` was omitted here and in the per-tick snapshot, and while the
    // omission is harmless at spawn (the camera also starts at 1.0), the two
    // omissions had the same cause: a designated-initialiser list that reads as
    // complete. The per-tick one was the run smear (docs/FINDING_RUN_SMEAR.md).
    world.add(id, components::PreviousCameraPose{
                      .position = eye, .yaw = 0.0f, .pitch = 0.0f, .fov_scale = 1.0f});
    return id;
}

void player_accumulate_input(ecs::World& world, const platform::IInput& input) {
    for (auto [id, state] : world.view<PlayerState>()) {
        (void)id;
        accumulate_input(input, state);
    }
}

void player_pre_step(ecs::World& world, platform::IPhysics& physics,
                     const WaterSurfaceFn& water_surface_at, const StepContext& step) {
    for (auto [id, state, transform, prev_transform, camera, prev_camera] :
         world.view<PlayerState, components::Transform, components::PreviousTransform,
                    components::CameraPose, components::PreviousCameraPose>()) {
        StepContext ctx = step;
        ctx.walker = id;
        // Depth is measured from the FEET (Transform.position is the capsule
        // bottom) against the surface engine/world reports. No callback bound
        // means a world without water — dry, not broken.
        float depth = 0.0f;
        if (water_surface_at) {
            const std::optional<float> surface =
                water_surface_at(glm::vec2{transform.position.x, transform.position.z});
            if (surface) {
                depth = std::max(0.0f, *surface - transform.position.y);
            }
        }
        // BRUSH IS READ HERE, not passed in, and the asymmetry with water is
        // deliberate. Where water is belongs to engine/world, so its depth
        // arrives as a callback the app binds; where brush is belongs to THIS
        // zone — update_prop_collision built the drag field from the same
        // scatter it built the trunk bodies from, in the same reconcile. Asking
        // the app to ferry a number gameplay already owns would be a wire with
        // no information on it, and one more thing for a caller to forget.
        ctx.brush_density = brush_density_at(
            world, transform.position, static_cast<float>(config::PLAYER_CAPSULE_RADIUS));
        player_pre_step(state, physics, depth, transform, prev_transform, camera,
                        prev_camera, ctx);
    }
}

void player_post_step(ecs::World& world, platform::IPhysics& physics,
                      const StepContext& step) {
    for (auto [id, state, prev_transform, transform, camera] :
         world.view<PlayerState, components::PreviousTransform, components::Transform,
                    components::CameraPose>()) {
        StepContext ctx = step;
        ctx.walker = id;
        player_post_step(state, physics, prev_transform, transform, camera, ctx);
    }
}

} // namespace dfn::gameplay
