/*
Created: 09:08:2026 - 00:45:08
Last updated: 28:08:2026 - 13:22:10
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
- 09:08:2026 - 18:56:32: Added LAYER_INTERACTABLE for crosshair targeting.
- 28:08:2026 - 13:22:10: LAYER_LOOSE — подвижные предметы (зона ФИЗИКА
  ПРЕДМЕТОВ). Свой бит, а не LAYER_INTERACTABLE, по той же причине, по какой
  interactable в своё время отделили от static: прицел ХВАТА обязан находить
  только то, что можно поднять, и не спотыкаться ни о дверь, ни о точку
  посадки, стоящие в том же метре. Заметьте предупреждение в шапке: биты
  нигде не сериализуются — как только начнут, они заморожены (правило 7).
*/

#pragma once

#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::physics {

// Static world geometry: terrain, buildings, dungeon prefabs.
inline constexpr platform::CollisionMask LAYER_STATIC = 1u << 0;

// Kinematic characters (player + NPCs), including their raycastable bodies.
inline constexpr platform::CollisionMask LAYER_CHARACTER = 1u << 1;

// Interactable props: doors, chests, levers, loose items. Separate from
// LAYER_STATIC so the crosshair ray finds only things the player can act on
// and is never blocked by ordinary terrain in front of them.
inline constexpr platform::CollisionMask LAYER_INTERACTABLE = 1u << 2;

// Loose props: the crockery, stools, baskets and books a player may pick up,
// carry and stack. A body on this layer is DYNAMIC — it has mass, it falls, it
// sleeps — which is what separates it from LAYER_INTERACTABLE (a door leaf is
// interactable and immovable) and from LAYER_STATIC (a wall).
inline constexpr platform::CollisionMask LAYER_LOOSE = 1u << 3;

} // namespace dfn::physics
