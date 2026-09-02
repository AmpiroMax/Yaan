
# HUMANOID RIG — the contract (character zone)

This hierarchy is the FUTURE NPC CONTRACT (grill в16: the same rig later drives the
six NPC archetypes with visible equipment). It is frozen in the Rule 26 sense the
day another zone consumes it: adding bones at the END of the enum is a compatible
extension; renaming, reparenting or reordering existing bones requires a group sync.

## Bones (index, name, parent, proximal joint, rest direction, mesh id)

| # | Bone | Parent | Proximal joint at | Rest direction | RenderMesh id |
|---|---|---|---|---|---|
| 0 | Pelvis | — (root) | hip center, `BODY_HIP_HEIGHT_FRAC`·H | (box around joint) | 34 |
| 1 | Torso | Pelvis | hip center (same point) | +Y up to neck | 35 |
| 2 | Head | Torso | neck, `BODY_NECK_HEIGHT_FRAC`·H | +Y up | 36 |
| 3 | UpperArmL | Torso | left shoulder | −Y down | 37 |
| 4 | ForearmL | UpperArmL | left elbow | −Y down | 38 |
| 5 | HandL | ForearmL | left wrist | −Y down | 39 |
| 6 | UpperArmR | Torso | right shoulder | −Y down | 40 |
| 7 | ForearmR | UpperArmR | right elbow | −Y down | 41 |
| 8 | HandR | ForearmR | right wrist | −Y down | 42 |
| 9 | ThighL | Pelvis | left hip | −Y down | 43 |
| 10 | ShinL | ThighL | left knee | −Y down | 44 |
| 11 | FootL | ShinL | left ankle | −Z forward | 45 |
| 12 | ThighR | Pelvis | right hip | −Y down | 46 |
| 13 | ShinR | ThighR | right knee | −Y down | 47 |
| 14 | FootR | ShinR | right ankle | −Z forward | 48 |

H = `PLAYER_CAPSULE_HEIGHT` (deliberately: the body FITS THE CAPSULE sim owns —
one height, two consumers, Rule 35; there is no separate BODY_HEIGHT). All segment
lengths are the `BODY_*_FRAC` rows in NUMBERS.md (Drillis & Contini 1966 fractions
as reproduced in Winter, *Biomechanics and Motor Control of Human Movement*,
tbl 4.1; thickness/width rows carry their own derivations in NUMBERS.md).

Mesh ids 34..48 = 34 + bone index, spare 49 (range requested from render, whose
ProcMesh.h owns the id map; do not register before their ack).

## Spaces and conventions

