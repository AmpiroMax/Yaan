/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 22:40:04
Module: engine/gameplay
File: engine/gameplay/sources/PlayerMovement.cpp

Responsibility:
- Ref-based player movement core: input accumulation, look integration with
  pitch clamp, gravity, displacement submission, post-step component writes.
  No ECS dependency in this TU (World wrappers live in PlayerMovementWorld.cpp).

Key items:
- accumulate_input / player_pre_step / player_post_step (ref overloads).

Dependencies:
- Uses: PlayerMovement.h, generated constants (dfn::config), glm.
- Used by: PlayerMovementWorld.cpp wrappers, tests (null physics).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every tuning number comes from dfn::config (Rule 14) — no literals here.
- Conventions (yaw/pitch/axes) are documented in the header; keep in sync.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial implementation.
- 09:08:2026 - 17:08:40: DEBUG CONVENIENCE (user request): Shift now sprints at
                         RUN_SPEED * DEBUG_SPRINT_MULTIPLIER (30 m/s) for
                         crossing the valley on foot. RUN_SPEED unchanged.
                         Revisit at the movement/combat grill.
- 09:08:2026 - 22:18:17: Jump/crouch/swim implementation. Takeoff speed
                         DERIVED from JUMP_HEIGHT and the float draft
                         derived from the eye height — neither is a second
                         NUMBERS row that could drift from the first.
- 09:08:2026 - 22:29:52: Latch the interact / light / inventory keys.
- 09:08:2026 - 22:40:04: Latch inventory navigation (arrows, wheel, Enter).
*/

#include "engine/gameplay/sources/PlayerMovement.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include "engine/core/config/sources/Constants.h"
#include "engine/physics/sources/CollisionLayers.h"

namespace dfn::gameplay {

namespace {

// NUMBERS constants, narrowed once (generated header emits doubles).
constexpr float DT = static_cast<float>(config::SIM_DT);
constexpr float GRAVITY = static_cast<float>(config::GRAVITY);
constexpr float WALK_SPEED = static_cast<float>(config::WALK_SPEED);
// DEBUG CONVENIENCE (user request, 09:08:2026): Shift sprints at
// RUN_SPEED * DEBUG_SPRINT_MULTIPLIER = 30 m/s so the valley can be crossed in
// seconds while it is being built out. RUN_SPEED itself stays the game-design
// value (6 m/s) and must not be touched. REVISIT at the movement/combat grill —
// this must NOT ship as the release feel.
constexpr float SPRINT_SPEED =
    static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);
constexpr float MOUSE_SENSITIVITY = static_cast<float>(config::MOUSE_SENSITIVITY);
constexpr float PITCH_LIMIT = static_cast<float>(config::CAMERA_PITCH_LIMIT);
constexpr float EYE_HEIGHT = static_cast<float>(config::PLAYER_EYE_HEIGHT);
constexpr float STAND_HEIGHT = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
constexpr float CROUCH_HEIGHT = static_cast<float>(config::CROUCH_CAPSULE_HEIGHT);
constexpr float CROUCH_EYE = static_cast<float>(config::CROUCH_EYE_HEIGHT);
constexpr float CROUCH_SPEED = static_cast<float>(config::CROUCH_SPEED);
constexpr float CROUCH_BLEND_TIME = static_cast<float>(config::CROUCH_TRANSITION_TIME);
constexpr float SWIM_SPEED = static_cast<float>(config::SWIM_SPEED);
constexpr float SWIM_ENTER_DEPTH = static_cast<float>(config::SWIM_ENTER_DEPTH);
constexpr float SWIM_EXIT_DEPTH = static_cast<float>(config::SWIM_EXIT_DEPTH);
constexpr float SWIM_EYE_ABOVE = static_cast<float>(config::SWIM_FLOAT_EYE_ABOVE_WATER);
constexpr float WADE_FACTOR = static_cast<float>(config::WADE_SPEED_FACTOR);

// DERIVED, never stored twice (the lead's ruling and the reason only
// JUMP_HEIGHT is a row): the takeoff speed that reaches exactly JUMP_HEIGHT
// under GRAVITY. A tuned height and a tuned velocity as two rows is how they
// end up disagreeing.
const float JUMP_TAKEOFF_SPEED = std::sqrt(2.0f * GRAVITY * static_cast<float>(config::JUMP_HEIGHT));

// How deep a floating body sits: the eye rides SWIM_FLOAT_EYE_ABOVE_WATER
// above the surface, so the feet hang PLAYER_EYE_HEIGHT below that. Derived
// for the same reason as the takeoff speed.
constexpr float SWIM_FLOAT_DRAFT = EYE_HEIGHT - SWIM_EYE_ABOVE;

// True when the player can straighten up: nothing solid within standing height
// above the feet. The ray starts INSIDE the crouched capsule (never on the
// ground plane, where a ray can hit the floor it is standing on) and looks for
// the ceiling that would trap a standing capsule.
[[nodiscard]] bool has_standing_room(const platform::IPhysics& backend,
                                     const glm::vec3& feet) {
    constexpr float START = 0.5f * CROUCH_HEIGHT;
    const platform::RayHit hit =
        backend.raycast(feet + glm::vec3{0.0f, START, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f},
                        STAND_HEIGHT - START, dfn::physics::LAYER_STATIC);
    return !hit.hit;
}

} // namespace

