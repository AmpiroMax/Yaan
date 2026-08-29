
# Project Structure — Target Layout

The authoritative tree lives in [docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md)
(§ Repository layout). This file states the conventions that keep it stable.

## Module layout convention

- Every module = folder with `docs/` + `sources/`.
- `sources/` holds the actual `.h`/`.cpp` code.
- Only `engine/platform/*` modules also have `interfaces/`; their `sources/` contain
  backend subfolders (`bgfx/`, `glfw/`, `jolt/`, `ozz/`, `miniaudio/`, `llama/`, `null/`).
- Includes are absolute from repo root: `#include "engine/core/ecs/sources/World.h"`.

## Key principles

- Interfaces exist ONLY in `engine/platform/*` (isolate bgfx / GLFW / Jolt / ozz /
  miniaudio / llama.cpp). See ARCHITECTURE.md "Interface principle".
- `core/`, `world/`, `gameplay/` — zero external dependencies (glm exempt), pure C++.
- Only `platform/<m>/sources/` may include external libs.
- `games/daggerfall_n/assets/` — all game content in data files; user-facing strings
  in localization files.
- Every layer owns its own `CMakeLists.txt`; the root CMakeLists only adds
  subdirectories (Q34 — minimizes cross-agent merge conflicts).
- Numeric constants come from the header generated out of `docs/NUMBERS.md`.
