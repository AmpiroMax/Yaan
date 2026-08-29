/*
Module: tests
File: tests/core/TimeTests.cpp

Responsibility:
- FixedTimestep suite: step extraction, alpha range, catch-up clamp with
  excess drop, reset; Clock sanity.

Dependencies:
- Uses: doctest, dfn_core (time), generated constants.
- Used by: ctest (test_time).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/core/time/sources/Clock.h"
#include "engine/core/time/sources/FixedTimestep.h"

#include <doctest/doctest.h>

using dfn::time::Clock;
using dfn::time::FixedTimestep;

namespace {
constexpr double DT = dfn::config::SIM_DT;
constexpr uint32_t MAX_STEPS = static_cast<uint32_t>(dfn::config::SIM_MAX_CATCHUP_STEPS);
} // namespace

TEST_CASE("accumulates whole steps and keeps the remainder as alpha") {
    FixedTimestep ts(DT, MAX_STEPS);
    CHECK(ts.step_dt() == doctest::Approx(DT));
    CHECK(ts.alpha() == doctest::Approx(0.0));

    // Half a step: no simulation, alpha grows.
    CHECK(ts.accumulate(DT * 0.5) == 0);
    CHECK(ts.alpha() == doctest::Approx(0.5));

    // Another 0.75 steps: one whole step fires, 0.25 remains.
    CHECK(ts.accumulate(DT * 0.75) == 1);
    CHECK(ts.alpha() == doctest::Approx(0.25));

    // Exactly two steps on top of the remainder -> 2 steps, alpha unchanged.
    CHECK(ts.accumulate(DT * 2.0) == 2);
    CHECK(ts.alpha() == doctest::Approx(0.25));
}

TEST_CASE("fixed cadence over many uneven frames") {
    FixedTimestep ts(DT, MAX_STEPS);
    // 600 frames of ~1.7 * DT must simulate floor-exact total steps.
    uint64_t total_steps = 0;
    for (int i = 0; i < 600; ++i) {
        total_steps += ts.accumulate(DT * 1.7);
    }
    // Total simulated time may lag by at most one step's remainder.
    const double simulated = static_cast<double>(total_steps) * DT;
    const double real = 600.0 * DT * 1.7;
    CHECK(real - simulated >= 0.0);
    CHECK(real - simulated < DT);
    CHECK(ts.alpha() >= 0.0);
    CHECK(ts.alpha() < 1.0);
}

TEST_CASE("catch-up clamp drops the excess after a stall (spiral guard)") {
    FixedTimestep ts(DT, MAX_STEPS);
    // A 2-second stall at 60 Hz is 120 steps — far beyond the clamp.
    const uint32_t steps = ts.accumulate(2.0);
    CHECK(steps == MAX_STEPS);
    // Excess was dropped: alpha is a valid fraction and the next normal frame
    // does not burst.
    CHECK(ts.alpha() >= 0.0);
    CHECK(ts.alpha() < 1.0);
    CHECK(ts.accumulate(DT) <= 2);
}

TEST_CASE("reset drops accumulated time; negative dt is ignored") {
    FixedTimestep ts(DT, MAX_STEPS);
    CHECK(ts.accumulate(DT * 0.9) == 0);
    ts.reset();
    CHECK(ts.alpha() == doctest::Approx(0.0));
    CHECK(ts.accumulate(0.0) == 0);
    CHECK(ts.accumulate(-1.0) == 0);
    CHECK(ts.alpha() == doctest::Approx(0.0));
}

TEST_CASE("clock ticks are non-negative and monotonic-ish") {
    Clock clock;
    const double t1 = clock.tick();
    const double t2 = clock.tick();
    CHECK(t1 >= 0.0);
    CHECK(t2 >= 0.0);
    CHECK(clock.elapsed() >= 0.0);
}