- Character local space: origin at the GROUND POINT under the pelvis (the owner
  entity's `Transform.position`, i.e. the capsule bottom), +Y up, facing −Z at
  yaw 0. World yaw follows sim's convention: forward = (sin yaw, 0, −cos yaw);
  at yaw 0, +X is the character's RIGHT.
- A bone's frame: origin at its proximal joint; `LocalPose` stores one quaternion
  per bone RELATIVE TO THE REST POSE (identity everywhere = rest = standing).
  FK: `model[b] = model[parent[b]] * T(rest_offset[b]) * R(q[b])`; the pelvis
  additionally takes `LocalPose.pelvis_offset` (bob/sway) before its rotation.
- Segment meshes are authored in bone space (origin at the proximal joint,
  extending along the rest direction), so a segment entity's `Transform` IS the
  FK bone transform — no per-segment fixup anywhere.

## The stride-phase seam (sim's clock — agreed, pinned 10:08:2026)

**ШОВ ЗАМЕНЁН (решение группового синка 02.09: координатор daggerfall-n-63, anim,
sim; правило 26).** Владелец потребовал, чтобы ноги твёрдо стояли на земле, а
снос стопы при порядке «капсула → фаза → клип → стопа» структурный: три
источника скорости стопы сходятся только в среднем за цикл. Действующий порядок
— «клип → опорная стопа → корень → капсула» (docs/design/LOCOMOTION_GROUNDED.md):

- ЧАСЫ ЛОКОМОЦИИ ЖИВУТ В anim: `ClipPlayback::phase` растёт на dt·rate/длительность
  клипа роли, темп `rate` = заказ передачи / скорость клипа на этом теле в полосе
  `LOCOMOTION_TEMPO_BAND`; масштаба размаха ног нет (клип играет, как поставлен).
  Роль выбирается по НАМЕРЕНИЮ (`BodyDrive::want_speed_mps`), а не по фактической
  скорости капсулы — та сама следствие клипа.
- КОРНЕВОЕ СМЕЩЕНИЕ ЗА ТИК ВЫВОДИТСЯ ИЗ ОПОРНОЙ СТОПЫ финальной позы
  (`RootMotion.h`: самая низкая стопа ведёт, вторая — в полосе `FOOT_SUPPORT_BAND_M`,
  стопа, идущая вперёд или отстающая от скорости тела, — мах; в полёте — среднее
  последней опоры). `SkinnedCharacter::advance()` зовётся ДО шага сима и
  публикует `LocomotionOut` (смещение в системе тела, фаза, постановки);
  приложение переводит его в мир и кладёт в `StepContext::locomotion`; sim
  (`player_pre_step`) проводит смещение через физику (стена, склон, ступень);
  после шага `commit_root()` принимает факт.
- FOOT LOCK (`FootIk.h`: `FootLockState`, `apply_foot_lock`) держит подушечку или
  пятку опорной стопы в мировой точке касания (гистерезис `FOOT_LOCK_ON/OFF_WEIGHT`,
  отпускание за `FOOT_LOCK_RELEASE_S`, перецепка с пятки на носок при перекате),
  двузвенник колена — тот же, что у подъёма на грунт.
- `FootfallEvent` и боб камеры идут от фазы и постановок ЗАЯВКИ: одни часы,
  остальные читают. `stride_phase` в `PlayerState` — копия фазы anim, пока заявка
  есть; без неё (воздух, плавание, прежний шов) sim крутит фазу от смещения, как
  ниже.
- Контрольные руки из того же бинарника: `DFN_ROOT_FROM_FEET=0` — прежний шов
  целиком (фаза сима, стрид-скейл), `DFN_FOOT_LOCK=0` — без замка,
  `DFN_IDLE_SYMMETRY=0` — покой как в клипе. Приборы: `character_clips_slide`
  (зона anim, снос ≤ 0.004 мм), `app_grounded_locomotion` (путь игрока, кадр из
  палитры: снос ≤ `FOOT_SLIDE_MAX_M`; замер 02.09 — ходьба 1.2 мм, трусца 1.5,
  бег 1.8 против 330 мм у прежнего шва).

Абзац ниже описывает ПРЕЖНИЙ шов — он остаётся контрольной рукой и путём для
тела без клипов (коробочное тело, `DFN_PROC_GAIT`).


The stride cycle lives ONCE, in sim's `PlayerState` (Rule 35, state form). This
zone consumes it as a plain parameter ferried by the app; it never derives phase
from speed itself. Agreed semantics (pinned with sim 10:08:2026, lead-landed):

- phase ∈ [0,1) per full L+R cycle; advances from ACTUAL post-step horizontal
  displacement; step length = `STEP_LENGTH_BASE + STEP_LENGTH_PER_MPS`·v.
- LEFT foot plants at `FOOTFALL_PHASE_LEFT` (0.25), RIGHT at
  `FOOTFALL_PHASE_RIGHT` (0.75) — NUMBERS rows, two consumers by construction:
  sim fires `FootfallEvent` there, walk/run clips here MUST put the visual foot
  plant and the pelvis-bob minima at exactly these phases — tests compare both
  against the same generated names, with an offset-clip control (Rule 30).
- On stop the phase HOLDS; airborne SUSPENDS it; landing fires sim's
  `Landed{impact_speed}` (both feet — keyed separately from footfalls).

## Mirroring (the mirror map, grill в11)

Local mirror across the character's sagittal plane (local x = 0):
translation (x,y,z) → (−x,y,z); quaternion (w,x,y,z) → (w,x,−y,−z); then swap
each L bone with its R counterpart. This is an involution: mirror ∘ mirror = id
(tested, with an asymmetric-pose control — a symmetric pose passes trivially).
World mirror for the puppet reflects the root position and facing across the
mirror plane (point + horizontal normal) and applies the local mirror.

## What this rig is NOT yet

- Not skinned: v1 is rigid segments, one mesh per bone through the ordinary
  Transform+RenderMesh path. Skinning (IAnim/ozz, skinning matrices) is a later
  upgrade BEHIND this same hierarchy — bone indices are the stable contract.
- No foot IK: v1 foot placement is timing-only (plant on the phase). IK on
  slopes/stairs is a recorded later item (grill в24 notes stairs explicitly).
- No fingers, no face bones. Faces are a later character-zone stage and will
  extend the enum at the end, not reshape it.

## The rest pose «по швам» (owner's order 02.09)

**The rest is a STANCE, not only a skeleton, and it is solved on the skin.**
`RestStance` (Rig.h) names how the limbs hang at rest — leg splay off the
plumb, arm abduction, elbow flexion, forward carry — and `Rig::build(p,
stance)` turns it into the rest rotations. Two values of it exist:

- `RestStance::converged(p)` — the BOX BODY's rest: legs converged to the
  stance row (its hip joints sit on the skin, 0.334 m apart), arms dead
  vertical. `Rig::build(p)` is this rest and is bit-for-bit what it was.
- `RestStance::attention()` + `fit_rest_pose()` (RestFit.h) — the rest of a
  BOUND MODEL: legs vertical under the model's own hip joints, feet flat and
  forward, elbow at `REST_ELBOW_FLEX` (10°), arms along the sides at the
  abduction that clears the thighs. The abduction and the splay start at zero
  and are raised by a lever solve (asin of the deficit over the limb) until
  the body-gap instrument (BodyGaps.h) reads the `REST_GAP_*` rows on the
  SKIN — nога↔нога ≥ 2 cm, кисть↔бедро ≥ 1.5 cm, предплечье↔корпус ≥ 2 cm,
  signed, by height bands — then tightened by bisection to the smallest
  clearing angle. On HumanBase: splay 0°, abduction 6.8°, elbow 9.7°, stance
  0.170 m (= hip joints), hand-thigh 1.63 cm, legs 2.66 cm, hands' thinnest
  extent along X (palms to the thighs), toes 0.0° off forward.

**Why solved and not authored.** The converged box rest, applied through the
retarget to a model whose hip joints are 0.17 m apart, crossed the ankles
inside the body's axis: measured on the character screen, the legs 8.99 cm
into each other, the hand 2.92 cm inside the thigh, the forearm 1.25 cm
inside the trunk — while the stand, which plays the idle clip with the stance
layer and the arm clearance on top, reported zero intersections. A rest that
is right for one skeleton is a number; a rest right for THIS skeleton is a
measurement, and it has to be taken on the skin, because a joint has no
radius.

**One rest pose per body.** `anim::rest_rig_for(skeleton, skin)` is the call
every reader of a model makes — the importer's grounding, the proportion
judge's silhouette (and the baseline it records), the morph tool's rest space
(so `.morf` deltas are baked against this rest and re-baked when it moves),
the character's retarget, the character screen's portrait, the tests. The box
rig stays the app's `body_rig_` for the boxes; a `SkinnedCharacter` carries
its own fitted rig.

**The rest is the STANCE LAYER's zero.** `build_stance_layer(rig, …)` reads
the rest's ankle separation, elbow and hand drop through the retarget, and the
standing half of the layer (and `calibrate_arm_relax`) aims at THOSE — the
idle the world plays stands as wide, with the same elbow and the same hand
height, as the rest the screen shows. The former rows `STANCE_ELBOW_STAND`,
`STANCE_HAND_DROP`, `STANCE_WIDTH_SHOULDERS` were a second definition of the
standing man and are retired; the run targets (`STANCE_*_RUN`) stay rows.

**The FK root lifts by `Rig::rest_hip_height()`** — ankle + leg·cos(splay) —
so the soles stay on the ground in either rest (7 mm lower in the converged
one, exactly `hip_height` in the vertical one).

**`DFN_REST_POSE=legacy`** builds the player's body and the screen's body in
the converged rest, unfitted — the "before" arm of the comparison, from the
same binary (Rule 47). Its passport on the MPFB body (02.09): legs −7.4°, arms
0° — the legs 5.35 cm INTO each other, the hand 2.36 cm into the thigh, the
thigh boxes intersecting; the solved rest on the same body is legs +4.6°, arms
8.1°, gaps 2.00 / 3.10 / 5.35 cm (legs / hand-thigh / forearm-trunk), boxes
clear. The solve goes legs first, then arms, each on the lever from the joint to
the WORST BAND of the gap (the crotch is 6 cm under the hip; a whole-leg lever
undershot fifteen-fold) and each tightened by bisection; «clear» includes the
game's own boxes not touching (`anim::rest_pose_clear`).

## Contact points and grounding (locomotion-fix wave, 31.08)

The fifteen bones still have no toe, and the acceptance number the character zone
is judged by — how far the planted foot slides — is about a point that is ON the
ground. Two things follow, and both live in `ClipPlayer.h` rather than in the
enum, because neither adds a bone:

- **A foot's CONTACT POINTS are its rig joint plus whatever the IMPORTED skeleton
  hangs off it.** On HumanBase that is `DEF-foot.L → DEF-toe.L`; on a Skyrim
  skeleton it is `NPC L Foot → NPC L Toe0`. This is a rule about the imported
  hierarchy, not a name table, and it is what lets a fifteen-bone rig measure a
  fifty-three-joint model's ball-of-foot contact. Measuring the ANKLE instead is
  fair for a walk and wrong for a run: a running foot lands on the ball, and on
  this asset the jog's ankle never came within 2.6 cm of its own standing height
  while its toe was on the ground.
- **"The foot is down" is measured against that joint's height in OUR REST POSE,**
  not against its own lowest sample in the clip. Every joint has a minimum in
  every clip, including clips where it never approaches the ground.

**Grounding is a property of a clip AT A STRIDE SCALE, measured once.** Scaling a
leg's swing to cover sim's ground also changes how far the leg REACHES, so a
shrunk stride straightens the knee and the pelvis must ride higher. The lift that
puts the deepest contact of the cycle exactly on the ground is stored per scale
(`ClipEntry::ground_curve`) and added to the ROOT joint's translation in the
frame. It is a constant over the cycle on purpose: a per-frame clamp would delete
the flight phase of a run. The jump triple and the seat are excluded by name — a
body that is supposed to leave the ground may not be pulled back onto it.

**Still not foot IK.** There is no per-foot ground raycast and no two-link knee
solve, so on a slope or a stair the pair of feet is still level with each other.
What this adds is the seam such a solver needs: a vertical root correction derived
from a MEASUREMENT, with the measurement currently coming from the clip's own
cycle instead of from the world.

## Layers, masks and hitboxes (wave "stance, weapon, feet, hitboxes", 31.08)

**The fifteen bones did not change and will not change here.** Everything below
adds READERS of the enum; the enum, its order and its parents are still the
contract this document froze.

**A skeleton has two halves, and the split is by DESCENT, not by name.**
`PoseLayers.h` labels every imported joint `Lower` (descending from the joints
`ThighL`/`ThighR` bound to), `Upper` (descending from `Torso`) or `Root` (the
pelvis and the armature node). "Everything hanging off the joint our TORSO bone
bound to" is the same sentence on a Rigify export and on a Skyrim NPC, which is
why there is no per-asset name list. **The root follows the LOWER source
always**: the root joint's translation is where the body stands, and a body
cannot stand in two places. Measured on HumanBase: 43 upper joints, 8 lower,
2 root, of 53.

**A layer is PRE-MULTIPLIED in the parent's frame, never a replacement.** The
arm-relax layer (item 3 of the owner's 31.08 list) turns the upper arms inward
about the model's own +Z; the clip's arm swing survives underneath, which is
the whole difference between a layer and a second pose. **The angle is SOLVED,
not authored**: it is scanned until the hand lands where OUR REST POSE puts it,
so it follows a model with longer arms instead of being a constant that stops
being right. On HumanBase it comes out at 14.5 degrees.

**Fingers relax toward their BIND, not toward an authored open hand.** The bind
is the one pose every asset has; this one keys a closed fist in every clip
including its idle.

**Two grounds, and the frame uses the world's.** `FootIk.h` takes the height of
the ground under each contact point (the app raycasts; anim is handed answers),
shifts the ROOT to the LOWER planted foot and folds the difference out of the
other knee with a two-bone solve, then pitches the ankle so the toe meets its
own ground. **The stance weight is read off the POSE, not off the stride
phase** — "is this frame's foot near the ground" is the same question for a
walk, a run's flight phase, a jump and a crossfade between two clips whose
plants do not coincide. **A foot rises by the WORSE of its two contacts**: at a
stair's nosing the heel and the ball ask for heights 0.18 m apart, more than a
0.11 m foot can span, and standing on the nosing with the heel up is what a
person does.

**`ClipEntry::ground_curve` is now a MEASUREMENT and not a drawing step.** The
per-scale constant it holds still says how deep a clip sits at a stride scale,
and the tests read it; nothing adds it to the root any more, because the foot
solve above supplies the same shift from the world instead of from the clip's
own flat floor. Two mechanisms moving the root would double-count.

**A gear with no clip near it plays a BLEND of the two it has.** `ClipEntry`
carries a second clip and a weight; the two are sampled with their PLANTS
ALIGNED (each shifted by its own footfall phase) and blended before the stride
scale is applied. The weight is solved, not chosen: it is the blend whose
measured cycle travel equals what sim demands. On HumanBase the JOG takes
Jog_Fwd_Loop blended 75 % into Walk_Loop and its stride scale lands at 1.01.

**`ClipRole::WeaponIdle` is a LAYER, not a state.** `role_for_drive` never
returns it; it is what the upper half wears over the legs' locomotion while
`BodyDrive::weapon_drawn` is set, crossfaded over `WEAPON_CROSSFADE_S` (0.2 s).

**Hitboxes are defined by TWO JOINTS, never by a bone plus an offset.**
`Hitbox.h` gives each shape a `from`/`to` joint pair, a span of that line as a
FRACTION, and half extents across it. "Half a thigh down the bone's -Y" is a
statement about our axes; the imported skeleton has its own, and on a Rigify
export they are not close. A line between two joints is the same line in every
skeleton that has both. Sixteen shapes: a sphere for the skull, three boxes for
the trunk (contiguous by construction — a gap would be a stripe no shot can
hit), and one box per limb segment plus hands and feet. `BodyPart` is a
CONTRACT the moment anything serialises it: extend at the END, never reorder.