void accumulate_input(const platform::IInput& input, PlayerState& state) {
    // Look accumulates across frames between fixed ticks (no lost motion).
    state.pending_look += input.mouse_delta();

    // Key axes: latest sample wins. +y forward (W), +x strafe right (D).
    glm::vec2 axes{0.0f};
    if (input.is_down(platform::Key::W)) {
        axes.y += 1.0f;
    }
    if (input.is_down(platform::Key::S)) {
        axes.y -= 1.0f;
    }
    if (input.is_down(platform::Key::D)) {
        axes.x += 1.0f;
    }
    if (input.is_down(platform::Key::A)) {
        axes.x -= 1.0f;
    }
    state.move_axes = axes;
    state.run = input.is_down(platform::Key::LEFT_SHIFT) ||
                input.is_down(platform::Key::RIGHT_SHIFT);

    // Jump LATCHES rather than being sampled. Render runs faster than the fixed
    // tick, so a press and release inside one tick would be sampled as "not
    // down" and the jump would silently never happen — rarely, and only when
    // the frame rate is high, which is the worst kind of bug to chase.
    state.jump_pressed = state.jump_pressed || input.was_pressed(platform::Key::SPACE);
    state.crouch_held = input.is_down(platform::Key::LEFT_CONTROL) ||
                        input.is_down(platform::Key::RIGHT_CONTROL);

    // The action keys latch for the same reason jump does.
    state.interact_pressed =
        state.interact_pressed || input.was_pressed(platform::Key::E);
    state.toggle_light_pressed =
        state.toggle_light_pressed || input.was_pressed(platform::Key::F);
    state.toggle_inventory_pressed =
        state.toggle_inventory_pressed || input.was_pressed(platform::Key::I);

    // Inventory navigation. Latched like the rest, and sampled unconditionally:
    // whether the screen is open is World state, which this ref-based core
    // deliberately cannot see. The latches are simply ignored when it is shut.
    if (input.was_pressed(platform::Key::DOWN)) {
        ++state.pending_selection_delta;
    }
    if (input.was_pressed(platform::Key::UP)) {
        --state.pending_selection_delta;
    }
    // Wheel up moves UP the list, which is the direction the wheel points.
    state.pending_selection_delta -=
        static_cast<int32_t>(std::lround(input.scroll_delta().y));
    state.equip_pressed = state.equip_pressed || input.was_pressed(platform::Key::ENTER);
}

