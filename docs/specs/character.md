<!--
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 21:38:46
-->
<!--
UPD:
- 10:08:2026 - 01:56:45: Initial spec: rig contract, rigid-segmented body,
  stride seam with sim, mirror map plan.
- 10:08:2026 - 02:36:34: Sim's stride clock landed (715c9ab): ferry field
  names recorded; future integration-test tolerance pinned at period +/- 1.5
  ticks (sim's quantization bound).
- 10:08:2026 - 11:05:00: THE ACCEPTANCE SHOOT (plan step 2). Frames archived in
  docs/acceptance/. Passed: proportions to the pixel, ground contact, the
  footfall contract in profile, the mirror. Failed: the first-person look-down
  (see the defect below). Three defects recorded as step 3's actual worklist.
- 10:08:2026 - 11:42:00: Corrections after sim's review — the look-down fraction
  (63 %, not 100 %), the FOV those angles were computed against (75, not an
  assumed 60), PLAYER_EYE_FORWARD bounded at 0.10 rather than my 0.18 — plus
  the torso-top ruling (a2) and the assertion owed to sim (a3).
- 10:08:2026 - 12:05:00: USER ACCEPTANCE, four notes: too much chest, more
  rounded, legs closer together, and joints must not bend backwards. Joint
  limits now live in the rig, the walk grew a forefoot rocker, the stance row
  landed, and the segments are bevelled prisms instead of boxes.
- 10:08:2026 - 12:30:00: Two open seams recorded at the pause (a5): nothing
  ferries sim's Gait yet and this zone still re-derives it, which now leaves
  JOG rendering as a 29 % lean toward the run clip; and the thigh-swing cap is
  still a file-local constant that has to agree with sim's rows.
- 10:08:2026 - 12:36:00: a5(i) gains the test it owes — sim's slip instrument is
  blind to gait SELECTION faults by construction, so fixing the ferry and seeing
  no incidents would prove nothing.
- 10:08:2026 - 12:44:00: a5(i)'s test bound to STEADY STATE (sim's catch): the
  first phrasing forbade transition blends and would have gone red on correct
  code. Assert the outcome, not the mechanism.
- 10:08:2026 - 20:00:23: THREE USER NOTES ANSWERED WITH MEASUREMENTS, not with
  a second guess at the same fix (Rule 32): the chest is measured NOT visible
  at level gaze and the shape complaint relocated to the arms (a6); the wave's
  elbow defect found — a hinge does not clamp an off-axis rotation, it DELETES
  it, and the wag was on the elbow (a7); gait selection moved onto the ferried
  enum with the steady-state test and the 0.286 control (a5(i) closed).
- 10:08:2026 - 20:13:01: CORRECTION, and it is mine: I wrote in 19ae71e that
  the gait fix would help the chest. IT DOES THE OPPOSITE — jog's chest entry
  went 39 -> 35 deg, because the authored jog weight leans the trunk MORE than
  the 0.286 did. The real mechanism is a8, and it is bigger than either.
- 10:08:2026 - 20:31:38: a8 CLOSED. sim landed the consumer (0015f93),
  the eye rides the lean, and the frustum test was inverted together with its
  control. The order now holds at every gear and the margin GROWS with speed.
- 10:08:2026 - 20:35:08: a9 gains sim's compression (their transition worry and
  my vacuous steady-state qualifier are one missing piece) and their
  endorsement of the shape.
