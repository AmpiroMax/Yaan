<!--
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 00:50:00
-->
<!--
UPD:
- 09:08:2026 - 00:16:00: Stage-1 state: lead-authored frozen interface, no backends yet.
- 09:08:2026 - 00:50:00: Stage 2 — bgfx + null backends, shaderc build step.
-->

# engine/platform/render

## Responsibility

Platform rendering contract (Rule 0). `interfaces/IRenderer.h` is the
lead-authored FROZEN contract (Q55, Rule 26) — bgfx lives only behind it.
Backends (`sources/bgfx/`, `sources/null/`) are owned by the render agent and
arrive in stage 2.

## Key types

- `dfn::platform::IRenderer` — init/shutdown/resize, `begin_frame(view, proj)` /
  `end_frame`, `create_mesh`/`create_texture`/`load_program` + destroys,
  `submit`, `debug_line`, `save_screenshot` (tour backbone, Rule 27),
  `reload_shaders` (debug hot-reload, Q50).
- `RendererInitParams` — native window handle (from `IWindow`), framebuffer
  size, low-res internal target (Q9), vsync.
- `MeshHandle` / `TextureHandle` / `ProgramHandle` — opaque POD handles, 0 =
  invalid. `Vertex` — fixed stage-2 layout (position, normal, uv, color).

## Usage example

```cpp
dfn::platform::RendererInitParams p;
p.native_window_handle = window.native_handle();
const glm::uvec2 fb = window.framebuffer_size();
p.framebuffer_width = fb.x; p.framebuffer_height = fb.y;
p.internal_width = 640; p.internal_height = 360; // provisional, NUMBERS.md
renderer.init(p);
renderer.begin_frame(camera.view(alpha), camera.proj());
renderer.submit(mesh, program, transform);
renderer.end_frame();
```

## Dependencies

- Uses (interface): C++ stdlib, glm (Rule 2). Backends (stage 2): bgfx via
  pinned FetchContent (Rule 24) in `sources/bgfx/` only.
- Used by: `engine/render` (primary consumer), `engine/editor`, `engine/app`,
  tests (null backend), the screenshot tour.

## Current state (stage 2)

Implemented: `sources/bgfx/` — BgfxRenderer (Metal on macOS, single-threaded
bgfx; view 0 scene -> low-res internal target, view 1 letterbox clear, view 2
integer-scaled point-sampled upscale; screenshots via a custom bgfx callback
writing PNG through bimg; embedded shaders compiled by shaderc custom
commands with --bin2c; reload_shaders is a documented debug no-op this
stage) and `sources/null/` (all calls succeed, save_screenshot returns
false). Shaders: `sources/bgfx/shaders/*.sc` — terrain (directional lambert
over vertex ground tint), unlit, debug lines, upscale. Factories:
`sources/bgfx/CreateBgfxRenderer.h`, `sources/null/CreateNullRenderer.h`.
Target: `dfn_platform_render`; pins GLFW-independent bgfx.cmake at tag
v1.153.9398-566 (Rule 24). The interface stays frozen; changes go through a
group sync with the lead.
