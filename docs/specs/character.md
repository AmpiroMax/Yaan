<!--
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 11:05:00
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
      Its forward edge therefore sits 43 deg below the horizon: past ~13 deg of
      downward look the chest enters frame, and by ~73 deg it is the whole
      lower field. You can never see your own legs or feet — at −66 deg the
      frame is 100 % torso. This is not a tuning value, it is a missing seam:
      a real eye is FORWARD of the chest, and there is no row for that offset.
      Fix belongs with sim (they own CameraPose): an eye offset of about half
      the torso depth plus margin (~0.18 m along facing), or the camera hung
      off the Head bone. Frames: `character-lookdown-{66,40}deg-*`.
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
