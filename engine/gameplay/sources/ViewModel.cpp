/*
Module: engine/gameplay
File: engine/gameplay/sources/ViewModel.cpp

Responsibility:
- Places the first-person hand and the item in it at the eye anchor each tick.

Key items:
- spawn_view_model / update_view_model.
- hand_anchor(): eye pose -> world transform of the grip point.

Dependencies:
- Uses: ViewModel.h, core ecs World, core components, HeldItem.h, Item.h,
  generated constants (the torch hand anchor + HAND_OFFSET_FORWARD).
- Used by: engine/app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every offset comes from dfn::config (Rule 14); the anchor is the TORCH
  anchor and must not be duplicated into a second set of numbers.
*/

#include "engine/gameplay/sources/ViewModel.h"

#include <vector>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/HeldItem.h"
#include "engine/gameplay/sources/Item.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/StepFeel.h"

namespace dfn::gameplay {

namespace {

constexpr float OFFSET_RIGHT = static_cast<float>(config::TORCH_HAND_OFFSET_RIGHT);
constexpr float OFFSET_BELOW_EYE = static_cast<float>(config::TORCH_HAND_OFFSET_BELOW_EYE);
constexpr float OFFSET_FORWARD = static_cast<float>(config::HAND_OFFSET_FORWARD);
// The flame sits at the HEAD of the torch, not at the fist holding it. Render
// builds the stick this long from the same row, so wood and fire cannot drift.
constexpr float FLAME_ABOVE_GRIP = static_cast<float>(config::TORCH_FLAME_ABOVE_GRIP);
// Counterphase arm sway (в3): full VIEWMODEL_SWAY_AMPLITUDE at walking bob,
// scaled by the live amplitude so a stationary hand is perfectly still — the
// same zero-at-rest construction as the bob itself.
constexpr float SWAY_AMPLITUDE = static_cast<float>(config::VIEWMODEL_SWAY_AMPLITUDE);
constexpr float BOB_AT_WALK = static_cast<float>(config::HEADBOB_AMPLITUDE_AT_WALK);

struct Anchor {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

// The grip point in world space, and the orientation that keeps the hand fixed
// in the view. Conventions are the movement header's: yaw 0 faces -Z, positive
// yaw is clockwise from above (hence the negation about +Y), positive pitch
// looks up. Composed yaw-then-pitch so the model's -Z ends up along the exact
// camera forward vector.
[[nodiscard]] Anchor hand_anchor(const components::CameraPose& eye) {
    const glm::quat rotation = glm::angleAxis(-eye.yaw, glm::vec3{0.0f, 1.0f, 0.0f}) *
                               glm::angleAxis(eye.pitch, glm::vec3{1.0f, 0.0f, 0.0f});
    // Local axes of the view: +X right, +Y up, -Z forward.
    const glm::vec3 right = rotation * glm::vec3{1.0f, 0.0f, 0.0f};
    const glm::vec3 up = rotation * glm::vec3{0.0f, 1.0f, 0.0f};
    const glm::vec3 forward = rotation * glm::vec3{0.0f, 0.0f, -1.0f};

    Anchor anchor;
    anchor.position = eye.position + right * OFFSET_RIGHT - up * OFFSET_BELOW_EYE +
                      forward * OFFSET_FORWARD;
    anchor.rotation = rotation;
    return anchor;
}

} // namespace

glm::vec3 hand_anchor_position(const components::CameraPose& eye) {
    return hand_anchor(eye).position;
}

void spawn_view_model(ecs::World& world, ecs::EntityId carrier) {
    for (auto [id, part] : world.view<ViewModelPart>()) {
        (void)id;
        if (part.carrier == carrier) {
            return; // already present; spawning is idempotent
        }
    }

    const uint32_t hand_mesh =
        world.has_resource<ViewModelAssets>() ? world.resource<ViewModelAssets>().hand_mesh : 0u;

    for (const bool is_item : {false, true}) {
        const ecs::EntityId id = world.spawn();
        world.add(id, ViewModelPart{.carrier = carrier, .is_item = is_item});
        world.add(id, components::Transform{});
        world.add(id, components::PreviousTransform{});
        // The hand has its mesh from the start; the item's is whatever is held
        // this tick, which is nothing until something is picked up.
        world.add(id, components::RenderMesh{.mesh_asset = is_item ? 0u : hand_mesh,
                                             .texture_asset = 0u});
    }
}

void update_view_model(ecs::World& world) {
    const ItemDatabase* items =
        world.has_resource<ItemDatabase>() ? &world.resource<ItemDatabase>() : nullptr;

    // Carriers whose light now lives on their view model: their own CarriedLight
    // must stand down, or the flame is drawn twice, half a metre apart.
    std::vector<ecs::EntityId> lit_carriers;

    for (auto [id, part, transform, prev_transform, mesh] :
         world.view<ViewModelPart, components::Transform, components::PreviousTransform,
                    components::RenderMesh>()) {
        (void)id;
        const auto* eye = world.get<components::CameraPose>(part.carrier);
        if (eye == nullptr) {
            continue; // carrier gone or has no view: leave the parts where they are
        }

        // Snapshot discipline (Rule 12): render interpolates these too, so a
        // view model that skipped it would judder while everything else is smooth.
        prev_transform.position = transform.position;
        prev_transform.rotation = transform.rotation;
        prev_transform.scale = transform.scale;

        Anchor anchor = hand_anchor(*eye);
        // Counterphase arm sway: the hand swings OPPOSITE the camera's lateral
        // bob, from the same stride clock (PlayerState is the one step clock —
        // Rule 35; agreed with character). The camera's own bob already moves
        // the anchor with the eye; this adds the arm's pendulum against it.
        if (const auto* walker = world.get<PlayerState>(part.carrier)) {
            const float ratio = walker->bob_amplitude / BOB_AT_WALK;
            const float sway =
                -bob_lateral(walker->stride_phase, SWAY_AMPLITUDE * ratio);
            const glm::vec3 right = anchor.rotation * glm::vec3{1.0f, 0.0f, 0.0f};
            anchor.position += right * sway;
        }
        transform.position = anchor.position;
        transform.rotation = anchor.rotation;

        if (!part.is_item) {
            continue;
        }

        // The item slot mirrors what the hand actually holds.
        const auto* held = world.get<HeldItem>(part.carrier);
        const ItemDef* def =
            (held != nullptr && held->item.valid() && items != nullptr)
                ? items->find(held->item)
                : nullptr;
        mesh.mesh_asset = def != nullptr ? def->mesh_id : 0u;

        const bool lit = held != nullptr && held->item.valid() && held->lit;
        if (lit) {
            lit_carriers.push_back(part.carrier);
        }
        // The light rides the ITEM, not the carrier, so the flame sits exactly
        // where the wood is drawn even when looking straight up or down.
        // The offset runs up the torch from the grip: render applies the
        // entity's FULL rotation to it (checked, not assumed), and this entity
        // carries the view rotation, so local +Y is along the stick whichever
        // way the player is looking. A zero offset would burn at the wrist.
        const glm::vec3 flame{0.0f, FLAME_ABOVE_GRIP, 0.0f};
        auto* light = world.get<components::CarriedLight>(id);
        if (light == nullptr) {
            if (lit) {
                world.add(id, components::CarriedLight{.active = true,
                                                       .radius_m = 0.0f,
                                                       .color_rgb = 0,
                                                       .offset = flame});
            }
        } else {
            light->active = lit;
            light->offset = flame;
        }
    }

    for (const ecs::EntityId carrier : lit_carriers) {
        if (auto* light = world.get<components::CarriedLight>(carrier)) {
            light->active = false;
        }
    }
}

} // namespace dfn::gameplay
