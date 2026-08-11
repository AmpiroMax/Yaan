<!--
Created: 11:08:2026 - 13:28:15
Last updated: 11:08:2026 - 13:28:15
-->
<!--
UPD:
- 11:08:2026 - 13:28:15: Recovered from five state captures taken 10:08:2026 23:51-23:52 by the smear investigation, which stalled one sentence before writing them down ("Five samples at full run speed, and they settle it"). The captures survived on disk; this file is them. Owner of the mechanism: sim (engine/gameplay). Written by lead so the finding stops living in a thread.
-->

# FINDING — the picture smears at run, and `fov_scale` is not inert

User's complaint, verbatim and repeated over two days:

> тряска — во время бега происходит, стою норм, иду норм, но при беге
> вся картинка плывет, словно 2 секунды прошлые не затираются

Already excluded by measurement, do not re-run these:
- **MSAA** — excluded by the user himself: `DFN_MSAA=0` did not remove it.
- **LOD cross-fade** — excluded by measurement: the nearest terrain that can
  ever be mid-fade is 254 m away, because LOD nodes are excluded from the
  streamed rect; at RUN, 96.4% of frames have nothing dissolving at all.
- **A single still frame at 6 m/s** — comes back clean. Whatever this is, it
  lives BETWEEN frames.

## The five samples

All five at nominally constant run speed, ~2.4 game-seconds apart, build
`8ec5ae0+dirty`, seed 1, stand 0. Files kept at
`scratchpad/fov_{7.3,9.7,12.1,14.5,16.9}/capture_000.txt`.

| # | game_seconds | speed_mps | stride_phase | fov_y_rad | implied fov_scale |
|---|---|---|---|---|---|
| 1 | 871.304877 | 6.000277 | 0.789229 | 1.339619 | 1.023391 |
| 2 | 873.704419 | 6.000108 | 0.707624 | 1.336768 | 1.021213 |
| 3 | 876.105488 | 6.000193 | 0.687236 | 1.343485 | 1.026345 |
| 4 | 878.505508 | 5.989608 | 0.340134 | 1.343557 | 1.026400 |
| 5 | 880.902011 | 5.999241 | 0.400666 | 1.321620 | 1.009641 |

(`fov_scale` = `fov_y_rad` / `CAMERA_FOV_Y` 1.309, per App.cpp:1903.)

## What the five samples establish

**1. `fov_scale` is NOT inert.** Range 1.321620 … 1.343557 rad — a spread of
0.021937 rad, i.e. **1.257° of vertical FOV** at a speed that never leaves
6.0 m/s. The open question from yesterday ("inert or oscillating") is
answered: it moves.

**2. It is not an ease still in flight.** Under
`PlayerMovement.cpp:486` the update is
`fov_scale += (fov_scale_target(speed) - fov_scale) * (1 - exp(-DT/FOV_EASE))`,
which at a CONSTANT target is monotone by construction. The sequence is
1.0234 → 1.0212 (down) → 1.0263 (up) → 1.0264 → 1.0096 (down).
**Non-monotone. Therefore the target itself is moving.**

**3. It never reaches what the formula demands.** `fov_scale_target`
(StepFeel.cpp:132) with `WALK_SPEED` 1.8 and `RUN_SPEED` 6.0 clamps `t` to 1
at any speed ≥ 6.0, so the target at 6.0 m/s is `FOV_SPEED_SCALE_MAX` = 1.08,
i.e. `fov_y` = 1.41372 rad. We measure 1.0096–1.0264. Inverting the formula,
the speed the target function actually SEES is **≈ 2.3–3.2 m/s**, not 6.0.

## The one candidate that explains all three at once

`PlayerMovement.cpp:437`:

    const float speed = glm::length(moved) / DT;

This is deliberately the **actual displacement the solver granted**, not the
commanded speed — the comment above it says so, and that choice is right for
its original purpose (feet must stop against a wall). But it means `speed` is
a per-tick difference, and if the solver grants uneven displacement from tick
to tick, `speed` jitters even while the player's commanded velocity is
rock-steady.

The snapshot's `speed_mps` is `ps->stride_speed` (App.cpp:1251), which is
**this same value** — so 6.000277 is one tick's sample, not proof of
steadiness across ticks.

Why this is the whole complaint and not just the FOV: the SAME `speed`
feeds three consumers in the same function —

- `fov_scale_target(speed)` — scales the whole projection;
- `bob_amplitude_target(speed)` — head bob amplitude;
- `advance_stride(state.stride_phase, speed, DT)` — the step clock.

A jittering `speed` therefore breathes the FOV, modulates the bob, and
stutters the stride **simultaneously and in phase**, at run only, and not at
all standing still. That is the shape of the user's report exactly: fine
standing, fine walking, the whole picture swimming at run.

## The falsifying measurement (this is the next step, and it is cheap)

Log `speed` (the value at PlayerMovement.cpp:437) **per tick** for 2 seconds
of steady running on flat ground, and report min / max / mean / per-tick
standard deviation.

- If it jitters roughly 2–6 m/s while commanded velocity is 6.0 — mechanism
  confirmed, and the fix is about which velocity each of the three consumers
  should read, not about the FOV curve.
- If it is steady at 6.0 — this whole file is wrong and something downstream
  of the target is moving `fov_scale`; say so and the five samples above
  become the control for whatever comes next.

Rule 30: run the standing-still arm as the control — `speed` must be 0 with
zero spread there, or the instrument is lying.

Do not close this on a measurement alone (Rule 27): the acceptance is a
**frame sequence from our build at run speed**, before and after, archived in
`docs/acceptance/`, because the thing being fixed is something the user sees
between frames.
