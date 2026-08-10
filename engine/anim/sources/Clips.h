/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 22:25:12
Module: engine/anim
File: engine/anim/sources/Clips.h

Responsibility:
- Procedural animation clips for the humanoid rig (no animation assets exist
  yet, Rule 5 does not apply to code-generated placeholder motion): idle, gait
  (walk/run from the SAME phase sim owns), crouch, air, landing dip, wave,
  flex. Pure functions: same inputs -> same pose.

Key items:
- idle_pose / gait_pose / air_pose / wave_pose / flex_pose: clip evaluators.
- apply_crouch / apply_land_dip: pose modifiers layered on a base pose.
- ShowcaseClip: the cycle the mirror map's showcase mode steps through.

Dependencies:
- Uses: Rig.h, Pose.h, generated constants (FOOTFALL_PHASE_*).
- Used by: Body.cpp, tests.

Notes:
- THE GAIT CONTRACT (docs/RIG.md, NUMBERS rows FOOTFALL_PHASE_LEFT/RIGHT):
  the LEFT foot's lowest point and the pelvis-bob minimum land exactly on
  FOOTFALL_PHASE_LEFT, the right's on FOOTFALL_PHASE_RIGHT — sim fires
  FootfallEvents at the same rows. Tests compare both against the same
  generated names with an offset-clip control (Rule 30).
- Clip-internal SHAPE values (amplitude ratios, flexion curves) are procedural
  ASSET data (lead ruling, 10:08:2026): documented at the definition in
  Clips.cpp with their gait-literature rationale, deliberately not NUMBERS
  rows. Values a second zone must agree with (the phases) ARE rows.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Never advance a phase here: phase is sim's clock, arriving as a parameter
  (Rule 35, state form). A time-based oscillator in this file is the exact
  defect the seam exists to prevent.
*/
/*
UPD:
- 10:08:2026 - 01:56:45: Initial procedural clip set.
- 10:08:2026 - 20:00:23: anim::Gait + gait_run_weight(): the gear is ferried and looked up, never re-derived from speed (Rules 35, 37).
- 10:08:2026 - 20:22:44: eye_lean_offset() declared — producer/consumer with sim, deliberately not a NUMBERS row.
- 10:08:2026 - 22:25:12: crouch_eye_offset() declared — the crouched camera comes from the RIG, not from CROUCH_EYE_HEIGHT.
*/

#pragma once

#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/Rig.h"

#include <cstdint>
#include <glm/vec2.hpp>

namespace dfn::anim {

// Breathing sway. time_s is the app's animation time (render-visual, seconds).
[[nodiscard]] LocalPose idle_pose(float time_s);

// One evaluator for walk AND run: amplitudes derive from step_length_m (sim's
// length(v) model, ferried in), so the visual stride follows the actual
// distance covered; run_weight in [0,1] layers lean + arm carry on top.
// phase is sim's stride phase in [0,1).
[[nodiscard]] LocalPose gait_pose(const Rig& rig, float phase, float step_length_m,
                                  float run_weight);

// Crouch: lowers the pelvis and folds the legs so the feet stay grounded
// (two-link geometry), blend in [0,1] (sim's crouch_blend).
void apply_crouch(const Rig& rig, float blend, LocalPose& pose);

// Airborne: tucked legs, arms slightly out; vertical_velocity_mps leans the
// tuck (rising vs falling).
[[nodiscard]] LocalPose air_pose(float vertical_velocity_mps);

// Landing dip: both knees + pelvis absorb, dip01 is the envelope (1 at
// touchdown decaying to 0 — the caller owns the timer, keyed off sim's
// Landed event, NOT off a phase value).
void apply_land_dip(const Rig& rig, float dip01, LocalPose& pose);

// Showcase clips (mirror map's techno-demo mode).
[[nodiscard]] LocalPose wave_pose(float time_s);
[[nodiscard]] LocalPose flex_pose(float time_s);

// THE GEAR, ferried from sim's `PlayerState::gait` (the app switches on it
// explicitly; anim sits below gameplay in the DAG so the two enums exist by
// construction — see App.cpp's ferry for why a static_cast is refused).
// Values match gameplay::Gait so a reader can diff the two lists at a glance;
// NOTHING may rely on that — the ferry is a switch, and it is the only place
// allowed to know both.
enum class Gait : uint8_t {
    Walk = 0, // WALK_SPEED, the strolling default
    Jog = 1,  // JOG_SPEED
    Run = 2,  // RUN_SPEED (and the debug sprint)
};

// The run clip's weight for a gear. AUTHORED PER GEAR, NOT INTERPOLATED, and
// that is the whole point (Rule 37): the weight used to be
// (speed - WALK_SPEED) / (RUN_SPEED - WALK_SPEED), which was right while
// there were two gears and became a defect the moment JOG_SPEED 3.0 landed
// between them — jog rendered as a walk leaning 0.286 toward run, a gait
// nobody chose, produced by a map that was never calibrated for the point it
// was being asked about. A table cannot acquire an interior point by
// accident: adding a gear here is a decision someone has to write down.
[[nodiscard]] float gait_run_weight(Gait gait);

// HOW FAR THE EYE MOVES BECAUSE THE TRUNK IS LEANING, for `run_weight` in
// [0,1]. `.x` = forward advance (m, along the facing), `.y` = drop (m,
// positive = down). Zero at a walk.
//
// THE SEAM THIS CLOSES: the trunk pitches about the HIP while sim's camera
// sits bolt upright on the capsule axis, so every degree of lean was spent
// carrying the chest toward a stationary eye — measured, the chest entered
// frame at 45 - 18 x run_weight degrees, i.e. at RUN you met your own chest at
// 27 deg and your feet only at 41. A real lean carries the HEAD forward too,
// which is exactly what keeps your chest out of your own view.
//
// PRODUCER/CONSUMER, DELIBERATELY, and not a NUMBERS row: this zone owns the
// rig and the lean, so it owns the offset between them; the app ferries the
// result and sim adds it to CameraPose along the facing. A row would still be
// two readers, and re-deriving it on sim's side would copy both RUN_LEAN and
// gait_run_weight's authored table.
[[nodiscard]] glm::vec2 eye_lean_offset(const RigProportions& p, float run_weight);

// WHERE THE CROUCH PUTS THE EYE, for `blend` in [0,1] (sim's crouch_blend).
// `.x` = forward advance (m, along the facing), `.y` = drop from the STANDING
// eye height (m, positive = down). Same producer/consumer shape as
// eye_lean_offset above, and the same reason.
//
// THE SEAM THIS CLOSES, and it is the same bug one pose over: the camera used
// `CROUCH_EYE_HEIGHT` 0.85 while apply_crouch dropped the pelvis by half the
// LEG. At full crouch that put the camera 0.3602 m below the drawn skull and
// 0.2478 m below the NECK — inside the chest, reported twice by the user. The
// crouched eye is not a fraction anybody chose: it is where the skull is, and
// this zone is the one that knows.
[[nodiscard]] glm::vec2 crouch_eye_offset(const RigProportions& p, float blend);

enum class ShowcaseClip : uint8_t {
    Idle = 0,
    Walk,
    Run,
    Jump,
    Wave,
    Flex,
};
inline constexpr uint32_t SHOWCASE_CLIP_COUNT = 6;

} // namespace dfn::anim
