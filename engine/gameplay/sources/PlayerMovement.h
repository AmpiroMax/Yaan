/*
Created: 09:08:2026 - 00:45:08
Last updated: 10:08:2026 - 22:37:10
Module: engine/gameplay
File: engine/gameplay/sources/PlayerMovement.h

Responsibility:
- First-person player movement: mouse look (pitch clamp), WASD + run intent,
  gravity, driving the IPhysics character, and publishing the fixed-tick
  Transform/CameraPose snapshot pairs render interpolates (Rule 12; contract
  ACKed with render and the lead at the stage-1 sync).

Key items:
- PlayerState: per-player component (Rule 8) — character handle, look angles,
  vertical velocity, accumulated input intent.
- accumulate_input / player_pre_step / player_post_step: ref-based core,
  unit-testable with the null backend, no World required.
- spawn_player + World-facing wrappers: what engine/app calls.

Dependencies:
- Uses: core components (lead-owned pairs), platform IInput/IPhysics
  interfaces, core ecs (EntityId; World forward-declared), generated constants.
- Used by: engine/app fixed-tick loop, tests.

Notes:
- Tick order (agreed with the lead): render frame -> player_accumulate_input
  (after IInput::update); each fixed tick -> player_pre_step, THEN the app
  calls IPhysics::step(SIM_DT), THEN player_post_step. Movement code never
  calls step() itself.
- Conventions (render sync): right-handed Y-up, +X east, +Z south; yaw 0
  faces -Z, positive yaw = clockwise from above; positive pitch = look up;
  forward = (sin yaw, 0, -cos yaw). Mouse +x -> +yaw, mouse +y -> -pitch.
- All numbers from dfn::config (Rule 14): WALK/RUN_SPEED, GRAVITY,
  MOUSE_SENSITIVITY, CAMERA_PITCH_LIMIT, PLAYER_* capsule constants.
- Look pixels ACCUMULATE across render frames between fixed ticks (no lost
  mouse motion when render outpaces sim); key axes are latest-sampled.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Snapshot discipline is contract: pre_step copies curr->prev for BOTH pairs
  before any mutation; only post_step writes new curr values.
- The PLAYER is not an NPC: NpcAction (Rule 15) governs NPCs; this system is
  the player-input path and never touches NPC state.
*/
/*
UPD:
- 10:08:2026 - 12:08:26: THREE GEARS (user ruling): WALK 1.8 / JOG 3.0 / RUN
                         6.0 + the debug sprint on its own flag. New `Gait`
                         enum on PlayerState — THE one gait decision, which
                         character reads for clip selection instead of
                         re-deriving it from speeds (Rule 35). Default gear is
                         WALK: with only a walk clip in the engine, every gear
                         above it saturates the clip's swing cap and slides
                         (jog 31%, run 60%) — revisit when the flight-phase
                         clips land.
- 10:08:2026 - 11:06:41: Eye moved onto the FACE (PLAYER_EYE_FORWARD): the eye
                         point is now capsule bottom + PLAYER_EYE_HEIGHT +
                         facing * PLAYER_EYE_FORWARD. Consumers deriving from
                         CameraPose (interaction ray, hand anchor) shift
                         forward with it — consequences, not surprises.
- 10:08:2026 - 01:53:17: THE STEP IS AN EVENT (landscape stage, в3): stride
                         cycle in PlayerState (the one step clock, Rule 35 —
                         character's leg animation consumes the same phase);
                         head bob whose minima fire FootfallEvents; landing
                         dip from measured impact; stop settle; FOV-speed
                         coupling via CameraPose.fov_scale; StepContext with
                         EventBus + surface-class callback. Old signatures
                         kept as delegating overloads (app rewires via the
                         lead's block).
- 09:08:2026 - 00:45:08: Stage 2 — initial movement contract + implementation.
- 09:08:2026 - 22:18:17: Jump, crouch and swim (v1, user-approved).
                         Locomotion mode enum; jump LATCHES across render
                         frames; crouch resizes the capsule, not just the
                         camera; water depth arrives as a parameter
                         because deciding where water is belongs to
                         engine/world (core's ruling).
- 09:08:2026 - 22:29:52: Action latches (interact / light / inventory) —
                         edge events survive a fast render loop.
- 09:08:2026 - 22:40:04: Inventory navigation latches (selection, equip).
- 09:08:2026 - 22:44:47: Drop latch.
- 10:08:2026 - 20:26:56: THE EYE RIDES THE TRUNK'S LEAN. StepContext gains
  eye_lean, FERRIED from character (anim::eye_lean_offset), never derived here
  -- deriving it would copy their AUTHORED gait_run_weight table. Not eased on
  this side on purpose: the property is "chest never ahead of eye", which holds
  only if the eye tracks the body's lean at every instant.
- 10:08:2026 - 20:32:57: CORRECTION (Rule 16/17). The stamp on the entry
                         above was written 22 minutes AHEAD of the clock I had
                         just read; it now reads the true 20:26:56. Recorded as
                         an appended entry rather than a silent edit, because
                         UPD blocks are this project's only cross-zone ordering
                         record, so a forward stamp REORDERS history rather
                         than merely misdating a file (character2's catch,
                         independent of my own).
- 10:08:2026 - 22:37:10: StepContext::crouch_eye -- the crouched eye placement,
  FERRIED from character (anim::crouch_eye_offset) exactly as eye_lean is.
  character's lead carve; see the .cpp entry for the 0.4081 m.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "engine/core/components/sources/Components.h"
#include "engine/core/ecs/sources/EntityId.h"
#include "engine/core/math/sources/SurfaceField.h"
#include "engine/platform/input/interfaces/IInput.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::ecs {
class World;
}
namespace dfn::events {
class EventBus;
}

namespace dfn::gameplay {

// How the player is moving right now. Derived every tick from water depth and
// the crouch key; never set directly.
// The three gears the user ruled for (WALK/JOG/RUN rows). THE ONE GAIT
// DECISION: character selects locomotion clips from this field rather than
// re-deriving it by comparing speed against the rows, so a gait can never be
// two different things in two zones (Rule 35 — the defect this project spent
// a morning on was exactly two copies of one quantity).
// The DEBUG sprint reports Run: it is Run's gait at an absurd speed, not a
// fourth gait, and it is scheduled to disappear at the movement grill.
enum class Gait : uint8_t {
    Walk = 0, // WALK_SPEED — a strolling walk; the default gear
    Jog = 1,  // JOG_SPEED — the old "walk", named for what it always was
    Run = 2,  // RUN_SPEED (and the debug sprint)
};

enum class Locomotion : uint8_t {
    Ground = 0, // walking, running, falling — gravity applies
    Wade = 1,   // standing in shallow water: ground rules, reduced speed
    Swim = 2,   // buoyant: no gravity, movement follows the look direction
};

// Per-player component (Rule 8: plain data). Holds controller state at the
// fixed tick plus the input intent accumulated since the previous tick.
struct PlayerState {
    platform::CharacterHandle character{};
    float yaw = 0.0f;               // radians; 0 = -Z, positive = clockwise from above
    float pitch = 0.0f;             // radians; positive = up, clamped by CAMERA_PITCH_LIMIT
    float vertical_velocity = 0.0f; // m/s (gravity integration)
    glm::vec2 pending_look{0.0f};   // pixels accumulated since the last fixed tick
    glm::vec2 move_axes{0.0f};      // x = +strafe right, y = +forward; each in [-1, 1]

    // Gear selection. Default (no modifier) is WALK: with only a walk clip in
    // the engine today, every gear above it slides badly, so the default is
    // the one gear that looks right — revisit when character's flight-phase
    // clips land. `run` keeps its name (Shift) for the callers that set it.
    bool jog = false;           // LEFT_ALT
    bool run = false;           // LEFT_SHIFT
    bool debug_sprint = false;  // RIGHT_SHIFT — debug only, user-requested
    Gait gait = Gait::Walk;     // resolved each tick; read by character

    // Jump: LATCHED like pending_look, not sampled. Render outpaces the fixed
    // tick, so a press and release inside one tick would otherwise be lost —
    // the player would press jump and nothing would happen, occasionally.
    bool jump_pressed = false;
    // Crouch/descend key state and the resulting capsule state. `crouched` is
    // NOT a copy of the key: standing up is refused while a ceiling is in the
    // way, so the key can be released while the capsule stays short.
    bool crouch_held = false;
    bool crouched = false;
    // 0 = standing eye height, 1 = crouched eye height. Only the CAMERA eases;
    // the capsule swaps instantly, because a capsule that interpolated would
    // be briefly the wrong size for the ceiling it is under.
    float crouch_blend = 0.0f;

    Locomotion locomotion = Locomotion::Ground;
    float water_depth = 0.0f; // meters of water above the feet, 0 when dry

    // Action latches. Same discipline and same reason as jump_pressed: these
    // are EDGE events sampled by a render loop that runs faster than the fixed
    // tick, so they are OR-ed in until a tick consumes them. Sampling them
    // would drop presses at high frame rates — silently, and only sometimes.
    bool interact_pressed = false;
    bool toggle_light_pressed = false;
    bool toggle_inventory_pressed = false;
    // Inventory navigation, meaningful only while the screen is open.
    int32_t pending_selection_delta = 0; // + moves down the list
    bool equip_pressed = false;          // put the selected item in the hand
    bool drop_pressed = false;           // let the selected item go into the world

    // --- Stride cycle: THE step clock (в3; Rule 35 state form, agreed with
    // character 10:08:2026). One clock, several consumers: the camera bob's
    // minima, the FootfallEvents audio plays, and character's leg animation
    // all read THIS phase. Convention (cited by character's RIG.md): phase in
    // [0,1) spans one full stride; the LEFT foot plants at FOOTFALL_PHASE_LEFT
    // (0.25), the RIGHT at FOOTFALL_PHASE_RIGHT (0.75). The phase advances
    // only from ACTUAL post-step horizontal displacement (blocked against a
    // wall the feet stop), HOLDS on stop, and is suspended while airborne.
    float stride_phase = 0.0f;  // [0,1)
    float stride_speed = 0.0f;  // actual horizontal speed this tick, m/s
    float bob_amplitude = 0.0f; // eased vertical half-amplitude, m

    // Landing dip: set on the grounded edge from the measured impact speed.
    // dip_elapsed >= LANDING_DIP_TIME means inactive.
    float dip_depth = 0.0f;
    float dip_elapsed = 1.0e9f;
    bool airborne = false;    // tracked across ticks for the landing edge
    float fall_speed = 0.0f;  // downward speed while airborne, for the impact

    // Stop settle: a short overshoot-and-ease when walking ends, seeded from
    // the live bob offset so the stop continues the last half-step.
    // settle_elapsed >= STOP_SETTLE_TIME means inactive.
    float settle_start = 0.0f;
    float settle_depth = 0.0f;
    float settle_elapsed = 1.0e9f;

    // FOV-speed coupling (eased toward fov_scale_target, written to
    // CameraPose.fov_scale each tick).
    float fov_scale = 1.0f;
};

// Everything post_step needs to make the step an EVENT rather than a curve:
// where to publish, who is stepping, what ground is underfoot, and the user's
// bob setting. Default-constructed = silent (no events, full bob) — existing
// tests and the pre-wiring app keep exactly their old behavior.
struct StepContext {
    events::EventBus* events = nullptr; // null = publish nothing
    ecs::EntityId walker{};             // carried in every event
    // Surface class under a world x/z — bind to core's query (app side).
    // Empty = surface unknown, events report Grass (the least wrong sound).
    std::function<std::optional<math::SurfaceClass>(glm::vec2)> surface_class_at;
    // User setting («слайдер боба» — the research's motion-sickness mandate):
    // scales bob/dip/settle amplitudes. 0 disables the motion entirely;
    // events still fire — sound and animation are not what causes sickness.
    float bob_scale = 1.0f;
    // THE EYE RIDES THE TRUNK'S LEAN. `.x` = forward advance along the facing,
    // `.y` = drop (positive = down), both metres, produced by
    // anim::eye_lean_offset() and ferried in by the app.
    //
    // FERRIED, NOT DERIVED, and that is the whole design (Rule 35). sim must
    // not recompute this from the gait, because that would put a second copy of
    // character's AUTHORED gait_run_weight table and of RUN_LEAN on this side —
    // and an authored number with two copies drifts the day it is re-authored.
    // This is BodyDrive::gait run backwards: the LEAN CHARACTER CHOSE, not the
    // gait it was derived from.
    //
    // NOT EASED HERE, deliberately, and this is the subtle half: the property
    // the seam exists for is "the chest is never ahead of the eye", and that
    // holds only if the eye's offset tracks the body's actual lean at EVERY
    // instant. Easing the camera while the body snaps would desynchronise them
    // and open the exact gap this closes, during the transition. If the pop on
    // a gear change wants smoothing, the ease belongs in the PRODUCER so body
    // and eye ease together — one place, still one number.
    glm::vec2 eye_lean{0.0f, 0.0f};

    // THE CROUCHED EYE IS WHERE THE DRAWN SKULL IS. Same shape, same ferry and
    // the same rule as eye_lean above: `.x` = forward advance along the facing,
    // `.y` = drop BELOW THE STANDING EYE HEIGHT (positive = down), metres,
    // produced by anim::crouch_eye_offset(rig, crouch_blend).
    //
    // WHAT IT REPLACED, because a reader will ask why the row is gone: this
    // side used to lower the camera to `CROUCH_EYE_HEIGHT` 0.85 while character
    // folded the body's legs by half the LEG (0.4419 m). Both were honestly "a
    // half"; they were halves of DIFFERENT quantities and differed by 0.4081 m,
    // so at full crouch the camera sat 0.3602 m below the body's own eye and
    // 0.2478 m below its NECK — literally inside the chest. The user reported
    // it twice. There is no camera-side crouch constant any more: the depth of
    // a squat is decided once, by the zone that draws the squat.
    //
    // ZERO IS THE HONEST DEFAULT: a caller that ferries nothing is not crouched
    // as far as the camera is concerned, exactly as it leans not at all. It is
    // also why standing frames are bit-for-bit unchanged by this.
    glm::vec2 crouch_eye{0.0f, 0.0f};
};

// --- Ref-based core (unit-testable without a World) --------------------------

// Once per render frame, after input.update(): accumulates mouse delta into
// pending_look, samples WASD into move_axes, shift into run.
void accumulate_input(const platform::IInput& input, PlayerState& state);

// Fixed tick, BEFORE IPhysics::step: snapshots curr->prev on both pairs,
// applies look, resolves the locomotion mode, integrates gravity or buoyancy,
// and submits the displacement via move_character.
//
// `water_depth` is meters of water standing above the FEET (transform.position,
// the capsule bottom), 0 when dry. It is a parameter rather than something this
// function looks up because deciding where water is belongs to engine/world —
// the truth is `ChunkManager::water_surface_at`, and the drawable lake/river
// primitives are approximations of it that over-cover (core's ruling, on
// record). Passing the depth in also makes swimming testable with no world.
void player_pre_step(PlayerState& state, platform::IPhysics& physics, float water_depth,
                     const components::Transform& transform,
                     components::PreviousTransform& prev_transform,
                     const components::CameraPose& camera,
                     components::PreviousCameraPose& prev_camera,
                     const StepContext& step = {});

// Fixed tick, AFTER IPhysics::step: reads back position/grounded, advances
// the stride cycle from the ACTUAL displacement (prev_transform -> new
// position), publishes FootfallEvent at each bob minimum and Landed on the
// grounded edge, and writes the new current Transform + CameraPose (eye =
// bottom + PLAYER_EYE_HEIGHT + bob/dip/settle offsets; fov_scale from speed).
void player_post_step(PlayerState& state, platform::IPhysics& physics,
                      const components::PreviousTransform& prev_transform,
                      components::Transform& transform,
                      components::CameraPose& camera,
                      const StepContext& step = {});

// Transitional overload (pre-step-feel signature): no previous transform
// means no measurable displacement, so the stride cycle stays parked — the
// exact old behavior. Callers migrate to the full signature; kept so the app
// compiles until the lead lands the wiring block.
void player_post_step(PlayerState& state, platform::IPhysics& physics,
                      components::Transform& transform,
                      components::CameraPose& camera);

// --- World-facing API (what engine/app calls) --------------------------------

// Spawns the player entity with PlayerState + Transform/PreviousTransform +
// CameraPose/PreviousCameraPose and creates the IPhysics capsule from the
// PLAYER_* constants. spawn_pos is the capsule BOTTOM point.
[[nodiscard]] ecs::EntityId spawn_player(ecs::World& world, platform::IPhysics& physics,
                                         const glm::vec3& spawn_pos);

void player_accumulate_input(ecs::World& world, const platform::IInput& input);

// `water_surface_at` answers "what is the water surface height at this x/z, or
// nothing at all" — bind it to world::ChunkManager::water_surface_at. It is a
// callback rather than a ChunkManager parameter so that gameplay does not have
// to know how water is decided, and so the app can bind it the day core's
// accessor lands without any signature here changing.
// An EMPTY callback means a world with no water, which is the honest answer for
// tests and headless tools rather than a silent failure: nothing to swim in.
using WaterSurfaceFn = std::function<std::optional<float>(glm::vec2 world_xz)>;
void player_pre_step(ecs::World& world, platform::IPhysics& physics,
                     const WaterSurfaceFn& water_surface_at = {},
                     const StepContext& step = {});
// The wrapper fills StepContext.walker with the player entity itself; pass
// events + surface_class_at bound app-side. Default = old silent behavior.
void player_post_step(ecs::World& world, platform::IPhysics& physics,
                      const StepContext& step = {});

} // namespace dfn::gameplay
