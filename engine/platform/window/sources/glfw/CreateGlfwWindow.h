/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
Module: engine/platform/window
File: engine/platform/window/sources/glfw/CreateGlfwWindow.h

Responsibility:
- Factory for the GLFW window backend (lead-agreed integration convention).

Key items:
- create_glfw_window().

Dependencies:
- Uses: IWindow interface.
- Used by: engine/app (backend wiring).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial factory.
*/

#pragma once

#include "engine/platform/window/interfaces/IWindow.h"

#include <memory>

namespace dfn::platform {

/// Creates the GLFW window backend. The returned window must also be the one
/// passed to create_glfw_input (the input backend reaches its GLFW handle).
[[nodiscard]] std::unique_ptr<IWindow> create_glfw_window();

} // namespace dfn::platform
