/*
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 18:56:32
Module: engine/gameplay
File: engine/gameplay/sources/Interaction.h

Responsibility:
- Interaction components (Q11): explicitly marked interactive objects —
  highlightable, openable, lootable. No physics sandbox; interaction is
  semantic, not physical.

Key items:
- InteractionVerb: what a target offers (None/Take/Open/Close/Use).
- Highlightable: hover prompt (localization key, Rule 5).
- Openable: doors/containers open/locked state (state machine shaped here).
- Pickup: a loose item, the TAKE target.
- Usable: content-declared "activate" (levers, campfires, beds).
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
- 09:08:2026 - 18:56:32: Interaction stage: InteractionVerb, Pickup (TAKE),
                         Usable (USE), and the Openable state machine written
                         down (locking shaped, unlocking deferred).
*/

#pragma once

#include <cstdint>
#include <string>

#include "engine/gameplay/sources/Ids.h"

namespace dfn::gameplay {

// What an interactable offers when looked at. The value travels to render in
// the HoverTarget resource as a raw uint8_t (render may not include gameplay,
// Rule 1), where it selects the reticle shape. Append-only.
enum class InteractionVerb : uint8_t {
    None = 0,
    Take = 1,  // loose item -> inventory
    Open = 2,  // closed door/chest -> open
    Close = 3, // open door/chest -> closed
    Use = 4,   // lever, campfire, bed: content-declared activation
};

// Marks an entity as interactive: it highlights on hover and offers a prompt.
struct Highlightable {
    std::string prompt_key; // localization key (Rule 5), e.g. verb of the interaction
    float max_use_distance = 0.0f; // meters; 0 = INTERACT_DISTANCE default (NUMBERS.md)
};

// Doors, chests, drawers. Openable state persists in the save delta.
//
// State machine (shaped now, locking implemented later per stage scope):
//   Closed --open--> Open --close--> Closed
//   Closed + locked --open--> refused (InteractionFailure::Locked)
//   unlocking (key / Lockpicking dice vs lock_level) is NOT implemented this
//   stage; `locked` is honoured as a hard refusal so content can already author
//   a locked door and the verb behaves correctly around it.
struct Openable {
    bool open = false;
    bool locked = false;
    uint32_t lock_level = 0; // 0 = trivial; checked against Lockpicking via dice
};

// A loose item lying in the world: the TAKE verb's target. Taking it moves
// `count` of `item` into the actor's inventory and despawns the entity.
struct Pickup {
    ItemId item{};
    uint32_t count = 1;
};

// A generic "activate" target: levers, campfires, beds. Content declares what
// it means; the engine only reports that it happened, so new usables need no
// C++ change (Rule 6). `action` is the hashed authored action id, carried in
// the Used event for quests and systems to react to.
struct Usable {
    uint64_t action = 0;   // hashed content action id (e.g. "use.lever.mill_gate")
    bool repeatable = true; // false = fires once, then offers nothing
    bool used = false;      // save-delta state for non-repeatable usables
};

// A loot source. Contents are defined by a content-file loot table (Rule 5);
// the roll uses the simulation Rng (Dice.h) at first open, then persists.
struct Lootable {
    LootTableId loot_table{};
    bool looted = false;
};

} // namespace dfn::gameplay
