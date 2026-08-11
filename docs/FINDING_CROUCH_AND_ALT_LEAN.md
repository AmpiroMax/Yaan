<!--
Created: 11:08:2026 - 15:20:11
Last updated: 11:08:2026 - 15:20:11
-->
<!--
UPD:
- 11:08:2026 - 15:20:11: Written by sim/character while the session was winding
  down, so the findings live in a FILE and not in a thread (two zones lost a
  day's work yesterday one sentence before writing theirs down). Two
  user-named complaints, both three days old. THE ALT LEAN IS SOLVED, FIXED AND
  MEASURED BOTH WAYS. THE CROUCH IS NOT: the lead's leading hypothesis (we
  measured the settled pose, he is complaining about the transition) is
  REFUTED for the standing arm by a per-frame measurement, and the negative
  result is recorded here in full because it costs the next pass a day
  otherwise.
-->

# FINDING — the ALT lean (SOLVED) and the crouch (NOT SOLVED, hypothesis refuted)

The user's two sentences, verbatim, both repeated over three days:

> присел, стало хуже, ещё ниже камера опустилась словно

> ещё какое-то странное действие при нажатии кнопки option, словно я шеей
> вперед двигаю

## The instrument, and why the old ones could not see either of these

Both complaints are about a TRANSITION — something that happens at the moment a
key goes down. Every capture door this project owns settles, freezes or waits
(`docs/FINDING_RUN_SMEAR.md` cost two days learning that), and on top of that
**the bot could hold a GEAR but could never PRESS A KEY.** So the crouch could
only ever be measured in its settled pose, which is the one shape a transition
complaint is not about, and the ALT lean had no automated arm at all.

New door, gameplay's zone, off unless set, intents only (same path a human key
takes), `engine/gameplay/sources/PlaytestBot.cpp`:

    DFN_PLAYTEST_CROUCH=<t0>:<t1>   hold LEFT_CONTROL for those sim-seconds
    DFN_PLAYTEST_JOG=<t0>:<t1>      hold LEFT_ALT for those sim-seconds
    DFN_PLAYTEST_STILL=1            no move input, no look: the ZERO-DOSE arm

`DFN_PLAYTEST_STILL=1` is the load-bearing one (Rule 48): with it the scripted
press is the ONLY thing that changes in the whole run, so anything the frame log
shows is attributable to the press and to nothing else.

