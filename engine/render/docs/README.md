<!--
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 00:16:00
-->
<!--
UPD:
- 09:08:2026 - 00:16:00: Stage-1 state: public headers only (camera, render system, tour, debug draw).
-->

# engine/render

## Responsibility

The engine render layer on top of `IRenderer`: first-person camera with
fixed-step interpolation (Rule 12), the RenderSystem facade that turns ECS
views and chunk heightfields into draw submissions, the screenshot tour
harness (Rule 27), and debug-draw helpers. Never touches bgfx (Rule 1).

## Key types

- `dfn::render::FirstPersonCamera` (`sources/FirstPersonCamera.h`) — consumes
  shared `dfn::components::CameraPose` prev/curr snapshots, blends with the
  frame alpha, builds view/proj for `IRenderer::begin_frame`.
- `dfn::render::RenderSystem` (`sources/RenderSystem.h`) — `render(world,
  renderer, camera, alpha)`; `upload_terrain`/`drop_terrain` consume
  `dfn::math::HeightFieldView` (boundary agreed with core). Maps engine asset
  ids to renderer handles internally.
- `dfn::render::Tour`, `TourStep` (`sources/Tour.h`) — `DFN_TOUR=1` screenshot
  tour; stage-2 acceptance = 4 non-black frames with ground and horizon (Q51).
- Debug draw free functions (`sources/DebugDraw.h`) — axes, AABB, grid, arrow
  over `IRenderer::debug_line`.

## Usage example

```cpp
// App loop sketch (stage 2):
camera.set_poses(prev_pose, curr_pose);          // once per fixed sim step
render_system.render(world, renderer, camera, alpha); // every render frame
if (tour.active()) {
    tour.apply(camera);
    if (tour.post_frame(renderer)) window.request_close();
}
```

## Dependencies

- Uses: `engine/core` (ecs World, math HeightFieldView, shared components),
  `engine/platform/render` (IRenderer). glm per Rule 2.
- Used by: `engine/app` (main loop), `engine/editor`.

## Current state (stage 1)

Public headers only — no `.cpp` (contracts-only stage, Q38/Q51). Internal
design and the stage-2 plan (terrain mesher, shader set, tour route): see
`docs/specs/render.md`.
