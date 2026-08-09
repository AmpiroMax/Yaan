/*
Created: 09:08:2026 - 22:21:30
Last updated: 09:08:2026 - 22:21:30
Module: engine/gameplay
File: engine/gameplay/sources/ViewModel.h

Responsibility:
- The first-person presentation: the hand you see, and the item in it. Answers
  the first half of the user's complaint ("рук нет и трогать нечего") — the
  simulation side of having visible hands.

Key items:
- ViewModelAssets: the World resource naming the hand mesh (content, Rule 5).
- ViewModelPart: marks the hand / held-item entities so they can be updated.
- spawn_view_model(): creates the two entities for a carrier. Idempotent.
- update_view_model(): parks them at the hand anchor every fixed tick.

Dependencies:
- Uses: core ecs + components (Transform pair, RenderMesh, CameraPose,
  CarriedLight), HeldItem.h, Item.h, generated constants.
- Used by: engine/app (spawn once, update per tick), tests.

Notes:
- NO NEW RENDER PASS IS NEEDED, and that is the whole design. The hand is an
  ORDINARY WORLD-SPACE ENTITY with Transform + PreviousTransform + RenderMesh,
  parked in front of the eye each tick; render's existing ECS pass draws and
  interpolates it like any other object. A true view-space viewmodel pass would
  have meant a second view in the bgfx backend (one scene view, reversed-Z, no
  depth-clear seam) — i.e. a frozen-contract change (Rule 26) for a result the
  player cannot tell apart at 0.45 m from the near plane of 0.1 m.
- THE ANCHOR IS THE TORCH ANCHOR. TORCH_HAND_OFFSET_RIGHT and
  TORCH_HAND_OFFSET_BELOW_EYE already fix two of its three axes because the
  carried light needed them; HAND_OFFSET_FORWARD is the third. One anchor, so a
  torch's flame and a torch's wood can never drift apart.
- The hand follows YAW AND PITCH, unlike the body (yaw only): it is attached to
  the view, not to the feet. That is also why the light moves here for carriers
  that have a view model — otherwise, looking straight down would slide the
  flame away from the wood by the length of the forward offset.
- Mesh ids are CONTENT (Rule 5): the hand's id comes from the ViewModelAssets
  resource and an item's from its ItemDef. A zero id means "no mesh registered
  yet", which draws nothing and is not an error — the systems work before the
  art exists.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Plain data (Rule 8). Never hardcode a mesh id here; it is content.
- Call update_view_model AFTER player_post_step in the tick: it reads the
  CameraPose that post_step writes, and a stale eye pose puts the hand a tick
  behind the view, which reads as the hand lagging when you turn.
*/
/*
UPD:
- 09:08:2026 - 22:21:30: Created — visible hands (user request: "рук нет").
*/

#pragma once

#include <cstdint>

#include "engine/core/ecs/sources/EntityId.h"

namespace dfn::ecs {
class World;
}

namespace dfn::gameplay {

// World RESOURCE: engine-level asset ids for the first-person presentation.
// Registered by the app from content; 0 means "not registered", which draws
// nothing rather than drawing something wrong.
struct ViewModelAssets {
    uint32_t hand_mesh = 0;
};

// Marks one of the two entities that make up a carrier's view model.
struct ViewModelPart {
    ecs::EntityId carrier{};
    bool is_item = false; // false = the hand itself, true = what it holds
};

// Creates the hand and held-item entities for `carrier` (which must have a
// CameraPose). Calling it twice does nothing the second time, so it is safe on
// a load path that may re-run.
void spawn_view_model(ecs::World& world, ecs::EntityId carrier);

// Once per fixed tick, AFTER player_post_step: snapshots curr->prev on both
// parts, places them at the hand anchor derived from the carrier's CameraPose,
// and points the item entity's RenderMesh at whatever HeldItem now holds.
void update_view_model(ecs::World& world);

} // namespace dfn::gameplay