Recipe, identical for every arm below, real renderer (never the null one — the
instrument's own frame rate is part of the measurement):

    DFN_STAND=testbed DFN_NULL_AUDIO=1 DFN_PLAYTEST=soak DFN_PLAYTEST_SEED=1 \
    DFN_PLAYTEST_GAIT=<walk|jog|run> DFN_PLAYTEST_SECONDS=12 \
    [DFN_PLAYTEST_STILL=1] [DFN_PLAYTEST_CROUCH=4:8] [DFN_PLAYTEST_JOG=4:8] \
    DFN_PLAYTEST_DIR=<dir> DFN_FRAME_LOG=<file> ./build_sim/engine/app/dfn_app

---

# BUG 2 — THE ALT LEAN. SOLVED, FIXED, AND MEASURED BOTH WAYS.

## What it is, in one line

**Holding LEFT_ALT while standing perfectly still moves the camera 66.4 mm
FORWARD and 7.2 mm DOWN — against a body that does not move at all.** That is
the user's sentence measured: the head lunges, nothing else does.

## The measurement (per presented frame, no settle, no freeze)

Standing still on the testbed, yaw fixed at 0, no movement input, ALT scripted
down from sim-second 4 to 8. Eye travel is measured from the pre-press eye:

| arm | forward travel | vertical | frames |
|---|---|---|---|
| **BEFORE, ALT held** | **+66.376 mm** | **−7.166 mm** | 12 427 |
| **AFTER, ALT held** | **+0.007 mm** | −0.002 mm | 1 315 |
| **CONTROL, no ALT (Rule 48, zero dose)** | +0.007 mm | −0.002 mm | 12 390 |

The travel eases in over ~0.6 s (48.5 mm at 0.5 s after the press, 66.2 mm at
1.5 s) and eases back out on release — an ease, not a pop, which is why it reads
as *moving your neck forward* rather than as a jump cut.

**The AFTER arm does not merely improve, it lands exactly on the zero-dose
control, to the last digit printed.** The 0.007 mm both share is the log's own
noise floor at this print precision.

## The mechanism, and it is an ASYMMETRY, not a missing gate

`engine/anim/sources/Body.cpp`, `evaluate_body_pose` asks two different
questions, and the file says so itself:

- `gait_w` — *are the feet moving at all* — a fade out of idle, a function of
  SPEED: `clamp(speed / (GAIT_FULL_AT_FRAC · WALK_SPEED))`, i.e. full by
  0.54 m/s, **zero when standing**;
- `run_w` — *which gear* — sim's decision, ferried, never re-derived.

The trunk lean lives inside `gait_pose(...)`, which is blended into the idle
pose **by `gait_w`**. So the drawn body's lean is `gait_w · RUN_LEAN · run_w`,
and a standing player with ALT held leans **nothing** — correctly.

The eye's lean was ferried by the app as
`anim::eye_lean_offset(proportions, drive->run_weight)` — `run_w` **with no
`gait_w` factor at all.** So the camera leaned by the gear alone.

The two zones agreed about the gear and disagreed about whether the feet were
moving. Not two copies of a formula drifting (the defect the ferry was built to
prevent, `Clips.cpp:267`) — a **missing factor**, which is the same defect with
nothing to grep for. The user's own hypothesis, relayed by the lead, was
"attached to the SELECTED GEAR rather than the ACHIEVED SPEED", and that is
exactly right: **confirmed, and the gate that already existed on the other side
is the one that was missing.**

Why it never showed while actually jogging: `gait_w` is 1 at any speed above
0.54 m/s, so eye and trunk agree at every real gait. The defect is confined to
standing still and to the first ~0.1 s of a start — which is precisely the
moment the user names, "при нажатии кнопки".

## The fix (one product, in character's zone, no app change)

`BodyDrive::run_weight` stops being the eased gear and becomes **the lean the
trunk is actually drawn with**. The ease keeps its own integrator:

    drive.gear_weight += (gait_run_weight(drive.gait) - drive.gear_weight) * ease;
    drive.run_weight   = gait_fade(drive.speed_mps) * drive.gear_weight;

`gait_fade()` is now a named function with the two callers that must agree
(the pose blend and this publication) instead of an expression written once and
forgotten once. `gait_pose` keeps receiving `gear_weight`, because everything
inside it is already scaled by the fade through the blend — passing the product
there would square the fade and quietly straighten the trunk at low speed.

**The app is untouched.** It still ferries `drive->run_weight`; that float now
simply means what its two readers always assumed it meant.

## The arm that could have rejected the fix (Rule 30a)

A "fix" that deleted the lean would pass every table above. It does not:
`tests/character/BodyTests.cpp` drives `update_bodies` at `RUN_SPEED` a tick at
a time and asserts the published weight ARRIVES at `eye_lean_offset(…, 1.0)`
within 3 mm, with the step-function control still failing the same bound by
6.6x. **10/10 cases, 663 assertions, green** after the change (`hold_gear()`
now seeds the integrator instead of the published product, or it would have
stopped crossing the transition it exists to cross — noted in the test).

## Evidence (Rule 27)

`docs/acceptance/character-alt-lean-{BEFORE,AFTER,CONTROL}-*-0825317.log`, all
three from the recipe at the top of this file. The BEFORE and CONTROL arms are
decimated 1:10 with the reason stated in their own headers: **this defect is a
sustained displacement, not a between-frames meander, so decimation cannot hide
it** — unlike the run smear, where every frame had to be kept.

**Owed, and not done:** the PIXEL pair. A 66 mm translation at a fixed yaw shows
as parallax, so the frames must be shot facing something NEAR (a wall, a stone),
not the testbed's open ground. The recipe is `DFN_CAPTURE_AFTER=<s>` on the two
arms of the still+ALT run above, at a stand with something within a metre or
two. **And the user must confirm on his own build** — he is the one who can feel
the neck move.

---

# BUG 1 — THE CROUCH. NOT SOLVED. THE LEADING HYPOTHESIS IS REFUTED.

## The state of the disagreement

character measured the SETTLED crouch and reported it better: camera 1.2284 m
against the drawn eye 1.2211 m, body's share of a look-down frame 79.6 % → 11.1 %.
The user says it got WORSE and that the camera goes "even lower". The lead's
leading hypothesis was that both are true because they measure different things:
the settled pose versus the TRANSITION, "a dive with an overshoot that a settled
capture cannot contain" — the exact shape of the run-smear error.

**Measured per presented frame. The transition hypothesis does not survive.**

## What the frame log says, standing arm (the clean one: nothing else moves)

Crouch scripted down at sim-second 4, up at 8, standing still on the testbed:

| quantity | value |
|---|---|
| standing eye | 21.725428 |
| crouched eye | 21.253847 |
| **total drop** | **0.4716 m** |
| descent duration | **0.1489 s** (174 presented frames) |
| ascent duration | 0.1488 s (173 frames) |
| **frames that reverse direction during the descent** | **0** |
| frames that reverse direction during the ascent | 0 |
| **overshoot below the settled crouched height** | **0.0000 mm** |
| largest single-frame step | 5.3 mm (~3.2 m/s, i.e. the ramp) |
| eye height after standing back up | 21.725428 — **bit-identical to before** |

No overshoot. No non-monotonicity. No residual. The camera goes down, stays
down, comes back to exactly where it started. `CROUCH_TRANSITION_TIME` is
0.15 s and the measured ramp is 0.1489 s, so the transition is doing precisely
what it is written to do.

The walking (1.79 → 1.49 m/s) and running (6.0 → 1.5 m/s) arms were shot with
the same script and show the same shape — a ~0.15 s descent with no rebound
beyond the head bob already on screen. They are archived but they are NOT
conclusive on their own, and the reason is the next section.

## What is still MISSING from the instrument, and it is one column

The frame log carries the EYE, in world space. Standing still that is enough,
because the feet do not move. **Walking, it is not**: the eye's absolute height
mixes the crouch with the terrain the bot is walking over, and the two cannot be
separated after the fact. The quantity that decides this is **eye height above
the feet**, and nothing logs the feet per frame.

One column — `position.y` — added to `DFN_SPEED_PROBE` (`PlayerMovement.cpp`,
sim's own probe, which already carries the eye) makes the moving arms readable.
That is the next step and it is cheap. It was not taken today because the
session was winding down and a half-added probe in a shared tree is worse than
none.

## What this refutation does and does not license

**It does NOT mean the user is wrong.** It means the defect is not an overshoot
in the eye's vertical track while standing, and the next hypothesis has to come
from somewhere else. Three candidates are recorded below, unmeasured, in the
order I would take them.

### Candidate A — the drop is LINEAR, and everything else in this camera is eased

`crouch_blend` advances by a clamped fixed step (`PlayerMovement.cpp:442-446`),
so the eye's vertical velocity goes 0 → 3.17 m/s in one tick, holds, and returns
to 0 in one tick. Every other camera quantity in this project eases
exponentially (`fov_scale`, the gear blend, the land dip). A velocity STEP is
what a vestibular system reads as a drop; a velocity RAMP is what it reads as a
squat. This would explain "ещё ниже… словно" — a feeling of falling further than
you did — **while every number above stays exactly as it is.** Cheap to test:
same arm, ease the blend, compare the per-frame velocity profile.

### Candidate B — he is comparing against the previous build and the sign is not what we assume

Before character's change the crouched eye sat at `CROUCH_EYE_HEIGHT` 0.85 m;
now it sits at 1.2284 m. The camera is **0.38 m HIGHER**, and the drop is
**smaller** (0.4716 m against 0.85) and **slower** (3.17 m/s against 5.67).
Every axis of "lower" got better, which is why the two reports cannot both be
about the settled height. Worth asking him one question: whether "ниже" is the
eye's height or how much of the world drops away when he ducks.

### Candidate C — the crouched eye is ABOVE the crouched capsule

`CROUCH_CAPSULE_HEIGHT` is 1.0 m and the crouched eye is 1.2284 m above the
feet. **The camera sits 0.2284 m above the top of the capsule it is crouching
inside.** In a tunnel carved to the crouched capsule the eye is inside — or
through — the ceiling, which would read as the view doing something wrong the
moment he crouches under something. Two zones' numbers (sim's capsule, the rig's
skull) have never been made to meet; `CROUCH_EYE_HEIGHT` was retired from
`docs/NUMBERS.md` without anything taking over the constraint. This is a real
open defect regardless of what the user's sentence turns out to mean.

## And one measured mismatch nobody has ruled on

The crouch ferry lands ONE TICK LATE by design (`App.cpp`, stated in its own
comment: post_step has already run when the ferry is written). During a 9-tick
crouch that is **~0.052 m of vertical disagreement between the drawn head and
the camera, for the length of every transition** — invisible in first person,
visible in the mirror map, and it goes to zero the moment the transition ends,
which is why every settled measurement calls it clean. Small, but it belongs
written down next to the numbers above rather than rediscovered.

---

# DEFERRED — found on the way, not fixed, deliberately left (user's instruction)

1. **The crouched eye sits 0.2284 m above the crouched capsule** (Candidate C
   above). Needs a ruling between `CROUCH_CAPSULE_HEIGHT` and the rig, and
   probably a check that fails when they part.
2. **The crouch blend is linear where the rest of the camera is eased**
   (Candidate A). A NUMBERS row and a shape, not a tuning dial.
3. **The one-tick crouch ferry lag**, 0.052 m of head-vs-camera disagreement
   during the transition only.
4. **`DFN_SPEED_PROBE` has no `position.y`**, so no arm with moving feet can be
   read as "eye height above the ground". One column.
5. **`PreviousCameraPose` should not be a hand-maintained struct at all** —
   raised by sim after the run smear, still open, `Components.h` is the lead's.

Nothing in this list is today's defect; all of it is real and none of it should
be rediscovered from scratch.
