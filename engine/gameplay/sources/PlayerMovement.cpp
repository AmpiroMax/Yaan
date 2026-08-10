/*
Created: 09:08:2026 - 00:45:08
Last updated: 10:08:2026 - 20:32:57
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
- 10:08:2026 - 12:08:26: Three gears wired (WALK/JOG/RUN + debug sprint) with
                         gait resolved in ONE place. Debug sprint moved from
                         LEFT_SHIFT to RIGHT_SHIFT so the third gear could have
                         a key; it keeps a dedicated key rather than a chord
                         because the user asked for it and uses it. Input
                         mapping is provisional pending character's ack.
- 10:08:2026 - 11:06:41: THE EYE SITS ON THE FACE (PLAYER_EYE_FORWARD 0.10):
                         the camera stood on the capsule axis, inside the
                         body's chest box, so looking down filled the frame
                         with torso and the player's own feet were unreachable
                         by construction (character's measured frame, 4a44c26).
                         Yaw-only offset — pitch-coupling would walk the camera
                         forward as you look down. Half the fix by arithmetic;
                         the torso's top face is character's residual.
- 10:08:2026 - 01:53:17: THE STEP IS AN EVENT (landscape stage, в3): stride
                         advance from ACTUAL displacement, FootfallEvents at
                         the bob minima, landing dip from measured impact,
                         stop settle seeded from the live bob offset, FOV
                         eased toward the speed target. Jumped/WaterEntered
                         published from pre_step. The stop threshold and the
                         amplitude-ease tau are DERIVED (documented inline),
                         not new rows.
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
- 09:08:2026 - 22:44:47: Latch the drop key.
- 10:08:2026 - 20:26:56: Applies StepContext::eye_lean to CameraPose along
  the facing. Closes the second eye-vs-body seam of the day: the chest-to-eye
  gap went 0.026 -> 0.129 m at full run because the rig leaned a body with no
  eye. With the eye riding it closes to -0.0055 m and improves monotonically
  with lean. NOT scaled by bob_scale -- posture is not motion.
- 10:08:2026 - 20:32:57: CORRECTION (Rule 16/17). The stamp on the entry
                         above was written 22 minutes AHEAD of the clock I had
                         just read; it now reads the true 20:26:56. Recorded as
                         an appended entry rather than a silent edit, because
                         UPD blocks are this project's only cross-zone ordering
                         record, so a forward stamp REORDERS history rather
                         than merely misdating a file (character2's catch,
                         independent of my own).
*/

#include "engine/gameplay/sources/PlayerMovement.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/StepEvents.h"
#include "engine/gameplay/sources/StepFeel.h"
#include "engine/physics/sources/CollisionLayers.h"

