/*
Module: engine/gameplay
File: engine/gameplay/sources/Inventory.cpp

Responsibility:
- Inventory operations and the item definition table: stacking, counting,
  removal-with-shortfall, quest-item drop protection.

Key items:
- ItemDatabase::add/find/max_stack/is_quest_item.
- count_item / can_drop / add_item / remove_item.

Dependencies:
- Uses: Inventory.h, Item.h.
- Used by: the interaction system, quest effects, save sections, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- has_item semantics (total count) are contract with story — do not change.
*/

#include "engine/gameplay/sources/Inventory.h"

#include <algorithm>

namespace dfn::gameplay {

// --- ItemDatabase ------------------------------------------------------------

void ItemDatabase::add(ItemDef def) {
    const uint64_t key = def.id.value;
    defs_[key] = std::move(def);
}

const ItemDef* ItemDatabase::find(ItemId id) const {
    const auto it = defs_.find(id.value);
    return it != defs_.end() ? &it->second : nullptr;
}

bool ItemDatabase::contains(ItemId id) const {
    return defs_.contains(id.value);
}

uint32_t ItemDatabase::max_stack(ItemId id) const {
    const ItemDef* def = find(id);
    // Unknown id: treat as non-stackable so nothing merges unexpectedly.
    return def != nullptr ? std::max(1u, def->max_stack) : 1u;
}

bool ItemDatabase::is_quest_item(ItemId id) const {
    const ItemDef* def = find(id);
    return def != nullptr && def->quest_item;
}

std::vector<ItemId> ItemDatabase::all_ids() const {
    std::vector<ItemId> ids;
    ids.reserve(defs_.size());
    for (const auto& [key, def] : defs_) {
        ids.push_back(def.id);
    }
    // Deterministic order (Rule 13.2): hash-map iteration order is not stable.
    std::sort(ids.begin(), ids.end(),
              [](ItemId a, ItemId b) { return a.value < b.value; });
    return ids;
}

// --- Queries -----------------------------------------------------------------

uint32_t count_item(const Inventory& inventory, ItemId item) {
    uint32_t total = 0;
    for (const ItemStack& stack : inventory.stacks) {
        if (stack.item.value == item.value) {
            total += stack.count;
        }
    }
    return total;
}

bool can_drop(const ItemDatabase& items, ItemId item) {
    // Quest items are held, given and taken by quest effects — never dropped
    // or sold by the player (story's integrity requirement).
    return !items.is_quest_item(item);
}

// --- Mutation ----------------------------------------------------------------

uint32_t add_item(Inventory& inventory, const ItemDatabase& items, ItemId item,
                  uint32_t count) {
    if (!item.valid() || count == 0) {
        return 0;
    }
    const uint32_t stack_limit = items.max_stack(item);
    uint32_t remaining = count;

    // Top up existing stacks first so the inventory stays compact.
    for (ItemStack& stack : inventory.stacks) {
        if (remaining == 0) {
            break;
        }
        if (stack.item.value != item.value || stack.count >= stack_limit) {
            continue;
        }
        const uint32_t space = stack_limit - stack.count;
        const uint32_t moved = std::min(space, remaining);
        stack.count += moved;
        remaining -= moved;
    }
    // Then open new stacks for whatever is left.
    while (remaining > 0) {
        const uint32_t moved = std::min(stack_limit, remaining);
        inventory.stacks.push_back(ItemStack{item, moved});
        remaining -= moved;
    }
    return count; // no weight/slot limit yet: everything fits
}

uint32_t remove_item(Inventory& inventory, ItemId item, uint32_t count) {
    if (!item.valid() || count == 0) {
        return 0;
    }
    uint32_t remaining = count;
    for (ItemStack& stack : inventory.stacks) {
        if (remaining == 0) {
            break;
        }
        if (stack.item.value != item.value) {
            continue;
        }
        const uint32_t taken = std::min(stack.count, remaining);
        stack.count -= taken;
        remaining -= taken;
    }
    // Drop emptied stacks; never leaves count == 0 entries behind.
    std::erase_if(inventory.stacks,
                  [](const ItemStack& stack) { return stack.count == 0; });
    return count - remaining; // the caller sees the shortfall in the difference
}

} // namespace dfn::gameplay
