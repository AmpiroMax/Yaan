<!--
Created: 09:08:2026 - 00:20:00
Last updated: 09:08:2026 - 11:57:20
-->
<!--
UPD:
- 09:08:2026 - 00:20:00: Initial stage-1 spec: zone contracts, bgfx plan, boundary agreements with core/sim/lead.
- 09:08:2026 - 00:50:00: Stage 2 implementation: backend factories, embedded
  shader decision (--bin2c), exact dependency pins, TerrainMesher header,
  DFN_INTERNAL_RES override, test suite.
- 09:08:2026 - 10:32:00: "Flipped image" investigation: render orientation was
  correct end to end (Metal caps.originBottomLeft=false, identity upscale UVs,
  yflip=false passthrough verified against the pinned bgfx source). Real cause:
  Tour::default_steps assumed ground at y=0 while worldgen ground at the chunk
  center is ~24 m (seed 1) — vantages 00-02 were underground, showing the
  terrain underside. Fix: default_steps(ground_height = 0.0f), app passes the
  terrain height at the chunk center (App.cpp one-liner, lead). Defaulted
  parameter addition to the frozen Tour.h agreed with the lead (Rule 26).
- 09:08:2026 - 11:28:00: Stage 3 «Картинка»: procedural textures (ProcTexture),
  terrain splat v2, sky/fog/sun atmosphere, water plane capability, palette
  post flag (Q9б), Tour v2 six-vantage route + 4-way res/palette matrix.
  Contract sync 10:48 (RenderEnvironment/set_environment, palette_post).
  Boundary agreement with core for stage 3b (SurfaceFieldView, scatter).
- 09:08:2026 - 11:57:20: Stage 3b «Долина видима» (lead-approved batch):
  surface-truth splat from core's SurfaceFieldView (vertex weight channels +
  fs_terrain v3 ordered dither, slope band from SLOPE_GRASS_MAX/SLOPE_ROCK_MIN),
  per-body water (WaterMesher: lake planes + river ribbons from
  ChunkManager::water_bodies), scatter batching (ProcMesh §5 species +
  ScatterBatcher, GRASS_VIEW_DISTANCE micro tiles), §6 site placeholder meshes
  under blessed ids 1..7, "prop" program (backend), Tour v3 §7.1 route with
  lazy ground resolution + tour-driven streaming focus (app wiring by lead).
-->

# Spec — render agent

Written per Q35 / `rules/documentation.md`: seven sections, aimed at a newcomer
who must continue the work from this document alone.

## Zone of responsibility

Per Rule 25:

- `engine/platform/window` — `IWindow` interface + `glfw/` and `null/` backends.
- `engine/platform/input` — `IInput` interface + `glfw/` and `null/` backends.
- `engine/platform/render` — **sources only**: `bgfx/` and `null/` backends.
  `interfaces/IRenderer.h` is the lead-authored FROZEN contract (Q55, Rule 26);
  this zone implements it and never edits it.
- `engine/render` — the render layer on top of `IRenderer`: first-person camera
  with fixed-step interpolation (Rule 12), RenderSystem facade (ECS view ->
  submissions), terrain meshing from chunk heightfields, screenshot tour
  harness (Rule 27), debug-draw helpers. Later: materials, post-process
  (palette), LOD.

## Public interface

Stage-1 headers (all frozen for the stage per Rule 26):

- `engine/platform/window/interfaces/IWindow.h` — `dfn::platform::IWindow`:
  `init(WindowInitParams)` / `shutdown`, `poll_events` (once per frame),
  `should_close` / `request_close`, `native_handle()` (feeds
  `RendererInitParams::native_window_handle`; NSWindow* on macOS, HWND on
  Windows, nullptr from null), `framebuffer_size()` (physical pixels,
  HiDPI-aware), `consume_resize()` (one-shot flag consumed by the app, which
  forwards to `IRenderer::resize`). Polling model, no callbacks — trivially
  implementable by any backend (Rule 4) and by null (Rule 3).
- `engine/platform/input/interfaces/IInput.h` — `dfn::platform::IInput`:
  `update()` once per frame after `poll_events`; `is_down` / `was_pressed` /
  `was_released` over engine-owned `Key` / `MouseButton` enums (stable values,
  append-only — the future rebinding layer serializes them, Q58);
  `mouse_position` (free-cursor mode), `mouse_delta` (pixels, +x right / +y
  down — the mouse-look source), `scroll_delta`, `set_cursor_captured` /
  `is_cursor_captured`. Device level only: action mapping / rebinding is a
  later engine-layer module ON TOP of this interface; gamepad arrives as
  additive `Gamepad*` methods via group sync. Nothing here breaks for either.
