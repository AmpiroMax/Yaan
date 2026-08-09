/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 00:45:00
Module: engine/platform/input
File: engine/platform/input/sources/null/NullInput.cpp

Responsibility:
- create_null_input factory (NullInput itself is header-only trivial).

Key items:
- create_null_input().

Dependencies:
- Uses: NullInput.h, CreateNullInput.h.
- Used by: dfn_platform_input target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial factory.
*/

#include "engine/platform/input/sources/null/NullInput.h"

#include "engine/platform/input/sources/null/CreateNullInput.h"

#include <memory>

namespace dfn::platform {

std::unique_ptr<IInput> create_null_input() {
    return std::make_unique<NullInput>();
}

} // namespace dfn::platform
