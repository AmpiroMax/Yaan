<!--
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
-->
<!--
UPD:
- 09:08:2026 - 00:16:55: Stage 1 — public contract documented (headers only, no implementation yet).
-->

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
dfn::time::FixedTimestep ts(dfn::config::SIM_DT, /* SIM_MAX_CATCHUP_STEPS */ 5);
while (running) {
    uint32_t steps = ts.accumulate(clock.tick());
    for (uint32_t i = 0; i < steps; ++i) simulate(dfn::config::SIM_DT);
    render(world, ts.alpha());
}
```

## Dependencies

Uses std `<chrono>` and `engine/core/config` (SIM_DT). Used by engine/app (loop)
and engine/render (alpha). Pending NUMBERS.md entry: `SIM_MAX_CATCHUP_STEPS`.