- `engine/render/sources/FirstPersonCamera.h` — `dfn::render::FirstPersonCamera`:
  `set_poses(prev, curr)` with shared `dfn::components::CameraPose` snapshots
  (one call per fixed step), `set_projection(fov_y, aspect, near, far)`,
  `interpolated_pose(alpha)`, `view(alpha)` / `proj()` for
  `IRenderer::begin_frame`, `forward`/`right` accessors. Conventions:
  radians/meters (Rule 14); right-handed, Y up, +X east, +Z south; yaw 0
  looks toward -Z, positive yaw turns right; positive pitch looks up; yaw
  blends over the shortest arc; pitch clamped.
- `engine/render/sources/RenderSystem.h` — `dfn::render::RenderSystem`:
  `init`/`shutdown` (program + resource lifetime), `render(world, renderer,
  camera, alpha)` — iterates `world.view<Transform, PreviousTransform,
  RenderMesh>()`, interpolates, culls against `LocalBounds`, submits;
  `upload_terrain(renderer, HeightFieldView)` / `drop_terrain(renderer,
  chunk_coord)` for the chunk pipeline. Platform interfaces are parameters,
  never stored (Rule 9); internal maps are resource bookkeeping (asset id ->
  handle), not game state (Rule 10).
- `engine/render/sources/Tour.h` — `dfn::render::Tour` + `TourStep{label,
  position, yaw, pitch, wait_frames}`: `enabled_by_env()` (`DFN_TOUR=1`,
  `DFN_TOUR_DIR` for output), `begin(steps, dir)`, `apply(camera)` at frame
  start, `post_frame(renderer)` after `end_frame` (waits, saves `NN_label.png`
  via `save_screenshot`, advances; returns true when done -> app calls
  `IWindow::request_close`), `default_steps()` = the stage-2 acceptance route.
  Modeled on Quicky's gloom Tour, reduced to full first-person poses; the game
  itself stays tour-free — only `engine/app` knows the tour exists.
- `engine/render/sources/DebugDraw.h` — free functions over
  `IRenderer::debug_line`: `debug_draw_axes` / `debug_draw_aabb` /
  `debug_draw_grid` / `debug_draw_arrow`. Colors 0xAABBGGRR as in `IRenderer`.

Stage-2 additions (my zone, agreed with the lead at stage kickoff):

- Backend factories (integration convention): `create_glfw_window()`,
  `create_null_window()`, `create_glfw_input(IWindow&)` (requires a
  GlfwWindow), `create_null_input()`, `create_bgfx_renderer()`,
  `create_null_renderer()` — each in
  `engine/platform/<module>/sources/<backend>/Create*.h`.
- `engine/render/sources/TerrainMesher.h` — `TerrainMeshData` +
  `build_terrain_mesh(HeightFieldView)`: pure, deterministic, GPU-free.
- `Tour::internal_res_from_env(fallback)` — parses `DFN_INTERNAL_RES=WxH`.

Stage-3 additions (contract sync 09:08:2026 10:48, applied by the lead):

- `IRenderer::set_environment(const RenderEnvironment&)` — per-frame
  atmosphere + shared material parameters (sun/ambient, fog span+color, sky
  gradient colors, terrain splat thresholds, water color/scroll, visual time).
  Values originate in `engine/render/sources/Materials.h` (look-dev constants,
  explicitly temporary until the design doc; NUMBERS.md migration flagged);
  backends map them to shader uniforms — retuning never recompiles shaders.
  Null backend: accepted and ignored.
- `RendererInitParams::palette_post` (Q9б) — optional post pass in the upscale
  shader: 4x4 Bayer dither in internal-pixel space + nearest-color
  quantization to a fixed 64-color palette (8 ramps x 8 shades,
  `BgfxPalette.{h,cpp}`). `DFN_PALETTE=1` -> AppConfig (lead wiring). OFF by
  default = bit-exact stage-2 passthrough.
- `engine/render/sources/ProcTexture.h` — procedural textures (Q4в): periodic
  integer-hash value-noise fBm (`tileable_fbm`, exact wrap), non-periodic
  `value_noise01`, per-kind recipes (grass/rock/sand/dirt/water, quantized
  5-8 shade ramps), `generate_terrain_atlas` 2x2 layout contract with
  fs_terrain (grass|rock / sand|dirt). Deterministic and byte-stable; cached
  by parameters in RenderSystem under registry-assigned dense asset ids.
