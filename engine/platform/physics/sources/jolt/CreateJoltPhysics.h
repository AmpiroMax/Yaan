/*
Module: engine/platform/physics
File: engine/platform/physics/sources/jolt/CreateJoltPhysics.h

Responsibility:
- Factory for the Jolt physics backend. The only Jolt-facing symbol the rest of
  the engine ever sees; all Jolt types stay inside the backend TU (Rule 1).

Key items:
- create_jolt_physics(): the only symbol; concrete type stays private to the TU.

Dependencies:
- Uses: interfaces/IPhysics.h, <memory>. NO Jolt headers here.
- Used by: engine/app (backend wiring), jolt-backed tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Never leak Jolt types/includes through this header (Rule 1).
*/

#pragma once

#include <memory>

#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::platform {

// Jolt Physics backend (JoltPhysics v5.2.0, FetchContent-pinned). Implements
// the full IPhysics contract: static terrain/boxes, kinematic capsule
// characters (collide-and-slide, stairs, slope limit), masked raycasts.
[[nodiscard]] std::unique_ptr<IPhysics> create_jolt_physics();

} // namespace dfn::platform
