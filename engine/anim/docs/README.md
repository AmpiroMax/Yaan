
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
- `Posture.h` — `Posture` (None/Sit/Lie), `sit_pose(rig, seat_above_ground)`,
  `lie_pose(rig, deck_above_ground)`, `posture_pose(rig, поза, высота, доли)` и
  `posture_transit()`/`posture_transit_s()` (переход движением, см. ниже),
  `posture_eye()`, `lie_yaw_for_head_dir()`.
  ОДИН ВХОД — ВЫСОТА ПЛОЩАДКИ: 0.45 у лавки, 0.50 у кровати, и оба числа
  ЗАМЕРЕНЫ с геометрии предмета приложением, а не выбраны здесь. Сидя бедро
  выводится ДВУЗВЕННИКОМ по лодыжке (стопа встаёт на пол; недосягаемый пол
  честно даёт висящие ноги); лёжа всё тело кладёт ОДИН поворот таза на +90°
  вокруг X — руки и стопы ложатся правильно сами, потому что висят по -Y.
  Следствие, о котором надо помнить: у лежащего голова уходит в местное +Z
  (назад), отсюда `lie_yaw_for_head_dir`.
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
- `PoseLayers.h` — `Branch`/`BranchMask`/`build_branch_mask()` (верх/низ по
  ВЕТВЯМ скелета, а не по списку имён), `blend_masked()`, `ArmRelax` +
  `calibrate_arm_relax()`/`apply_arm_relax()` (приведение плеч и расслабление
  пальцев; УГОЛ РЕШАЕТСЯ по нашей позе покоя — 14.5° на HumanBase, а не
  назначенные «10-12°»), `measure_hand_spread()`/`measure_hand_openness()` —
  числа, в которых написана приёмка пункта 3.
- `FootIk.h` — `FootIkSetup`/`build_foot_ik()`, `FootIkProbe` (что ответил мир,
  в СОБСТВЕННОЙ системе тела), `FootIkPlan`/`plan_foot_ik()` (замер),
  `apply_foot_ik()` (сдвиг корня к нижней стопе + двузвенник колена + тангаж
  стопы), `foot_penetration()` (прибор приёмки). Луча здесь НЕТ: зона не видит
  мира (правило 1), высоты грунта приходят аргументом от приложения.
- `Hitbox.h` — `BodyPart` (КАНАЛ, а не подпись: 16 частей + None),
  `HitboxSlot`/`HitboxSet`/`build_hitboxes(proportions)` (форма задаётся ПАРОЙ
  СУСТАВОВ и долей отрезка между ними — единственная запись, переживающая
  чужой скелет), `hitbox_pose()`, `hitbox_raycast()`, `hitbox_contains()`.
- `BodyMesh.h` — `build_body_segment_mesh(bone, proportions)` (flat-shaded
  boxes, bone-space, `platform::Vertex`), `body_segment_mesh_id()` (= 34 +
  bone index; range 34..49 blessed in render's ProcMesh id map).
- `Body.h` — components `BodyRig`, `BodyDrive` (app-ferried copy of sim's
  stride phase/speed/GAIT/`weapon_drawn`/etc — never a second clock and never a
  re-derived gear), `MirrorPuppet`; systems
  `spawn_body`, `spawn_mirror_puppet`, `note_landed`, `evaluate_body_pose`,
  `update_bodies` (fixed tick, after `player_post_step`), `body_root_for`.

  **ПОЗА МЕБЕЛИ — ЗАЯВКА И ЕЁ ИНТЕГРАТОР, А НЕ ОДНО ПОЛЕ.** `BodyDrive::posture`
  говорит, чего хотят; `posture_blend` — время перехода в долях (0..1), и он
  доезжает до заявки за `posture_transit_s(позы)` (0.60 с сесть, 0.90 с лечь —
  лечь дольше на целое откидывание). ЛИНЕЙНО, и это по-прежнему намеренно:
  экспонента не доходит до нуля, и признак `eye_valid` остался бы истинным
  навсегда после первого вставания. `posture_shown` — что РИСУЕТСЯ, пока
  блендер возвращается: заявка гаснет первой, и без этого поля вставание с
  кровати шло через сидячую позу. `eye_point` — ВЫХОД: глаз нарисованной позы,
  публикуемый из той же позы и того же корня, которыми поставлены сегменты (у
  камеры не бывает второго описания того, где голова — на присяде зона уже
  платила за это 0.36 м).

  **ПЕРЕХОД — ДВИЖЕНИЕ, А НЕ КРОССФЕЙД** (`Posture.h`: `PostureTransit`,
  `posture_transit`, `posture_pose`). Ведёт его ТРАЕКТОРИЯ ТАЗА, а колени и
  бёдра ВЕДОМЫЕ: двузвенник решается заново на каждой высоте таза, поэтому
  стопа стоит на полу на всём спуске, а не только на концах. Пять долей одного
  времени, и их сдвиг и есть форма движения: `take` (живой слой отдаёт тело в
  первой трети), `drop` (таз вниз), `plan` (корень к мебели — ОПЕРЕЖАЕТ спуск у
  сиденья, оттого путь дуга, а не отрезок наискось), `settle` (корпус и руки),
  `recline` (откидывание на спину, только лёжа). Новых клипов нет и не нужно:
  обе позы — функции высоты, и промежуточный кадр это та же функция,
  спрошенная о промежуточной высоте. Замер: `artifacts/acceptance/seat-poses.md`
  (таз и глаз монотонны, рывок 0.005–0.007 хода, пик 1.2 м/с сесть).

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
