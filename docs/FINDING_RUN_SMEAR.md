
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

---

# THE FALSIFYING MEASUREMENT — RESULT: THE MECHANISM IS REFUTED

Run by `sim`, 11:08:2026 - 13:39:12. Instrument: `DFN_SPEED_PROBE=<path>` (new, permanent,
off unless the variable names a file), one CSV row per FIXED TICK written from
`PlayerMovement.cpp` — the COMMANDED horizontal speed taken from the very
displacement vector handed to the solver in `player_pre_step`, beside the
ACTUAL `speed` computed at what is now `PlayerMovement.cpp:496`, plus the
tick's vertical travel, grounded flag, stride phase and `fov_scale`.

Binary: `build_sim/engine/app/dfn_app`, built from `54bcd15` + this probe,
13:32:10; newest source 13:31:54 — **the binary is newer than every source it
contains**, checked by mtime because `verify_fresh.py --app` is tree-wide and
peers are editing other zones (it flags their stale test binaries, not this
app). Stand 0, seed 1, null audio, null renderer.

## The four numbers, at run

The instrument the file asked for, on the flat testbed ring, over the longest
leg where the commanded speed never left 6.0 (603 ticks = 10.05 s):

| | commanded | **actual `speed`** |
|---|---|---|
| min | 6.0000 | **5.8975** |
| max | 6.0000 | **6.0015** |
| mean | 6.0000 | **5.9987** |
| per-tick sd | 0.0000 | **0.0075** |

**`speed` is 6.0 m/s. It is not jittering between 2 and 6.** The per-tick
standard deviation is 0.0075 m/s — **0.12 % of the commanded speed**. In the
quietest 2-second window of that leg the whole spread is 0.0014 m/s; in the
worst, 0.104 m/s, and that worst window is the one containing the bot's single
4-tick turn-in-place stop.

## The three control arms (Rule 30)

| arm | commanded | actual: min / max / mean / sd | `fov_scale` |
|---|---|---|---|
| **STAND** (no bot, 320 ticks) | 0.0000, sd 0 | 0.0000 / 0.0280 / 0.0045 / 0.0016 | **1.000000, zero spread** |
| **WALK** (soak ring, longest 1.8 leg, 634 ticks) | 1.8000, sd 0 | 1.7591 / 1.8003 / 1.7811 / 0.0076 | 1.000000 … 1.001186 |
| **RUN** (soak ring, longest 6.0 leg, 603 ticks) | 6.0000, sd 0 | 5.8975 / 6.0015 / 5.9987 / 0.0075 | 1.0641 … 1.0800 |
| **RUN, real terrain** (random explorer, 45 s, commanded 6.0 on every one of 2700 ticks) | 6.0000, sd 0 | **3.3399 / 6.8914 / 5.9915 / 0.0896** | 1.07555 … 1.08000 after settle |

**The standing control does not read exactly zero, and the honest number is
0.004578 m/s** — after the spawn drop it is not noise but a CONSTANT creep
(sd 0.000119 over 260 ticks, i.e. 0.076 mm per tick of horizontal slide on the
ground plane). It is 39x below `STOP_SPEED_EPS` (0.1 x `WALK_SPEED` = 0.18 m/s),
so `striding` is false and every consumer sees a hard zero: `fov_scale` is
1.000000 with **zero** spread across the whole arm, `stride_speed` is 0, the bob
is 0. The instrument is not lying — it is showing a sub-millimetre creep the
gate already eats. Reporting it rather than rounding it away, because a control
that reads a suspiciously perfect 0 is the reading this repo has been burned by.

The walking arm behaves as the user describes it: `fov_scale` never leaves
1.0011, because `fov_scale_target` is 1.0 at and below `WALK_SPEED`.

## What this does to the file's three conclusions

**Conclusion 1 stands.** `fov_scale` is not inert; it moves.

**Conclusion 2 stands, and is now sharper.** The target does move. But it does
not move because `speed` jitters.

