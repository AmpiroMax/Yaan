<!--
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 12:30:00
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

      ii. **`BODY_THIGH_SWING_MAX_SIN` is requested but not landed.** 0.55 lives
         as a file-local constexpr in Clips.cpp while it now has to AGREE with
         sim's step-length rows — it caps the visual half-step at 0.55 x leg
         length = 0.486 m, and sim's binding check wants to be arithmetic over
         the generated header with no cross-zone include. The check must be
         written as "residual slip stays under a perceptual bound", never as
         "the clamp is inactive": at WALK 1.8 the residual is 0.8 %, and a
         check that goes red the day it is written teaches everyone to ignore
         it. (Agreed with sim independently on both halves.)

   b. **THE ARMS ARE HALF BURIED IN THE TORSO.** The shoulder joint is at
      +/-`BODY_SHOULDER_WIDTH_FRAC`/2 = 0.233 m — exactly the torso box's own
      half-width — so an arm of `BODY_ARM_THICKNESS_FRAC`·H = 0.099 m hangs
      with half its thickness inside the box. Measured on the standing double
      at 6 m: the arm reads 2 px wide where the mesh is 4 px. At rest the
      forearm and hand also pass THROUGH the pelvis and thigh boxes (visible
      colour interleaving down the hip column). Fix is one number, but it is a
      NUMBERS row, not a literal: the arm hangs outboard of the acromion, so
      either the shoulder joint moves out by half the arm thickness or the
      torso box narrows below the shoulder line.
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
