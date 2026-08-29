/*
Module: engine/core/time
File: engine/core/time/sources/Clock.cpp

Responsibility:
- Monotonic frame clock implementation (steady_clock).

Key items:
- Clock::tick / Clock::elapsed.

Dependencies:
- Uses: Clock.h, <chrono>.
- Used by: dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/core/time/sources/Clock.h"

namespace dfn::time {

namespace {
double seconds_between(std::chrono::steady_clock::time_point from,
                       std::chrono::steady_clock::time_point to) {
    return std::chrono::duration<double>(to - from).count();
}
} // namespace

Clock::Clock() : start_(std::chrono::steady_clock::now()), last_tick_(start_) {}

double Clock::tick() {
    const auto now = std::chrono::steady_clock::now();
    const double dt = seconds_between(last_tick_, now);
    last_tick_ = now;
    return dt < 0.0 ? 0.0 : dt;
}

double Clock::elapsed() const {
    return seconds_between(start_, std::chrono::steady_clock::now());
}

} // namespace dfn::time
