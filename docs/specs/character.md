<!--
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 01:56:45
-->
<!--
UPD:
- 10:08:2026 - 01:56:45: Initial spec: rig contract, rigid-segmented body,
  stride seam with sim, mirror map plan.
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

- sim: the stride clock (phase, step length) lives in their PlayerState; the
  app ferries it into `BodyDrive` (agreed 10:08:2026: LEFT plants at
  `FOOTFALL_PHASE_LEFT`, phase from post-step displacement, holds on stop,
  suspends airborne, `Landed{impact_speed}` for the both-feet dip). The
  view-model hand (id 32) retires later, coordinated — nothing new may
  assume it permanent.
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
3. First-person polish from frames: arm visibility at rest, torso at
   look-down, land dip feel. View-model retirement sync with sim once arms
   can present a held item.
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

## What this zone does NOT do

- No stride clock of its own (sim's, Rule 35 state form). No camera motion —
  head-bob/landing camera curves are sim's (в3); this zone moves the BODY.
- No collision for body segments; no NPC control paths (Rule 15 stays sim's).
- No mesh registration into the renderer (render's registry, app's ferry).
- No terrain foot IK yet; no skinning yet; no content-file animation loading
  yet (procedural clips are code, real assets arrive via IAnim later).
