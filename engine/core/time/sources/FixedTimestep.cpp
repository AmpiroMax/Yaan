/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 00:42:03
Module: engine/core/time
File: engine/core/time/sources/FixedTimestep.cpp

Responsibility:
- Fixed-timestep accumulator implementation (Rule 12): whole-step extraction,
  catch-up clamp with excess-time drop, interpolation alpha.

Key items:
- FixedTimestep::accumulate / alpha / reset.

Dependencies:
- Uses: FixedTimestep.h, <cmath>.
- Used by: dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- After a clamped catch-up the excess time is DROPPED (accumulator folded into
  [0, step_dt)) so alpha() stays in [0, 1) — documented header contract.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — implementation.
*/

#include "engine/core/time/sources/FixedTimestep.h"

#include <cmath>

namespace dfn::time {

FixedTimestep::FixedTimestep(double step_dt, uint32_t max_catchup_steps)
    : step_dt_(step_dt), max_catchup_steps_(max_catchup_steps) {}

uint32_t FixedTimestep::accumulate(double frame_dt) {
    if (frame_dt > 0.0) {
        accumulator_ += frame_dt;
    }
    const double whole = std::floor(accumulator_ / step_dt_);
    uint32_t steps = whole < 0.0 ? 0u : static_cast<uint32_t>(whole);
    if (steps > max_catchup_steps_) {
        // Spiral-of-death guard: run the clamped number of steps and drop the
        // rest of the accumulated backlog, keeping only the sub-step remainder
        // so alpha() remains a valid interpolation fraction.
        steps = max_catchup_steps_;
        accumulator_ = std::fmod(accumulator_, step_dt_);
    } else {
        accumulator_ -= static_cast<double>(steps) * step_dt_;
    }
    return steps;
}

double FixedTimestep::alpha() const {
    const double a = accumulator_ / step_dt_;
    return a < 0.0 ? 0.0 : (a >= 1.0 ? 0.0 : a);
}

double FixedTimestep::step_dt() const { return step_dt_; }

void FixedTimestep::reset() { accumulator_ = 0.0; }

} // namespace dfn::time
