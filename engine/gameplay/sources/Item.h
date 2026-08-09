/*
Created: 09:08:2026 - 18:56:32
Last updated: 09:08:2026 - 18:56:32
Module: engine/gameplay
File: engine/gameplay/sources/Item.h

Responsibility:
- Item definitions: what an item IS (name key, stacking, weight, quest-item
  protection). Definitions come from content files (Rule 5), never from C++.

Key items:
- ItemDef: one item's authored definition, keyed by ItemId.
- ItemDatabase: the loaded definition table, a World resource (Rule 10).

Dependencies:
- Uses: Ids.h, C++ stdlib.
- Used by: Inventory (stacking rules, drop protection), the TAKE verb, UI,
  loot rolls, story's quest effects (give_item/take_item).

Notes:
- `quest_item` (story's ask, stage sync): quest-critical items must not be
  droppable or sellable. Losing the crown grant to a shop UI would make a main
  quest unfinishable and force a recovery branch per quest item; losing it to
  the plot is authored reactivity, which is a different thing. Enforcement
  lives in the drop/sell paths — an inventory can HOLD one, nothing can remove
  one except explicit quest effects.
- Ids follow story's convention: dot-namespaced snake_case,
  item.<category>.<name> (item.quest.crown_grant), hashed to ItemId by the
  frozen FNV-1a at load. C++ never contains the string ids (Rule 5).
- `display_name_key` / `description_key` are localization keys, never text.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Never hardcode an item here; definitions are data (Rules 5-6).
*/
/*
UPD:
- 09:08:2026 - 18:56:32: Initial item definition contract (interaction stage).
*/

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/gameplay/sources/Ids.h"

namespace dfn::gameplay {

// One item's authored definition. Plain data loaded from content.
struct ItemDef {
    ItemId id{};
    std::string display_name_key; // localization key (Rule 5)
    std::string description_key;  // localization key; may be empty
    uint32_t max_stack = 1;       // 1 = non-stackable
    float weight_kg = 0.0f;
    // Quest-critical: may be held and given by quest effects, but never
    // dropped or sold by the player (story's integrity requirement).
    bool quest_item = false;
    // Content declares which items can be lit in the hand (torch, lantern,
    // candle). Gameplay refuses to light anything else, so a content mistake
    // surfaces instead of producing a torch-shaped rock that glows.
    bool light_source = false;
};

// The loaded definition table (World resource). Unknown ids are answered with
// nullptr so a save referencing removed content degrades instead of crashing.
class ItemDatabase {
public:
    void add(ItemDef def);

    [[nodiscard]] const ItemDef* find(ItemId id) const;
    [[nodiscard]] bool contains(ItemId id) const;
    [[nodiscard]] std::size_t size() const { return defs_.size(); }

    // Stack ceiling for an item; 1 for unknown ids (safe default: no merging).
    [[nodiscard]] uint32_t max_stack(ItemId id) const;
    // Unknown ids are NOT quest items — they cannot be authored-critical.
    [[nodiscard]] bool is_quest_item(ItemId id) const;

    [[nodiscard]] std::vector<ItemId> all_ids() const;

private:
    std::unordered_map<uint64_t, ItemDef> defs_;
};

} // namespace dfn::gameplay
