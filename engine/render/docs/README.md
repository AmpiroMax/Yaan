<!--
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 11:25:00
-->
<!--
UPD:
- 09:08:2026 - 00:16:00: Stage-1 state: public headers only (camera, render system, tour, debug draw).
- 09:08:2026 - 00:50:00: Stage 2 — implementations + TerrainMesher; tests.
- 09:08:2026 - 10:31:00: Tour::default_steps(ground_height) — vantages offset by the app-supplied terrain height at the chunk center (old absolute heights sat under the generated surface and showed the terrain underside, mistaken for a flipped image).
- 09:08:2026 - 11:25:00: Stage 3 «Картинка» — ProcTexture module, Materials.h look-dev environment, RenderSystem v2 (splat atlas, water plane, RenderEnvironment), Tour v2 six-vantage route, mesher dryness alpha.
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
  tour; stage-3 route = six vantages (texture tiling, fog/horizon, slope splat,
  water valley, overview, sky+sun), all aimed into the testbed interior.
- Debug draw free functions (`sources/DebugDraw.h`) — axes, AABB, grid, arrow
  over `IRenderer::debug_line`.
- ProcTexture (`sources/ProcTexture.h`, stage 3) — procedural textures (Q4в:
  code only, no image files): `tileable_fbm` / `value_noise01` (deterministic
  integer-hash noise), `generate_proc_texture` (grass/rock/sand/dirt/water,
  palette-quantized 5-8 shades), `generate_terrain_atlas` (2x2 atlas, layout
  contract with fs_terrain.sc: grass|rock / sand|dirt).
- Materials.h (stage 3) — ALL look-dev visual constants (sun/sky/fog colors,
  fog span tied to CAMERA_FAR, splat thresholds, water params, texture seeds)
  + `make_default_environment()` -> `platform::RenderEnvironment`. Explicitly
  temporary until the landscape design doc; values ship to the GPU as uniforms,
  so retuning never touches shaders.

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

## Current state (stage 3)

Everything from stage 2 (FirstPersonCamera with shortest-arc yaw +
perspectiveRH_ZO, crack-free TerrainMesher, Tour flush-frame screenshot
scheduling, DebugDraw) plus the stage-3 visual foundation:

- **RenderSystem v2**: on init generates + uploads the procedural terrain
  atlas and water texture (cached by params; registry-assigned dense asset
  ids), loads the "water" program, seeds the frame `RenderEnvironment` from
  Materials.h and sends it via `IRenderer::set_environment` each render
  (time_seconds from a render-side visual clock — never simulation time,
  Rule 12). Terrain submits carry the atlas; the water plane (if set) is
  submitted last (transparent).
- **Water capability**: `set_water(renderer, height_m, center_xz,
  half_extent_m)` / `clear_water` / `water_enabled` — one flat semi-transparent
  animated plane; setting water raises the environment sand line just above
  the waterline (beaches). Debug env: `DFN_WATER=<height_m>` enables a
  testbed-covering plane at init. Proper multi-body placement arrives with
  core's SurfaceFieldView/water primitives (agreed 09:08:2026, stage 3b).
- **TerrainMesher v2**: vertex alpha now carries grass<->dirt "dryness"
  (two-octave world-space value noise — continuous across chunk borders);
  rgb tint kept for the untextured fallback and large-scale variation.
- Tests: `tests/render.cmake` — mesher, camera, tour headless, null backends,
  proc textures, palette, render system (water/env) — 7 suites.

Look-dev findings recorded for the design phase: the stage-2 worldgen is a
near-flat plain (slope p99 = 0.005), so the rock splat band is set to
0.0025-0.0060 to catch its steepest ~5%; realistic values after worldgen v2
will be ~0.07-0.18. All in Materials.h.
