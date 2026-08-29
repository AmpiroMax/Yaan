/*
Module: engine/platform/window
File: engine/platform/window/sources/null/CreateNullWindow.h

Responsibility:
- Factory for the null window backend (lead-agreed integration convention).

Key items:
- create_null_window().

Dependencies:
- Uses: IWindow interface.
- Used by: engine/app (backend wiring), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#pragma once

#include "engine/platform/window/interfaces/IWindow.h"

#include <memory>

namespace dfn::platform {

/// Creates the headless window backend (Rule 3). Never fails.
[[nodiscard]] std::unique_ptr<IWindow> create_null_window();

} // namespace dfn::platform
