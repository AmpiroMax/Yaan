/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/core/time
File: engine/core/time/sources/FixedTimestep.h

Responsibility:
- Fixed-timestep accumulator (Rule 12): converts variable real frame time into a
  whole number of fixed simulation steps plus the interpolation alpha for
  rendering between the last two states.

Key items:
- FixedTimestep: accumulate(frame_dt) -> steps, alpha() for render interpolation.

Dependencies:
- Uses: engine/core/config (SIM_DT from the generated constants).
- Used by: engine/app main loop; engine/render reads alpha for interpolation.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Tick rate comes from the generated constants (SIM_TICK_RATE / SIM_DT in
  NUMBERS.md); never hardcode it (Rule 14).
- max_catchup_steps: pending NUMBERS.md entry SIM_MAX_CATCHUP_STEPS (flagged to
  the lead); app passes the value from constants, no default here.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — accumulator with catch-up clamp and
  interpolation alpha.
*/

#pragma once

#include <cstdint>

namespace dfn::time {

/// Fixed-timestep accumulator.
///
/// The canonical loop (Rule 12):
///   int steps = timestep.accumulate(clock.tick());
///   for (int i = 0; i < steps; ++i) simulate(SIM_DT);   // fixed step
///   render(world, timestep.alpha());                    // interpolate prev->curr
///
/// Simulation therefore always advances in exact SIM_DT increments; rendering is
/// uncapped and blends PreviousTransform -> Transform by alpha().
class FixedTimestep {
public:
    /// `step_dt` — seconds per simulation step (pass config::SIM_DT).
    /// `max_catchup_steps` — upper bound on steps returned by one accumulate()
    /// call; excess accumulated time is DROPPED (spiral-of-death guard after a
    /// stall). Pass the generated constant (pending SIM_MAX_CATCHUP_STEPS).
    FixedTimestep(double step_dt, uint32_t max_catchup_steps);

    /// Adds real elapsed seconds and returns how many fixed steps the caller
    /// must simulate now (0..max_catchup_steps). Negative frame_dt is treated
    /// as 0.
    [[nodiscard]] uint32_t accumulate(double frame_dt);

    /// Fraction [0, 1) of a step accumulated beyond the last returned whole
    /// step — the render interpolation factor between the previous and current
    /// simulation states (Rule 12).
    [[nodiscard]] double alpha() const;

    /// The fixed step duration in seconds (as passed to the constructor).
    [[nodiscard]] double step_dt() const;

    /// Drops accumulated time (level load, debugger resume) so no catch-up
    /// burst happens on the next frame. alpha() becomes 0.
    void reset();

private:
    double step_dt_ = 0.0;
    double accumulator_ = 0.0;
    uint32_t max_catchup_steps_ = 0;
};

} // namespace dfn::time
