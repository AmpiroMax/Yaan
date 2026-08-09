/*
Created: 10:08:2026 - 01:53:17
Last updated: 10:08:2026 - 01:53:17
Module: engine/gameplay
File: engine/gameplay/sources/StepFeel.h

Responsibility:
- The pure math of the step feel (в3): stride-cycle advance with footfall
  crossings, the bob curve whose MINIMA ARE THE FOOTFALLS, the landing-dip /
  stop-settle punctuation curve, and the FOV-speed coupling target. All pure
  functions over floats — unit-testable with no physics, no world, no clock.

Key items:
- step_length(speed): length(v) = STEP_LENGTH_BASE + STEP_LENGTH_PER_MPS * v.
- advance_stride(): phase advance + which feet planted this tick.
- bob_vertical() / bob_lateral(): camera offsets for a phase and amplitude.
- bob_amplitude_target(): amplitude from actual speed, ZERO at rest.
- punctuation_curve() / settle_offset(): fast-down-slow-up micro-curves.
- fov_scale_target(): 1 at walk and below, FOV_SPEED_SCALE_MAX at run, clamped.

Dependencies:
- Uses: generated constants (dfn::config), <cmath>. Nothing else.
- Used by: PlayerMovement post_step (the integrator), ViewModel (counterphase
  sway), tests.

Notes:
- PHASE CONVENTION (the one clock, Rule 35; agreed with character): phase in
  [0,1) covers one full left+right cycle. The LEFT foot plants at
  FOOTFALL_PHASE_LEFT (0.25), the RIGHT at FOOTFALL_PHASE_RIGHT (0.75) —
  registry rows with two consumers by construction (sim fires sound there,
  character plants feet there).
- The vertical bob -A·(1-cos(4πφ))/2 has its minima EXACTLY at 0.25/0.75 and
  zeroes at 0/0.5: the eye rides high mid-swing, dips at the plant. The
  lateral sway sin(2πφ) peaks toward the planted foot at those same phases —
  the half-frequency roll the research recommends over taller vertical bob.
- Every constant is a NUMBERS row (Rule 14); derivations live in the registry.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this file pure: no state, no ECS, no platform types, no clock.
*/
/*
UPD:
- 10:08:2026 - 01:53:17: Created for the landscape stage (шаг как событие).
*/

#pragma once

namespace dfn::gameplay {

// Meters per single step at a given horizontal speed (m/s).
[[nodiscard]] float step_length(float speed);

// Result of advancing the stride phase by one tick of actual displacement.
struct StrideAdvance {
    float new_phase = 0.0f; // [0,1)
    int footfalls = 0;      // 0..2 plants crossed this tick
    bool first_is_left = false;
};

// Advances `phase` by (speed * dt) / (2 * step_length(speed)) and reports
// which footfall phases were crossed. speed <= 0 advances nothing: the phase
// HOLDS on stop and while blocked — feet do not march in place.
[[nodiscard]] StrideAdvance advance_stride(float phase, float speed, float dt);

// Vertical bob amplitude target for an actual horizontal speed: linear in
// speed through HEADBOB_AMPLITUDE_AT_WALK at WALK_SPEED, capped at
// HEADBOB_AMPLITUDE_MAX, and ZERO at zero speed by construction — the
// rejected «парение» is the stationary case, and it must produce nothing.
[[nodiscard]] float bob_amplitude_target(float speed);

// Camera offsets for a phase and (eased) amplitude. Vertical is <= 0 (the
// cycle dips from the rest height, never rises above it — rising bob is the
// motion-sickness variant). Lateral is signed, applied along the right axis;
// its `amplitude` parameter is the lateral amplitude itself (the camera
// passes HEADBOB_LATERAL_FACTOR x vertical, the hand sway passes its own
// row) — one waveform, two consumers, each with its own registry amplitude.
[[nodiscard]] float bob_vertical(float phase, float amplitude);
[[nodiscard]] float bob_lateral(float phase, float amplitude);

// The punctuation micro-curve: u in [0,1] -> depth weight in [0,1].
// Fast down over the first third, slow ease back over the rest — the
// «3–5 кадров кривой» shape from the research, normalized.
[[nodiscard]] float punctuation_curve(float u);

// The stop-settle offset: starts from wherever the bob left the camera
// (start_offset, <= 0), overshoots down to -depth, eases back to 0. Starting
// from the live offset is what makes the stop a continuation of the last
// half-step instead of a pop-then-dip.
[[nodiscard]] float settle_offset(float u, float start_offset, float depth);

// FOV multiplier target: 1 at <= WALK_SPEED, FOV_SPEED_SCALE_MAX at >=
// RUN_SPEED, linear between. The clamp is the row's contract: debug sprint
// (30 m/s) widens exactly as much as an honest run.
[[nodiscard]] float fov_scale_target(float speed);

} // namespace dfn::gameplay
