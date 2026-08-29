/*
Module: engine/platform/input
File: engine/platform/input/sources/null/CreateNullInput.h

Responsibility:
- Factory for the null input backend (lead-agreed integration convention).

Key items:
- create_null_input().

Dependencies:
- Uses: IInput interface.
- Used by: engine/app (backend wiring), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#pragma once

#include "engine/platform/input/interfaces/IInput.h"

#include <memory>

namespace dfn::platform {

/// Creates the headless input backend (Rule 3). Never fails.
[[nodiscard]] std::unique_ptr<IInput> create_null_input();

} // namespace dfn::platform