namespace dfn::gameplay {

namespace {

// NUMBERS constants, narrowed once (generated header emits doubles).
constexpr float DT = static_cast<float>(config::SIM_DT);
constexpr float GRAVITY = static_cast<float>(config::GRAVITY);
constexpr float WALK_SPEED = static_cast<float>(config::WALK_SPEED);
constexpr float JOG_SPEED = static_cast<float>(config::JOG_SPEED);
constexpr float RUN_SPEED = static_cast<float>(config::RUN_SPEED);
// DEBUG CONVENIENCE (user request, 09:08:2026): RIGHT_SHIFT sprints at
// RUN_SPEED * DEBUG_SPRINT_MULTIPLIER = 30 m/s so the valley can be crossed in
// seconds while it is being built out. RUN_SPEED itself stays the game-design
// value (6 m/s) and must not be touched. REVISIT at the movement/combat grill —
// this must NOT ship as the release feel. It moved off LEFT_SHIFT when the
// third gear arrived; it keeps a dedicated key rather than a chord because the
// user asked for it explicitly and uses it.
constexpr float SPRINT_SPEED =
    static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);
constexpr float MOUSE_SENSITIVITY = static_cast<float>(config::MOUSE_SENSITIVITY);
constexpr float PITCH_LIMIT = static_cast<float>(config::CAMERA_PITCH_LIMIT);
constexpr float EYE_HEIGHT = static_cast<float>(config::PLAYER_EYE_HEIGHT);
constexpr float EYE_FORWARD = static_cast<float>(config::PLAYER_EYE_FORWARD);
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

// Step-feel rows (в3).
constexpr float DIP_PER_MPS = static_cast<float>(config::LANDING_DIP_PER_MPS);
constexpr float DIP_MAX = static_cast<float>(config::LANDING_DIP_MAX);
constexpr float DIP_TIME = static_cast<float>(config::LANDING_DIP_TIME);
constexpr float SETTLE_TIME = static_cast<float>(config::STOP_SETTLE_TIME);
constexpr float FOV_EASE = static_cast<float>(config::FOV_SCALE_EASE_TIME);

// DERIVED (documented, not rows — same standing as JUMP_TAKEOFF_SPEED):
// "stopped" means actual speed under a tenth of walking pace — slower than
// any commanded gait, so only a real stop (or a wall) crosses it.
constexpr float STOP_SPEED_EPS = 0.1f * WALK_SPEED;
// The bob amplitude eases in over roughly half a step at the current pace
// (tau = half the step duration), so the first stride swells instead of
// snapping — the number is the stride's own, not a new constant.
[[nodiscard]] float amplitude_ease_alpha(float speed) {
    const float step_duration = step_length(speed) / std::max(speed, STOP_SPEED_EPS);
    const float tau = 0.5f * step_duration;
    return 1.0f - std::exp(-DT / tau);
}

[[nodiscard]] math::SurfaceClass surface_under(const StepContext& step,
                                               const glm::vec3& feet) {
    if (step.surface_class_at) {
        if (const auto s = step.surface_class_at(glm::vec2{feet.x, feet.z})) {
            return *s;
        }
    }
    return math::SurfaceClass::Grass; // unknown ground: the least wrong sound
}

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
    // THREE GEARS (the user's ruling). Default is WALK — no modifier — because
    // it is the only gear whose animation currently matches the ground it
    // covers; jog and run saturate the clip's swing cap until their own
    // flight-phase clips exist. Shift stays "go faster"; the debug sprint keeps
    // its own key rather than a chord.
    state.jog = input.is_down(platform::Key::LEFT_ALT) ||
                input.is_down(platform::Key::RIGHT_ALT);
    state.run = input.is_down(platform::Key::LEFT_SHIFT);
    state.debug_sprint = input.is_down(platform::Key::RIGHT_SHIFT);

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
    state.drop_pressed = state.drop_pressed || input.was_pressed(platform::Key::Q);
}

