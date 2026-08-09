/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 00:45:08
Module: engine/platform/physics
File: engine/platform/physics/sources/null/CreateNullPhysics.h

Responsibility:
- Factory for the null physics backend (Rule 3): a runnable mode where capsules
  glide on their spawn plane and every call succeeds deterministically.

Key items:
- create_null_physics(): the only symbol; concrete type stays private to the TU.

Dependencies:
- Uses: interfaces/IPhysics.h, <memory>.
- Used by: engine/app (backend wiring), tests (headless simulation).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Behavior contract lives in interfaces/IPhysics.h notes; keep them in sync.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — factory per the lead's integration convention.
*/

#pragma once

#include <memory>

#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::platform {

// Null backend (Rule 3, Q31): step is a no-op for bodies; characters apply
// horizontal displacement fully, vertical is ignored; grounded is always true;
// raycasts miss. Deterministic — the default for gameplay tests.
[[nodiscard]] std::unique_ptr<IPhysics> create_null_physics();

} // namespace dfn::platform
