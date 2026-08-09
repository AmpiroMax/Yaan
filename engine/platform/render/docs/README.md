<!--
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 20:50:00
-->
<!--
UPD:
- 09:08:2026 - 00:16:00: Stage-1 state: lead-authored frozen interface, no backends yet.
- 09:08:2026 - 00:50:00: Stage 2 — bgfx + null backends, shaderc build step.
- 09:08:2026 - 11:26:00: Stage 3 — RenderEnvironment/set_environment + palette_post (contract sync 10:48), sky pass, water state, palette post, point-sampled textures.
- 09:08:2026 - 11:57:20: Stage 3b — "prop" logical program (vs_terrain + fs_prop); terrain fragment v3 (splat-weight vertex channels + ordered dither).
- 09:08:2026 - 14:11:37: Dynamic sun shadows (в1): shadow view 0 (views renumbered), depth-only "shadow" program, dfn_shadow.sh sampling in terrain/prop; terrain fragment v4 (surface-class-only splat — legacy height-sand and dirt dryness removed per design ruling).
- 09:08:2026 - 20:50:00: Interior lighting: CUBE SHADOWS for carried lights (12 face views into one distance atlas, "point_shadow" program, dfn_pointshadow.sh), caster culling from mesh bounds measured at create_mesh, and the touch-order rule that empty draws must precede every setUniform of the frame.
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

## Current state (stage 3 + shadows batch)

Implemented: `sources/bgfx/` — BgfxRenderer (Metal on macOS, single-threaded
bgfx; view 0 sun shadow map (depth only), views 1-12 the carried-light cube
faces, view 13 scene -> low-res internal target, view 14 letterbox clear,
view 15 integer-scaled point-sampled upscale; screenshots via a custom bgfx callback
writing PNG through bimg; embedded shaders compiled by shaderc custom
commands with --bin2c; reload_shaders is a documented debug no-op this
stage) and `sources/null/` (all calls succeed, save_screenshot returns
false, set_environment accepted-and-ignored). Factories:
`sources/bgfx/CreateBgfxRenderer.h`, `sources/null/CreateNullRenderer.h`.
Target: `dfn_platform_render`; pins bgfx.cmake at tag v1.153.9398-566
(Rule 24). The interface stays frozen; changes go through a group sync with
the lead (last: 09:08:2026 10:48 — RenderEnvironment/set_environment +
RendererInitParams::palette_post).

Stage-3 additions in the bgfx backend:

- **Environment uniforms**: `set_environment` caches the RenderEnvironment;
  `begin_frame` packs it into the `u_envParams[11]` vec4 array (index layout =
  `shaders/dfn_env.sh`, shared by terrain/water/sky fragments).
- **Sky pass**: backend-internal "sky" program; fullscreen quad drawn first in
  the scene view (view 0 is `ViewMode::Sequential` since stage 3), no depth
  test/write; horizon->zenith gradient + sun disc/glow from the environment.
- **Palette post (Q9b)**: `RendererInitParams::palette_post` (app wires
  DFN_PALETTE=1) -> the upscale shader Bayer-dithers in internal-pixel space
  and quantizes to the fixed 64-color palette (`BgfxPalette.{h,cpp}`,
  pure/testable, 8 ramps x 8 shades). OFF = exact stage-2 passthrough.
- **Point sampling**: all textures bound via `submit` are point-sampled
  (Daggerfall crunch; also prevents bleed across terrain-atlas cells).
