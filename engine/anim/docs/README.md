<!--
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 01:56:45
-->
<!--
UPD:
- 10:08:2026 - 01:56:45: Initial module doc: rig, pose math, procedural clips,
  rigid-segmented body + mirror puppet.
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
  `ShowcaseClip`.
- `BodyMesh.h` — `build_body_segment_mesh(bone, proportions)` (flat-shaded
  boxes, bone-space, `platform::Vertex`), `body_segment_mesh_id()` (= 34 +
  bone index; range 34..49 blessed in render's ProcMesh id map).
- `Body.h` — components `BodyRig`, `BodyDrive` (app-ferried copy of sim's
  stride phase/speed/etc — never a second clock), `MirrorPuppet`; systems
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
