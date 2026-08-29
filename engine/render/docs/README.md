<!--

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
- `dfn::render::MapScreen` (`sources/MapScreen.h`) — the world map screen:
  `note_chunk` (bakes a top-down tile per VISITED chunk: averaged height, hill
  shade, water OR), `note_site` (marker memory keyed to the blessed mesh ids
  1..11), `compose(w, h, eye, yaw)` -> a `PixelCanvas` with the plate, the
  frame, the north tick, the site silhouettes and the player arrow.
  `toggle_map()` on RenderSystem opens it; `DFN_MAP=1` opens it at init.
- `dfn::render::PixelCanvas` (`sources/PixelCanvas.h`) — the first UI
  primitive: a clipped CPU raster surface (rects, frames, 1-bit stamps,
  triangles) in internal-resolution pixels, uploaded as one RGBA8 texture and
  blitted by `RenderSystem::draw_overlay`. Screen-agnostic on purpose: the
  planned start menu draws through the same two calls. `clear_transparent()`
  makes it a HUD instead of a screen.
- `RenderSystem::ScreenProp` + `set_screen_prop` (27.08) — ONE MESH DRAWN OVER
  THE SCREEN CANVAS, today the main menu's 3D emblem (`engine/app/sources/
  MenuEmblem.*`). The transform arrives in CAMERA AXES and is composed with
  `inverse(view)` here, so the prop hangs in front of the eye wherever the
  camera happens to stand. Submitted immediately AFTER the HUD blit, which
  puts it on top: the scene view is `Sequential`, and the canvas quad is
  blended and writes no depth. It lives ONE frame and is cleared on submit,
  like the transient lights — a playing frame must not inherit the emblem from
  the last menu frame. The two rejected alternatives, kept here because they
  are not visible in the code: submitting BEFORE the canvas (menu pages clear
  it opaque, so not one pixel would show) and punching a hole in the canvas
  (nothing is guaranteed to be behind it — leaving a game to the main menu
  does not unload the world).
- `RenderSystem::overlay_depth_m(camera)` — how far in front of the eye the
  screen canvas hangs, ONE definition (Rule 39). It was an expression inside
  `draw_overlay` while that was its only reader; the emblem, which has to sit
  in front of the canvas, is the second.
- `dfn::render::BitmapFont` (`sources/BitmapFont.h`) — THE FONT. A fixed-cell
  6x9 atlas (5x8 ink plus the built-in gaps, so the cell IS the advance),
  printable ASCII + the whole Cyrillic alphabet + « » — (166 codepoints),
  baked once. `draw_text(canvas, x, y, utf8, colour, shadow)` and
  `text_width_px` are the entire surface: NO wrapping, kerning or newline
  handling, by decision. Anything unmapped — an unknown codepoint, malformed
  UTF-8, or a glyph nobody drew — renders as a SOLID BLOCK, because nothing
  else in the font fills its cell and absence must never look like a space.
  Cyrillic letters shaped like Latin ones are aliases of that art, not copies.
  No user-facing string appears here or in any render caller (Rule 5): every
  entry point takes UTF-8 the caller resolved from a localization file.
- `RenderSystem::hud()` — a transparent screen-space canvas the CALLER draws
  into every frame (the interaction prompt, later the crosshair), composited
  over the world and under the map through the backend's alpha-blended
  `"overlay"` program. render owns the surface and the blit; it never owns the
  words, because the words are a localization lookup.
- `dfn::render::SkyModel` (`sources/SkyModel.h`) — `apply_sky_time(env,
  day_fraction, lunar_phase)`: the whole day/night look from two normalized
  fractions (sun and moon direction, sun/ambient/fog/sky colours, moonlight,
  stars). Moon direction is DERIVED from phase, so a full moon rises at
  sunset. Also `TORCH_COLOR`/`TORCH_RADIUS_M` for the carried light.
- `dfn::render::CloudModel` (`sources/CloudModel.h`) — the drift of THE cloud
  coverage field (`apply_clouds`) plus that field's CPU REFERENCE
  (`cloud_field` / `cloud_field3` / `cloud_alpha`), mirrored by
  `dfn_env.sh` on the GPU. The reference exists so the field's DISTRIBUTION can
  be asserted rather than assumed (Rule 31), which is why each function ships
  with a rejected form beside it: `cloud_field_raw` (pre-CDF, Gaussian) and
  `cloud_field_fixed_sd` (the LOD remapped through the full-resolution SD — the
  R3.3 defect). `cloud_lod_residual` is how much of the field survives the
  per-octave LOD at a sampling rate, and it is what the coverage threshold and
  the far-field convergence are both keyed to.
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
  proc textures, palette, render system (water/env), proc meshes, scatter
  batcher, water mesher — 10 suites.

Stage 3b («make the generated valley visible», lead-approved batch):

- **Surface-truth splat**: `upload_terrain(renderer, heightfield, surface)`
  overload takes core's `math::SurfaceFieldView` (spec Dependencies item 8);
  TerrainMesher bakes vertex-color WEIGHTS (R sand / G rock / B water-bed /
  A dryness) from `surface_class`; fs_terrain v3 augments rock by slope
  (thresholds derived from SLOPE_GRASS_MAX/SLOPE_ROCK_MIN via 1-cos) and
  dithers transitions with an ordered 4x4 Bayer in internal-pixel space
  (LANDSCAPE §4). The stage-2/3 two-arg upload stays (slope-only fallback).
- **Per-body water**: `set_water_bodies(renderer, lakes, stations, offsets)`
  builds one ellipse plane per `math::LakePlane` and one ribbon per river
  segment from `math::RiverStation` polylines (width per station, surface
  descending source -> mouth; WaterMesher, pure + unit-tested). Bodies extend
  LOOKDEV_WATER_EDGE_MARGIN_M past the banks; overlap hides under terrain via
  depth test. `set_water`/DFN_WATER stays as the debug fallback only.
- **Scatter drawing**: `upload_scatter(renderer, coord, instances)` /
  `drop_scatter` — P5 data (never entities) baked into per-chunk batches
  (ScatterBatcher): one tree batch (oak/pine/birch per the §5 silhouettes,
  ProcMesh) always drawn; bush/stone micro tiles (4x4 per chunk) culled by
  GRASS_VIEW_DISTANCE from the eye. Batching keeps the frozen IRenderer
  contract (instancing only via a future contract sync if profiling demands).
- **Site placeholders**: ProcMesh builds §6 silhouette-coded structures
  (gable dwelling, porch trader, two-storey L tavern, tall-roof barn, spired
  shrine, dark portal, broken tower) registered at init under the
  lead-blessed RenderMesh ids 1..7 — chunk-streamed site entities render with
  zero extra wiring. ECS submissions moved from "unlit" to the lit+fogged
  "prop" program.
- **Tour v3**: `testbed_steps()` — the LANDSCAPE §7.1 route (crag-from-hamlet
  money shot, river ford, lake bluff, hamlet approach, forest species,
  overview); steps are ground_relative, resolved per frame through the
  `begin(steps, dir, ground_at)` callback while the app streams around
  `focus_position()` (far vantages' chunks are not resident at arm time).

Look-dev note: the flat-worldgen rock band (0.0025-0.0060) is gone; slope
thresholds now derive from the design constants in NUMBERS.md. Placeholder
mesh dimensions/colors cite LANDSCAPE §5/§6 and move to data files with the
content pipeline (Rule 5).

Map screen (user request «миникарта как в скайриме», after the feature-requests
batch):

- **No IRenderer change (Rule 26)**: the screen is composed on the CPU into a
  `PixelCanvas`, uploaded as one RGBA8 texture and drawn as an unlit quad that
  exactly fills the frustum just past the near plane (`draw_overlay`). The
  world keeps rendering behind the opaque map, so the toggle costs nothing and
  the app loop is untouched.
- **Explored only**: a chunk enters the map in `upload_terrain` (i.e. when the
  player streamed it in) and never leaves; sites are noted in the ECS pass
  BEFORE the mesh lookup, so the castle (ids 8..11, no placeholder mesh yet)
  is on the map anyway. Castle parts merge into one marker.
- **Legibility findings (measured on the frames, not guessed)**: normalizing
  the elevation ramp over 0..WORLDGEN_MAX_HEIGHT flattened the whole valley
  into one green band — the ramp is stretched over the EXPLORED span instead;
  and true-scale hill shading is invisible on ground this gentle, so shading
  uses a cartographic z-factor of 4 and is normalized against flat ground.
- **Scale**: MAP_TILE_PX = 80 px per chunk (3.2 m per map pixel) -> the 1024 m
  testbed is a 320x320 plate inside 640x360; compose() only picks an integer
  DOWNSCALE (2 at 320x180), never a fractional zoom, so pixels stay square.