void player_pre_step(PlayerState& state, platform::IPhysics& physics, float water_depth,
                     const components::Transform& transform,
                     components::PreviousTransform& prev_transform,
                     const components::CameraPose& camera,
                     components::PreviousCameraPose& prev_camera) {
    // 1. Snapshot discipline (Rule 12 contract): prev <- curr, before anything.
    prev_transform.position = transform.position;
    prev_transform.rotation = transform.rotation;
    prev_transform.scale = transform.scale;
    prev_camera.position = camera.position;
    prev_camera.yaw = camera.yaw;
    prev_camera.pitch = camera.pitch;

    // 2. Look: mouse +x -> +yaw (turn right), mouse +y -> -pitch (look down).
    state.yaw += state.pending_look.x * MOUSE_SENSITIVITY;
    state.pitch = std::clamp(state.pitch - state.pending_look.y * MOUSE_SENSITIVITY,
                             -PITCH_LIMIT, PITCH_LIMIT);
    state.pending_look = glm::vec2{0.0f};

    // 3. Locomotion mode. TWO thresholds on purpose: with one, a player on a
    // shelving shore crosses it every tick and flips between swimming and
    // walking forever. Enter deep, leave shallow.
    state.water_depth = std::max(0.0f, water_depth);
    const bool was_swimming = state.locomotion == Locomotion::Swim;
    bool swimming = was_swimming ? (state.water_depth >= SWIM_EXIT_DEPTH)
                                 : (state.water_depth >= SWIM_ENTER_DEPTH);
    if (swimming) {
        state.locomotion = Locomotion::Swim;
    } else {
        state.locomotion = state.water_depth > 0.0f ? Locomotion::Wade : Locomotion::Ground;
    }

    // 4. Crouch. The capsule changes SIZE, it does not merely lower the camera:
    // the world is voxels, so ceilings in carved tunnels are real geometry and
    // a camera-only crouch would duck under nothing. Standing up is refused
    // while something is overhead, so releasing the key is a request, not a
    // command. Swimming cancels crouch — there is no floor to crouch on.
    const bool want_crouch = state.crouch_held && !swimming;
    if (want_crouch != state.crouched) {
        if (want_crouch) {
            state.crouched = true;
            physics.set_character_height(state.character, CROUCH_HEIGHT);
        } else if (has_standing_room(physics, transform.position)) {
            state.crouched = false;
            physics.set_character_height(state.character, STAND_HEIGHT);
        }
    }
    // Only the camera eases between the two eye heights.
    {
        const float target = state.crouched ? 1.0f : 0.0f;
        const float step = CROUCH_BLEND_TIME > 0.0f ? DT / CROUCH_BLEND_TIME : 1.0f;
        state.crouch_blend = std::clamp(
            state.crouch_blend + std::clamp(target - state.crouch_blend, -step, step), 0.0f,
            1.0f);
    }

    // 5. Movement.
    const glm::vec3 forward{std::sin(state.yaw), 0.0f, -std::cos(state.yaw)};
    const glm::vec3 right{std::cos(state.yaw), 0.0f, std::sin(state.yaw)};
    glm::vec2 axes = state.move_axes;
    if (const float len = glm::length(axes); len > 1.0f) {
        axes /= len; // diagonals are not faster
    }

    glm::vec3 displacement{0.0f};
    if (swimming) {
        // Entering the water kills the fall: without this, a dive carries the
        // accumulated fall speed and drives the player straight into the bed.
        state.vertical_velocity = 0.0f;

        // Swimming follows the LOOK direction in full 3D — looking down and
        // pressing forward dives, which is the whole point of a 3D world with
        // caves under the water. Strafe stays horizontal, as the body does.
        const float cp = std::cos(state.pitch);
        const glm::vec3 look{forward.x * cp, std::sin(state.pitch), forward.z * cp};
        glm::vec3 dir = look * axes.y + right * axes.x;
        if (const float len = glm::length(dir); len > 0.0f) {
            dir /= len;
        }
        displacement = dir * SWIM_SPEED * DT;

        // Buoyancy: with no vertical input, seek the float line where the eye
        // rides just above the surface. Where the bed is shallower than the
        // draft, collision simply stops the descent and the player stands —
        // which is what happens to a real body in chest-deep water.
        if (std::abs(axes.y) < 1e-4f || std::abs(look.y) < 1e-4f) {
            const float target_feet =
                (transform.position.y + state.water_depth) - SWIM_FLOAT_DRAFT;
            const float error = target_feet - transform.position.y;
            displacement.y += std::clamp(error, -SWIM_SPEED * DT, SWIM_SPEED * DT);
        }
    } else {
        float speed = state.crouched ? CROUCH_SPEED
                                     : (state.run ? SPRINT_SPEED : WALK_SPEED);
        if (state.locomotion == Locomotion::Wade) {
            speed *= WADE_FACTOR; // water drags, even when you can still stand
        }
        displacement = (right * axes.x + forward * axes.y) * speed * DT;

        // Jump. Grounded only, and never while crouched: a crouch-jump is the
        // classic way to climb geometry that was designed to stop you.
        const bool grounded = physics.character_grounded(state.character);
        if (state.jump_pressed && grounded && !state.crouched) {
            state.vertical_velocity = JUMP_TAKEOFF_SPEED;
        }

        // Gravity (the backend only collides and slides; vertical is ours).
        state.vertical_velocity -= GRAVITY * DT;
        displacement.y = state.vertical_velocity * DT;
    }

    // The latch is cleared whether or not the jump was allowed: a press that
    // arrived mid-air is spent, not banked until landing.
    state.jump_pressed = false;

    physics.move_character(state.character, displacement);
}

void player_post_step(PlayerState& state, platform::IPhysics& physics,
                      components::Transform& transform,
                      components::CameraPose& camera) {
    const glm::vec3 position = physics.character_position(state.character);
    if (physics.character_grounded(state.character) && state.vertical_velocity < 0.0f) {
        state.vertical_velocity = 0.0f;
    }

    transform.position = position;
    // Body faces yaw: default forward is -Z; math rotation about +Y is CCW from
    // above, our yaw is CW, hence the negation (header conventions).
    transform.rotation = glm::angleAxis(-state.yaw, glm::vec3{0.0f, 1.0f, 0.0f});

    // Eye height eases between standing and crouched (the capsule did not).
    const float eye = EYE_HEIGHT + (CROUCH_EYE - EYE_HEIGHT) * state.crouch_blend;
    camera.position = position + glm::vec3{0.0f, eye, 0.0f};
    camera.yaw = state.yaw;
    camera.pitch = state.pitch;
}

} // namespace dfn::gameplay
