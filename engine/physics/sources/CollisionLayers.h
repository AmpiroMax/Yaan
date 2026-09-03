/*
Module: engine/physics
File: engine/physics/sources/CollisionLayers.h

Responsibility:
- Engine-level semantics of the opaque platform CollisionMask bits (the
  IPhysics contract says bit meanings live HERE, not in the interface).

Key items:
- LAYER_STATIC / LAYER_CHARACTER: the stage-2 collision layer set.
- LAYER_INTERACTABLE / LAYER_LOOSE / LAYER_HITBOX / LAYER_FOOT / LAYER_RAGDOLL.

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

// ХИТБОКСЫ ЧАСТЕЙ ТЕЛА: кинематические коробки, едущие за позой (голова,
// корпус, конечности). ОТДЕЛЬНЫЙ СЛОЙ ОТ КАПСУЛЫ, И ЭТО НЕ АККУРАТНОСТЬ, А
// РАЗНЫЕ ВОПРОСЫ. Капсула отвечает «куда человек может пройти» и обязана быть
// гладкой: локти и колени в проёме — это застревание. Хитбоксы отвечают «во
// что попали» и обязаны быть подробными. Один слой на оба ответа заставил бы
// локомоцию цепляться за собственные руки.
inline constexpr platform::CollisionMask LAYER_HITBOX = 1u << 4;

// ФИЗИЧЕСКИЕ СТОПЫ (LOCOMOTION_GROUNDED §12): тело стопы — коробка по
// хитбоксу, в махе кинематическая, на постановке динамическая с трением.
// СВОЙ БИТ, потому что стопа трогает МИР (LAYER_STATIC | LAYER_LOOSE |
// LAYER_INTERACTABLE — это её `collides_with`) и не трогает ничего своего:
// ни капсулу, ни хитбоксы. Луч прицела стопы не ищет; луч «что под ногой»
// — ищет ровно их.
inline constexpr platform::CollisionMask LAYER_FOOT = 1u << 5;

// РЕГДОЛЛ (HIT_REACTIONS_PHYSICS §3): части тела динамические, на своих
// суставах. Тот же довод, что у стоп: трогает мир и другие регдоллы, не
// трогает капсулу владельца и его хитбоксы (пока тело — регдолл, хитбоксы —
// и есть его части).
inline constexpr platform::CollisionMask LAYER_RAGDOLL = 1u << 6;

} // namespace dfn::physics
