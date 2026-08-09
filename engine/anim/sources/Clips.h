/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 01:56:45
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
*/

#pragma once

#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/Rig.h"

#include <cstdint>

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
