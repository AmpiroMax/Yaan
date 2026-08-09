<!--
Created: 10:08:2026 - 02:23:05
Last updated: 10:08:2026 - 02:23:05
-->
<!--
UPD:
- 10:08:2026 - 02:23:05: v1 spec (user request: «продумайте систему
  автономного play testing»). Bot driver + runtime invariants + artifacts +
  gate; controls per Rule 30; roadmap for the two checks v1 cannot do honestly.
-->

# Autonomous playtest — spec (`engine/gameplay/docs/PLAYTEST.md`)

Owner: agent **sim** (`engine/gameplay`). Requested by the user by name:
«продумайте систему автономного play testing».

## Purpose

A bot plays the real game — real input path, real physics step, real streaming,
real renderer — while runtime invariants watch every tick. Any violation
becomes an **incident** with artifacts (screenshot, position, tick, invariant
name) in a run directory, and the process exits nonzero so a run can gate.
This is the difference between "the tour showed nice frames" and "a player
walked for ten minutes and nothing broke".

## Design rules

1. **Not a special mode.** The bot synthesizes PLAYER INPUT into the same
   `PlayerState` intent fields (`move_axes`, `pending_look`, `jump_pressed`,
   `run`) that `accumulate_input` writes; movement, collision and streaming
   run exactly the code a human exercises. A physics shortcut would test a
   game nobody ships.
2. **Checks are cause-based** (Rule 36): every invariant states WHAT failure
   class it detects, and its exclusions are by cause, never by magnitude.
3. **Deterministic where the game is**: the bot's randomness is a seeded
   `gameplay::Rng`; same seed + same world seed = same walk (frame-time
   incidents excepted — wall-clock is not deterministic and is not meant to be).
4. **The checker itself has controls** (Rule 30): the suite includes runs
   that MUST produce incidents (teleport under the world, an impossible speed
   injection). A green checker that has never fired is a description.

## Modes (`BotMode`)

| Mode | Behavior | Use |
|---|---|---|
| `WaypointPatrol` | visit a list of world x/z points in order, then stop (or loop for a duration) | scripted acceptance walks (the crag tunnel, the castle ford) |
| `RandomExplorer` | pick a random target inside the world bounds whose terrain is resident, walk to it, occasionally jump; re-pick when arrived or stuck | soak-style bug hunting |
| `Soak` | walk a fixed circle around the spawn for N minutes | endurance: streaming churn, memory, frame time |

Bot steering: desired yaw from the target bearing, delivered as
`pending_look` pixels through `MOUSE_SENSITIVITY` (the same integration path a
mouse uses); forward axis 1; arrival radius = `NPC_ARRIVE_RADIUS`. A stuck bot
(commanded motion, ~zero actual displacement, grounded) first tries a jump,
then re-picks (explorer) or records a `stuck` incident (patrol — a scripted
route that cannot be walked IS a finding).

## Invariants (v1), each with its failure class

| Name | Fires when | Class it catches |
|---|---|---|
| `nan_position` | any component of the player transform is not finite | integrator/solver corruption |
| `below_world` | feet y < world floor (app passes the world's minimum terrain height minus margin) | fell through everything — the definite fall-through |
| `below_surface` | feet y < heightfield height − `deep_margin` (margin covers legitimate carved tunnels; app-provided) | fell through terrain into the deep underground |
| `freefall` | airborne with growing fall speed for > 3 s continuously | fell out of the collision world but not yet past the floor |
| `speed_bound` | actual horizontal speed > 1.2 × the fastest legal gait (sprint) | teleports, solver explosions |
| `water_mismatch` | \|drawn water surface − analytic water surface\| > 0.05 m at the player column, both defined | the «vision swims where the body cannot» class (core closed the 7.98 m pond gap in 384093f; this keeps it closed) |
| `stuck` | commanded motion, ~zero displacement, grounded, for > 5 s | geometry traps, embedment symptom |
| `frame_budget` | a render frame took > 50 ms (three 60 Hz frames) | freezes a human would feel; min/mean fps land in the summary regardless |

**Deliberately NOT in v1, recorded so nobody mistakes absence for coverage:**

- `embedded_in_static` (capsule overlapping static geometry) needs an
  IPhysics overlap query — an ADDITIVE platform-contract change (like
  `set_character_height` was) that goes through a group sync. The `stuck`
  invariant catches its common symptom meanwhile.
- `below_surface` cannot distinguish a carved tunnel from a fall-through at
  margins tighter than the deepest legitimate carve; a precise version needs
  core's enclosure truth at the player position (their zone; discussed, not
  yet requested formally).

## Artifacts

Run directory (app creates, e.g. `screenshots/playtest_<mode>_<stamp>/`):

- `incidents.log` — one line per incident: tick, invariant, position, detail.
- `incident_NNN.png` — screenshot at (or one frame after) each incident,
  captured app-side via `IRenderer::save_screenshot` (the tour's path).
  Screenshots are capped (first 20) so a cascading failure cannot fill a disk.
- `summary.txt` — incidents count by invariant, distance walked, sim time,
  wall time, min/mean fps, seed, mode. Written on exit ALWAYS (a summary that
  only exists on success reports nothing when it matters most).

**Exit code**: nonzero iff incidents > 0 — the run can gate a merge.

## Split of responsibilities

- `engine/gameplay/sources/PlaytestBot.{h,cpp}` (sim zone): bot driver,
  invariant checks, incident records, summary/log writers. No renderer, no
  env vars, no file paths of its own beyond what it is handed.
- **App wiring (lead's zone, block supplied by sim)**: `DFN_PLAYTEST` env
  parsing, run-directory creation, binding the check environment
  (`height_at`, `water_surface_at`, drawn-water sampler from `water_bodies()`,
  frame dt), calling the screenshot on new incidents, writing the summary,
  returning the exit code. Same delivery pattern as the interaction wiring
  block, which worked.

Env contract (parsed app-side):
`DFN_PLAYTEST=patrol|explore|soak`, `DFN_PLAYTEST_SECONDS` (default 300),
`DFN_PLAYTEST_SEED` (default 1), `DFN_PLAYTEST_DIR` (default under
`screenshots/`). `DFN_NULL_RENDER=1` combines for headless gating runs
(screenshots then silently skip — null renderer returns false by contract).

## Controls (Rule 30) — shipped with the checker

- `sim_playtest` test teleports the character below the world floor and
  asserts `below_world` fires **within one tick**.
- Injects a 100 m single-tick displacement and asserts `speed_bound` fires.
- Asserts a clean 10-second patrol on flat generated terrain fires NOTHING
  (30a — the case that can pass), and that the bot actually covered distance
  (a bot that stands still passes every invariant vacuously).
- Water mismatch: fed a fake drawn-water sampler offset by 1 m, the check
  fires; fed the analytic value itself, it does not.

## Roadmap

1. v1.1: `embedded_in_static` after the IPhysics overlap-query sync.
2. v1.1: screenshot ring buffer of the last N seconds before an incident
   (context of HOW it happened, not just the aftermath).
3. v2: multi-bot runs (NPC executors under the same invariants).
4. v2: fold the tour's visual checks in — one autonomous QA entry point.
