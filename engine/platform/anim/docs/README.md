
# engine/platform/anim

Zone: `character` (Rule 25, carved from sim 10:08:2026).

## Responsibility

The platform skeletal animation contract (Rule 0): skeleton/clip loading
(runtime format), per-character instances, sampled + blended evaluation writing
skinning matrices into plain `glm::mat4` spans. ozz-animation lives only behind
`interfaces/IAnim.h`. No renderer dependency — output is matrix spans.

## Key types

- `IAnim` — load skeleton/clip, `joint_count`, `clip_duration`, instances,
  `evaluate(instance, layers, out_span)`.
- `AnimLayer` — one (clip, time, weight) blend input.
- `SkeletonHandle`, `ClipHandle`, `AnimInstanceHandle` — opaque POD handles.

## Usage example

```cpp
auto skeleton = anim.load_skeleton("assets/rigs/humanoid.ozz");
auto walk = anim.load_clip(skeleton, "assets/anim/walk.ozz");
auto instance = anim.create_instance(skeleton);
std::vector<glm::mat4> palette(anim.joint_count(skeleton));
dfn::platform::AnimLayer layers[] = {{walk, time_s, 1.0f}};
bool ok = anim.evaluate(instance, layers, palette); // skinning-ready matrices
```

## Dependencies

- Uses: stdlib + glm only (Rule 1).
- Used by: `engine/anim` (state machines, humanoid rig contract), `engine/render`
  (stage 3, palettes into skinned submit via contract sync), tests.
- Backends (stage 2/3): `sources/ozz/` (ozz-animation, FetchContent pinned),
  `sources/null/` — runnable mode: loads succeed, evaluate fills identities
  (bind pose), headless tours never crash.
