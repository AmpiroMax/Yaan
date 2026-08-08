<!--
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 00:16:00
-->
<!--
UPD:
- 09:08:2026 - 00:16:00: Stage-1 state: lead-authored frozen interface, no backends yet.
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

## Current state (stage 1)

Frozen interface only — no `sources/` yet (contracts-only stage, Q38/Q51).
Backend plan (bgfx FetchContent pin, shaderc CMake step, hot-reload, low-res
target + integer upscale): see `docs/specs/render.md`. Do not edit the
interface; changes go through a group sync with the lead.