**Conclusion 3 is where the file goes wrong, and the error is an inference, not
a measurement.** The step "invert the formula, therefore the target function
sees 2.3–3.2 m/s" assumes `fov_scale` had CONVERGED on its target. It had not.
`FOV_SCALE_EASE_TIME` is 0.3 s, so 1.0234 is not "the target is 1.0234" — it is
**0.104 s after the ease started climbing from 1.0**, and 1.0096 is 0.036 s
after. Measured directly: hold 6.0 m/s and `fov_scale` reaches 1.0800, the full
clamp, and stays there. The five samples were five points on a rising ease, not
five points on a moving target.

The mechanism is therefore refuted on its own terms: **the candidate required
`speed` to jitter roughly 2–6 m/s, and it does not.** Per the file's own
falsification clause, the five samples now become the control for the next
hypothesis.

## What the measurement DID find, and it is not nothing

Over 45 s of continuous run across real terrain — commanded 6.0 on every one
of 2700 ticks — `speed` left the 6.0 band **three times**:

| ticks | duration | actual min…max | vertical in the same ticks | `fov_scale` |
|---|---|---|---|---|
| 672–678 | 100 ms | 3.340 … 6.891 | dy up to **+0.173 m** | 1.0800 → 1.0756 |
| 2235–2239 | 67 ms | 4.953 … 5.581 | dy −0.005 (leaves the ground) | 1.0800 → 1.0770 |
| 2472–2476 | 67 ms | 3.725 … 5.818 | dy up to +0.044 m | 1.0800 → 1.0766 |

All three are **STEP-UPS**: the capsule climbing terrain or a ledge trades
horizontal displacement for vertical for a few ticks and then gets it back
(hence the overshoot to 6.89 — the solver returns the withheld travel). 0.27 %
of ticks fall below 5.5 m/s; **none** below 3.34.

The FOV consequence is real but small and fast: **0.33° of vertical FOV, over
100 ms.** That is not 1.257° and it is not two seconds. The whole settled
45-second run spans 1.07555 … 1.08000 of `fov_scale` = **0.333° total**, against
the 1.257° the five samples showed.

One structural note worth keeping even though it is not today's defect: at
`RUN_SPEED` the target is AT its clamp (`t` = 1), so speed jitter at run is
**rectified** — every dip costs FOV and no rise can return it faster than the
0.3 s ease. Every step-up is therefore a one-sided sag-and-recover. At 3 events
per 45 s it is not what the user is reporting.

## What is now excluded by measurement (do not re-run these either)

- **`speed` jitter as the driver of the run smear.** 6.0000 ± 0.0075 m/s per
  tick over 10 s of steady run; 5.9915 ± 0.0896 over 45 s of real terrain.
- **A per-frame / per-tick mismatch in the solver call.** Checked in the loop
  rather than assumed: `App.cpp` calls `physics_->step(step_dt)` INSIDE the
  fixed-step `for` loop, between `player_pre_step` and `player_post_step`. One
  solve per tick, always. Excess accumulated time after a stall is DROPPED by
  `FixedTimestep`, never piled into one solve.
- **The standing and walking arms.** Both quiet, matching the user's "fine
  standing, fine walking" exactly, and for a reason the code states: the FOV
  target is 1.0 at and below `WALK_SPEED`.

## Where the next hypothesis should look

The user's words are «словно 2 секунды прошлые не затираются» — *as if the last
2 seconds are not being erased*. Everything in this file's chain was about a
value that changes too much. That chain is now closed. Two seconds of image
that will not clear is the vocabulary of **accumulation between frames**, not
of a gameplay scalar: the sim quantity behind the projection is steady to 0.12 %
and the whole 45-second FOV excursion is a third of a degree, which no player
sees. The FOV, the bob and the stride are exonerated together, because all three
read the same `speed` and that `speed` is steady.

Handing that direction to whoever picks this up rather than acting on it: it is
render's zone, and this file's job was to falsify sim's candidate, which it did.

---

## RESOLVED 11:08:2026 - 13:47:08 — the cause is one missing line, and the instrument was the whole difficulty

The user found the flaw in our method before we did:

> при прогоне бега — есть тряска, но в момент, когда делается скрин, тряски
> нет, картинка статичная. так что надо по иному скрины делать

