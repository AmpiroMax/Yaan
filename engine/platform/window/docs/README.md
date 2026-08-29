
# engine/platform/window

## Responsibility

Platform window contract (Rule 0): lifecycle, OS event pump, native handle for
the renderer, framebuffer size, resize/close signalling. GLFW is hidden behind
it; backends arrive in stage 2.

## Key types

- `dfn::platform::IWindow` (`interfaces/IWindow.h`) — pure virtual contract:
  `init/shutdown`, `poll_events`, `should_close`/`request_close`,
  `native_handle`, `framebuffer_size`, `consume_resize`.
- `dfn::platform::WindowInitParams` — logical size, title (localized by the
  caller, Rule 5), fullscreen/resizable flags.

## Usage example

```cpp
dfn::platform::WindowInitParams params{1280, 720, localized_title, false, true};
if (!window.init(params)) { /* fail startup */ }
while (!window.should_close()) {
    window.poll_events();
    if (window.consume_resize()) {
        const glm::uvec2 fb = window.framebuffer_size();
        renderer.resize(fb.x, fb.y);
    }
    // input.update(), sim, render ...
}
window.shutdown();
```

## Dependencies

- Uses: C++ stdlib, glm (Rule 2). Backends (stage 2): `sources/glfw/` (GLFW),
  `sources/null/` (headless runnable mode, Rule 3).
- Used by: `engine/app` (owns the window, feeds `RendererInitParams` from
  `native_handle` + `framebuffer_size`), tests.

## Current state (stage 2)

Implemented: `sources/glfw/` (GlfwWindow — GLFW 3.4, polling model, Cocoa
native handle; owns glfwInit/Terminate, one live window at a time; claims no
GLFW callbacks — those belong to GlfwInput) and `sources/null/` (headless,
Rule 3). Factories: `sources/glfw/CreateGlfwWindow.h`,
`sources/null/CreateNullWindow.h`. Target: `dfn_platform_window`
(CMakeLists here, GLFW pinned at tag 3.4). Windows branch (Win32 handle)
compiles clean but is untested this stage.
