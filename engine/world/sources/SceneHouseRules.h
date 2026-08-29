/*
Module: engine/world
File: engine/world/sources/SceneHouseRules.h

Responsibility:
- ПРАВИЛА ПОСТРОЙКИ (docs/HOUSES.md §3-5 и §8): everything the scene judge
  knows about how a house is ASSEMBLED — panels seated in joints, joints
  loaded no harder than their facet count allows, decks and roofs carried by
  horizontal joints and never hanging in the air. Split out of Scene.cpp,
  which had already outgrown Rule 21 (1052 lines) before this order doubled
  the connector rules.

Key items:
- check_house_rules(): appends findings to the caller's vector.
- The rules: JointSeat, JointAngle, WallTwoJoints, JointCapacity,
  DeckOnJoints, RoofSeat (SceneRule values live in Scene.h with the rest).

Dependencies:
- Uses: engine/world/Scene.h (SceneDoc, SceneWorld, SceneLimits, SceneFinding),
  glm, std. NOT engine/render: a rule about what stands where must be
  checkable in a tool with no window, exactly like the rules in Scene.cpp.
- Used by: check_scene() in Scene.cpp; tests/core/SceneHouseRuleTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NAMES CARRY THE WORKING PROPERTIES. A joint's diameter and facet count, a
  sleeper's length, a slope's run/rise — every one of them is read out of the
  REGISTRY NAME, never out of the forge. That is the kit's own rule (HOUSES.md
  §4), and it is what lets this file judge parts it cannot construct and
  assemblies that were baked months ago.
- ONE SEATING CONVENTION FOR BOTH ORIENTATIONS: a panel meets a joint with its
  MID-THICKNESS PLANE ON THE JOINT'S AXIS (§3.2). Vertical or horizontal makes
  no difference to it, and it must not: a deck that "rests on top" of a joist
  and a wall that is "centred on" a post would be two conventions, and the
  first composer to mix them would get a hairline seam nobody could name.
- A JOINT WITH NOTHING ON IT IS NEVER A FINDING. The user said it in as many
  words («столбы можно ставить без стен, без ограничений»): a colonnade, a
  ruin, a fence of posts are all legal. Only what IS attached is judged.
*/

#pragma once

#include "engine/world/sources/Scene.h"

#include <vector>

namespace dfn::world {

/// Judges HOW THE THING IS BUILT and appends what it finds. Separate entry
/// point rather than a second check_scene: the caller keeps one report, and
/// the building rules keep one file (Rule 21).
///
/// Needs `world.object_box` — every rule here measures a panel's own local
/// extent — and quietly does nothing without it, the same door the box-based
/// rules in Scene.cpp already use.
void check_house_rules(const SceneDoc& doc, const SceneWorld& world,
                       const SceneLimits& limits,
                       std::vector<SceneFinding>& found);

} // namespace dfn::world
