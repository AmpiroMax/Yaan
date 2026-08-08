<!--
Created: 09:08:2026 - 00:20:00
Last updated: 09:08:2026 - 00:20:00
-->
<!--
UPD:
- 09:08:2026 - 00:20:00: Initial stage-1 spec: zone contracts, bgfx plan, boundary agreements with core/sim/lead.
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

## Internal design

**bgfx integration plan (stage 2).** bgfx arrives exclusively in
`engine/platform/render/sources/bgfx/`, fetched via CMake FetchContent from
`bkaradzic/bgfx.cmake` (the CMake-native distribution bundling bgfx + bimg +
bx) pinned to an exact release tag (Rule 24; the concrete tag is chosen at
stage-2 start by verifying one tag against both toolchains — Homebrew clang 22
and Apple clang 15 — and is recorded in `engine/platform/render/CMakeLists.txt`,
never a branch name). The BgfxRenderer implements `IRenderer` with two bgfx
views: view 0 renders the scene into an offscreen framebuffer at
`INTERNAL_RES` (NUMBERS.md, provisional 640×360), view 1 blits/upscales to the
window backbuffer. `save_screenshot` requests a bgfx frame capture of the final
view and writes a PNG. Shader compilation per Q50: `shaderc` (built as part of
bgfx.cmake's tools) runs as a custom CMake build step over
`engine/platform/render/sources/bgfx/shaders/*.sc`, emitting per-API binaries
(metal + spirv now, dxbc later on Windows) into the build tree under logical
names; `load_program("terrain")` resolves name -> per-API binary at runtime, so
consumers never see paths or APIs. Shader hot-reload in debug builds (Q50):
`reload_shaders()` re-stats the compiled artifacts and recreates programs whose
files changed; a debug key in `engine/app` (and later the editor) triggers it —
combined with the CMake step, "edit .sc, rebuild shaders target, press reload"
needs no app restart. The null renderer (`sources/null/`) implements the whole
contract inert-but-valid: monotonically increasing handles, all calls succeed,
`save_screenshot` returns false — a runnable headless mode, not a stub
(Rule 3).

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

Stage 1 (this changeset — contracts only, done):
1. `IWindow.h`, `IInput.h` public contracts.
2. `engine/render` public headers: FirstPersonCamera, RenderSystem, Tour,
   DebugDraw.
3. Boundary agreements above; module `docs/README.md` files; this spec.

Stage 2 (skeleton, Q37/Q51 — order chosen so teammates unblock early):
1. `engine/platform/window/sources/null` + `engine/platform/input/sources/null`
   + `engine/platform/render/sources/null` — the headless trio first, so core
   and sim can run ECS/physics tests against real interfaces immediately.
2. Zone CMakeLists (window/input/render layers) with GLFW + bgfx.cmake
   FetchContent pins; verify tags on both toolchains.
3. `glfw/` window + input backends (macOS first).
4. `bgfx/` renderer: init with native handle, internal target + integer
   upscale, clear + frame; then mesh/texture/program resources; then submit
   path; then `save_screenshot`; then `debug_line`; then `reload_shaders`.
5. shaderc CMake step + the two stage-2 shader pairs ("terrain", "unlit").
6. TerrainMesher over `HeightFieldView`; RenderSystem submit path + camera.
7. Tour implementation + `default_steps()` for the flat test chunk; run the
   stage-2 acceptance tour (criterion below) and file the frames with the lead
   for the devlog.

Stage 3+ (after the next sync): palette post-process flag, skinned meshes
(contract sync), frustum culling with core's math types, LOD/skirts, sub-tick
mouse-look offset, gamepad methods, editor render hooks.

## How it is verified

- **Stage-2 acceptance criterion (Q51):** `DFN_TOUR=1` run captures **4
  screenshots, none black, with visible ground and a correct horizon** (the
  horizon line sits where CAMERA far plane meets the flat chunk at eye height,
  i.e. not at the frame's top or bottom edge in a level shot). Checked frame
  by frame against the checklist (Q24); golden-image pixel comparison later.
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
