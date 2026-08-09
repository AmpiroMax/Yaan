/*
Created: 09:08:2026 - 18:56:32
Last updated: 09:08:2026 - 18:56:32
Module: engine/gameplay
File: engine/gameplay/sources/Inventory.h

Responsibility:
- The inventory component and its operations. Answers story's frozen has_item
  condition atom (§2.1) and backs the give_item/take_item quest effects
  (§2.2); persists in the save delta.

Key items:
- ItemStack / Inventory: plain-data component (Rule 8), one stack per ItemId.
- add_item / remove_item / count_item / can_drop: the whole surface.
- ItemCountChanged / ItemRemovalShortfall: events.

Dependencies:
- Uses: Ids.h, Item.h (stacking + quest-item rules), core ecs EntityId.
- Used by: the TAKE verb, quest effects, UI, save sections, tests.

Notes:
- has_item semantics (confirmed with story, both sides on record): the atom
  compares the TOTAL COUNT of an ItemId in the inventory. Non-stackables count
  1 each and sum. NOT equipped-only, NOT container-scoped — "wielding X" would
  be a different atom through a group sync, never a reinterpretation of this.
- remove_item never fails a caller: it removes what is present and REPORTS the
  shortfall (story's decision — effects run after a transition is already
  chosen, so a hard-failing effect would strand the state machine). The
  shortfall is published as an event so it shows up in a debug log instead of
  vanishing; quest authoring gates such transitions on a has_item condition.
- Stacking: one entry per distinct ItemId, split into multiple entries only
  when max_stack requires it, so count_item is a sum and never a scan for
  duplicates.
- Quest items (ItemDef::quest_item) may be added and removed by quest effects
  but can_drop() refuses them: the player cannot drop or sell a quest item.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Plain data (Rule 8). Never bypass these functions to edit stacks directly —
  the events and quest-item rules live here.
*/
/*
UPD:
- 09:08:2026 - 18:56:32: Initial inventory contract (interaction stage);
                         has_item/shortfall semantics agreed with story.
*/

#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/ecs/sources/EntityId.h"
#include "engine/gameplay/sources/Ids.h"
#include "engine/gameplay/sources/Item.h"

namespace dfn::gameplay {

struct ItemStack {
    ItemId item{};
    uint32_t count = 0;
};

// Per-entity inventory (player now; NPCs and containers later). Save-delta state.
struct Inventory {
    std::vector<ItemStack> stacks;
};

// --- Queries -----------------------------------------------------------------

// Total count of `item` across all stacks. THE backing of story's has_item.
[[nodiscard]] uint32_t count_item(const Inventory& inventory, ItemId item);

// True when the player may drop/sell this item (false for quest items).
[[nodiscard]] bool can_drop(const ItemDatabase& items, ItemId item);

// --- Mutation ----------------------------------------------------------------

// Adds `count`, filling existing stacks to max_stack before opening new ones.
// Returns how many were actually added (currently always `count` — no weight
// or slot limit yet; the return exists so limits can land without a signature
// change).
uint32_t add_item(Inventory& inventory, const ItemDatabase& items, ItemId item,
                  uint32_t count);

// Removes up to `count`. Returns how many were actually removed; the caller
// learns of a shortfall from the difference (see the header notes: this never
// hard-fails). Empty stacks are erased.
uint32_t remove_item(Inventory& inventory, ItemId item, uint32_t count);

// --- Events ------------------------------------------------------------------

struct ItemCountChanged {
    ecs::EntityId owner{};
    ItemId item{};
    uint32_t new_count = 0;
    int32_t delta = 0; // + added, - removed
};

// Published when remove_item could not satisfy the full request. Story's quest
// authoring should have gated the transition on has_item; this surfaces the
// authoring gap in a debug log rather than silently under-removing.
struct ItemRemovalShortfall {
    ecs::EntityId owner{};
    ItemId item{};
    uint32_t requested = 0;
    uint32_t removed = 0;
};

} // namespace dfn::gameplay