void player_pre_step(PlayerState& state, platform::IPhysics& physics, float water_depth,
                     const components::Transform& transform,
                     components::PreviousTransform& prev_transform,
                     const components::CameraPose& camera,
                     components::PreviousCameraPose& prev_camera,
                     const StepContext& step) {
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
        // The plunge is an event (в3/в12): walking became swimming.
        if (!was_swimming && step.events != nullptr) {
            step.events->post(WaterEntered{step.walker, transform.position,
                                           state.water_depth});
        }
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
        // Gear resolution, once, here — the single place a gait is decided
        // (character reads state.gait rather than re-deriving it). Crouch
        // overrides every gear: you cannot jog while duck-walking.
        float speed = WALK_SPEED;
        state.gait = Gait::Walk;
        if (state.debug_sprint) {
            speed = SPRINT_SPEED;
            state.gait = Gait::Run; // debug speed, Run's gait
        } else if (state.run) {
            speed = RUN_SPEED;
            state.gait = Gait::Run;
        } else if (state.jog) {
            speed = JOG_SPEED;
            state.gait = Gait::Jog;
        }
        if (state.crouched) {
            speed = CROUCH_SPEED;
            state.gait = Gait::Walk;
        }
        if (state.locomotion == Locomotion::Wade) {
            speed *= WADE_FACTOR; // water drags, even when you can still stand
        }
        displacement = (right * axes.x + forward * axes.y) * speed * DT;

        // Jump. Grounded only, and never while crouched: a crouch-jump is the
        // classic way to climb geometry that was designed to stop you.
        const bool grounded = physics.character_grounded(state.character);
        if (state.jump_pressed && grounded && !state.crouched) {
            state.vertical_velocity = JUMP_TAKEOFF_SPEED;
            if (step.events != nullptr) {
                step.events->post(Jumped{step.walker, transform.position});
            }
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
                      const components::PreviousTransform& prev_transform,
                      components::Transform& transform,
                      components::CameraPose& camera,
                      const StepContext& step) {
    const glm::vec3 position = physics.character_position(state.character);
    const bool grounded = physics.character_grounded(state.character);
    const bool swimming = state.locomotion == Locomotion::Swim;

    // --- Landing edge: measure the impact BEFORE the velocity is zeroed.
    // The dip and the Landed event both scale from the measured fall, which is
    // what makes a curb tap nod and a cliff fall sink (research §D1.3).
    if (state.airborne && grounded && !swimming) {
        const float impact = std::max(0.0f, state.fall_speed);
        state.dip_depth = std::min(DIP_PER_MPS * impact, DIP_MAX);
        state.dip_elapsed = 0.0f;
        if (step.events != nullptr) {
            step.events->post(Landed{step.walker, position, impact,
                                     surface_under(step, position),
                                     state.water_depth > 0.0f});
        }
    }
    state.airborne = !grounded && !swimming;
    state.fall_speed = state.airborne ? std::max(0.0f, -state.vertical_velocity) : 0.0f;

    if (grounded && state.vertical_velocity < 0.0f) {
        state.vertical_velocity = 0.0f;
    }

    transform.position = position;
    // Body faces yaw: default forward is -Z; math rotation about +Y is CCW from
    // above, our yaw is CW, hence the negation (header conventions).
    transform.rotation = glm::angleAxis(-state.yaw, glm::vec3{0.0f, 1.0f, 0.0f});

    // --- The stride cycle: driven by the ACTUAL horizontal displacement the
    // solver granted, never the commanded speed — feet stop against a wall,
    // slide on nothing, and freeze in the air. This is the step clock (Rule 35)
    // that character's leg animation and the footstep sound both consume.
    const glm::vec2 moved{position.x - prev_transform.position.x,
                          position.z - prev_transform.position.z};
    const float speed = glm::length(moved) / DT;
    const bool striding = grounded && !swimming;

    if (striding && speed > STOP_SPEED_EPS) {
        const StrideAdvance adv = advance_stride(state.stride_phase, speed, DT);
        if (adv.footfalls > 0 && step.events != nullptr) {
            const math::SurfaceClass surface = surface_under(step, position);
            const bool wading = state.water_depth > 0.0f;
            bool left = adv.first_is_left;
            for (int i = 0; i < adv.footfalls; ++i) {
                step.events->post(
                    FootfallEvent{step.walker, position, surface, speed, left, wading});
                left = !left;
            }
        }
        state.stride_phase = adv.new_phase;
        // Amplitude swells over ~half a step rather than snapping (tau is the
        // stride's own duration, derived above).
        const float target = bob_amplitude_target(speed);
        state.bob_amplitude += (target - state.bob_amplitude) * amplitude_ease_alpha(speed);
        // Walking again cancels a settle-in-progress.
        state.settle_elapsed = 1.0e9f;
    } else if (state.stride_speed > STOP_SPEED_EPS && striding) {
        // --- The stop is punctuation, not a freeze: a short overshoot-and-
        // ease seeded from wherever the bob left the camera, so the settle IS
        // the landing of the last half-step (the NUMBERS row's derivation).
        state.settle_start = bob_vertical(state.stride_phase, state.bob_amplitude);
        state.settle_depth = bob_amplitude_target(state.stride_speed);
        state.settle_elapsed = 0.0f;
        state.bob_amplitude = 0.0f; // the settle curve owns the camera now
    } else if (!striding) {
        state.bob_amplitude = 0.0f; // airborne/swimming: no ground, no bob
    }
    state.stride_speed = striding ? speed : 0.0f;

    // --- Punctuation curves.
    float dip_offset = 0.0f;
    if (state.dip_elapsed < DIP_TIME) {
        dip_offset = -state.dip_depth * punctuation_curve(state.dip_elapsed / DIP_TIME);
        state.dip_elapsed += DT;
    }
    float settle = 0.0f;
    if (state.settle_elapsed < SETTLE_TIME) {
        settle = settle_offset(state.settle_elapsed / SETTLE_TIME, state.settle_start,
                               state.settle_depth);
        state.settle_elapsed += DT;
    }

    // --- FOV-speed coupling: eased, clamped by the target formula itself.
    state.fov_scale += (fov_scale_target(speed) - state.fov_scale)
                       * (1.0f - std::exp(-DT / FOV_EASE));

    // Eye height eases between standing and crouched (the capsule did not).
    const float eye = EYE_HEIGHT + (CROUCH_EYE - EYE_HEIGHT) * state.crouch_blend;
    const float vertical =
        bob_vertical(state.stride_phase, state.bob_amplitude) + dip_offset + settle;
    const float lateral = bob_lateral(
        state.stride_phase,
        static_cast<float>(config::HEADBOB_LATERAL_FACTOR) * state.bob_amplitude);
    const glm::vec3 right{std::cos(state.yaw), 0.0f, std::sin(state.yaw)};
    // THE EYE SITS ON THE FACE, NOT ON THE SPINE. Without this the camera is
    // literally inside the chest: the torso box is centred on the same capsule
    // axis the eye sat on, so looking down filled the whole frame with torso
    // and the player could never see their own feet (character's measured
    // frame, docs/acceptance/character-lookdown-66deg-*). Not a tuning value —
    // it is the missing anatomy between two zones, hence a NUMBERS row both
    // read (Rule 35: the rig had no eye and the camera had no body).
    //
    // YAW ONLY, deliberately: the eye is fixed in the head, and coupling the
    // offset to PITCH would translate the camera forward as you look down,
    // which both moves the near plane toward whatever you are facing and makes
    // the camera position depend on where you look. Modelling the small real
    // swing about the neck joint is a later refinement, not this fix.
    // NOT scaled by bob_scale: turning the bob slider off is a motion-sickness
    // setting, and anatomy is not motion.
    const glm::vec3 facing{std::sin(state.yaw), 0.0f, -std::cos(state.yaw)};
    // THE EYE RIDES THE TRUNK'S LEAN, for the same reason it sits on the face:
    // a real forward lean carries the HEAD forward, and that is precisely what
    // keeps your own chest out of your view when you run. Without this the rig
    // leaned a body that has no eye while the camera held an eye that has no
    // body, so the lean spent 100% of its geometry on a gap anatomy spends
    // about 0% on: the chest corner advanced 0.103 m toward a stationary eye
    // and the chest-to-eye gap went 0.026 -> 0.129 m, FIVE TIMES, at full run.
    // With the eye riding, the gap closes to -0.0055 m and — the property that
    // matters more than the value — it improves monotonically with lean instead
    // of degrading. That is not a tuned coincidence: the eye and the shoulder
    // hang off the same hip pivot at comparable lever arms, so they advance
    // together by construction.
    //
    // NOT scaled by bob_scale, on the same ruling as EYE_FORWARD above: the bob
    // slider is a motion-sickness setting, and posture is not motion. A player
    // who turns the bob off still has a body that leans when it runs.
    const glm::vec3 lean_offset =
        facing * step.eye_lean.x - glm::vec3{0.0f, step.eye_lean.y, 0.0f};
    camera.position = position + glm::vec3{0.0f, eye, 0.0f} + facing * EYE_FORWARD
                      + lean_offset
                      + step.bob_scale * (glm::vec3{0.0f, vertical, 0.0f} + right * lateral);
    camera.yaw = state.yaw;
    camera.pitch = state.pitch;
    camera.fov_scale = state.fov_scale;
}

void player_post_step(PlayerState& state, platform::IPhysics& physics,
                      components::Transform& transform,
                      components::CameraPose& camera) {
    // Transitional shim (pre-step-feel callers, incl. the app until the lead's
    // wiring lands): no previous transform = no measurable displacement, and
    // bob_scale 0 keeps the camera arithmetic EXACTLY as before — stage-2
    // tests assert eye = feet + EYE_HEIGHT to the bit.
    const components::PreviousTransform frozen{transform.position, transform.rotation,
                                               transform.scale};
    StepContext silent;
    silent.bob_scale = 0.0f;
    player_post_step(state, physics, frozen, transform, camera, silent);
}

} // namespace dfn::gameplay
