
# engine/core/time

## Responsibility

Frame timing for the app loop: monotonic wall-clock and the fixed-timestep
accumulator with render interpolation alpha (Rule 12). Gameplay never touches
these — it receives dt as a parameter.

## Key types

- `Clock` (`sources/Clock.h`) — monotonic timer; `tick()` returns real seconds
  since the previous tick.
- `FixedTimestep` (`sources/FixedTimestep.h`) — `accumulate(frame_dt)` returns the
  number of fixed steps to simulate (clamped by max catch-up); `alpha()` is the
  render interpolation fraction between previous and current sim state.

## Usage example

```cpp
dfn::time::Clock clock;
dfn::time::FixedTimestep ts(dfn::config::SIM_DT,
                            static_cast<uint32_t>(dfn::config::SIM_MAX_CATCHUP_STEPS));
while (running) {
    uint32_t steps = ts.accumulate(clock.tick());
    for (uint32_t i = 0; i < steps; ++i) simulate(dfn::config::SIM_DT);
    render(world, ts.alpha());
}
```

## Dependencies

Uses std `<chrono>` and `engine/core/config` (SIM_DT, SIM_MAX_CATCHUP_STEPS).
Used by engine/app (loop) and engine/render (alpha). Implemented in stage 2;
covered by `tests/core/TimeTests.cpp` (cadence, clamp/drop, alpha range).
