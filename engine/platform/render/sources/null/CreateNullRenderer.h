/*
Module: engine/platform/render
File: engine/platform/render/sources/null/CreateNullRenderer.h

Responsibility:
- Factory for the null renderer backend (lead-agreed integration convention).

Key items:
- create_null_renderer().

Dependencies:
- Uses: IRenderer interface.
- Used by: engine/app (backend wiring), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#pragma once

#include "engine/platform/render/interfaces/IRenderer.h"

#include <memory>

namespace dfn::platform {

/// Creates the headless renderer backend (Rule 3). Never fails.
[[nodiscard]] std::unique_ptr<IRenderer> create_null_renderer();

} // namespace dfn::platform