He is exactly right, and it explains two days. **Every capture door we own
either freezes the tick (the tour) or waits for the backend to flush (F2, and
the body probe's `cooldown = 4`).** A defect that lives in the DIFFERENCE
between consecutive frames cannot survive any of that. Our clean single frames
were the instrument agreeing with itself.

So the instrument changed: `DFN_FRAME_LOG=<path>` (lead, `engine/app`) writes
ONE LINE PER PRESENTED FRAME — frame index, dt, game clock, speed, `fov_y`,
eye position, yaw, pitch — with no readback, no settle and no cooldown. It
cannot quiet what it is pointed at. Between-frames motion becomes arithmetic on
adjacent lines.

### What it says, first run, 1361 frames of live running

| quantity | run (1349 moving frames) |
|---|---|
| speed | 5.2808 … 6.0015 m/s, sd 0.0330 |
| `fov_y` | **1.309110 … 1.412968 rad** |
| span | **0.103858 rad = 5.951°** |
| per-frame \|Δfov\| | mean 0.0512 rad (**2.93° every frame**) |
| **edge pixel shift from FOV alone** | **mean 9.38 px, max 19.81 px** of a 180 px half-height |
| direction reversals | **97.9%** of changing frame pairs |

97.9% reversals is not drift and not noise. It is a square wave: the projection
alternates between two values on almost every frame, moving the entire image by
about nine pixels at the edge, at 118 frames per second. **That is the smear,
and it is nine pixels of the whole picture, not a subtle one.**

### Controls (Rule 30), same build, same log

| arm | `fov_y` span |
|---|---|
| **run** | **5.951°** |
| walk (883 moving frames) | 1.309000 … 1.309001 — **0.0000°** |
| standing | **0.0000°** |

Fine standing, fine walking, the whole picture swimming at run. The user's
sentence, reproduced as three numbers.

### The cause, and it is not a tuning value

The measured range is 1.309110 … 1.412968. `CAMERA_FOV_Y` is 1.309 and
`CAMERA_FOV_Y` × `FOV_SPEED_SCALE_MAX` is 1.413720. **The observed span is
exactly [CAMERA_FOV_Y, CAMERA_FOV_Y × 1.08], both ends to four decimals.** The
projection is not wandering — it is being swept across the FULL coupling range
and back, once per simulation tick.

`App.cpp` interpolates the eye pose for the render frame:

    const float fs = prev_pose->fov_scale
                   + (pose->fov_scale - prev_pose->fov_scale) * alpha;

`PlayerMovement.cpp:339-341` publishes the previous pose:

    prev_camera.position = camera.position;
    prev_camera.yaw      = camera.yaw;
    prev_camera.pitch    = camera.pitch;

**`fov_scale` is not copied.** `PreviousCameraPose::fov_scale` therefore keeps
its default 1.0 forever, and the interpolation runs from a constant 1.0 to the
live 1.08 as `alpha` sweeps 0 → 1 within every tick — instead of running
between two consecutive `fov_scale` values, which is what interpolation means.

Why nobody saw it: `fov_scale` was added to BOTH components in one change
(Components.h, UPD 10:08:2026 01:52:38) and the note reads «default 1.0 keeps
behaviour». It does keep behaviour — on the side that gets written. The
writer of the shadow copy was never updated, and the default that made the
change safe is the same default that made the defect invisible. **Rule 39: a
shadow copy of a chain becomes a defect the moment the original gains a
branch** — here the branch was a new FIELD, and the rule holds identically.

Why it is run-only, which is the user's own report: at walk `fov_scale_target`
is 1.0, so the stale previous and the live current AGREE and there is nothing
to sweep. The bug's amplitude is exactly the speed coupling's amplitude, so it
switches on with the gear.

### The fix

One line, in `engine/gameplay/sources/PlayerMovement.cpp`, sim's zone:

    prev_camera.fov_scale = camera.fov_scale;   // before camera.fov_scale is rewritten

Placement is the whole care required: it must copy the OLD value, at the same
point where position/yaw/pitch are copied, and `camera.fov_scale` must be
written after (line 549 today).

**Then look for siblings, because the defect is a class, not an instance
(Rule 32).** Any field added to `CameraPose` after this copy was written has
the same hole. `PreviousCameraPose` should not be able to omit a field
silently — a copy that must be maintained by hand will be wrong again.

### Acceptance

Not a still frame — that is the whole point of this file. The run arm of
`DFN_FRAME_LOG` must show the `fov_y` span collapse from 5.951° toward the
walk arm's 0.0000°, with the walk and standing arms unchanged, **and the user
must confirm on his own build**, because he is the one who can see it.

---

# THE EYE TRACK — a new candidate, with the discriminator the user handed us

Added by `sim`, 11:08:2026 - 13:49:03, after the user's observation:

> я вчера вечером заметил, что при прогоне бега - есть тряска
> но в момент, когда делается скрин, тряски нет, картинка статичная
> так что надо по иному скрины делать и искать проблему где-то тут

**The instrument was suppressing the defect.** So the probe stopped being a
speed probe: it now writes, in the same tick and with nothing stopping to record
it, everything that moves the picture — `fov_scale`, `bob_amp`, both bob
offsets, and **the final camera pose**. Binary rebuilt 13:42; same seed, same
route, live ticks, no tour, no freeze.

## The measurement

45 s of running across real terrain, explorer seed 7, then the SAME route at
walk. Per-tick change in the camera's vertical position, **grounded on both
ticks** so no jump or landing is counted:

| | STAND | **WALK 1.8 m/s** | **RUN 6.0 m/s** |
|---|---|---|---|
| grounded ticks | 320 | 8326 (138.8 s) | 2189 (36.5 s) |
| median per-tick eye rise/fall | 0 | 0.00228 m | 0.00509 m |
| p99 | 0 | 0.00684 m | 0.01598 m |
| p99.9 | 0 | 0.02673 m | **0.10317 m** |
| **max, in ONE tick** | 0 | 0.08055 m (4.8 m/s) | **0.16805 m (10.08 m/s)** |
| jumps > 0.05 m in one tick | 0 | 0.007 /s | **0.192 /s** |
| jumps > 0.10 m in one tick | 0 | **0** | **0.082 /s** |

Per metre of ground covered (219 m at run, 250 m at walk — comparable, so this
is not merely "faster means more per second"): **0.032 jumps/m over 5 cm at
run against 0.004 at walk, an 8x difference, and over 10 cm the walk arm has
none at all.**

## What it is

Not the bob. Subtracting the bob offsets leaves the residual essentially
unchanged (sd of the per-tick eye step 0.00857 m, of which the ground track
contributes 0.00645 and the bob 0.00564), and the extremes survive subtraction
intact. It is `position.y` — the capsule's own vertical, which
`player_post_step` hands to `camera.position` **1:1**.

The worst events, all grounded on both sides:

| tick | capsule rise/fall in ONE tick | as a velocity | `speed` that tick |
|---|---|---|---|
| 674 | **+0.1731 m** | +10.38 m/s | 6.423 |
| 2478 | −0.1261 m | −7.57 m/s | 6.001 |
| 682 | −0.1000 m | −6.00 m/s | 6.000 |

These are **step-ups**: `PLAYER_STEP_HEIGHT` is 0.35 m, and the character
controller lifts the capsule onto a ledge WITHIN A SINGLE TICK. The camera rides
it with no smoothing at all. 0.17 m in one tick is **three times the entire
head-bob amplitude** (`HEADBOB_AMPLITUDE_MAX` 0.06) delivered in 16.7 ms, and
**33x the median per-tick camera motion** at the same gait.

## Why it is worse at run than at walk, which is the user's own sentence

At 1.8 m/s a tick advances the capsule 0.030 m of ground; at 6.0 m/s it advances
0.100 m. The controller resolves whatever height change falls inside that span
in ONE tick. So the same micro-relief that a walker climbs smoothly over three
ticks is delivered to a runner as a single snap 3.3x taller — and above
`PLAYER_STEP_HEIGHT`-scale features, as a snap that the walking arm never
produces at all. Standing there is no ground traversal, so there is nothing to
snap: `dy` is 0.

**Fine standing, fine walking, the whole picture swimming at run** — measured,
in that order, on the same terrain.

## And this is why two days of screenshots came back clean

The flat testbed soak ring, the arm every automated frame in this project is
shot from, has a per-tick `dy` range of **−0.0131 … +0.0071 m** — thirteen times
smaller than the explorer's worst, and never once above the 0.02 m mark. **The
artifact cannot occur where the camera has been pointed.** That is Rule 27's
"a vantage that cannot fail is not evidence", and it had been failing silently
for two days: the instrument was not only frozen in time, it was also parked on
the one piece of ground with no relief to snap over.

## The contract question this raises (NOT yet actioned)

The original question — "which velocity should each of the three consumers
read" — is closed by the refutation above: `speed` is steady, so all three are
reading a correct number and none of them is the fault.

The question that replaces it is about the EYE, and it is a genuine contract,
not a tuning value: **should `camera.position.y` ride `position.y` raw?** The
capsule's vertical is a COLLISION RESULT — it may legitimately teleport, because
a solver resolving a step is not modelling a body being lifted. A head is not
attached to a collision capsule; it is attached to legs, and legs cannot raise a
skull 0.17 m in 16.7 ms. The standard treatment is a step-smoothing offset: the
capsule snaps, the eye keeps the old height and decays the difference away over
a short time constant, so the ledge is climbed rather than teleported.

Deliberately NOT implemented in this pass, for two reasons, both of which are
this file's own lessons:

1. **Rule 27 cannot be satisfied yet.** This is a between-frames artifact, so it
   closes on a LIVE FRAME SEQUENCE at run speed, before and after. The lead owns
   `engine/app` and the capture path and is fixing the instrument; shipping a
   camera change before the instrument that can see it exists would repeat
   exactly the mistake that cost two days.
2. **Rule 30 needs the arm that rejects.** The control is already identified and
   already measured: the flat soak ring, where the artifact is absent, must stay
   absent, and the explorer arm's 0.192 jumps/s over 5 cm must go to zero. Any
   before/after must be shot on TERRAIN WITH RELIEF, not on the ring.

Nothing here is a new NUMBERS row yet. If the smoothing lands, its time constant
is a row and it will be requested from the lead (Rule 14/35), derived from the
step height and the gait rather than dialled by eye.

---

# THE FIX, AND ITS BEFORE/AFTER (sim, 11:08:2026 - 13:56:21)

## The change

`PlayerMovement.cpp`, in the snapshot block of `player_pre_step`:

    prev_camera.fov_scale = camera.fov_scale;

Placed with the other three copies, so it carries the value `camera.fov_scale`
held at the END OF THE PREVIOUS TICK — `player_post_step` has not yet
overwritten it this tick. `prev` = tick N−1, `current` = tick N, which is what
the app's alpha blend has always assumed it was handed.

## Before/after, measured with `DFN_FRAME_LOG` (the lead's instrument)

Recipe, identical across all six runs: `DFN_STAND=testbed DFN_NULL_AUDIO=1
DFN_PLAYTEST=soak DFN_PLAYTEST_GAIT=<gait> DFN_PLAYTEST_SEED=1
DFN_PLAYTEST_SECONDS=12 DFN_FRAME_LOG=<file>`, real renderer (the null renderer
runs uncapped at ~95 000 fps and would divide the per-frame delta by a thousand
— **the instrument's own frame rate is part of this measurement**). Standing arm
uses `DFN_CAPTURE_AFTER=8` instead, since the bot always moves.

**The BEFORE arm is a real control binary, not a recollection**: the same tree
with that one line commented out, built, run, and then restored and rebuilt. It
reproduced the lead's run frame-for-frame — 1361 frames — which is the
determinism check on the comparison.

Startup dropped (30 frames: the world streams in at 100–300 ms per frame and the
FOV legitimately eases up from 1.0 for the first time).

| arm | frames | mean per-frame Δfov_y | max Δ | direction reversals | edge shift, mean | edge shift, max |
|---|---|---|---|---|---|---|
| **BEFORE / run** | 1331 | **2.9040°** | 4.7177° | **98.3 %** | **9.319 px** | 15.833 px |
| **AFTER / run** | 1333 | **0.0022°** | 0.1627° | **2.8 %** | **0.007 px** | 0.517 px |
| BEFORE / walk | 1365 | 0.0002° | 0.0024° | — | 0.00 px | 0.01 px |
| AFTER / walk | 1364 | 0.0000° | 0.0001° | — | 0.00 px | 0.00 px |
| BEFORE / stand | 825 | **0.0000°** | 0.0000° | 0 % | 0.00 px | 0.00 px |
| AFTER / stand | 805 | **0.0000°** | 0.0000° | 0 % | 0.00 px | 0.00 px |

**1320x less per-frame FOV motion at run. The meander is gone: 98.3 % of
consecutive frame pairs reversed direction before, 2.8 % after.** Edge shift is
the whole image's displacement at the frame edge for a half-height of 180 px,
`180 · Δtan(fov/2) / tan(fov/2)` — **9.3 pixels every frame becomes 0.007.**

**Both controls are unchanged, and one of them is the point.** Standing measures
0.0000° in BOTH arms — the fix cannot have "improved" a number that was already
zero, so the improvement at run is attributable to running and not to the
binary. Walking was already quiet for the reason the code states (the target is
1.0 at and below `WALK_SPEED`) and stays quiet.

### The one number that does NOT change, and why that is correct

The RANGE of `fov_y` is ~5.9° in both arms (1.3094…1.4132 before,
1.3107…1.4137 after). That is not a residual defect — it is the speed coupling
doing its job. On the soak ring the bot stops to turn and re-accelerates, so the
FOV legitimately travels its whole span. **Before the fix that span was
traversed inside every tick, as a meander; after it, once per acceleration,
monotonically.** Which is exactly why range was the wrong statistic to have
reasoned from: the five state captures at the top of this file measured RANGE,
and range is identical on a healthy build and a sick one. The discriminator is
the per-frame delta and the reversal rate — quantities no single frame, and no
pair of frames 2.4 seconds apart, can express (Rule 30's mechanical form: no
threshold on the range separates the accepted build from the rejected one).

Evidence archived (Rule 27), all six runs:
`docs/acceptance/sim-run-smear-{before,after}-{run,walk,stand}-116a49f.log`.
The pixel sequence is the lead's, from `engine/app`.

## The sibling sweep (Rule 32) — the defect is a class

Every hand-written shadow copy in the tree, audited:

| site | status |
|---|---|
| `PlayerMovement.cpp` per-tick camera snapshot | **THE DEFECT.** Fixed. |
| `PlayerMovementWorld.cpp:87` spawn `PreviousCameraPose` | `fov_scale` omitted; harmless (spawn value is 1.0 either way) — **now spelled out**, because the omission had the same cause |
| `Body.cpp` mirror-puppet snapshot | copied only `position`; rotation and scale were freezing at their defaults. A no-op today (the puppet's rotation is never written) and **NOT a bug fix** — it is the line that stops being a no-op silently on the day someone rotates the puppet |
| `Body.cpp` `write_segments` | complete (all three fields) |
| `ViewModel.cpp:141` | complete (all three fields) |

**And the structural half, because a mirror maintained by hand will lose the
next field the same way.** The copy site now carries:

    static_assert(sizeof(components::CameraPose) == 24, ...);
    static_assert(sizeof(components::PreviousCameraPose) == 24, ...);
    static_assert(sizeof(components::Transform) == 40, ...);
    static_assert(sizeof(components::PreviousTransform) == 40, ...);

Sizes of BOTH halves of each pair, deliberately, and **not** a check that the
two are equal to each other — an equality check would have waved this exact
defect through, because `fov_scale` was added to both structs in the same edit.
Any field added to either one now fails the build at the copy that must learn
about it, with a message naming this file.

The permanent fix belongs one level down — `PreviousCameraPose` should not be a
separately-declared struct that can drift from `CameraPose` at all. That is
`Components.h`, which is the lead's zone; **raised, not edited.**

## What this file cost, and the one sentence worth keeping

Two days, and none of it was spent on the defect — one missing assignment, found
in a single run once an instrument existed that could not be put to sleep. Every
door this project had for looking at the picture (the tour, the state capture,
the playtest summary) **quiets the thing it is pointed at**, and the artifact
lived strictly between frames. The user said so before we measured it: *"при
прогоне бега тряска есть, а в момент, когда делается скрин, тряски нет"*.

The sim-side numbers in the sections above were all correct and all irrelevant:
`speed` really is 6.0000 ± 0.0075, `state.fov_scale` really does settle at
1.0800 and stay there. **The simulation was healthy the whole time; what was
sick was the copy of it the renderer read.** A probe that logs the value a
system computes cannot see a defect in the value a system PUBLISHES, and this
is the second half of the same lesson as Rule 27's — the instrument must sit
where the consumer sits.
