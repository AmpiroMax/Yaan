/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/CreateBgfxRenderer.h

Responsibility:
- Factory for the bgfx renderer backend (lead-agreed integration convention).

Key items:
- create_bgfx_renderer().

Dependencies:
- Uses: IRenderer interface.
- Used by: engine/app (backend wiring).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial factory.
*/

#pragma once

#include "engine/platform/render/interfaces/IRenderer.h"

#include <memory>

namespace dfn::platform {

/// Creates the bgfx renderer backend. init() needs a real native window handle
/// (from a GlfwWindow); use create_null_renderer for headless runs.
[[nodiscard]] std::unique_ptr<IRenderer> create_bgfx_renderer();

} // namespace dfn::platform
