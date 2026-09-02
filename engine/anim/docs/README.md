
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
  Плюс ОБХОД ТЕЛА РУКОЙ (фидбек 31.08-2, пункт 1): `ArmClearance` +
  `build_arm_clearance()`/`apply_arm_clearance()`/`measure_arm_body_gap()`.
  ОТДЕЛЬНЫЙ слой, а не поправка к приведению: приведение решается ОДИН РАЗ на
  позе покоя, а проход сквозь таз случается в ФАЗЕ МАХА, и уменьшать приведение
  «на всякий случай» — значит развести руки на всём цикле ради двух его кадров.
  Мерится до ФОРМ таза и бёдер (`Hitbox.h`), а не до кости: вопрос об ОБЪЁМЕ, и
  объём тела в проекте описан ровно один раз (правило 35). Цель —
  `ARM_BODY_CLEARANCE`, доза живёт в `ClipLibrary::arm_clearance_m` (0 —
  побитовое тождество, на нём стоит контрольная рука приёмки).
- `FootIk.h` — `FootIkSetup`/`build_foot_ik()`, `FootIkProbe` (что ответил мир,
  в СОБСТВЕННОЙ системе тела), `FootIkPlan`/`plan_foot_ik()` (замер),
  `apply_foot_ik()` (сдвиг корня к нижней стопе + двузвенник колена + тангаж
  стопы), `foot_gap()` (ЗНАКОВЫЙ зазор по каждой стопе: + парит, − утонула) и
  `foot_penetration()`, ВЫРАЖЕННАЯ через него. Порядок именно такой, и это
  находка волны «тело и камера»: `foot_penetration` берёт `max(0, …)`, то есть
  срезает ровно ПОЛОЖИТЕЛЬНУЮ половину — а жалоба «одна стопа парит» живёт
  целиком в ней, и прибор читал проверяемое им состояние нулём ПО ПОСТРОЕНИЮ
  (правило 47 на собственном приборе зоны). Луча здесь НЕТ: зона не видит
  мира (правило 1), высоты грунта приходят аргументом от приложения.
- `Hitbox.h` — `BodyPart` (КАНАЛ, а не подпись: 16 частей + None),
  `HitboxSlot`/`HitboxSet`/`build_hitboxes(proportions)` (форма задаётся ПАРОЙ
  СУСТАВОВ и долей отрезка между ними — единственная запись, переживающая
  чужой скелет), `hitbox_pose()`, `hitbox_raycast()`, `hitbox_contains()`,
  `hitbox_distance()` (расстояние до поверхности одной части, 0 внутри — им
  написан слой обхода тела рукой: «попал / не попал» отвечает уже ПОСЛЕ того,
  как дефект стал виден).
- `BodyGaps.h` — ПРИБОР ЗАЗОРОВ ОДНОГО ТЕЛА (заказ владельца 02.09: «ноги
  слиплись, руки у ягодиц» НА ЭКРАНЕ СОЗДАНИЯ при нуле пересечений на
  стенде). `label_skin_parts()` (вершина → кость рига), `measure_body_gaps()`
  — нога↔нога, кисть↔бедро, предплечье↔корпус по МЕШУ (боковой ЗНАКОВЫЙ зазор
  по полосам высоты: отрицательное число — глубина взаимопроникновения; плюс
  минимум по парам вершин, который отрицательным быть не умеет и именно
  поэтому «слиплись» пропускал) и по КОРОБКАМ (`hitbox_pair_distance`).
  Пороги — строки `REST_GAP_*` через `BodyGapTargets::from_config()`, вердикт
  `gaps_meet()`, строка журнала `describe_gaps()`. Один прибор на экран
  создания, смотровую, стенд и решатель рест-позы (правило 47: обе руки одним
  прибором). Замер «до» на экране создания: нога-нога −8.99 см, кисть-бедро
  −2.92, предплечье-корпус −1.25, все три пары коробок в пересечении.
- `Mirror.h` — `MirrorMap`/`build_mirror_map()` (зеркальные пары суставов,
  найденные ГЕОМЕТРИЕЙ бинда и проверенные РОДИТЕЛЕМ: совпасть должны не две
  точки, а две цепи), `mirror_pose()`, `mirror_blend()` (поза, смешанная с
  зеркалом позы полуциклом позже — цикл ходьбы АНТИСИММЕТРИЧЕН, и купленный
  клип этого не обещает), `mirror_asymmetry()` (прибор). Смесь в МОДЕЛЬНОМ
  пространстве, а не в локальном: локальные системы левого и правого родителей
  на купленном скелете не отражают друг друга точно ни на одном. Доза —
  `ClipLibrary::mirror_dose` (0.5 — точная антисимметрия, 0 — побитовое
  тождество). Замер на HumanBase: зеркальная разница цикла 13.50 см → 0.0007 мм.
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
