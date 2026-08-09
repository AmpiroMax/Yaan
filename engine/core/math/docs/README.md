<!--
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 11:05:22
-->
<!--
UPD:
- 09:08:2026 - 00:16:55: Stage 1 — public contract documented (headers only, no implementation yet).
- 09:08:2026 - 00:42:03: Stage 2 — implemented (Aabb/Ray/Frustum/Intersect .cpp); suite tests/core/MathTests.cpp.
- 09:08:2026 - 11:05:22: Stage 3b — added SurfaceField.h (additive core<->render handoff: SurfaceFieldView, ScatterInstance, LakePlane/RiverStation). HeightField.h untouched.
-->

# engine/core/math

## Responsibility

Thin extensions of glm (Rule 2 — glm is used directly, never wrapped): AABB,
frustum, ray, intersection tests, and the cross-zone `HeightFieldView` contract.
Units: meters, radians (Rule 14).

## Key types

- `Aabb` (`sources/Aabb.h`) — min/max box; expand/contains/overlaps/transformed.
- `Plane`, `Frustum`, `Containment` (`sources/Frustum.h`) — six-plane frustum from
  a view-proj matrix; AABB/sphere classification for culling.
- `Ray` (`sources/Ray.h`) — origin + unit direction.
- `RayHit`, `ray_vs_*`, `aabb_vs_aabb` (`sources/Intersect.h`) — pure intersection
  functions.
- `HeightFieldView` (`sources/HeightField.h`) — FROZEN boundary contract (Rule 26,
  core/render/sim): non-owning view of one chunk's raw uint16 heights;
  `height_m = offset + raw * scale`; row-major, x fastest; +X east, +Z south, Y up.
- `SurfaceClass`, `NO_WATER`, `SurfaceFieldView`, `ScatterSpecies`,
  `ScatterInstance`, `LakePlane`, `RiverStation` (`sources/SurfaceField.h`) —
  stage-3b ADDITIVE companion agreed core<->render: per-sample splat/water
  inputs (dist-to-water, water surface, class mask), per-chunk scatter
  instances, explicit water-body primitives. Same grid conventions and
  lifetime as HeightFieldView; produced by world::ChunkManager.

## Usage example

```cpp
auto frustum = dfn::math::Frustum::from_view_proj(proj * view);
if (frustum.visible(chunk_bounds)) { /* submit */ }
if (auto hit = dfn::math::ray_vs_aabb(ray, box)) { use(hit->point); }
```

## Dependencies

Uses glm only. Used by engine/render (culling, terrain meshing input),
engine/physics (terrain collision input), engine/world (chunk bounds, height
sampling), gameplay (picking/interaction).
