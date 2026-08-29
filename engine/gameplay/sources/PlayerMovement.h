/*
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
/// СКОЛЬКО ЧЕЛОВЕК РАСТАЛКИВАЕТ ТЕЛОМ НА ХОДУ, ньютонов (заказ владельца
/// 28.08: «моё тело тоже имеет физические свойства — хочу банки, бутылки, еду
/// толкать, двигать»). Это ВТОРОЙ из двух потолков зоны предметов; первый —
/// сила ХВАТА (engine/app/sources/GrabDrive.h), и разводить их обязательно:
/// иначе «не могу поднять шкаф» и «не могу отпихнуть шкаф ногой» стали бы
/// одной ручкой.
///
/// ЧИСЛО ИЗМЕРЕНО, А НЕ ВЫБРАНО. Умолчание Jolt — 100 Н, и при нём
/// полукилограммовый кубок, задетый на ходу, улетал на 4.8-6.6 м: человек не
/// задевал посуду, а пинал её. 30 Н дали ровно то же (5.8 м) — то есть дело не
/// в «чуть поменьше». 8 Н дают 0.75 м у кубка и 0.02 м у бутыли: посуда
/// отъезжает на шаг и остаётся стоять, а не разлетается по комнате.
///
/// ЖИВЁТ ЗДЕСЬ, А НЕ В NUMBERS.md, потому что о нём пока договорились не две
/// зоны, а одна (правило 14 в его собственной формулировке). Читают его двое —
/// spawn_player и рукав sim_loose_props, — и ровно поэтому это КОНСТАНТА, а не
/// литерал в обоих местах (правило 35).
inline constexpr float PLAYER_PUSH_FORCE_N = 8.0f;

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

    // HOW DEEP IN BRUSH THE WALKER IS, 0 clear .. 1 the heart of the thickest
    // shrub they are standing in. Ferried in, exactly as the two offsets above
    // are, and for the same reason: WHERE the bushes are is the world's
    // knowledge (PropCollision::brush_density_at reads the drag field built
    // from the drawn scatter), and WHAT being in one costs is movement's. The
    // wrapper binds it; the ref-based core takes it as data so a test can put
    // the player in a thicket with no world at all.
    //
    // ZERO IS THE HONEST DEFAULT: a caller that ferries nothing is standing in
    // the open, which is why every existing test and frame is unchanged by
    // this field's arrival.
    float brush_density = 0.0f;
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