- `RenderSystem::set_water/clear_water/water_enabled` + `environment()`
  accessor; `DFN_WATER=<height_m>` debug env in init. Water renders via the
  "water" logical program: backend gives it alpha blend + read-only depth
  (name->state convention, acknowledged by the lead; see the platform README
  table) and RenderSystem submits it after all opaques (scene view is
  sequential since stage 3).
- Backend-internal: "sky" program (fullscreen gradient + sun, drawn first, no
  depth), `u_envParams[11]` uniform layout shared via `shaders/dfn_env.sh`
  (change only together with `apply_environment`), point-sampled material
  textures.

**Boundary agreement for stage 3b (with core, in-session 09:08:2026):**
core adds an ADDITIVE `dfn::math::SurfaceFieldView` (same grid/lifetime as
HeightFieldView; float spans dist_to_water + water_surface with NO_WATER
sentinel, uint8 SurfaceClass mask) exposed via `ChunkManager::surfacefield`,
app ferries it like heightfields; render will rebake per-vertex splat weights
from it (real beaches via dist_to_water, class mask as design truth) and
generalize water to explicit body primitives (lake plane, river ribbon) when
core exposes them. Scatter (P5): per-chunk `ScatterInstance` arrays, drawing
is render's (per-instance submits of shared meshes first; instancing via
contract sync only if profiling demands). Optional future: river flow
direction for current-following water scroll.

## Internal design

