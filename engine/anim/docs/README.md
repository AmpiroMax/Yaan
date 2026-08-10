<!--
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 20:06:45
-->
<!--
UPD:
- 10:08:2026 - 01:56:45: Initial module doc: rig, pose math, procedural clips,
  rigid-segmented body + mirror puppet.
- 10:08:2026 - 20:06:45: `anim::Gait` + `gait_run_weight()` (the gear is
  ferried, never re-derived from speed); `BodyDrive::gait`; the wave's wag
  moved off the elbow onto humeral rotation, and the standing rule that
  came out of it.
-->

# engine/anim — humanoid rig, procedural animation, rigid-segmented body

Zone: `character` (Rule 25, carved from sim 10:08:2026).

## Responsibility

The humanoid skeleton contract (see [RIG.md](RIG.md) — bones frozen, future
NPC rig), FK pose math with mirroring, procedural locomotion/showcase clips
driven by SIM'S stride clock, and the ECS body: 15 rigid segment entities per
character drawn by render's ordinary interpolated pass. First-person = the
same body with the head mesh hidden. No skinning yet; `IAnim`/ozz is the later
upgrade behind the same bone indices.

## Key types

- `Rig.h` — `Bone` (15, frozen order), `BONE_PARENT`, `MIRROR_BONE`,
  `RigProportions` (meters; `from_config()` = BODY_*_FRAC rows x
  `PLAYER_CAPSULE_HEIGHT`), `Rig::build()` (rest offsets).
- `Pose.h` — `LocalPose` (quat per bone relative to rest + pelvis offset),
  `BodyRoot`, `forward_kinematics()`, `mirror_pose()` (involution),
  `mirror_point()/mirror_yaw()`, `blend()`.
- `Clips.h` — `idle_pose`, `gait_pose(phase, step_length, run_weight)` (one
  evaluator for walk+run; compass-gait pelvis arc keeps the stance foot
  grounded and touching down exactly on `FOOTFALL_PHASE_LEFT/RIGHT`),
  `apply_crouch`, `air_pose`, `apply_land_dip`, `wave_pose`, `flex_pose`,
  `ShowcaseClip`, `Gait` + `gait_run_weight(Gait)`.

  **CLIPS AUTHOR ONLY MOTION THE JOINT CAN PERFORM.** `apply_joint_limits`
  reduces a hinge (both knees, both elbows) to its own X axis, and an off-axis
  component handed to one is NOT clamped — it is DELETED. `wave_pose` wagged
  the forearm with a roll and therefore did not move at all: the clamped
  quaternion was constant across the whole cycle and the hand travelled 11 mm.
  Anything sideways belongs on a FREE bone — the shoulder's own long axis is
  what a real wave and a real elbow-splay use. `character_clips` now enforces
  this for every clip x every hinge bone, so the next author gets a red test
  instead of a silent nothing.

  **`gait_run_weight` IS A TABLE, NOT A MAP** (Rule 37). It used to be
  `(speed - WALK_SPEED) / (RUN_SPEED - WALK_SPEED)`, which was correct until
  `JOG_SPEED` landed between those two rows and jog silently began rendering
  as a walk leaning 0.286 toward run. Adding a gear here has to be a decision
  somebody writes down; a linear map acquires interior points by itself.
- `BodyMesh.h` — `build_body_segment_mesh(bone, proportions)` (flat-shaded
  boxes, bone-space, `platform::Vertex`), `body_segment_mesh_id()` (= 34 +
  bone index; range 34..49 blessed in render's ProcMesh id map).
- `Body.h` — components `BodyRig`, `BodyDrive` (app-ferried copy of sim's
  stride phase/speed/GAIT/etc — never a second clock and never a re-derived
  gear), `MirrorPuppet`; systems
  `spawn_body`, `spawn_mirror_puppet`, `note_landed`, `evaluate_body_pose`,
  `update_bodies` (fixed tick, after `player_post_step`).

## Usage example

```cpp
const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
anim::spawn_body(world, player, rig, /*hide_head=*/true);
// each fixed tick, after player_post_step (app ferries sim's PlayerState):
auto* drive = world.get<anim::BodyDrive>(player);
drive->stride_phase = ps.stride_phase;  // sim's clock — the only source
drive->step_length_m = ps.step_length;  // sim's length(v)
drive->speed_mps = speed; drive->facing_yaw = ps.yaw;
switch (ps.gait) {                      // the GEAR, not the speed it came from
case gameplay::Gait::Walk: drive->gait = anim::Gait::Walk; break;
case gameplay::Gait::Jog:  drive->gait = anim::Gait::Jog;  break;
case gameplay::Gait::Run:  drive->gait = anim::Gait::Run;  break;
}
drive->grounded = grounded; drive->crouch_blend = ps.crouch_blend;
anim::update_bodies(world, rig);        // FK -> segment Transform pairs
```

## Dependencies

Uses: core (ecs, components, config), platform render INTERFACE (Vertex
only), glm. Deliberately below gameplay in the DAG — it never reads
PlayerState; the app copies. Used by: engine/app (wiring), tests/character.

Watchpoint (Rule 32): `BodyMesh.cpp` re-states tiny quad/box helpers because
anim cannot include engine/render's ProcMesh (sibling layers). If a third
zone ever needs them, they move to core/math instead of a third copy.
