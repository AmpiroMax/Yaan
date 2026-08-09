/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 00:45:08
Module: engine/physics
File: engine/physics/sources/CollisionLayers.h

Responsibility:
- Engine-level semantics of the opaque platform CollisionMask bits (the
  IPhysics contract says bit meanings live HERE, not in the interface).

Key items:
- LAYER_STATIC / LAYER_CHARACTER: the stage-2 collision layer set.

Dependencies:
- Uses: interfaces/IPhysics.h (CollisionMask).
- Used by: engine/physics, engine/gameplay (raycasts, character creation),
  engine/world terrain spawning, tests.

Notes:
- Backends never interpret these bits (they store & AND them); adding a layer
  is therefore an engine-side change only. Extend at a sync when interactables
  or projectiles need their own bit.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Values are serialized nowhere yet; if they ever are, they freeze (Rule 7).
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial layer set (static, character).
*/

#pragma once

#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::physics {

// Static world geometry: terrain, buildings, dungeon prefabs.
inline constexpr platform::CollisionMask LAYER_STATIC = 1u << 0;

// Kinematic characters (player + NPCs), including their raycastable bodies.
inline constexpr platform::CollisionMask LAYER_CHARACTER = 1u << 1;

} // namespace dfn::physics