- 10:08:2026 - 20:41:46: a9 LANDED (this zone's half) — the gear weight is eased
  once in update_bodies and read by both the trunk and, via the app, the eye.
  The steady-state test stops being vacuous in the same change.
- 10:08:2026 - 21:21:19: ITEM b CLOSED, and all three costed options were wrong including
  the lead's ruling. The pelvis box is 7.65 cm INSIDE the hip silhouette, so it
  is not a lever in either direction; the lever is the trunk box, which had no
  row of its own and borrowed the biacromial JOINT SPAN for a body width. One
  expression, no rows, silhouette identical to four decimals. Two hip findings
  filed for lead rather than fixed half-way.
- 10:08:2026 - 21:38:46: a10 OPENED WITH A BIGGER FINDING THAN THE ITEM IT WAS OPENED FOR.
  Chasing the crouch-hunch eye offset found that sim's camera sits 0.36 m BELOW
  this zone's own head at full crouch — a1 returning in a pose nobody
  re-measured — because a comment claims a half-of-the-LEG drop matches a
  half-of-the-EYE-HEIGHT drop. They differ by 0.4081 m. The hunch offset
  itself measures 0.1815 m, larger than the run lean's 0.1320 because the
  crouch does not counter-pitch the head. No producer landed: a function with
  no consumer is the same defect as a row with no reader.
-->

# Spec: character (engine/anim + engine/platform/anim)

## Zone of responsibility

The humanoid rig (frozen bone contract — future NPC rig), pose math,
procedural animation clips, the first-person full body (user decision в11,
option а), the mirror-map puppet + showcase (в11), and later: NPC bodies,
faces, visible equipment, skinning via `IAnim`/ozz. `engine/platform/anim`
(IAnim interface + backends) is also this zone's.

## Public interface

- `engine/anim/docs/RIG.md` — the bone table (15 bones, mesh ids 34..48),
  spaces, the stride seam. Frozen in the Rule 26 sense once consumed.
- `engine/anim/sources/{Rig,Pose,Clips,BodyMesh,Body}.h` — see module README.
  The app-facing surface is `spawn_body` / `spawn_mirror_puppet` /
  `note_landed` / `update_bodies` + `BodyDrive` as the ferry target.
- `engine/platform/anim/interfaces/IAnim.h` — frozen stage-1 contract
  (inherited from sim); unused until skinning.

## Internal design

Rigid segments, not skinning: each bone is one flat-shaded box mesh submitted
as an ordinary `Transform + PreviousTransform + RenderMesh + LocalBounds`
entity through render's existing ECS pass — zero renderer changes, sim's
collision-from-drawn-triangles reasoning untouched (segments have NO
collision; the capsule stays the only player collider). FK is a single
forward pass; identity pose = rest. Gait is a compass-gait model: pelvis
vertical arc DERIVED from the stance-leg pendulum, so grounded stance feet
and plant timing come from one mechanism. Mirroring = L/R bone swap +
(w,x,y,z)→(w,x,−y,−z) + world reflection across the mirror plane.

## Dependencies

- sim: the stride clock lives in their PlayerState (landed 715c9ab; fields
  `stride_phase`, `stride_speed`, `yaw`, `airborne`, `vertical_velocity`,
  `crouch_blend`; step length = `gameplay::step_length(stride_speed)` called
  fresh, never cached); the app ferries it into `BodyDrive` (agreed
  10:08:2026: LEFT plants at `FOOTFALL_PHASE_LEFT`, phase from post-step
  displacement, holds on stop, suspends airborne, `Landed{impact_speed}` for
  the both-feet dip — one measured impact number feeds both sim's camera dip
  and this zone's body dip). The view-model hand (id 32) retires later,
  coordinated — nothing new may assume it permanent.
- render: mesh id range 34..49 blessed; `RenderSystem::register_mesh` (bool,
  refuses collisions and foreign ranges) landing on their side; segments draw
  on the default "prop" program.
- lead: NUMBERS rows BODY_*_FRAC (Winter/Drillis-Contini fractions of
  `PLAYER_CAPSULE_HEIGHT` — deliberately no BODY_HEIGHT row, Rule 35),
  FOOTFALL_PHASE_LEFT/RIGHT; app wiring (ferry + update call + mirror map).

## Step-by-step plan

1. DONE: rig + pose math + procedural clips + body/puppet systems + tests.
2. App wiring block to the lead (registration ferry, BodyDrive ferry,
   update_bodies call, mirror map spawn); land, then Rule 27 frames:
   look-down mid-stride (feet plant on the phase row) and the mirror stand.
3. First-person polish from frames — NOW A MEASURED LIST, not a guess:

   a. **THE EYE IS INSIDE THE CHEST.** Sim puts the camera on the capsule AXIS
      at `PLAYER_EYE_HEIGHT` 1.7 m. The torso box's top face is at the neck,
      `BODY_NECK_HEIGHT_FRAC`·H = 1.566 m, i.e. 13.4 cm straight below the eye,
      and it spans +/-0.233 m across and +/-0.126 m fore-aft AROUND that eye.
      Its forward edge therefore sits atan(0.134/0.126) = 46.8 deg below the
      horizon, so with `CAMERA_FOV_Y` 75 deg the chest enters frame past 9.3
      deg of downward look. **CORRECTED 10:08:2026 - 11:40, and the correction
      is on the record because the first version of this paragraph was
      committed wrong (4a44c26):** the earlier 43 deg / ~13 deg were computed
      against an assumed 60 deg FOV before the row was read, and the claim
      "at −66 deg the frame is 100 % torso" was an eyeball. Measured on the
      archived frame: the chest starts at row 113 of 360 and covers 63 % of it.
      sim caught the overstatement.

      WHAT SURVIVES THE CORRECTION IS THE CONCLUSION, and it is stronger than
      the fraction was: EVERY foot position lies inside the chest's angular
      shadow. The most forward the foot ever gets is the capped visual reach
      0.486 m, which puts it at atan(1.7/0.486) = 74.0 deg of depression
      against a chest edge at 46.8 deg. Not "usually hidden" — geometrically
      unreachable, at every phase, at every pitch.

      This is not a tuning value, it is a missing seam: a real eye is FORWARD
      of the chest, and there is no row for that offset. Fix belongs with sim
      (they own CameraPose). MY ~0.18 m SUGGESTION WAS WRONG TOO — sim bounded
      it at `PLAYER_EYE_FORWARD` = 0.10 m, and both bounds check out here:
      the head mesh's own front face is at head_width/2 * HEAD_DEPTH_RATIO =
      0.09 * 1.15 = 0.1035 m, so 0.18 would have floated the eye 7.6 cm in
      front of its own face — invisible in first person, glaring in the mirror
      map that exists to look at yourself; and 0.18 + `CAMERA_NEAR` = 0.28
      against `PLAYER_CAPSULE_RADIUS` 0.35 leaves the near plane 7 cm from a
      surface the body cannot pass. Frames: `character-lookdown-{66,40}deg-*`.
   a2. **THE TORSO SLAB IS FULL-DEPTH THROUGH THE NECK — my row, my call, and
      the answer is yes, drop it to the shoulder line.** sim routed this back
      with the two numbers that decide it rather than a shape, which is the
      right way round. The torso mesh runs to `torso_len` — the NECK, 1.566 m —
      at full `BODY_TORSO_DEPTH_FRAC`, where a real chest tops out at the
      collarbone, `BODY_SHOULDER_HEIGHT_FRAC`·H = 1.472 m, with a narrower neck
      rising from it. The arithmetic, at sim's eye offset of 0.10 m (chest
      forward edge then 0.026 m ahead of the eye):

      | torso top | chest edge, depression | leading foot at 77.2 deg clears by |
      |---|---|---|
      | neck 1.566 m (today) | atan(0.134/0.026) = 79.0 deg | −1.8 deg: STILL HIDDEN |
      | shoulder 1.472 m | atan(0.228/0.026) = 83.5 deg | +6.3 deg, ~25 px at 640x360 |

      So the eye row ALONE does not put the feet back on screen — it misses by
      about two degrees. The pair does. That is the finding: neither change is
      sufficient alone, and I would rather record that than let sim's row land
      and have the feet still missing. The bone table does not move (the Torso
      bone still spans hip→neck, Rule 26 intact) — only `BodyMesh.cpp`'s box
      for it, plus a narrower neck stub. NOT IMPLEMENTED: the user directs the
      next work; this is the costed decision waiting for him.

   a3. **The assertion I owe sim**, once `PLAYER_EYE_FORWARD` lands: the eye
      must stay behind its own face, `eye_forward <= head half-depth`. Assert
      it against the ACTUAL head segment mesh — `build_body_segment_mesh(
      Bone::Head, ...).bounds_min.z` — not against a re-derived formula, so
      it keeps holding if `HEAD_DEPTH_RATIO` or `BODY_HEAD_WIDTH_FRAC` moves.
      Rule 30 control: an eye_forward of head half-depth + 1 cm must fail it.
      Today the relation is 0.10 <= 0.1035 — 3.5 mm of margin, which is
      exactly why it wants a test and not a comment.

   a4. **DONE — THE KNEES BENT BACKWARDS, and the elbows never did.** The user
      called it «крипово» and he was right. Measured from WORLD joint positions
      across every shipped clip (quaternion signs lie too easily — my first
      metric had the elbow inverted and only a raw coordinate dump caught it):

      | clip | knee | elbow |
      |---|---|---|
      | walk / run | **−33.4 deg**, hyperextended | +17 .. +63, clean |
      | jump / air | +51.6 flexion | +17.2, clean |
      | flex | 0 | +108 (a bicep curl, correct) |
      | idle | 0 | +8.6, clean |

      CAUSE, and it is construction rather than accident: the walk set
      `knee = -thigh` to hold the shin VERTICAL through stance, which is what
      kept the planted foot on the ground. Holding a shin vertical while the
      thigh swings BACKWARD is anatomically impossible — it opens the knee by
      exactly the thigh's swing amplitude, which is 33.4 deg. Same shape as the
      birch: a rule stated for the object that the construction satisfied the
      wrong way.

      FIX, two halves. (1) The rig carries a hinge RANGE per bone and reduces
      those bones to one axis when any pose is evaluated, so a hyperextended or
      twisted knee is UNREPRESENTABLE — covering crouch, the landing dip, the
      showcase reel and anything authored later, not just today's clips.
      (2) The walk clip stops asking: the stance knee is capped and the foot
      rolls over the toe (the forefoot rocker), because clamping alone lifts
      the ankle 7.3 cm and floats the foot. The rocker asks for 22.4 deg of
      toe-off and real toe-off is 20-25 — the model landing on a number it was
      not fitted to, the same check the stance row passed.

      WHAT IT COST ELSEWHERE, recorded because it is a contract change: the
      old clip released the foot exactly when the other foot planted (single
      support). A real walk rolls over the toe and lets go AFTER the other foot
      is down, so the footfall test now asserts double support instead of
      equality — and it measures the SOLE rather than the ankle, since the heel
      lifting is precisely what makes the ankle a liar about ground contact.
      Touch-down, the assertion sim's audio rides on, is untouched at 0.25/0.75.

   a5. **OPEN SEAMS AT THE PAUSE — recorded so neither is rediscovered as a
      mystery.** Both are one line of somebody's work, neither is mine alone.

      i. **NOTHING FERRIES `Gait` YET, and this zone is still re-deriving it.**
         sim's `PlayerState::gait` exists, and their header says in as many
         words that character must select clips from that field "rather than
         re-deriving it by comparing speed against the rows". `evaluate_body_pose`
         still compares against `WALK_SPEED` / `RUN_SPEED`. That is the Rule 35
         defect this project spent a morning on, live, in my file, behind a
         comment forbidding it. It was harmless while there were two gears and
         is not harmless now that there are three — with the user's rows
         (1.8 / 3.0 / 6.0) the measured consequence is:

         | gear | speed | my run-clip weight |
         |---|---|---|
         | walk | 1.8 | 0.00 |
         | **jog** | **3.0** | **0.29** |
         | run | 6.0 | 1.00 |

         So JOG currently renders as a walk leaning 29 % toward the run clip —
         an artifact of a linear blend between two rows that now have a third
         row between them, and a number nobody chose. Fix is the app ferry
         (lead's line) plus switching selection to the enum (mine). Until then
         treat locomotion between 1.8 and 6.0 as unruled.

         **AND IT NEEDS ITS OWN TEST, because sim's slip instrument cannot see
         it** (their limit, raised by them before anyone could over-trust it).
         Today their check WOULD fire at jog — but for the wrong reason: jog
         slips 30.6 % because the swing clamp saturates, so the incident reads
         "foot_slip" while the fault is SELECTION. Once jog and run clips exist
         and their strides match the rows, a wrongly-weighted blend that still
         plants feet on the ground slides by nothing and the instrument is
         silent. Foot slip and gait selection are different classes and only
         the first is covered. The test this zone owes, in the Rule 30 shape:
         hold a `Gait` on PlayerState past any transition blend, THEN assert
         that gear's clip is at full weight — and the rejected instance is
         today's speed-derived selection, which must fail it.

         THE STEADY-STATE QUALIFIER IS LOAD-BEARING and it is sim's catch on my
         first phrasing. "Must not be an interpolation toward a neighbour" is
         right at rest and WRONG mid-transition: accelerating walk→jog, a brief
         blend is correct animation and a hard clip swap would snap. A test
         phrased that way goes red the day someone implements a good
         transition, and they will weaken the test rather than argue with it.
         The control survives the qualifier unharmed — speed-derived selection
         gives jog a 0.286 run-lean after 0.1 s and after an hour, because it
         is a pure function of speed with nothing to settle, so it fails
         exactly where a legitimate transition would not.

         Which is the same move as sim's slip bound, and the pair is worth more
         than either: ASSERT THE OUTCOME, NOT THE MECHANISM. "Residual slip is
         imperceptible" rather than "the clamp is inactive"; "this gait renders
         as this gait" rather than "no interpolation ran". The mechanism-shaped
         assertion is the one that goes red on correct code.

         The generalisable shape, worth more than this instance: **a linear map
         between two named values becomes a latent defect the moment a third
         named value lands between them.** Nothing about the interpolation
         changed; a row appeared in the middle and a correct-looking blend
         silently became a number nobody chose. That is not specific to gaits,
         and may deserve a home wider than this spec.

      ii. **`BODY_THIGH_SWING_MAX_SIN` is requested but not landed.** 0.55 lives
         as a file-local constexpr in Clips.cpp while it now has to AGREE with
         sim's step-length rows — it caps the visual half-step at 0.55 x leg
         length = 0.486 m, and sim's binding check wants to be arithmetic over
         the generated header with no cross-zone include. The check must be
         written as "residual slip stays under a perceptual bound", never as
         "the clamp is inactive": at WALK 1.8 the residual is 0.8 %, and a
         check that goes red the day it is written teaches everyone to ignore
         it. (Agreed with sim independently on both halves.)

   a6. **THE CHEST COMPLAINT, RE-OPENED AND MEASURED — AND THE FIX HAD
      LANDED.** User, 10:08:2026: «когда я хожу, вижу свою грудь, мне не
      нравится, форма персонажа странная». This was the second time, so the
      premise was checked before anything was changed (Rules 32, 34), and the
      premise is FALSE for the shipped build. Two instruments agree:

      | look angle (down) | first body part in frame | torso |
      |---|---|---|
      | 0 deg (walking, level) | NOTHING | not in frame |
      | 25 deg | hand | not in frame |
      | 31 deg | forearm | not in frame |
      | 41 deg | foot | not in frame |
      | **45 deg** | — | **enters** |

      Analytic (every mesh vertex of every segment through
      `evaluate_body_pose` + FK, projected against `CAMERA_FOV_Y` 75 deg and
      the eye at `PLAYER_EYE_HEIGHT` + `PLAYER_EYE_FORWARD`), and confirmed by
      frame: `character-walk-level-gaze-no-chest-b134936.png` at pitch 0.00,
      1.80 m/s, phase 0.23, contains no body at all. The clavicle cut did what
      it was measured to do. A REAL body IS visible at 45 deg of depression,
      so nothing here is a defect to re-fix.

      **THE ONE REAL FINDING, and it is a coupling nobody predicted:** at JOG
      the torso enters at **39 deg — BEFORE the feet at 41**, so the chest
      arrives while the feet are still hidden, which is exactly the complaint.
      Cause is a5(i): the 0.286 run-lean pitched the trunk 0.057 rad forward
      and carried its top corner 13 mm toward the eye. **Fixing the gait
      selection fixes part of the chest complaint** — the two items the user
      reported separately are one defect. Walk was never affected (0 lean).

      WHAT IS NOT ANSWERED BY GEOMETRY: 45 deg is an ordinary glance at the
      ground. If the user still dislikes it, the lever is `CAMERA_FOV_Y` (75
      deg puts the frame edge 37.5 deg below the axis, far past a comfortable
      human downward field) — sim's row, and a taste decision, not a bug.

   a8. **THE TRUNK LEANS AND THE EYE DOES NOT — the actual cause of «вижу
      свою грудь» at any speed above a walk, and it is a SEAM, not a tuning
      value.** Found by checking my own claim (Rule 34 turned inward): 19ae71e
      says the gait-selection fix helps the chest. It does not. Measured:

      | run_weight | chest enters | foot enters | order |
      |---|---|---|---|
      | 0.00 (walk) | 45 deg | 41 deg | feet first — correct |
      | 0.20 | 41 | 41 | the crossover |
      | 0.286 (the old defect) | 39 | 41 | chest first |
      | **0.50 (jog today)** | **35** | 41 | chest first — WORSE than the defect |
      | 1.00 (run) | 27 | 41 | chest first |

      `entry_angle(chest) = 45 - 18 x run_weight` degrees, dead linear, and it
      crosses the foot at run_weight 0.20 — i.e. at 2.3 deg of trunk lean. So
      this cannot be tuned away: a run needs a lean, and ANY lean worth seeing
      already puts the chest in front of the feet.

      MECHANISM: `RUN_LEAN` pitches the torso about the HIP while the eye
      stays bolt upright on the capsule axis at `PLAYER_EYE_HEIGHT`. The
      shoulder corner therefore advances 0.518 x sin(0.20) = 0.103 m toward the
      eye at full lean, taking the chest-to-eye gap from 0.026 m to 0.129 m —
      five times — while the eye advances by exactly nothing. In a real body a
      forward lean carries the HEAD forward too, which is precisely what keeps
      your chest out of your own view; here the lean spends 100 % of its
      geometry closing a gap that anatomy spends ~0 % on.

      SAME SHAPE AS `PLAYER_EYE_FORWARD` THIS MORNING, and that is the reason
      to trust the diagnosis: neither zone has a bug on its own. The rig leans
      a body that has no eye; the camera holds an eye that has no body; the
      offset between them belongs to nobody (Rule 35). The eye should ride the
      trunk's lean — at full run that is ~0.12 m forward and ~0.01 m down at
      the neck — which is sim's `CameraPose`, so the fix is theirs to place and
      the number is a registry row both read, not a constant either of us keeps.

      NOT FIXED HERE ON PURPOSE. Reducing `RUN_LEAN` below 0.04 rad would make
      the test pass and the run look like a walk, which is the instance rather
      than the mechanism (Rule 32) — and it is the second time today that
      lowering a number would have hidden a missing seam.

      **CLOSED 10:08:2026 - 20:31:38 — and the fix does not reduce the problem,
      it REVERSES it.** sim derived the offset independently (agreeing with
      mine to the millimetre on the eye, and correcting me: my 0.12 m was the
      NECK; the eye is 0.134 m higher and 0.10 m ahead of it, so it swings
      0.1320 m forward and 0.0206 m down at full lean). `eye_lean_offset()`
      lives here, sim's `player_post_step` applies it, and the app ferries —
      NOT a NUMBERS row, because re-deriving it on sim's side would copy
      `gait_run_weight`'s authored table.

      | gear | foot enters | chest enters | margin |
      |---|---|---|---|
      | walk | 41 deg | 45 deg | 4 |
      | jog | 43 | 48 | **5** |
      | run | 45 | **51** | **6** |

      The chest is now HARDEST to see at a run, where it used to be easiest
      (27 deg). The margin grows with speed because the eye and the shoulder
      hang off the same hip pivot at comparable lever arms — 0.746 m against
      0.518 m, with the head's counter-pitch trimming the difference — so they
      advance together by construction rather than by fitting.

      TEST: `character_body` "at every gear, the feet enter the frame before
      the chest does". Inverted in ONE edit with its control, which is the
      whole point of Rule 38's corollary: the fixed eye is now the case that
      must FAIL, and it is re-verified against the NEW bounds rather than
      deleted. Splitting that across two commits is how one half lands alone.

      STILL OPEN, and filed rather than done: a gear change moves the eye
      0.132 m in ONE tick, because `gait_run_weight` is a step function. Body
      and eye pop together, so nothing is exposed — but see a9.

   a9. **THE GEAR CHANGE POPPED, and easing it was NOT a one-zone fix.
      LANDED 10:08:2026 - 20:41:06 — this zone's half; the app's one line follows
      immediately (lead was at the keyboard, so the desync window is seconds
      rather than a session, which is the whole reason it was done now).** Raised by
      sim when their consumer landed. `gait_run_weight` is a step function, so
      walk -> run moves the trunk AND the eye 0.132 m within a single tick —
      about 1.3 ticks' worth of normal running displacement delivered in one,
      i.e. a one-frame doubling of apparent speed.

      THE OBVIOUS FIX IS A TRAP. Easing the eye alone desynchronises it from
      the body, and the two directions are NOT symmetric:

      | transition | body vs eye during the ease | chest-to-eye gap |
      |---|---|---|
      | accelerating | eye leads a body still straightening up | SAFER than steady state |
      | **decelerating** | **body still leaning, eye already back on the axis** | **the defect returns** |

      So an eased camera would reintroduce the chest for the duration of every
      run -> walk, and only that direction — the kind of asymmetry that reads
      as an intermittent glitch nobody can reproduce.

      THE CORRECT SHAPE: ease the WEIGHT, once, where both consumers read it —
      `BodyDrive::run_weight` as internal state advanced in `update_bodies`
      (like `anim_time_s` and `land_dip`), with `evaluate_body_pose` reading it
      and the app ferrying THAT float to `eye_lean_offset` instead of
      recomputing from the gait. Body and eye then share one number and cannot
      drift by construction. sim reviewed the asymmetry and withdrew their own
      "ease it in the producer" suggestion in favour of this shape, so both
      zones agreed the design before anyone wrote it.

      BLEND TIME 0.20 s, sized against the stride rather than picked for feel:
      a walk step lasts step_length(1.8)/1.8 = 0.54 s, so the change settles
      inside the stride the player changed gear in, and the eye's 0.132 m is
      spread to ~0.66 m/s — a tenth of running speed, i.e. under the camera
      motion already on screen. Exponential, matching sim's fov_scale easing.
      MEASURED worst tick after: 0.011 m against 0.132 m before, 12x better.

      AND THE TEST STOPPED BEING FREE. "A gait held past any transition
      renders as that gait" now HOLDS a gear — it spawns a body, steps fixed
      ticks, and arrives; before the ease, evaluate_body_pose was a pure
      function of the gait and the qualifier was vacuously true. Each gear is
      approached from the FURTHEST one so every case crosses a transition, and
      the Rule 38 re-check of the control is now a real re-check rather than
      the same pure function read twice.

      BONUS, and it is why this is worth doing rather than tolerating: it would
      make the steady-state qualifier in the gait test REAL. Today nothing
      settles, so "held past any transition" is vacuously true; with an eased
      weight the test would have to step ticks to reach full weight, which is
      what the phrasing has always claimed to measure.

      **sim's compression of that, and it is the sentence to keep: THEIR
      TRANSITION WORRY AND MY VACUOUS QUALIFIER ARE THE SAME MISSING PIECE.**
      One zone was worried about a blend that does not exist; the other had
      written a test that passes for free because it does not exist. Neither
      of us could see the whole shape from one side — which is the argument
      for a9 being scheduled rather than tolerated, since it closes a real
      motion artifact AND a test that currently costs nothing to satisfy.

   a7. **THE ELBOW — «в анимации махания всё такая же проблема, локоть
      неестественно двигается». THE JOINT LIMITS WERE NEVER THE GAP.** They
      cover the wave path (`evaluate_body_pose` clamps at every exit, verified).
      The gap is what a hinge DOES with an illegal axis: it does not clamp it,
      it DELETES it (`Pose.cpp`: swing-twist about X, keep the twist). The
      wave wagged the forearm with `roll(WAVE_AMP*sin)` — a roll, on an elbow.

      Measured through `evaluate_body_pose` over a full 1.8 Hz wag, the
      clamped forearm quaternion was the CONSTANT (0.989, 0.149, 0, 0) at
      every instant, and the right hand travelled **0.011 m** across the whole
      cycle — all of it the idle breath. The arm was a rigid stick at a 17.2
      deg elbow. Not "insufficiently clamped" and not "clamped on the wrong
      path": the clip asked a hinge for something hinges cannot do, and got
      silence.

      FIX, on the mechanism rather than the clip (Rule 32): a human waves with
      HUMERAL ROTATION — the upper arm turns about its own long axis while the
      elbow holds a bend — so the wag moved to the shoulder (a FREE bone) and
      the elbow took a real 1.40 rad. `flex_pose` had the same defect in its
      forearm rolls and is fixed in the same change. Measured after: hand
      sweep **0.236 m**, elbow 80.2 deg, and clamped == authored everywhere.

      THE TEST IS THE OUTCOME (Rule 38): *"the wave waves"* — hand sweep over
      one cycle > 0.10 m — with the shipped elbow-roll version kept in the
      suite as the control at 0.011 m. Plus the standing guard the defect
      earned: **no shipped clip may lose motion to the hinge reduction** (every
      clip x every hinge bone, off-axis components below 1e-6), which is what
      makes this a fix to the mechanism instead of to one gesture.

      AND A RULE 27 CASUALTY, recorded because it is the same lesson in the
      camera: `character-showcase-wave-da9ff6e.png` claimed the wave "reads as
      a hand gesture". **A still frame cannot show that.** The broken clip
      produced the identical raised arm, so the vantage could not fail. Frame
      deleted; the measurement replaces it.

   b. **THE ARMS ARE HALF BURIED IN THE TORSO — and this is where «форма
      персонажа странная» actually lives (measured 10:08:2026 in the mirror
      stand, `character-mirror-shape-b134936.png`).** Judged from the eye the
      shape complaint has no subject at all, since nothing is in frame; judged
      in the mirror it does: the arms do not break the silhouette, so the body
      reads as a slab with a head. STILL NOT FIXED, and now with the two
      candidates costed, because neither is free: The shoulder joint is at
      +/-`BODY_SHOULDER_WIDTH_FRAC`/2 = 0.233 m — exactly the torso box's own
      half-width — so an arm of `BODY_ARM_THICKNESS_FRAC`·H = 0.099 m hangs
      with half its thickness inside the box. Measured on the standing double
      at 6 m: the arm reads 2 px wide where the mesh is 4 px. At rest the
      forearm and hand also pass THROUGH the pelvis and thigh boxes (visible
      colour interleaving down the hip column). Fix is one number, but it is a
      NUMBERS row, not a literal: the arm hangs outboard of the acromion, so
      either the shoulder joint moves out by half the arm thickness or the
      torso box narrows below the shoulder line.

      | option | silhouette across the shoulders | what breaks |
      |---|---|---|
      | joint out by half the arm | 0.565 x 2 = **0.664 m** | a linebacker; real is 0.46-0.50 |
      | trunk narrows to shoulder_w - 2 x arm | chest **0.268 m** wide | narrower than the 0.344 m pelvis box: a mushroom |
      | joint INBOARD to (shoulder_w - arm)/2, trunk to shoulder_w/2 - arm | **0.466 m**, = biacromial | chest reads 0.149H against a real 0.174H |

      The third is the only one that keeps the total right, and it wants the
      pelvis box revisited in the same breath — which is why it is a costed
      decision for the user and not a quiet edit. NOTE for whoever takes it:
      `BODY_STANCE_WIDTH_FRAC`'s row already anticipates exactly this, calling
      the hip-joint-vs-trochanter conflation an honest simplification that a
      real joint row would supersede. The shoulder has the same shape.

      **CLOSED — AND ALL THREE COSTED OPTIONS WERE WRONG, INCLUDING THE ONE THE
      LEAD RULED FOR.** Lead ruled for the third (revisit the pelvis box), with
      the standing invitation to overrule it by measurement. The measurement
      overruled it, and the instrument is a front-view silhouette scan: every
      mesh vertex of every segment through FK, scanline in Y at 2 mm, X interval
      per bone group and for the union.

      **THE PELVIS BOX IS NOT A LEVER IN EITHER DIRECTION, and this is the
      number.** At the hip line the THIGHS already reach +/-0.2484 m while the
      pelvis box reaches +/-0.1719 m. The box is **entirely interior to the hip
      silhouette — 7.65 cm inside it per side** — so no width you give it changes
      one pixel of the standing figure. This is Rule 41's shape in a place nobody
      was looking: the quantity being argued over cannot express the difference.

      What IS broken at the hip is a different defect with the same cause: the
      delivered hip silhouette is 0.4968 m = **0.276H against a real
      bitrochanteric 0.197H, 40 % over**, because `BODY_HIP_WIDTH_FRAC` 0.191 is
      an OUTER silhouette width used as a JOINT SPAN — its own row says so in as
      many words, so the defect was written down and then depended on for a day.
      NOT FIXED, deliberately: moving the thigh pivots inboard by half a leg
      drops `leg_convergence` 7.37 deg -> 2.39 deg, outside the rig test's own
      5-12 deg control band. It is a NUMBERS row request (femoral head width,
      which `BODY_STANCE_WIDTH_FRAC` already anticipates) PLUS a control revisit,
      and doing half of it would break a green check for the right reason and
      teach the next reader to weaken it (Rule 38).

      **THE LEVER IS THE TRUNK BOX, AND IT IS RULE 43 AT THE SHOULDER.**
      `BODY_SHOULDER_WIDTH_FRAC` is the BIACROMIAL breadth — acromion to
      acromion — which genuinely IS a joint span, so the rig is RIGHT to hang the
      shoulder joints at +/-sx from it. The trunk box then also drew itself to
      sx, so the torso wall reached exactly the arm's own centre line. **A chest
      is not as wide as the acromion span.** There is no chest-breadth row, so a
      box with no row of its own borrowed a number that means something else, and
      the bound written on the biacromial was being judged on the silhouette —
      the same number only if arms have zero thickness.

      Fix: `cx = sx - arm_thickness/2`, one expression in `BodyMesh.cpp`'s Torso
      case. No NUMBERS row, no pelvis revisit, no user trade. Measured:

      | y (m) | trunk half-width B -> A | arm-to-trunk gap B -> A | silhouette B -> A |
      |---|---|---|---|
      | 1.46 (acromion) | 0.2325 -> 0.1832 | **-0.0488** -> +0.0005 | 0.5650 -> **0.5650** |
      | 1.35 | 0.2205 -> 0.1737 | -0.0343 -> +0.0125 | 0.5601 -> 0.5601 |
      | 1.15 (mid-chest) | 0.1987 -> 0.1565 | -0.0081 -> **+0.0341** | 0.5512 -> 0.5512 |
      | 1.05 (waist) | 0.1877 -> 0.1479 | +0.0053 -> +0.0451 | 0.5465 -> 0.5465 |

      **The silhouette is identical to four decimals at every height** — the
      change is entirely internal separation, which is why it costs nothing. The
      arm goes from 4.88 cm buried (49 % of its 9.9 cm) to flush at the acromion
      and 3.4 cm clear at mid-chest. Delivered shoulder breadth stays 0.5652 m =
      0.314H against a real bideltoid ~0.280H (the declared stocky bias); the
      chest lands at 0.3672 m = 0.204H against a real ~0.181H. Option 3 would
      have moved a CORRECT joint to compensate for a WRONG box and paid 0.149H of
      chest for it — trading one wrong silhouette for another, which is exactly
      what it was chosen to avoid.

      The trapezius wedge KEEPS its full acromial base while the trunk below it
      stops at `cx`, and the 4.95 cm step out at the shoulder line is deliberate:
      the arm hangs DOWNWARD from the joint, so it occupies only y < shoulder
      line and never competes with the wedge. A real shoulder does this — the
      widest point of the body is the deltoid, just BELOW the bony corner it
      hangs from.

      FRAMES: `character-mirror-arms-{BEFORE,AFTER,ZOOM5x}-3903d69.png` +
      `character-mirror-arms-restore.txt`, one recipe for both arms. **The zoom
      is part of the evidence, not decoration** — 3.4 cm at 6 m is 5 px on a
      640x360 frame, so a verdict read off the archived pair alone would be a
      vantage that cannot fail (Rule 27). At 5x the before is a slab with a
      1-2 px sleeve sliver at each edge; the after has two distinct arm columns
      with the trunk drawn in behind them.

      **AND THE POINT THE USER IS OWED:** «форма персонажа странная» was never a
      taste question and never needed his ruling. The arm was half buried because
      a box with no row borrowed a number that means something else. The question
      put to him is answered by measurement and withdrawn.
   a10. **THE CROUCH PUTS THE CAMERA 0.36 m BELOW THE BODY'S OWN HEAD — and
      the cause is a comment asserting that two things match when they are
      HALVES OF DIFFERENT QUANTITIES.** Measured 10:08:2026 through FK over
      `apply_crouch` and the real segment meshes, while going after the much
      smaller hunch seam this item was opened for.

      | | standing | full crouch |
      |---|---|---|
      | body's neck joint | 1.5660 | **1.0978** |
      | body's EYE, riding the skull at PLAYER_EYE_HEIGHT/FORWARD in head space | 1.7000 | **1.2102** |
      | sim's camera, `CROUCH_EYE_HEIGHT` 0.85 | 1.7000 | **0.8500** |

      Disagreement **0.3602 m**. The crouched torso spans hip 0.5121 to neck
      1.0978, so a camera at 0.85 sits **0.2478 m below the neck — inside the
      chest.** That is item a1 returning in a pose nobody re-measured after the
      clavicle cut fixed it standing, which is the general lesson: **a fix
      verified in one pose is verified in one pose.** The crouch was authored
      later and inherited none of that morning's measurement.

      MECHANISM. `apply_crouch` drops the pelvis by `0.5 * (thigh + shin)` and
      says in as many words that this "matches `CROUCH_EYE_HEIGHT` being about
      half of `PLAYER_EYE_HEIGHT` without duplicating sim's camera constant."
      Both are honestly "a half". They are halves of different things:

      - half the EYE HEIGHT = **0.8500 m** of drop (sim's)
      - half the LEG = **0.4419 m** of drop (this zone's)

      They differ by **0.4081 m**, and that comment is the only thing that ever
      claimed they agreed. Rule 39's shape applied to a PROPORTIONALITY rather
      than to a chain — the copies were "identical" when written because the
      word *half* was the same, and nothing made them so. A comment is not a
      mechanism; calling one function is.

      NOT FIXED HERE: `CROUCH_EYE_HEIGHT` is sim's row and is marked
      ПРЕДВАРИТЕЛЬНО. A real deep squat drops the eye ~0.45-0.55 m on a 1.8 m
      adult, not 0.85, which puts the eye barely above `BODY_KNEE_HEIGHT_FRAC`·H
      = 0.513 m. What the drawn body offers is 1.7 − 0.4898 = **1.2102**.

      **THE SMALLER SEAM, which is what this item was opened for, and its number
      is bigger than the estimate:** the crouch hunches the trunk
      `pitch(-0.25 x blend)` and the eye does not ride it — exactly
      `eye_lean_offset`'s case, with one difference that changes the answer:
      **the crouch does NOT counter-pitch the head, where the run does**
      (`HEAD_STABILIZE`), so the eye swings by the FULL theta about the neck.

      | crouch blend | eye forward | eye down (hunch only, excluding the pelvis drop) |
      |---|---|---|
      | 0.25 | 0.0464 | 0.0119 |
      | 0.50 | 0.0922 | 0.0242 |
      | 1.00 | **0.1815** | **0.0479** |

      0.1815 m is LARGER than the run lean's 0.1320 despite a SMALLER trunk
      angle (0.25 rad against 0.20), purely because nothing stabilises the head.
      The working estimate was ~0.11 m; it was low, and the head is why.

      NO PRODUCER LANDED, DELIBERATELY. The geometry generalises cleanly —
      `eye_lean_offset`'s body depends only on theta and the stabilisation
      fraction — but landing a pure function with no consumer would be a
      function with zero readers, which is the exact defect this session removed
      from this zone (`BODY_THIGH_SWING_MAX_SIN`, a row nothing read). It lands
      the same hour as sim's consumer, the way a8 did, or not at all.

      THIRD FINDING, free with the measurement and this zone's own: the drawn
      head pitches −0.25 rad at full crouch, so a crouched character looks 14.3
      deg at the floor. Invisible in first person, glaring on the mirror double
      and on any NPC. It is also what makes the offset 0.1815 rather than ~0.13
      — so the smaller number is bought by stabilising the head, not by shrinking
      the offset.

   c. **THE HEAD IS A BOX ON A BOX.** No neck segment exists between
      `BODY_NECK_HEIGHT_FRAC` and the head, so the head sits straight on the
      shoulders. Harmless in first person (hidden), mannequin-ish on NPCs.
      Deferred until faces, which is the same stage that will want a neck.

   View-model retirement sync with sim once arms can present a held item.
4. Later stages: foot IK on slopes/stairs (recorded, в24), NPC archetype
   bodies + faces + equipment on this same rig, skinning behind IAnim (ozz
   backend), LocomotionSignals component co-proposed with sim when NPCs walk.

## How it is verified

`tests/character.cmake`: `character_rig_pose` (FK heights from config rows,
mirror involution + the it-actually-moves-sides check, asymmetric-pose
control), `character_clips` (THE FOOTFALL CONTRACT: touch-down edge equals
the FOOTFALL_PHASE rows with a 0.07-shifted clip as the failing control —
the rejected instance is audio/visual desync; amplitude-follows-step with
cap; crouch two-link geometry with a no-fold control), `character_body`
(segment spawn/hide-head, snapshot discipline, mirror puppet reflection with
the no-swap lag-double control, showcase cycling). Rule 27: the mirror map
IS the standing visual instrument; acceptance frames per plan step 2.
FUTURE integration test (either zone may build it): sim's fired
FootfallEvent ticks vs this zone's visual plants in a RUNNING sim — its
tolerance is stride period +/- 1.5 ticks (sim's phase-crossing quantization
bound, agreed 10:08:2026); the current clip tests are analytic against the
rows and deliberately tighter (0.02 cycle).

**The camera instrument (Rule 27), and the bearing that cannot fail.** The
screenshot Tour freezes the simulation, so it can photograph still life only —
no tick means no `update_bodies`, and every animated subject in the project is
invisible to it by construction. `DFN_BODY_PROBE` (App.cpp, gated) is the
opposite instrument: the world runs and the shot is triggered off simulation
state — a named stride phase, a clip time, a yaw — with the achieved value
logged beside the frame. Modes: `stride` (look down at your own feet while the
playtest bot walks), `gait` (the live mirror double, strafing), `profile` /
`plant` (the showcase double from its side), `mirror`, `showcase`.

The bearing matters and it nearly cost a false bug report. The FRONT view of a
walker cannot show a fore-aft leg scissor: it projects to almost nothing, and
the frontal `gait` frames read as if the legs were together at the plant
phases — the opposite of what the clip does. The side profile at the same four
phases shows the scissor exactly on `FOOTFALL_PHASE_LEFT` / `_RIGHT` and the
legs passing at 0.5 / 1.0. Recorded as a standing warning: the mirror double
can NEVER supply a profile of a walk, because it reflects the camera's own
facing and so turns to face you whichever way you turn. Only the showcase
double, whose facing is independent, can be walked around.

**The v1 slide, measured.** `THIGH_SWING_MAX_SIN` caps the half-step reach at
0.55·(hip−ankle) = 0.486 m, so the visual stride is 0.97 m against sim's
`step_length(3.0)` = 1.40 m at a walk — the foot must slip 0.43 m per step
(31 %), and at `RUN_SPEED` 0.97 m against 2.45 m (60 %). NOTE WHAT THE FRAMES
DO NOT SAY: a still cannot show slip, which is a motion artifact. The frames
show only that the stance at the plant phases is SHORT for the step being
taken. Judging the slide needs a ground-relative sequence, not a screenshot,
and that instrument does not exist yet.

## What this zone does NOT do

- No stride clock of its own (sim's, Rule 35 state form). No camera motion —
  head-bob/landing camera curves are sim's (в3); this zone moves the BODY.
- No collision for body segments; no NPC control paths (Rule 15 stays sim's).
- No mesh registration into the renderer (render's registry, app's ferry).
- No terrain foot IK yet; no skinning yet; no content-file animation loading
  yet (procedural clips are code, real assets arrive via IAnim later).
