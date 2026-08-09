/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
Module: engine/platform/input
File: engine/platform/input/sources/glfw/CreateGlfwInput.h

Responsibility:
- Factory for the GLFW input backend (lead-agreed integration convention).

Key items:
- create_glfw_input(IWindow&).

Dependencies:
- Uses: IInput + IWindow interfaces.
- Used by: engine/app (backend wiring).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial factory.
*/

#pragma once

#include "engine/platform/input/interfaces/IInput.h"
#include "engine/platform/window/interfaces/IWindow.h"

#include <memory>

namespace dfn::platform {

/// Creates the GLFW input backend bound to `window`, which MUST be a window
/// created by create_glfw_window and already init()ed (the factory reaches the
/// GLFW handle internally; asserted). Returns nullptr on a wrong window type.
[[nodiscard]] std::unique_ptr<IInput> create_glfw_input(IWindow& window);

} // namespace dfn::platform
