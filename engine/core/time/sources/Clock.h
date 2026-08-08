/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/core/time
File: engine/core/time/sources/Clock.h

Responsibility:
- Monotonic wall-clock for the app loop: measures real frame delta time that
  feeds the fixed-timestep accumulator.

Key items:
- Clock: monotonic timer with tick() returning seconds since the previous tick.

Dependencies:
- Uses: <chrono>.
- Used by: engine/app main loop, profiling. NOT by gameplay (Rule 12: gameplay
  never reads wall-clock time).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Only the app loop and tooling may use this; simulation code receives dt as a
  parameter and must not include this header.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — monotonic frame clock.
*/

#pragma once

#include <chrono>

namespace dfn::time {

/// Monotonic frame timer (std::chrono::steady_clock).
///
/// Usage in the app loop:
///   Clock clock;                 // starts "now"
///   while (running) {
///       double frame_dt = clock.tick();
///       ...
///   }
class Clock {
public:
    /// Starts the clock; the first tick() measures from construction.
    Clock();

    /// Seconds elapsed since the previous tick() (or construction), and restarts
    /// the interval. Always >= 0.
    double tick();

    /// Seconds since construction, without restarting anything.
    [[nodiscard]] double elapsed() const;

private:
    std::chrono::steady_clock::time_point start_;
    std::chrono::steady_clock::time_point last_tick_;
};

} // namespace dfn::time
