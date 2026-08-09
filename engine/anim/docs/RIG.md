<!--
Created: 10:08:2026 - 01:54:36
Last updated: 10:08:2026 - 01:54:36
-->
<!--
UPD:
- 10:08:2026 - 01:54:36: Initial humanoid rig contract (15 bones, frozen), stride-phase
  seam with sim recorded, mesh id table 34..48 (pending render's ack), mirror math.
-->

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