**bgfx integration (stage 2, implemented).** bgfx lives exclusively in
`engine/platform/render/sources/bgfx/`, fetched via CMake FetchContent from
`bkaradzic/bgfx.cmake` (bundles bgfx + bimg + bx) pinned to release tag
**v1.153.9398-566**; GLFW pinned to tag **3.4** (Rule 24; both recorded in
this zone's CMakeLists headers). The BgfxRenderer runs bgfx single-threaded
(renderFrame before init), Metal on macOS, and uses three views: 0 = scene
into the internal low-res target (RGBA8 + D24S8, point-sampled), 1 =
backbuffer clear (letterbox black), 2 = integer-scaled upscale quad.
`save_screenshot` schedules a bgfx backbuffer capture into the NEXT end_frame;
the custom `bgfx::CallbackI` writes the PNG via `bimg::imageWritePng`; the
Tour renders flush frames after scheduling so files land before advancing.
Shader compilation per Q50: `shaderc` (from bgfx.cmake tools) runs as CMake
custom commands over `sources/bgfx/shaders/*.sc` with `--bin2c`, generating
embedded headers in the build tree (`shaders_gen/<name>_mtl.h`);
`load_program("terrain"/"unlit")` resolves logical names against the embedded
table ("debug" and "upscale" are backend-internal). DECISION (stage 2):
shaders are embedded rather than loaded from disk — zero runtime path issues
for the tour; `reload_shaders()` is therefore a documented debug no-op this
stage (accepted by the lead), and disk artifacts + real hot-reload arrive with
the stage-3 material work. The null renderer implements the whole contract
inert-but-valid: monotonically increasing handles, all calls succeed,
`save_screenshot` returns false — a runnable headless mode, not a stub
(Rule 3); it carries the headless tour smoke test.

**Low-res target + integer upscale (Q9).** The internal target is fixed at
`INTERNAL_RES` regardless of the window framebuffer; the backend upscales by
the largest integer factor that fits the framebuffer (letterboxed if needed) —
point-sampled, so pixels stay square and crisp. Consumers never see the
internal target: `IRenderer::resize` only re-derives the upscale factor. The
limited palette (Q9) is an optional post-process flag on the upscale pass —
palette-quantization in the blit shader, off by default, toggled by config;
planned for stage 3+, but the two-view structure that makes it a one-shader
change is built in stage 2.

**Camera interpolation (Rule 12).** Sim writes `CameraPose` +
`PreviousCameraPose` (shared components) at each fixed tick; the app computes
alpha = accumulator / SIM_DT and calls `camera.set_poses` + `view(alpha)`.
The camera stores snapshots only — no clock, no ECS access. Sub-tick mouse
latency (render-side view-only rotation offset) is a flagged stage-3 topic
with sim.

**Terrain meshing (stage 2).** A TerrainMesher (internal to `engine/render`,
new file at stage 2) consumes `dfn::math::HeightFieldView`: vertices at each
of resolution×resolution samples, `height_m = height_offset + raw *
height_scale`, normals by central differences, UVs = sample position / chunk
size, vertex color white; indices as two triangles per cell. Shared edge rows
between neighbor chunks (agreed with core) make meshes stitch without cracks.
Output goes straight to `IRenderer::create_mesh` (Vertex layout is the frozen
contract). LOD and skirts are stage-3 topics.

**File discipline.** Each backend file targets ~300 lines, hard cap 800
(Rule 21); BgfxRenderer splits (resources / frame / capture) before nearing
the cap. C++23 with C++20 fallback guards (Rule 19); no C++23-only feature is
currently used in the public headers.

## Dependencies

Uses: `engine/core` (ecs, math, shared components — from `engine/render` only;
platform interfaces include nothing but stdlib + glm per Rule 1/2),
`engine/platform/render/interfaces/IRenderer.h` (frozen). Third-party (stage
2, backends only): bgfx via bgfx.cmake, GLFW — both pinned FetchContent
(Rule 24). Used by: `engine/app` (loop wiring), `engine/editor`, tests, the
tour.

**Boundary agreements (Rule 26), all confirmed in-session 09:08:2026:**

1. **Chunk geometry handoff (with core).** Agreed type
   `dfn::math::HeightFieldView` in `engine/core/math/sources/HeightField.h`
   (core zone; placed in core because world and render are DAG siblings):
   `glm::ivec2 chunk_coord; glm::vec2 origin` (world x,z of sample 0,0);
   `uint32_t resolution` (= HEIGHTMAP_RESOLUTION 129); `float step`
   (= HEIGHTMAP_STEP 2.0 m); `std::span<const uint16_t> heights` (row-major, x
   fastest, `heights[z * resolution + x]`); `float height_scale` (meters per
   raw unit, precomputed offline as (max−min)/65535); `float height_offset`
   (meters). `height_m = height_offset + raw * height_scale`. Conventions:
   right-handed, Y up, +X east, +Z south. Edge rows are shared between
   neighbors (sample 128 of chunk x == sample 0 of chunk x+1). Triangulation
   is render's job. Lifetime: view valid from `ChunkLoaded{coord}` until after
   `ChunkUnloaded{coord}` (both on the core EventBus, batched per streaming
   tick; unload fires before the memory is freed so the mesh is destroyed
   first). Render cannot include world, so the app layer (lead) subscribes and
   routes events to `RenderSystem::upload_terrain` / `drop_terrain` — wiring
   flagged for the lead at the sync.
2. **Shared renderable components (proposed to lead, authored by lead,
   ACKed by core).** `engine/core/components/sources/Components.h`
   (`dfn::components`): `Transform` / `PreviousTransform` (position, quat
   rotation, scale — sim writes both each tick), `CameraPose` /
   `PreviousCameraPose` (eye position + yaw/pitch radians; PENDING sim's
   formal ACK at the sync — sim proposed the identical shape under the name
   EyePose, semantics already agreed with sim directly), `RenderMesh`
   (uint32 mesh_asset / texture_asset name-hash ids; hashes shared from
   core's fnv1a-64 truncated to 32 unless the lead widens the fields),
   `LocalBounds` (glm min/max; may migrate to core's `Aabb` at the sync).
   Components never hold platform handles (Rule 8); `engine/render` maps
   asset id -> `MeshHandle`/`TextureHandle` internally.
3. **ECS surface (with core).** Namespace `dfn::ecs`, include
   `"engine/core/ecs/sources/World.h"`, `World::view<T...>()` yields
   `(EntityId, T&...)`, iteration order unspecified. RenderSystem signature
   `render(ecs::World&, platform::IRenderer&, const FirstPersonCamera&, float)`
   confirmed compatible with core's batch-ops design.
4. **Camera/controller interpolation (with sim, via lead relay + direct
   confirmation).** Sim owns the fixed-tick controller (capsule via IPhysics)
   and writes the eye pose + previous snapshot only at fixed tick; render
   reads (prev, curr, alpha), interpolates (lerp position, shortest-arc yaw,
   clamped pitch) and never writes controller state; sim never sees alpha.
   Angle/axis conventions mirrored in sim's spec.
5. **Skinning (with sim, informational for stage 3).** `IAnim::evaluate`
   yields `std::span<glm::mat4>` — column-major, model-space joint × inverse
   bind pose, ordered by joint index, size = `joint_count`, evaluated at fixed
   tick; null anim gives identity palettes (bind pose in headless tours).
   `create_skinned_mesh` + `submit` overload enter `IRenderer` via contract
   sync at stage 3 (per the frozen header's note), not ad hoc.

**Constants referenced (NUMBERS.md).** `CAMERA_FOV_Y`, `CAMERA_NEAR`,
`CAMERA_FAR`, `INTERNAL_RES` (all provisional, lead-added on render's request —
to confirm at the sync; 640×360 vs 320×180 is a user decision), plus
`CHUNK_SIZE`, `HEIGHTMAP_RESOLUTION`, `HEIGHTMAP_STEP`, `SIM_TICK_RATE` /
`SIM_DT`. No render-zone numeric constant may bypass NUMBERS.md (Rule 14).

## Step-by-step plan

Stage 1 (done): `IWindow.h` / `IInput.h` contracts, `engine/render` public
headers, boundary agreements, module docs, this spec.

Stage 2 (done in this changeset — skeleton, Q37/Q51):
1. Null backends for window/input/render (headless trio, Rule 3) + factory
   headers per the lead's integration convention
   (`Create<Backend><Module>.h`, `create_*` free functions).
2. Zone CMakeLists: `dfn_platform_window` (GLFW 3.4 pin),
   `dfn_platform_input`, `dfn_platform_render` (bgfx.cmake v1.153.9398-566
   pin + shaderc step), `dfn_render`.
3. `glfw/` window + input backends (macOS tested; Win32 branches
   compile-clean, untested).
4. `bgfx/` renderer (see Internal design) + four shader pairs: terrain
   (lambert over vertex ground tint), unlit, debug (lines), upscale.
5. TerrainMesher over `HeightFieldView` (crack-free shared edges);
   FirstPersonCamera; RenderSystem; Tour with `default_steps()` and
   `internal_res_from_env` (DFN_INTERNAL_RES override for the 640x360 vs
   320x180 user decision).
6. Tests: `tests/render.cmake` — mesher, camera, tour (headless), null
   backends.
7. Integration: the LEAD's engine/app drives window -> input -> renderer ->
   RenderSystem -> Tour and shoots the Q51 acceptance frames at both internal
   resolutions.

Stage 3 «Картинка» (done in this changeset):
1. ProcTexture module + tests (determinism, exact tiling, atlas layout,
   low color count).
2. Contract sync batch (lead applied 10:48): RenderEnvironment +
   set_environment, palette_post; both backends implement.
3. Shaders v2: terrain atlas splat (slope/height/dryness uniforms) + fog;
   new sky and water pairs; upscale palette+dither; shared dfn_env.sh.
4. RenderSystem v2: atlas/water textures (dense-id registry cache), frame
   environment (Materials.h), water plane API + DFN_WATER debug env.
5. Tour v2 six-vantage route; 4-way matrix (2 res x palette on/off) shot via
   tools/run_tour.sh; all frames read and checked (Rule 27) — see devlog
   img for the sync.
6. Look-dev calibration measured, not guessed: seed-1 slope statistics probed
   (p99 = 0.005) -> rock band 0.0025-0.0060 documented as flat-worldgen
   placeholder; fog span 0.25-0.70 of CAMERA_FAR hides the streaming edge.

Stage 3b (done in this changeset — «make the generated valley visible»):
1. TerrainMesher v3: vertex color re-purposed to splat weights (R sand /
   G rock / B water-bed / A dryness) from SurfaceFieldView.surface_class;
   upload_terrain 3-arg overload (2-arg = slope-only fallback).
2. fs_terrain v3: weight+slope splat, ordered 4x4 Bayer transitions in
   internal-pixel space (§4 "dither, not gradients"); slope thresholds derive
   from config SLOPE_GRASS_MAX / SLOPE_ROCK_MIN via 1 - cos (Materials.h).
3. WaterMesher (pure): lake ellipse fans + river ribbon strips (per-station
   width, descending surface); RenderSystem::set_water_bodies/clear_water_
   bodies over ChunkManager::water_bodies() (app ferry, lead). set_water +
   DFN_WATER demoted to debug fallback.
4. ProcMesh (pure): §5 species (oak ball-on-stump / pine cones / pale leaning
   birch / bush / stone) + §6 silhouette-coded structures, blessed RenderMesh
   ids 1..7 registered at init (site entities render via the ECS path, now on
   the lit+fogged "prop" program — backend pairs vs_terrain + fs_prop).
5. ScatterBatcher (pure): world-space baked per-chunk batches — trees always,
   bush/stone in 4x4 micro tiles culled by GRASS_VIEW_DISTANCE; RenderSystem
   upload_scatter/drop_scatter mirror the terrain pair (app ferry, lead).
6. Tour v3: testbed_steps() at the §7.1 layout coords (crag money shot, river
   ford, lake bluff, hamlet approach, forest species, overview); additive
   TourStep::ground_relative + begin(..., ground_at) lazy resolution +
   focus_position() so the app streams around the tour camera (far vantages
   are not resident at arm time — the single-height lesson, generalized).
7. Tests: 10 render suites (added proc mesh budgets/bounds/determinism,
   scatter batching, water meshing, splat weight channels, tour v3 shape).

Stage 4 (next): skinned meshes (contract sync), frustum culling with core's
math types, LOD/skirts, grass cards (P6 micro, §5.6), flower patches,
sub-tick mouse-look offset, editor render hooks, shader hot-reload from disk,
instancing sync if scatter profiling demands.

## How it is verified

- **Stage-3b acceptance (Rule 27):** `bash tools/run_tour.sh <build>` shoots
  the 4-way matrix (640x360 / 320x180 x palette on/off) over the Tour v3
  route (7 frames: crag-from-hamlet, crag final approach, river, lake shore,
  hamlet approach, forest species, overview) — 28 frames, all read in the
  09:08:2026 look-dev loop (6 iterations, vantages corrected against the
  GENERATED world probed via scratch tools, not the §7.1 plan table).
  Findings recorded in the DONE report: pine strips out-angle the L0 from
  town ground (C4 violation -> design/core); hydrology drifted from the §7.1
  coordinates (fords/lake); WaterBed mud margins are 2.7% of the world and
  read very wide near bends.
- **Stage-3 acceptance (Rule 27):** `DFN_WATER=15 bash tools/run_tour.sh
  <build>` shoots the 4-way matrix (640x360 / 320x180 x palette on/off), six
  frames each. Checklist per frame: textures tile without obvious repetition
  at eye level; slope splat reads (grey rock on the steepest ground — faint by
  design on the flat stage-2 worldgen, see Materials.h); horizon fog blends
  terrain into the sky horizon color with no visible world edge (all vantages
  aim into the testbed interior); sky gradient + sun disc; water plane with
  beach band; palette mode visibly quantizes (Bayer dither) without destroying
  readability; both resolutions consistent. All 24 frames read by the agent in
  this stage's look-dev loop; the rock-splat mechanism additionally verified
  with a temporary aggressive threshold shoot (not shipped).
- **Stage-2 acceptance criterion (Q51, kept green):** `DFN_TOUR=1` run captures
  screenshots, none black, with visible ground and a correct horizon. Checked
  frame by frame against the checklist (Q24); golden-image pixel comparison
  later.
- Rule 27: any image-affecting change re-runs the tour before review.
- Headless: the same tour under null window/render must walk all steps and
  exit 0 (Rule 3) — CI smoke test without a GPU.
- doctest suites (stage 2): camera interpolation math (alpha 0/0.5/1,
  shortest-arc yaw across ±pi, pitch clamp), TerrainMesher (vertex count,
  edge-row equality between neighbor views, normals on a known slope), input
  edge detection semantics on the null backend.
- `python3 tools/header_check.py --all` passes; zero warnings with
  `-Wall -Wextra -Wpedantic` on both toolchains (build gates).

## What this zone does NOT do

- Does not edit `IRenderer.h` or any frozen contract — change requests go to
  the lead for a group sync (Rule 26).
- Does not generate or own world data: no heightmap generation, no chunk
  streaming policy, no world file IO (core's world zone). Render only
  consumes `HeightFieldView`s handed to it.
- Does not own shared components — proposals go to the lead
  (`engine/core/components`).
- Does not simulate: no camera physics, no collision, no controller logic
  (sim). The camera never integrates motion, it only blends sim snapshots;
  gameplay code never calls `IRenderer` directly.
- Does not do UI/editor drawing (Dear ImGui is `engine/editor`'s documented
  exception) and does not own the app loop (`engine/app`, lead).
- Does not leak third-party types: bgfx/GLFW appear only under
  `engine/platform/*/sources/`; no other layer sees them (Rule 1).
- Does not hardcode constants that belong in NUMBERS.md (Rule 14) and does
  not put user-facing strings in C++ (Rule 5 — window titles are localized by
  the caller).
- Does not install anything; all dependencies are pinned FetchContent
  (Rule 24).
