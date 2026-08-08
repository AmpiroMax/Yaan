/*
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 00:18:26
Module: engine/gameplay
File: engine/gameplay/sources/Interaction.h

Responsibility:
- Interaction components (Q11): explicitly marked interactive objects —
  highlightable, openable, lootable. No physics sandbox; interaction is
  semantic, not physical.

Key items:
- Highlightable: hover prompt (localization key, Rule 5).
- Openable: doors/containers open/locked state.
- Lootable: loot source backed by a content loot table.

Dependencies:
- Uses: engine/gameplay Ids.h, C++ stdlib.
- Used by: interaction system (IPhysics raycast from the camera -> writes the
  lead-owned HoverTarget resource, agreed stage-1 sync), UI (prompt), save
  delta (open/locked/looted flags persist).

Notes:
- Flow (Q11, agreed with lead): the interaction system raycasts through
  IPhysics each tick, resolves RayHit.user_data to an EntityId, checks for
  Highlightable, and publishes the result via the HoverTarget World resource
  (type owned by the lead in engine/core/components); render reads only that
  resource to draw the highlight; UI shows prompt_key localized.
- prompt_key is a localization key, never display text (Rule 5) — e.g. the key
  for "Open", "Loot", "Talk"; actual strings live in localization files.
- Player-triggered interactions mutate these components through the interaction
  system only; NPC-triggered ones arrive via NpcActions (Rule 15). All three
  flag sets are save-delta state (world stores the delta, Q56).
- Locks: lock_level is compared via the dice API against Lockpicking at the
  interaction; formulas from NUMBERS at the combat/skills grill.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Plain data only (Rule 8). No literal user-facing strings (Rule 5).
- Do not grow a physics sandbox here — interactions are explicit (Q11).
*/
/*
UPD:
- 09:08:2026 - 00:18:26: Initial stage-1 interaction components (highlightable,
                         openable, lootable).
*/

#pragma once

#include <cstdint>
#include <string>

#include "engine/gameplay/sources/Ids.h"

namespace dfn::gameplay {

// Marks an entity as interactive: it highlights on hover and offers a prompt.
struct Highlightable {
    std::string prompt_key; // localization key (Rule 5), e.g. verb of the interaction
    float max_use_distance = 0.0f; // meters; 0 = INTERACT_DISTANCE default (NUMBERS.md)
};

// Doors, chests, drawers. Openable state persists in the save delta.
struct Openable {
    bool open = false;
    bool locked = false;
    uint32_t lock_level = 0; // 0 = trivial; checked against Lockpicking via dice
};

// A loot source. Contents are defined by a content-file loot table (Rule 5);
// the roll uses the simulation Rng (Dice.h) at first open, then persists.
struct Lootable {
    LootTableId loot_table{};
    bool looted = false;
};

} // namespace dfn::gameplay