- **Dynamic sun shadows (в1, feature-requests batch)**: 2048 depth-only map
  (D16, hardware compare LEQUAL) over the loaded chunk ring — eye-centered
  ortho (half extent 640 m, texel-snapped) built each frame from
  `environment.sun_direction`, so the app-animated day/night sun (в2) moves
  the shadows with no further wiring. Every opaque `submit` renders into the
  shadow view with the internal "shadow" program (terrain, trees, houses,
  scatter all cast; water/debug/sky do not); `shaders/dfn_shadow.sh` samples
  one hard compare tap (PCF off — pixelated edges fit the art style) with
  normal-offset + depth bias against acne (`u_lightMtx`/`u_shadowParams`
  packing is a contract with `BgfxRenderer.cpp::update_shadow`). Sun below
  0.05 elevation -> shadows off (night is ambient-lit; ndotl is 0 anyway).
  Shadow constants are backend look-dev values flagged for the NUMBERS.md
  migration. (Now 4096 over a 320 m half extent = 0.156 m per texel — the
  thin-caster fix; a caster must be ~2 texels wide, i.e. ~0.31 m, to shadow.)
- **Carried-light cube shadows (interiors)**: up to
  `MAX_SHADOW_POINT_LIGHTS` (2) point lights get a real omnidirectional map.
  Six 90-degree faces per light are packed into ONE 2D atlas (4x3 tiles of
  512 px, R32F, RGBA8 fallback) that stores LINEAR DISTANCE / radius, so the
  receiver compares world metres and never linearizes a depth buffer or knows
  a face's near/far. `dfn_pointshadow.sh` picks the face by the major axis of
  the direction to the fragment — the exact region a 90-degree face covers —
  which is why neither side needs a cube-map sampling convention. Lights are
  reordered so shadow casters take the first slots, making the shader's light
  index the cube index by construction.
  Casters are culled twice: to the light SPHERE (from a bounding sphere
  measured in `create_mesh`, since the frozen `submit` carries no bounds) and
  then per FACE against the four side planes, which takes a prop from 6 draws
  to about 1-2. Alpha-CUTOUT programs are deliberately skipped: a leaf card
  would punch its rectangle into torchlight.
  **Order rule learned here, and it applies to every future view**:
  `bgfx::touch` is an empty draw, an empty draw SWALLOWS the pending uniform
  range without applying it, and views render sorted by id rather than in
  submission order. Every touch of the frame therefore happens in
  `begin_frame` BEFORE the first `setUniform`
  (`Impl::touch_point_shadow_views`). Touching after `apply_environment` made
  the whole world render with the DEFAULT (daylit) environment the moment a
  torch was lit.

### Logical program name -> render state (backend convention)

| Name | State |
|---|---|
| `terrain`, `unlit`, `prop` (and any unlisted name) | opaque: RGB+A+Z write, depth test LESS |
| `water` | transparent: RGB+A write, depth test LESS, NO depth write, alpha blend; callers submit transparents after opaques (scene view is sequential); transparents do NOT cast shadows |
| `foliage` | alpha-cutout: opaque state, mask-driven discard; casts through `shadow_cutout` in the SUN map, and is skipped entirely in the carried-light cube pass |
| `debug`, `sky`, `upscale`, `shadow`, `shadow_cutout`, `point_shadow` | backend-internal (lines / background / post / sun caster depth / cutout caster depth / point-light distance) |

Shaders: `sources/bgfx/shaders/*.sc` — terrain v4 (2x2 atlas splat driven by
per-vertex weights R=sand/G=rock/B=water-bed baked from core's
SurfaceFieldView surface_class ONLY — design ruling: render never re-derives
material bands from raw dist/height fields; slope rock augmentation from env
uniforms gives the §4 grass<->rock dither band, ordered 4x4
dither transitions in internal-pixel space + lambert x shadow + distance
fog), prop (vertex-color albedo, lambert x shadow + fog — placeholder
scatter/site meshes; shares vs_terrain), shadow (depth-only caster pass),
unlit, debug lines, sky, water (dual scrolled samples, fog-aware
alpha), upscale (+palette), `dfn_env.sh` (environment uniform layout — change
only together with `BgfxRenderer.cpp::apply_environment`), `dfn_shadow.sh`
(sun shadow sampling — change only together with
`BgfxRenderer.cpp::update_shadow`), `dfn_pointshadow.sh` (carried-light cube
lookup — face order and row-major matrix packing are a contract with
`BgfxRenderer.cpp::update_point_shadows`).
